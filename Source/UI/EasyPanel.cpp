#include "EasyPanel.h"
#include "../PluginProcessor.h"

namespace luthier
{

EasyPanel::EasyPanel (LuthierAudioProcessor& p)
    : processor (p),
      guitarBody (p),
      fretboard (p)
{
    addAndMakeVisible (guitarBody);
    addAndMakeVisible (fretboard);

    // ---- macro row ---------------------------------------------------------------
    struct MacroSetup
    {
        LuthierKnob* knob;
        const char* paramId;
        const char* tooltip;
        juce::Colour colour;
    };

    const MacroSetup macros[] =
    {
        { &attackKnob, ParamIDs::macroAttack,
          "Pick attack: soft and round at the left, sharp and bright at the right. "
          "Moves the contact bandwidth of whatever is touching the string.", Palette::accent },
        { &bodyKnob, ParamIDs::macroBody,
          "How much of the guitar's body you hear. Never fully off: even a solid "
          "body colours the sound.", Palette::accent },
        { &driveKnob, ParamIDs::macroDrive,
          "Amp gain. Adds to the amp's own gain control rather than replacing it.", Palette::accent },
        { &toneKnob, ParamIDs::macroTone,
          "Global tone, dark to bright. Moves the guitar's tone control and the "
          "amp's treble together.", Palette::accent },
        { &spaceKnob, ParamIDs::macroSpace,
          "Room and ambience. Adds to the room blend.", Palette::secondary },
        { &humanizeKnob, ParamIDs::macroHumanize,
          "How human the playing is: timing, velocity, tuning and attack variation. "
          "At zero the plugin is machine-perfect.", Palette::secondary }
    };

    for (const auto& m : macros)
    {
        addAndMakeVisible (*m.knob);
        m.knob->attachTo (processor, m.paramId, m.tooltip);
        m.knob->setShowDiceAndLock (true);
        m.knob->setAccentColour (m.colour);
    }

    // ---- style ---------------------------------------------------------------------
    addAndMakeVisible (styleLabel);
    styleLabel.setFont (Fonts::label());
    styleLabel.setColour (juce::Label::textColourId, Palette::textMuted);

    addAndMakeVisible (styleBox);
    styleBox.setTooltip ("Factory sounds, grouped by style. Picking one loads its preset.");
    styleBox.onChange = [this]
    {
        const int selected = styleBox.getSelectedItemIndex();

        if (selected >= 0)
            applyStylePreset (selected);
    };

    refreshStyleList();

    // ---- playing mode -----------------------------------------------------------------
    addAndMakeVisible (playingModeSelector);
    playingModeSelector.attachTo (processor, ParamIDs::playingMode,
                                  "Mono routes every note to one string with legato between them. "
                                  "Poly voices chords across the strings. Guitar Controller maps "
                                  "MIDI channel to string for hex pickups and MPE.");

    // ---- audition ---------------------------------------------------------------------
    addAndMakeVisible (auditionPhraseBox);
    auditionPhraseBox.setTooltip ("What the Audition button plays");

    for (int i = 0; i < (int) AuditionPhrase::Type::NumTypes; ++i)
        auditionPhraseBox.addItem (AuditionPhrase::getName ((AuditionPhrase::Type) i), i + 1);

    auditionPhraseBox.setSelectedItemIndex ((int) processor.getUiState().auditionType,
                                            juce::dontSendNotification);
    auditionPhraseBox.onChange = [this]
    {
        processor.getUiState().auditionType =
            (AuditionPhrase::Type) juce::jmax (0, auditionPhraseBox.getSelectedItemIndex());
    };

    addAndMakeVisible (auditionButton);
    auditionButton.setTooltip ("Play the selected phrase through the current sound, "
                               "so you can hear it without a MIDI keyboard.");
    auditionButton.setColour (juce::TextButton::textColourOffId, Palette::accent);
    auditionButton.onClick = [this]
    {
        if (processor.isAuditioning())
            processor.stopAudition();
        else
            processor.startAudition ((AuditionPhrase::Type)
                                     juce::jmax (0, auditionPhraseBox.getSelectedItemIndex()));
    };

    // ---- utility ------------------------------------------------------------------------
    addAndMakeVisible (exportButton);
    exportButton.setTooltip ("Render the audition phrase, or a MIDI file, to audio");
    exportButton.onClick = [this] { if (onOpenExport) onOpenExport(); };

    addAndMakeVisible (randomiseButton);
    randomiseButton.setTooltip ("New sound. Locked controls are kept; every press starts "
                                "from the defaults so the results do not compound.");
    randomiseButton.onClick = [this] { processor.randomiseParameters(); };

    addAndMakeVisible (resetButton);
    resetButton.setTooltip ("Return every setting to its default");
    resetButton.onClick = [this]
    {
        processor.pushUndoState ("Reset");
        processor.getPresetManager().resetToDefaults();
        processor.getParameterBridge().applyAllNow();
    };

    // ---- meter, chord readout and data stream --------------------------------------------
    addAndMakeVisible (meter);
    meter.setSource (&processor);

    addAndMakeVisible (chordLabel);
    chordLabel.setFont (Fonts::mono (14.0f));
    chordLabel.setColour (juce::Label::textColourId, Palette::accent);
    chordLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (dataStream);
    dataStream.setSource (&processor);
    dataStream.setNumLines (10);

    fretboard.onStringSelected = [this] (int s) { processor.getUiState().selectedString = s; };

    startTimerHz (10);
}

EasyPanel::~EasyPanel()
{
    stopTimer();
}

//==============================================================================
void EasyPanel::refreshStyleList()
{
    styleBox.clear (juce::dontSendNotification);
    stylePresetIndices.clear();

    auto& manager = processor.getPresetManager();

    int itemId = 1;
    juce::String lastCategory;

    for (int i = 0; i < manager.getNumPresets(); ++i)
    {
        const auto* info = manager.getPreset (i);

        if (info == nullptr)
            continue;

        if (info->category != lastCategory)
        {
            styleBox.addSectionHeading (info->category);
            lastCategory = info->category;
        }

        styleBox.addItem (info->name, itemId++);
        stylePresetIndices.add (i);
    }

    const int current = manager.getCurrentPresetIndex();
    const int listIndex = stylePresetIndices.indexOf (current);

    if (listIndex >= 0)
        styleBox.setSelectedItemIndex (listIndex, juce::dontSendNotification);
}

void EasyPanel::applyStylePreset (int listIndex)
{
    if (! juce::isPositiveAndBelow (listIndex, stylePresetIndices.size()))
        return;

    processor.pushUndoState ("Load style");
    processor.getPresetManager().loadPreset (stylePresetIndices[listIndex]);
    processor.getParameterBridge().applyAllNow();
}

//==============================================================================
void EasyPanel::timerCallback()
{
    auditionButton.setButtonText (processor.isAuditioning() ? "Stop" : "Audition");

    const auto chord = processor.getEngine().getLastChordName();
    const int activeNotes = processor.getEngine().getMidiInterpreter().getActiveNoteCount();

    if (activeNotes == 0)
        chordLabel.setText ({}, juce::dontSendNotification);
    else if (chord.isNotEmpty())
        chordLabel.setText (chord, juce::dontSendNotification);

    // Keep the style list in step with preset changes made elsewhere.
    const int current = processor.getPresetManager().getCurrentPresetIndex();
    const int listIndex = stylePresetIndices.indexOf (current);

    if (listIndex >= 0 && listIndex != styleBox.getSelectedItemIndex())
        styleBox.setSelectedItemIndex (listIndex, juce::dontSendNotification);
}

//==============================================================================
void EasyPanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    auto bounds = getLocalBounds().reduced (Metrics::windowPadding, Metrics::grid);

    const int band1 = juce::roundToInt (bounds.getHeight() * 0.50f);
    const int band2 = juce::roundToInt (bounds.getHeight() * 0.28f);

    auto top = bounds.removeFromTop (band1);
    auto middle = bounds.removeFromTop (band2);

    LuthierLookAndFeel::drawSeparator (g, { top.getX(), top.getBottom() + 2, top.getWidth(), 1 });
    LuthierLookAndFeel::drawSeparator (g, { middle.getX(), middle.getBottom() + 2, middle.getWidth(), 1 });
}

void EasyPanel::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::windowPadding, Metrics::grid);

    const int band1 = juce::roundToInt (bounds.getHeight() * 0.50f);
    const int band2 = juce::roundToInt (bounds.getHeight() * 0.28f);

    // ---- band 1: guitar and fretboard ---------------------------------------------
    auto top = bounds.removeFromTop (band1);

    auto meterColumn = top.removeFromRight (34);
    meter.setBounds (meterColumn.reduced (4, Metrics::grid));

    auto fretboardArea = top.removeFromBottom (juce::jmax (90, top.getHeight() / 2));
    fretboard.setBounds (fretboardArea.reduced (0, Metrics::gridHalf));

    // The guitar illustration takes the left of what is left; the data stream fills
    // the empty space beside it rather than leaving a hole in the layout.
    auto guitarArea = top;
    auto streamArea = guitarArea.removeFromRight (juce::jmax (0, guitarArea.getWidth() / 3));

    guitarBody.setBounds (guitarArea);
    dataStream.setBounds (streamArea.reduced (Metrics::grid, Metrics::gridHalf));

    bounds.removeFromTop (Metrics::gridHalf);

    // ---- band 2: macros ---------------------------------------------------------------
    auto middle = bounds.removeFromTop (band2);

    LuthierKnob* knobs[] = { &attackKnob, &bodyKnob, &driveKnob, &toneKnob, &spaceKnob, &humanizeKnob };
    const int count = (int) (sizeof (knobs) / sizeof (knobs[0]));

    const int knobHeight = juce::jmin (middle.getHeight() - Metrics::grid,
                                       LuthierKnob::preferredHeightFor (LuthierKnob::Size::Macro));
    const int cellWidth = middle.getWidth() / count;

    auto knobRow = middle.withSizeKeepingCentre (middle.getWidth(), knobHeight);

    for (int i = 0; i < count; ++i)
        knobs[i]->setBounds (knobRow.removeFromLeft (cellWidth).reduced (Metrics::gridHalf, 0));

    bounds.removeFromTop (Metrics::gridHalf);

    // ---- band 3: style and play ---------------------------------------------------------
    auto bottom = bounds;

    auto row = bottom.removeFromTop (juce::jmin (bottom.getHeight(), 52));

    auto styleArea = row.removeFromLeft (juce::jmax (180, row.getWidth() / 4));
    styleLabel.setBounds (styleArea.removeFromTop (14));
    styleBox.setBounds (styleArea.reduced (0, 2).withHeight (26));

    row.removeFromLeft (Metrics::grid);

    playingModeSelector.setBounds (row.removeFromLeft (150).withHeight (40));

    row.removeFromLeft (Metrics::grid);

    // Chord readout sits between the mode selector and the transport controls.
    chordLabel.setBounds (row.removeFromLeft (juce::jmax (70, row.getWidth() / 6)));

    resetButton.setBounds (row.removeFromRight (80).reduced (2, 12));
    randomiseButton.setBounds (row.removeFromRight (90).reduced (2, 12));
    exportButton.setBounds (row.removeFromRight (80).reduced (2, 12));

    row.removeFromRight (Metrics::grid);

    auditionPhraseBox.setBounds (row.removeFromRight (juce::jmin (170, row.getWidth() / 2)).reduced (2, 13));
    auditionButton.setBounds (row.removeFromRight (100).reduced (2, 12));
}

} // namespace luthier
