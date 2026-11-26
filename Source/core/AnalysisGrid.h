#pragma once

#include <array>
#include <cmath>

namespace magiceq
{

/** Detect compares spectra on a fixed grid of 1/6-octave points from 40 Hz to about 16 kHz. */
struct AnalysisGrid
{
    static constexpr double lowestHz = 40.0;
    static constexpr int pointsPerOctave = 6;
    static constexpr int numPoints = 53; // 40 Hz * 2^(52/6) ~= 16.2 kHz

    static double frequencyAt (int index) noexcept
    {
        return lowestHz * std::pow (2.0, (double) index / pointsPerOctave);
    }
};

using GridCurve = std::array<float, AnalysisGrid::numPoints>;

} // namespace magiceq
