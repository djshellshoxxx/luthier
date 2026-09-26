#include "LivePanel.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

namespace
{
    constexpr int kPad = 10;
    constexpr int kRowHeight = 26;
    constexpr int kHeadingHeight = 20;

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
    setTooltip ("The snapshot bank. Click a pad to select it; filled pads are lit.");

    AccessibleSetup::configureDescriptive (*this, "Snapshot bank",
                                           "A grid of 128 snapshot slots. Click one to select it.");
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

void SnapshotGrid::paint (juce::Graphics& g)
{
    const auto& bank = processor.getSnapshots();
    const int current = bank.getCurrentSnapshot();

    for (int slot = 0; slot < kColumns * kRows; ++slot)
    {
        auto cell = boundsForSlot (slot).reduced (1);

        if (cell.isEmpty())
            continue;

        const bool filled = slot < bank.getNumSnapshots()
                              && ! bank.getSnapshot (slot).isEmpty();

        /*  Three states, and they have to be distinguishable without colour -
            accessibility.md 6's colourblind palettes swap the hues out. Filled is
            a solid fill, selected gets a bright border, and the one that is
            actually loaded gets a dot. */
        g.setColour (filled ? Palette::accentDim.withAlpha (0.55f)
                            : Palette::panelSunken);
        g.fillRoundedRectangle (cell.toFloat(), 2.0f);

        g.setColour (slot == selected ? Palette::accentBright : Palette::edge);
        g.drawRoundedRectangle (cell.toFloat(), 2.0f, slot == selected ? 1.6f : 0.8f);

        if (slot == current && filled)
        {
            g.setColour (Palette::textPrimary);
            g.fillEllipse (cell.toFloat().withSizeKeepingCentre (4.0f, 4.0f));
        }
    }
}

void SnapshotGrid::mouseDown (const juce::MouseEvent& e)
{
    const int slot = slotAt (e.getPosition());

    if (slot >= 0)
        setSelectedSlot (slot);
}

void SnapshotGrid::mouseMove (const juce::MouseEvent& e)
{
    const int slot = slotAt (e.getPosition());

    if (slot < 0)
        return;

    const auto& bank = processor.getSnapshots();

    const bool filled = slot < bank.getNumSnapshots() && ! bank.getSnapshot (slot).isEmpty();

    /*  The number is one-based because that is what the live strip and the
        snapshot digits use; the slot index is an implementation detail nobody
        performing should have to translate. */
    setTooltip ("Snapshot " + juce::String (slot + 1)
                  + (filled ? ": " + bank.getSnapshot (slot).label : " (empty)"));
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

    styleNote (slotLabel);
    addAndMakeVisible (slotLabel);

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
                   processor.recallSnapshotAsUserAction (grid.getSelectedSlot());   // action-and-undo.md 3.7
                   refresh();
               });

    addButton (renameButton, "Give the selected snapshot a name",
               [this] { renameSelected(); });

    addButton (clearButton, "Empty the selected slot", [this] { clearSelected(); });

    // ---- the setlist ---------------------------------------------------------
    setlistBox.setModel (setlistModel.get());
    setlistBox.setRowHeight (22);
    setlistBox.setColour (juce::ListBox::backgroundColourId, Palette::panelSunken);
    addAndMakeVisible (setlistBox);

    styleNote (setlistEmptyLabel, Palette::textDisabled);
    setlistEmptyLabel.setText ("No setlist loaded. Add a snapshot to start one.",
                               juce::dontSendNotification);
    addAndMakeVisible (setlistEmptyLabel);

    addButton (addToSetlistButton, "Put the selected snapshot at the end of the set",
               [this] { addSelectedToSetlist(); });

    addButton (removeFromSetlistButton, "Take the selected row out of the set",
               [this]
               {
                   auto set = processor.getSetlist().getSetlist();

                   if (set.removeEntry (setlistBox.getSelectedRow()))
                   {
                       processor.pushUndoState ("Remove setlist entry");   // action-and-undo.md 3.10
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
                       processor.pushUndoState ("Move setlist entry");   // action-and-undo.md 3.10
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
                       processor.pushUndoState ("Move setlist entry");   // action-and-undo.md 3.10
                       processor.getSetlist().setSetlist (set);
                       rebuildSetlistRows();
                       setlistBox.selectRow (row + 1);
                   }
               });

    // ---- morph and crossfade -------------------------------------------------
    styleNote (crossfadeLabel);
    crossfadeLabel.setText ("Crossfade", juce::dontSendNotification);
    addAndMakeVisible (crossfadeLabel);

    /*  0 to 500 ms, which is `SnapshotBank::setCrossfadeMs`'s own clamp. A wider
        range would let the slider show a number the engine had silently refused -
        the first version offered five seconds and a test caught the slider
        reading 900 while the bank held 500. */
    crossfade.setRange (0.0, 500.0, 1.0);
    crossfade.setTextValueSuffix (" ms");
    crossfade.setTooltip ("How long a recall takes to arrive. Zero is an instant switch.");
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
    addAndMakeVisible (morphSlotsLabel);

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

    slotLabel.setText ("Slot " + juce::String (slot + 1)
                         + (filled ? ": " + bank.getSnapshot (slot).label
                                   : juce::String (" - empty")),
                       juce::dontSendNotification);

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
    const int slot = grid.getSelectedSlot();

    /*  A capture over a filled slot keeps the existing label rather than
        silently replacing it with nothing - the slot keeps its name and gets a
        new sound, which is what re-capturing a pad means to a player. The
        capture is one undo entry (action-and-undo.md 3.7). */
    const auto& bank = processor.getSnapshots();

    const juce::String existing = (slot < bank.getNumSnapshots() && ! bank.getSnapshot (slot).isEmpty())
                                    ? bank.getSnapshot (slot).label
                                    : juce::String();

    processor.captureSnapshotAsUserAction (slot, existing);
    refresh();
}

void LivePanel::clearSelected()
{
    processor.deleteSnapshotAsUserAction (grid.getSelectedSlot(), true);   // action-and-undo.md 3.7
    refresh();
}

void LivePanel::renameSelected()
{
    const int slot = grid.getSelectedSlot();

    auto& bank = processor.getSnapshots();

    if (slot >= bank.getNumSnapshots() || bank.getSnapshot (slot).isEmpty())
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
        std::unique_ptr<juce::Component> (editor), renameButton.getScreenBounds(), nullptr);

    editor->onReturnKey = [this, editor, slot, &box]
    {
        processor.renameSnapshotAsUserAction (slot, editor->getText());   // action-and-undo.md 3.7
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

    processor.pushUndoState ("Add setlist entry");   // action-and-undo.md 3.10
    processor.getSetlist().setSetlist (set);
    rebuildSetlistRows();
}

//==============================================================================
void LivePanel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::panel);
}

void LivePanel::resized()
{
    auto bounds = getLocalBounds().reduced (kPad);

    // ---- bank ----------------------------------------------------------------
    bankHeading.setBounds (bounds.removeFromTop (kHeadingHeight));

    /*  The grid keeps its 2:1 cell aspect rather than filling whatever is left:
        128 pads stretched to a tall column stop reading as a bank of pads. */
    const int gridHeight = juce::jlimit (80, 200, bounds.getWidth() / SnapshotGrid::kColumns * 2);
    grid.setBounds (bounds.removeFromTop (gridHeight));

    bounds.removeFromTop (4);
    slotLabel.setBounds (bounds.removeFromTop (18));

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

    morphSlotsLabel.setBounds (bounds.removeFromTop (18));

    bounds.removeFromTop (kPad);

    // ---- setlist takes what is left ------------------------------------------
    setlistHeading.setBounds (bounds.removeFromTop (kHeadingHeight));

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
