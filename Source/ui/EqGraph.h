#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

namespace magiceq::ui
{

/** The big EQ display: live spectrum, the EQ curve, Detect's aim, and draggable band nodes.

    Mouse:
      drag a node          move frequency and gain
      scroll on a node     change Q (width)
      double-click empty   add a band there
      double-click a node  switch that band off
      right-click a node   change its type or remove it
*/
class EqGraph : public juce::Component,
                private juce::Timer
{
public:
    explicit EqGraph (MagicEQProcessor&);
    ~EqGraph() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;

    int getSelectedBand() const noexcept { return selectedBand; }
    void setSelectedBand (int band);
    std::function<void (int)> onBandSelected;

    static constexpr float minHz = 20.0f, maxHz = 20000.0f, rangeDb = 18.0f;

private:
    void timerCallback() override;
    void updateSpectrum();

    juce::Rectangle<float> plotArea() const;
    float freqToX (double hz) const;
    double xToFreq (float x) const;
    float dbToY (double db) const;
    double yToDb (float y) const;
    float spectrumDbToY (float db) const;

    juce::Point<float> nodePosition (int band) const;
    int hitTestNode (juce::Point<float>) const;
    static bool hasGain (BandType t) { return t == BandType::Bell || t == BandType::LowShelf || t == BandType::HighShelf; }

    void setParam (const juce::String& id, float value, bool asGesture = true);
    void showBandMenu (int band);

    MagicEQProcessor& processor;

    // Analyzer
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> history = std::vector<float> ((size_t) fftSize, 0.0f);
    std::vector<float> fftData = std::vector<float> ((size_t) fftSize * 2, 0.0f);
    std::vector<float> smoothedDb = std::vector<float> ((size_t) fftSize / 2 + 1, -120.0f);
    std::vector<float> pullBuffer = std::vector<float> (8192, 0.0f);

    int selectedBand = -1;
    int hoverBand = -1;
    int dragBand = -1;
    std::array<BandSettings, EqEngine::numBands> lastBands {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqGraph)
};

} // namespace magiceq::ui
