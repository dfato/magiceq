#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

namespace magiceq::ui
{

/** The strip under the graph: 12 band buttons plus full controls for the selected band. */
class BandPanel : public juce::Component,
                  private juce::Timer
{
public:
    explicit BandPanel (MagicEQProcessor&);
    ~BandPanel() override;

    void setBand (int band);
    std::function<void (int)> onBandChosen;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshChips();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    MagicEQProcessor& processor;
    int band = -1;

    std::array<juce::TextButton, EqEngine::numBands> chips;
    juce::ToggleButton onButton { "On" };
    juce::ComboBox typeBox, slopeBox;
    juce::Slider freq, gain, q;
    juce::Label freqLabel, gainLabel, qLabel, typeLabel, slopeLabel, hint;

    std::unique_ptr<ButtonAttachment> onAttach;
    std::unique_ptr<ComboAttachment> typeAttach, slopeAttach;
    std::unique_ptr<SliderAttachment> freqAttach, gainAttach, qAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandPanel)
};

} // namespace magiceq::ui
