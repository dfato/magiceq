#include "SpectrumAccumulator.h"

#include <algorithm>
#include <cmath>

namespace magiceq
{

SpectrumAccumulator::SpectrumAccumulator()
{
    prepare (sampleRate);
}

void SpectrumAccumulator::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

    window.resize ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i) // periodic Hann
        window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) fftSize);

    input.assign ((size_t) fftSize, 0.0f);
    work.assign ((size_t) fftSize * 2, 0.0f);
    powerSum.assign ((size_t) fftSize / 2 + 1, 0.0);
    framePower.assign ((size_t) fftSize / 2 + 1, 0.0);
    reset();
}

void SpectrumAccumulator::reset()
{
    std::fill (input.begin(), input.end(), 0.0f);
    std::fill (powerSum.begin(), powerSum.end(), 0.0);
    inputPos = 0;
    frameLevels.clear();
    frameLevels.reserve (2048);
    activeFrames = 0;
    activeSeconds = 0.0;
    loudestFrameDb = -200.0f;
    lastFrameDb = -200.0f;
}

void SpectrumAccumulator::push (const float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        input[(size_t) inputPos++] = samples[i];

        if (inputPos == fftSize)
        {
            analyseFrame();
            // Keep the second half for 50% overlap.
            std::copy (input.begin() + hopSize, input.end(), input.begin());
            inputPos = fftSize - hopSize;
        }
    }
}

void SpectrumAccumulator::analyseFrame()
{
    double sumSquares = 0.0;
    float peak = 0.0f;
    for (float s : input)
    {
        sumSquares += (double) s * s;
        peak = std::max (peak, std::abs (s));
    }

    const double rms = std::sqrt (sumSquares / fftSize);
    lastFrameDb = (float) (20.0 * std::log10 (std::max (rms, 1.0e-10)));

    loudestFrameDb = std::max (loudestFrameDb, lastFrameDb);
    if (lastFrameDb < absoluteGateDb || lastFrameDb < loudestFrameDb - relativeGateDb)
        return;

    for (int i = 0; i < fftSize; ++i)
        work[(size_t) i] = input[(size_t) i] * window[(size_t) i];
    std::fill (work.begin() + fftSize, work.end(), 0.0f);

    fft.performFrequencyOnlyForwardTransform (work.data(), true);

    for (int bin = 0; bin <= fftSize / 2; ++bin)
    {
        const double m = work[(size_t) bin];
        powerSum[(size_t) bin] += m * m;
    }

    frameLevels.push_back ({ lastFrameDb, (float) (20.0 * std::log10 (std::max ((double) peak, 1.0e-10))) });
    ++activeFrames;
    activeSeconds += (double) hopSize / sampleRate;
}

bool SpectrumAccumulator::getAverageSpectrum (GridCurve& out) const
{
    if (activeFrames == 0)
        return false;

    const double binHz = sampleRate / fftSize;
    const int lastBin = fftSize / 2;
    const double halfWidth = std::pow (2.0, 1.0 / (2.0 * AnalysisGrid::pointsPerOctave));

    GridCurve raw {};
    for (int p = 0; p < AnalysisGrid::numPoints; ++p)
    {
        const double f = AnalysisGrid::frequencyAt (p);
        const int lo = std::clamp ((int) std::ceil ((f / halfWidth) / binHz), 1, lastBin);
        const int hi = std::clamp ((int) std::floor ((f * halfWidth) / binHz), 1, lastBin);

        double sum = 0.0;
        int count = 0;
        for (int b = lo; b <= hi; ++b)
        {
            sum += powerSum[(size_t) b];
            ++count;
        }

        if (count == 0) // band narrower than one bin (low frequencies): use the nearest bin
        {
            sum = powerSum[(size_t) std::clamp ((int) std::lround (f / binHz), 1, lastBin)];
            count = 1;
        }

        // Average power density per Hz times frequency = energy per fraction of an octave.
        const double density = sum / count / activeFrames;
        raw[(size_t) p] = (float) (10.0 * std::log10 (std::max (density * f, 1.0e-20)));
    }

    // Light smoothing across neighbouring points (about 1/3 octave).
    for (int p = 0; p < AnalysisGrid::numPoints; ++p)
    {
        const float l = raw[(size_t) std::max (p - 1, 0)];
        const float r = raw[(size_t) std::min (p + 1, AnalysisGrid::numPoints - 1)];
        out[(size_t) p] = 0.25f * l + 0.5f * raw[(size_t) p] + 0.25f * r;
    }

    return true;
}

} // namespace magiceq
