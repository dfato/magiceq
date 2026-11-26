// MagicEQ automated tests: filter maths, the EQ engine, and the Detect pipeline.
// Run: ctest --test-dir build -C Release --output-on-failure   (or run the MagicEQTests program directly)

#include "core/Biquad.h"
#include "core/MatchFitter.h"
#include "core/SpectrumAccumulator.h"
#include "core/StyleLibrary.h"
#include "dsp/EqEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

using namespace magiceq;

namespace
{
    int failures = 0, checks = 0;

    void check (bool ok, const std::string& what, double value = 0.0)
    {
        ++checks;
        if (! ok)
        {
            ++failures;
            std::printf ("  FAIL: %s (value %.3f)\n", what.c_str(), value);
        }
    }

    void near (double value, double expected, double tolerance, const std::string& what)
    {
        check (std::abs (value - expected) <= tolerance,
               what + " expected " + std::to_string (expected) + " +/- " + std::to_string (tolerance), value);
    }

    constexpr double fs = 48000.0;

    BandSettings band (BandType t, float f, float g, float q, int slope = 1)
    {
        BandSettings b;
        b.enabled = true; b.type = t; b.frequency = f; b.gainDb = g; b.q = q; b.slopeIndex = slope;
        return b;
    }

    double totalResponse (const std::vector<BandSettings>& bands, double f)
    {
        double sum = 0.0;
        for (const auto& b : bands)
            if (b.type != BandType::LowCut) // ignore the low cut when judging tone
                sum += bandResponseDb (b, f, fs);
        return sum;
    }

    /** A noisy, voice-like test signal whose average spectrum follows `curveDb` (energy per 1/6 octave),
        made from many sine partials at random log-spaced frequencies, with a slow level wobble. */
    std::vector<float> synthesise (const std::function<double (double)>& curveDb, double seconds, unsigned seed)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<double> uni (0.0, 1.0);
        const int numPartials = 360;
        const int n = (int) (seconds * fs);
        std::vector<double> out ((size_t) n, 0.0);

        for (int p = 0; p < numPartials; ++p)
        {
            const double f = 40.0 * std::pow (2.0, uni (rng) * std::log2 (16000.0 / 40.0));
            const double amp = std::pow (10.0, curveDb (f) / 20.0);
            const double w = 2.0 * 3.14159265358979 * f / fs;
            double phase = uni (rng) * 6.283185307;
            // Rotating phasor avoids calling sin() for every sample.
            double re = std::cos (phase), im = std::sin (phase);
            const double cr = std::cos (w), ci = std::sin (w);
            for (int i = 0; i < n; ++i)
            {
                out[(size_t) i] += amp * im;
                const double nr = re * cr - im * ci;
                im = re * ci + im * cr;
                re = nr;
            }
        }

        double peak = 0.0;
        for (double v : out) peak = std::max (peak, std::abs (v));
        std::vector<float> result ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const double wobble = 0.6 + 0.4 * std::sin (2.0 * 3.14159265358979 * 0.7 * i / fs);
            result[(size_t) i] = (float) (out[(size_t) i] / peak * 0.5 * wobble);
        }
        return result;
    }

    double styleCurveAt (int style, double f)
    {
        // Interpolate the style's target curve (defined on the analysis grid) at any frequency.
        const auto c = StyleLibrary::targetCurve (style);
        const double pos = std::log2 (f / AnalysisGrid::lowestHz) * AnalysisGrid::pointsPerOctave;
        const int i = std::clamp ((int) pos, 0, AnalysisGrid::numPoints - 2);
        const double t = std::clamp (pos - i, 0.0, 1.0);
        return c[(size_t) i] + t * (c[(size_t) i + 1] - c[(size_t) i]);
    }

    double bumpDb (double f, double centre, double gainDb, double octaves)
    {
        const double d = std::log2 (f / centre) / (octaves * 0.5);
        return gainDb * std::exp (-d * d);
    }
} // namespace

static void testFilterMaths()
{
    std::puts ("Filter maths");
    near (bandResponseDb (band (BandType::Bell, 1000, 6, 1), 1000, fs), 6.0, 0.05, "bell +6 dB at its centre");
    near (bandResponseDb (band (BandType::Bell, 1000, -9, 2), 1000, fs), -9.0, 0.05, "bell -9 dB at its centre");
    near (bandResponseDb (band (BandType::Bell, 1000, 6, 1), 100, fs), 0.0, 0.3, "bell is flat far away");
    near (bandResponseDb (band (BandType::LowShelf, 200, 6, 0.707f), 30, fs), 6.0, 0.3, "low shelf reaches its gain");
    near (bandResponseDb (band (BandType::HighShelf, 5000, -4, 0.707f), 18000, fs), -4.0, 0.4, "high shelf reaches its gain");
    near (bandResponseDb (band (BandType::LowCut, 100, 0, 0.707f, 1), 100, fs), -3.0, 0.2, "24 dB/oct low cut is -3 dB at its corner");
    near (bandResponseDb (band (BandType::LowCut, 100, 0, 0.707f, 1), 50, fs), -24.0, 1.5, "24 dB/oct low cut, one octave down");
    near (bandResponseDb (band (BandType::LowCut, 100, 0, 0.707f, 3), 50, fs), -48.0, 2.0, "48 dB/oct low cut, one octave down");
    near (bandResponseDb (band (BandType::HighCut, 2000, 0, 0.707f, 0), 4000, fs), -12.0, 1.5, "12 dB/oct high cut, one octave up");
    check (bandResponseDb (band (BandType::Notch, 3000, 0, 8), 3000, fs) < -40.0, "notch is deep at its centre");

    auto off = band (BandType::Bell, 1000, 12, 1);
    off.enabled = false;
    near (bandResponseDb (off, 1000, fs), 0.0, 1e-9, "a disabled band does nothing");
}

static void testEngine()
{
    std::puts ("EQ engine");
    EqEngine eq;
    eq.setTarget (0, band (BandType::Bell, 1000, 6, 1));
    eq.prepare (fs);

    const int n = 48000;
    std::vector<float> l ((size_t) n), r ((size_t) n);
    for (int i = 0; i < n; ++i)
        l[(size_t) i] = r[(size_t) i] = 0.25f * (float) std::sin (2.0 * 3.14159265358979 * 1000.0 * i / fs);

    float* ch[] = { l.data(), r.data() };
    for (int start = 0; start < n; start += 512)
    {
        float* block[] = { ch[0] + start, ch[1] + start };
        eq.process (block, 2, std::min (512, n - start));
    }

    float peak = 0.0f;
    for (int i = n / 2; i < n; ++i) peak = std::max (peak, std::abs (l[(size_t) i]));
    near (20.0 * std::log10 (peak / 0.25), 6.0, 0.1, "1 kHz sine is boosted 6 dB by a 1 kHz bell");
    near (l[(size_t) n - 1], r[(size_t) n - 1], 1e-6, "left and right are processed identically");
}

static void testGate()
{
    std::puts ("Detect level gate");
    SpectrumAccumulator acc;
    acc.prepare (fs);

    std::vector<float> silence ((size_t) fs * 3, 0.0f);
    acc.push (silence.data(), (int) silence.size());
    near (acc.getActiveSeconds(), 0.0, 1e-9, "silence does not count as listening time");

    // 2 s of voice-like signal, then 3 s of silence: only the 2 s should count.
    auto voice = synthesise ([] (double f) { return styleCurveAt (0, f); }, 2.0, 7);
    acc.push (voice.data(), (int) voice.size());
    acc.push (silence.data(), (int) silence.size());
    near (acc.getActiveSeconds(), 2.0, 0.15, "only audible frames count toward the listen time");
}

static MatchResult detect (const std::function<double (double)>& curve, int style, float strength, unsigned seed)
{
    SpectrumAccumulator acc;
    acc.prepare (fs);
    auto signal = synthesise (curve, 8.0, seed);
    acc.push (signal.data(), (int) signal.size());

    GridCurve spectrum {};
    check (acc.getAverageSpectrum (spectrum), "accumulator produced a spectrum");

    MatchFitter::Options opt;
    opt.strength = strength;
    return MatchFitter::fit (spectrum, style, opt);
}

static void testDetect()
{
    std::puts ("Detect end to end");
    const int style = 0; // Modern Pop

    // A vocal that already matches the style should be left nearly alone.
    {
        auto m = detect ([&] (double f) { return styleCurveAt (style, f); }, style, 1.0f, 11);
        double worst = 0.0;
        for (double f : { 300.0, 700.0, 1500.0, 3000.0, 6000.0 })
            worst = std::max (worst, std::abs (totalResponse (m.bands, f)));
        check (worst < 2.0, "a vocal that already matches gets only small changes", worst);
        check (m.bands.front().type == BandType::LowCut, "Detect always starts with a low cut");
    }

    // A boxy (+6 dB at 400 Hz) and dull (-5 dB around 4 kHz) vocal should be corrected the other way.
    {
        auto curve = [&] (double f) { return styleCurveAt (style, f) + bumpDb (f, 400, 6.0, 1.0) + bumpDb (f, 4000, -5.0, 1.2); };
        auto m = detect (curve, style, 1.0f, 21);
        const double at400 = totalResponse (m.bands, 400), at4k = totalResponse (m.bands, 4000), at1500 = totalResponse (m.bands, 1500);
        std::printf ("  boxy+dull vocal -> %d tone bands, 400 Hz %.1f dB, 1.5 kHz %.1f dB, 4 kHz %.1f dB\n",
                     (int) m.bands.size() - 1, at400, at1500, at4k);
        near (at400, -6.0, 2.0, "Detect cuts the boxy 400 Hz build-up");
        near (at4k, 5.0, 2.0, "Detect lifts the dull 4 kHz presence region");
        check (std::abs (at1500) < 2.5, "Detect leaves the in-between region mostly alone", at1500);
        check ((int) m.bands.size() <= 8, "Detect leaves bands free for you", (double) m.bands.size());

        // Half strength should do about half as much.
        auto half = detect (curve, style, 0.5f, 21);
        near (totalResponse (half.bands, 400), at400 * 0.5, 1.5, "Strength 50% does about half the cut");
    }

    // Every style should give sane, finite settings.
    for (int s = 0; s < StyleLibrary::numStyles(); ++s)
    {
        auto m = detect ([] (double f) { return styleCurveAt (0, f) + bumpDb (f, 250, 4.0, 1.0); }, s, 1.0f, 31 + (unsigned) s);
        bool ok = m.valid;
        for (const auto& b : m.bands)
            ok = ok && std::isfinite (b.gainDb) && std::isfinite (b.q) && b.frequency >= 20.0f && b.frequency <= 20000.0f
                    && std::abs (b.gainDb) <= 12.0f;
        check (ok, std::string ("style '") + StyleLibrary::get (s).name + "' gives sane settings");
    }
}

int main()
{
    testFilterMaths();
    testEngine();
    testGate();
    testDetect();

    std::printf ("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
