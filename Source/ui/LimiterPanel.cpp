#include "LimiterPanel.h"
#include "RainLookAndFeel.h"

namespace magiceq::ui
{

using namespace juce;

LimiterPanel::LimiterPanel (MagicEQProcessor& p, std::function<float()> grSource)
    : processor (p), getGr (std::move (grSource))
{
    for (auto* s : { &ceiling, &release })
    {
        s->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (Slider::TextBoxBelow, false, 70, 16);
        s->setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        s->setColour (Slider::rotarySliderFillColourId, colours::band (5));
        addAndMakeVisible (*s);
    }
    ceiling.setDoubleClickReturnValue (true, -1.0);

    for (auto [l, t] : { std::pair { &ceilingLabel, "CEILING" }, std::pair { &releaseLabel, "RELEASE" } })
    {
        l->setText (t, dontSendNotification);
        l->setFont (FontOptions (11.0f, Font::bold));
        l->setColour (Label::textColourId, colours::textMuted);
        l->setJustificationType (Justification::centred);
        addAndMakeVisible (*l);
    }

    truePeakButton.setTooltip ("Also catch peaks that appear between samples after conversion (recommended for streaming)");
    addAndMakeVisible (onButton);
    addAndMakeVisible (truePeakButton);

    auto& s = processor.apvts;
    onAttach      = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, params::limOn, onButton);
    tpAttach      = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, params::limTruePeak, truePeakButton);
    ceilingAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, params::limCeiling, ceiling);
    releaseAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, params::limRelease, release);

    startTimerHz (30);
}

LimiterPanel::~LimiterPanel()
{
    stopTimer();
}

void LimiterPanel::timerCallback()
{
    const float gr = getGr ? getGr() : 0.0f;
    shownGr = gr > shownGr ? gr : shownGr + 0.15f * (gr - shownGr);

    const bool on = processor.getLimiter().enabled;
    ceiling.setAlpha (on ? 1.0f : 0.5f);
    release.setAlpha (on ? 1.0f : 0.5f);
    truePeakButton.setAlpha (on ? 1.0f : 0.5f);
    repaint (grBounds.expanded (2));
}

void LimiterPanel::paint (Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);

    g.setColour (colours::text);
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawText ("LIMITER", getLocalBounds().reduced (12, 10).removeFromTop (26).withTrimmedLeft (78), Justification::centredLeft);

    // Horizontal "how much is it limiting" bar, 0 to 12 dB.
    auto bar = grBounds.toFloat();
    g.setColour (colours::panelRaised);
    g.fillRoundedRectangle (bar, 3.0f);
    if (shownGr > 0.05f)
    {
        g.setColour (colours::band (5));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * jlimit (0.0f, 1.0f, shownGr / 12.0f)), 3.0f);
    }
    g.setColour (colours::textMuted);
    g.setFont (FontOptions (10.0f));
    g.drawText ("LIMITING  " + (shownGr > 0.05f ? "-" + String (shownGr, 1) + " dB" : String ("0.0 dB")),
                grBounds.translated (0, 9).withHeight (14), Justification::centredLeft);
}

void LimiterPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    onButton.setBounds (r.removeFromTop (26).removeFromLeft (72));
    r.removeFromTop (6);

    auto bottom = r.removeFromBottom (48);
    truePeakButton.setBounds (bottom.removeFromTop (24));
    grBounds = bottom.withTrimmedTop (4).withHeight (6);

    const int w = r.getWidth() / 2;
    for (auto [s, l] : { std::pair { &ceiling, &ceilingLabel }, std::pair { &release, &releaseLabel } })
    {
        auto col = r.removeFromLeft (w);
        l->setBounds (col.removeFromTop (13));
        s->setBounds (col);
    }
}

} // namespace magiceq::ui
