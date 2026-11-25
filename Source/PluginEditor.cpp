#include "PluginEditor.h"

using namespace magiceq;
using namespace magiceq::ui;
using juce::String;

MagicEQEditor::MagicEQEditor (MagicEQProcessor& p)
    : AudioProcessorEditor (&p), processor (p), graph (p), bandPanel (p), compPanel (p), meters (p),
      limiterPanel (p, [this] { return meters.getLimiterGrDb(); }), presetBar (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (graph);
    addAndMakeVisible (bandPanel);
    addAndMakeVisible (compPanel);
    addAndMakeVisible (meters);
    addAndMakeVisible (limiterPanel);
    addAndMakeVisible (presetBar);

    graph.onBandSelected = [this] (int b) { bandPanel.setBand (b); };
    bandPanel.onBandChosen = [this] (int b) { graph.setSelectedBand (b); };

    // Style menu, grouped by genre. Item ids must be index + 1 for the attachment.
    {
        String lastGenre;
        const auto& styles = StyleLibrary::all();
        for (int i = 0; i < (int) styles.size(); ++i)
        {
            if (lastGenre != styles[(size_t) i].genre)
            {
                lastGenre = styles[(size_t) i].genre;
                styleBox.addSectionHeading (lastGenre);
            }
            styleBox.addItem (styles[(size_t) i].name, i + 1);
        }
    }
    lengthBox.addItemList (params::detectLengthNames, 1);

    for (auto* s : { &strength, &output })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
        s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (*s);
    }
    strength.setTextValueSuffix ("%");
    output.setDoubleClickReturnValue (true, 0.0);

    auto setupLabel = [this] (juce::Label& l, const String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, colours::textMuted);
        addAndMakeVisible (l);
    };
    setupLabel (styleLabel, "STYLE");
    setupLabel (lengthLabel, "LISTEN FOR");
    setupLabel (strengthLabel, "STRENGTH");
    setupLabel (outputLabel, "OUTPUT");

    auto& s = processor.apvts;
    styleAttach    = std::make_unique<ComboAttachment> (s, params::style, styleBox);
    lengthAttach   = std::make_unique<ComboAttachment> (s, params::detectLen, lengthBox);
    strengthAttach = std::make_unique<SliderAttachment> (s, params::strength, strength);
    outputAttach   = std::make_unique<SliderAttachment> (s, params::output, output);

    addAndMakeVisible (styleBox);
    addAndMakeVisible (lengthBox);

    detectButton.setColour (juce::TextButton::buttonColourId, colours::accent);
    detectButton.setColour (juce::TextButton::textColourOffId, colours::background);
    detectButton.onClick = [this]
    {
        if (processor.getDetectState() == MagicEQProcessor::DetectState::listening)
            processor.cancelDetect();
        else
        {
            doneMessage.clear();
            processor.startDetect();
        }
        updateDetectUi();
    };
    addAndMakeVisible (detectButton);

    status.setFont (juce::FontOptions (13.0f));
    status.setColour (juce::Label::textColourId, colours::textMuted);
    addAndMakeVisible (status);

    undoButton.onClick = [this] { processor.undoManager.undo(); };
    redoButton.onClick = [this] { processor.undoManager.redo(); };
    abButton.setTooltip ("Switch between two complete settings (A and B) to compare them");
    abButton.onClick = [this]
    {
        processor.switchAB();
        abButton.setButtonText (processor.isOnB() ? "B" : "A");
    };
    abButton.setButtonText (processor.isOnB() ? "B" : "A");
    for (auto* b : { &undoButton, &redoButton, &abButton })
        addAndMakeVisible (*b);

    setResizable (true, true);
    setResizeLimits (1160, 680, 1800, 1100);
    setSize (1240, 740);
    setWantsKeyboardFocus (true);

    startTimerHz (30);
    updateDetectUi();
}

MagicEQEditor::~MagicEQEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

bool MagicEQEditor::keyPressed (const juce::KeyPress& key)
{
    const auto cmd = juce::ModifierKeys::commandModifier;
    if (key == juce::KeyPress ('z', cmd, 0))                                       { processor.undoManager.undo(); return true; }
    if (key == juce::KeyPress ('z', cmd | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress ('y', cmd, 0))                                    { processor.undoManager.redo(); return true; }
    return false;
}

void MagicEQEditor::timerCallback()
{
    updateDetectUi();
}

void MagicEQEditor::updateDetectUi()
{
    using DS = MagicEQProcessor::DetectState;
    const auto state = processor.getDetectState();

    if (lastState != DS::idle && state == DS::idle && processor.getLastMatch().valid
        && lastState == DS::applying)
    {
        const auto& m = processor.getLastMatch();
        const int toneBands = (int) m.bands.size() - 1;
        const auto& d = processor.getLastDynamics();
        doneMessage = "Done. Set a low cut plus " + String (toneBands) + " band" + (toneBands == 1 ? "" : "s")
                    + (d.comp.enabled ? " and the compressor" : "") + " for "
                    + StyleLibrary::get ((int) processor.apvts.getRawParameterValue (params::style)->load()).name
                    + ". Not right? Undo, or tweak anything.";
    }
    lastState = state;

    const float target = state == DS::listening ? processor.getDetectProgress() : (state == DS::applying ? 1.0f : 0.0f);
    shownProgress += 0.25f * (target - shownProgress);

    switch (state)
    {
        case DS::listening:
        {
            detectButton.setButtonText ("Cancel");
            const String heard = String (processor.getDetectSecondsHeard(), 1) + " / "
                               + String ((int) processor.getDetectTargetSeconds()) + " s";
            status.setText (processor.isHearingVoice() ? "Listening to your vocal...  " + heard
                                                       : "Waiting for vocals. Press play in your DAW.  " + heard,
                            juce::dontSendNotification);
            break;
        }
        case DS::applying:
            detectButton.setButtonText ("Tuning...");
            status.setText ("Fine-tuning your EQ...", juce::dontSendNotification);
            break;
        case DS::idle:
        default:
            detectButton.setButtonText ("Detect");
        {
            const auto& style = StyleLibrary::get ((int) processor.apvts.getRawParameterValue (params::style)->load());
            status.setText (doneMessage.isNotEmpty() ? doneMessage
                                                     : String (style.name) + ": " + style.description + " Play your vocal, then press Detect.",
                            juce::dontSendNotification);
            styleBox.setTooltip (String (style.name) + ": " + style.description);
        }
            break;
    }

    detectButton.setEnabled (state != DS::applying);
    undoButton.setEnabled (processor.undoManager.canUndo());
    redoButton.setEnabled (processor.undoManager.canRedo());
    repaint (progressBounds);
}

void MagicEQEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    // Header
    auto header = getLocalBounds().removeFromTop (78).reduced (16, 0);
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("MagicEQ", header.removeFromLeft (150).withTrimmedTop (14).withHeight (32), juce::Justification::centredLeft);
    g.setColour (colours::textMuted);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("by RAINWORKS", juce::Rectangle<int> (16, 46, 150, 14), juce::Justification::centredLeft);

    // Detect progress bar
    if (! progressBounds.isEmpty())
    {
        auto bar = progressBounds.toFloat();
        g.setColour (colours::panelRaised);
        g.fillRoundedRectangle (bar, 2.0f);
        if (shownProgress > 0.002f)
        {
            g.setGradientFill (juce::ColourGradient (colours::accent, bar.getX(), 0.0f, colours::accent2, bar.getRight(), 0.0f, false));
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, shownProgress)), 2.0f);
        }
    }
}

void MagicEQEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (78).reduced (16, 10);
    header.removeFromLeft (160);

    // Right side: undo/redo, A/B, output
    auto right = header.removeFromRight (290);
    auto outCol = right.removeFromRight (110);
    outputLabel.setBounds (outCol.removeFromTop (14));
    output.setBounds (outCol.withTrimmedTop (2));
    right.removeFromRight (10);
    auto buttons = right.withSizeKeepingCentre (right.getWidth(), 30);
    abButton.setBounds (buttons.removeFromRight (40));
    buttons.removeFromRight (8);
    redoButton.setBounds (buttons.removeFromRight (56));
    buttons.removeFromRight (6);
    undoButton.setBounds (buttons.removeFromRight (56));

    // Middle: style, length, strength, detect
    auto styleCol = header.removeFromLeft (190);
    styleLabel.setBounds (styleCol.removeFromTop (14));
    styleBox.setBounds (styleCol.withTrimmedTop (4).withHeight (30));
    header.removeFromLeft (10);

    auto lenCol = header.removeFromLeft (80);
    lengthLabel.setBounds (lenCol.removeFromTop (14));
    lengthBox.setBounds (lenCol.withTrimmedTop (4).withHeight (30));
    header.removeFromLeft (10);

    auto strCol = header.removeFromLeft (110);
    strengthLabel.setBounds (strCol.removeFromTop (14));
    strength.setBounds (strCol.withTrimmedTop (2));
    header.removeFromLeft (12);

    auto detectCol = header.removeFromLeft (104);
    detectButton.setBounds (detectCol.withTrimmedTop (14).withHeight (34));

    // Status line with progress bar
    auto statusRow = r.removeFromTop (34).reduced (16, 0);
    progressBounds = statusRow.removeFromBottom (4);
    statusRow.removeFromBottom (2);
    presetBar.setBounds (statusRow.removeFromRight (380));
    statusRow.removeFromRight (12);
    status.setBounds (statusRow);

    r.reduce (12, 8);
    auto bottom = r.removeFromBottom (210);
    limiterPanel.setBounds (bottom.removeFromRight (190));
    bottom.removeFromRight (8);
    compPanel.setBounds (bottom.removeFromRight (juce::jmax (400, bottom.getWidth() * 9 / 20)));
    bottom.removeFromRight (8);
    bandPanel.setBounds (bottom);
    r.removeFromBottom (8);
    meters.setBounds (r.removeFromRight (74));
    r.removeFromRight (8);
    graph.setBounds (r);
}
