#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

namespace magiceq::ui
{

/** One vertical level meter: RMS bar, peak bar, a held peak line and a clip light.
    Click it to clear the clip light and the held peak. */
class LevelMeter : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    explicit LevelMeter (juce::String label) : name (std::move (label)) {}

    void push (float peakLin, float rmsLin);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { heldDb = -100.0f; clipped = false; repaint(); }

    static constexpr float floorDb = -60.0f;

private:
    float toY (float db, juce::Rectangle<float> bar) const;

    juce::String name;
    float peakDb = -100.0f, rmsDb = -100.0f, heldDb = -100.0f;
    int holdTicks = 0;
    bool clipped = false;
};

/** The input and output meters, fed from the processor at 30 frames per second. */
class MeterStrip : public juce::Component,
                   private juce::Timer
{
public:
    explicit MeterStrip (MagicEQProcessor&);
    ~MeterStrip() override;
    void resized() override;
    void paint (juce::Graphics&) override;

    /** The limiter's gain reduction from the latest readings, for the limiter panel. */
    float getLimiterGrDb() const noexcept { return limiterGr; }

private:
    void timerCallback() override;

    MagicEQProcessor& processor;
    LevelMeter in { "IN" }, out { "OUT" };
    float limiterGr = 0.0f;
};

} // namespace magiceq::ui
