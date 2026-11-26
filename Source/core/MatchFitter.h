#pragma once

#include "AnalysisGrid.h"
#include "Biquad.h"

#include <vector>

namespace magiceq
{

/** What Detect decided, plus the curves it used (the editor draws them). */
struct MatchResult
{
    std::vector<BandSettings> bands; // low cut first, then bells/shelves sorted by frequency
    GridCurve measured {};           // your vocal, level-matched to the target
    GridCurve target {};             // the selected style's tone curve
    GridCurve correction {};         // the EQ curve Detect was aiming for
    GridCurve achieved {};           // what the chosen bands actually do
    bool valid = false;
};

/** Turns "your vocal's average spectrum" + "the style's target" into a small set of EQ bands.

    The approach: work out the difference between the two curves, scale it by Strength,
    limit it to sensible amounts, smooth it so it describes broad tonal moves instead of
    every little wiggle, then repeatedly place a band on the biggest remaining difference.
    A final pass re-balances the band gains together (least squares) so overlapping bands
    don't overshoot.
*/
class MatchFitter
{
public:
    struct Options
    {
        float strength = 1.0f;  // 0..1, how far toward the target to move
        int maxToneBands = 7;   // bands Detect may use besides the low cut (leaves room for yours)
        float minUsefulGainDb = 0.75f;
    };

    static MatchResult fit (const GridCurve& measuredSpectrum, int styleIndex, const Options& options);
};

} // namespace magiceq
