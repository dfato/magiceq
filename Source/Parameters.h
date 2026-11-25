#pragma once

#include <JuceHeader.h>

#include "core/Biquad.h"
#include "core/StyleLibrary.h"
#include "dsp/EqEngine.h"

namespace magiceq::params
{

inline constexpr int numBands = EqEngine::numBands;

inline juce::String bandId (int band, const char* suffix) { return "b" + juce::String (band + 1) + "_" + suffix; }
inline juce::String on (int b)    { return bandId (b, "on"); }
inline juce::String type (int b)  { return bandId (b, "type"); }
inline juce::String freq (int b)  { return bandId (b, "freq"); }
inline juce::String gain (int b)  { return bandId (b, "gain"); }
inline juce::String q (int b)     { return bandId (b, "q"); }
inline juce::String slope (int b) { return bandId (b, "slope"); }

inline const juce::String output    = "output";
inline const juce::String style     = "style";
inline const juce::String strength  = "strength";
inline const juce::String detectLen = "detect_len";

inline const juce::StringArray typeNames { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch" };
inline const juce::StringArray slopeNames { "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" };
inline const juce::StringArray detectLengthNames { "10 s", "20 s", "30 s" };
inline constexpr double detectLengthSeconds[] = { 10.0, 20.0, 30.0 };

inline float defaultFrequency (int band)
{
    // Spread the 12 bands from 50 Hz to 15 kHz so a freshly enabled band lands somewhere sensible.
    return (float) (50.0 * std::pow (300.0, band / (double) (numBands - 1)));
}

/** A log-scaled range, so each octave gets the same knob travel. */
inline juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float start, float end, float t) { return start * std::pow (end / start, t); },
             [] (float start, float end, float v) { return std::log (v / start) / std::log (end / start); },
             [] (float start, float end, float v) { return juce::jlimit (start, end, v); } };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto hz = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    {
        return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (roundToInt (v)) + " Hz";
    });
    auto db = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    {
        return (v > 0.0f ? "+" : "") + String (v, 1) + " dB";
    });
    auto qAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2); });

    for (int b = 0; b < numBands; ++b)
    {
        const auto prefix = "Band " + String (b + 1) + " ";
        auto group = std::make_unique<AudioProcessorParameterGroup> ("band" + String (b + 1), prefix.trim(), "|");

        group->addChild (std::make_unique<AudioParameterBool>  (ParameterID { on (b), 1 }, prefix + "On", false));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { type (b), 1 }, prefix + "Type", typeNames, 0));
        group->addChild (std::make_unique<AudioParameterFloat> (ParameterID { freq (b), 1 }, prefix + "Freq",
                                                                logRange (20.0f, 20000.0f), defaultFrequency (b), hz));
        group->addChild (std::make_unique<AudioParameterFloat> (ParameterID { gain (b), 1 }, prefix + "Gain",
                                                                NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, db));
        group->addChild (std::make_unique<AudioParameterFloat> (ParameterID { q (b), 1 }, prefix + "Q",
                                                                logRange (0.1f, 18.0f), 0.707f, qAttr));
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { slope (b), 1 }, prefix + "Slope", slopeNames, 1));
        layout.add (std::move (group));
    }

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { output, 1 }, "Output",
                                                       NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f, db));

    StringArray styleNames;
    for (const auto& s : StyleLibrary::all())
        styleNames.add (s.name);

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { style, 1 }, "Style", styleNames, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { strength, 1 }, "Detect Strength",
                                                       NormalisableRange<float> (0.0f, 100.0f, 1.0f), 75.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { detectLen, 1 }, "Detect Length", detectLengthNames, 1));

    return layout;
}

} // namespace magiceq::params
