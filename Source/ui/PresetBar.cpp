#include "PresetBar.h"
#include "RainLookAndFeel.h"

namespace magiceq::ui
{

using namespace juce;

namespace
{
    const String starOn  = String::fromUTF8 ("\xe2\x98\x85"); // ★
    const String starOff = String::fromUTF8 ("\xe2\x98\x86"); // ☆
}

PresetBar::PresetBar (MagicEQProcessor& p) : processor (p)
{
    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    nameButton.setTooltip ("Browse presets");
    star.setTooltip ("Add to (or remove from) your favourites");
    save.setTooltip ("Save the current settings as your own preset");

    prev.onClick = [this] { processor.stepPreset (-1); refresh(); };
    next.onClick = [this] { processor.stepPreset (+1); refresh(); };
    nameButton.onClick = [this] { showMenu(); };
    star.onClick = [this]
    {
        auto& pm = processor.getPresets();
        const auto name = processor.getCurrentPresetName();
        pm.setFavourite (name, ! pm.isFavourite (name));
        refresh();
    };
    save.onClick = [this] { showSaveDialog(); };

    for (auto* b : { &prev, &next, &nameButton, &star, &save })
        addAndMakeVisible (*b);

    refresh();
    startTimerHz (4);
}

PresetBar::~PresetBar()
{
    stopTimer();
}

void PresetBar::timerCallback()
{
    refresh();
}

void PresetBar::refresh()
{
    const auto name = processor.getCurrentPresetName();
    nameButton.setButtonText (name + (processor.isPresetModified() ? " *" : ""));
    const bool fav = processor.getPresets().isFavourite (name);
    star.setButtonText (fav ? starOn : starOff);
    star.setColour (TextButton::textColourOffId, fav ? Colour (0xffffc46b) : colours::textMuted);
}

void PresetBar::showMenu()
{
    auto& pm = processor.getPresets();
    const auto current = processor.getCurrentPresetName();
    const auto all = pm.getAll();

    PopupMenu menu;
    std::vector<String> ids; // menu item id - 1 -> preset name

    auto addPreset = [&] (PopupMenu& m, const PresetManager::Preset& p)
    {
        ids.push_back (p.name);
        m.addItem ((int) ids.size(), (pm.isFavourite (p.name) ? starOn + " " : String()) + p.name, true, p.name == current);
    };

    const auto favs = pm.getFavourites();
    if (! favs.empty())
    {
        menu.addSectionHeader ("Favourites");
        for (const auto& p : favs) addPreset (menu, p);
    }

    menu.addSectionHeader ("Factory");
    for (const auto& p : all) if (p.factory) addPreset (menu, p);

    menu.addSectionHeader ("My Presets");
    bool anyUser = false;
    for (const auto& p : all) if (! p.factory) { addPreset (menu, p); anyUser = true; }
    if (! anyUser)
        menu.addItem (-1, "(none yet: press Save)", false);

    constexpr int saveId = 10000, deleteId = 10001, folderId = 10002;
    menu.addSeparator();
    menu.addItem (saveId, "Save as new preset...");
    const auto* currentPreset = pm.find (current);
    menu.addItem (deleteId, "Delete \"" + current + "\"", currentPreset != nullptr && ! currentPreset->factory);
    menu.addItem (folderId, "Show presets folder");

    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&nameButton),
                        [this, ids] (int result)
                        {
                            auto& presets = processor.getPresets();
                            if (result == saveId) { showSaveDialog(); return; }
                            if (result == folderId) { presets.getFolder().startAsProcess(); return; }
                            if (result == deleteId)
                            {
                                if (const auto* p = presets.find (processor.getCurrentPresetName()))
                                    if (presets.remove (*p))
                                        processor.loadPreset ("Init (Flat)");
                                refresh();
                                return;
                            }
                            if (result > 0 && result <= (int) ids.size())
                                processor.loadPreset (ids[(size_t) result - 1]);
                            refresh();
                        });
}

void PresetBar::showSaveDialog()
{
    const auto current = processor.getCurrentPresetName();
    const auto* preset = processor.getPresets().find (current);
    const String suggestion = (preset != nullptr && ! preset->factory) ? current : String();

    saveWindow = std::make_unique<AlertWindow> ("Save preset", "Give your preset a name. Using an existing name of yours replaces it.",
                                                MessageBoxIconType::NoIcon, this);
    saveWindow->addTextEditor ("name", suggestion, "Name");
    saveWindow->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    saveWindow->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));

    saveWindow->enterModalState (true, ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && saveWindow != nullptr)
        {
            const auto name = saveWindow->getTextEditorContents ("name").trim();
            if (! processor.saveCurrentAs (name))
                AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Couldn't save",
                                                  name.isEmpty() ? "Please type a name." : "\"" + name + "\" is a factory preset name or can't be used. Try another.");
        }
        saveWindow.reset();
        refresh();
    }), false);
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    save.setBounds (r.removeFromRight (56));
    r.removeFromRight (6);
    star.setBounds (r.removeFromRight (30));
    r.removeFromRight (6);
    prev.setBounds (r.removeFromLeft (28));
    next.setBounds (r.removeFromRight (28));
    nameButton.setBounds (r.reduced (4, 0));
}

} // namespace magiceq::ui
