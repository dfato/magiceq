#pragma once

#include <JuceHeader.h>

namespace magiceq::ui
{

/** RainWorks colours. Dark "night rain" blues with a bright rain-drop accent. */
namespace colours
{
    inline const juce::Colour background { 0xff0b1020 };
    inline const juce::Colour panel      { 0xff131a2c };
    inline const juce::Colour panelRaised{ 0xff1b2440 };
    inline const juce::Colour outline    { 0xff2a3557 };
    inline const juce::Colour grid       { 0xff1f2944 };
    inline const juce::Colour text       { 0xffe8eefb };
    inline const juce::Colour textMuted  { 0xff8b98b8 };
    inline const juce::Colour accent     { 0xff4fb3ff }; // rain blue
    inline const juce::Colour accent2    { 0xff7cf0dc }; // mist teal
    inline const juce::Colour curve      { 0xfff4f7ff };

    /** One colour per band, spread around the cool side of the wheel. */
    inline juce::Colour band (int index)
    {
        static const juce::uint32 c[] = { 0xff5ab4ff, 0xff6ee7d8, 0xff8b8cff, 0xffb689ff, 0xffef8fd5, 0xffff9c8a,
                                          0xffffc46b, 0xffd8e86b, 0xff8de08a, 0xff5fd1a8, 0xff58c9e8, 0xffa9b8ff };
        return juce::Colour (c[(size_t) juce::jlimit (0, 11, index)]);
    }
}

class RainLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RainLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& background,
                               bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::FontOptions (14.0f, juce::Font::bold); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

} // namespace magiceq::ui
