#include "Meters.h"
#include "RainLookAndFeel.h"

namespace magiceq::ui
{

using namespace juce;

void LevelMeter::push (float peakLin, float rmsLin)
{
    const float p = Decibels::gainToDecibels (peakLin, -100.0f);
    const float r = Decibels::gainToDecibels (rmsLin, -100.0f);

    // Rise instantly, fall at about 20 dB per second.
    peakDb = p > peakDb ? p : jmax (p, peakDb - 0.7f);
    rmsDb = r > rmsDb ? rmsDb + 0.5f * (r - rmsDb) : jmax (r, rmsDb - 0.7f);

    if (p >= heldDb) { heldDb = p; holdTicks = 60; }
    else if (--holdTicks <= 0) heldDb = jmax (-100.0f, heldDb - 0.5f);

    if (p > -0.05f) clipped = true;
    repaint();
}

float LevelMeter::toY (float db, Rectangle<float> bar) const
{
    const float t = jlimit (0.0f, 1.0f, (db - floorDb) / (0.0f - floorDb));
    return bar.getBottom() - t * bar.getHeight();
}

void LevelMeter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto label = r.removeFromBottom (14.0f);
    auto readout = r.removeFromTop (14.0f);
    auto clipLight = r.removeFromTop (6.0f).reduced (2.0f, 0.0f);
    r.removeFromTop (3.0f);
    auto bar = r.reduced (3.0f, 0.0f);

    g.setColour (clipped ? Colour (0xffff5a6e) : colours::panelRaised);
    g.fillRoundedRectangle (clipLight, 2.0f);

    g.setColour (colours::panelRaised);
    g.fillRoundedRectangle (bar, 3.0f);

    // Peak (faint) and RMS (solid). Green-blue normally, warm near the top.
    auto fillTo = [&] (float db, float alpha)
    {
        if (db <= floorDb) return;
        auto filled = bar.withTop (toY (db, bar));
        g.setGradientFill (ColourGradient (Colour (0xffff7a6b).withAlpha (alpha), 0.0f, toY (0.0f, bar),
                                           colours::accent2.withAlpha (alpha), 0.0f, toY (-18.0f, bar), false));
        g.fillRoundedRectangle (filled, 3.0f);
    };
    fillTo (peakDb, 0.35f);
    fillTo (rmsDb, 0.95f);

    if (heldDb > floorDb)
    {
        g.setColour (colours::text);
        g.fillRect (Rectangle<float> (bar.getX(), toY (heldDb, bar) - 1.0f, bar.getWidth(), 2.0f));
    }

    g.setFont (FontOptions (10.0f));
    g.setColour (heldDb > -0.05f ? Colour (0xffff7a6b) : colours::text);
    g.drawText (heldDb > floorDb ? String (heldDb, 1) : String ("-inf"), readout, Justification::centred);

    g.setColour (colours::textMuted);
    g.setFont (FontOptions (10.0f, Font::bold));
    g.drawText (name, label, Justification::centred);
}

MeterStrip::MeterStrip (MagicEQProcessor& p) : processor (p)
{
    addAndMakeVisible (in);
    addAndMakeVisible (out);
    in.setTooltip ("Input level (click to reset)");
    out.setTooltip ("Output level after the limiter (click to reset)");
    startTimerHz (30);
}

MeterStrip::~MeterStrip()
{
    stopTimer();
}

void MeterStrip::timerCallback()
{
    const auto m = processor.takeMeterReadings();
    in.push (m.inPeak, m.inRms);
    out.push (m.outPeak, m.outRms);
    limiterGr = m.limiterGrDb;
}

void MeterStrip::paint (Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);

    // Scale between the two meters.
    auto r = getLocalBounds().toFloat().reduced (4.0f, 8.0f);
    r.removeFromBottom (14.0f);
    r.removeFromTop (23.0f);
    g.setFont (FontOptions (9.0f));
    g.setColour (colours::textMuted);
    for (float db : { 0.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f })
    {
        const float y = r.getBottom() - (db - LevelMeter::floorDb) / -LevelMeter::floorDb * r.getHeight();
        g.drawText (String ((int) db), Rectangle<float> (r.getCentreX() - 10.0f, y - 5.0f, 20.0f, 10.0f), Justification::centred);
    }
}

void MeterStrip::resized()
{
    auto r = getLocalBounds().reduced (4, 8);
    const int w = (r.getWidth() - 20) / 2;
    in.setBounds (r.removeFromLeft (w));
    out.setBounds (r.removeFromRight (w));
}

} // namespace magiceq::ui
