#include "BandPanel.h"
#include "RainLookAndFeel.h"

namespace magiceq::ui
{

using namespace juce;

BandPanel::BandPanel (MagicEQProcessor& p) : processor (p)
{
    for (int b = 0; b < EqEngine::numBands; ++b)
    {
        auto& c = chips[(size_t) b];
        c.setButtonText (String (b + 1));
        c.setClickingTogglesState (false);
        c.onClick = [this, b]
        {
            setBand (b);
            if (onBandChosen) onBandChosen (b);
        };
        addAndMakeVisible (c);
    }

    typeBox.addItemList (params::typeNames, 1);
    slopeBox.addItemList (params::slopeNames, 1);

    for (auto* s : { &freq, &gain, &q })
    {
        s->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (Slider::TextBoxBelow, false, 80, 18);
        s->setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        s->setDoubleClickReturnValue (false, 0.0);
        addAndMakeVisible (*s);
    }
    gain.setDoubleClickReturnValue (true, 0.0);

    auto setupLabel = [this] (Label& l, const String& text)
    {
        l.setText (text, dontSendNotification);
        l.setFont (FontOptions (12.0f));
        l.setColour (Label::textColourId, colours::textMuted);
        l.setJustificationType (Justification::centred);
        addAndMakeVisible (l);
    };
    setupLabel (freqLabel, "FREQ");
    setupLabel (gainLabel, "GAIN");
    setupLabel (qLabel, "Q / WIDTH");
    setupLabel (typeLabel, "TYPE");
    setupLabel (slopeLabel, "SLOPE");

    hint.setText ("Pick a band above, or double-click the graph to add one.", dontSendNotification);
    hint.setColour (Label::textColourId, colours::textMuted);
    hint.setJustificationType (Justification::centred);
    addChildComponent (hint);

    addAndMakeVisible (onButton);
    addAndMakeVisible (typeBox);
    addAndMakeVisible (slopeBox);

    setBand (-1);
    startTimerHz (10);
}

BandPanel::~BandPanel()
{
    stopTimer();
}

void BandPanel::setBand (int newBand)
{
    if (newBand == band && newBand >= 0)
        return;
    band = newBand;

    onAttach.reset(); typeAttach.reset(); slopeAttach.reset();
    freqAttach.reset(); gainAttach.reset(); qAttach.reset();

    const bool hasBand = band >= 0;
    for (Component* c : std::initializer_list<Component*> { &onButton, &typeBox, &slopeBox, &freq, &gain, &q,
                                                            &freqLabel, &gainLabel, &qLabel, &typeLabel, &slopeLabel })
        c->setVisible (hasBand);
    hint.setVisible (! hasBand);

    if (hasBand)
    {
        auto& s = processor.apvts;
        onAttach    = std::make_unique<ButtonAttachment> (s, params::on (band), onButton);
        typeAttach  = std::make_unique<ComboAttachment>  (s, params::type (band), typeBox);
        slopeAttach = std::make_unique<ComboAttachment>  (s, params::slope (band), slopeBox);
        freqAttach  = std::make_unique<SliderAttachment> (s, params::freq (band), freq);
        gainAttach  = std::make_unique<SliderAttachment> (s, params::gain (band), gain);
        qAttach     = std::make_unique<SliderAttachment> (s, params::q (band), q);

        const auto c = colours::band (band);
        for (auto* sl : { &freq, &gain, &q })
            sl->setColour (Slider::rotarySliderFillColourId, c);
        onButton.setColour (ToggleButton::tickColourId, c);
    }

    refreshChips();
    timerCallback();
}

void BandPanel::timerCallback()
{
    refreshChips();

    if (band >= 0)
    {
        // Grey out controls that do nothing for this band type.
        const auto t = processor.getBand (band).type;
        const bool isCut = t == BandType::LowCut || t == BandType::HighCut;
        gain.setEnabled (! isCut && t != BandType::Notch);
        slopeBox.setEnabled (isCut);
        slopeLabel.setAlpha (isCut ? 1.0f : 0.4f);
        gainLabel.setAlpha (gain.isEnabled() ? 1.0f : 0.4f);
    }
}

void BandPanel::refreshChips()
{
    for (int b = 0; b < EqEngine::numBands; ++b)
    {
        auto& c = chips[(size_t) b];
        const bool enabled = processor.getBand (b).enabled;
        const auto col = colours::band (b);
        c.setColour (TextButton::buttonColourId, b == band ? col.withAlpha (0.9f) : (enabled ? col.withAlpha (0.25f) : colours::panelRaised));
        c.setColour (TextButton::textColourOffId, b == band ? colours::background : (enabled ? colours::text : colours::textMuted));
    }
}

void BandPanel::paint (Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
}

void BandPanel::resized()
{
    auto r = getLocalBounds().reduced (12, 10);

    auto chipRow = r.removeFromTop (28);
    const int chipW = jmin (44, (chipRow.getWidth() - 11 * 6) / 12);
    for (auto& c : chips)
    {
        c.setBounds (chipRow.removeFromLeft (chipW));
        chipRow.removeFromLeft (6);
    }

    r.removeFromTop (8);
    hint.setBounds (r);

    auto left = r.removeFromLeft (jmin (220, r.getWidth() / 3));
    onButton.setBounds (left.removeFromTop (26).withWidth (80));
    left.removeFromTop (4);
    typeLabel.setBounds (left.removeFromTop (16));
    typeBox.setBounds (left.removeFromTop (26));
    left.removeFromTop (4);
    slopeLabel.setBounds (left.removeFromTop (16));
    slopeBox.setBounds (left.removeFromTop (26));

    r.removeFromLeft (16);
    const int knobW = jmin (130, r.getWidth() / 3);
    for (auto [s, l] : { std::pair { &freq, &freqLabel }, std::pair { &gain, &gainLabel }, std::pair { &q, &qLabel } })
    {
        auto col = r.removeFromLeft (knobW);
        l->setBounds (col.removeFromTop (16));
        s->setBounds (col);
    }
}

} // namespace magiceq::ui
