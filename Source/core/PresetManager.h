#pragma once

#include <juce_core/juce_core.h>

#include <map>
#include <vector>

namespace magiceq
{

/** Parameter values by ID, in real units (Hz, dB, ms...), the way presets store them. */
using ParamValues = std::map<juce::String, float>;

/** Factory and user presets, plus favourites.

    User presets are small XML files (".magiceq") in the presets folder, so they can be
    copied between computers or shared. Favourites are remembered in a separate file.
    A parameter a preset doesn't mention is reset to its default when loaded, so old presets
    keep working when new parameters are added.
*/
class PresetManager
{
public:
    struct Preset
    {
        juce::String name;
        juce::String category; // "Factory" or "My Presets"
        bool factory = false;
        juce::File file;       // user presets only
    };

    static constexpr const char* fileExtension = ".magiceq";

    explicit PresetManager (juce::File presetFolder);

    /** The standard location: (user app data)/RainWorks/MagicEQ/Presets */
    static juce::File defaultFolder();

    std::vector<Preset> getAll() const;                        // factory first, then yours A-Z
    std::vector<Preset> getFavourites() const;
    const Preset* find (const juce::String& name) const;       // looks in getAll()

    bool save (const juce::String& name, const ParamValues& values);   // overwrites a user preset of the same name
    bool remove (const Preset& preset);                                 // user presets only
    bool load (const Preset& preset, ParamValues& out) const;

    bool isFavourite (const juce::String& name) const;
    void setFavourite (const juce::String& name, bool shouldBe);

    juce::File getFolder() const { return folder; }

    /** Names that can't be saved (factory names) or contain nothing usable. */
    bool isValidUserName (const juce::String& name) const;

    static const std::vector<std::pair<juce::String, ParamValues>>& factoryPresets();

private:
    juce::File fileFor (const juce::String& name) const;
    void loadFavourites();
    void saveFavourites() const;

    juce::File folder;
    juce::StringArray favourites;
    mutable std::vector<Preset> cache;
};

} // namespace magiceq
