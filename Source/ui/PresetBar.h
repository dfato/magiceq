#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

namespace magiceq::ui
{

/** Preset browser:  < [ Preset name * ] >  ★  Save */
class PresetBar : public juce::Component,
                  private juce::Timer
{
public:
    explicit PresetBar (MagicEQProcessor&);
    ~PresetBar() override;

    void resized() override;

private:
    void timerCallback() override;
    void refresh();
    void showMenu();
    void showSaveDialog();

    MagicEQProcessor& processor;
    juce::TextButton prev { "<" }, next { ">" }, nameButton, star, save { "Save" };
    std::unique_ptr<juce::AlertWindow> saveWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};

} // namespace magiceq::ui
