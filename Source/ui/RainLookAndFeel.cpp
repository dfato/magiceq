#include "RainLookAndFeel.h"

namespace magiceq::ui
{

RainLookAndFeel::RainLookAndFeel()
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId, colours::background);
    setColour (Label::textColourId, colours::text);
    setColour (Slider::textBoxTextColourId, colours::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::rotarySliderFillColourId, colours::accent);
    setColour (TextButton::buttonColourId, colours::panelRaised);
    setColour (TextButton::buttonOnColourId, colours::accent);
    setColour (TextButton::textColourOffId, colours::text);
    setColour (TextButton::textColourOnId, colours::background);
    setColour (ComboBox::backgroundColourId, colours::panelRaised);
    setColour (ComboBox::textColourId, colours::text);
    setColour (ComboBox::outlineColourId, colours::outline);
    setColour (ComboBox::arrowColourId, colours::textMuted);
    setColour (PopupMenu::backgroundColourId, colours::panel);
    setColour (PopupMenu::textColourId, colours::text);
    setColour (PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.25f));
    setColour (PopupMenu::highlightedTextColourId, colours::text);
    setColour (PopupMenu::headerTextColourId, colours::accent);
    setColour (ToggleButton::textColourId, colours::text);
    setColour (ToggleButton::tickColourId, colours::accent);
}

void RainLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace juce;
    auto bounds = Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float lineW = jmax (2.5f, radius * 0.14f);
    const float arcR = radius - lineW * 0.5f;
    const auto fill = slider.findColour (Slider::rotarySliderFillColourId);

    Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::outline);
    g.strokePath (track, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));

    // Bipolar knobs (like gain) fill from the centre; the rest fill from the start.
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float zeroAngle = bipolar ? startAngle + (float) slider.valueToProportionOfLength (0.0) * (endAngle - startAngle)
                                    : startAngle;
    const float valueAngle = startAngle + pos * (endAngle - startAngle);

    if (slider.isEnabled())
    {
        Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, jmin (zeroAngle, valueAngle), jmax (zeroAngle, valueAngle), true);
        g.setColour (fill);
        g.strokePath (value, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const float knobR = radius - lineW * 2.2f;
    g.setGradientFill (ColourGradient (colours::panelRaised.brighter (0.15f), centre.x, centre.y - knobR,
                                       colours::panel, centre.x, centre.y + knobR, false));
    g.fillEllipse (Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (centre));

    const auto tip = centre.getPointOnCircumference (knobR * 0.8f, valueAngle);
    const auto base = centre.getPointOnCircumference (knobR * 0.25f, valueAngle);
    g.setColour (slider.isEnabled() ? colours::text : colours::textMuted);
    g.drawLine ({ base, tip }, 2.0f);
}

void RainLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& background,
                                            bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto c = button.getToggleState() ? button.findColour (juce::TextButton::buttonOnColourId) : background;
    if (down)             c = c.brighter (0.2f);
    else if (highlighted) c = c.brighter (0.08f);

    g.setColour (c);
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (colours::outline.withAlpha (button.getToggleState() ? 0.0f : 1.0f));
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);
}

void RainLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, 7.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) width - 16.0f, ay = (float) height * 0.5f;
    arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
    arrow.lineTo (ax, ay + 2.5f);
    arrow.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void RainLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    // A small pill switch followed by the label.
    auto bounds = button.getLocalBounds().toFloat();
    auto sw = bounds.removeFromLeft (34.0f).withSizeKeepingCentre (30.0f, 16.0f);
    const bool on = button.getToggleState();

    g.setColour (on ? colours::accent : colours::outline.brighter (highlighted ? 0.2f : 0.0f));
    g.fillRoundedRectangle (sw, 8.0f);
    g.setColour (on ? colours::background : colours::textMuted);
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ on ? sw.getRight() - 8.0f : sw.getX() + 8.0f, sw.getCentreY() }));

    g.setColour (button.findColour (juce::ToggleButton::textColourId));
    g.setFont (juce::FontOptions (14.0f));
    g.drawFittedText (button.getButtonText(), bounds.toNearestInt().withTrimmedLeft (4), juce::Justification::centredLeft, 1);
}

juce::Label* RainLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (slider);
    l->setFont (juce::FontOptions (13.0f));
    l->setColour (juce::Label::textColourId, colours::text);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

} // namespace magiceq::ui
