#include "PresetManager.h"
#include "ParamIds.h"
#include "StyleLibrary.h"

namespace magiceq
{

using namespace juce;

namespace
{
    // Builders for readable factory presets.
    struct Band { int type; float freq, gain, q; int slope = 1; };
    struct Comp { float threshold, ratio, attack, release, makeup, knee = 6.0f, mix = 100.0f; };

    int styleIndex (const char* name)
    {
        const auto& all = StyleLibrary::all();
        for (size_t i = 0; i < all.size(); ++i)
            if (String (all[i].name) == name) return (int) i;
        return 0;
    }

    ParamValues make (std::initializer_list<Band> bands, const Comp* comp, const char* style)
    {
        ParamValues v;
        int b = 0;
        for (const auto& band : bands)
        {
            v[params::on (b)] = 1.0f;
            v[params::type (b)] = (float) band.type;
            v[params::freq (b)] = band.freq;
            v[params::gain (b)] = band.gain;
            v[params::q (b)] = band.q;
            v[params::slope (b)] = (float) band.slope;
            ++b;
        }
        if (comp != nullptr)
        {
            v[params::compOn] = 1.0f;
            v[params::compThresh] = comp->threshold;
            v[params::compRatio] = comp->ratio;
            v[params::compAttack] = comp->attack;
            v[params::compRelease] = comp->release;
            v[params::compMakeup] = comp->makeup;
            v[params::compKnee] = comp->knee;
            v[params::compMix] = comp->mix;
        }
        if (style != nullptr)
            v[params::style] = (float) styleIndex (style);
        return v;
    }

    // Band type numbers (match the Type parameter).
    constexpr int Bell = 0, LowShelf = 1, HighShelf = 2, LowCut = 3, HighCut = 4;
} // namespace

const std::vector<std::pair<String, ParamValues>>& PresetManager::factoryPresets()
{
    // Starting points, by ear-free guesswork like the style curves: tune them with Dominic.
    static const Comp clean { -20, 3, 10, 100, 3 }, airy { -22, 4, 5, 60, 4 }, warm { -20, 3, 15, 120, 3 },
                      rap { -24, 6, 2, 40, 6 }, podcast { -24, 3.5f, 10, 150, 5 }, rock { -22, 5, 8, 80, 5 },
                      phone { -25, 8, 2, 50, 6 }, leveler { -26, 2, 30, 200, 4, 12 }, lofi { -24, 6, 5, 80, 6 },
                      parallel { -30, 8, 1, 60, 10, 3, 40 };

    static const std::vector<std::pair<String, ParamValues>> presets {
        { "Init (Flat)", {} },
        { "Clean Vocal Starter", make ({ { LowCut, 90, 0, 0.707f }, { Bell, 300, -2, 1.2f }, { Bell, 3500, 2, 0.9f }, { HighShelf, 10000, 2, 0.707f } }, &clean, "Modern Pop") },
        { "Airy Pop Lead",       make ({ { LowCut, 100, 0, 0.707f }, { Bell, 250, -2.5f, 1.0f }, { Bell, 2500, 1.5f, 1.0f }, { Bell, 5000, 2, 1.2f }, { HighShelf, 12000, 4, 0.707f } }, &airy, "Modern Pop") },
        { "Warm R&B",            make ({ { LowCut, 75, 0, 0.707f }, { LowShelf, 150, 2, 0.707f }, { Bell, 350, -1.5f, 1.5f }, { HighShelf, 11000, 3, 0.707f } }, &warm, "Contemporary R&B") },
        { "Upfront Rap",         make ({ { LowCut, 110, 0, 0.707f }, { Bell, 220, -2, 1.0f }, { Bell, 500, -1.5f, 1.4f }, { Bell, 4000, 3, 1.0f }, { HighShelf, 10000, 2.5f, 0.707f } }, &rap, "Trap") },
        { "Podcast Voice",       make ({ { LowCut, 80, 0, 0.707f }, { LowShelf, 120, 1.5f, 0.707f }, { Bell, 300, -2, 1.0f }, { Bell, 2800, 2.5f, 1.0f }, { Bell, 6500, -2, 3.0f } }, &podcast, "Podcast / Spoken") },
        { "Rock Grit",           make ({ { LowCut, 120, 0, 0.707f }, { Bell, 400, -1, 1.0f }, { Bell, 1200, 2, 1.0f }, { Bell, 2500, 2.5f, 1.2f }, { HighShelf, 10000, 1, 0.707f } }, &rock, "Alt Rock") },
        { "Gentle Leveler",      make ({ { LowCut, 80, 0, 0.707f } }, &leveler, nullptr) },
        { "Parallel Punch",      make ({ { LowCut, 100, 0, 0.707f } }, &parallel, nullptr) },
        { "Telephone FX",        make ({ { LowCut, 400, 0, 0.707f, 3 }, { HighCut, 3200, 0, 0.707f, 3 }, { Bell, 1500, 6, 1.5f } }, &phone, nullptr) },
        { "Lo-Fi Vocal",         make ({ { LowCut, 250, 0, 0.707f }, { HighCut, 5000, 0, 0.707f }, { Bell, 1000, 3, 0.8f } }, &lofi, "Lo-Fi") },
    };
    return presets;
}

PresetManager::PresetManager (File presetFolder) : folder (std::move (presetFolder))
{
    folder.createDirectory();
    loadFavourites();
}

File PresetManager::defaultFolder()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
        .getChildFile ("RainWorks").getChildFile ("MagicEQ").getChildFile ("Presets");
}

std::vector<PresetManager::Preset> PresetManager::getAll() const
{
    std::vector<Preset> all;
    for (const auto& [name, values] : factoryPresets())
        all.push_back ({ name, "Factory", true, {} });

    auto files = folder.findChildFiles (File::findFiles, false, String ("*") + fileExtension);
    files.sort();
    for (const auto& f : files)
        all.push_back ({ f.getFileNameWithoutExtension(), "My Presets", false, f });

    cache = all;
    return all;
}

std::vector<PresetManager::Preset> PresetManager::getFavourites() const
{
    std::vector<Preset> favs;
    for (const auto& p : getAll())
        if (isFavourite (p.name))
            favs.push_back (p);
    return favs;
}

const PresetManager::Preset* PresetManager::find (const String& name) const
{
    getAll();
    for (const auto& p : cache)
        if (p.name == name)
            return &p;
    return nullptr;
}

File PresetManager::fileFor (const String& name) const
{
    return folder.getChildFile (File::createLegalFileName (name.trim()) + fileExtension);
}

bool PresetManager::isValidUserName (const String& name) const
{
    const auto clean = File::createLegalFileName (name.trim());
    if (clean.isEmpty())
        return false;
    for (const auto& [factoryName, values] : factoryPresets())
        if (factoryName.equalsIgnoreCase (clean))
            return false;
    return true;
}

bool PresetManager::save (const String& name, const ParamValues& values)
{
    if (! isValidUserName (name))
        return false;

    XmlElement xml ("MagicEQPreset");
    xml.setAttribute ("version", 1);
    xml.setAttribute ("name", name.trim());
    for (const auto& [id, value] : values)
    {
        auto* p = xml.createNewChildElement ("PARAM");
        p->setAttribute ("id", id);
        p->setAttribute ("value", (double) value);
    }

    folder.createDirectory();
    return xml.writeTo (fileFor (name));
}

bool PresetManager::remove (const Preset& preset)
{
    if (preset.factory || ! preset.file.existsAsFile())
        return false;
    setFavourite (preset.name, false);
    return preset.file.deleteFile();
}

bool PresetManager::load (const Preset& preset, ParamValues& out) const
{
    out.clear();
    if (preset.factory)
    {
        for (const auto& [name, values] : factoryPresets())
            if (name == preset.name) { out = values; return true; }
        return false;
    }

    auto xml = parseXML (preset.file);
    if (xml == nullptr || ! xml->hasTagName ("MagicEQPreset"))
        return false;

    for (auto* p : xml->getChildWithTagNameIterator ("PARAM"))
        if (p->hasAttribute ("id"))
            out[p->getStringAttribute ("id")] = (float) p->getDoubleAttribute ("value");
    return true;
}

bool PresetManager::isFavourite (const String& name) const
{
    return favourites.contains (name);
}

void PresetManager::setFavourite (const String& name, bool shouldBe)
{
    if (shouldBe) favourites.addIfNotAlreadyThere (name);
    else          favourites.removeString (name);
    saveFavourites();
}

void PresetManager::loadFavourites()
{
    favourites.clear();
    if (auto xml = parseXML (folder.getChildFile ("Favourites.xml")))
        for (auto* f : xml->getChildWithTagNameIterator ("FAVOURITE"))
            favourites.addIfNotAlreadyThere (f->getStringAttribute ("name"));
}

void PresetManager::saveFavourites() const
{
    XmlElement xml ("MagicEQFavourites");
    for (const auto& f : favourites)
        xml.createNewChildElement ("FAVOURITE")->setAttribute ("name", f);
    folder.createDirectory();
    xml.writeTo (folder.getChildFile ("Favourites.xml"));
}

} // namespace magiceq
