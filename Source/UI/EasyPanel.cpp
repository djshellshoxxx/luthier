#include "EasyPanel.h"
#include "PedalRack.h"
#include "../PluginProcessor.h"

namespace luthier
{

//==============================================================================
//  CompactRack
//==============================================================================
CompactRack::CompactRack (LuthierAudioProcessor& p, bool post)
    : processor (p), postChain (post)
{
    setTitle (post ? "Post-effects rack" : "Pre-effects rack");
    setDescription ("Eight pedal slots. Click one to open its controls.");
    startTimerHz (4);
    timerCallback();
}

CompactRack::~CompactRack()
{
    stopTimer();
}

juce::Rectangle<int> CompactRack::slotBounds (int index) const
{
    // Two rows of four.
    const int cols = 4;
    const int w = getWidth() / cols, h = getHeight() / 2;
    return { (index % cols) * w, (index / cols) * h, w, h };
}

int CompactRack::slotAt (juce::Point<int> p) const
{
    for (int i = 0; i < kSlots; ++i)
        if (slotBounds (i).contains (p))
            return i;
    return -1;
}

void CompactRack::timerCallback()
{
    auto& chain = postChain ? processor.getEngine().getPostEffects() : processor.getEngine().getPreEffects();
    juce::StringArray names;

    for (int i = 0; i < kSlots; ++i)
    {
        const auto type = chain.getSlotType (i);
        const auto* pedal = chain.getPedal (i);
        juce::String name = type == PedalType::None ? juce::String ("-") : Parameters::pedalTypeNames()[(int) type];

        if (pedal != nullptr && pedal->isBypassed())
            name << " (off)";

        names.add (name);
    }

    if (names != shownNames)
    {
        shownNames = names;
        repaint();
    }
}

void CompactRack::paint (juce::Graphics& g)
{
    for (int i = 0; i < kSlots; ++i)
    {
        const auto r = slotBounds (i).reduced (2).toFloat();
        const auto name = shownNames[i];
        const bool empty = name == "-";
        const bool off = name.endsWith ("(off)");

        g.setColour (empty ? Palette::panelSunken : Palette::panelRaised);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (empty ? Palette::edge : off ? Palette::edgeBright : Palette::accent.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

        // A pedal LED: lit when the pedal is in and on.
        if (! empty)
        {
            g.setColour (off ? Palette::textDisabled : Palette::success);
            g.fillEllipse (r.getX() + 4.0f, r.getY() + 4.0f, 4.0f, 4.0f);
        }

        g.setColour (empty ? Palette::textDisabled : off ? Palette::textMuted : Palette::textPrimary);
        g.setFont (Fonts::ui (9.5f));
        g.drawFittedText (empty ? juce::String (i + 1) : name.upToFirstOccurrenceOf (" (off)", false, false),
                          r.reduced (3.0f, 2.0f).toNearestInt(), juce::Justification::centred, 2);
    }
}

juce::Component* CompactRack::openSlot (int index)
{
    if (! juce::isPositiveAndBelow (index, kSlots))
        return nullptr;

    // The same slot editor as Advanced mode, as a popover (3.2).
    auto editor = std::make_unique<PedalSlotComponent> (processor, postChain, index);
    editor->setSize (300, juce::jmax (140, editor->getPreferredHeight()));
    auto* shown = editor.get();

    if (isShowing())
        juce::CallOutBox::launchAsynchronously (std::move (editor), slotBounds (index) + getScreenPosition(), nullptr);
    else
        unshownPopover = std::move (editor);   // nowhere to show it (a test without a window): kept for inspection

    return shown;
}

void CompactRack::mouseDown (const juce::MouseEvent& e)
{
    openSlot (slotAt (e.getPosition()));
}

//==============================================================================
//  EasyPanel
//==============================================================================
EasyPanel::EasyPanel (LuthierAudioProcessor& p)
    : processor (p),
      guitarBody (p),
      preRack (p, false),
      postRack (p, true)
{
    addAndMakeVisible (guitarBody);

    // ---- playing strip (3.3) -------------------------------------------------------
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
          "At zero the plugin is machine-perfect.", Palette::secondary },
        { &characterKnob, ParamIDs::macroCharacter,
          "Character: dead spots, tuner drift, fret wear, body age and string noise "
          "together. The CHARACTER tab has each one on its own.", Palette::secondary }
    };

    for (const auto& m : macros)
    {
        addAndMakeVisible (*m.knob);
        m.knob->attachTo (processor, m.paramId, m.tooltip);
        m.knob->setAccentColour (m.colour);
    }

    for (auto* k : { &attackKnob, &bodyKnob, &driveKnob, &toneKnob, &spaceKnob, &humanizeKnob })
        k->setShowDiceAndLock (true);

    // The whammy display, shown only when the bridge has an arm (3.3).
    addChildComponent (whammyKnob);
    whammyKnob.attachTo (processor, ParamIDs::whammyPos, "The whammy arm's position.");

    addAndMakeVisible (playingModeSelector);
    playingModeSelector.attachTo (processor, ParamIDs::playingMode,
                                  "Mono routes every note to one string with legato between them. "
                                  "Poly voices chords across the strings. Guitar Controller maps "
                                  "MIDI channel to string for hex pickups and MPE.");

    // ---- tone strip (3.4) ------------------------------------------------------------
    inputKnob.attachTo (processor, ParamIDs::inputGain, "Input gain: how hard the guitar hits the pedals and the amp.");
    outputKnob.attachTo (processor, ParamIDs::masterGain, "Output gain, after everything.");
    mixKnob.attachTo (processor, ParamIDs::outputMix, "Wet/dry: the whole rig against the guitar's direct signal.");
    widthKnob.attachTo (processor, ParamIDs::stereoWidth, "Stereo width: mono at the left, as recorded in the middle, wider at the right.");

    for (auto* k : { &inputKnob, &outputKnob, &mixKnob, &widthKnob })
        addAndMakeVisible (k);

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

    // ---- audition --------------------------------------------------------------------
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

    // ---- meter and chord readout -------------------------------------------------------
    addAndMakeVisible (meter);
    meter.setSource (&processor);

    addAndMakeVisible (chordLabel);
    chordLabel.setFont (Fonts::mono (14.0f));
    chordLabel.setColour (juce::Label::textColourId, Palette::accent);
    chordLabel.setJustificationType (juce::Justification::centred);

    buildRigStrip();
    buildRhythmStrip();

    startTimerHz (10);
}

EasyPanel::~EasyPanel()
{
    stopTimer();
}

//==============================================================================
void EasyPanel::buildRigStrip()
{
    // 1. Guitar circuit (volume-knob-interaction.md 10): volume, tone and the live response.
    guitarVolumeKnob.attachTo (processor, ParamIDs::guitarVolume, "The guitar's volume knob, with everything a real one does to the tone.");
    guitarToneKnob.attachTo (processor, ParamIDs::guitarTone, "The guitar's tone knob.");
    addAndMakeVisible (guitarVolumeKnob);
    addAndMakeVisible (guitarToneKnob);

    circuitView = std::make_unique<CircuitResponseView> (processor);
    addAndMakeVisible (*circuitView);

    // 2 and 4. The racks.
    addAndMakeVisible (preRack);
    addAndMakeVisible (postRack);

    // 3. Amp: the model, and the face carrying gain, bass, mid, treble, presence and master.
    ampModel.attachTo (processor, ParamIDs::ampModel, "Amp model");
    addAndMakeVisible (ampModel);
    addAndMakeVisible (ampFace);

    // 5. Cabinet.
    cabModel.attachTo (processor, ParamIDs::cabType, "Cabinet");
    mic1.attachTo (processor, ParamIDs::micType, "First microphone");
    mic2.attachTo (processor, ParamIDs::micType2, "Second microphone");
    micBlend.attachTo (processor, ParamIDs::micBlend, "Blend between the two microphones");

    for (auto* c : { &cabModel, &mic1, &mic2 })
        addAndMakeVisible (c);
    addAndMakeVisible (micBlend);

    // 6. Room.
    roomSize.attachTo (processor, ParamIDs::roomSize, "Room size");
    roomMix.attachTo (processor, ParamIDs::roomBlend, "How much of the room you hear");
    addAndMakeVisible (roomSize);
    addAndMakeVisible (roomMix);
}

//==============================================================================
void EasyPanel::buildRhythmStrip()
{
    rhythmLabel.setFont (Fonts::label());
    rhythmLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    addAndMakeVisible (rhythmLabel);

    processor.getGenreKits().refresh();

    int itemId = 1;

    for (const auto& name : processor.getGenreKits().getNames())
        rhythmGenreBox.addItem (name, itemId++);

    rhythmGenreBox.setTextWhenNothingSelected ("Style");
    rhythmGenreBox.setTooltip ("Genre kit: sets the voicing, the pattern and the feel in one go.");

    rhythmGenreBox.onChange = [this]
    {
        const int index = rhythmGenreBox.getSelectedId() - 1;

        if (! juce::isPositiveAndBelow (index, processor.getGenreKits().getNumKits()))
            return;

        processor.applyGenreKit (index);

        // Choosing a style in Easy mode is a request to hear it, so the engine
        // comes on rather than waiting for a second click.
        processor.getEngine().getRhythmEngine().setEnabled (true);

        refreshRhythmStrip();
    };

    addAndMakeVisible (rhythmGenreBox);

    // rhythm-engine.md 8 / gui-integration.md 3.5: the kit dropdown with dice.
    rhythmDice.setTooltip ("A random genre kit");
    rhythmDice.onClick = [this] { rollRhythmDice(); };
    addAndMakeVisible (rhythmDice);

    // One knob for feel: it scales every humanisation amount at once, and
    // strum-dynamics.md 6's crossing velocity and evenness with it.
    rhythmFeelSlider.setRange (0.0, 200.0, 1.0);
    rhythmFeelSlider.setValue (100.0, juce::dontSendNotification);
    rhythmFeelSlider.setDoubleClickReturnValue (true, 100.0);
    rhythmFeelSlider.setTooltip ("Feel: how loose the playing is. Centre is the "
                                 "style's own feel, left is machine-tight, right is sloppier.");

    rhythmFeelSlider.onValueChange = [this]
    {
        auto& engine = processor.getEngine().getRhythmEngine();

        auto humanise = engine.getHumanise();
        humanise.amount = rhythmFeelSlider.getValue() / 100.0;
        engine.setHumanise (humanise);
    };

    addAndMakeVisible (rhythmFeelSlider);

    rhythmEnableButton.setClickingTogglesState (true);
    rhythmEnableButton.setTooltip ("Turns the rhythm engine on and off.");

    rhythmEnableButton.onClick = [this]
    {
        processor.getEngine().getRhythmEngine()
                 .setEnabled (rhythmEnableButton.getToggleState());

        refreshRhythmStrip();
    };

    addAndMakeVisible (rhythmEnableButton);

    rhythmHintLabel.setFont (Fonts::ui (10.0f));
    rhythmHintLabel.setColour (juce::Label::textColourId, Palette::warning);
    addAndMakeVisible (rhythmHintLabel);

    rhythmReadout.setFont (Fonts::mono (13.0f));
    rhythmReadout.setColour (juce::Label::textColourId, Palette::accent);
    rhythmReadout.setJustificationType (juce::Justification::centredRight);
    rhythmReadout.setTooltip ("The chord the rhythm engine is playing, and the next strum");
    addAndMakeVisible (rhythmReadout);

    refreshRhythmStrip();
}

void EasyPanel::rollRhythmDice()
{
    const int kits = processor.getGenreKits().getNumKits();

    if (kits <= 0)
        return;

    // A different kit from the one showing, so the dice always changes something.
    const int current = rhythmGenreBox.getSelectedId() - 1;
    int next = juce::Random::getSystemRandom().nextInt (kits);

    if (kits > 1 && next == current)
        next = (next + 1) % kits;

    rhythmGenreBox.setSelectedId (next + 1, juce::sendNotificationSync);
}

void EasyPanel::refreshRhythmStrip()
{
    auto& engine = processor.getEngine().getRhythmEngine();
    const bool on = engine.isEnabled();

    rhythmEnableButton.setToggleState (on, juce::dontSendNotification);
    rhythmEnableButton.setButtonText (on ? "ON" : "OFF");

    rhythmFeelSlider.setValue (engine.getHumanise().amount * 100.0, juce::dontSendNotification);

    // rhythm-engine 8: Poly is the required mode, and the strip says so rather
    // than leaving the user wondering why nothing strums.
    const auto* modeParam = processor.getState().getRawParameterValue (ParamIDs::playingMode);
    const bool isPoly = (modeParam != nullptr) && ((int) modeParam->load() == 1);

    rhythmHintLabel.setText ((on && ! isPoly) ? "Needs Poly mode" : juce::String(),
                             juce::dontSendNotification);

    // 3.5: the current chord and the next strum's arrow.
    juce::String readout;

    if (on)
    {
        const auto chord = processor.getEngine().getLastChordName();
        const auto next = engine.getNextStrumType();
        const auto arrow = next == StrumType::down || next == StrumType::downMute ? juce::String::fromUTF8 ("\xe2\x86\x93")
                         : next == StrumType::up || next == StrumType::upMute     ? juce::String::fromUTF8 ("\xe2\x86\x91")
                         : next == StrumType::rest                                ? juce::String ("-")
                                                                                  : juce::String ("~");
        readout = (chord.isNotEmpty() ? chord : juce::String ("--")) + "  " + arrow;
    }

    rhythmReadout.setText (readout, juce::dontSendNotification);
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

    // 3.3: the whammy display only when the bridge has an arm.
    const bool whammy = WhammyPopover::isWhammyFitted (processor);

    if (whammy != whammyKnob.isVisible())
    {
        whammyKnob.setVisible (whammy);
        resized();
    }

    refreshRhythmStrip();
}

//==============================================================================
void EasyPanel::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::windowPadding, Metrics::grid);

    // ---- 3.2: the rig strip down the right, 280 points ------------------------------
    rigArea = bounds.removeFromRight (juce::jmin (kRigWidth, bounds.getWidth() / 3));
    bounds.removeFromRight (Metrics::grid);

    rigCards.clearQuick();
    {
        auto rig = rigArea;
        const int total = rig.getHeight();

        auto card = [&rig, &total, this] (float share, const juce::String& title)
        {
            auto r = rig.removeFromTop (juce::roundToInt ((float) total * share));
            rigCards.add ({ r.reduced (0, 2), title });
            auto inner = r.reduced (6, 4);
            inner.removeFromTop (16);   // the card's title
            return inner;
        };

        auto circuit = card (0.20f, "Guitar");
        {
            auto knobs = circuit.removeFromLeft (circuit.getWidth() / 2);
            guitarVolumeKnob.setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
            guitarToneKnob.setBounds (knobs);
            circuitView->setBounds (circuit.reduced (2));
        }

        preRack.setBounds (card (0.12f, "Pre-effects"));

        auto amp = card (0.26f, "Amp");
        {
            // TODO 2h: the knobs sit on the face in one row, each with the card's
            // full width to share, rather than two cramped rows of three.
            ampModel.setBounds (amp.removeFromTop (juce::jmin (amp.getHeight() / 3, 44)));
            amp.removeFromTop (2);
            ampFace.setBounds (amp);
        }

        postRack.setBounds (card (0.12f, "Post-effects"));

        auto cab = card (0.17f, "Cabinet");
        {
            auto top = cab.removeFromTop (cab.getHeight() / 2);
            cabModel.setBounds (top.removeFromLeft (top.getWidth() / 2));
            micBlend.setBounds (top);
            mic1.setBounds (cab.removeFromLeft (cab.getWidth() / 2));
            mic2.setBounds (cab);
        }

        auto room = card (0.13f, "Room");
        {
            roomSize.setBounds (room.removeFromLeft (room.getWidth() / 2));
            roomMix.setBounds (room);
        }
    }

    // ---- left column: guitar, then the three strips ---------------------------------
    const int stripH = juce::jmax (64, juce::roundToInt ((float) bounds.getHeight() * 0.16f));

    rhythmArea = bounds.removeFromBottom (juce::jmin (stripH, 56));
    bounds.removeFromBottom (Metrics::gridHalf);
    toneArea = bounds.removeFromBottom (stripH);
    bounds.removeFromBottom (Metrics::gridHalf);
    playingArea = bounds.removeFromBottom (stripH);
    bounds.removeFromBottom (Metrics::gridHalf);

    // 3.1: the guitar, with the level meter and the chord beside it.
    auto guitarArea = bounds;
    auto meterColumn = guitarArea.removeFromRight (28);
    meter.setBounds (meterColumn.reduced (4, Metrics::grid));
    chordLabel.setBounds (guitarArea.removeFromTop (20).removeFromRight (120));
    guitarBody.setBounds (guitarArea);

    // 3.3 playing strip: mode, then the macros, then the whammy if fitted.
    {
        auto r = playingArea.reduced (4, 2);
        r.removeFromTop (14);
        playingModeSelector.setBounds (r.removeFromLeft (130).withSizeKeepingCentre (130, juce::jmin (48, r.getHeight())));
        r.removeFromLeft (Metrics::grid);

        juce::Array<LuthierKnob*> knobs { &attackKnob, &bodyKnob, &driveKnob, &toneKnob, &spaceKnob, &humanizeKnob, &characterKnob };
        if (whammyKnob.isVisible())
            knobs.add (&whammyKnob);

        const int w = r.getWidth() / juce::jmax (1, knobs.size());
        for (auto* k : knobs)
            k->setBounds (r.removeFromLeft (w));
    }

    // 3.4 tone strip: the four knobs, then style, audition and the utilities.
    {
        auto r = toneArea.reduced (4, 2);
        r.removeFromTop (14);
        const int knobW = juce::jmin (70, r.getWidth() / 10);

        for (auto* k : { &inputKnob, &outputKnob, &mixKnob, &widthKnob })
            k->setBounds (r.removeFromLeft (knobW));

        r.removeFromLeft (Metrics::grid);
        auto rows = r;
        auto top = rows.removeFromTop (rows.getHeight() / 2).reduced (0, 2);
        auto bottom = rows.reduced (0, 2);

        styleLabel.setBounds (top.removeFromLeft (40));
        styleBox.setBounds (top.removeFromLeft (juce::jmax (120, top.getWidth() / 2)));
        top.removeFromLeft (Metrics::gridHalf);
        auditionPhraseBox.setBounds (top.removeFromLeft (juce::jmax (100, top.getWidth() - 90)));
        auditionButton.setBounds (top.reduced (2, 0));

        const int bw = bottom.getWidth() / 3;
        exportButton.setBounds (bottom.removeFromLeft (bw).reduced (2, 0));
        randomiseButton.setBounds (bottom.removeFromLeft (bw).reduced (2, 0));
        resetButton.setBounds (bottom.reduced (2, 0));
    }

    // 3.5 rhythm strip: kit and dice, feel, on/off, the readout.
    {
        auto r = rhythmArea.reduced (4, 6);
        rhythmLabel.setBounds (r.removeFromLeft (56));
        rhythmGenreBox.setBounds (r.removeFromLeft (170));
        rhythmDice.setBounds (r.removeFromLeft (48).reduced (2, 0));
        r.removeFromLeft (Metrics::grid);
        rhythmEnableButton.setBounds (r.removeFromLeft (52));
        r.removeFromLeft (Metrics::grid);
        rhythmReadout.setBounds (r.removeFromRight (110));
        rhythmHintLabel.setBounds (r.removeFromRight (110));
        rhythmFeelSlider.setBounds (r);
    }
}

void EasyPanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    // The rig strip's cards, each a small framed panel with its name.
    for (auto& [r, title] : rigCards)
    {
        LuthierLookAndFeel::drawPanel (g, r.toFloat());
        LuthierLookAndFeel::drawSectionHeader (g, r.reduced (6, 2).withHeight (18), title);
    }

    for (auto [area, title] : { std::pair<juce::Rectangle<int>, const char*> { playingArea, "Playing" },
                                { toneArea, "Tone" }, { rhythmArea, "Rhythm" } })
    {
        LuthierLookAndFeel::drawPanel (g, area.toFloat());

        if (juce::String (title) != "Rhythm")
            LuthierLookAndFeel::drawSectionHeader (g, area.reduced (6, 1).withHeight (16), title);
    }
}

} // namespace luthier
