#include "MatchFitter.h"
#include "StyleLibrary.h"

#include <algorithm>
#include <cmath>

namespace magiceq
{

namespace
{
    constexpr int N = AnalysisGrid::numPoints;
    constexpr double designRate = 48000.0; // band shapes barely depend on sample rate below 16 kHz

    double freqAt (int i) { return AnalysisGrid::frequencyAt (i); }

    double weightedMean (const GridCurve& c, double loHz, double hiHz)
    {
        double sum = 0.0;
        int n = 0;
        for (int i = 0; i < N; ++i)
            if (freqAt (i) >= loHz && freqAt (i) <= hiHz) { sum += c[(size_t) i]; ++n; }
        return n > 0 ? sum / n : 0.0;
    }

    void smooth (GridCurve& c)
    {
        GridCurve s = c;
        for (int i = 0; i < N; ++i)
        {
            const float l = c[(size_t) std::max (i - 1, 0)];
            const float r = c[(size_t) std::min (i + 1, N - 1)];
            s[(size_t) i] = 0.25f * l + 0.5f * c[(size_t) i] + 0.25f * r;
        }
        c = s;
    }

    GridCurve responseOf (const BandSettings& b)
    {
        GridCurve r {};
        for (int i = 0; i < N; ++i)
            r[(size_t) i] = (float) bandResponseDb (b, freqAt (i), designRate);
        return r;
    }

    /** Bandwidth in octaves -> RBJ Q. */
    float qFromOctaves (double octaves)
    {
        const double p = std::pow (2.0, octaves);
        return (float) (std::sqrt (p) / (p - 1.0));
    }
} // namespace

MatchResult MatchFitter::fit (const GridCurve& measuredSpectrum, int styleIndex, const Options& options)
{
    MatchResult result;
    const auto& style = StyleLibrary::get (styleIndex);
    const float strength = std::clamp (options.strength, 0.0f, 1.0f);

    // 1. Level-match: line both curves up across the core vocal range so we compare tone, not loudness.
    result.target = StyleLibrary::targetCurve (styleIndex);
    const double offset = weightedMean (measuredSpectrum, 200.0, 4000.0) - weightedMean (result.target, 200.0, 4000.0);
    for (int i = 0; i < N; ++i)
        result.measured[(size_t) i] = (float) (measuredSpectrum[(size_t) i] - offset);

    // 2. Where should Detect listen? Not below the low cut, and less at the very top,
    //    where noise and mic roll-off make the measurement unreliable.
    GridCurve weight {};
    const double lowCut = style.lowCutHz;
    for (int i = 0; i < N; ++i)
    {
        const double f = freqAt (i);
        double w = 1.0;
        if (f < lowCut * 1.25)       w = 0.0;
        else if (f < lowCut * 1.6)   w = 0.5;
        if (f > 12000.0)             w *= 0.6;
        weight[(size_t) i] = (float) w;
    }

    // 3. The correction we'd like, scaled by Strength and limited per style.
    GridCurve desired {};
    for (int i = 0; i < N; ++i)
    {
        float d = (result.target[(size_t) i] - result.measured[(size_t) i]) * strength;

        // If the top end is far below the target, it is usually a dull mic or the vocal simply
        // has nothing there: boosting would only raise hiss. Cap boosts in that case.
        if (freqAt (i) > 8000.0 && d > 0.0f && (result.target[(size_t) i] - result.measured[(size_t) i]) > 15.0f)
            d = std::min (d, 3.0f);

        d = std::clamp (d, -style.maxCutDb, style.maxBoostDb);
        desired[(size_t) i] = d * weight[(size_t) i];
    }
    smooth (desired);
    smooth (desired);
    result.correction = desired;

    // 4. Greedy placement: put a band on the largest remaining difference, subtract its shape, repeat.
    std::vector<BandSettings> tone;
    GridCurve residual = desired;

    for (int k = 0; k < options.maxToneBands; ++k)
    {
        int peak = -1;
        float peakAbs = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const float a = std::abs (residual[(size_t) i]) * (weight[(size_t) i] > 0.0f ? 1.0f : 0.0f);
            if (a > peakAbs) { peakAbs = a; peak = i; }
        }

        if (peak < 0 || peakAbs < 1.0f)
            break;

        const float peakVal = residual[(size_t) peak];
        const float half = std::abs (peakVal) * 0.5f;

        // How wide is this bump? Walk outwards until it falls to half height or changes sign.
        int lo = peak, hi = peak;
        while (lo > 0 && residual[(size_t) lo - 1] * peakVal > 0.0f && std::abs (residual[(size_t) lo - 1]) > half) --lo;
        while (hi < N - 1 && residual[(size_t) hi + 1] * peakVal > 0.0f && std::abs (residual[(size_t) hi + 1]) > half) ++hi;

        const double octaves = std::max ((double) (hi - lo + 1) / AnalysisGrid::pointsPerOctave, 1.0 / 3.0);

        BandSettings b;
        b.enabled = true;
        b.gainDb = peakVal;

        const double f = freqAt (peak);
        if (f >= 8000.0)
        {
            b.type = BandType::HighShelf;
            b.frequency = (float) std::clamp (f / 1.6, 5000.0, 10000.0);
            b.q = 0.707f;
        }
        else
        {
            b.type = BandType::Bell;
            b.frequency = (float) f;
            b.q = std::clamp (qFromOctaves (octaves), 0.4f, 4.0f);
        }

        const auto r = responseOf (b);
        for (int i = 0; i < N; ++i)
            residual[(size_t) i] -= r[(size_t) i];

        tone.push_back (b);
    }

    // 5a. The greedy pass can land two bands of the same kind almost on top of each other
    //     (most often two high shelves at the same corner). Merge those into one.
    for (size_t a = 0; a < tone.size(); ++a)
    {
        for (size_t b = a + 1; b < tone.size();)
        {
            const double ratio = std::max (tone[a].frequency, tone[b].frequency) / std::min (tone[a].frequency, tone[b].frequency);
            if (tone[a].type == tone[b].type && ratio < std::pow (2.0, 1.0 / 3.0))
            {
                tone[a].frequency = std::sqrt (tone[a].frequency * tone[b].frequency);
                tone[a].gainDb += tone[b].gainDb;
                tone[a].q = std::min (tone[a].q, tone[b].q);
                tone.erase (tone.begin() + (std::ptrdiff_t) b);
            }
            else
                ++b;
        }
    }

    // 5b. Re-balance gains together. A bell's dB shape scales almost linearly with its gain,
    //    so each pass solves a small weighted least-squares problem per band.
    for (int pass = 0; pass < 4 && ! tone.empty(); ++pass)
    {
        for (size_t b = 0; b < tone.size(); ++b)
        {
            GridCurve others {};
            for (size_t o = 0; o < tone.size(); ++o)
            {
                if (o == b) continue;
                const auto r = responseOf (tone[o]);
                for (int i = 0; i < N; ++i) others[(size_t) i] += r[(size_t) i];
            }

            auto unit = tone[b];
            unit.gainDb = 1.0f;
            const auto shape = responseOf (unit);

            double num = 0.0, den = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const double w = weight[(size_t) i];
                num += w * shape[(size_t) i] * (desired[(size_t) i] - others[(size_t) i]);
                den += w * shape[(size_t) i] * shape[(size_t) i];
            }

            if (den > 1.0e-6)
                tone[b].gainDb = (float) std::clamp (num / den, (double) -style.maxCutDb, (double) style.maxBoostDb);
        }
    }

    // 6. Drop bands that ended up doing almost nothing, and sort by frequency.
    tone.erase (std::remove_if (tone.begin(), tone.end(),
                                [&] (const BandSettings& b) { return std::abs (b.gainDb) < options.minUsefulGainDb; }),
                tone.end());
    std::sort (tone.begin(), tone.end(), [] (const BandSettings& a, const BandSettings& b) { return a.frequency < b.frequency; });

    for (auto& b : tone)
        b.gainDb = std::round (b.gainDb * 10.0f) / 10.0f;

    // Low cut first: every style starts by clearing rumble and handling noise below the voice.
    BandSettings lowCutBand;
    lowCutBand.enabled = true;
    lowCutBand.type = BandType::LowCut;
    lowCutBand.frequency = style.lowCutHz;
    lowCutBand.q = 0.707f;
    lowCutBand.slopeIndex = 1; // 24 dB/oct

    result.bands.push_back (lowCutBand);
    result.bands.insert (result.bands.end(), tone.begin(), tone.end());

    result.achieved = {};
    for (const auto& b : tone)
    {
        const auto r = responseOf (b);
        for (int i = 0; i < N; ++i) result.achieved[(size_t) i] += r[(size_t) i];
    }

    result.valid = true;
    return result;
}

} // namespace magiceq
