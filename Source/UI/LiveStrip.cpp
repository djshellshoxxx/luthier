#include "LiveStrip.h"
#include "../PluginProcessor.h"

namespace luthier
{

namespace
{
    /** The sixteen snapshot colour tags (live-performance 1), built around the
        theme's own accents so a tagged snapshot still looks like part of the
        plugin rather than like a sticker on it. */
    juce::Colour snapshotTagColour (int tag)
    {
        static const juce::Colour tags[Snapshot::kNumColourTags] =
        {
            Palette::accent,           Palette::accentBright,   Palette::accentDim,
            Palette::secondary,        Palette::secondaryDim,   Palette::success,
            Palette::warning,          Palette::clip,           Palette::dataStream,
            juce::Colour (0xff7a6fd1), juce::Colour (0xffd16f9e), juce::Colour (0xff6f9ed1),
            juce::Colour (0xffd1b06f), juce::Colour (0xff8fd1c7), juce::Colour (0xffb0d16f),
            Palette::edgeBright
        };

        return tags[juce::jlimit (0, Snapshot::kNumColourTags - 1, tag)];
    }

    void drawLiveButton (juce::Graphics& g, juce::Rectangle<int> bounds,
                         const juce::String& text, bool on, juce::Colour onColour,
                         bool showOutlineOnly = false)
    {
        const auto area = bounds.toFloat().reduced (1.5f);

        g.setColour (on ? onColour : Palette::panelSunken);
        g.fillRoundedRectangle (area, 3.0f);

        // An active control is marked by a thicker border as well as by colour,
        // so that the state survives a colourblind palette (accessibility 2).
        g.setColour (on ? onColour.brighter (0.4f) : Palette::edge);
        g.drawRoundedRectangle (area, 3.0f, on ? 2.0f : 1.0f);

        if (showOutlineOnly)
            return;

        g.setColour (on ? Palette::backgroundDeep : Palette::textMuted);
        g.setFont (juce::Font (juce::FontOptions (12.0f)).boldened());
        g.drawText (text, bounds, juce::Justification::centred, false);
    }
}

juce::Colour getSnapshotTagColour (int tag)
{
    return snapshotTagColour (tag);
}

//==============================================================================
/*  SPEC-SWEEP: A11Y-9 - the strip paints its eight pads, so a screen reader and
    the Tab key see nothing there. Each pad gets an invisible button over it that
    carries the name and does what a click does; it lets mouse clicks through, so
    the strip's own right-click menu is untouched. */
class SnapshotStrip::SlotAccessor : public juce::Button
{
public:
    SlotAccessor() : juce::Button ({})
    {
        setInterceptsMouseClicks (false, false);
        setWantsKeyboardFocus (true);
    }

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        if (hasKeyboardFocus (false))
        {
            g.setColour (Palette::accentBright);
            g.drawRect (getLocalBounds(), 2);
        }
    }

    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override   { repaint(); }
};

SnapshotStrip::SnapshotStrip (LuthierAudioProcessor& p)
    : processor (p)
{
    for (int slot = 0; slot < kButtonsShown; ++slot)
    {
        auto* accessor = slotAccessors.add (new SlotAccessor());

        accessor->onClick = [this, slot]
        {
            const int index = bankStart() + slot;

            if (! processor.getSnapshots().getSnapshot (index).isEmpty())
                processor.recallSnapshot (index);
            else
                processor.captureSnapshot (index);

            refresh();
        };

        addAndMakeVisible (accessor);
    }

    prevButton.setTitle ("Previous snapshot");
    nextButton.setTitle ("Next snapshot");

    setTooltip ("Snapshots. Click to recall, Shift-click to save the current state, "
                "right-click to capture, rename or tag. Keys 1-9 recall directly; [ and ] step.");   // SPEC-SWEEP: GI-72

    prevButton.onClick = [this] { processor.previousSnapshot(); refresh(); };
    nextButton.onClick = [this] { processor.nextSnapshot(); refresh(); };

    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    updateSlotAccessors();
}

SnapshotStrip::~SnapshotStrip() = default;

int SnapshotStrip::bankStart() const
{
    // The strip follows the current snapshot in banks of eight, so recalling
    // snapshot 20 shows 16 to 23 rather than scrolling one at a time.
    const int current = juce::jmax (0, processor.getSnapshots().getCurrentSnapshot());

    return (current / kButtonsShown) * kButtonsShown;
}

juce::String SnapshotStrip::getPadText (int index, const juce::String& label)
{
    auto text = juce::String (index + 1);

    if (label.isNotEmpty())
        text << "  " << (label.length() > kPadLabelChars ? label.substring (0, kPadLabelChars - 1) + juce::String::fromUTF8 ("\u2026")
                                                         : label);

    return text;
}

juce::Button* SnapshotStrip::getSlotAccessor (int slot) const
{
    return slotAccessors[slot];
}

void SnapshotStrip::updateSlotAccessors()
{
    const auto& bank = processor.getSnapshots();
    const int start = bankStart();

    for (int slot = 0; slot < slotAccessors.size(); ++slot)
    {
        const int index = start + slot;
        const auto& snapshot = bank.getSnapshot (index);

        juce::String title ("Snapshot " + juce::String (index + 1));

        if (snapshot.isEmpty())
            title << ", empty";
        else
            title << ": " << snapshot.label << (index == bank.getCurrentSnapshot() ? ", active" : "");

        auto* accessor = slotAccessors[slot];

        if (accessor->getTitle() != title)
        {
            accessor->setTitle (title);
            accessor->setButtonText (title);
            accessor->setTooltip (title);
        }
    }
}

void SnapshotStrip::refresh()
{
    updateSlotAccessors();   // SPEC-SWEEP: A11Y-9

    const int current = processor.getSnapshots().getCurrentSnapshot();
    const int count = processor.getSnapshots().getNumSnapshots();

    if (current == lastCurrent && count == lastCount)
        return;

    lastCurrent = current;
    lastCount = count;

    repaint();
}

juce::Rectangle<int> SnapshotStrip::buttonBounds (int slot) const
{
    auto area = getLocalBounds().withTrimmedLeft (26).withTrimmedRight (26);

    const int width = juce::jmax (1, area.getWidth() / kButtonsShown);

    return { area.getX() + slot * width, area.getY(), width, area.getHeight() };
}

int SnapshotStrip::slotAt (juce::Point<int> position) const
{
    for (int slot = 0; slot < kButtonsShown; ++slot)
        if (buttonBounds (slot).contains (position))
            return slot;

    return -1;
}

void SnapshotStrip::paint (juce::Graphics& g)
{
    const auto& bank = processor.getSnapshots();

    const int start = bankStart();
    const int current = bank.getCurrentSnapshot();

    for (int slot = 0; slot < kButtonsShown; ++slot)
    {
        const int index = start + slot;
        const auto& snapshot = bank.getSnapshot (index);

        const bool filled = ! snapshot.isEmpty();
        const bool active = (index == current) && filled;

        const auto bounds = buttonBounds (slot);

        drawLiveButton (g, bounds, {}, active, snapshotTagColour (snapshot.colourTag), true);

        // The number always shows; the label only when there is room for it, so
        // that an eight-across strip on a narrow window stays readable.
        // SPEC-SWEEP: GI-73 - the label is cut to twelve characters.
        const auto text = (filled && bounds.getWidth() > 64) ? getPadText (index, snapshot.label)
                                                             : juce::String (index + 1);

        g.setColour (active ? Palette::backgroundDeep
                            : (filled ? Palette::textPrimary : Palette::textDisabled));

        auto font = juce::Font (juce::FontOptions (active ? 12.0f : 11.0f));

        // The active snapshot is bold as well as lit, so which one is live reads
        // without relying on colour (accessibility 2).
        g.setFont (active ? font.boldened() : font);

        g.drawText (text, bounds.reduced (4, 0), juce::Justification::centred, true);
    }

    // SPEC-SWEEP: GI-86 - an empty bank, or a click on an empty pad, says how
    // to fill it.
    bool anyFilled = false;

    for (int slot = 0; slot < kButtonsShown; ++slot)
        anyFilled = anyFilled || ! bank.getSnapshot (start + slot).isEmpty();

    if (showEmptyHint || ! anyFilled)
    {
        auto area = getLocalBounds().withTrimmedLeft (26).withTrimmedRight (26);
        g.setColour (Palette::textMuted);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (kEmptySlotHint, area.removeFromBottom (juce::jmin (14, area.getHeight() / 3)),
                    juce::Justification::centred, true);
    }
}

void SnapshotStrip::resized()
{
    auto bounds = getLocalBounds();

    prevButton.setBounds (bounds.removeFromLeft (26).reduced (1));
    nextButton.setBounds (bounds.removeFromRight (26).reduced (1));

    for (int slot = 0; slot < slotAccessors.size(); ++slot)   // SPEC-SWEEP: A11Y-9
        slotAccessors[slot]->setBounds (buttonBounds (slot));
}

void SnapshotStrip::mouseDown (const juce::MouseEvent& event)
{
    const int slot = slotAt (event.getPosition());

    if (slot < 0)
        return;

    const int index = bankStart() + slot;

    if (event.mods.isPopupMenu())
    {
        showSlotMenu (index);
        return;
    }

    // SPEC-SWEEP: GI-72 - click loads, Shift-click writes (keeping the pad's
    // label); a plain click on an empty pad only explains how to fill it
    // (GI-86), so a stray tap on stage never overwrites anything. Both go
    // through the undoable user actions (action-and-undo.md 3.7).
    const auto existing = processor.getSnapshots().getSnapshot (index);

    if (event.mods.isShiftDown())
    {
        processor.captureSnapshotAsUserAction (index, existing.label);
        showEmptyHint = false;
    }
    else if (! existing.isEmpty())
    {
        processor.recallSnapshotAsUserAction (index);
        showEmptyHint = false;
    }
    else
    {
        showEmptyHint = true;
    }

    refresh();
    repaint();
}

void SnapshotStrip::showSlotMenu (int index)
{
    const auto& snapshot = processor.getSnapshots().getSnapshot (index);
    const bool filled = ! snapshot.isEmpty();

    juce::PopupMenu menu;

    menu.addSectionHeader ("Snapshot " + juce::String (index + 1));
    menu.addItem (1, filled ? "Capture over this snapshot" : "Capture here");
    menu.addItem (2, "Rename", filled);
    menu.addItem (3, "Clear", filled);

    // SPEC-SWEEP: MM-49 - modulation-matrix 6.
    menu.addItem (4, "Includes modulation", filled, snapshot.includesModulation);
    menu.addSeparator();

    juce::PopupMenu colours;

    for (int tag = 0; tag < Snapshot::kNumColourTags; ++tag)
        colours.addItem (100 + tag, "Colour " + juce::String (tag + 1), filled,
                         snapshot.colourTag == tag);

    menu.addSubMenu ("Colour tag", colours, filled);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [this, index] (int result)
    {
        if (result == 0)
            return;

        if (result == 1)
        {
            processor.captureSnapshotAsUserAction (index, processor.getSnapshots().getSnapshot (index).label);
        }
        else if (result == 2)
        {
            // live-performance 0.5: no modal dialog on the live surface. The
            // rename happens in a non-modal editor that its own Escape dismisses.
            auto* editor = new juce::TextEditor();
            editor->setText (processor.getSnapshots().getSnapshot (index).label);
            editor->setSize (160, 24);
            editor->setEscapeAndReturnKeysConsumed (true);

            // launchAsynchronously takes ownership, so the box is kept by raw
            // pointer to dismiss it from the editor's own callback.
            auto& box = juce::CallOutBox::launchAsynchronously (
                std::unique_ptr<juce::Component> (editor),
                getScreenBounds(), nullptr);

            editor->onReturnKey = [this, editor, index, &box]
            {
                processor.renameSnapshotAsUserAction (index, editor->getText());   // action-and-undo.md 3.7
                refresh();
                repaint();

                box.dismiss();
            };

            editor->grabKeyboardFocus();
        }
        else if (result == 3)
        {
            processor.deleteSnapshotAsUserAction (index, false);   // action-and-undo.md 3.7
        }
        else if (result == 4)
        {
            processor.setSnapshotIncludesModulation (
                index, ! processor.getSnapshots().getSnapshot (index).includesModulation);
        }
        else if (result >= 100 && result < 100 + Snapshot::kNumColourTags)
        {
            processor.setSnapshotColourAsUserAction (index, result - 100);   // action-and-undo.md 3.7
        }

        refresh();
        repaint();
    });
}

//==============================================================================
SetlistTriptych::SetlistTriptych (LuthierAudioProcessor& p)
    : processor (p)
{
    setTooltip ("Setlist: previous, current and next. Click to open a setlist; "
                "PageUp and PageDown step through it.");
}

SetlistTriptych::~SetlistTriptych() = default;

void SetlistTriptych::refresh()
{
    auto& player = processor.getSetlist();

    auto textFor = [] (const SetlistEntry* entry)
    {
        return (entry != nullptr) ? entry->getDisplayName() : juce::String ("-");
    };

    const auto previous = textFor (player.getPreviousEntry());
    const auto current = textFor (player.getCurrentEntry());
    const auto next = textFor (player.getNextEntry());

    if (previous == previousText && current == currentText && next == nextText)
        return;

    previousText = previous;
    currentText = current;
    nextText = next;

    repaint();
}

void SetlistTriptych::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour (Palette::panelSunken);
    g.fillRoundedRectangle (bounds.toFloat(), 3.0f);

    g.setColour (Palette::edge);
    g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 3.0f, 1.0f);

    const int third = bounds.getWidth() / 3;

    auto previousArea = bounds.removeFromLeft (third);
    auto nextArea = bounds.removeFromRight (third);
    auto currentArea = bounds;

    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.setColour (Palette::textDisabled);
    g.drawText (previousText, previousArea.reduced (4, 0), juce::Justification::centredLeft, true);
    g.drawText (nextText, nextArea.reduced (4, 0), juce::Justification::centredRight, true);

    g.setFont (juce::Font (juce::FontOptions (12.0f)).boldened());
    g.setColour (Palette::textPrimary);
    g.drawText (currentText, currentArea.reduced (4, 0), juce::Justification::centred, true);
}

void SetlistTriptych::mouseDown (const juce::MouseEvent&)
{
    showSetlistMenu();
}

void SetlistTriptych::showSetlistMenu()
{
    juce::PopupMenu menu;

    const auto files = Setlist::findSetlists();

    menu.addSectionHeader ("Setlists");

    if (files.isEmpty())
        menu.addItem (1, "No setlists in your Luthier folder", false);
    else
        for (int i = 0; i < files.size(); ++i)
            menu.addItem (100 + i, files[i].getFileNameWithoutExtension());

    // SPEC-SWEEP: LP-5 - no "Open a setlist file..." here: a file dialog is
    // modal, and the LIVE tab is where setlists are managed.
    menu.addSeparator();

    const bool haveSetlist = processor.getSetlist().getSetlist().getNumEntries() > 0;

    menu.addItem (3, "Previous entry", haveSetlist);
    menu.addItem (4, "Next entry", haveSetlist);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [this, files] (int result)
    {
        if (result == 0)
            return;

        if (result == 3)
        {
            if (processor.getSetlist().previous())
                processor.applyCurrentSetlistEntry();
        }
        else if (result == 4)
        {
            if (processor.getSetlist().next())
                processor.applyCurrentSetlistEntry();
        }
        else if (result >= 100 && result - 100 < files.size())
        {
            processor.loadSetlist (files[result - 100]);
        }

        refresh();
    });
}

//==============================================================================
TapPad::TapPad (LuthierAudioProcessor& p)
    : processor (p)
{
    setTooltip ("Tap tempo. Tap four times or more. While the host is playing, "
                "the host's tempo wins unless internal tempo is forced.");

    motion.startTimerHz (*this, 30);
}

TapPad::~TapPad()
{
    motion.stopTimer();
}

void TapPad::timerCallback()
{
    const double bpm = processor.getEffectiveTempo();
    const double now = juce::Time::getMillisecondCounterHiRes();

    // live-performance 5: the pad blinks on each beat, in the accent colour.
    const double beatMs = 60000.0 / juce::jmax (20.0, bpm);

    bool shouldRepaint = false;

    // cpu-quality-modes 6: no flash at Off; the BPM still updates.
    if (! AnimationPolicy::get().mayAnimate (AnimationPolicy::Transition))
    {
        shouldRepaint = beatLit;
        beatLit = false;
        lastBeatMs = now;
    }
    else
    if (now - lastBeatMs >= beatMs)
    {
        lastBeatMs = now;
        beatLit = true;
        shouldRepaint = true;
    }
    else if (beatLit && (now - lastBeatMs) > juce::jmin (90.0, beatMs * 0.4))
    {
        beatLit = false;
        shouldRepaint = true;
    }

    if (std::abs (bpm - displayedBpm) > 0.05)
    {
        displayedBpm = bpm;
        shouldRepaint = true;
    }

    if (shouldRepaint)
        repaint();
}

void TapPad::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6

    drawLiveButton (g, getLocalBounds(), {}, beatLit, Palette::accent, true);

    g.setColour (Palette::textMuted);
    g.setFont (juce::Font (juce::FontOptions (8.0f)));
    g.drawText ("TAP", getLocalBounds().removeFromTop (12), juce::Justification::centred, false);

    g.setColour (beatLit ? Palette::accentBright : Palette::textPrimary);
    g.setFont (juce::Font (juce::FontOptions (14.0f)).boldened());
    g.drawText (juce::String (displayedBpm, 1),
                getLocalBounds().withTrimmedTop (10), juce::Justification::centred, false);
}

void TapPad::mouseDown (const juce::MouseEvent&)
{
    processor.tapTempoNow();
    repaint();
}

//==============================================================================
LiveStrip::LiveStrip (LuthierAudioProcessor& p)
    : processor (p)
{
    snapshotStrip = std::make_unique<SnapshotStrip> (processor);
    triptych = std::make_unique<SetlistTriptych> (processor);
    tapPad = std::make_unique<TapPad> (processor);

    addAndMakeVisible (*snapshotStrip);
    addAndMakeVisible (*triptych);
    addAndMakeVisible (*tapPad);

    // SPEC-SWEEP: LP-11 - footswitch / CC assignment for every live action.
    ccButton = std::make_unique<LiveActionButton> (processor, "CC");
    addAndMakeVisible (*ccButton);
    // FEAT-JAM (jam-mode 8.2): a tap starts or stops the band, a long press is FILL.
    jamPill = std::make_unique<JamPill> (processor, JamPill::Mode::live);
    addChildComponent (*jamPill);
    refreshJamPill();

    // ---- morph -------------------------------------------------------------------
    morphEnable.setClickingTogglesState (true);
    morphEnable.setTooltip ("Morph continuously between the two snapshot slots.");

    morphEnable.onClick = [this]
    {
        processor.getSnapshots().setMorphEnabled (morphEnable.getToggleState());
        refreshMorphControls();
    };

    addAndMakeVisible (morphEnable);

    slotAButton.setTooltip ("Morph slot A. Click to choose which snapshot it holds.");
    slotBButton.setTooltip ("Morph slot B. Click to choose which snapshot it holds.");

    slotAButton.onClick = [this] { showSlotMenu (false); };
    slotBButton.onClick = [this] { showSlotMenu (true); };

    addAndMakeVisible (slotAButton);
    addAndMakeVisible (slotBButton);

    morphSlider.setRange (0.0, 1.0, 0.001);
    morphSlider.setTooltip ("Morph position. Assign an expression pedal to this "
                            "through MIDI Learn.");

    // SPEC-SWEEP: LP-16 - the knob is the snapshot_morph parameter, which the
    // processor's timer follows (automation and modulation included).
    morphAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.getState(), ParamIDs::snapshotMorph, morphSlider);

    addAndMakeVisible (morphSlider);

    // ---- kill switch ----------------------------------------------------------------
    // Momentary, not a toggle: it mutes for as long as it is held, which is what
    // live-performance 6 describes and what a kill switch on a guitar does.
    killButton.setTooltip ("Kill switch: mutes the output for as long as it is held.");

    killButton.onStateChange = [this]
    {
        const bool down = killButton.isDown();

        if (down != lastKillActive)
        {
            lastKillActive = down;
            processor.getKillSwitch().setActive (down);
            repaint();
        }
    };

    addAndMakeVisible (killButton);

    // ---- monitor ---------------------------------------------------------------------
    monitorLabel.setText ("MON", juce::dontSendNotification);
    monitorLabel.setFont (juce::Font (juce::FontOptions (9.0f)).boldened());
    monitorLabel.setColour (juce::Label::textColourId, Palette::textMuted);
    monitorLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (monitorLabel);

    monitorLevel.setRange (-60.0, 12.0, 0.1);
    monitorLevel.setValue (processor.getMonitorMix().getLevelDb(), juce::dontSendNotification);
    monitorLevel.setDoubleClickReturnValue (true, 0.0);
    monitorLevel.setTooltip ("Monitor level. The monitor mix goes to its own "
                             "output bus, never to the main output.");

    monitorLevel.onValueChange = [this]
    {
        processor.getMonitorMix().setLevelDb (monitorLevel.getValue());
    };

    addAndMakeVisible (monitorLevel);

    refreshMorphControls();
    motion.startTimerHz (*this, 20);
}

LiveStrip::~LiveStrip()
{
    motion.stopTimer();
}

void LiveStrip::refreshMorphControls()
{
    const auto& bank = processor.getSnapshots();

    morphEnable.setToggleState (bank.isMorphEnabled(), juce::dontSendNotification);

    slotAButton.setButtonText ("A" + juce::String (bank.getMorphSlotA() + 1));
    slotBButton.setButtonText ("B" + juce::String (bank.getMorphSlotB() + 1));

    morphSlider.setEnabled (bank.isMorphEnabled());
}

void LiveStrip::showSlotMenu (bool slotB)
{
    const auto& bank = processor.getSnapshots();

    juce::PopupMenu menu;
    menu.addSectionHeader (slotB ? "Morph slot B" : "Morph slot A");

    const int count = juce::jmax (1, bank.getNumSnapshots());
    const int currentSlot = slotB ? bank.getMorphSlotB() : bank.getMorphSlotA();

    for (int i = 0; i < count; ++i)
    {
        const auto& snapshot = bank.getSnapshot (i);

        menu.addItem (i + 1,
                      juce::String (i + 1) + "  "
                        + (snapshot.label.isNotEmpty() ? snapshot.label : juce::String ("(empty)")),
                      ! snapshot.isEmpty(), i == currentSlot);
    }

    menu.showMenuAsync (juce::PopupMenu::Options()
                          .withTargetComponent (slotB ? &slotBButton : &slotAButton),
                        [this, slotB] (int result)
    {
        if (result == 0)
            return;

        auto& bank2 = processor.getSnapshots();

        if (slotB)
            bank2.setMorphSlots (bank2.getMorphSlotA(), result - 1);
        else
            bank2.setMorphSlots (result - 1, bank2.getMorphSlotB());

        refreshMorphControls();
    });
}

void LiveStrip::refreshJamPill()
{
    // FEAT-JAM (jam-mode 8.2): the JAM pill shows only while jam_enabled is on.
    auto* enabled = processor.getState().getParameter (ParamIDs::jamEnabled);
    const bool show = enabled != nullptr && enabled->getValue() > 0.5f;

    if (jamPill->isVisible() != show)
    {
        jamPill->setVisible (show);
        resized();
    }
}

void LiveStrip::timerCallback()
{
    snapshotStrip->refresh();
    triptych->refresh();

    refreshJamPill();   // FEAT-JAM
    // (The morph knob needs no polling: SPEC-SWEEP LP-16 attaches it to the
    // snapshot_morph parameter.)

    if (std::abs (monitorLevel.getValue() - processor.getMonitorMix().getLevelDb()) > 0.05)
        monitorLevel.setValue (processor.getMonitorMix().getLevelDb(), juce::dontSendNotification);
}

void LiveStrip::paint (juce::Graphics& g)
{
    AnimationPolicy::notePaint (*this);   // cpu-quality-modes 6

    g.setColour (Palette::panel);
    g.fillRect (getLocalBounds());

    g.setColour (Palette::edge);
    g.drawLine (0.0f, 0.0f, (float) getWidth(), 0.0f, 1.0f);

    // live-performance 6: while the kill switch is down it is a large red pill.
    if (processor.getKillSwitch().isActive())
    {
        g.setColour (Palette::clip.withAlpha (0.18f));
        g.fillRect (getLocalBounds());
    }
}

void LiveStrip::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::gridHalf);

    // Every control on this strip is a touch target, so the row is laid out at
    // the full height rather than padded into something smaller.
    const int height = juce::jmax (bounds.getHeight(), 1);

    auto takeLeft = [&bounds, height] (int width)
    {
        auto r = bounds.removeFromLeft (width).withHeight (height);
        bounds.removeFromLeft (Metrics::gridHalf);
        return r;
    };

    auto takeRight = [&bounds, height] (int width)
    {
        auto r = bounds.removeFromRight (width).withHeight (height);
        bounds.removeFromRight (Metrics::gridHalf);
        return r;
    };

    // Right-hand end first, so the snapshot strip gets whatever is left over -
    // it is the control that most benefits from extra width.
    monitorLevel.setBounds (takeRight (juce::jmin (130, bounds.getWidth() / 5)));
    monitorLabel.setBounds (takeRight (32));

    killButton.setBounds (takeRight (LiveStrip::kTouchTargetHeight + 20));

    morphSlider.setBounds (takeRight (height));
    slotBButton.setBounds (takeRight (40));
    slotAButton.setBounds (takeRight (40));
    morphEnable.setBounds (takeRight (64));

    tapPad->setBounds (takeLeft (64));
    ccButton->setBounds (takeLeft (LiveStrip::kTouchTargetHeight));   // SPEC-SWEEP: LP-11

    if (jamPill->isVisible())   // FEAT-JAM
        jamPill->setBounds (takeLeft (88));
    triptych->setBounds (takeLeft (juce::jmax (120, bounds.getWidth() / 3)));

    snapshotStrip->setBounds (bounds.withHeight (height));
}

} // namespace luthier
