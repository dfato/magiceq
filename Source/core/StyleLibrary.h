#pragma once

#include "AnalysisGrid.h"

#include <utility>
#include <vector>

namespace magiceq
{

/** A sound profile Detect tunes toward.

    IMPORTANT: every number in the built-in styles is a first-draft GUESS, written from
    general mixing practice. They are meant to be tuned by ear. See docs/STYLE_CURVES.md.
*/
struct StyleProfile
{
    const char* name;
    const char* genre;
    const char* description;

    /** Tone changes relative to the shared "polished vocal" baseline, as (Hz, dB) points.
        Values between points are interpolated on a log-frequency scale. */
    std::vector<std::pair<float, float>> offsets;

    float lowCutHz;     // where Detect places its high-pass (low cut) filter
    float maxBoostDb;   // the most Detect may boost any single region
    float maxCutDb;     // the most Detect may cut any single region (positive number)

    // Compressor character: Detect's starting point, adapted to each performance (see Dynamics.cpp).
    float compRatio;
    float compAttackMs;
    float compReleaseMs;
    float compTargetGainReductionDb;
};

class StyleLibrary
{
public:
    static const std::vector<StyleProfile>& all();
    static int numStyles() { return (int) all().size(); }
    static const StyleProfile& get (int index);

    /** The style's full target tone curve on the analysis grid (baseline + offsets), in dB. */
    static GridCurve targetCurve (int index);

    /** The shared "polished vocal" baseline, in dB per 1/6-octave band (a GUESS). */
    static GridCurve baselineCurve();

    /** Interpolates (Hz, dB) points on a log-frequency axis, holding the end values flat. */
    static float interpolate (const std::vector<std::pair<float, float>>& points, double frequency);
};

} // namespace magiceq
