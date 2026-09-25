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

        // One line, fitted (smaller before it is cut short); clear of the LED.
        const auto text = empty ? juce::String (i + 1) : name.upToFirstOccurrenceOf (" (off)", false, false);
        auto textArea = r.reduced (3.0f, 1.0f).toNearestInt();

        if (! empty)
            textArea.removeFromLeft (7);

        const auto label = Fonts::fitLabel (text, Fonts::ui (9.5f), (float) textArea.getWidth(), 0.0f);

        if (label.ellipsised && textArea.getHeight() >= 24)
        {
            g.setFont (Fonts::ui (9.5f));
            g.drawFittedText (text, textArea, juce::Justification::centred, 2);
        }
        else
        {
            Fonts::drawFittedLabel (g, label, textArea, juce::Justification::centred);
        }
    }
}

juce::Component* CompactRack::openSlot (int index)
{
    if (! juce::isPositiveAndBelow (index, kSlots))
        return nullptr;

    // The same slot editor as Advanced mode, as a popover (3.2). It follows
    // its pedal's height itself: the callout box re-lays out when its content
    // resizes, so a pedal picked into an empty slot gets its whole face.
    auto editor = std::make_unique<PedalSlotComponent> (processor, postChain, index);
    editor->setSizesToContent (true);
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

    // A piano keyboard under the guitar, behind a "Keys" toggle (PianoKeyboard.h).
    keysDrawer = std::make_unique<PianoKeyboardDrawer> (p);
    keysDrawer->addTo (*this);

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

    addAndMakeVisible (fingersToggle);
    fingersToggle.attachTo (processor, ParamIDs::useFingers,
                            "Play with the fingers instead of a pick: a rounder, softer attack "
                            "with no pick click. Acoustic and classical guitars start with it on.");

    muteButton = std::make_unique<EasyMuteButton> (processor);
    addAndMakeVisible (*muteButton);

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

    // Shown only when the strip is shorter than its cards' floors (layoutRigStrip).
    rigScrollBar.setAutoHide (false);
    rigScrollBar.setSingleStepSize (24.0);
    rigScrollBar.addListener (this);
    addChildComponent (rigScrollBar);

    // 3. Amp: the model, and the face carrying gain, bass, mid, treble, presence and master.
    ampModel.attachTo (processor, ParamIDs::ampModel, "Amp model");
    ampModel.setLabelVisible (false);   // the card's AMP plate is its label
    ampModel.getComboBox().setTitle ("Amp model");
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
    roomSize.setLabelVisible (false);   // the card's ROOM plate is its label
    roomSize.getComboBox().setTitle ("Room size");
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

    // The room light follows the size and blend the room is set to (10 Hz is
    // plenty: these are settings, not a signal).
    {
        float size = 0.5f, wet = 0.0f;

        if (auto* p = processor.getState().getParameter (ParamIDs::roomSize))
            size = p->getValue();

        if (auto* p = processor.getState().getParameter (ParamIDs::roomBlend))
            wet = p->getValue();

        if (auto* p = processor.getState().getParameter (ParamIDs::macroSpace))
            wet = juce::jlimit (0.0f, 1.0f, wet + p->getValue() * 0.45f);   // as the engine adds it

        if (std::abs (size - roomLightSize) > 0.01f || std::abs (wet - roomLightWet) > 0.01f)
        {
            roomLightSize = size;
            roomLightWet = wet;
            repaint (roomArea);
        }
    }
}

//==============================================================================
EasyPanel::CardHeights EasyPanel::floorHeights() noexcept
{
    /*  The least each card keeps: the guitar its Small knobs with a body, a
        rack two rows of 20-point slots, the amp its one-row face, the cabinet
        two unlabelled rows of combos, the room its combo beside a knob. */
    CardHeights f;
    f.circuit = kCircuitMinHeight;
    f.rack = kRackMinHeight;
    f.amp = 100;
    f.cab = 76;
    f.room = 76;
    return f;
}

EasyPanel::CardHeights EasyPanel::cardHeights (int total) noexcept
{
    /*  What each card needs (title row and padding included): the guitar card a
        Small knob with its rows beside the response miniature, a rack two rows
        of 20-point slots (a readable name each), the amp face two rows of three
        knobs of about 44 points with their printed labels, the cabinet a combo
        over the two labelled microphones, the room a labelled combo beside a
        knob. */
    CardHeights h;
    h.circuit = 80;
    h.rack = kRackMinHeight;
    h.amp = 228;
    h.cab = 92;
    h.room = 82;

    const int needed = h.circuit + 2 * h.rack + h.amp + h.cab + h.room;

    if (total < needed)
    {
        /*  A short strip (Live Mode's strip under the header takes 52 points).
            Squashing every card alike left the racks as 3-point slivers and the
            guitar card's knobs with no body at all, so the cards give way in
            order: the amp first, down to its one-row face; then the guitar, the
            cabinet and the room down to their floors (floorHeights). The racks
            keep their two rows of slots throughout. The panel scrolls a strip
            shorter than every floor together (layoutRigStrip), so the scaled
            branch below is only the arithmetic's answer, never a layout. */
        const auto floor = floorHeights();

        const int floors = floor.circuit + 2 * floor.rack + floor.amp + floor.cab + floor.room;

        if (total < floors)
        {
            const float scale = (float) total / (float) floors;
            h.circuit = juce::roundToInt ((float) floor.circuit * scale);
            h.rack = juce::roundToInt ((float) floor.rack * scale);
            h.cab = juce::roundToInt ((float) floor.cab * scale);
            h.room = juce::roundToInt ((float) floor.room * scale);
            h.amp = total - h.circuit - 2 * h.rack - h.cab - h.room;
            return h;
        }

        int deficit = needed - total;

        // The amp first.
        const int fromAmp = juce::jmin (deficit, h.amp - floor.amp);
        h.amp -= fromAmp;
        deficit -= fromAmp;

        // Then the others, each in proportion to what it has above its floor.
        const int slack = (h.circuit - floor.circuit) + (h.cab - floor.cab) + (h.room - floor.room);

        if (deficit > 0 && slack > 0)
        {
            const float share = (float) deficit / (float) slack;
            h.circuit -= juce::roundToInt ((float) (h.circuit - floor.circuit) * share);
            h.cab -= juce::roundToInt ((float) (h.cab - floor.cab) * share);
            h.room -= juce::roundToInt ((float) (h.room - floor.room) * share);
        }

        // Rounding lands on the amp, which always has the most room.
        h.amp = total - h.circuit - 2 * h.rack - h.cab - h.room;
        return h;
    }

    // Surplus: mostly to the amp and the guitar, which have the most to show.
    const int surplus = total - needed;
    h.circuit += juce::roundToInt ((float) surplus * 0.22f);
    h.rack += juce::roundToInt ((float) surplus * 0.05f);
    h.cab += juce::roundToInt ((float) surplus * 0.12f);
    h.room += juce::roundToInt ((float) surplus * 0.08f);
    h.amp = total - h.circuit - 2 * h.rack - h.cab - h.room;
    return h;
}

int EasyPanel::rigFloorHeight() noexcept
{
    const auto f = floorHeights();
    return f.circuit + 2 * f.rack + f.amp + f.cab + f.room;
}

void EasyPanel::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // The wheel over a scrolling rig strip scrolls it (knobs and combos pass it up).
    if (rigScrollBar.isVisible() && rigArea.withTop (0).withBottom (getHeight())
                                           .contains (e.getEventRelativeTo (this).getPosition()))
    {
        rigScrollBar.setCurrentRangeStart (rigScrollBar.getCurrentRangeStart()
                                           - (double) wheel.deltaY * 120.0,
                                           juce::sendNotificationSync);
        return;
    }

    juce::Component::mouseWheelMove (e, wheel);
}

void EasyPanel::scrollBarMoved (juce::ScrollBar*, double newRangeStart)
{
    const int scroll = juce::roundToInt (newRangeStart);

    if (scroll != rigScroll)
    {
        rigScroll = scroll;
        layoutRigStrip();
        repaint (rigArea.withTop (0).withBottom (getHeight()));
    }
}

void EasyPanel::layoutRigStrip()
{
    rigCards.clearQuick();

    /*  TODO 2h: at 1200 x 720 the strip is about 600 points tall for six cards,
        and equal-ish shares left the amp's six knobs in one row of 16-point
        bodies. Each card now has the height its controls need (cardHeights),
        the amp card's need is two rows of three on the face, and any surplus
        on a taller window goes mostly to the amp and the guitar. A shorter
        strip gives way card by card down to the floors (cardHeights); one
        shorter than every floor together (Live Mode with the practice drawer
        open) scrolls, with a scrollbar down its right edge, rather than
        squash the racks to slivers and the knobs to nothing. */
    const int floor = rigFloorHeight();
    const bool scrolls = getHeight() < floor;
    auto rig = rigArea;

    // Just short of the floors: the strip borrows the panel's margins first.
    if (! scrolls && rig.getHeight() < floor)
        rig = rig.withY (juce::jmax (0, rig.getY() - (floor - rig.getHeight()) / 2)).withHeight (floor);

    if (scrolls)
    {
        // The strip runs to the panel's edges, so a card scrolled part-way out
        // is cut by the panel rather than drawn into the margin.
        auto column = rigArea.withTop (0).withBottom (getHeight());
        rigScrollBar.setBounds (column.removeFromRight (8));
        column.removeFromRight (2);

        rigScrollBar.setRangeLimits (0.0, (double) floor, juce::dontSendNotification);
        rigScrollBar.setCurrentRange ((double) rigScroll, (double) column.getHeight(), juce::dontSendNotification);
        rigScroll = juce::roundToInt (rigScrollBar.getCurrentRangeStart());
        rigScrollBar.setVisible (true);

        rig = { column.getX(), column.getY() - rigScroll, column.getWidth(), floor };
    }
    else
    {
        rigScroll = 0;
        rigScrollBar.setVisible (false);
    }

    const auto heights = cardHeights (rig.getHeight());

    auto card = [&rig, this] (int height, const juce::String& title, int titleRow = 16)
    {
        auto r = rig.removeFromTop (height);
        rigCards.add ({ r.reduced (0, 2), title });
        auto inner = r.reduced (6, 4);
        inner.removeFromTop (titleRow);   // the card's title
        return inner;
    };

    auto circuit = card (heights.circuit, "Guitar");
    {
        /*  The two knobs at the left, the response miniature in what is left
            to their right: side by side, so they can never overlap. The knobs
            take the width their Small size asks for (never more than half the
            card), and the miniature is only drawn when it has room to say
            something. */
        const int knobW = juce::jmin (circuit.getWidth() / 2,
                                      juce::jmax (LuthierKnob::preferredWidthFor (LuthierKnob::Size::Small),
                                                  circuit.getWidth() / 4));
        guitarVolumeKnob.setBounds (circuit.removeFromLeft (knobW));
        guitarToneKnob.setBounds (circuit.removeFromLeft (knobW));
        circuit.removeFromLeft (Metrics::gridHalf);
        circuitView->setBounds (circuit.reduced (2));
        circuitView->setVisible (circuitView->getWidth() >= 48 && circuitView->getHeight() >= 24);
    }

    preRack.setBounds (card (heights.rack, "Pre-effects"));

    {
        // The model choice sits in the title row, beside the AMP plate, so the
        // whole card below it is the face (visual-polish.md 2: the model is
        // not a control an amp has on its front).
        auto amp = card (heights.amp, "Amp", ampTitleRow);
        auto titleRow = juce::Rectangle<int> (amp.getX(), amp.getY() - ampTitleRow, amp.getWidth(), ampTitleRow);
        ampModel.setBounds (titleRow.removeFromRight (juce::jmax (120, titleRow.getWidth() - 64)).reduced (0, 1));
        ampFace.setBounds (amp);
    }

    postRack.setBounds (card (heights.rack, "Post-effects"));

    auto cab = card (heights.cab, "Cabinet");
    {
        // The blend knob has the card's full height at the right; the cabinet
        // and the two microphones stack at the left.
        micBlend.setBounds (cab.removeFromRight (juce::jmax (56, cab.getWidth() * 2 / 7)));
        cab.removeFromRight (Metrics::gridHalf);
        const int rowH = LuthierChoice::labelHeight + Metrics::rowHeight;

        /*  The plate already says CABINET: a short card drops the model's own
            label first, then the microphones' (their tooltips still name
            them), so every combo keeps its full height. */
        const bool cabLabel = cab.getHeight() >= 2 * rowH + Metrics::gridHalf;
        const bool micLabels = cab.getHeight() >= Metrics::rowHeight + rowH + Metrics::gridHalf;
        cabModel.setLabelVisible (cabLabel);
        mic1.setLabelVisible (micLabels);
        mic2.setLabelVisible (micLabels);

        cabModel.setBounds (cab.removeFromTop (cabLabel ? rowH : Metrics::rowHeight));
        cab.removeFromTop (Metrics::gridHalf);
        auto mics = cab.removeFromTop (micLabels ? rowH : Metrics::rowHeight);
        mic1.setBounds (mics.removeFromLeft (mics.getWidth() / 2).withTrimmedRight (2));
        mic2.setBounds (mics.withTrimmedLeft (2));
    }

    auto room = card (heights.room, "Room");
    roomArea = rigCards.getLast().first;
    {
        // The plate says ROOM, so the size choice needs no label of its own.
        roomMix.setBounds (room.removeFromRight (juce::jmax (56, room.getWidth() * 2 / 7)));
        room.removeFromRight (Metrics::gridHalf);
        roomSize.setBounds (room.withSizeKeepingCentre (room.getWidth(), Metrics::rowHeight));
    }
}

void EasyPanel::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::windowPadding, Metrics::grid);

    // ---- 3.2: the rig strip down the right, 280 points ------------------------------
    rigArea = bounds.removeFromRight (juce::jmin (kRigWidth, bounds.getWidth() / 3));
    bounds.removeFromRight (Metrics::grid);

    layoutRigStrip();

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
    auto topRow = guitarArea.removeFromTop (20);
    chordLabel.setBounds (topRow.removeFromRight (120));
    guitarArea = keysDrawer->layout (guitarArea, topRow.removeFromRight (56).reduced (0, 1));
    guitarBody.setBounds (guitarArea);

    // 3.3 playing strip: mode, then the macros, then the whammy if fitted.
    {
        auto r = playingArea.reduced (4, 2);
        r.removeFromTop (14);
        playingModeSelector.setBounds (r.removeFromLeft (130).withSizeKeepingCentre (130, juce::jmin (48, r.getHeight())));
        r.removeFromLeft (Metrics::gridHalf);
        fingersToggle.setBounds (r.removeFromLeft (64).withSizeKeepingCentre (64, Metrics::buttonHeight));
        r.removeFromLeft (Metrics::gridHalf);
        muteButton->setBounds (r.removeFromLeft (96).withSizeKeepingCentre (96, Metrics::buttonHeight));
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

void EasyPanel::paintRoomLight (juce::Graphics& g, juce::Rectangle<float> card, float size, float wet)
{
    /*  visual-polish.md 4: the ROOM card's background warms and widens with the
        room size and the wet level, so the space being played in has a look. A
        warm pool of light from the card's centre: its reach follows the size,
        its strength the wet level; a dry room leaves the walnut alone. Static
        per setting (nothing animates), and off under High contrast, which
        turns every gradient off (visual-polish.md 0.2). */
    if (! Palette::textured || wet <= 0.001f)
        return;

    const auto warm = Palette::accent.interpolatedWith (Palette::warning, 0.35f);
    const float reach = (0.35f + 0.65f * size) * juce::jmax (card.getWidth(), card.getHeight()) * 0.75f;
    const float strength = 0.08f + 0.30f * wet;
    const auto centre = card.getCentre();

    juce::ColourGradient pool (warm.withAlpha (strength), centre.x, centre.y,
                               warm.withAlpha (0.0f), centre.x + reach, centre.y, true);
    pool.addColour (0.45, warm.withAlpha (strength * 0.55f));

    juce::Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (card.reduced (1.0f).toNearestInt());
    g.setGradientFill (pool);
    g.fillRoundedRectangle (card.reduced (1.0f), Metrics::panelCorner);
}

void EasyPanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    // The rig strip's cards, each a small framed panel with its name (clipped
    // to the strip, which may be scrolling).
    {
        juce::Graphics::ScopedSaveState save (g);

        if (rigScrollBar.isVisible())
            g.reduceClipRegion (rigArea.withTop (0).withBottom (getHeight()));

        for (auto& [r, title] : rigCards)
        {
            LuthierLookAndFeel::drawPanel (g, r.toFloat());

            if (r == roomArea)
                paintRoomLight (g, r.toFloat(), roomLightSize, roomLightWet);

            LuthierLookAndFeel::drawSectionHeader (g, r.reduced (6, 2).withHeight (18), title);
        }
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
