#pragma once

#include <juce_core/juce_core.h>

/** Parameter IDs, kept free of plugin code so presets and tests can use them too.
    Changing an ID breaks saved sessions and presets, so treat these as permanent. */
namespace magiceq::params
{

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

inline const juce::String compOn      = "comp_on";
inline const juce::String compThresh  = "comp_threshold";
inline const juce::String compRatio   = "comp_ratio";
inline const juce::String compAttack  = "comp_attack";
inline const juce::String compRelease = "comp_release";
inline const juce::String compKnee    = "comp_knee";
inline const juce::String compMakeup  = "comp_makeup";
inline const juce::String compMix     = "comp_mix";

inline const juce::String limOn       = "lim_on";
inline const juce::String limCeiling  = "lim_ceiling";
inline const juce::String limRelease  = "lim_release";
inline const juce::String limTruePeak = "lim_truepeak";

} // namespace magiceq::params
