#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "ui/BandPanel.h"
#include "ui/CompPanel.h"
#include "ui/LimiterPanel.h"
#include "ui/Meters.h"
#include "ui/EqGraph.h"
#include "ui/RainLookAndFeel.h"

class MagicEQEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit MagicEQEditor (MagicEQProcessor&);
    ~MagicEQEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void selectBand (int band) { graph.setSelectedBand (band); }

private:
    void timerCallback() override;
    void updateDetectUi();

    MagicEQProcessor& processor;
    magiceq::ui::RainLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };

    magiceq::ui::EqGraph graph;
    magiceq::ui::BandPanel bandPanel;
    magiceq::ui::CompPanel compPanel;
    magiceq::ui::MeterStrip meters;
    magiceq::ui::LimiterPanel limiterPanel;

    juce::ComboBox styleBox, lengthBox;
    juce::Slider strength, output;
    juce::Label styleLabel, lengthLabel, strengthLabel, outputLabel, status;
    juce::TextButton detectButton { "Detect" };
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" }, abButton { "A" };

    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<ComboAttachment> styleAttach, lengthAttach;
    std::unique_ptr<SliderAttachment> strengthAttach, outputAttach;

    juce::Rectangle<int> progressBounds;
    float shownProgress = 0.0f;
    MagicEQProcessor::DetectState lastState = MagicEQProcessor::DetectState::idle;
    juce::String doneMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MagicEQEditor)
};
