#pragma once

/*  The LIVE tab of Advanced column 4, gui-integration.md section 4.4.

    "Snapshot bank editor, setlist editor, morph configuration, expression-pedal
    calibration. The live-strip in the bottom of the window is the runtime
    surface; this tab is the setup surface."

    That sentence is the whole design. `LiveStrip` is for a player mid-set who
    needs one thing to be one press away; this is for the hour beforehand, when
    the question is which 128 snapshots exist and what order they come in. Both
    read the same `SnapshotBank` and `SetlistPlayer`, so nothing is mirrored and
    nothing can disagree.

    **Expression-pedal calibration is not here**, and that is deliberate rather
    than missed. It already exists as the Options EXPRESSION page, working and
    reachable, and the last time this build had one feature in two places - the
    CONTROLLERS page, which owns a `ControllerProfileLibrary` by value - the two
    copies would have scanned the same folder separately and gone stale against
    each other. `ExpressionCalibrationSet` is a user-global singleton with a file
    behind it, so a second editor is a second writer to that file. Section 4.4
    lists it here and section 5 does not list it in Options; that conflict is
    recorded in GAPS.md rather than resolved by building the page twice.

    The snapshot grid is 128 cells and deliberately not a list. A list of 128
    rows is a scroll, and the thing a player wants to know at a glance is which
    of their pads are filled - which is a shape, not a sequence.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Widgets.h"
#include "../Live/Setlist.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/*  The 128-cell snapshot bank. One cell per slot: its number, its label, its
    colour tag down the left edge, a dot on the one that is loaded and a bright
    border on the one that is selected.

    gui-integration.md 8 and 4.4 fix the gestures, and they are the live strip's:
    click selects, double-click recalls, Shift-click captures into the slot,
    right-click opens the menu. Eight columns rather than sixteen, because at
    column 4's width sixteen columns made a cell 28 points wide, too narrow for
    a number let alone a name. */
class SnapshotGrid final : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    explicit SnapshotGrid (LuthierAudioProcessor& processor);

    static constexpr int kColumns = 8;
    static constexpr int kRows = 16;

    /** The cell height the grid wants for a given width: about a third of the
        cell width, between 20 and 28 points, so a cell is a readable pad. */
    static int cellHeightFor (int width) noexcept;
    static int preferredHeightFor (int width) noexcept { return kRows * cellHeightFor (width); }

    /** Which slot is under this point, or -1. */
    int slotAt (juce::Point<int> position) const;

    /** The cell for a slot, in this component's coordinates. Empty if out of range. */
    juce::Rectangle<int> boundsForSlot (int slot) const;

    /** The slot the user last clicked, which the buttons below act on. */
    int getSelectedSlot() const noexcept { return selected; }
    void setSelectedSlot (int slot);

    /** The gestures, each acting on the bank directly. Public so the panel's
        buttons and the tests share one implementation with the mouse. */
    void captureSlot (int slot);
    void recallSlot (int slot);
    void clearSlot (int slot);
    void setSlotColourTag (int slot, int tag);

    /** Opens the right-click menu for a slot. */
    void showSlotMenu (int slot);

    std::function<void()> onSelectionChanged;

    /** Fired after any gesture edited the bank, so the panel re-reads it. */
    std::function<void()> onBankChanged;

    /** Rename is the panel's, because it owns the inline editor. */
    std::function<void (int slot)> onRenameRequested;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    bool isFilled (int slot) const;
    void bankChanged();

    LuthierAudioProcessor& processor;
    int selected = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnapshotGrid)
};

//==============================================================================
/** Advanced column 4 -> LIVE. */
class LivePanel final : public juce::Component,
                        private juce::Timer
{
public:
    explicit LivePanel (LuthierAudioProcessor& processor);
    ~LivePanel() override;

    /** Re-reads the bank and the setlist. Called on a timer and after any edit,
        because a snapshot can be captured from the live strip while this is on
        screen. */
    void refresh();

    /** The height the panel's content wants at its current width. The grid's
        cells scale with the width, so this is not a constant. */
    int preferredHeight() const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void captureSelected();
    void clearSelected();
    void renameSelected();
    void renameSlot (int slot, juce::Rectangle<int> screenAnchor);
    void addSelectedToSetlist();

    void rebuildSetlistRows();

    LuthierAudioProcessor& processor;

    juce::Label bankHeading, setlistHeading, morphHeading;

    SnapshotGrid grid;

    juce::Label slotLabel;

    /** gui-integration 14: the empty-slot hint, shown while the bank has nothing
        in it. */
    juce::Label bankEmptyLabel;
    juce::TextButton captureButton { "Capture" };
    juce::TextButton recallButton  { "Recall" };
    juce::TextButton renameButton  { "Rename..." };
    juce::TextButton clearButton   { "Clear" };

    /*  The setlist is a list because it *is* a sequence - the order is the whole
        content - which is exactly why the bank above it is not one. */
    juce::ListBox setlistBox;
    juce::TextButton addToSetlistButton { "Add snapshot" };
    juce::TextButton removeFromSetlistButton { "Remove" };
    juce::TextButton setlistUpButton { "Up" };
    juce::TextButton setlistDownButton { "Down" };
    juce::Label setlistEmptyLabel;

    /*  A plain slider rather than a LuthierKnob: the crossfade time is
        SnapshotBank state, not a parameter, so there is nothing to attach to and
        no automation or MIDI Learn on it. live-performance.md keeps it out of the
        parameter list deliberately - it describes how the plugin moves between
        sounds rather than being part of one. */
    WheelPassSlider crossfade { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label crossfadeLabel;

    juce::ComboBox morphCurveBox;
    juce::ToggleButton morphEnabled { "Morph between two snapshots" };
    juce::Label morphSlotsLabel;

    std::unique_ptr<juce::AlertWindow> renameWindow;

    class SetlistModel;
    std::unique_ptr<SetlistModel> setlistModel;

    bool updatingControls = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LivePanel)
};

} // namespace luthier
