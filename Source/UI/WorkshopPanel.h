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
#include "Guitar/GuitarRenderer.h"
#include "../Workshop/SpectrumDelta.h"

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

    /** workshop-ui.md 2: the pick shows on the bench while its tool is in use
        (the drawer's Pick category, or the pick selected). */
    void setPickShown (bool shown);
    bool isPickShown() const noexcept { return pickShown; }

    /** The overlay the bench paints now: capo, slide and pick from the parameters. */
    GuitarOverlay currentOverlay() const;

    /** Where the slide rests on the bench when no note holds it (fret). */
    float getSlideRestFret() const noexcept { return slideRestFret; }

    /** Section 16's sentence for an accessory region. */
    juce::String describeAccessory (GuitarRegion region) const;

private:
    void timerCallback() override;
    GuitarRegion regionAt (juce::Point<float> px, int* stringIndex = nullptr) const;
    void rebuild (bool force);
    int pickupIndexFor (GuitarRegion) const;

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

    // Drag state
    enum class Drag { none, pickup, saddle, pan, nut, pick, pickRotate, slide, slideRotate, capo };
    Drag drag = Drag::none;
    int dragIndex = -1;
    juce::Point<float> dragStartMm;
    double dragStartValue = 0.0, dragStartValue2 = 0.0;

    // The accessories (workshop-ui.md 4): parameters, dragged with a gesture each.
    bool pickShown = false;
    float slideRestFret = 7.0f;
    GuitarRegion accessoryAt (juce::Point<float> px, bool* onHandle = nullptr) const;
    void beginParameterGesture (const char* id);
    void endParameterGestures();
    void setParameterPlain (const char* id, double plain);
    double getParameterPlain (const char* id) const;
    juce::StringArray gestureIds;
    double lastOverlaySignature = 0.0;
    double scaleMm() const;

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
    void clickCard (int index, bool ontoSelectedString = false);
    void hoverCard (int index, bool altDown);

    /** The slot a card in the current category goes into. */
    GuitarSlot targetSlot() const;

    /*  guitar-illustration.md 12.1: the drawer's first category changes the
        guitar's family. The first change in a session asks first; tests and
        the second change go straight through. */
    bool switchFamily (const juce::String& family, bool confirmed);
    bool familyConfirmedThisSession = false;

    /*  Section 3.3: with a string selected, the inspector's "string gauge",
        "string wound" and "string material" rows edit that string's override. */
    static constexpr const char* kStringFieldPrefix = "#string.";

    /** Section 5: edits one field of the selected part (a user copy of it). */
    bool editInspectorField (const juce::String& field, const juce::String& text);

    /** What the inspector is showing: "Bridge: ABR-1 Tune-o-Matic" etc. */
    juce::String getInspectorTitle() const { return inspectorTitle; }
    juce::StringArray getInspectorLines() const { return inspectorLines; }

    /** The spectrum pane's current sentence (section 10). */
    juce::String getSpectrumSummary() const { return spectrum.summary; }
    const SpectrumDelta::Result& getSpectrumResult() const noexcept { return spectrum; }

    /** Waits (pumping nothing) for the spectrum worker's newest result; tests only. */
    bool waitForSpectrum (int timeoutMs);

    /*  Section 10: the spectrum delta reaches a screen reader as its summary
        sentence, announced when a new result lands. The last one, for tests. */
    juce::String getLastAnnouncement() const { return lastAnnouncement; }

private:
    void timerCallback() override;
    void refreshAll();
    void refreshHeader();
    void refreshInspector();
    void refreshDrawer();
    void requestSpectrum (int pickupIndex = -1, double positionMm = 0.0);
    void takeSpectrum (SpectrumDelta::Result&& result);
    juce::String lastAnnouncement;
    void paintSpectrum (juce::Graphics&, juce::Rectangle<int> area);
    void paintDrawer (juce::Graphics&, juce::Rectangle<int> area);
    void paintInspector (juce::Graphics&, juce::Rectangle<int> area);
    int cardAt (juce::Point<int>) const;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void modifierKeysChanged (const juce::ModifierKeys&) override;

    LuthierAudioProcessor& processor;
    WorkshopBench& bench;

    BenchIllustration illustration;
    SpectrumDelta worker;
    SpectrumDelta::Result spectrum;
    juce::uint32 lastRequest = 0;
    bool autoZoom = false;

    juce::Label title, guitarName;
    juce::TextButton saveAsButton { "Save As Guitar" };
    juce::OwnedArray<juce::TextButton> slotButtons;
    juce::OwnedArray<juce::TextButton> categoryButtons;
    juce::TextButton swapButton { "Swap" }, revertButton { "Revert" }, savePartButton { "Save as user part" };
    juce::ToggleButton autoZoomToggle { "Auto-zoom" };

    /*  Section 1's narrow layouts: below 900 points the inspector is a drawer
        beside the illustration while a part is selected; below 700 the drawer's
        categories are a dropdown. */
public:
    static constexpr int kWideBench = 900, kNarrowBench = 700;
    bool isInspectorCollapsed() const noexcept { return inspectorCollapsed; }
    bool isInspectorShowing() const noexcept { return ! inspectorArea.isEmpty(); }
    bool areCategoriesADropdown() const noexcept { return categoryBox.isVisible(); }
private:
    juce::ComboBox categoryBox;
    bool inspectorCollapsed = false;

    std::unique_ptr<LuthierKnob> actionTreble, actionBass, relief;
    juce::OwnedArray<LuthierKnob> nutDepths;

    juce::String category = "Pickups";
    juce::Array<PartPtr> drawerParts;
    juce::Array<juce::Rectangle<int>> cardBounds;
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
    }

    juce::Point<int> getPreferredSize() const override { return { 1180, 720 }; }

    WorkshopPanel& getPanel() noexcept { return panel; }

protected:
    void layoutContent (juce::Rectangle<int> content) override { panel.setBounds (content); }

private:
    WorkshopPanel panel;
};

} // namespace luthier
