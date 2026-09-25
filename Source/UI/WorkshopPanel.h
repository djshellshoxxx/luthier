#pragma once

/*  The Workshop bench (workshop-ui.md, gui-integration.md 6).

    +---------------------------------------------------------------+
    | WORKSHOP  [guitar name (modified)]  [Save As Guitar]  [A - H] |
    +-----------------------------------------------------+---------+
    |         GUITAR ILLUSTRATION (interactive)           |INSPECTOR|
    |   [ruler: mm from saddle, pickup rail]              |         |
    +-----------------------------------------------------+         |
    | PARTS DRAWER: Body | Neck | ... | Pick | Slide | Capo |         |
    +-----------------------------------------------------+         |
    | SETUP STRIP                                |  SPECTRUM DELTA  |
    +---------------------------------------------------------------+

    Everything it changes goes through WorkshopBench, so every commit is one
    undo entry in real units and audition never commits. The same panel is the
    Advanced column-4 WORKSHOP tab and the Easy-mode overlay.
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "Widgets.h"
#include "Overlays.h"
#include "FirstEncounterHint.h"
#include "Guitar/GuitarRenderer.h"
#include "../Workshop/SpectrumDelta.h"
#include "../Workshop/WorkshopBench.h"
#include "../PhysicalRange.h"

namespace luthier
{

class LuthierAudioProcessor;
class WorkshopBench;

//==============================================================================
/** The illustration on the bench: hit-tested parts, drags, ruler, zoom (workshop-ui.md 2-4). */
class BenchIllustration : public juce::Component,
                          private juce::Timer
{
public:
    explicit BenchIllustration (LuthierAudioProcessor& processor);
    ~BenchIllustration() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    GuitarRegion getSelected() const noexcept { return selected; }
    int getSelectedString() const noexcept { return selectedString; }
    GuitarRegion getHovered() const noexcept { return hovered; }

    /** Selects a region as a click would (keyboard, tests). */
    void select (GuitarRegion region, int stringIndex = -1);

    /*  The player's accessories drawn over the guitar (workshop-ui.md 2, 4;
        guitar-illustration.md 5's layers 28 and 29): the pick at its position
        and angle, the slide bar at its fret and slant (Slide Mode on), the
        capo at its fret - parked on the headstock when it is off. They sit
        above every part, so a click on one takes it before the part under it. */
    enum class Tool { none, pick, slide, capo };

    Tool getSelectedTool() const noexcept { return selectedTool; }
    void selectTool (Tool tool);

    /** Whether a tool is on the bench right now (the slide needs Slide Mode). */
    bool hasTool (Tool tool) const;

    /** The tool's outline in millimetres, for hit-testing and the tests. */
    juce::Path toolArea (Tool tool) const;

    /** The pick's point as drawn, mm: tests aim a drag at it. */
    juce::Point<float> pickTipMm() const;

    /** The nut slot the illustration would drag at a point, or -1. */
    int nutSlotAt (juce::Point<float> px) const;

    /** The builder's order (section 10): Tab walks it. */
    static const std::vector<GuitarRegion>& builderOrder();

    /** Called on selection, and on every live drag step (pickup index, mm; -1 when none). */
    std::function<void()> onSelectionChanged;
    std::function<void (int pickupIndex, double positionMm)> onPickupDragged;
    std::function<void (const juce::String& message)> onLimit;

    /** Rebuilds from the bench now (after a commit, an undo, a recall). */
    void refresh();

    /** Screen <-> millimetres, for tests that drive a drag. */
    juce::Point<float> toMm (juce::Point<float> px) const;
    juce::Point<float> toPx (juce::Point<float> mm) const;

    const GuitarScene& getScene() const noexcept { return scene; }

    float getZoom() const noexcept { return zoom; }

    /** Section 16's sentence for the selected part, string or tool. */
    juce::String describeSelection() const;

private:
    void timerCallback() override;
    GuitarRegion regionAt (juce::Point<float> px, int* stringIndex = nullptr) const;
    Tool toolAt (juce::Point<float> px) const;
    void rebuild (bool force);
    int pickupIndexFor (GuitarRegion) const;
    void paintTools (juce::Graphics&) const;
    void paintStringOverrides (juce::Graphics&) const;
    void announceSelection();
    juce::String toolDescription (Tool tool) const;
    bool updateLiveOverlay();

    LuthierAudioProcessor& processor;
    WorkshopBench& bench;

    GuitarScene scene;
    juce::AffineTransform mmToPx;
    juce::int64 shownKey = 0;
    bool shownAudition = false;

    float zoom = 1.0f;
    juce::Point<float> panPx;   ///< view offset after zoom, pixels

    GuitarRegion hovered = GuitarRegion::none, selected = GuitarRegion::none;
    int selectedString = -1;
    Tool selectedTool = Tool::none, hoveredTool = Tool::none;

    /** What is sounding (gui-engine-dataflow.md 6), painted as the live layer. */
    GuitarOverlay live;

    // Drag state
    enum class Drag { none, pickup, saddle, nutSlot, pick, pickAngle, slide, slideSlant, capo, pan };
    Drag drag = Drag::none;
    int dragIndex = -1;
    juce::Point<float> dragStartMm;
    double dragStartValue = 0.0, dragStartValue2 = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BenchIllustration)
};

//==============================================================================
class WorkshopPanel : public juce::Component,
                      private juce::Timer
{
public:
    explicit WorkshopPanel (LuthierAudioProcessor& processor);
    ~WorkshopPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Opens the Save As Guitar dialog (the editor provides it). */
    std::function<void()> onSaveAsGuitar;

    //==========================================================================
    // For tests and keyboard parity.
    BenchIllustration& getIllustration() noexcept { return illustration; }

    /** The drawer's categories, in section 1's order. */
    static juce::StringArray drawerCategories();
    void showCategory (const juce::String& name);
    juce::String getCategory() const { return category; }

    /** The parts the drawer shows for the current category. */
    const juce::Array<PartPtr>& getDrawerParts() const noexcept { return drawerParts; }

    /** A card clicked (fits) or Alt-hovered (auditions). */
    void clickCard (int index);
    void hoverCard (int index, bool altDown);

    /** The slot a card in the current category goes into. */
    GuitarSlot targetSlot() const;

    /*  The drawer's cards as laid out: one rectangle per drawer part, empty for a
        card scrolled out of view. The wheel over the drawer scrolls it a row at
        a time; showing a category scrolls the fitted part into view. */
    const juce::Array<juce::Rectangle<int>>& getCardBounds() const noexcept { return cardBounds; }
    void scrollDrawer (int rows);
    int getDrawerFirstRow() const noexcept { return drawerFirstRow; }

    /** The category buttons' rows (section 1): more than one when the labels do not fit in one. */
    int getCategoryRows() const noexcept { return categoryRows; }
    juce::TextButton* getCategoryButton (int index) const { return categoryButtons[index]; }
    juce::TextButton& getSwapButton() noexcept { return swapButton; }
    juce::TextButton& getRevertButton() noexcept { return revertButton; }
    juce::TextButton& getSavePartButton() noexcept { return savePartButton; }

    //==========================================================================
    /*  Finish colours (guitar-illustration.md 11): the inspector's colour
        controls for the selected body (its colour, a burst's edge, the preset
        swatches) or plastics (pickguard, knobs, switch tip, plastic covers).
        A chip opens a gradient picker (juce::ColourSelector) in a call-out;
        picks while it drags preview on the bench and commit as one undo entry
        once the drag rests (kPaintSettleMs) or the call-out closes. */
    using Paint = WorkshopBench::Paint;

    /** The paints the current selection offers, in the order the chips show them. */
    juce::Array<Paint> paintTargets() const;

    /** The colour a paint shows now, as drawn. */
    juce::Colour getPaintColour (Paint which) const;

    /** What the picker does on every drag step: live on the bench, no undo entry yet. */
    void previewPaint (Paint which, juce::Colour colour);

    /** Commits a previewed colour now (the settle timer and the call-out closing call this). */
    bool commitPaint();

    /** A committed pick in one step (a swatch, a typed colour, a test). */
    bool pickPaint (Paint which, juce::Colour colour);

    /** A preset swatch ("sunburst", "cherry", ...): one undo entry. */
    bool applyFinishPreset (const juce::String& id);

    /** Opens the gradient picker for a paint, pointing at its chip. */
    void openColourPicker (Paint which);

    /*  The picker the call-out holds: a juce::ColourSelector (the colour, its
        hex, the saturation / value square and the hue strip), with Centre and
        Edge tabs and the burst they make for a burst's body. Closing it
        commits what it previewed. */
    std::unique_ptr<juce::Component> createColourPicker (Paint which);

    static constexpr int kPaintSettleMs = 450;

    /*  guitar-illustration.md 12.1: the drawer's first category changes the
        guitar's family. The first change in a session asks first; tests and
        the second change go straight through. */
    bool switchFamily (const juce::String& family, bool confirmed);
    bool familyConfirmedThisSession = false;

    /** Section 5: edits one field of the selected part (a user copy of it). */
    bool editInspectorField (const juce::String& field, const juce::String& text);

    /** What the inspector is showing: "Bridge: ABR-1 Tune-o-Matic" etc. */
    juce::String getInspectorTitle() const { return inspectorTitle; }
    juce::StringArray getInspectorLines() const { return inspectorLines; }

    /** The banner over the bench (a limit, a family note), for the tests. */
    juce::String getBannerMessage() const { return limitMessage; }

    /** The spectrum pane's current sentence (section 10). */
    juce::String getSpectrumSummary() const { return spectrum.summary; }
    const SpectrumDelta::Result& getSpectrumResult() const noexcept { return spectrum; }

    /*  Section 10: the summary is what a screen reader hears, as an
        announcement each time it changes (accessibility.md 1). The last one
        announced, for the tests. */
    juce::String getLastSpectrumAnnouncement() const { return announcedSummary; }

    /*  gui-integration 21 / advanced-ranges.md 6.1: the range families whose
        parameters this panel holds - the setup strip's (fret-buzz.md 7) - so
        the WORKSHOP tab carries a padlock when any of them is unlocked or
        outside stock. Part fields have no ranges (workshop-ui.md 5). */
    static juce::Array<RangeFamily> rangeFamilies();

    /** The category buttons carry translated text; this is the id behind each. */
    juce::String categoryIdOfButton (int index) const;

    /** Waits (pumping nothing) for the spectrum worker's newest result; tests only. */
    bool waitForSpectrum (int timeoutMs);

    /*  onboarding.md 9: the one-time hint under the bench header, in the
        first session. The timer calls this while the bench is on screen. */
    void showFirstEncounterHintIfDue();
    FirstEncounterHint& getFirstEncounterHint() noexcept { return firstEncounterHint; }

private:
    void timerCallback() override;
    void refreshAll();
    void refreshHeader();
    void refreshInspector();
    void refreshInspectorText();
    void refreshDrawer();
    void requestSpectrum (int pickupIndex = -1, double positionMm = 0.0);
    void takeSpectrumResult (SpectrumDelta::Result&& result);
    void paintSpectrum (juce::Graphics&, juce::Rectangle<int> area);
    void paintDrawer (juce::Graphics&, juce::Rectangle<int> area);
    void paintInspector (juce::Graphics&, juce::Rectangle<int> area);
    int cardAt (juce::Point<int>) const;

    void layoutDrawer();
    void scrollFittedIntoView();
    void layoutPaintControls();
    void layoutInspectorButtons();

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void modifierKeysChanged (const juce::ModifierKeys&) override;

    LuthierAudioProcessor& processor;
    WorkshopBench& bench;

    BenchIllustration illustration;
    SpectrumDelta worker;
    SpectrumDelta::Result spectrum;
    juce::uint32 lastRequest = 0;
    bool autoZoom = false;
    juce::String announcedSummary;
    juce::Component spectrumPane;      ///< the accessible element behind the painted curve

    juce::Label title, guitarName;
    juce::TextButton saveAsButton { "Save As Guitar" };
    FirstEncounterHint firstEncounterHint { FirstEncounterHint::kWorkshopKey, FirstEncounterHint::kWorkshopText };
    juce::OwnedArray<juce::TextButton> slotButtons;
    juce::OwnedArray<juce::TextButton> categoryButtons;
    juce::TextButton swapButton { "Swap" }, revertButton { "Revert" }, savePartButton { "Save as user part" };
    juce::ToggleButton autoZoomToggle { "Auto-zoom" };

    std::unique_ptr<LuthierKnob> actionTreble, actionBass, relief;
    juce::OwnedArray<LuthierKnob> nutDepths;

    juce::String category = "Pickups";
    juce::Array<PartPtr> drawerParts;
    juce::Array<juce::Rectangle<int>> cardBounds;
    juce::Rectangle<int> cardsArea;
    int drawerFirstRow = 0, drawerPerRow = 1, drawerRows = 0, drawerVisibleRows = 0, drawerHidden = 0;
    int categoryRows = 1;

    // The inspector's paint controls: a chip per paint, the preset swatches, plastics reset.
    class PaintChip;
    juce::OwnedArray<PaintChip> paintChips, presetSwatches;
    std::unique_ptr<juce::TextButton> plasticsReset;
    juce::Rectangle<int> paintArea;
    std::optional<Paint> pendingPaint;
    juce::Colour pendingColour;
    juce::uint32 pendingSince = 0;
    juce::Component::SafePointer<juce::CallOutBox> pickerBox;
    int hoveredCard = -1;
    bool auditioning = false;

    juce::String inspectorTitle;
    juce::StringArray inspectorLines;
    juce::StringArray inspectorFields;          ///< the part field behind each line, or empty
    juce::Array<juce::Rectangle<int>> inspectorRows;
    std::unique_ptr<juce::TextEditor> fieldEditor;
    juce::String editingField;
    juce::String limitMessage;
    juce::uint32 limitShownAt = 0;

    juce::Rectangle<int> illustrationArea, drawerArea, inspectorArea, setupArea, spectrumArea, headerArea;
    juce::int64 shownGuitarKey = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorkshopPanel)
};

//==============================================================================
/** Easy mode's Workshop: the same bench as an overlay, opened by the header
    wrench and closed with Escape (gui-integration.md 6). */
class WorkshopOverlay : public OverlayPanel
{
public:
    explicit WorkshopOverlay (LuthierAudioProcessor& processor) : OverlayPanel ("Workshop"), panel (processor)
    {
        addAndMakeVisible (panel);

        // onboarding 9 says "Escape closes." - true of the overlay, not of the tab.
        panel.getFirstEncounterHint().setText (juce::String (FirstEncounterHint::kWorkshopText) + " Escape closes.");
    }

    juce::Point<int> getPreferredSize() const override { return { 1180, 720 }; }

    WorkshopPanel& getPanel() noexcept { return panel; }

protected:
    void layoutContent (juce::Rectangle<int> content) override { panel.setBounds (content); }

private:
    WorkshopPanel panel;
};

} // namespace luthier
