/*  The Workshop bench's surface: workshop-ui.md 3, 4, 9, 10 and 11. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/WorkshopPanel.h"
#include "../UI/AdvancedPanel.h"

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
            auto& source = juce::Desktop::getInstance().getMainMouseSource();
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
    CHECK (b.processor.getUndoDescription() == "Change guitar family");

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
