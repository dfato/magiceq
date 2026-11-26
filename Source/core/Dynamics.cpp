#include "Dynamics.h"
#include "StyleLibrary.h"

#include <cmath>

namespace magiceq
{

namespace
{
    float percentile (std::vector<float> v, float p)
    {
        if (v.empty()) return -60.0f;
        const size_t k = (size_t) std::clamp ((int) std::lround (p * (float) (v.size() - 1)), 0, (int) v.size() - 1);
        std::nth_element (v.begin(), v.begin() + (std::ptrdiff_t) k, v.end());
        return v[k];
    }

    float roundTo (float v, float step) { return std::round (v / step) * step; }
} // namespace

DynamicsProfile DynamicsFitter::measure (const std::vector<SpectrumAccumulator::FrameLevel>& frames)
{
    DynamicsProfile d;
    if (frames.size() < 4)
        return d;

    std::vector<float> levels;
    levels.reserve (frames.size());
    double crest = 0.0;
    for (const auto& f : frames)
    {
        levels.push_back (detectorLevelDb (f));
        crest += f.peakDb - f.rmsDb;
    }

    d.quietDb = percentile (levels, 0.10f);
    d.typicalDb = percentile (levels, 0.50f);
    d.loudDb = percentile (levels, 0.90f);
    d.crestDb = (float) (crest / (double) frames.size());
    d.valid = true;
    return d;
}

DynamicsFit DynamicsFitter::fit (const std::vector<SpectrumAccumulator::FrameLevel>& frames, int styleIndex, float strength)
{
    DynamicsFit result;
    result.profile = measure (frames);
    if (! result.profile.valid)
        return result;

    const auto& style = StyleLibrary::get (styleIndex);
    const auto& p = result.profile;
    strength = std::clamp (strength, 0.0f, 1.0f);

    // Start from the style's character (GUESSES, see docs/STYLE_CURVES.md) ...
    float ratio = style.compRatio;
    float targetGr = style.compTargetGainReductionDb;
    float attack = style.compAttackMs;
    float release = style.compReleaseMs;

    // ... then adapt it to this vocal.
    // A very dynamic performance (big gap between soft and loud lines) needs firmer control;
    // one that is already even (or already compressed) needs less.
    const float range = p.rangeDb();
    if (range > 18.0f)      { ratio *= 1.25f; targetGr *= 1.15f; }
    else if (range < 8.0f)  { ratio *= 0.75f; targetGr *= 0.65f; }

    // Spiky, transient-heavy vocals (hard consonants, shouting) get a slightly faster attack.
    if (p.crestDb > 14.0f) attack *= 0.7f;

    ratio = std::clamp (ratio, 1.5f, 10.0f);
    targetGr *= strength;

    CompSettings c;
    c.enabled = targetGr >= 0.5f;
    c.ratio = roundTo (ratio, 0.1f);
    c.attackMs = roundTo (std::clamp (attack, 0.5f, 60.0f), 0.1f);
    c.releaseMs = roundTo (std::clamp (release, 20.0f, 500.0f), 1.0f);
    c.kneeDb = 6.0f;
    c.mix = 1.0f;

    // Threshold: put the loud phrases `targetGr` dB into compression.
    c.thresholdDb = p.loudDb - targetGr / (1.0f - 1.0f / c.ratio);
    c.thresholdDb = roundTo (std::clamp (c.thresholdDb, -50.0f, -3.0f), 0.5f);

    // Expected gain reduction over the whole performance, using the same static curve the plugin runs.
    double sum = 0.0;
    std::vector<float> grs;
    grs.reserve (frames.size());
    for (const auto& f : frames)
    {
        const float gr = -compressorGainDb (detectorLevelDb (f), c.thresholdDb, c.ratio, c.kneeDb);
        sum += gr;
        grs.push_back (gr);
    }
    result.expectedAverageGrDb = (float) (sum / (double) frames.size());
    result.expectedLoudGrDb = percentile (grs, 0.90f);

    // Makeup: give back most of the typical level lost, so bypassing it is a fair comparison.
    c.makeupDb = c.enabled ? roundTo (std::clamp (result.expectedAverageGrDb * 0.9f, 0.0f, 12.0f), 0.1f) : 0.0f;

    result.comp = c;
    return result;
}

} // namespace magiceq
