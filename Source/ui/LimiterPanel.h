#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

namespace magiceq::ui
{

/** Limiter controls: on/off, ceiling, release, true-peak, and how much it is limiting. */
class LimiterPanel : public juce::Component,
                     private juce::Timer
{
public:
    LimiterPanel (MagicEQProcessor&, std::function<float()> grSource);
    ~LimiterPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MagicEQProcessor& processor;
    std::function<float()> getGr;

    juce::ToggleButton onButton { "On" }, truePeakButton { "True peak" };
    juce::Slider ceiling, release;
    juce::Label ceilingLabel, releaseLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach, tpAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ceilingAttach, releaseAttach;

    juce::Rectangle<int> grBounds;
    float shownGr = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LimiterPanel)
};

} // namespace magiceq::ui
