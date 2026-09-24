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
                      public juce::SettableTooltipClient,
                      private juce::Timer
{
public:
    explicit ModSourceCard (LuthierAudioProcessor& processor);
    ~ModSourceCard() override;

    void setSlot (int slot);
    int getSlot() const noexcept { return slot; }

    void paint (juce::Graphics&) override;
    void resized() override;

    /** The header, the scope, and the tallest family's seven rows. */
    static constexpr int headerHeight = 16;
    static constexpr int scopeHeight = 34;
    static constexpr int rowHeight = 22;
    static constexpr int maxRows = 7;
    static constexpr int preferredHeight = headerHeight + scopeHeight + 2 + rowHeight * maxRows;

private:
    void timerCallback() override;
    void rebuildControls();
    void pushToSource();

    LuthierAudioProcessor& processor;
    int slot = ModSourceSlots::lfoBase;

    juce::ComboBox shapeBox, divisionBox, retriggerBox, directionBox, detectionBox, followerSourceBox;
    WheelPassSlider rateSlider, depthSlider, symmetrySlider, smoothingSlider;
    WheelPassSlider delaySlider, attackSlider, holdSlider, decaySlider, sustainSlider, releaseSlider;
    WheelPassSlider lengthSlider, swingSlider;
    WheelPassSlider followerAttackSlider, followerReleaseSlider, thresholdSlider;
    juce::ToggleButton syncButton { "Sync" }, bipolarButton { "Bipolar" };

    /** One short label per slider, shown and placed with it. */
    juce::OwnedArray<juce::Label> labels;
    std::vector<std::pair<juce::Slider*, juce::Label*>> labelled;

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

    /** What clicking the cell does: depth is typed, curve cycles, On toggles,
        x removes. Nothing on the cell itself says so. */
    juce::String getCellTooltip (int row, int columnId) override;

    void resized() override;

    /** gui-integration 14: the empty state, over the table when it has no rows. */
    void paintOverChildren (juce::Graphics&) override;

    std::function<void()> onRoutesChanged;

private:
    enum ColumnId { source = 1, destination, depth, curve, enabled, remove };

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
