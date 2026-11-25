#include "CompPanel.h"
#include "RainLookAndFeel.h"

namespace magiceq::ui
{

using namespace juce;

CompPanel::CompPanel (MagicEQProcessor& p) : processor (p)
{
    const std::array<std::pair<const char*, String>, 7> defs { {
        { "THRESHOLD", params::compThresh }, { "RATIO", params::compRatio }, { "ATTACK", params::compAttack },
        { "RELEASE", params::compRelease },  { "KNEE", params::compKnee },   { "MAKEUP", params::compMakeup },
        { "MIX", params::compMix } } };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 70, 16);
        k.slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        k.slider.setColour (Slider::rotarySliderFillColourId, colours::accent2);
        k.attachment = std::make_unique<SliderAttachment> (processor.apvts, defs[i].second, k.slider);
        addAndMakeVisible (k.slider);

        k.label.setText (defs[i].first, dontSendNotification);
        k.label.setFont (FontOptions (11.0f, Font::bold));
        k.label.setColour (Label::textColourId, colours::textMuted);
        k.label.setJustificationType (Justification::centred);
        addAndMakeVisible (k.label);
    }
    knobs[5].slider.setDoubleClickReturnValue (true, 0.0);  // makeup
    knobs[6].slider.setDoubleClickReturnValue (true, 100.0); // mix
    knobs[6].slider.setTextValueSuffix ("%");

    onAttachment = std::make_unique<ButtonAttachment> (processor.apvts, params::compOn, onButton);
    addAndMakeVisible (onButton);

    detectNote.setFont (FontOptions (12.0f));
    detectNote.setColour (Label::textColourId, colours::textMuted);
    detectNote.setJustificationType (Justification::centredRight);
    addAndMakeVisible (detectNote);

    startTimerHz (30);
}

CompPanel::~CompPanel()
{
    stopTimer();
}

void CompPanel::timerCallback()
{
    // Meter: jump up quickly, fall back smoothly, and hold the peak for a moment.
    const float gr = processor.takeGainReductionDb();
    meterGr = gr > meterGr ? gr : meterGr + 0.2f * (gr - meterGr);
    if (gr >= peakGr) { peakGr = gr; peakHoldTicks = 45; }
    else if (--peakHoldTicks <= 0) peakGr = jmax (0.0f, peakGr - 0.3f);

    const bool on = processor.getComp().enabled;
    for (auto& k : knobs)
        k.slider.setAlpha (on ? 1.0f : 0.5f);

    const auto& d = processor.getLastDynamics();
    detectNote.setText (d.profile.valid && d.comp.enabled
                            ? "Detect: about " + String (d.expectedLoudGrDb, 1) + " dB on loud lines"
                            : String(),
                        dontSendNotification);

    repaint (meterBounds.expanded (2));
}

void CompPanel::paint (Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);

    g.setColour (colours::text);
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawText ("COMPRESSOR", getLocalBounds().reduced (12, 10).removeFromTop (26).withTrimmedLeft (78), Justification::centredLeft);

    // Gain-reduction meter: grows downward from the top, 0 to 24 dB.
    auto m = meterBounds.toFloat();
    g.setColour (colours::panelRaised);
    g.fillRoundedRectangle (m, 3.0f);

    constexpr float maxDb = 24.0f;
    auto toY = [&] (float db) { return m.getY() + m.getHeight() * jlimit (0.0f, 1.0f, db / maxDb); };

    if (meterGr > 0.05f)
    {
        g.setGradientFill (ColourGradient (colours::accent2, 0.0f, m.getY(), colours::accent, 0.0f, m.getBottom(), false));
        g.fillRoundedRectangle (m.withBottom (toY (meterGr)), 3.0f);
    }
    if (peakGr > 0.05f)
    {
        g.setColour (colours::text);
        g.fillRect (Rectangle<float> (m.getX(), toY (peakGr) - 1.0f, m.getWidth(), 2.0f));
    }

    g.setFont (FontOptions (10.0f));
    for (float db : { 3.0f, 6.0f, 12.0f, 18.0f, 24.0f })
    {
        g.setColour (colours::textMuted);
        g.drawText (String ((int) db), Rectangle<float> (m.getRight() + 3.0f, toY (db) - 6.0f, 20.0f, 12.0f), Justification::centredLeft);
    }
    g.setColour (colours::textMuted);
    g.setFont (FontOptions (10.0f, Font::bold));
    g.drawText ("GR", Rectangle<float> (m.getX() - 4.0f, m.getBottom() + 2.0f, m.getWidth() + 8.0f, 12.0f), Justification::centred);
}

void CompPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    auto header = r.removeFromTop (26);
    onButton.setBounds (header.removeFromLeft (72));
    detectNote.setBounds (header);
    r.removeFromTop (6);

    auto meterCol = r.removeFromRight (40);
    meterBounds = meterCol.withTrimmedBottom (16).withTrimmedRight (22).withTrimmedTop (2);
    r.removeFromRight (6);

    const int rowH = r.getHeight() / 2;
    auto row1 = r.removeFromTop (rowH);
    auto row2 = r;
    const int w = row1.getWidth() / 4;

    auto place = [] (Knob& k, Rectangle<int> cell)
    {
        k.label.setBounds (cell.removeFromTop (13));
        k.slider.setBounds (cell);
    };
    for (int i = 0; i < 4; ++i) place (knobs[(size_t) i], row1.removeFromLeft (w));
    for (int i = 4; i < 7; ++i) place (knobs[(size_t) i], row2.removeFromLeft (w));
}

} // namespace magiceq::ui
