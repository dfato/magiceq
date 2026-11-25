#include "StyleLibrary.h"

#include <algorithm>
#include <cmath>

namespace magiceq
{

// ---------------------------------------------------------------------------
// FIRST-DRAFT GUESSES. Every curve below needs to be checked by ear against
// reference records before release. See docs/STYLE_CURVES.md for the review list.
// ---------------------------------------------------------------------------

namespace
{
    // Long-term average spectrum of a "finished, radio-ready" lead vocal, measured as
    // energy per 1/6 octave and normalised so the 250-350 Hz region sits at 0 dB.
    // Below the low cut is ignored by Detect, so the low end only needs to be roughly right.
    const std::vector<std::pair<float, float>> baselinePoints {
        {    40.0f, -30.0f }, {    60.0f, -20.0f }, {    90.0f, -10.0f }, {   130.0f,  -4.0f },
        {   180.0f,  -1.0f }, {   250.0f,   0.0f }, {   350.0f,   0.0f }, {   500.0f,  -0.5f },
        {   700.0f,  -1.5f }, {  1000.0f,  -3.0f }, {  1500.0f,  -5.0f }, {  2000.0f,  -6.5f },
        {  3000.0f,  -8.0f }, {  4000.0f,  -9.0f }, {  5000.0f, -10.0f }, {  6500.0f, -11.5f },
        {  8000.0f, -13.0f }, { 10000.0f, -15.0f }, { 12500.0f, -17.5f }, { 16000.0f, -22.0f },
    };

    std::vector<StyleProfile> makeStyles()
    {
        //  name, genre, description,
        //  offsets {Hz, dB},
        //  lowCut, maxBoost, maxCut, ratio, attack, release, targetGR
        return {
            { "Modern Pop", "Pop", "Bright, forward and polished with controlled lows and lifted air.",
              { { 250, -1.5f }, { 400, -1.0f }, { 1000, 0 }, { 3000, 1.5f }, { 5000, 2.0f }, { 10000, 3.0f }, { 14000, 3.0f } },
              90, 6, 9, 4.0f, 5, 60, 6 },
            { "Bedroom Pop", "Pop", "Soft and intimate, warm body with gentle sparkle.",
              { { 150, 1.0f }, { 300, 0 }, { 2500, -0.5f }, { 6000, 1.0f }, { 12000, 2.0f } },
              80, 5, 8, 3.0f, 15, 120, 4 },
            { "Dance Pop", "Pop", "Thin low mids, strong presence to cut through dense tracks.",
              { { 300, -2.0f }, { 2500, 2.0f }, { 5000, 2.5f }, { 12000, 3.5f } },
              110, 6, 9, 5.0f, 3, 50, 8 },

            { "Trap", "Hip-Hop", "Clear and crisp on top of heavy 808s, low mids tucked away.",
              { { 200, 0.5f }, { 400, -2.0f }, { 800, -1.0f }, { 3000, 2.0f }, { 6000, 3.0f }, { 12000, 4.0f } },
              100, 6, 9, 6.0f, 2, 40, 8 },
            { "Boom Bap", "Hip-Hop", "Chesty and punchy with a slightly darker top.",
              { { 150, 1.5f }, { 300, 0.5f }, { 1000, 1.0f }, { 2500, 1.5f }, { 8000, 0 }, { 12000, -1.0f } },
              90, 5, 8, 4.0f, 10, 80, 6 },
            { "Melodic Rap", "Hip-Hop", "Smooth and present, airy on the sung parts.",
              { { 300, -1.5f }, { 3000, 1.5f }, { 8000, 2.5f }, { 12000, 3.0f } },
              100, 6, 9, 4.0f, 5, 60, 6 },
            { "Drill", "Hip-Hop", "Aggressive upper mids, tight and controlled.",
              { { 250, -1.0f }, { 500, -1.5f }, { 1500, 1.0f }, { 4000, 3.0f }, { 8000, 2.0f }, { 12000, 2.0f } },
              110, 6, 9, 6.0f, 1, 40, 9 },

            { "Contemporary R&B", "R&B", "Silky top, warm low end, scooped boxiness.",
              { { 150, 1.0f }, { 300, -1.0f }, { 1000, 0 }, { 3000, 1.0f }, { 6000, 1.5f }, { 12000, 3.5f } },
              80, 6, 8, 3.0f, 10, 100, 5 },
            { "Neo-Soul", "R&B", "Warm, round and natural, little extra top.",
              { { 150, 1.5f }, { 300, 0.5f }, { 800, 0.5f }, { 3000, 0 }, { 6000, -0.5f }, { 10000, 1.0f } },
              70, 5, 7, 3.0f, 20, 150, 4 },

            { "Alt Rock", "Rock", "Mid-forward and gritty so it sits inside guitars.",
              { { 200, -0.5f }, { 500, 0.5f }, { 1200, 1.5f }, { 2500, 2.0f }, { 5000, 1.0f }, { 12000, 1.0f } },
              110, 5, 8, 5.0f, 8, 80, 7 },
            { "Pop Punk", "Rock", "Bright, upfront and tightly compressed.",
              { { 300, -1.5f }, { 1500, 1.5f }, { 3000, 2.5f }, { 6000, 1.5f }, { 12000, 2.0f } },
              120, 6, 9, 6.0f, 3, 50, 9 },
            { "Indie", "Rock", "Natural and a little lo-fi, mild presence and air.",
              { { 200, 0.5f }, { 400, 0 }, { 1000, 1.0f }, { 3000, 0.5f }, { 8000, 1.0f }, { 12000, 1.5f } },
              90, 4, 7, 3.0f, 15, 120, 4 },

            { "Modern Country", "Country", "Clear storytelling vocal, present and shiny.",
              { { 250, -1.0f }, { 1000, 0.5f }, { 3000, 2.0f }, { 6000, 2.0f }, { 12000, 2.5f } },
              100, 6, 8, 4.0f, 8, 80, 6 },
            { "Folk / Acoustic", "Country", "Honest and open, light touch.",
              { { 150, 1.0f }, { 400, 0 }, { 2000, 0.5f }, { 5000, 0.5f }, { 10000, 1.0f } },
              75, 4, 6, 2.5f, 20, 150, 3 },

            { "Gospel / Worship", "Gospel", "Big and full, with room for dynamic peaks.",
              { { 150, 1.0f }, { 350, -0.5f }, { 1000, 0.5f }, { 3000, 1.5f }, { 8000, 1.5f }, { 12000, 2.5f } },
              70, 5, 8, 3.0f, 15, 120, 5 },

            { "Afrobeats", "World", "Smooth, bright and relaxed, clean low mids.",
              { { 300, -1.5f }, { 2000, 1.0f }, { 5000, 2.0f }, { 10000, 3.0f }, { 14000, 3.0f } },
              100, 6, 8, 4.0f, 5, 60, 6 },
            { "Reggaeton / Latin", "World", "Upfront and crisp, tight low mids.",
              { { 300, -2.0f }, { 1500, 1.0f }, { 4000, 2.5f }, { 10000, 3.0f } },
              110, 6, 9, 5.0f, 3, 50, 7 },

            { "EDM Topline", "Electronic", "Thin, bright and very present to ride over big drops.",
              { { 250, -2.5f }, { 500, -1.0f }, { 3000, 2.0f }, { 6000, 3.0f }, { 12000, 4.5f } },
              130, 7, 9, 6.0f, 2, 40, 9 },
            { "Lo-Fi", "Electronic", "Warm and dark, rolled-off top.",
              { { 120, 1.0f }, { 300, 1.0f }, { 2000, -1.0f }, { 5000, -3.0f }, { 10000, -6.0f }, { 14000, -8.0f } },
              100, 4, 9, 4.0f, 20, 150, 6 },

            { "Podcast / Spoken", "Spoken", "Clear, intelligible speech with gentle warmth.",
              { { 120, 1.0f }, { 300, -1.0f }, { 2500, 2.0f }, { 5000, 1.0f }, { 10000, -0.5f }, { 14000, -1.0f } },
              80, 5, 8, 3.0f, 10, 150, 6 },
            { "Voiceover / Broadcast", "Spoken", "Close, deep and authoritative.",
              { { 120, 2.0f }, { 250, -0.5f }, { 3000, 2.5f }, { 6000, 1.5f }, { 12000, 1.0f } },
              70, 6, 8, 4.0f, 5, 100, 8 },
        };
    }
} // namespace

const std::vector<StyleProfile>& StyleLibrary::all()
{
    static const std::vector<StyleProfile> styles = makeStyles();
    return styles;
}

const StyleProfile& StyleLibrary::get (int index)
{
    const auto& s = all();
    return s[(size_t) std::clamp (index, 0, (int) s.size() - 1)];
}

float StyleLibrary::interpolate (const std::vector<std::pair<float, float>>& points, double frequency)
{
    if (points.empty())
        return 0.0f;
    if (frequency <= points.front().first)
        return points.front().second;
    if (frequency >= points.back().first)
        return points.back().second;

    for (size_t i = 1; i < points.size(); ++i)
    {
        if (frequency <= points[i].first)
        {
            const double l0 = std::log2 ((double) points[i - 1].first);
            const double l1 = std::log2 ((double) points[i].first);
            const double t = (std::log2 (frequency) - l0) / (l1 - l0);
            return (float) (points[i - 1].second + t * (points[i].second - points[i - 1].second));
        }
    }
    return points.back().second;
}

GridCurve StyleLibrary::baselineCurve()
{
    GridCurve c {};
    for (int i = 0; i < AnalysisGrid::numPoints; ++i)
        c[(size_t) i] = interpolate (baselinePoints, AnalysisGrid::frequencyAt (i));
    return c;
}

GridCurve StyleLibrary::targetCurve (int index)
{
    const auto& style = get (index);
    auto c = baselineCurve();
    for (int i = 0; i < AnalysisGrid::numPoints; ++i)
        c[(size_t) i] += interpolate (style.offsets, AnalysisGrid::frequencyAt (i));
    return c;
}

} // namespace magiceq
