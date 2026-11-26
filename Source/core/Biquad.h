#pragma once

#include <array>
#include <cmath>

namespace magiceq
{

/** The shapes a MagicEQ band can take. The order matches the "Type" parameter. */
enum class BandType : int
{
    Bell = 0,
    LowShelf,
    HighShelf,
    LowCut,
    HighCut,
    Notch,
    numTypes
};

/** Slopes for the low/high cut types, in dB per octave. The order matches the "Slope" parameter. */
inline constexpr int cutSlopesDbPerOct[] = { 12, 24, 36, 48 };
inline constexpr int numCutSlopes = 4;

/** Everything that describes one EQ band, independent of the plugin's parameter system. */
struct BandSettings
{
    bool enabled = false;
    BandType type = BandType::Bell;
    float frequency = 1000.0f; // Hz
    float gainDb = 0.0f;       // ignored by cuts and notch
    float q = 0.707f;
    int slopeIndex = 1;        // index into cutSlopesDbPerOct, only used by cuts
};

/** Normalised (a0 == 1) second-order filter coefficients. */
struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    /** Magnitude of this section's response at a frequency, as a linear gain. */
    double magnitudeAt (double frequency, double sampleRate) const noexcept;
};

/** A band can need up to four cascaded sections (a 48 dB/oct cut). */
inline constexpr int maxSectionsPerBand = 4;

struct BandDesign
{
    std::array<BiquadCoeffs, maxSectionsPerBand> sections {};
    int numSections = 0;
};

/** Designs the sections for one band (RBJ "Audio EQ Cookbook" formulas plus Butterworth cascades for steep cuts). */
BandDesign designBand (const BandSettings& band, double sampleRate);

/** The band's response at a frequency, in dB. Disabled bands return 0. */
double bandResponseDb (const BandSettings& band, double frequency, double sampleRate);

/** A stereo transposed direct form II biquad. */
struct BiquadState
{
    double z1[2] { 0.0, 0.0 };
    double z2[2] { 0.0, 0.0 };

    inline float process (const BiquadCoeffs& c, float input, int channel) noexcept
    {
        const double x = input;
        const double y = c.b0 * x + z1[channel];
        z1[channel] = c.b1 * x - c.a1 * y + z2[channel];
        z2[channel] = c.b2 * x - c.a2 * y;
        return (float) y;
    }

    void reset() noexcept { z1[0] = z1[1] = z2[0] = z2[1] = 0.0; }
};

} // namespace magiceq
