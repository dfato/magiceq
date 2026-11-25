// MagicEQ automated tests: filter maths, the EQ engine, and the Detect pipeline.
// Run: ctest --test-dir build -C Release --output-on-failure   (or run the MagicEQTests program directly)

#include "core/Biquad.h"
#include "core/Dynamics.h"
#include "dsp/Compressor.h"
#include "dsp/Limiter.h"
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

static void testCompressor()
{
    std::puts ("Compressor");
    near (compressorGainDb (-30.0f, -18.0f, 4.0f, 0.0f), 0.0, 1e-6, "no gain change below the threshold");
    near (compressorGainDb (-6.0f, -18.0f, 4.0f, 0.0f), -9.0, 1e-4, "12 dB over at 4:1 gives 9 dB of reduction");
    near (compressorGainDb (-18.0f, -18.0f, 4.0f, 6.0f), -0.75 * 0.75, 1e-4, "soft knee eases in at the threshold");
    near (compressorGainDb (-24.0f, -18.0f, 4.0f, 6.0f), 0.0, 1e-4, "soft knee ends half a knee below the threshold");

    auto run = [] (const CompSettings& c, float amplitude)
    {
        Compressor comp;
        comp.setSettings (c);
        comp.prepare (fs);
        const int n = 48000;
        std::vector<float> l ((size_t) n), r ((size_t) n);
        for (int i = 0; i < n; ++i)
            l[(size_t) i] = r[(size_t) i] = amplitude * (float) std::sin (2.0 * 3.14159265358979 * 220.0 * i / fs);
        float maxGr = 0.0f;
        for (int start = 0; start < n; start += 256)
        {
            float* block[] = { l.data() + start, r.data() + start };
            maxGr = std::max (maxGr, comp.process (block, 2, std::min (256, n - start)));
        }
        float peak = 0.0f;
        for (int i = n * 3 / 4; i < n; ++i) peak = std::max (peak, std::abs (l[(size_t) i]));
        return std::pair<float, float> { 20.0f * std::log10 (peak), maxGr };
    };

    CompSettings c;
    c.enabled = true; c.thresholdDb = -18.0f; c.ratio = 4.0f; c.kneeDb = 0.0f; c.attackMs = 1.0f; c.releaseMs = 100.0f;
    auto [outDb, gr] = run (c, 0.5f); // -6 dBFS peaks
    near (outDb, -15.0, 1.0, "a -6 dBFS sine comes out near -15 dBFS (9 dB of reduction)");
    near (gr, 9.0, 1.0, "the meter reports about 9 dB of reduction");

    c.makeupDb = 6.0f;
    near (run (c, 0.5f).first, -9.0, 1.0, "makeup gain adds back 6 dB");

    c.makeupDb = 0.0f;
    c.mix = 0.0f;
    near (run (c, 0.5f).first, -6.0, 0.05, "mix at 0% passes the signal untouched");

    c.mix = 1.0f;
    c.enabled = false;
    near (run (c, 0.5f).first, -6.0, 0.05, "a switched-off compressor passes the signal untouched");
}

static std::vector<SpectrumAccumulator::FrameLevel> frames (float quietDb, float loudDb, float crestDb, int count)
{
    std::vector<SpectrumAccumulator::FrameLevel> f;
    for (int i = 0; i < count; ++i)
    {
        const float rms = quietDb + (loudDb - quietDb) * (float) i / (float) (count - 1);
        f.push_back ({ rms, rms + crestDb });
    }
    return f;
}

static void testDynamicsFit()
{
    std::puts ("Detect compressor settings");
    const int style = 0; // Modern Pop: about 6 dB on loud lines
    const auto& st = StyleLibrary::get (style);

    auto fit = DynamicsFitter::fit (frames (-34.0f, -16.0f, 10.0f, 400), style, 1.0f);
    check (fit.comp.enabled, "Detect switches the compressor on");
    near (fit.expectedLoudGrDb, st.compTargetGainReductionDb, 1.5, "loud lines get about the style's target reduction");
    check (fit.comp.makeupDb > 0.0f && fit.comp.makeupDb < fit.expectedLoudGrDb, "makeup gives back some, not all, of the reduction", fit.comp.makeupDb);
    near (fit.comp.attackMs, st.compAttackMs, 0.01, "attack comes from the style");

    // Louder recording, same performance: settings should move with the level, not change character.
    auto louder = DynamicsFitter::fit (frames (-24.0f, -6.0f, 10.0f, 400), style, 1.0f);
    near (louder.comp.thresholdDb - fit.comp.thresholdDb, 10.0, 0.6, "a 10 dB louder recording gets a 10 dB higher threshold");

    // An already-flat (or already compressed) vocal gets a gentler setting.
    auto flat = DynamicsFitter::fit (frames (-20.0f, -15.0f, 6.0f, 400), style, 1.0f);
    check (flat.comp.ratio < fit.comp.ratio, "an even vocal gets a lower ratio", flat.comp.ratio);
    check (flat.expectedLoudGrDb < fit.expectedLoudGrDb, "an even vocal gets less reduction", flat.expectedLoudGrDb);

    auto none = DynamicsFitter::fit (frames (-34.0f, -16.0f, 10.0f, 400), style, 0.0f);
    check (! none.comp.enabled, "Strength 0% leaves the compressor off");

    check (! DynamicsFitter::fit ({}, style, 1.0f).profile.valid, "no audio, no compressor decision");
}

static void testLimiter()
{
    std::puts ("Limiter");

    // A sine at a quarter of the sample rate, phased so every sample misses the crest:
    // samples peak at 0.707 but the real (true) peak is 1.0.
    {
        std::vector<float> x (4800);
        for (size_t i = 0; i < x.size(); ++i)
            x[i] = (float) std::sin (2.0 * 3.14159265358979 * 0.25 * (double) i + 3.14159265358979 / 4.0);
        near (Limiter::measureTruePeak (x.data(), (int) x.size()), 1.0, 0.03, "true-peak meter finds the peak between samples");
    }

    auto run = [] (const LimiterSettings& s, const std::vector<float>& input, std::vector<float>& output)
    {
        Limiter lim;
        lim.setSettings (s);
        lim.prepare (fs);
        lim.setSettings (s);
        output = input;
        std::vector<float> r = input;
        float maxGr = 0.0f;
        for (size_t start = 0; start < output.size(); start += 333)
        {
            const int n = (int) std::min<size_t> (333, output.size() - start);
            float* ch[] = { output.data() + start, r.data() + start };
            maxGr = std::max (maxGr, lim.process (ch, 2, n));
        }
        return std::pair<int, float> { lim.getLatencySamples(), maxGr };
    };

    // Loud, spiky material: noise bursts and a sine, peaking about 8 dB over the ceiling.
    std::mt19937 rng (5);
    std::normal_distribution<float> noise (0.0f, 0.35f);
    std::vector<float> loud (48000);
    for (size_t i = 0; i < loud.size(); ++i)
    {
        const double t = (double) i / fs;
        const float burst = std::fmod (t, 0.25) < 0.05 ? noise (rng) : 0.0f;
        loud[i] = 0.9f * (float) std::sin (2.0 * 3.14159265358979 * 3100.0 * t) + burst;
    }

    LimiterSettings s;
    s.ceilingDb = -1.0f;
    s.truePeak = false;
    std::vector<float> out;
    auto [latency, gr] = run (s, loud, out);
    float peak = 0.0f;
    for (float v : out) peak = std::max (peak, std::abs (v));
    check (20.0 * std::log10 (peak) <= -1.0 + 0.01, "sample-peak mode never goes over the ceiling", 20.0 * std::log10 (peak));
    check (gr > 3.0f, "the limiter reports its gain reduction", gr);

    s.truePeak = true;
    run (s, loud, out);
    const float tp = Limiter::measureTruePeak (out.data(), (int) out.size());
    check (20.0 * std::log10 (tp) <= -1.0 + 0.3, "true-peak mode keeps between-sample peaks at the ceiling", 20.0 * std::log10 (tp));

    // Quiet material passes untouched, just delayed by the reported latency.
    std::vector<float> quiet (4800, 0.0f);
    quiet[100] = 0.5f;
    run (s, quiet, out);
    check (latency > 0 && latency < 100, "latency is small (about 1.2 ms)", latency);
    near (out[(size_t) (100 + latency)], 0.5, 1e-6, "below the ceiling the signal is untouched, only delayed by the reported latency");

    // Switched off: same delay (so the DAW's timing doesn't jump), no limiting.
    s.enabled = false;
    run (s, loud, out);
    near (out[(size_t) (2000 + latency)], loud[2000], 1e-6, "switched off it passes audio unchanged, with the same delay");
}

int main()
{
    testFilterMaths();
    testEngine();
    testGate();
    testDetect();
    testCompressor();
    testDynamicsFit();
    testLimiter();

    std::printf ("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
