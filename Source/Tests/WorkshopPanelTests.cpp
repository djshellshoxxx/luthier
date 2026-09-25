/*  The Workshop bench's surface: workshop-ui.md 3, 4, 9, 10 and 11. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../UI/WorkshopPanel.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/HeaderBar.h"
#include "../UI/Overlays.h"
#include "../UI/RangesUi.h"
#include "../Accessibility/Localisation.h"
#include "../Model/Workshop/PartAcoustics.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    struct Bench
    {
        LuthierAudioProcessor processor;
        std::unique_ptr<WorkshopPanel> panel;

        explicit Bench (GuitarType type = GuitarType::LesPaul)
        {
            processor.prepareToPlay (48000.0, 512);
            auto* p = processor.getState().getParameter (ParamIDs::guitarType);
            p->setValueNotifyingHost (p->convertTo0to1 ((float) type));
            processor.getParameterBridge().applyAllNow();

            panel = std::make_unique<WorkshopPanel> (processor);
            panel->setVisible (true);
            panel->setSize (1200, 760);
        }

        BenchIllustration& illustration() { return panel->getIllustration(); }

        juce::MouseEvent event (juce::Component& target, juce::Point<float> p, juce::ModifierKeys mods = {})
        {
            auto source = juce::Desktop::getInstance().getMainMouseSource();
            return juce::MouseEvent (source, p, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                     &target, &target, juce::Time::getCurrentTime(), p,
                                     juce::Time::getCurrentTime(), 1, false);
        }

        /** The centre of a region's hit area, in the illustration's pixels. */
        juce::Point<float> centreOf (GuitarRegion region)
        {
            for (auto& h : illustration().getScene().hits)
                if (h.region == region)
                    return illustration().toPx (h.area.getBounds().getCentre());
            return {};
        }
    };

    juce::Image render (juce::Component& c)
    {
        juce::Image image (juce::Image::ARGB, juce::jmax (1, c.getWidth()), juce::jmax (1, c.getHeight()), true,
                           juce::SoftwareImageType());
        {
            juce::Graphics g (image);
            c.paintEntireComponent (g, true);
        }
        return image;
    }

    GuitarRegion regionFor (GuitarSlot slot)
    {
        switch (slot)
        {
            case GuitarSlot::body:          return GuitarRegion::body;
            case GuitarSlot::neck:          return GuitarRegion::neck;
            case GuitarSlot::fretboard:     return GuitarRegion::fretboard;
            case GuitarSlot::nut:           return GuitarRegion::nut;
            case GuitarSlot::bridge:        return GuitarRegion::bridge;
            case GuitarSlot::tuners:        return GuitarRegion::tuners;
            case GuitarSlot::strings:       return GuitarRegion::strings;
            case GuitarSlot::pickupNeck:    return GuitarRegion::pickupNeck;
            case GuitarSlot::pickupMiddle:  return GuitarRegion::pickupMiddle;
            case GuitarSlot::pickupBridge:  return GuitarRegion::pickupBridge;
            default:                        return GuitarRegion::none;
        }
    }
}

//==============================================================================
LUTHIER_TEST (WorkshopPanel, everyFittedPartIsReachableAndNothingElseIs)
{
    // Section 11: a grid over the illustration reaches every slot the guitar has,
    // and never reports a part it does not have.
    for (const auto& entry : juce::RangedDirectoryIterator (PartLibrary::getFactoryGuitarsFolder(), true, "*.luthierguitar"))
    {
        WorkshopGuitar guitar;
        PartLibrary::LoadReport report;
        PartLibrary library;
        library.refreshFrom (PartLibrary::getFactoryPartsFolder(), juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-no-user-parts"));
        library.loadGuitar (entry.getFile(), guitar, report);

        const auto scene = GuitarRenderer::build (guitar);
        std::set<int> reached;

        for (float x = scene.bounds.getX(); x < scene.bounds.getRight(); x += 1.0f)
            for (float y = scene.bounds.getY(); y < scene.bounds.getBottom(); y += 1.0f)
                if (const auto* hit = GuitarRenderer::hitTest (scene, { x, y }))
                    reached.insert ((int) hit->region);

        const auto name = entry.getFile().getFileNameWithoutExtension();

        for (auto slot : { GuitarSlot::body, GuitarSlot::neck, GuitarSlot::fretboard, GuitarSlot::nut, GuitarSlot::bridge,
                           GuitarSlot::tuners, GuitarSlot::strings, GuitarSlot::pickupNeck, GuitarSlot::pickupMiddle, GuitarSlot::pickupBridge })
        {
            const auto part = guitar.get (slot);
            const bool drawn = part != nullptr && ! (part->type == PartType::pickup && part->text ("family") == "piezo");
            const bool isReached = reached.count ((int) regionFor (slot)) > 0;

            if (drawn)
                CHECK_MSG (isReached, name + ": the " + WorkshopBench::describeSlot (slot) + " cannot be clicked");
            else if (regionFor (slot) != GuitarRegion::none && slot != GuitarSlot::bridge)
                CHECK_MSG (! isReached, name + ": a " + WorkshopBench::describeSlot (slot) + " it does not have can be clicked");
        }
    }
}

LUTHIER_TEST (WorkshopPanel, hoverDoesNotSelect)
{
    Bench b;
    const int steps = b.processor.getNumUndoSteps();
    const auto before = b.panel->getInspectorTitle();

    for (auto r : { GuitarRegion::body, GuitarRegion::pickupNeck, GuitarRegion::bridge, GuitarRegion::headstock, GuitarRegion::pickupBridge })
        b.illustration().mouseMove (b.event (b.illustration(), b.centreOf (r)));

    CHECK (b.illustration().getSelected() == GuitarRegion::none);
    CHECK (b.panel->getInspectorTitle() == before);
    CHECK (b.processor.getNumUndoSteps() == steps);
}

LUTHIER_TEST (WorkshopPanel, auditionFromTheDrawerNeverCommits)
{
    Bench b;
    const auto committed = b.processor.getCurrentGuitar();
    const int steps = b.processor.getNumUndoSteps();

    b.panel->showCategory ("Bridge");
    CHECK (b.panel->getDrawerParts().size() > 1);

    int other = 0;
    while (b.panel->getDrawerParts()[other]->name == committed.get (GuitarSlot::bridge)->name)
        ++other;

    b.panel->hoverCard (other, true);
    CHECK (b.processor.getBench().isAuditioning());
    CHECK (b.processor.getCurrentGuitar() == committed);

    // The spectrum pane shows candidate against committed.
    CHECK (b.panel->waitForSpectrum (10000));
    CHECK (b.panel->getSpectrumSummary().isNotEmpty());

    b.panel->hoverCard (other, false);
    CHECK (! b.processor.getBench().isAuditioning());
    CHECK (b.processor.getCurrentGuitar() == committed);
    CHECK (b.processor.getNumUndoSteps() == steps);
}

LUTHIER_TEST (WorkshopPanel, clickingACardFitsItAsOneUndoEntry)
{
    Bench b;
    const int steps = b.processor.getNumUndoSteps();

    b.panel->showCategory ("Bridge");
    const auto fitted = b.processor.getCurrentGuitar().get (GuitarSlot::bridge)->name;

    int other = 0;
    while (b.panel->getDrawerParts()[other]->name == fitted)
        ++other;

    const auto choice = b.panel->getDrawerParts()[other]->name;
    b.panel->clickCard (other);

    CHECK (b.processor.getCurrentGuitar().get (GuitarSlot::bridge)->name == choice);
    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK (b.processor.getUndoDescription().startsWith ("Fitted " + choice));
}

LUTHIER_TEST (WorkshopPanel, aPickupDragIsOneEntryAndTheRulerValueFollows)
{
    Bench b;
    auto& ill = b.illustration();
    const int steps = b.processor.getNumUndoSteps();
    const double start = b.processor.getCurrentGuitar().placements[0].positionMm;

    const auto from = b.centreOf (GuitarRegion::pickupNeck);
    ill.mouseDown (b.event (ill, from));
    CHECK (ill.getSelected() == GuitarRegion::pickupNeck);

    // Drag toward the bridge by 10 mm, in steps, as a hand would.
    const float pxPerMm = ill.toPx ({ 1.0f, 0.0f }).x - ill.toPx ({ 0.0f, 0.0f }).x;   // negative: X runs right to left

    for (int i = 1; i <= 10; ++i)
        ill.mouseDrag (b.event (ill, from + juce::Point<float> (-pxPerMm * (float) i, 0.0f)));

    // Live: the bench shows the new value before release (section 4).
    CHECK_NEAR (b.processor.getBench().current().placements[0].positionMm, start - 10.0, 0.6);

    ill.mouseUp (b.event (ill, from));

    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK (b.processor.getUndoDescription().startsWith ("Moved neck pickup"));
    CHECK_NEAR (b.processor.getCurrentGuitar().placements[0].positionMm, start - 10.0, 0.6);
}

LUTHIER_TEST (WorkshopPanel, keyboardNudgesMatchADrag)
{
    // Section 10 / 11 keyboard parity: arrows reach what a drag reaches.
    Bench b;
    auto& ill = b.illustration();
    const double start = b.processor.getCurrentGuitar().placements[0].positionMm;

    ill.select (GuitarRegion::pickupNeck);

    for (int i = 0; i < 6; ++i)
        ill.keyPressed (juce::KeyPress (juce::KeyPress::rightKey));

    CHECK_NEAR (b.processor.getCurrentGuitar().placements[0].positionMm, start - 6.0, 1.0e-6);

    ill.keyPressed (juce::KeyPress (juce::KeyPress::leftKey, juce::ModifierKeys::shiftModifier, 0));
    CHECK_NEAR (b.processor.getCurrentGuitar().placements[0].positionMm, start - 5.9, 1.0e-6);

    // Tab walks the parts in the builder's order.
    ill.select (GuitarRegion::none);
    ill.keyPressed (juce::KeyPress (juce::KeyPress::tabKey));
    CHECK (ill.getSelected() == GuitarRegion::body);
    ill.keyPressed (juce::KeyPress (juce::KeyPress::tabKey));
    CHECK (ill.getSelected() == GuitarRegion::neck);
}

LUTHIER_TEST (WorkshopPanel, theInspectorShowsTheSelectedPart)
{
    Bench b;
    b.illustration().select (GuitarRegion::bridge, 2);

    CHECK_MSG (b.panel->getInspectorTitle().startsWith ("Bridge: "), "inspector: " + b.panel->getInspectorTitle());
    CHECK (b.panel->getInspectorLines().joinIntoString ("|").contains ("Factory part"));
    CHECK (b.panel->getInspectorLines().joinIntoString ("|").contains ("string 3 saddle"));
    CHECK (b.panel->getCategory() == "Bridge");

    b.illustration().select (GuitarRegion::pickupNeck);
    CHECK (b.panel->getInspectorLines().joinIntoString ("|").contains ("mm from the saddle"));
}

LUTHIER_TEST (WorkshopPanel, aSlideNeedsSlideMode)
{
    // Section 9: the Slide category says how to enable it rather than failing silently.
    Bench b;
    b.panel->showCategory ("Slide");
    CHECK (b.panel->getDrawerParts().size() > 0);

    const int steps = b.processor.getNumUndoSteps();
    b.panel->clickCard (0);
    CHECK (b.processor.getNumUndoSteps() == steps);
}

LUTHIER_TEST (WorkshopPanel, itPaintsAndTheWorkshopTabTakesOverColumnsThreeAndFour)
{
    Bench b;
    const auto image = render (*b.panel);
    const juce::Image::BitmapData px (image, juce::Image::BitmapData::readOnly);
    int drawn = 0;
    for (int y = 0; y < px.height; y += 8)
        for (int x = 0; x < px.width; x += 8)
            drawn += px.getPixelColour (x, y).getAlpha() > 0 ? 1 : 0;
    CHECK (drawn > 100);

    // For a person to look at, beside the guitar renders.
    {
        b.illustration().select (GuitarRegion::pickupNeck);
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-guitar-renders");
        dir.createDirectory();
        const auto file = dir.getChildFile ("_workshop.png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (render (*b.panel), out);
    }

    AdvancedPanel advanced (b.processor);
    advanced.setVisible (true);
    advanced.setSize (1600, 900);

    CHECK (advanced.setWorkspaceTabNamed ("WORKSHOP"));
    CHECK (advanced.isWorkshopShowing());
    CHECK (advanced.getWorkspaceTabName (0) == "WORKSHOP");

    // The bench is wider than column 4 alone would be.
    CHECK_MSG (advanced.getWorkshopPanel()->getWidth() > 1600 / 2,
               "the bench is only " + juce::String (advanced.getWorkshopPanel()->getWidth()) + " px wide");

    CHECK (advanced.setWorkspaceTabNamed ("MOD"));
    CHECK (! advanced.isWorkshopShowing());
}

LUTHIER_TEST (WorkshopPanel, theGuitarCategorySwitchesFamily)
{
    // guitar-illustration.md 12.1: the drawer's first category is the family.
    Bench b;
    CHECK (WorkshopPanel::drawerCategories()[0] == "Guitar");

    b.panel->showCategory ("Guitar");
    CHECK (b.panel->getDrawerParts().size() == 5);

    // Confirmed (the dialog's answer), the guitar becomes a bass.
    CHECK (b.panel->switchFamily ("bass", true));
    CHECK (b.processor.getCurrentGuitar().family == "bass");
    CHECK (b.processor.getUndoDescription().startsWith ("Change guitar family"));

    // After the first confirmation in a session, a card click goes straight through.
    int acoustic = -1;
    for (int i = 0; i < b.panel->getDrawerParts().size(); ++i)
        if (b.panel->getDrawerParts()[i]->text ("family") == "acoustic")
            acoustic = i;

    b.panel->clickCard (acoustic);
    CHECK (b.processor.getCurrentGuitar().family == "acoustic");
}

LUTHIER_TEST (WorkshopPanel, editingAFieldMakesAUserCopy)
{
    // workshop-ui.md 5: editing a factory part fits an edited user copy, one undo entry.
    Bench b;
    b.panel->getIllustration().select (GuitarRegion::pickupBridge);
    const auto before = b.processor.getCurrentGuitar().get (GuitarSlot::pickupBridge);
    const int steps = b.processor.getNumUndoSteps();

    CHECK (b.panel->editInspectorField ("dc_resistance_k", "9.4"));

    const auto after = b.processor.getCurrentGuitar().get (GuitarSlot::pickupBridge);
    CHECK (after != nullptr && ! after->isFactory);
    CHECK_NEAR (after->number ("dc_resistance_k", 0.0), 9.4, 1.0e-9);
    CHECK (before->isFactory);
    CHECK_NEAR (before->number ("dc_resistance_k", 0.0), 8.1, 1.0e-9);   // the factory part is untouched
    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK (b.processor.getUndoDescription().startsWith ("Set dc resistance k of " + before->name));
    CHECK (b.processor.isGuitarEdited());
}

//==============================================================================
namespace
{
    template <typename T>
    void collect (juce::Component& root, juce::Array<T*>& found)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* match = dynamic_cast<T*> (child))
                found.add (match);

            collect<T> (*child, found);
        }
    }

    template <typename T>
    T* findOne (juce::Component& root)
    {
        juce::Array<T*> found;
        collect<T> (root, found);
        return found.isEmpty() ? nullptr : found.getFirst();
    }

    double plain (LuthierAudioProcessor& processor, const char* id)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id));
        return (double) p->convertFrom0to1 (p->getValue());
    }

    void setPlain (LuthierAudioProcessor& processor, const char* id, double value)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id));
        p->setValueNotifyingHost (p->convertTo0to1 ((float) value));
    }

    /** A screen offset for a millimetre offset, at the illustration's transform. */
    juce::Point<float> pxFor (BenchIllustration& ill, juce::Point<float> mmOffset)
    {
        return ill.toPx (mmOffset) - ill.toPx ({ 0.0f, 0.0f });
    }
}

LUTHIER_TEST (WorkshopPanel, aNutSlotDragsDownPerString)
{
    // workshop-ui.md 4: nut slots drag down per string, 0.05 mm steps, 0 - 1.2 mm.
    Bench b;
    auto& ill = b.illustration();
    const int steps = b.processor.getNumUndoSteps();

    // The nut runs 5 mm toward the headstock from where the strings cross it.
    const auto at = ill.toPx (ill.getScene().nutPoints[1] + juce::Point<float> (2.5f, 0.0f));
    CHECK_MSG (ill.nutSlotAt (at) == 1, "the nut point of string 2 hits slot " + juce::String (ill.nutSlotAt (at)));

    const auto& depths = b.processor.getCurrentGuitar().setup.nutSlotDepthsMm;
    const double start = juce::isPositiveAndBelow (1, depths.size()) ? depths[1] : 0.5;

    ill.mouseDown (b.event (ill, at));
    CHECK (ill.getSelected() == GuitarRegion::nut);
    CHECK (ill.getSelectedString() == 1);

    // 4 mm of travel toward the treble side cuts 0.4 mm deeper, in steps, to the 1.2 mm limit.
    const double expected = juce::jmin (WorkshopBench::kMaxNutSlot, start + 0.4);
    for (int i = 1; i <= 8; ++i)
        ill.mouseDrag (b.event (ill, at + pxFor (ill, { 0.0f, 0.5f * (float) i })));

    CHECK_NEAR (b.processor.getBench().current().setup.nutSlotDepthsMm[1], expected, 1.0e-6);
    ill.mouseUp (b.event (ill, at));

    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK_MSG (b.processor.getUndoDescription().startsWith ("Set string 2 nut slot"), "undo reads \"" + b.processor.getUndoDescription() + "\"");
    CHECK_NEAR (b.processor.getCurrentGuitar().setup.nutSlotDepthsMm[1], expected, 1.0e-6);

    // The inspector reads the slot; the description names it for a screen reader.
    CHECK (b.panel->getInspectorLines().joinIntoString ("|").contains ("string 2 slot"));
    CHECK (ill.describeSelection().contains ("String 2 slot"));

    // Keyboard parity: up cuts a step shallower, down deeper, to the limits.
    ill.keyPressed (juce::KeyPress (juce::KeyPress::upKey));
    CHECK_NEAR (b.processor.getCurrentGuitar().setup.nutSlotDepthsMm[1], expected - 0.05, 1.0e-6);
    for (int i = 0; i < 40; ++i)
        ill.keyPressed (juce::KeyPress (juce::KeyPress::downKey));
    CHECK_NEAR (b.processor.getCurrentGuitar().setup.nutSlotDepthsMm[1], WorkshopBench::kMaxNutSlot, 1.0e-6);
    for (int i = 0; i < 40; ++i)
        ill.keyPressed (juce::KeyPress (juce::KeyPress::upKey));
    CHECK_NEAR (b.processor.getCurrentGuitar().setup.nutSlotDepthsMm[1], 0.0, 1.0e-6);
}

LUTHIER_TEST (WorkshopPanel, thePickSlideAndCapoAreOnTheBenchAndDrag)
{
    Bench b;
    auto& ill = b.illustration();
    using Tool = BenchIllustration::Tool;

    // The pick is always there; the slide needs Slide Mode; the capo is parked on the headstock.
    CHECK (ill.hasTool (Tool::pick));
    CHECK (! ill.hasTool (Tool::slide));
    CHECK (ill.hasTool (Tool::capo));
    CHECK (! ill.toolArea (Tool::pick).isEmpty());
    CHECK (ill.toolArea (Tool::slide).isEmpty());

    // ---- the pick: drag along the strings, one entry --------------------------------------
    int steps = b.processor.getNumUndoSteps();
    const double pickStart = b.processor.getBench().getPickPositionMm();
    const auto grip = ill.toPx (ill.pickTipMm() + juce::Point<float> (0.0f, -8.0f));
    ill.mouseDown (b.event (ill, grip));
    CHECK_MSG (ill.getSelectedTool() == Tool::pick, "clicking the pick selected something else");
    CHECK (b.panel->getInspectorTitle().startsWith ("Pick"));

    for (int i = 1; i <= 10; ++i)
        ill.mouseDrag (b.event (ill, grip + pxFor (ill, { -1.0f * (float) i, 0.0f })));
    ill.mouseUp (b.event (ill, grip));

    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK_MSG (b.processor.getUndoDescription().startsWith ("Moved pick"), "undo reads \"" + b.processor.getUndoDescription() + "\"");
    CHECK_NEAR (b.processor.getBench().getPickPositionMm(), pickStart - 10.0, 0.6);
    CHECK (b.panel->getInspectorLines().joinIntoString ("|").contains ("mm from the saddle"));
    CHECK (ill.describeSelection().startsWith ("Pick:"));

    // The inspector's fields are the precise path (section 0.1).
    CHECK (b.panel->editInspectorField ("pick_angle_deg", "25"));
    CHECK_NEAR (b.processor.getBench().getPickAngleDegrees(), 25.0, 1.0e-6);

    // ---- the capo: on the headstock until dragged onto a fret --------------------------------
    CHECK (ill.describeSelection().isNotEmpty());
    ill.selectTool (Tool::capo);
    CHECK (ill.describeSelection().contains ("headstock"));
    CHECK (b.panel->getInspectorTitle().startsWith ("Capo"));

    steps = b.processor.getNumUndoSteps();
    const auto& scene = ill.getScene();
    const auto parked = ill.toolArea (Tool::capo).getBounds().getCentre();
    const auto parkedPx = ill.toPx ({ parked.x, scene.nutPoints[0].y + (scene.nutPoints[(size_t) (scene.numStrings - 1)].y - scene.nutPoints[0].y) * 0.5f });
    ill.mouseDown (b.event (ill, parkedPx));
    CHECK (ill.getSelectedTool() == Tool::capo);

    // Toward the body until it is behind the 5th fret.
    const float targetX = scene.stringAt (0, 4.65f).x;
    const float fromX = parked.x;
    for (int i = 1; i <= 20; ++i)
        ill.mouseDrag (b.event (ill, parkedPx + pxFor (ill, { (targetX - fromX) * (float) i / 20.0f, 0.0f })));
    ill.mouseUp (b.event (ill, parkedPx));

    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK_MSG (b.processor.getBench().getCapoFret() == 5, "the capo landed on fret " + juce::String (b.processor.getBench().getCapoFret()));
    CHECK_NEAR (plain (b.processor, ParamIDs::capoFret), 5.0, 1.0e-6);
    CHECK (b.processor.getUndoDescription() == "Put the capo on fret 5");
    CHECK (ill.describeSelection().contains ("fret 5"));

    // Keyboard: right moves it up a fret; a whole-fret snap.
    ill.keyPressed (juce::KeyPress (juce::KeyPress::rightKey));
    CHECK (b.processor.getBench().getCapoFret() == 6);
    CHECK (ill.toolArea (Tool::capo).getBounds().getCentreX() < parked.x);

    // ---- the slide: Slide Mode on, drag the bar, drag an end to slant ------------------------
    setPlain (b.processor, ParamIDs::slideGuitar, 1.0);
    CHECK (ill.hasTool (Tool::slide));
    ill.selectTool (Tool::slide);
    CHECK (b.panel->getInspectorTitle().startsWith ("Slide"));

    steps = b.processor.getNumUndoSteps();
    const double fretStart = b.processor.getBench().getSlideFret();
    const auto barCentre = ill.toPx ((scene.stringAt (0, (float) fretStart) + scene.stringAt (scene.numStrings - 1, (float) fretStart)) * 0.5f);
    ill.mouseDown (b.event (ill, barCentre));
    CHECK (ill.getSelectedTool() == Tool::slide);
    for (int i = 1; i <= 10; ++i)
        ill.mouseDrag (b.event (ill, barCentre + pxFor (ill, { -3.0f * (float) i, 0.0f })));
    ill.mouseUp (b.event (ill, barCentre));

    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK_MSG (b.processor.getBench().getSlideFret() > fretStart + 0.3, "the bar did not move up the neck");
    CHECK (b.processor.getUndoDescription().startsWith ("Moved slide to fret"));

    // Tab reaches the tools after the parts (section 10).
    ill.select (GuitarRegion::none);
    std::set<int> reached;
    for (int i = 0; i < 40; ++i)
    {
        ill.keyPressed (juce::KeyPress (juce::KeyPress::tabKey));
        if (ill.getSelectedTool() != Tool::none)
            reached.insert ((int) ill.getSelectedTool());
    }
    CHECK (reached.count ((int) Tool::pick) && reached.count ((int) Tool::slide) && reached.count ((int) Tool::capo));

    // Hover over a tool names it without selecting it.
    ill.select (GuitarRegion::none);
    ill.mouseMove (b.event (ill, grip));
    CHECK (ill.getSelectedTool() == Tool::none);
    CHECK (ill.getHelpText().startsWith ("Pick:"));
}

LUTHIER_TEST (WorkshopPanel, aStringOverrideShowsInTheInspectorAndIsDrawnInItsColour)
{
    // workshop-ui.md 3.3: clicking a string shows the set and that string's own
    // fields; an overridden string is drawn in its material's colour
    // (guitar-illustration.md 10).
    Bench b;
    auto& ill = b.illustration();
    const int low = b.processor.getCurrentGuitar().getStringCount() - 1;

    ill.select (GuitarRegion::strings, low);
    auto lines = b.panel->getInspectorLines().joinIntoString ("|");
    CHECK_MSG (lines.contains ("string " + juce::String (low + 1) + " gauge in:"), "inspector: " + lines);
    CHECK (lines.contains ("plays the set's string"));

    const int steps = b.processor.getNumUndoSteps();
    CHECK (b.panel->editInspectorField ("string_material", "phosphor bronze"));
    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK (b.processor.getUndoDescription().startsWith ("Set string " + juce::String (low + 1)));

    const auto* o = b.processor.getCurrentGuitar().getStringOverride (low);
    CHECK (o != nullptr && o->material == "phosphor_bronze");
    lines = b.panel->getInspectorLines().joinIntoString ("|");
    CHECK (lines.contains ("material: phosphor bronze"));
    CHECK (lines.contains ("is overridden"));
    CHECK (ill.describeSelection().contains ("Overridden"));

    // Drawn in phosphor bronze along the string.
    const auto image = render (ill);
    const auto want = GuitarRenderer::stringColour ("phosphor_bronze", true, false);
    const auto& scene = ill.getScene();
    float best = 1.0e9f;

    for (float t = 0.3f; t <= 0.7f; t += 0.05f)
    {
        const auto p = ill.toPx (scene.saddlePoints[(size_t) low] + (scene.nutPoints[(size_t) low] - scene.saddlePoints[(size_t) low]) * t);

        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
            {
                const auto c = image.getPixelAt (juce::roundToInt (p.x) + dx, juce::roundToInt (p.y) + dy);
                const float d = std::abs ((float) c.getRed() - want.getRed()) + std::abs ((float) c.getGreen() - want.getGreen())
                              + std::abs ((float) c.getBlue() - want.getBlue());
                best = juce::jmin (best, d);
            }
    }

    CHECK_MSG (best < 90.0f, "no pixel along the overridden string is near its material colour (closest " + juce::String (best) + ")");

    // Back to the set's material clears the override; a gauge of its own makes one.
    CHECK (b.panel->editInspectorField ("string_material", "nickel plated steel"));
    CHECK (b.processor.getCurrentGuitar().getStringOverride (low) == nullptr);
    CHECK (b.processor.getUndoDescription().startsWith ("Cleared string"));

    CHECK (b.panel->editInspectorField ("string_gauge_in", "0.052"));
    CHECK_NEAR (b.processor.getCurrentGuitar().getStringGaugeIn (low), 0.052, 1.0e-12);
    CHECK (b.processor.isGuitarEdited());

    // And it rides in the plugin state (guitar-workshop.md 8).
    juce::MemoryBlock state;
    b.processor.getStateInformation (state);
    LuthierAudioProcessor restored;
    restored.prepareToPlay (48000.0, 512);
    restored.setStateInformation (state.getData(), (int) state.getSize());
    CHECK_NEAR (restored.getCurrentGuitar().getStringGaugeIn (low), 0.052, 1.0e-12);
}

LUTHIER_TEST (WorkshopPanel, theWorkshopTabCarriesTheSetupStripsPadlock)
{
    // gui-integration 21 / advanced-ranges.md 6.1: the tab holding the setup
    // strip's parameters shows the padlock when their family is unlocked or a
    // value sits outside stock.
    Bench b;
    CHECK (WorkshopPanel::rangeFamilies().contains (RangeFamily::buzz));
    CHECK (RangeRegistry::find (ParamIDs::setupActionTreble) != nullptr
           && RangeRegistry::find (ParamIDs::setupActionTreble)->family == RangeFamily::buzz);
    CHECK (RangeRegistry::find (ParamIDs::setupNutDepth (1))->family == RangeFamily::buzz);

    AdvancedPanel advanced (b.processor);
    advanced.setVisible (true);
    advanced.setSize (1600, 900);

    juce::Array<RangesUi::RangeTabButton*> tabs;
    collect<RangesUi::RangeTabButton> (advanced, tabs);
    RangesUi::RangeTabButton* workshop = nullptr;
    for (auto* t : tabs)
        if (t->getButtonText() == "WORKSHOP")
            workshop = t;

    CHECK_MSG (workshop != nullptr, "the WORKSHOP tab has no padlock");
    if (workshop == nullptr)
        return;

    CHECK (workshop->getPadlockState() == 0);
    b.processor.getRanges().setFamilyAdvanced (RangeFamily::buzz, true);
    CHECK (workshop->getPadlockState() == 2);
    b.processor.getRanges().setFamilyAdvanced (RangeFamily::buzz, false);
    CHECK (workshop->getPadlockState() == 0);
}

LUTHIER_TEST (WorkshopPanel, theSpectrumSummaryIsAnnouncedToScreenReaders)
{
    // workshop-ui.md 10: the delta is announced as its sentence, not drawn only.
    Bench b;
    CHECK (b.panel->getLastSpectrumAnnouncement().isEmpty());

    b.panel->showCategory ("Bridge");
    const auto fitted = b.processor.getCurrentGuitar().get (GuitarSlot::bridge)->name;
    int other = 0;
    while (b.panel->getDrawerParts()[other]->name == fitted)
        ++other;

    b.panel->hoverCard (other, true);
    CHECK (b.panel->waitForSpectrum (10000));
    CHECK (b.panel->getSpectrumSummary().isNotEmpty());
    CHECK_MSG (b.panel->getLastSpectrumAnnouncement() == b.panel->getSpectrumSummary(),
               "announced \"" + b.panel->getLastSpectrumAnnouncement() + "\"");
    b.panel->hoverCard (other, false);

    // The pane is an accessible element carrying the sentence.
    juce::Array<juce::Component*> all;
    collect<juce::Component> (*b.panel, all);
    bool found = false;
    for (auto* c : all)
        if (c->getTitle() == "Spectrum delta" && c->getHelpText() == b.panel->getSpectrumSummary())
            found = true;
    CHECK_MSG (found, "no accessible element carries the spectrum summary");
}

LUTHIER_TEST (WorkshopPanel, everyControlHasATooltipAndANameFromTheCatalog)
{
    // accessibility.md 0: a label and a description for every control, from the catalog.
    Bench b;
    juce::Array<juce::Button*> buttons;
    collect<juce::Button> (*b.panel, buttons);
    CHECK (buttons.size() > 20);

    for (auto* button : buttons)
    {
        // The first-encounter hint's button is onboarding's, not the bench's.
        if (! button->isVisible() || button->findParentComponentOfClass<FirstEncounterHint>() != nullptr)
            continue;

        CHECK_MSG (button->getTooltip().isNotEmpty(), "no tooltip on \"" + button->getButtonText() + "\"");
        CHECK_MSG (button->getTitle().isNotEmpty(), "no accessible name on \"" + button->getButtonText() + "\"");
    }

    juce::Array<LuthierKnob*> knobs;
    collect<LuthierKnob> (*b.panel, knobs);
    CHECK (knobs.size() == 3 + ParamIDs::kNumNutDepths);
    for (auto* knob : knobs)
        CHECK_MSG (knob->getTooltip().isNotEmpty(), "a setup knob has no tooltip");

    auto& catalog = Localisation::get();
    for (const char* key : { "workshop.saveAsGuitar", "workshop.category.capo", "workshop.tip.nutSlot", "workshop.a11y.capoOff",
                             "workshop.spectrum.empty", "workshop.string.overridden", "workshop.field.pickPosition" })
        CHECK_MSG (catalog.hasKey (key), juce::String ("missing catalog key ") + key);

    // The category buttons show the catalog's text and keep their ids.
    CHECK (b.panel->categoryIdOfButton (0) == "Guitar");
    CHECK (b.panel->categoryIdOfButton (WorkshopPanel::drawerCategories().size() - 1) == "Capo");
}

LUTHIER_TEST (WorkshopPanel, theWrenchOpensTheBenchAsAnOverlayInEasyMode)
{
    // gui-integration.md 6: in Easy mode the header's wrench opens the same
    // bench as an overlay; Escape closes it.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    processor.getUiState().advancedMode = false;

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    CHECK_MSG (editor != nullptr, "no editor - is this target headless?");
    if (editor == nullptr)
        return;

    editor->setVisible (true);
    editor->setSize (1200, 720);

    auto* header = findOne<HeaderBar> (*editor);
    auto* host = findOne<OverlayHost> (*editor);
    CHECK (header != nullptr && host != nullptr);
    if (header == nullptr || host == nullptr)
        return;

    CHECK (! host->isShowingOverlay());

    // The wrench is a button with a name, and it does what its callback does.
    juce::Array<juce::Button*> buttons;
    collect<juce::Button> (*header, buttons);
    juce::Button* wrench = nullptr;
    for (auto* button : buttons)
        if (button->getButtonText() == "Workshop")
            wrench = button;
    CHECK_MSG (wrench != nullptr && wrench->getTitle().isNotEmpty() && wrench->getTooltip().isNotEmpty(), "the header has no Workshop button");

    CHECK (header->onOpenWorkshop != nullptr);
    if (wrench != nullptr && wrench->onClick != nullptr)
        wrench->onClick();
    else if (header->onOpenWorkshop)
        header->onOpenWorkshop();

    auto* overlay = dynamic_cast<WorkshopOverlay*> (host->getCurrentOverlay());
    CHECK_MSG (overlay != nullptr, "the wrench opened no Workshop overlay in Easy mode");
    if (overlay == nullptr)
        return;

    CHECK (overlay->isVisible() && overlay->getWidth() > 0 && overlay->getHeight() > 0);
    CHECK (overlay->getPanel().getIllustration().getScene().hits.size() > 0);

    const auto image = render (*overlay);
    const juce::Image::BitmapData px (image, juce::Image::BitmapData::readOnly);
    int drawn = 0;
    for (int y = 0; y < px.height; y += 8)
        for (int x = 0; x < px.width; x += 8)
            drawn += px.getPixelColour (x, y).getAlpha() > 0 ? 1 : 0;
    CHECK (drawn > 100);

    CHECK (editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK (! host->isShowingOverlay());
}

//==============================================================================
//  The body swap, finish paint and the drawer's legibility (2026-09-25).
//==============================================================================
namespace
{
    juce::Array<juce::File> factoryGuitarFiles()
    {
        juce::Array<juce::File> files;
        for (const auto& entry : juce::RangedDirectoryIterator (PartLibrary::getFactoryGuitarsFolder(), true, "*.luthierguitar"))
            files.add (entry.getFile());
        files.sort();
        return files;
    }

    /** A processor playing one factory guitar file, and the bench open on it. */
    struct GuitarBench
    {
        LuthierAudioProcessor processor;
        std::unique_ptr<WorkshopPanel> panel;
        bool loaded = false;

        explicit GuitarBench (const juce::File& file, int width = 1200, int height = 760)
        {
            processor.prepareToPlay (48000.0, 512);
            juce::String error;
            loaded = processor.loadGuitarFile (file, error);

            panel = std::make_unique<WorkshopPanel> (processor);
            panel->setVisible (true);
            panel->setSize (width, height);
        }
    };

    bool sameBodyConfig (const BodyConfig& a, const BodyConfig& b)
    {
        return a.shape == b.shape && a.topWood == b.topWood && a.backWood == b.backWood && a.sideWood == b.sideWood
            && a.bracing == b.bracing && a.scaleWidth == b.scaleWidth && a.scaleDepth == b.scaleDepth
            && a.topThicknessMm == b.topThicknessMm && a.soundHoleScale == b.soundHoleScale;
    }

    bool imagesDiffer (const juce::Image& a, const juce::Image& b)
    {
        const juce::Image::BitmapData da (a, juce::Image::BitmapData::readOnly), db (b, juce::Image::BitmapData::readOnly);
        int different = 0;

        for (int y = 0; y < a.getHeight(); y += 2)
            for (int x = 0; x < a.getWidth(); x += 2)
                if (da.getPixelColour (x, y) != db.getPixelColour (x, y))
                    ++different;

        return different > 50;
    }

    /** A flat fill of about this colour (aging fades a finish a few steps, 11.7). */
    bool sceneHasFill (const GuitarScene& scene, juce::Colour c)
    {
        auto near = [c] (juce::Colour f)
        {
            return std::abs ((int) f.getRed() - (int) c.getRed()) <= 16 && std::abs ((int) f.getGreen() - (int) c.getGreen()) <= 16
                && std::abs ((int) f.getBlue() - (int) c.getBlue()) <= 16;
        };

        for (const auto& sh : scene.shapes)
            if (sh.fill.isColour() && sh.fill.colour.getAlpha() > 128 && near (sh.fill.colour))
                return true;
        return false;
    }
}

/*  The report: "in the workshop you are unable to change the body of the guitar".
    Root cause: a body part carried no outline of its own, so fitting one kept
    the guitar file's body_style - the illustration and the engine's body shape
    stayed put - and the drawer listed bodies alphabetically with no scrolling,
    so on most guitars the family's own bodies were cut off below the fold.
    For every factory guitar: at least two bodies for its family, the first
    cards; fitting a different one changes the body, the drawn outline, and the
    engine's body config, as one undo entry that undoes. */
LUTHIER_TEST (WorkshopPanel, everyFactoryGuitarCanChangeItsBody)
{
    const auto files = factoryGuitarFiles();
    CHECK (files.size() >= 20);

    for (const auto& file : files)
    {
        const auto name = file.getFileNameWithoutExtension();
        GuitarBench b (file);
        CHECK_MSG (b.loaded, name + ": did not load");

        auto& panel = *b.panel;
        panel.showCategory ("Body");

        const auto before = b.processor.getCurrentGuitar();
        const auto family = before.family;
        const auto& parts = panel.getDrawerParts();

        int suiting = 0, candidate = -1;

        for (int i = 0; i < parts.size(); ++i)
        {
            if (! parts[i]->suits (family))
                continue;

            // The family's bodies come first in the drawer.
            CHECK_MSG (i == suiting, name + ": " + parts[i]->name + " is listed after another family's body");
            ++suiting;

            if (candidate < 0 && parts[i]->name != before.get (GuitarSlot::body)->name)
                candidate = i;
        }

        CHECK_MSG (suiting >= 2, name + ": only " + juce::String (suiting) + " " + family + " bodies to choose from");

        if (candidate < 0)
            continue;

        CHECK_MSG (! panel.getCardBounds()[candidate].isEmpty(), name + ": the " + parts[candidate]->name + " card is out of view");

        const auto choice = parts[candidate];
        const auto sceneBefore = GuitarRenderer::build (before);
        const auto derivedBefore = mapSpec (before);
        const int steps = b.processor.getNumUndoSteps();

        // Clicked as a person would.
        static_cast<juce::Component&> (panel).mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(),
                                           panel.getCardBounds()[candidate].getCentre().toFloat(), {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                           &panel, &panel, juce::Time::getCurrentTime(),
                                           panel.getCardBounds()[candidate].getCentre().toFloat(), juce::Time::getCurrentTime(), 1, false));

        const auto after = b.processor.getCurrentGuitar();
        CHECK_MSG (after.get (GuitarSlot::body) != nullptr && after.get (GuitarSlot::body)->name == choice->name,
                   name + ": clicking " + choice->name + " did not fit it");
        CHECK_MSG (b.processor.getNumUndoSteps() == steps + 1, name + ": the body swap is not one undo entry");

        // The illustration: the bench's and a fresh build both draw the new body.
        const auto sceneAfter = GuitarRenderer::build (after);
        const bool outlineChanged = sceneAfter.bodyStyle != sceneBefore.bodyStyle || sceneAfter.bodyBounds != sceneBefore.bodyBounds;
        const bool sameShapeFamily = WorkshopGuitar::bodyStyleOf (choice.get()) == WorkshopGuitar::bodyStyleOf (before.get (GuitarSlot::body).get());

        if (sameShapeFamily)
            CHECK_MSG (imagesDiffer (GuitarRenderer::render (before, 480, 200), GuitarRenderer::render (after, 480, 200)),
                       name + ": " + choice->name + " looks the same as the body it replaced");
        else
            CHECK_MSG (outlineChanged, name + ": the outline stayed " + sceneBefore.bodyStyle + " with " + choice->name);

        CHECK_MSG (panel.getIllustration().getScene().bodyStyle == sceneAfter.bodyStyle,
                   name + ": the bench still draws " + panel.getIllustration().getScene().bodyStyle);

        // The engine's body.
        CHECK_MSG (! sameBodyConfig (mapSpec (after).body, derivedBefore.body), name + ": the engine's body did not change");

        // And undo puts the old body - and its outline - back.
        b.processor.undo();
        CHECK_MSG (b.processor.getCurrentGuitar().get (GuitarSlot::body)->name == before.get (GuitarSlot::body)->name,
                   name + ": undo did not put the body back");
        CHECK_MSG (b.processor.getCurrentGuitar().bodyStyle == before.bodyStyle, name + ": undo did not put the outline back");
    }
}

LUTHIER_TEST (WorkshopPanel, anotherFamilysBodyFitsWithAReasonAndTheFix)
{
    GuitarBench b (PartLibrary::getFactoryGuitarsFolder().getChildFile ("Electric/Vintage Single-Cut.luthierguitar"));
    auto& panel = *b.panel;
    panel.showCategory ("Body");

    int dread = -1;
    for (int i = 0; i < panel.getDrawerParts().size(); ++i)
        if (panel.getDrawerParts()[i]->name == "Dreadnought Mahogany Body")
            dread = i;

    CHECK (dread >= 0);

    if (dread < 0)
        return;

    // Scrolled to, since the electric bodies come first.
    while (panel.getCardBounds()[dread].isEmpty() && panel.getDrawerFirstRow() < 20)
        panel.scrollDrawer (1);

    CHECK (! panel.getCardBounds()[dread].isEmpty());

    // guitar-workshop.md 5: a warning, never a refusal - and the banner says what makes a matched build.
    panel.clickCard (dread);
    CHECK (b.processor.getCurrentGuitar().get (GuitarSlot::body)->name == "Dreadnought Mahogany Body");
    CHECK (b.processor.getCurrentGuitar().bodyStyle == "dreadnought");
    CHECK_MSG (panel.getBannerMessage().contains ("acoustic") && panel.getBannerMessage().contains ("Guitar > Acoustic"),
               "banner: " + panel.getBannerMessage());
}

LUTHIER_TEST (WorkshopPanel, revertPutsTheFilesBodyOutlineBack)
{
    // The 7-string draws its shared single-cut body part as a superstrat; revert keeps that.
    GuitarBench b (PartLibrary::getFactoryGuitarsFolder().getChildFile ("Electric/7-String Modern.luthierguitar"));
    auto& bench = b.processor.getBench();
    const auto style = b.processor.getCurrentGuitar().bodyStyle;

    CHECK (bench.fit (GuitarSlot::body, b.processor.getPartLibrary().find (PartType::body, "Alder Offset")));
    CHECK (b.processor.getCurrentGuitar().bodyStyle == "offset");
    CHECK (bench.revert (GuitarSlot::body));
    CHECK_MSG (b.processor.getCurrentGuitar().bodyStyle == style, "reverted to " + b.processor.getCurrentGuitar().bodyStyle);
}

//==============================================================================
LUTHIER_TEST (WorkshopPanel, categoryBarAndInspectorButtonsNeverClip)
{
    Bench b;

    for (int width : { 1200, 900, 640 })
    {
        b.panel->setSize (width, 760);

        auto fits = [] (juce::TextButton& button)
        {
            const auto font = button.getLookAndFeel().getTextButtonFont (button, button.getHeight());
            const auto text = button.getButtonText().toUpperCase();
            return juce::GlyphArrangement::getStringWidth (font, text) + 4.0f <= (float) button.getWidth();
        };

        for (int i = 0; i < WorkshopPanel::drawerCategories().size(); ++i)
        {
            auto* button = b.panel->getCategoryButton (i);
            CHECK_MSG (fits (*button), "at " + juce::String (width) + " px the " + button->getButtonText() + " tab clips ("
                                         + juce::String (button->getWidth()) + " px, " + juce::String (b.panel->getCategoryRows()) + " rows)");
        }

        for (auto* button : { &b.panel->getSwapButton(), &b.panel->getRevertButton(), &b.panel->getSavePartButton() })
            CHECK_MSG (fits (*button), "at " + juce::String (width) + " px the inspector's " + button->getButtonText() + " clips");
    }

    // Narrow enough, the bar takes a second row rather than squeezing.
    b.panel->setSize (640, 760);
    CHECK (b.panel->getCategoryRows() >= 2);

    // For a person to look at.
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-guitar-renders");
    dir.createDirectory();
    for (int width : { 640, 1200 })
    {
        b.panel->setSize (width, 760);
        juce::FileOutputStream out (dir.getChildFile ("workshop-" + juce::String (width) + ".png"));
        if (out.openedOk()) { out.setPosition (0); out.truncate(); juce::PNGImageFormat().writeImageToStream (render (*b.panel), out); }
    }
}

//==============================================================================
LUTHIER_TEST (WorkshopPanel, bodyAndPlasticsColoursArePaintWithOneUndoEntryEach)
{
    GuitarBench b (PartLibrary::getFactoryGuitarsFolder().getChildFile ("Electric/Vintage Double-Cut.luthierguitar"));
    auto& panel = *b.panel;
    auto& ill = panel.getIllustration();
    using Paint = WorkshopPanel::Paint;

    // Nothing selected: nothing to paint.
    CHECK (panel.paintTargets().isEmpty());

    // The body of a burst: its centre and its edge.
    ill.select (GuitarRegion::body);
    CHECK (panel.paintTargets() == juce::Array<Paint> ({ Paint::body, Paint::burstEdge }));

    const auto derivedBefore = mapSpec (b.processor.getCurrentGuitar());
    auto* material = b.processor.getState().getParameter (ParamIDs::stringMaterial);
    material->setValueNotifyingHost (material->convertTo0to1 (3.0f));   // a refined parameter a colour must not reset
    const float refined = material->getValue();

    // Dragging in the picker previews and pushes nothing...
    int steps = b.processor.getNumUndoSteps();
    const auto committedFinish = b.processor.getCurrentGuitar().finish;

    for (auto c : { juce::Colour (0xff204080), juce::Colour (0xff2050a0), juce::Colour (0xff1e3e62) })
        panel.previewPaint (Paint::burstEdge, c);

    CHECK (b.processor.getNumUndoSteps() == steps);
    CHECK (b.processor.getCurrentGuitar().finish.colourA == committedFinish.colourA);
    CHECK (b.processor.getBench().current().finish.colourA == "#1E3E62");
    CHECK_MSG (sceneHasFill (ill.getScene(), juce::Colour (0xff1e3e62)), "the bench does not show the colour being dragged");

    // ...and settles into one entry.
    CHECK (panel.commitPaint());
    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK_MSG (b.processor.getUndoDescription().contains ("burst edge colour"), b.processor.getUndoDescription());
    CHECK (b.processor.getCurrentGuitar().finish.colourA == "#1E3E62");
    CHECK (b.processor.getCurrentGuitar().finish.colourB == committedFinish.colourB);

    // The centre is colour_b.
    steps = b.processor.getNumUndoSteps();
    CHECK (panel.pickPaint (Paint::body, juce::Colour (0xffc0c0c0)));
    CHECK (b.processor.getCurrentGuitar().finish.colourB == "#C0C0C0");
    CHECK (b.processor.getNumUndoSteps() == steps + 1);

    // A preset swatch: type and colours, one entry.
    steps = b.processor.getNumUndoSteps();
    CHECK (panel.applyFinishPreset ("seafoam"));
    CHECK (b.processor.getCurrentGuitar().finish.type == "solid");
    CHECK (b.processor.getCurrentGuitar().finish.colourA == "#98C9B0");
    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK (panel.paintTargets() == juce::Array<Paint> ({ Paint::body }));
    CHECK (sceneHasFill (ill.getScene(), juce::Colour (0xff98c9b0)));

    // Plastics: the pickguard, and whatever else the renderer draws in plastic.
    ill.select (GuitarRegion::pickguard);
    CHECK (panel.paintTargets() == juce::Array<Paint> ({ Paint::plastics }));
    steps = b.processor.getNumUndoSteps();
    CHECK (panel.pickPaint (Paint::plastics, juce::Colour (0xffd3e2c6)));
    CHECK (b.processor.getCurrentGuitar().finish.plasticColour == "#D3E2C6");
    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK (sceneHasFill (ill.getScene(), juce::Colour (0xffd3e2c6)));
    CHECK (panel.getPaintColour (Paint::plastics) == juce::Colour (0xffd3e2c6));

    {
        ill.select (GuitarRegion::body);
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-guitar-renders");
        dir.createDirectory();
        juce::FileOutputStream out (dir.getChildFile ("workshop-paint-inspector.png"));
        if (out.openedOk()) { out.setPosition (0); out.truncate(); juce::PNGImageFormat().writeImageToStream (render (panel), out); }

        // The picker itself, as the call-out would show it.
        ill.select (GuitarRegion::pickguard);
    }

    // Paint only: the engine's guitar is the same and the refined parameter stands.
    CHECK (mapSpec (b.processor.getCurrentGuitar()) == derivedBefore);
    CHECK (material->getValue() == refined);

    // Undo walks the paint back, one pick at a time.
    b.processor.undo();
    CHECK (b.processor.getCurrentGuitar().finish.plasticColour.isEmpty());
    b.processor.undo();
    CHECK (b.processor.getCurrentGuitar().finish.type == "burst");

    // The Advanced / Easy illustrations rebuild from the committed guitar's key.
    const auto key = GuitarRenderer::keyFor (b.processor.getCurrentGuitar(), {});
    CHECK (panel.pickPaint (Paint::plastics, juce::Colour (0xff101010)));
    CHECK (GuitarRenderer::keyFor (b.processor.getCurrentGuitar(), {}) != key);
}

LUTHIER_TEST (WorkshopPanel, paintSavesWithTheGuitarAndOlderFilesKeepTheirFinish)
{
    PartLibrary library;
    library.refreshFrom (PartLibrary::getFactoryPartsFolder(), juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-no-user-parts"));

    WorkshopGuitar g;
    PartLibrary::LoadReport report;
    CHECK (library.loadGuitar (PartLibrary::getFactoryGuitarsFolder().getChildFile ("Electric/Vintage Single-Cut.luthierguitar"), g, report));

    // A file without plastic_color: the pickguard part's own colour, and nothing written back.
    CHECK (g.finish.plasticColour.isEmpty());
    CHECK (! g.toVar().getProperty ("parts", {}).getProperty ("finish", {}).hasProperty ("plastic_color"));
    CHECK (g.finish.type == "burst" && g.finish.colourA == "#7A2E1B");

    // Painted, it round-trips by reference and embedded (the preset's override).
    g.finish.colourA = "#1E3E62";
    g.finish.plasticColour = "#D3E2C6";

    for (const auto& json : { g.toVar(), g.toEmbeddedVar() })
    {
        WorkshopGuitar back;
        PartLibrary::LoadReport r;
        CHECK (library.buildGuitar (juce::JSON::parse (juce::JSON::toString (json)), back, r));
        CHECK (back.finish.plasticColour == "#D3E2C6");
        CHECK (back.finish.colourA == "#1E3E62");
        CHECK (back == g);
    }

    // Paint is all that differs.
    WorkshopGuitar original;
    library.loadGuitar (PartLibrary::getFactoryGuitarsFolder().getChildFile ("Electric/Vintage Single-Cut.luthierguitar"), original, report);
    CHECK (g.differsOnlyInPaint (original));
    g.finish.gloss = 0.2;
    CHECK (! g.differsOnlyInPaint (original));   // gloss reaches the sound (part-acoustics 9)
}

LUTHIER_TEST (WorkshopPanel, theGradientPickerPreviewsAndClosingCommitsOnce)
{
    GuitarBench b (PartLibrary::getFactoryGuitarsFolder().getChildFile ("Electric/Vintage Single-Cut.luthierguitar"));
    auto& panel = *b.panel;
    using Paint = WorkshopPanel::Paint;

    panel.getIllustration().select (GuitarRegion::body);
    const int steps = b.processor.getNumUndoSteps();

    // What the call-out holds: the ColourSelector (gradient square and hue strip).
    auto picker = panel.createColourPicker (Paint::body);

    std::function<juce::ColourSelector* (juce::Component&)> findSelector = [&] (juce::Component& c) -> juce::ColourSelector*
    {
        if (auto* s = dynamic_cast<juce::ColourSelector*> (&c))
            return s;
        for (auto* child : c.getChildren())
            if (auto* s = findSelector (*child))
                return s;
        return nullptr;
    };

    auto* selector = findSelector (*picker);
    CHECK (selector != nullptr);

    if (selector == nullptr)
        return;

    // A burst's centre is what it starts on.
    CHECK (selector->getCurrentColour() == panel.getPaintColour (Paint::body));

    for (auto c : { juce::Colour (0xff808080), juce::Colour (0xff90a0b0), juce::Colour (0xff4a7ab0) })
    {
        selector->setCurrentColour (c);
        selector->dispatchPendingMessages();
    }

    CHECK (b.processor.getNumUndoSteps() == steps);
    CHECK (b.processor.getBench().current().finish.colourB == "#4A7AB0");

    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-guitar-renders");
        dir.createDirectory();
        picker->setLookAndFeel (&panel.getLookAndFeel());
        juce::FileOutputStream out (dir.getChildFile ("workshop-colour-picker.png"));
        if (out.openedOk()) { out.setPosition (0); out.truncate(); juce::PNGImageFormat().writeImageToStream (render (*picker), out); }
        picker->setLookAndFeel (nullptr);
    }

    // Closing the call-out is the committed pick: one entry.
    picker.reset();
    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK (b.processor.getCurrentGuitar().finish.colourB == "#4A7AB0");
    CHECK (! b.processor.getBench().isInGesture());
    CHECK_MSG (b.processor.getUndoDescription().contains ("body colour"), b.processor.getUndoDescription());
}
