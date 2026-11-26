#pragma once

#include "core/Biquad.h"

#include <array>
#include <cmath>

namespace magiceq
{

/** The 12-band EQ itself. Zero latency (minimum-phase biquads), stereo, real-time safe.

    Parameter changes glide over roughly 20 ms so moving a band never clicks or zips.
*/
class EqEngine
{
public:
    static constexpr int numBands = 12;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        for (int b = 0; b < numBands; ++b)
        {
            current[(size_t) b] = target[(size_t) b];
            redesign (b);
        }
        reset();
    }

    void reset()
    {
        for (auto& band : states)
            for (auto& s : band)
                s.reset();
    }

    /** Call from the audio thread before process() with the latest parameter values. */
    void setTarget (int band, const BandSettings& s) noexcept { target[(size_t) band] = s; }

    void process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = numChannels < 2 ? numChannels : 2;
        const float alpha = 1.0f - std::exp (-(float) subBlock / (0.02f * (float) sampleRate));

        for (int start = 0; start < numSamples; start += subBlock)
        {
            const int n = (numSamples - start) < subBlock ? (numSamples - start) : subBlock;

            for (int b = 0; b < numBands; ++b)
                if (advance (b, alpha))
                    redesign (b);

            for (int b = 0; b < numBands; ++b)
            {
                if (! active[(size_t) b])
                    continue;

                const auto& d = designs[(size_t) b];
                for (int sec = 0; sec < d.numSections; ++sec)
                {
                    auto& state = states[(size_t) b][(size_t) sec];
                    const auto& c = d.sections[(size_t) sec];
                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        float* x = channels[ch] + start;
                        for (int i = 0; i < n; ++i)
                            x[i] = state.process (c, x[i], ch);
                    }
                }
            }
        }
    }

private:
    static constexpr int subBlock = 32;

    static bool same (float a, float b) noexcept { return ! (a < b) && ! (a > b); } // exact compare, on purpose

    /** Moves the band's live settings toward its target. Returns true if the filter needs redesigning. */
    bool advance (int b, float alpha) noexcept
    {
        auto& c = current[(size_t) b];
        const auto& t = target[(size_t) b];
        bool changed = false;

        // Shape changes and cuts switching on/off happen at once; bells, shelves and
        // notches fade their gain/depth through zero instead so toggling them is click-free.
        if (c.type != t.type || c.slopeIndex != t.slopeIndex)
        {
            const bool wasCut = c.type == BandType::LowCut || c.type == BandType::HighCut;
            const bool isCut = t.type == BandType::LowCut || t.type == BandType::HighCut;
            if (wasCut != isCut || c.slopeIndex != t.slopeIndex)
                for (auto& s : states[(size_t) b]) s.reset();
            c.type = t.type;
            c.slopeIndex = t.slopeIndex;
            changed = true;
        }

        const bool isCut = t.type == BandType::LowCut || t.type == BandType::HighCut;
        const float targetGain = t.enabled ? t.gainDb : 0.0f;

        if (t.enabled != c.enabled && (isCut || t.type == BandType::Notch))
        {
            c.enabled = t.enabled;
            for (auto& s : states[(size_t) b]) s.reset();
            changed = true;
        }
        else if (t.enabled && ! c.enabled)
        {
            c.enabled = true;
            c.gainDb = 0.0f;
            changed = true;
        }

        auto glide = [&] (float& value, float goal, float epsilon, bool logDomain)
        {
            if (std::abs (value - goal) <= epsilon * (logDomain ? goal : 1.0f))
            {
                if (! same (value, goal)) { value = goal; changed = true; }
                return;
            }
            value = logDomain ? std::exp (std::log (value) + alpha * (std::log (goal) - std::log (value)))
                              : value + alpha * (goal - value);
            changed = true;
        };

        glide (c.frequency, t.frequency, 0.0005f, true);
        glide (c.q, t.q, 0.0005f, true);
        glide (c.gainDb, targetGain, 0.01f, false);

        // A bell/shelf being switched off is finished once its gain has faded to zero.
        if (! t.enabled && c.enabled && ! isCut && t.type != BandType::Notch && same (c.gainDb, 0.0f))
        {
            c.enabled = false;
            changed = true;
        }

        return changed;
    }

    void redesign (int b) noexcept
    {
        const auto& c = current[(size_t) b];
        active[(size_t) b] = c.enabled;
        if (c.enabled)
            designs[(size_t) b] = designBand (c, sampleRate);
    }

    double sampleRate = 48000.0;
    std::array<BandSettings, numBands> target {}, current {};
    std::array<BandDesign, numBands> designs {};
    std::array<bool, numBands> active {};
    std::array<std::array<BiquadState, maxSectionsPerBand>, numBands> states {};
};

} // namespace magiceq
