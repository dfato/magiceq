#include "EqGraph.h"
#include "RainLookAndFeel.h"

namespace magiceq::ui
{

using namespace juce;

EqGraph::EqGraph (MagicEQProcessor& p) : processor (p)
{
    setOpaque (true);
    startTimerHz (30);
}

EqGraph::~EqGraph()
{
    stopTimer();
}

// ----------------------------------------------------------------------------
// Coordinates
// ----------------------------------------------------------------------------

Rectangle<float> EqGraph::plotArea() const
{
    return getLocalBounds().toFloat().reduced (10.0f).withTrimmedLeft (24.0f).withTrimmedBottom (14.0f);
}

float EqGraph::freqToX (double hz) const
{
    const auto a = plotArea();
    return a.getX() + a.getWidth() * (float) (std::log (hz / minHz) / std::log (maxHz / minHz));
}

double EqGraph::xToFreq (float x) const
{
    const auto a = plotArea();
    const double t = jlimit (0.0, 1.0, (double) ((x - a.getX()) / a.getWidth()));
    return minHz * std::pow (maxHz / minHz, t);
}

float EqGraph::dbToY (double db) const
{
    const auto a = plotArea();
    return a.getCentreY() - (float) (db / rangeDb) * (a.getHeight() * 0.5f);
}

double EqGraph::yToDb (float y) const
{
    const auto a = plotArea();
    return (a.getCentreY() - y) / (a.getHeight() * 0.5f) * rangeDb;
}

float EqGraph::spectrumDbToY (float db) const
{
    const auto a = plotArea();
    constexpr float top = 0.0f, bottom = -84.0f;
    return a.getY() + a.getHeight() * (top - db) / (top - bottom);
}

Point<float> EqGraph::nodePosition (int band) const
{
    const auto& b = lastBands[(size_t) band];
    const float y = hasGain (b.type) ? dbToY (b.gainDb) : dbToY (0.0);
    return { freqToX (b.frequency), y };
}

int EqGraph::hitTestNode (Point<float> pos) const
{
    int best = -1;
    float bestDist = 14.0f;
    for (int b = 0; b < EqEngine::numBands; ++b)
    {
        if (! lastBands[(size_t) b].enabled)
            continue;
        const float d = nodePosition (b).getDistanceFrom (pos);
        if (d < bestDist) { bestDist = d; best = b; }
    }
    return best;
}

// ----------------------------------------------------------------------------
// Analyzer
// ----------------------------------------------------------------------------

void EqGraph::timerCallback()
{
    for (int b = 0; b < EqEngine::numBands; ++b)
        lastBands[(size_t) b] = processor.getBand (b);

    updateSpectrum();
    repaint();
}

void EqGraph::updateSpectrum()
{
    int got = 0;
    for (;;)
    {
        const int n = processor.analyzerFifo.pull (pullBuffer.data(), (int) pullBuffer.size());
        if (n <= 0) break;
        got += n;

        // Keep the most recent fftSize samples.
        if (n >= fftSize)
            std::copy (pullBuffer.begin() + (n - fftSize), pullBuffer.begin() + n, history.begin());
        else
        {
            std::move (history.begin() + n, history.end(), history.begin());
            std::copy (pullBuffer.begin(), pullBuffer.begin() + n, history.end() - n);
        }
    }

    if (got == 0)
    {
        // No audio arriving (transport stopped): let the display fall away gently.
        for (auto& v : smoothedDb) v = jmax (-120.0f, v - 1.5f);
        return;
    }

    std::copy (history.begin(), history.end(), fftData.begin());
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    const float norm = 4.0f / (float) fftSize; // a full-scale sine reads 0 dB
    for (size_t i = 0; i < smoothedDb.size(); ++i)
    {
        const float db = Decibels::gainToDecibels (fftData[i] * norm, -120.0f);
        auto& s = smoothedDb[i];
        s = db > s ? s + 0.6f * (db - s) : s + 0.15f * (db - s); // fast rise, slow fall
    }
}

// ----------------------------------------------------------------------------
// Painting
// ----------------------------------------------------------------------------

void EqGraph::paint (Graphics& g)
{
    const auto area = plotArea();
    g.fillAll (colours::panel);

    // Grid
    g.setFont (FontOptions (11.0f));
    for (double f : { 30.0, 50.0, 100.0, 200.0, 300.0, 500.0, 1000.0, 2000.0, 3000.0, 5000.0, 10000.0, 20000.0 })
    {
        const float x = freqToX (f);
        g.setColour (colours::grid);
        g.drawVerticalLine (roundToInt (x), area.getY(), area.getBottom());
        g.setColour (colours::textMuted);
        const String label = f >= 1000.0 ? String ((int) (f / 1000.0)) + "k" : String ((int) f);
        g.drawText (label, Rectangle<float> (x - 20.0f, area.getBottom() + 1.0f, 40.0f, 13.0f), Justification::centred);
    }
    for (double db : { -12.0, -6.0, 0.0, 6.0, 12.0 })
    {
        const float y = dbToY (db);
        g.setColour (std::abs (db) < 0.01 ? colours::outline : colours::grid);
        g.drawHorizontalLine (roundToInt (y), area.getX(), area.getRight());
        g.setColour (colours::textMuted);
        g.drawText ((db > 0 ? "+" : "") + String ((int) db), Rectangle<float> (area.getX() - 28.0f, y - 7.0f, 24.0f, 14.0f),
                    Justification::centredRight);
    }

    // Live spectrum (tilted +4.5 dB/oct around 1 kHz so a voice looks roughly level).
    {
        const double sr = processor.getCurrentSampleRate();
        const double binHz = sr / fftSize;
        Path spectrum;
        bool started = false;
        for (float x = area.getX(); x <= area.getRight(); x += 2.0f)
        {
            const double f = xToFreq (x);
            const double binPos = f / binHz;
            const int b0 = jlimit (0, (int) smoothedDb.size() - 2, (int) binPos);
            const float frac = (float) jlimit (0.0, 1.0, binPos - b0);
            float db = smoothedDb[(size_t) b0] + frac * (smoothedDb[(size_t) b0 + 1] - smoothedDb[(size_t) b0]);
            db += 4.5f * (float) std::log2 (f / 1000.0);
            const float y = jlimit (area.getY(), area.getBottom(), spectrumDbToY (db));
            if (! started) { spectrum.startNewSubPath (x, area.getBottom()); started = true; }
            spectrum.lineTo (x, y);
        }
        spectrum.lineTo (area.getRight(), area.getBottom());
        spectrum.closeSubPath();

        g.setGradientFill (ColourGradient (colours::accent.withAlpha (0.30f), 0.0f, area.getY(),
                                           colours::accent.withAlpha (0.04f), 0.0f, area.getBottom(), false));
        g.fillPath (spectrum);
    }

    // Band designs, computed once per paint.
    const double sr = processor.getCurrentSampleRate();
    std::array<BandDesign, EqEngine::numBands> designs;
    for (int b = 0; b < EqEngine::numBands; ++b)
        if (lastBands[(size_t) b].enabled)
            designs[(size_t) b] = designBand (lastBands[(size_t) b], sr);

    auto bandDb = [&] (int b, double f)
    {
        double mag = 1.0;
        for (int s = 0; s < designs[(size_t) b].numSections; ++s)
            mag *= designs[(size_t) b].sections[(size_t) s].magnitudeAt (f, sr);
        return 20.0 * std::log10 (jmax (mag, 1.0e-9));
    };

    // Selected band's own contribution, softly filled.
    if (selectedBand >= 0 && lastBands[(size_t) selectedBand].enabled)
    {
        Path p;
        p.startNewSubPath (area.getX(), dbToY (0.0));
        for (float x = area.getX(); x <= area.getRight(); x += 2.0f)
            p.lineTo (x, jlimit (area.getY(), area.getBottom(), dbToY (bandDb (selectedBand, xToFreq (x)))));
        p.lineTo (area.getRight(), dbToY (0.0));
        p.closeSubPath();
        g.setColour (colours::band (selectedBand).withAlpha (0.18f));
        g.fillPath (p);
    }

    // What Detect was aiming for (dashed).
    const auto& match = processor.getLastMatch();
    if (match.valid)
    {
        Path aim;
        for (int i = 0; i < AnalysisGrid::numPoints; ++i)
        {
            const auto pt = Point<float> (freqToX (AnalysisGrid::frequencyAt (i)), dbToY (match.correction[(size_t) i]));
            if (i == 0) aim.startNewSubPath (pt); else aim.lineTo (pt);
        }
        Path dashed;
        const float dashes[] = { 5.0f, 4.0f };
        PathStrokeType (1.4f).createDashedStroke (dashed, aim, dashes, 2);
        g.setColour (colours::accent2.withAlpha (0.7f));
        g.fillPath (dashed);
    }

    // The combined EQ curve.
    {
        Path curve;
        bool first = true;
        for (float x = area.getX(); x <= area.getRight(); x += 1.5f)
        {
            const double f = xToFreq (x);
            double total = 0.0;
            for (int b = 0; b < EqEngine::numBands; ++b)
                if (lastBands[(size_t) b].enabled)
                    total += bandDb (b, f);
            const float y = jlimit (area.getY() - 2.0f, area.getBottom() + 2.0f, dbToY (total));
            if (first) { curve.startNewSubPath (x, y); first = false; } else curve.lineTo (x, y);
        }
        g.setColour (colours::accent.withAlpha (0.35f));
        g.strokePath (curve, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (colours::curve);
        g.strokePath (curve, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // Nodes
    for (int b = 0; b < EqEngine::numBands; ++b)
    {
        if (! lastBands[(size_t) b].enabled)
            continue;
        const auto pos = nodePosition (b);
        const bool sel = b == selectedBand, hover = b == hoverBand;
        const float r = sel ? 11.0f : (hover ? 10.0f : 9.0f);
        const auto c = colours::band (b);

        if (sel)
        {
            g.setColour (c.withAlpha (0.25f));
            g.fillEllipse (Rectangle<float> (r * 3.0f, r * 3.0f).withCentre (pos));
        }
        g.setColour (sel ? c : c.withAlpha (0.85f));
        g.fillEllipse (Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos));
        g.setColour (colours::background);
        g.setFont (FontOptions (11.0f, Font::bold));
        g.drawText (String (b + 1), Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos), Justification::centred);
    }

    // Legend
    g.setFont (FontOptions (11.0f));
    auto legend = Rectangle<float> (area.getRight() - 220.0f, area.getY() + 4.0f, 216.0f, 14.0f);
    g.setColour (colours::textMuted);
    if (match.valid)
    {
        g.setColour (colours::accent2.withAlpha (0.8f));
        g.drawText ("- - Detect target   ", legend, Justification::centredRight);
    }

    g.setColour (colours::outline);
    g.drawRect (area, 1.0f);
}

void EqGraph::resized() {}

// ----------------------------------------------------------------------------
// Interaction
// ----------------------------------------------------------------------------

void EqGraph::setSelectedBand (int band)
{
    if (selectedBand == band)
        return;
    selectedBand = band;
    if (onBandSelected)
        onBandSelected (band);
    repaint();
}

void EqGraph::setParam (const String& id, float value, bool asGesture)
{
    if (auto* p = processor.apvts.getParameter (id))
    {
        if (asGesture) p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        if (asGesture) p->endChangeGesture();
    }
}

void EqGraph::mouseMove (const MouseEvent& e)
{
    const int h = hitTestNode (e.position);
    if (h != hoverBand)
    {
        hoverBand = h;
        setMouseCursor (h >= 0 ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor);
        repaint();
    }
}

void EqGraph::mouseDown (const MouseEvent& e)
{
    const int hit = hitTestNode (e.position);
    if (hit < 0)
        return;

    setSelectedBand (hit);

    if (e.mods.isPopupMenu())
    {
        showBandMenu (hit);
        return;
    }

    dragBand = hit;
    processor.undoManager.beginNewTransaction ("Move band " + String (hit + 1));
    for (const auto& id : { params::freq (hit), params::gain (hit) })
        if (auto* p = processor.apvts.getParameter (id))
            p->beginChangeGesture();
}

void EqGraph::mouseDrag (const MouseEvent& e)
{
    if (dragBand < 0)
        return;

    const auto area = plotArea();
    const float x = jlimit (area.getX(), area.getRight(), e.position.x);
    const float y = jlimit (area.getY(), area.getBottom(), e.position.y);

    setParam (params::freq (dragBand), (float) xToFreq (x), false);
    if (hasGain (lastBands[(size_t) dragBand].type))
        setParam (params::gain (dragBand), (float) jlimit (-24.0, 24.0, yToDb (y)), false);

    lastBands[(size_t) dragBand] = processor.getBand (dragBand);
    repaint();
}

void EqGraph::mouseUp (const MouseEvent&)
{
    if (dragBand < 0)
        return;
    for (const auto& id : { params::freq (dragBand), params::gain (dragBand) })
        if (auto* p = processor.apvts.getParameter (id))
            p->endChangeGesture();
    dragBand = -1;
}

void EqGraph::mouseDoubleClick (const MouseEvent& e)
{
    const int hit = hitTestNode (e.position);
    processor.undoManager.beginNewTransaction();

    if (hit >= 0)
    {
        setParam (params::on (hit), 0.0f);
        setSelectedBand (-1);
        return;
    }

    for (int b = 0; b < EqEngine::numBands; ++b)
    {
        if (processor.getBand (b).enabled)
            continue;

        const double f = xToFreq (e.position.x);
        // Near the edges a cut is usually what people want; elsewhere a bell.
        const auto type = f < 40.0 ? BandType::LowCut : (f > 15000.0 ? BandType::HighCut : BandType::Bell);
        setParam (params::type (b), (float) (int) type);
        setParam (params::freq (b), (float) f);
        setParam (params::gain (b), type == BandType::Bell ? (float) jlimit (-24.0, 24.0, yToDb (e.position.y)) : 0.0f);
        setParam (params::q (b), 1.0f);
        setParam (params::on (b), 1.0f);
        lastBands[(size_t) b] = processor.getBand (b);
        setSelectedBand (b);
        return;
    }
}

void EqGraph::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    int band = hitTestNode (e.position);
    if (band < 0) band = selectedBand;
    if (band < 0 || ! lastBands[(size_t) band].enabled)
        return;

    const float factor = std::pow (1.25f, wheel.deltaY * (wheel.isReversed ? -1.0f : 1.0f) * 4.0f);
    const float q = jlimit (0.1f, 18.0f, lastBands[(size_t) band].q * factor);
    if (auto* p = processor.apvts.getParameter (params::q (band)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (q));
        p->endChangeGesture();
    }
}

void EqGraph::showBandMenu (int band)
{
    PopupMenu menu;
    menu.addSectionHeader ("Band " + String (band + 1));
    const int currentType = (int) lastBands[(size_t) band].type;
    for (int t = 0; t < params::typeNames.size(); ++t)
        menu.addItem (t + 1, params::typeNames[t], true, t == currentType);
    menu.addSeparator();
    menu.addItem (100, "Remove band");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                        [this, band] (int result)
                        {
                            if (result == 0) return;
                            processor.undoManager.beginNewTransaction();
                            if (result == 100)
                            {
                                setParam (params::on (band), 0.0f);
                                setSelectedBand (-1);
                            }
                            else
                                setParam (params::type (band), (float) (result - 1));
                        });
}

} // namespace magiceq::ui
