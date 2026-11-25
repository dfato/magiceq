#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

namespace magiceq::ui
{

/** Compressor controls with a gain-reduction meter. */
class CompPanel : public juce::Component,
                  private juce::Timer
{
public:
    explicit CompPanel (MagicEQProcessor&);
    ~CompPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    MagicEQProcessor& processor;
    juce::ToggleButton onButton { "On" };
    std::unique_ptr<ButtonAttachment> onAttachment;
    std::array<Knob, 7> knobs;
    juce::Label detectNote;

    juce::Rectangle<int> meterBounds;
    float meterGr = 0.0f, peakGr = 0.0f;
    int peakHoldTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompPanel)
};

} // namespace magiceq::ui
