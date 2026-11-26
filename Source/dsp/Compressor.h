#pragma once

#include "core/Dynamics.h"

#include <cmath>

namespace magiceq
{

/** A feed-forward, stereo-linked vocal compressor. Zero latency, real-time safe.

    - Detector: peak level of the louder channel, through the soft-knee gain computer.
    - Ballistics: smooth "branching" attack/release applied to the gain reduction (decoupled design).
    - Mix: below 100% blends in the dry signal (parallel / "New York" compression).
*/
class Compressor
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
        updateCoefficients();
    }

    void reset() noexcept
    {
        grDb = 0.0f;
        makeupLin = targetMakeupLin();
        activeAmount = settings.enabled ? 1.0f : 0.0f;
    }

    void setSettings (const CompSettings& s) noexcept
    {
        const bool timingChanged = std::abs (s.attackMs - settings.attackMs) > 1.0e-4f || std::abs (s.releaseMs - settings.releaseMs) > 1.0e-4f;
        settings = s;
        if (timingChanged)
            updateCoefficients();
    }

    /** Processes in place. Returns the largest gain reduction in this block, in dB (>= 0). */
    float process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = numChannels < 2 ? numChannels : 2;
        const float targetActive = settings.enabled ? 1.0f : 0.0f;
        const float makeupTarget = targetMakeupLin();
        float maxGr = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = std::max (peak, std::abs (channels[ch][i]));

            const float levelDb = peak > 1.0e-6f ? 20.0f * std::log10 (peak) : -120.0f;
            const float wantGr = -compressorGainDb (levelDb, settings.thresholdDb, settings.ratio, settings.kneeDb);

            grDb = wantGr > grDb ? attackCoeff * grDb + (1.0f - attackCoeff) * wantGr
                                 : releaseCoeff * grDb + (1.0f - releaseCoeff) * wantGr;

            // Smooth on/off and makeup changes so nothing clicks.
            activeAmount += smoothCoeff * (targetActive - activeAmount);
            makeupLin += smoothCoeff * (makeupTarget - makeupLin);

            const float wetGain = std::pow (10.0f, -grDb / 20.0f) * makeupLin;
            const float wet = settings.mix * activeAmount;
            const float gain = (1.0f - wet) + wet * wetGain;

            for (int ch = 0; ch < numChannels; ++ch)
                channels[ch][i] *= gain;

            maxGr = std::max (maxGr, grDb * activeAmount);
        }

        return maxGr;
    }

private:
    float targetMakeupLin() const noexcept { return std::pow (10.0f, settings.makeupDb / 20.0f); }

    void updateCoefficients() noexcept
    {
        auto coeff = [this] (float ms) { return std::exp (-1.0f / (std::max (ms, 0.01f) * 0.001f * (float) sampleRate)); };
        attackCoeff = coeff (settings.attackMs);
        releaseCoeff = coeff (settings.releaseMs);
        smoothCoeff = 1.0f - coeff (20.0f);
    }

    double sampleRate = 48000.0;
    CompSettings settings;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f, smoothCoeff = 0.001f;
    float grDb = 0.0f, makeupLin = 1.0f, activeAmount = 0.0f;
};

} // namespace magiceq
