#include "LivePanel.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../Accessibility/Localisation.h"

namespace luthier
{

namespace
{
    constexpr int kPad = 10;
    constexpr int kRowHeight = 26;
    constexpr int kHeadingHeight = 20;

    /** Under the grid: the selected-slot readout, then the empty-bank hint. */
    constexpr int kSlotLabelHeight = 18;
    constexpr int kHintHeight = 16;

    /** The setlist shows six rows; the panel scrolls rather than the list. */
    constexpr int kSetlistRows = 6;
    constexpr int kSetlistRowHeight = 22;

    /** gui-integration 8: a label is shown truncated to twelve characters. */
    constexpr int kLabelChars = 12;

    /*  The sixteen snapshot colour tags (live-performance 1). The same table
        as the live strip's, which keeps it in an anonymous namespace; the two
        must agree, because a tag is picked here and seen there. */
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

    juce::String truncatedLabel (const juce::String& label)
    {
        return label.length() > kLabelChars ? label.substring (0, kLabelChars - 1) + juce::String::charToString (0x2026)
                                            : label;
    }

    void styleHeading (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (Fonts::ui (11.0f).boldened());
        label.setColour (juce::Label::textColourId, Palette::accent);
    }

    void styleNote (juce::Label& label, juce::Colour colour = Palette::textMuted)
    {
        label.setFont (Fonts::ui (10.5f));
        label.setColour (juce::Label::textColourId, colour);
    }
}

//==============================================================================
//  SnapshotGrid
//==============================================================================
SnapshotGrid::SnapshotGrid (LuthierAudioProcessor& p)
    : processor (p)
{
    setTooltip (tr ("live.grid.tooltip"));

    AccessibleSetup::configureDescriptive (*this, tr ("live.grid.name"), tr ("live.grid.tooltip"));
}

int SnapshotGrid::cellHeightFor (int width) noexcept
{
    return juce::jlimit (20, 28, width / kColumns / 3);
}

juce::Rectangle<int> SnapshotGrid::boundsForSlot (int slot) const
{
    if (! juce::isPositiveAndBelow (slot, kColumns * kRows))
        return {};

    const int cellW = getWidth() / kColumns;
    const int cellH = getHeight() / kRows;

    return { (slot % kColumns) * cellW, (slot / kColumns) * cellH, cellW, cellH };
}

int SnapshotGrid::slotAt (juce::Point<int> position) const
{
    /*  Negative coordinates first, because integer division truncates toward
        zero: -4 / cellW is 0, so a point above and left of the grid would report
        the top-left pad. A test caught this; a user with a slightly oversized
        parent would have found it by selecting slot 1 from empty space. */
    if (position.x < 0 || position.y < 0)
        return -1;

    const int cellW = juce::jmax (1, getWidth() / kColumns);
    const int cellH = juce::jmax (1, getHeight() / kRows);

    const int column = position.x / cellW;
    const int row = position.y / cellH;

    if (! juce::isPositiveAndBelow (column, kColumns) || ! juce::isPositiveAndBelow (row, kRows))
        return -1;

    return row * kColumns + column;
}

bool SnapshotGrid::isFilled (int slot) const
{
    const auto& bank = processor.getSnapshots();
    return slot >= 0 && slot < bank.getNumSnapshots() && ! bank.getSnapshot (slot).isEmpty();
}

void SnapshotGrid::setSelectedSlot (int slot)
{
    const int clamped = juce::jlimit (0, kColumns * kRows - 1, slot);

    if (clamped == selected)
        return;

    selected = clamped;
    repaint();

    if (onSelectionChanged != nullptr)
        onSelectionChanged();
}

void SnapshotGrid::bankChanged()
{
    repaint();

    if (onBankChanged != nullptr)
        onBankChanged();
}

void SnapshotGrid::captureSlot (int slot)
{
    if (! juce::isPositiveAndBelow (slot, kColumns * kRows))
        return;

    /*  A capture over a filled slot destroys what was there, and there is no undo
        through the snapshot bank, so the existing label and colour are carried
        across rather than silently replaced - the pad keeps its name and gets a
        new sound, which is what re-capturing a pad means to a player. */
    const auto& bank = processor.getSnapshots();

    const juce::String existing = isFilled (slot) ? bank.getSnapshot (slot).label : juce::String();
    const int tag = isFilled (slot) ? bank.getSnapshot (slot).colourTag : -1;

    // action-and-undo.md 3.7 snapshot-save: the bank is in the state block, so
    // the entry holds whatever the slot held before the capture.
    processor.pushUndoState ("Save snapshot " + juce::String (slot + 1)
                               + (existing.isNotEmpty() ? " " + existing : juce::String()));
    processor.getSnapshots().capture (slot, existing, tag);
    bankChanged();
}

void SnapshotGrid::recallSlot (int slot)
{
    if (isFilled (slot))
    {
        // Through the processor, which pushes the recall's undo entry
        // (action-and-undo.md 3.7) and cancels a running preset morph.
        processor.recallSnapshot (slot);
        bankChanged();
    }
}

void SnapshotGrid::clearSlot (int slot)
{
    if (isFilled (slot))
    {
        processor.pushUndoState ("Delete snapshot " + juce::String (slot + 1));   // 3.7 snapshot-delete
        processor.getSnapshots().remove (slot);
        bankChanged();
    }
}

void SnapshotGrid::setSlotColourTag (int slot, int tag)
{
    if (isFilled (slot))
    {
        processor.pushUndoState ("Change snapshot " + juce::String (slot + 1) + " colour");   // 3.7 snapshot-color
        processor.getSnapshots().setColourTag (slot, tag);
        bankChanged();
    }
}

void SnapshotGrid::showSlotMenu (int slot)
{
    if (! juce::isPositiveAndBelow (slot, kColumns * kRows))
        return;

    const bool filled = isFilled (slot);
    const auto& snapshot = processor.getSnapshots().getSnapshot (slot);

    juce::PopupMenu menu;

    menu.addSectionHeader (tr ("live.snapshot", { { "number", juce::String (slot + 1) } }));
    menu.addItem (1, tr ("live.menu.recall"), filled);
    menu.addItem (2, tr (filled ? "live.menu.captureOver" : "live.menu.captureHere"));
    menu.addItem (3, tr ("live.rename"), filled);

    juce::PopupMenu colours;

    for (int tag = 0; tag < Snapshot::kNumColourTags; ++tag)
        colours.addItem (100 + tag, tr ("live.colour", { { "number", juce::String (tag + 1) } }),
                         filled, filled && snapshot.colourTag == tag);

    menu.addSubMenu (tr ("live.colourTag"), colours, filled);
    menu.addSeparator();
    menu.addItem (4, tr ("live.clear"), filled);

    const auto cell = boundsForSlot (slot);

    menu.showMenuAsync (juce::PopupMenu::Options()
                          .withTargetComponent (this)
                          .withTargetScreenArea (localAreaToGlobal (cell)),
                        [this, slot] (int result)
    {
        switch (result)
        {
            case 1:  recallSlot (slot); break;
            case 2:  captureSlot (slot); break;
            case 3:  if (onRenameRequested != nullptr) onRenameRequested (slot); break;
            case 4:  clearSlot (slot); break;

            default:
                if (result >= 100 && result < 100 + Snapshot::kNumColourTags)
                    setSlotColourTag (slot, result - 100);
                break;
        }
    });
}

void SnapshotGrid::paint (juce::Graphics& g)
{
    const auto& bank = processor.getSnapshots();
    const int current = bank.getCurrentSnapshot();

    const auto numberFont = Fonts::mono (9.0f);
    const auto labelFont = Fonts::ui (10.0f);
    const auto emptyText = tr ("live.grid.emptyCell");
    const int widestNumber = (int) std::ceil (juce::GlyphArrangement::getStringWidth (
                                 numberFont, juce::String (kColumns * kRows))) + 3;

    for (int slot = 0; slot < kColumns * kRows; ++slot)
    {
        auto cell = boundsForSlot (slot).reduced (1);

        if (cell.isEmpty())
            continue;

        const bool filled = slot < bank.getNumSnapshots() && ! bank.getSnapshot (slot).isEmpty();

        /*  Three states, and they have to be distinguishable without colour -
            accessibility.md 6's colourblind palettes swap the hues out. Filled is
            a solid fill with text on it, selected gets a bright border, and the
            one that is actually loaded gets a dot. */
        g.setColour (filled ? Palette::accentDim.withAlpha (0.55f)
                            : Palette::panelSunken);
        g.fillRoundedRectangle (cell.toFloat(), 2.0f);

        if (filled)
        {
            // The colour tag is a stripe down the left edge, live-performance 1.
            g.setColour (snapshotTagColour (bank.getSnapshot (slot).colourTag));
            g.fillRect (cell.getX() + 1, cell.getY() + 2, 3, cell.getHeight() - 4);
        }

        g.setColour (slot == selected ? Palette::accentBright : Palette::edge);
        g.drawRoundedRectangle (cell.toFloat(), 2.0f, slot == selected ? 1.6f : 0.8f);

        auto text = cell.reduced (6, 0);
        text.removeFromLeft (1);

        // One-based, as the live strip and the snapshot digits number them.
        g.setColour (Palette::textMuted);
        g.setFont (numberFont);
        const auto number = juce::String (slot + 1);

        // Wide enough for the widest number ("128"), so "100" no longer runs
        // into the label beside it.
        const int numberWidth = juce::jmin (text.getWidth(), widestNumber);
        g.drawText (number, text.removeFromLeft (numberWidth), juce::Justification::centredLeft, false);

        if (slot == current && filled)
        {
            g.setColour (Palette::textPrimary);
            g.fillEllipse (text.removeFromRight (8).toFloat().withSizeKeepingCentre (4.0f, 4.0f));
        }

        if (text.getWidth() > 8)
        {
            g.setFont (labelFont);

            if (filled)
            {
                g.setColour (Palette::textPrimary);
                Fonts::drawFittedLabel (g, Fonts::fitLabel (truncatedLabel (bank.getSnapshot (slot).label),
                                                            labelFont, (float) text.getWidth(), 0.0f),
                                        text, juce::Justification::centredLeft);
            }
            else
            {
                g.setColour (Palette::textDisabled);
                Fonts::drawFittedLabel (g, Fonts::fitLabel (emptyText, labelFont, (float) text.getWidth(), 0.0f),
                                        text, juce::Justification::centredLeft);
            }
        }
    }
}

void SnapshotGrid::mouseDown (const juce::MouseEvent& e)
{
    const int slot = slotAt (e.getPosition());

    if (slot < 0)
        return;

    setSelectedSlot (slot);

    if (e.mods.isPopupMenu())
        showSlotMenu (slot);
    else if (e.mods.isShiftDown())
        captureSlot (slot);
}

void SnapshotGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int slot = slotAt (e.getPosition());

    if (slot >= 0 && ! e.mods.isPopupMenu() && ! e.mods.isShiftDown())
        recallSlot (slot);
}

void SnapshotGrid::mouseMove (const juce::MouseEvent& e)
{
    const int slot = slotAt (e.getPosition());

    if (slot < 0)
        return;

    /*  The number is one-based because that is what the live strip and the
        snapshot digits use; the slot index is an implementation detail nobody
        performing should have to translate. The tooltip says what the gestures
        are, because nothing else on the pad does. */
    if (isFilled (slot))
        setTooltip (tr ("live.grid.cell.filled", { { "number", juce::String (slot + 1) },
                                                   { "label", processor.getSnapshots().getSnapshot (slot).label } }));
    else
        setTooltip (tr ("live.grid.cell.empty", { { "number", juce::String (slot + 1) } }));
}

//==============================================================================
//  LivePanel::SetlistModel
//==============================================================================
class LivePanel::SetlistModel final : public juce::ListBoxModel
{
public:
    explicit SetlistModel (LuthierAudioProcessor& p) : processor (p) {}

    int getNumRows() override
    {
        return processor.getSetlist().getSetlist().getNumEntries();
    }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        const auto& set = processor.getSetlist().getSetlist();

        if (! juce::isPositiveAndBelow (row, set.getNumEntries()))
            return;

        if (selected)
        {
            g.setColour (Palette::accent.withAlpha (0.18f));
            g.fillRect (0, 0, width, height);
        }

        const auto& entry = set.getEntry (row);

        // The position in the set is the point of a setlist, so it leads.
        g.setColour (Palette::textMuted);
        g.setFont (Fonts::ui (10.0f));
        g.drawText (juce::String (row + 1), 4, 0, 22, height,
                    juce::Justification::centredLeft, false);

        g.setColour (Palette::textPrimary);
        g.setFont (Fonts::ui (11.0f));
        g.drawText (entry.getDisplayName(), 28, 0, width - 90, height,
                    juce::Justification::centredLeft, true);

        g.setColour (Palette::textDisabled);
        g.setFont (Fonts::ui (10.0f));
        g.drawText ("snap " + juce::String (entry.snapshotIndex + 1),
                    width - 60, 0, 56, height, juce::Justification::centredRight, false);
    }

private:
    LuthierAudioProcessor& processor;
};

//==============================================================================
//  LivePanel
//==============================================================================
LivePanel::LivePanel (LuthierAudioProcessor& p)
    : processor (p), grid (p)
{
    setlistModel = std::make_unique<SetlistModel> (processor);

    styleHeading (bankHeading, "SNAPSHOT BANK");
    styleHeading (setlistHeading, "SETLIST");
    styleHeading (morphHeading, "MORPH AND CROSSFADE");

    addAndMakeVisible (bankHeading);
    addAndMakeVisible (setlistHeading);
    addAndMakeVisible (morphHeading);

    // ---- the bank ------------------------------------------------------------
    addAndMakeVisible (grid);
    grid.onSelectionChanged = [this] { refresh(); };
    grid.onBankChanged = [this] { refresh(); };
    grid.onRenameRequested = [this] (int slot)
    {
        grid.setSelectedSlot (slot);
        renameSlot (slot, grid.localAreaToGlobal (grid.boundsForSlot (slot)));
    };

    styleNote (slotLabel);
    slotLabel.setTooltip (tr ("live.slotLabel.tooltip"));
    addAndMakeVisible (slotLabel);

    styleNote (bankEmptyLabel, Palette::textDisabled);
    bankEmptyLabel.setText (tr ("live.bank.emptyHint"), juce::dontSendNotification);
    bankEmptyLabel.setTooltip (tr ("live.grid.tooltip"));
    addChildComponent (bankEmptyLabel);

    auto addButton = [this] (juce::TextButton& button, const juce::String& tooltip,
                             std::function<void()> action)
    {
        button.setTooltip (tooltip);
        button.onClick = std::move (action);

        AccessibleSetup::configureButton (button, button.getButtonText(), tooltip);
        addAndMakeVisible (button);
    };

    addButton (captureButton, "Store the current sound in the selected slot",
               [this] { captureSelected(); });

    addButton (recallButton, "Load the selected slot, crossfading over the time below",
               [this]
               {
                   processor.recallSnapshot (grid.getSelectedSlot());   // pushes the undo entry (3.7)
                   refresh();
               });

    addButton (renameButton, "Give the selected snapshot a name",
               [this] { renameSelected(); });

    addButton (clearButton, "Empty the selected slot", [this] { clearSelected(); });

    // ---- the setlist ---------------------------------------------------------
    setlistBox.setModel (setlistModel.get());
    setlistBox.setRowHeight (kSetlistRowHeight);
    setlistBox.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);
    setlistBox.setTooltip (tr ("live.setlist.tooltip"));
    addAndMakeVisible (setlistBox);

    styleNote (setlistEmptyLabel, Palette::textDisabled);
    setlistEmptyLabel.setText (tr ("live.setlist.empty"), juce::dontSendNotification);
    setlistEmptyLabel.setTooltip (tr ("live.setlist.empty.tooltip"));
    addAndMakeVisible (setlistEmptyLabel);

    addButton (addToSetlistButton, "Put the selected snapshot at the end of the set",
               [this] { addSelectedToSetlist(); });

    addButton (removeFromSetlistButton, "Take the selected row out of the set",
               [this]
               {
                   auto set = processor.getSetlist().getSetlist();

                   if (set.removeEntry (setlistBox.getSelectedRow()))
                   {
                       processor.getSetlist().setSetlist (set);
                       rebuildSetlistRows();
                   }
               });

    addButton (setlistUpButton, "Move this row one earlier in the set",
               [this]
               {
                   const int row = setlistBox.getSelectedRow();
                   auto set = processor.getSetlist().getSetlist();

                   if (row > 0 && set.moveEntry (row, row - 1))
                   {
                       processor.getSetlist().setSetlist (set);
                       rebuildSetlistRows();
                       setlistBox.selectRow (row - 1);
                   }
               });

    addButton (setlistDownButton, "Move this row one later in the set",
               [this]
               {
                   const int row = setlistBox.getSelectedRow();
                   auto set = processor.getSetlist().getSetlist();

                   if (row >= 0 && row + 1 < set.getNumEntries() && set.moveEntry (row, row + 1))
                   {
                       processor.getSetlist().setSetlist (set);
                       rebuildSetlistRows();
                       setlistBox.selectRow (row + 1);
                   }
               });

    // ---- morph and crossfade -------------------------------------------------
    styleNote (crossfadeLabel);
    crossfadeLabel.setText (tr ("live.crossfade"), juce::dontSendNotification);
    crossfadeLabel.setTooltip (tr ("live.crossfade.tooltip"));
    addAndMakeVisible (crossfadeLabel);

    /*  0 to 500 ms, which is `SnapshotBank::setCrossfadeMs`'s own clamp. A wider
        range would let the slider show a number the engine had silently refused -
        the first version offered five seconds and a test caught the slider
        reading 900 while the bank held 500. */
    crossfade.setRange (0.0, 500.0, 1.0);
    crossfade.setTextValueSuffix (" ms");
    crossfade.setTooltip (tr ("live.crossfade.tooltip"));
    crossfade.onValueChange = [this]
    {
        if (! updatingControls)
            processor.getSnapshots().setCrossfadeMs (crossfade.getValue());
    };
    addAndMakeVisible (crossfade);

    for (int i = 0; i < (int) MorphCurve::numCurves; ++i)
        morphCurveBox.addItem (getMorphCurveName ((MorphCurve) i), i + 1);

    morphCurveBox.setTooltip ("How the morph travels between the two snapshots.");
    morphCurveBox.onChange = [this]
    {
        if (! updatingControls)
            processor.getSnapshots().setMorphCurve ((MorphCurve) (morphCurveBox.getSelectedId() - 1));
    };
    addAndMakeVisible (morphCurveBox);

    morphEnabled.setTooltip ("Blend continuously between two snapshots instead of "
                             "stepping from one to the next.");
    morphEnabled.onClick = [this]
    {
        if (! updatingControls)
            processor.getSnapshots().setMorphEnabled (morphEnabled.getToggleState());

        refresh();
    };
    addAndMakeVisible (morphEnabled);

    styleNote (morphSlotsLabel, Palette::textDisabled);
    morphSlotsLabel.setTooltip (tr ("live.morphSlots.tooltip"));
    addAndMakeVisible (morphSlotsLabel);

    // Column 4 sizes the tab from this; without it the panel sat at 80 points.
    setSize (480, preferredHeight());

    refresh();
    startTimerHz (6);
}

LivePanel::~LivePanel()
{
    stopTimer();
    setlistBox.setModel (nullptr);
}

void LivePanel::timerCallback()
{
    /*  A snapshot can be captured or recalled from the live strip, from a
        setlist step or from a MIDI program change while this panel is on screen,
        so it re-reads rather than assuming it is the only writer. */
    refresh();
}

void LivePanel::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updatingControls, true);

    auto& bank = processor.getSnapshots();

    const int slot = grid.getSelectedSlot();
    const bool filled = slot < bank.getNumSnapshots() && ! bank.getSnapshot (slot).isEmpty();

    slotLabel.setText (filled ? tr ("live.slot.selected", { { "number", juce::String (slot + 1) },
                                                            { "label", bank.getSnapshot (slot).label } })
                              : tr ("live.slot.selectedEmpty", { { "number", juce::String (slot + 1) } }),
                       juce::dontSendNotification);

    /*  gui-integration 14's empty state: shown while no slot holds anything,
        which is exactly when a player does not yet know what the pads are for. */
    bool anyFilled = false;

    for (int i = 0; i < bank.getNumSnapshots() && ! anyFilled; ++i)
        anyFilled = ! bank.getSnapshot (i).isEmpty();

    bankEmptyLabel.setVisible (! anyFilled);

    /*  Recall, rename and clear are meaningless on an empty slot, and a button
        that does nothing is worse than one that is plainly unavailable. */
    recallButton.setEnabled (filled);
    renameButton.setEnabled (filled);
    clearButton.setEnabled (filled);
    addToSetlistButton.setEnabled (filled);

    crossfade.setValue (bank.getCrossfadeMs(), juce::dontSendNotification);
    morphCurveBox.setSelectedId ((int) bank.getMorphCurve() + 1, juce::dontSendNotification);
    morphEnabled.setToggleState (bank.isMorphEnabled(), juce::dontSendNotification);

    morphCurveBox.setEnabled (bank.isMorphEnabled());

    morphSlotsLabel.setText (bank.isMorphEnabled()
                               ? "Morphing between snapshots "
                                   + juce::String (bank.getMorphSlotA() + 1) + " and "
                                   + juce::String (bank.getMorphSlotB() + 1)
                                   + ". The live strip drives the position."
                               : "Off: a recall steps to the new sound over the crossfade time.",
                             juce::dontSendNotification);

    const int rows = processor.getSetlist().getSetlist().getNumEntries();

    setlistEmptyLabel.setVisible (rows == 0);
    setlistBox.setVisible (rows > 0);

    removeFromSetlistButton.setEnabled (setlistBox.getSelectedRow() >= 0);
    setlistUpButton.setEnabled (setlistBox.getSelectedRow() > 0);
    setlistDownButton.setEnabled (setlistBox.getSelectedRow() >= 0
                                    && setlistBox.getSelectedRow() + 1 < rows);

    grid.repaint();
}

void LivePanel::rebuildSetlistRows()
{
    setlistBox.updateContent();
    setlistBox.repaint();
    refresh();
}

//==============================================================================
void LivePanel::captureSelected()
{
    // The grid's capture keeps the slot's label and colour; the button is the
    // same gesture as Shift-click on the pad.
    grid.captureSlot (grid.getSelectedSlot());
    refresh();
}

void LivePanel::clearSelected()
{
    grid.clearSlot (grid.getSelectedSlot());
    refresh();
}

void LivePanel::renameSelected()
{
    renameSlot (grid.getSelectedSlot(), renameButton.getScreenBounds());
}

void LivePanel::renameSlot (int slot, juce::Rectangle<int> screenAnchor)
{
    auto& bank = processor.getSnapshots();

    if (slot < 0 || slot >= bank.getNumSnapshots() || bank.getSnapshot (slot).isEmpty())
        return;

    /*  An inline callout rather than a modal dialog: modal loops are disabled in
        the plugin build, which is the same reason the right-click menu's value
        entry uses one. */
    auto* editor = new juce::TextEditor();
    editor->setSize (180, 24);
    editor->setText (bank.getSnapshot (slot).label, false);
    editor->selectAll();
    editor->setColour (juce::TextEditor::backgroundColourId, Palette::panelSunken);

    auto& box = juce::CallOutBox::launchAsynchronously (
        std::unique_ptr<juce::Component> (editor), screenAnchor, nullptr);

    editor->onReturnKey = [this, editor, slot, &box]
    {
        // 3.7 snapshot-rename: one entry per committed name (Return), not per keystroke.
        processor.pushUndoState ("Rename snapshot " + juce::String (slot + 1) + " to " + editor->getText());
        processor.getSnapshots().setLabel (slot, editor->getText());
        refresh();
        box.dismiss();
    };

    editor->onEscapeKey = [&box] { box.dismiss(); };
    editor->grabKeyboardFocus();
}

void LivePanel::addSelectedToSetlist()
{
    SetlistEntry entry;
    entry.snapshotIndex = grid.getSelectedSlot();

    /*  The preset path is what a setlist entry recalls first; an entry added from
        here inherits the preset that is loaded now, which is the one the snapshot
        was captured against. */
    entry.presetPath = processor.getPresetManager().getCurrentPresetFile().getFullPathName();

    auto set = processor.getSetlist().getSetlist();
    set.addEntry (entry);

    processor.getSetlist().setSetlist (set);
    rebuildSetlistRows();
}

//==============================================================================
void LivePanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::panel);
}

int LivePanel::preferredHeight() const
{
    const int width = (getWidth() > 0 ? getWidth() : 480) - kPad * 2;

    return kPad
         + kHeadingHeight + SnapshotGrid::preferredHeightFor (width)          // bank
         + 4 + kSlotLabelHeight + kHintHeight + kRowHeight
         + kPad
         + kHeadingHeight + kRowHeight * 3 + kSlotLabelHeight                 // morph
         + kPad
         + kHeadingHeight + kSetlistRows * kSetlistRowHeight + 4 + kRowHeight // setlist
         + kPad;
}

void LivePanel::resized()
{
    /*  The grid's cells scale with the width, so the height the panel wants is
        not known until the width is. Column 4 sets both at once from the old
        width; when they disagree, ask for the right height and lay out again.
        This converges in one step because the height depends only on width. */
    if (getWidth() > 0 && getHeight() != preferredHeight())
    {
        setSize (getWidth(), preferredHeight());
        return;
    }

    auto bounds = getLocalBounds().reduced (kPad);

    // ---- bank ----------------------------------------------------------------
    bankHeading.setBounds (bounds.removeFromTop (kHeadingHeight));

    /*  The grid keeps a flat cell rather than filling whatever is left: 128
        pads stretched to a tall column stop reading as a bank of pads. */
    grid.setBounds (bounds.removeFromTop (SnapshotGrid::preferredHeightFor (bounds.getWidth())));

    bounds.removeFromTop (4);
    slotLabel.setBounds (bounds.removeFromTop (kSlotLabelHeight));
    bankEmptyLabel.setBounds (bounds.removeFromTop (kHintHeight));

    auto buttonRow = bounds.removeFromTop (kRowHeight);
    const int buttonWidth = juce::jmax (52, buttonRow.getWidth() / 4 - 4);

    captureButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
    buttonRow.removeFromLeft (4);
    recallButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
    buttonRow.removeFromLeft (4);
    renameButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
    buttonRow.removeFromLeft (4);
    clearButton.setBounds (buttonRow.removeFromLeft (buttonWidth));

    bounds.removeFromTop (kPad);

    // ---- morph, which is short, so it goes before the list that stretches -----
    morphHeading.setBounds (bounds.removeFromTop (kHeadingHeight));

    auto crossfadeRow = bounds.removeFromTop (kRowHeight);
    crossfadeLabel.setBounds (crossfadeRow.removeFromLeft (70));
    crossfade.setBounds (crossfadeRow);

    morphEnabled.setBounds (bounds.removeFromTop (kRowHeight));

    auto curveRow = bounds.removeFromTop (kRowHeight);
    morphCurveBox.setBounds (curveRow.removeFromLeft (juce::jmin (180, curveRow.getWidth())));

    morphSlotsLabel.setBounds (bounds.removeFromTop (kSlotLabelHeight));

    bounds.removeFromTop (kPad);

    // ---- setlist: six rows, then its buttons ----------------------------------
    setlistHeading.setBounds (bounds.removeFromTop (kHeadingHeight));

    bounds = bounds.removeFromTop (kSetlistRows * kSetlistRowHeight + 4 + kRowHeight);

    auto setlistButtons = bounds.removeFromBottom (kRowHeight);
    const int setlistButtonWidth = juce::jmax (56, setlistButtons.getWidth() / 4 - 4);

    addToSetlistButton.setBounds (setlistButtons.removeFromLeft (setlistButtonWidth + 30));
    setlistButtons.removeFromLeft (4);
    removeFromSetlistButton.setBounds (setlistButtons.removeFromLeft (setlistButtonWidth));
    setlistButtons.removeFromLeft (4);
    setlistUpButton.setBounds (setlistButtons.removeFromLeft (44));
    setlistButtons.removeFromLeft (4);
    setlistDownButton.setBounds (setlistButtons.removeFromLeft (44));

    bounds.removeFromBottom (4);

    setlistBox.setBounds (bounds);
    setlistEmptyLabel.setBounds (bounds.removeFromTop (20));
}

} // namespace luthier
