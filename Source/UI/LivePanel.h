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
#include "LiveSetup.h"   // SPEC-SWEEP: LP-16 / LP-18 / LP-19 / LP-31 / GI-119

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** The 128-cell snapshot bank. One cell per slot, filled ones tinted. */
class SnapshotGrid final : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    explicit SnapshotGrid (LuthierAudioProcessor& processor);

    static constexpr int kColumns = 16;
    static constexpr int kRows = 8;

    /** Which slot is under this point, or -1. */
    int slotAt (juce::Point<int> position) const;

    /** The slot the user last clicked, which the buttons below act on. */
    int getSelectedSlot() const noexcept { return selected; }
    void setSelectedSlot (int slot);

    std::function<void()> onSelectionChanged;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> boundsForSlot (int slot) const;

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

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void captureSelected();
    void clearSelected();
    void renameSelected();
    void addSelectedToSetlist();

    void rebuildSetlistRows();

    LuthierAudioProcessor& processor;

    juce::Label bankHeading, setlistHeading, morphHeading;

    SnapshotGrid grid;

    juce::Label slotLabel;
    juce::TextButton captureButton { "Capture" };
    juce::TextButton recallButton  { "Recall" };
    juce::TextButton renameButton  { "Rename..." };
    juce::TextButton clearButton   { "Clear" };
    juce::TextButton colourButton  { "Colour" };   // SPEC-SWEEP: GI-4

public:
    /** SPEC-SWEEP: GI-4 - the colour-tag menu, exposed for tests: item id =
        1 + tag. */
    juce::PopupMenu buildColourMenu() const;
    void applyColourMenuResult (int result);
    juce::TextButton& getColourButton() noexcept { return colourButton; }

private:

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
    juce::Slider crossfade { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label crossfadeLabel;

    juce::ComboBox morphCurveBox;
    juce::ToggleButton morphEnabled { "Morph between two snapshots" };
    juce::Label morphSlotsLabel;

    // SPEC-SWEEP: LP-16 / LP-18 / LP-19 / LP-11 and LP-31 / GI-119.
    MorphSetupPanel morphSetup;
    juce::Label monitorHeading;
    MonitorSetupPanel monitorSetup;

public:
    MorphSetupPanel& getMorphSetup() noexcept { return morphSetup; }
    MonitorSetupPanel& getMonitorSetup() noexcept { return monitorSetup; }

private:

    std::unique_ptr<juce::AlertWindow> renameWindow;

    class SetlistModel;
    std::unique_ptr<SetlistModel> setlistModel;

    bool updatingControls = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LivePanel)
};

} // namespace luthier
