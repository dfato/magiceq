#pragma once

#include "SpectrumAccumulator.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace magiceq
{

/** Everything that describes the compressor, independent of the plugin's parameter system. */
struct CompSettings
{
    bool enabled = false;
    float thresholdDb = -18.0f;
    float ratio = 3.0f;
    float attackMs = 10.0f;
    float releaseMs = 100.0f;
    float kneeDb = 6.0f;
    float makeupDb = 0.0f;
    float mix = 1.0f; // 0..1, below 1 blends in the uncompressed signal (parallel compression)
};

/** The static curve: how many dB of gain change a given input level gets (always <= 0).
    Soft knee as in Giannoulis, Massberg & Reiss, "Digital Dynamic Range Compressor Design" (JAES 2012). */
inline float compressorGainDb (float inputDb, float thresholdDb, float ratio, float kneeDb) noexcept
{
    const float over = inputDb - thresholdDb;
    const float slope = 1.0f / ratio - 1.0f;

    if (kneeDb > 0.0f && 2.0f * std::abs (over) <= kneeDb)
    {
        const float x = over + kneeDb * 0.5f;
        return slope * x * x / (2.0f * kneeDb);
    }
    return over > 0.0f ? slope * over : 0.0f;
}

/** What Detect learned about the vocal's dynamics, shown to the user and used to set the compressor. */
struct DynamicsProfile
{
    float quietDb = -60.0f;  // 10th percentile frame level (softer phrases)
    float typicalDb = -60.0f; // median
    float loudDb = -60.0f;   // 90th percentile (belted / loud phrases)
    float crestDb = 0.0f;    // average peak-to-RMS ratio: how spiky the vocal is
    bool valid = false;

    float rangeDb() const noexcept { return loudDb - quietDb; }
};

struct DynamicsFit
{
    DynamicsProfile profile;
    CompSettings comp;
    float expectedAverageGrDb = 0.0f; // average gain reduction over the analysed vocal
    float expectedLoudGrDb = 0.0f;    // gain reduction on the loud phrases
};

class DynamicsFitter
{
public:
    static DynamicsProfile measure (const std::vector<SpectrumAccumulator::FrameLevel>& frames);

    /** Chooses compressor settings for the style. `strength` (0..1) scales how hard it works. */
    static DynamicsFit fit (const std::vector<SpectrumAccumulator::FrameLevel>& frames, int styleIndex, float strength);

    /** The level the compressor's peak-sensing detector roughly "sees" for a frame. */
    static float detectorLevelDb (const SpectrumAccumulator::FrameLevel& f) noexcept
    {
        // Between RMS and peak: attack smoothing catches most, but not all, of each peak.
        return f.rmsDb + 0.5f * (f.peakDb - f.rmsDb);
    }
};

} // namespace magiceq
