#pragma once

#include "AnalysisGrid.h"

#include <juce_dsp/juce_dsp.h>

#include <vector>

namespace magiceq
{

/** Builds a long-term average spectrum of the incoming vocal while Detect is listening.

    Only frames that actually contain singing count: quiet frames (silence, breaths, bleed
    between phrases) are skipped by a level gate, so "listen for 20 seconds" means
    20 seconds of voice, not 20 seconds of wall-clock time.

    Not real-time safe (it allocates in prepare); feed it from a non-audio thread.
*/
class SpectrumAccumulator
{
public:
    static constexpr int fftOrder = 12;               // 4096-point FFT
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 2;
    static constexpr float absoluteGateDb = -50.0f;   // frames quieter than this are ignored
    static constexpr float relativeGateDb = 25.0f;    // ...and frames this far below the loudest so far

    SpectrumAccumulator();

    void prepare (double sampleRate);
    void reset();

    /** Adds mono samples. */
    void push (const float* samples, int numSamples);

    /** Seconds of voice (frames that passed the gate) analysed so far. */
    double getActiveSeconds() const noexcept { return activeSeconds; }
    int getNumActiveFrames() const noexcept { return activeFrames; }

    /** Level of the most recent frame, in dBFS. */
    float getLastFrameLevelDb() const noexcept { return lastFrameDb; }

    /** The average spectrum so far, in dB of energy per 1/6 octave, on the analysis grid.
        Returns false if nothing has been analysed yet. */
    bool getAverageSpectrum (GridCurve& out) const;

private:
    void analyseFrame();

    double sampleRate = 48000.0;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> window, input, work;
    std::vector<double> powerSum, framePower;
    int inputPos = 0;

    int activeFrames = 0;
    double activeSeconds = 0.0;
    float loudestFrameDb = -200.0f;
    float lastFrameDb = -200.0f;
};

} // namespace magiceq
