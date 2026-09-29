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
      postRack (p, true),
      vuMeter (p),
      roomLight (p)
{
    addAndMakeVisible (guitarBody);

    // gui-integration 20 (TUNE-HELP-ONBOARDING): a ? on every strip.
    for (auto* help : getHelpButtons())
    {
        addAndMakeVisible (help);
        help->onHelp = [this] (const juce::String& topic)
        {
            if (onOpenHelp != nullptr)
                onOpenHelp (topic);
        };
    }

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

    // auto-articulation.md 7.1 (FEAT-ASSIST): AUTO pill and style under the mode.
    assistPill = std::make_unique<AssistPill> (processor);
    assistPill->onOpenRhythmTab = [this] { if (onOpenAssistRhythmTab != nullptr) onOpenAssistRhythmTab(); };
    addAndMakeVisible (*assistPill);
    assistStyle = std::make_unique<AssistStyleBox> (processor);
    addAndMakeVisible (*assistStyle);
    playingModeSelector.attachTo (processor, ParamIDs::playingMode,
                                  "Mono routes every note to one string with legato between them. "
                                  "Poly voices chords across the strings. Guitar Controller maps "
                                  "MIDI channel to string for hex pickups and MPE.");

    // REALISM-B, fingerstyle-attack.md 7: the Tool selector, beside the mode.
    toolSelector = std::make_unique<RightHandToolSelector> (processor);
    addAndMakeVisible (*toolSelector);

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

    // output-normalization.md 5.1: the badge under the meter, while on.
    addChildComponent (normalizationBadge);
    normalizationBadge.onVisibilityChanged = [this] { resized(); };

    // visual-polish.md 4: the room light sits behind the ROOM card's controls.
    addAndMakeVisible (roomLight);
    roomLight.toBack();
    addChildComponent (vuMeter);
    vuMeter.setVisible (VuMeter::isEnabledByUser());

    // piano-roll-chord-display.md 1, 3: the piano roll under the guitar; Easy
    // has no fretboard, so "Show fingering" draws on the illustration.
    addChildComponent (pianoRoll);
    pianoRoll.onLayoutChanged = [this] { resized(); };
    pianoRoll.onGhostDots = [this] (const std::vector<FretboardComponent::GhostDot>& dots)
    {
        std::vector<std::pair<int, double>> pairs;

        for (const auto& d : dots)
            pairs.emplace_back (d.string, d.fret);

        guitarBody.setGhostDots (pairs);
    };
    meter.setSource (&processor);

    addAndMakeVisible (chordLabel);
    chordLabel.setFont (Fonts::mono (14.0f));
    chordLabel.setColour (juce::Label::textColourId, Palette::accent);
    chordLabel.setJustificationType (juce::Justification::centred);

    buildRigStrip();
    buildRhythmStrip();

    // riff-library 7.3: the Riffs button, and the drawer as the session left it.
    riffsButton.setTooltip ("Riff library (R)");
    riffsButton.onClick = [this] { setRiffDrawerOpen (! isRiffDrawerOpen()); };
    AccessibleSetup::configureButton (riffsButton, "Riffs", "Opens the riff drawer to browse and audition riffs.");
    addAndMakeVisible (riffsButton);

    if (RiffUiState::fromVar (processor.getUiState().riffs).drawerOpen)
        setRiffDrawerOpen (true);

    startTimerHz (10);
}

juce::Rectangle<int> EasyPanel::getRiffDrawerBounds() const
{
    const auto area = getLocalBounds().reduced (Metrics::windowPadding, Metrics::grid);
    return area.withLeft (juce::jmax (area.getX(), area.getRight() - kRiffDrawerWidth));
}

void EasyPanel::setRiffDrawerOpen (bool shouldBeOpen)
{
    if (shouldBeOpen && riffDrawer == nullptr)
    {
        riffDrawer = std::make_unique<RiffBrowser> (processor, true);
        riffDrawer->onCloseRequested = [this]
        {
            setRiffDrawerOpen (false);
            riffsButton.grabKeyboardFocus();   // accessibility 1: focus goes back where it came from
        };
        addChildComponent (*riffDrawer);
    }

    if (riffDrawer == nullptr)
        return;

    riffDrawerOpen = shouldBeOpen;
    riffsButton.setToggleState (shouldBeOpen, juce::dontSendNotification);

    auto uiState = RiffUiState::fromVar (processor.getUiState().riffs);
    uiState.drawerOpen = shouldBeOpen;
    processor.getUiState().riffs = uiState.toVar();
    riffDrawer->getState().drawerOpen = shouldBeOpen;

    const auto target = getRiffDrawerBounds();
    const int ms = AccessibilitySettings::get().getAnimationMs (150);
    auto& animator = juce::Desktop::getInstance().getAnimator();

    if (shouldBeOpen)
    {
        riffDrawer->setBounds (ms > 0 ? target.translated (target.getWidth(), 0) : target);
        riffDrawer->setVisible (true);
        riffDrawer->toFront (false);

        if (ms > 0)
            animator.animateComponent (riffDrawer.get(), target, 1.0f, ms, false, 0.0, 0.0);

        riffDrawer->ensureLibraryLoaded();
        AccessibleSetup::announceOverlayOpened (*riffDrawer, "Riff drawer");
        riffDrawer->getSearchBox().grabKeyboardFocus();
    }
    else
    {
        animator.cancelAnimation (riffDrawer.get(), false);
        riffDrawer->setVisible (false);
    }
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

    // mic-placement.md 6.3 (FEAT-MIC): bright <-> warm, close <-> far.
    addAndMakeVisible (micPad);
    micPad.onOpenEditor = [this] { if (onOpenMicEditor) onOpenMicEditor(); };
    acMicMix.attachTo (processor, ParamIDs::acMicMix, "Pickup against the external microphones");
    addChildComponent (acMicMix);

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
    rhythmFeelSlider.setTooltip ("Feel. Centre is the style's own feel. Right strums faster and more "
                                 "evenly and loosens the timing; left strums slower and less evenly.");

    rhythmFeelSlider.onValueChange = [this]
    {
        auto& engine = processor.getEngine().getRhythmEngine();

        auto humanise = engine.getHumanise();
        humanise.amount = rhythmFeelSlider.getValue() / 100.0;
        engine.setHumanise (humanise);

        // strum-dynamics 6.3: the knob's 0..200 is Feel's 0..1, centre 0.5.
        engine.setStrumFeel (rhythmFeelSlider.getValue() / 200.0);
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
    rhythmReadout.setTooltip ("The chord the rhythm engine is playing");
    addAndMakeVisible (rhythmReadout);

    // SPEC-SWEEP (GD-10): the next strum is its own 60 Hz arrow now.
    nextStrumArrow = std::make_unique<NextStrumArrow> (processor);
    nextStrumArrow->setTooltip ("The next strum's direction; it flashes on each stroke");
    addAndMakeVisible (*nextStrumArrow);
    // FEAT-JAM (jam-mode 8.2): the band's pill, style, intensity and volume.
    jamGroup = std::make_unique<JamStripGroup> (processor);
    addAndMakeVisible (*jamGroup);

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
        // SPEC-SWEEP (GD-10): the arrow is NextStrumArrow's; this is the chord.
        const auto chord = processor.getEngine().getLastChordName();
        readout = chord.isNotEmpty() ? chord : juce::String ("--");
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

    processor.loadPresetAsUserAction (stylePresetIndices[listIndex]);   // action-and-undo.md 3.8
}

//==============================================================================
// SPEC-SWEEP (GD-9): the last chord stays up, dimmed after three quiet seconds,
// rather than vanishing the moment the hand lifts.
void EasyPanel::tickChordReadout (double nowMs)
{
    const auto chord = processor.getEngine().getLastChordName();
    const int activeNotes = processor.getEngine().getMidiInterpreter().getActiveNoteCount();

    if (activeNotes > 0 && chord.isNotEmpty())
    {
        if (chordLabel.getText() != chord)
            chordLabel.setText (chord, juce::dontSendNotification);

        lastChordMs = nowMs;
    }

    const bool dim = nowMs - lastChordMs > kChordStaleMs;
    const auto colour = dim ? Palette::textMuted : Palette::accent;

    if (chordLabel.findColour (juce::Label::textColourId) != colour)
        chordLabel.setColour (juce::Label::textColourId, colour);
}

//==============================================================================
void EasyPanel::timerCallback()
{
    // FEAT-MIC: the Cabinet card follows the guitar family (6.3).
    if (MicUi::isAcoustic (processor) != micPadAcoustic)
        resized();

    auditionButton.setButtonText (processor.isAuditioning() ? "Stop" : "Audition");

    tickChordReadout (juce::Time::getMillisecondCounterHiRes());   // SPEC-SWEEP GD-9

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

        // Give the amp face two generous rows while keeping the compact rack rows usable.
        auto circuit = card (0.12f, "Guitar");
        {
            auto knobs = circuit.removeFromLeft (circuit.getWidth() / 2);
            guitarVolumeKnob.setBounds (knobs.removeFromLeft (knobs.getWidth() / 2));
            guitarToneKnob.setBounds (knobs);
            circuitView->setBounds (circuit.reduced (2));
        }

        preRack.setBounds (card (0.085f, "Pre-effects"));

        auto amp = card (0.42f, "Amp");
        {
            ampCardArea = amp;
            ampModel.setBounds (amp.removeFromTop (26));
            amp.removeFromTop (2);
            ampFace.setBounds (amp);
        }

        postRack.setBounds (card (0.085f, "Post-effects"));

        auto cab = card (0.21f, "Cabinet");   // 0.20 before FEAT-MIC's pad (6.3)
        {
            // mic-placement.md 6.3 (FEAT-MIC): the pad under the cabinet's
            // choices; the mics (or, on an acoustic, Pickup <-> Mic) beside it.
            micPadAcoustic = MicUi::isAcoustic (processor);
            // Two rows: model and blend, then the pad beside the mics (side by
            // side, so each keeps its full row height at small window sizes).
            const int bottomH = juce::jmin (MicPad::kHeight + 4, juce::jmax (cab.getHeight() / 2, cab.getHeight() - 44));
            auto top = cab.removeFromTop (cab.getHeight() - bottomH);
            {
                auto left = top.removeFromLeft (top.getWidth() / 2);
                cabModel.setBounds (left.withSizeKeepingCentre (left.getWidth(), juce::jmin (left.getHeight(), 40)));
            }
            micBlend.setBounds (top);

            auto padArea = cab.removeFromLeft (juce::jmin (MicPad::kWidth, cab.getWidth() / 2));
            micPad.setBounds (padArea.withSizeKeepingCentre (padArea.getWidth(), juce::jmin (MicPad::kHeight, padArea.getHeight())));
            cab.removeFromLeft (4);

            mic1.setVisible (! micPadAcoustic);
            mic2.setVisible (! micPadAcoustic);
            acMicMix.setVisible (micPadAcoustic);

            if (micPadAcoustic)
            {
                acMicMix.setBounds (cab);
            }
            else
            {
                mic1.setBounds (cab.removeFromLeft (cab.getWidth() / 2));
                mic2.setBounds (cab);
            }
        }

        auto room = card (0.12f, "Room");   // 0.13 before FEAT-MIC's pad
        {
            roomLight.setBounds (rigCards.getLast().first);
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
    auto meterColumn = guitarArea.removeFromRight (normalizationBadge.isVisible() ? NormalizationBadge::preferredWidth : 28);

    if (normalizationBadge.isVisible())   // output-normalization.md 5.1
        normalizationBadge.setBounds (meterColumn.removeFromBottom (24).reduced (0, 2));

    meter.setBounds (meterColumn.reduced (4, Metrics::grid));
    chordLabel.setBounds (guitarArea.removeFromTop (20).removeFromRight (120));
    // piano-roll-chord-display.md 1: the roll strip under the guitar illustration.
    pianoRoll.setVisible (pianoRoll.isWanted());

    if (pianoRoll.isVisible())
    {
        pianoRoll.setBounds (guitarArea.removeFromBottom (PianoRollStrip::kEasyHeight));
        guitarArea.removeFromBottom (Metrics::gridHalf);
    }

    guitarBody.setBounds (guitarArea);

    // visual-polish.md 4: the VU needle over the guitar's top-left corner, beside the level meter's column.
    vuMeter.setBounds (guitarArea.getX() + 4, guitarArea.getY() - 16, 128, 66);

    // 3.3 playing strip: mode, then the macros, then the whammy if fitted.
    {
        auto r = playingArea.reduced (4, 2);
        r.removeFromTop (14);
        {
            // auto-articulation.md 7.1 (FEAT-ASSIST): the 130 px mode column is two rows.
            auto column = r.removeFromLeft (130);
            auto second = column.removeFromBottom (AssistPill::kHeight);
            column.removeFromBottom (2);
            playingModeSelector.setLabelVisible (column.getHeight() >= 40);
            playingModeSelector.setBounds (column.withSizeKeepingCentre (130, juce::jmin (48, column.getHeight())));
            assistPill->setBounds (second.removeFromLeft (AssistPill::kWidth));
            second.removeFromLeft (4);
            assistStyle->setBounds (second.removeFromLeft (70));
        }
        r.removeFromLeft (Metrics::grid);

        // REALISM-B: the Tool selector takes a share of the strip beside the mode.
        toolSelector->setBounds (r.removeFromLeft (juce::jlimit (140, 280, r.getWidth() / 3)));
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

    // gui-integration 20: each strip's ? at its top right.
    {
        const int s = PanelHelpButton::kSize;
        rigHelp.setBounds (rigArea.getRight() - s - 6, rigArea.getY() + 3, s, s);
        playingHelp.setBounds (playingArea.getRight() - s - 4, playingArea.getY() + 1, s - 2, s - 2);
        toneHelp.setBounds (toneArea.getRight() - s - 4, toneArea.getY() + 1, s - 2, s - 2);
        rhythmHelp.setBounds (rhythmArea.getRight() - s - 4, rhythmArea.getCentreY() - s / 2, s, s);
    }

    // 3.5 rhythm strip: kit and dice, feel, on/off, the readout.
    {
        auto r = rhythmArea.reduced (4, 6);
        r.removeFromRight (PanelHelpButton::kSize + 4);   // the strip's ?
        /*  FEAT-JAM (jam-mode 8.2): the JAM group sits at the right end and never
            hides the band's style, level or state, so on a narrow window the
            rest of the strip gives way first - the hint, then the readout, then
            the genre box, then the group itself down to its minimum. Only
            when even that does not fit (narrower than the window's minimum)
            is the group hidden rather than drawn outside the strip. */
        const bool hasJam = jamGroup != nullptr;
        const int fixedW = 56 + 48 + Metrics::grid + 52 + Metrics::grid + 20   // + SPEC-SWEEP GD-10's arrow
                            + 64 + Metrics::gridHalf;   // + riff-library 7.3's Riffs button
        constexpr int feelMin = 60;
        int genreW = 170, readoutW = 110, hintW = hasJam ? 90 : 110;
        int jamW = hasJam ? JamStripGroup::preferredWidth : 0;
        int deficit = fixedW + genreW + jamW + readoutW + hintW + feelMin - r.getWidth();

        auto give = [&deficit] (int& w, int minimum)
        {
            const int cut = juce::jlimit (0, juce::jmax (0, w - minimum), deficit);
            w -= cut;
            deficit -= cut;
        };

        give (hintW, 0);
        give (readoutW, 70);
        give (genreW, 110);

        if (hasJam)
        {
            give (jamW, JamStripGroup::minimumWidth);
            jamGroup->setVisible (deficit <= 0);

            if (deficit > 0)
                jamW = 0;
        }

        rhythmLabel.setBounds (r.removeFromLeft (56));
        rhythmGenreBox.setBounds (r.removeFromLeft (genreW));
        rhythmDice.setBounds (r.removeFromLeft (48).reduced (2, 0));
        r.removeFromLeft (Metrics::grid);
        rhythmEnableButton.setBounds (r.removeFromLeft (52));
        r.removeFromLeft (Metrics::grid);
        riffsButton.setBounds (r.removeFromRight (64).reduced (2, 0));   // riff-library 7.3
        r.removeFromRight (Metrics::gridHalf);

        // The JAM group, SPEC-SWEEP GD-10's next-strum arrow, then readout and
        // hint - all at adaptive widths from the deficit pass above.
        if (hasJam)
            jamGroup->setBounds (r.removeFromRight (jamW));

        nextStrumArrow->setBounds (r.removeFromRight (20));   // SPEC-SWEEP GD-10
        rhythmReadout.setBounds (r.removeFromRight (readoutW));
        rhythmHintLabel.setBounds (r.removeFromRight (hintW));
        rhythmFeelSlider.setBounds (r);
    }

    if (riffDrawer != nullptr && riffDrawerOpen)
        riffDrawer->setBounds (getRiffDrawerBounds());
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
