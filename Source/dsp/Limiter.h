#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace magiceq
{

struct LimiterSettings
{
    bool enabled = true;
    float ceilingDb = -1.0f;
    float releaseMs = 50.0f;
    bool truePeak = true;
};

/** A look-ahead brick-wall limiter, the last thing in MagicEQ's chain.

    How it guarantees the ceiling:
      1. Work out, for every sample, the gain that would keep it under the ceiling.
         In true-peak mode the "sample" is the highest of four interpolated points
         (4x oversampled), so peaks that only appear between samples after
         digital-to-analogue conversion are caught too.
      2. Hold the lowest required gain for the look-ahead time (1 ms), release it smoothly.
      3. Average that over the look-ahead window, so the gain is already down when
         the loud sample comes out of the delay line. No clipping, no distortion clicks.

    The look-ahead and the interpolation filter add a small, fixed latency (about 1.2 ms
    at 48 kHz) that the plugin reports to the DAW so tracks stay in time.
*/
class Limiter
{
public:
    static constexpr int firHalf = 8; // taps either side for the 4x interpolator

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        lookahead = std::max (16, (int) std::lround (0.001 * sampleRate));
        delay = lookahead + firHalf;

        historySize = 2 * firHalf + 2;
        for (auto& h : inputHistory) h.assign ((size_t) historySize, 0.0f);
        for (auto& d : audioDelay) d.assign ((size_t) delay, 0.0f); // read-then-write gives exactly `delay` samples
        holdBuffer.assign ((size_t) lookahead + 1, 1.0f);
        boxBuffer.assign ((size_t) lookahead + 1, 1.0);
        boxSum = (double) (lookahead + 1);
        envelope = 1.0f;
        historyPos = delayPos = holdPos = boxPos = 0;

        buildInterpolator();
        setSettings (settings);
    }

    void reset() { prepare (sampleRate); }

    /** Latency in samples, constant whether the limiter is on or off. */
    int getLatencySamples() const noexcept { return delay; }

    void setSettings (const LimiterSettings& s) noexcept
    {
        settings = s;
        ceilingLin = std::pow (10.0f, s.ceilingDb / 20.0f);
        releaseCoeff = std::exp (-1.0f / (std::max (s.releaseMs, 1.0f) * 0.001f * (float) sampleRate));
    }

    /** Processes in place. Returns the largest gain reduction in this block, in dB (>= 0). */
    float process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = std::min (numChannels, 2);
        float minGain = 1.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            // 1. Push the new input into the interpolator history.
            for (int ch = 0; ch < numChannels; ++ch)
                inputHistory[(size_t) ch][(size_t) historyPos] = channels[ch][i];
            historyPos = (historyPos + 1) % historySize;

            // The sample we can now fully analyse sits firHalf samples back.
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = std::max (peak, settings.truePeak ? truePeakOf (ch) : std::abs (historyAt (ch, firHalf)));

            const float required = peak > ceilingLin ? ceilingLin / peak : 1.0f;

            // 2. Hold the minimum over the look-ahead window...
            holdBuffer[(size_t) holdPos] = required;
            holdPos = (holdPos + 1) % (lookahead + 1);
            float held = 1.0f;
            for (float g : holdBuffer) held = std::min (held, g);

            // ...drop instantly, recover with the release time...
            envelope = held < envelope ? held : held + releaseCoeff * (envelope - held);

            // 3. ...and average over the window so the gain is fully down in time.
            boxSum += (double) envelope - boxBuffer[(size_t) boxPos];
            boxBuffer[(size_t) boxPos] = envelope;
            boxPos = (boxPos + 1) % (lookahead + 1);
            const float gain = settings.enabled ? std::min (1.0f, (float) (boxSum / (double) (lookahead + 1))) : 1.0f;

            // Delay the audio to line up with the gain, then apply it.
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& d = audioDelay[(size_t) ch];
                const float delayed = d[(size_t) delayPos];
                d[(size_t) delayPos] = channels[ch][i];
                channels[ch][i] = delayed * gain;
            }
            delayPos = (delayPos + 1) % delay;

            minGain = std::min (minGain, gain);
        }

        return minGain < 1.0f ? -20.0f * std::log10 (minGain) : 0.0f;
    }

    /** Highest 4x-oversampled level of a block, for tests and meters (independent of the limiter state). */
    static float measureTruePeak (const float* data, int numSamples);

private:
    float historyAt (int ch, int samplesBack) const noexcept
    {
        int idx = historyPos - 1 - samplesBack;
        while (idx < 0) idx += historySize;
        return inputHistory[(size_t) ch][(size_t) idx];
    }

    /** Peak of the sample firHalf back and the three interpolated points after it. */
    float truePeakOf (int ch) const noexcept
    {
        // Points m-firHalf+1 .. m+firHalf around m = firHalf samples back.
        float peak = std::abs (historyAt (ch, firHalf));
        for (int phase = 0; phase < 3; ++phase)
        {
            float acc = 0.0f;
            const auto& taps = interp[(size_t) phase];
            for (int t = 0; t < 2 * firHalf; ++t)
                acc += taps[(size_t) t] * historyAt (ch, 2 * firHalf - 1 - t);
            peak = std::max (peak, std::abs (acc));
        }
        return peak;
    }

    void buildInterpolator()
    {
        // Windowed-sinc taps for the points 1/4, 2/4 and 3/4 of the way between two samples.
        constexpr double pi = 3.14159265358979323846;
        for (int phase = 0; phase < 3; ++phase)
        {
            const double frac = (phase + 1) / 4.0;
            double sum = 0.0;
            for (int t = 0; t < 2 * firHalf; ++t)
            {
                const double x = (double) (t - (firHalf - 1)) - frac; // distance from the interpolated point
                const double sinc = std::abs (x) < 1e-9 ? 1.0 : std::sin (pi * x) / (pi * x);
                const double w = 0.5 + 0.5 * std::cos (pi * x / (firHalf + 0.5)); // Hann
                interp[(size_t) phase][(size_t) t] = (float) (sinc * w);
                sum += sinc * w;
            }
            for (auto& c : interp[(size_t) phase]) c = (float) (c / sum); // unity gain at DC
        }
    }

    double sampleRate = 48000.0;
    LimiterSettings settings;
    float ceilingLin = 0.891f, releaseCoeff = 0.999f, envelope = 1.0f;
    int lookahead = 48, delay = 56, historySize = 18;
    int historyPos = 0, delayPos = 0, holdPos = 0, boxPos = 0;

    std::array<std::vector<float>, 2> inputHistory, audioDelay;
    std::vector<float> holdBuffer;
    std::vector<double> boxBuffer;
    double boxSum = 0.0;
    std::array<std::array<float, 2 * firHalf>, 3> interp {};
};

inline float Limiter::measureTruePeak (const float* data, int numSamples)
{
    Limiter l;
    l.prepare (48000.0);
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        l.inputHistory[0][(size_t) l.historyPos] = data[i];
        l.historyPos = (l.historyPos + 1) % l.historySize;
        if (i >= 2 * firHalf)
            peak = std::max (peak, l.truePeakOf (0));
    }
    return peak;
}

} // namespace magiceq
