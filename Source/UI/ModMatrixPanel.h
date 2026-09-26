#pragma once

/*  The MOD panel (modulation-matrix.md section 5).

    The spec draws this as a left third of source cards and a right two thirds of
    routing table. Column 4 is a narrow scrolling column rather than a wide
    canvas, so the two parts are stacked instead of placed side by side: one
    source card at a time, chosen from a selector, above the routing table. The
    content is the same, and the editing is the same; only the axis differs.

    Routes are created from three places, and all three end up calling
    ModMatrix::addRoute: the ADD button here, the "Modulate" submenu on any
    control's right-click menu, and dragging a source card onto a control.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Widgets.h"
#include "../Modulation/ModMatrix.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** The compact editor for whichever source is selected. Which controls are
    shown depends on what kind of source it is; a CC source has nothing to edit,
    so it shows its live value instead. */
class ModSourceCard : public juce::Component,
                      private juce::Timer
{
public:
    explicit ModSourceCard (LuthierAudioProcessor& processor);
    ~ModSourceCard() override;

    void setSlot (int slot);
    int getSlot() const noexcept { return slot; }

    void paint (juce::Graphics&) override;
    void resized() override;

    // SPEC-SWEEP: room for the envelope's eight rows (MM-18/19/20).
    static constexpr int preferredHeight = 200;

    /** SPEC-SWEEP: the controls the source cards gained, for the tests. */
    struct SweepControls
    {
        juce::Slider* phase; juce::Slider* seqRate;
        juce::ComboBox* envRetrigger; juce::ComboBox* loopMode;
        juce::ComboBox* attackCurve; juce::ComboBox* decayCurve; juce::ComboBox* releaseCurve;
        juce::ComboBox* followerString; juce::ToggleButton* followerLog;
        juce::Slider* rate; juce::Slider* attack; juce::Slider* followerRelease;
    };

    SweepControls getSweepControls() noexcept
    {
        return { &phaseSlider, &seqRateSlider, &envRetriggerBox, &loopModeBox,
                 &attackCurveBox, &decayCurveBox, &releaseCurveBox,
                 &followerStringBox, &followerLogButton,
                 &rateSlider, &attackSlider, &followerReleaseSlider };
    }

private:
    void timerCallback() override;
    void rebuildControls();
    void pushToSource();
    void applyRangeLimits();   // SPEC-SWEEP: PR-44

    LuthierAudioProcessor& processor;
    int slot = ModSourceSlots::lfoBase;

    juce::ComboBox shapeBox, divisionBox, retriggerBox, directionBox, detectionBox, followerSourceBox;
    juce::Slider rateSlider, depthSlider, symmetrySlider, smoothingSlider;
    juce::Slider delaySlider, attackSlider, holdSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Slider lengthSlider, swingSlider;
    juce::Slider followerAttackSlider, followerReleaseSlider, thresholdSlider;
    juce::ToggleButton syncButton { "Sync" }, bipolarButton { "Bipolar" };

    // SPEC-SWEEP: MM-14 (LFO phase), MM-18/19/20 (envelope curves, retrigger,
    // loop), MM-23 (free sequencer rate), MM-25 (follower string and log).
    juce::Slider phaseSlider, seqRateSlider;
    juce::ComboBox envRetriggerBox, loopModeBox, attackCurveBox, decayCurveBox, releaseCurveBox;
    juce::ComboBox followerStringBox;
    juce::ToggleButton followerLogButton { "Log" };
    bool shownRangeAdvanced = false;   // PR-44: the family state the slider ranges were set for

    juce::OwnedArray<juce::Label> labels;

    float liveValue = 0.0f;
    bool updating = false;

    juce::Rectangle<int> scopeBounds;
    std::array<float, 96> history {};
    int historyWrite = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModSourceCard)
};

//==============================================================================
/** The routing table: one row per route, with depth, curve, enable and delete. */
class ModRouteTable : public juce::Component,
                      public juce::TableListBoxModel
{
public:
    explicit ModRouteTable (LuthierAudioProcessor& processor);
    ~ModRouteTable() override;

    void refresh();

    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int row, int width, int height, bool selected) override;
    void paintCell (juce::Graphics&, int row, int columnId, int width, int height, bool selected) override;
    void cellClicked (int row, int columnId, const juce::MouseEvent&) override;

    void resized() override;

    std::function<void()> onRoutesChanged;

private:
    enum ColumnId { source = 1, destination, depth, curve, enabled, remove,
                    offset };   // SPEC-SWEEP: MM-40

    /** Types a depth or offset for a row into a call-out editor. */
    void editValue (int row, bool isOffset, const juce::MouseEvent& e);

public:
    /** SPEC-SWEEP: MM-40 - the value a typed edit writes, without the call-out. */
    void applyTypedValue (int row, bool isOffset, float value);

private:

    LuthierAudioProcessor& processor;
    juce::TableListBox table;
    std::vector<ModRoute> cached;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModRouteTable)
};

//==============================================================================
class ModMatrixPanel : public juce::Component
{
public:
    explicit ModMatrixPanel (LuthierAudioProcessor& processor);
    ~ModMatrixPanel() override;

    int preferredHeight() const;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showAddRouteMenu();

    LuthierAudioProcessor& processor;

    juce::ComboBox sourceSelector;
    std::unique_ptr<ModSourceCard> card;
    std::unique_ptr<ModRouteTable> routeTable;

    juce::TextButton addButton { "ADD ROUTE" };
    juce::TextButton clearButton { "CLEAR ALL" };
    juce::Label summaryLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModMatrixPanel)
};

} // namespace luthier
