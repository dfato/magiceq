#include "Biquad.h"

#include <algorithm>
#include <complex>

namespace magiceq
{

namespace
{
    constexpr double pi = 3.14159265358979323846;

    BiquadCoeffs normalise (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        BiquadCoeffs c;
        c.b0 = b0 / a0;
        c.b1 = b1 / a0;
        c.b2 = b2 / a0;
        c.a1 = a1 / a0;
        c.a2 = a2 / a0;
        return c;
    }

    BiquadCoeffs lowPass (double f, double q, double fs)
    {
        const double w0 = 2.0 * pi * f / fs, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        return normalise ((1.0 - cw) / 2.0, 1.0 - cw, (1.0 - cw) / 2.0, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
    }

    BiquadCoeffs highPass (double f, double q, double fs)
    {
        const double w0 = 2.0 * pi * f / fs, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        return normalise ((1.0 + cw) / 2.0, -(1.0 + cw), (1.0 + cw) / 2.0, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
    }

    BiquadCoeffs peak (double f, double q, double gainDb, double fs)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * pi * f / fs, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        return normalise (1.0 + alpha * A, -2.0 * cw, 1.0 - alpha * A, 1.0 + alpha / A, -2.0 * cw, 1.0 - alpha / A);
    }

    BiquadCoeffs lowShelf (double f, double q, double gainDb, double fs)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * pi * f / fs, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double s = 2.0 * std::sqrt (A) * alpha;
        return normalise (A * ((A + 1.0) - (A - 1.0) * cw + s),
                          2.0 * A * ((A - 1.0) - (A + 1.0) * cw),
                          A * ((A + 1.0) - (A - 1.0) * cw - s),
                          (A + 1.0) + (A - 1.0) * cw + s,
                          -2.0 * ((A - 1.0) + (A + 1.0) * cw),
                          (A + 1.0) + (A - 1.0) * cw - s);
    }

    BiquadCoeffs highShelf (double f, double q, double gainDb, double fs)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * pi * f / fs, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double s = 2.0 * std::sqrt (A) * alpha;
        return normalise (A * ((A + 1.0) + (A - 1.0) * cw + s),
                          -2.0 * A * ((A - 1.0) + (A + 1.0) * cw),
                          A * ((A + 1.0) + (A - 1.0) * cw - s),
                          (A + 1.0) - (A - 1.0) * cw + s,
                          2.0 * ((A - 1.0) - (A + 1.0) * cw),
                          (A + 1.0) - (A - 1.0) * cw - s);
    }

    BiquadCoeffs notch (double f, double q, double fs)
    {
        const double w0 = 2.0 * pi * f / fs, cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        return normalise (1.0, -2.0 * cw, 1.0, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
    }

    /** Q of section k (0-based) in an even-order Butterworth filter of the given order. */
    double butterworthQ (int order, int k)
    {
        return 1.0 / (2.0 * std::sin ((2.0 * k + 1.0) * pi / (2.0 * order)));
    }
} // namespace

double BiquadCoeffs::magnitudeAt (double frequency, double sampleRate) const noexcept
{
    const double w = 2.0 * pi * frequency / sampleRate;
    const std::complex<double> z1 = std::polar (1.0, -w);
    const std::complex<double> z2 = z1 * z1;
    const auto num = b0 + b1 * z1 + b2 * z2;
    const auto den = 1.0 + a1 * z1 + a2 * z2;
    return std::abs (num / den);
}

BandDesign designBand (const BandSettings& band, double sampleRate)
{
    BandDesign d;

    // Keep the design stable near Nyquist.
    const double f = std::clamp ((double) band.frequency, 10.0, sampleRate * 0.49);
    const double q = std::clamp ((double) band.q, 0.1, 40.0);
    const double g = band.gainDb;

    switch (band.type)
    {
        case BandType::Bell:      d.sections[0] = peak (f, q, g, sampleRate);      d.numSections = 1; break;
        case BandType::LowShelf:  d.sections[0] = lowShelf (f, q, g, sampleRate);  d.numSections = 1; break;
        case BandType::HighShelf: d.sections[0] = highShelf (f, q, g, sampleRate); d.numSections = 1; break;
        case BandType::Notch:     d.sections[0] = notch (f, q, sampleRate);        d.numSections = 1; break;

        case BandType::LowCut:
        case BandType::HighCut:
        {
            const int slope = std::clamp (band.slopeIndex, 0, numCutSlopes - 1);
            const int order = cutSlopesDbPerOct[slope] / 6;
            d.numSections = order / 2;

            for (int k = 0; k < d.numSections; ++k)
            {
                // A 12 dB/oct cut uses the band's own Q (so it can have a resonant bump);
                // steeper cuts use Butterworth Qs for a clean, flat passband.
                const double sectionQ = (d.numSections == 1) ? q : butterworthQ (order, k);
                d.sections[(size_t) k] = band.type == BandType::LowCut ? highPass (f, sectionQ, sampleRate)
                                                                         : lowPass (f, sectionQ, sampleRate);
            }
            break;
        }

        case BandType::numTypes:
        default: break;
    }

    return d;
}

double bandResponseDb (const BandSettings& band, double frequency, double sampleRate)
{
    if (! band.enabled)
        return 0.0;

    const auto d = designBand (band, sampleRate);
    double mag = 1.0;
    for (int i = 0; i < d.numSections; ++i)
        mag *= d.sections[(size_t) i].magnitudeAt (frequency, sampleRate);

    return 20.0 * std::log10 (std::max (mag, 1.0e-9));
}

} // namespace magiceq
