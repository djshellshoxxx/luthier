/*  The Workshop bench's remainder (TODO 7, VISUAL-WORKSHOP-QA): per-string
    overrides (workshop-ui.md 3.3, guitar-illustration.md 10 and 13.2), the nut
    slot drag and the pick / slide / capo on the bench (workshop-ui.md 4),
    the WORKSHOP tab padlock (gui-integration.md 21), the spectrum summary for
    screen readers (workshop-ui.md 10) and the slide material in the engine. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/WorkshopPanel.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/RangesUi.h"
#include "../UI/Overlays.h"
#include "../PluginEditor.h"
#include "../Accessibility/Accessibility.h"
#include "../UI/Guitar/GuitarRenderer.h"
#include "../Model/Workshop/PartAcoustics.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    struct Bench
    {
        LuthierAudioProcessor processor;

        Bench (GuitarType type = GuitarType::LesPaul)
        {
            processor.prepareToPlay (48000.0, 512);
            loadType (type);
        }

        void loadType (GuitarType type)
        {
            auto* p = processor.getState().getParameter (ParamIDs::guitarType);
            p->setValueNotifyingHost (p->convertTo0to1 ((float) type));
            processor.getParameterBridge().applyAllNow();
        }

        void setPlain (const char* id, float plain)
        {
            if (auto* p = processor.getState().getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (plain));
            processor.getParameterBridge().applyAllNow();
        }

        WorkshopBench& bench() { return processor.getBench(); }
        const WorkshopGuitar& guitar() { return processor.getCurrentGuitar(); }

        std::vector<float> render (std::initializer_list<int> notes = { 40, 47, 52, 55, 59, 64 })
        {
            processor.getEngine().reset();
            juce::AudioBuffer<float> buffer (2, 512);
            std::vector<float> out;

            for (int block = 0; block < 24; ++block)
            {
                juce::MidiBuffer midi;
                if (block == 0)
                    for (int note : notes)
                        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);

                buffer.clear();
                processor.processBlock (buffer, midi);
                out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 512);
            }

            return out;
        }
    };

    double difference (const std::vector<float>& a, const std::vector<float>& b)
    {
        double d = 0.0;
        for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i)
            d += std::abs ((double) a[i] - (double) b[i]);
        return d;
    }

    juce::uint64 digest (const juce::Image& image)
    {
        juce::uint64 sum = 0;
        const juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < pixels.height; ++y)
            for (int x = 0; x < pixels.width; ++x)
                sum += (juce::uint64) pixels.getPixelColour (x, y).getARGB() * (juce::uint64) (x + 1);

        return sum;
    }
}

//==============================================================================
/*  3.3: a heavier third on its own - one undo entry in real units, the three
    feedbacks (drawn, read, heard), and the file round trip. */
LUTHIER_TEST (WorkshopStrings, aPerStringOverrideIsSeenReadHeardAndOneEntry)
{
    Bench b;
    const int s = 2;   // the G, engine index 2 (people call it string 3)

    const auto before = b.render();
    // A thousandth of an inch is under a pixel at the fit zoom, so the picture
    // is compared at the illustration's largest test size (section 19: 3072).
    const auto imageBefore = GuitarRenderer::render (b.guitar(), 3072, 1536);
    const auto described = b.bench().describeString (b.guitar(), s);
    const int steps = b.processor.getNumUndoSteps();

    StringOverride o;
    o.gaugeIn = 0.018;
    o.wound = 0;
    CHECK (b.bench().setStringOverride (s, o));

    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK_MSG (b.processor.getUndoDescription() == "Set string 3 to 0.018 plain (was " + described + ")",
               b.processor.getUndoDescription());

    // Read: the engine's gauge; drawn: the picture; heard: the render.
    CHECK (std::abs (b.processor.getEngine().getCustomStringGauge (s) - 0.018) < 1.0e-9);
    CHECK (digest (GuitarRenderer::render (b.guitar(), 3072, 1536)) != digest (imageBefore));
    CHECK (difference (b.render(), before) > 1.0e-3);

    // The same value again is not a change.
    CHECK (! b.bench().setStringOverride (s, o));

    // The file keeps it: per_string_override, people's numbering.
    const auto json = b.guitar().toVar();
    const auto list = json.getProperty ("parts", {}).getProperty ("strings", {}).getProperty ("per_string_override", {});
    CHECK (list.isArray() && list.size() == 1);
    CHECK ((int) list[0].getProperty ("string", 0) == 3);

    WorkshopGuitar reloaded;
    PartLibrary::LoadReport report;
    CHECK (b.processor.getPartLibrary().buildGuitar (b.guitar().toEmbeddedVar(), reloaded, report));
    CHECK (reloaded == b.guitar());

    // Undo puts the set's string back.
    b.processor.undo();
    CHECK (! b.guitar().stringOverrides[(size_t) s].isSet());
}

/*  guitar-illustration.md 10: an overridden string is drawn in its own
    material colour, independently of the set. */
LUTHIER_TEST (WorkshopStrings, anOverriddenStringIsDrawnInItsOwnMaterial)
{
    Bench b;
    auto g = b.guitar();
    g.stringOverrides[5].material = "phosphor_bronze";
    g.stringOverrides[5].wound = 1;

    const auto scene = GuitarRenderer::build (g);
    const GuitarScene::StringLine* low = nullptr;
    const GuitarScene::StringLine* next = nullptr;

    for (auto& line : scene.strings)
    {
        if (line.index == 5) low = &line;
        if (line.index == 4) next = &line;
    }

    CHECK (low != nullptr && next != nullptr);

    if (low != nullptr && next != nullptr)
    {
        CHECK (low->colour == GuitarRenderer::stringColour ("phosphor_bronze", true, false));
        CHECK (next->colour != low->colour);
    }

    CHECK (scene.stringDescriptions.size() >= 6);
    CHECK (scene.stringDescriptions[5].contains ("phosphor bronze wound") && scene.stringDescriptions[5].contains ("overridden"));

    // And the engine hears a bronze low E, not the set's nickel one.
    const auto d = mapSpec (g);
    CHECK (d.stringMaterialOverride[5] == (int) StringMaterial::PhosphorBronze);
    CHECK (d.stringMaterialOverride[4] == -1);
}

/*  13.2: a strings card onto a selected string overrides that string only; a
    card anywhere else replaces the set and its overrides. */
LUTHIER_TEST (WorkshopStrings, aCardOntoAStringOverridesItAndASetClearsOverrides)
{
    Bench b;
    WorkshopPanel panel (b.processor);
    panel.setSize (1100, 700);
    panel.showCategory ("Strings");

    int other = -1;
    for (int i = 0; i < panel.getDrawerParts().size(); ++i)
        if (panel.getDrawerParts()[i]->name != b.guitar().get (GuitarSlot::strings)->name)
        {
            other = i;
            break;
        }

    CHECK (other >= 0);
    if (other < 0)
        return;

    const auto setName = b.guitar().get (GuitarSlot::strings)->name;
    panel.getIllustration().select (GuitarRegion::strings, 5);
    panel.clickCard (other, true);

    CHECK (b.guitar().get (GuitarSlot::strings)->name == setName);
    CHECK (b.guitar().stringOverrides[5].isSet());
    CHECK (panel.getInspectorLines().joinIntoString ("\n").contains ("(override)"));

    // Editing the override from the inspector: "18" means 0.018".
    CHECK (panel.editInspectorField (juce::String (WorkshopPanel::kStringFieldPrefix) + "gauge", "18"));
    CHECK (std::abs (b.guitar().stringOverrides[5].gaugeIn - 0.018) < 1.0e-9);

    // A plain click fits the whole set, and the override goes with the old set.
    panel.clickCard (other, false);
    CHECK (b.guitar().get (GuitarSlot::strings)->name != setName);
    CHECK (! b.guitar().stringOverrides[5].isSet());
}

//==============================================================================
namespace
{
    struct BenchPanel : Bench
    {
        std::unique_ptr<WorkshopPanel> panel;

        explicit BenchPanel (GuitarType type = GuitarType::LesPaul) : Bench (type)
        {
            panel = std::make_unique<WorkshopPanel> (processor);
            panel->setVisible (true);
            panel->setSize (1200, 760);
        }

        BenchIllustration& ill() { return panel->getIllustration(); }

        juce::MouseEvent event (juce::Point<float> p, juce::ModifierKeys mods = {})
        {
            auto& target = ill();
            auto source = juce::Desktop::getInstance().getMainMouseSource();
            return juce::MouseEvent (source, p, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                     &target, &target, juce::Time::getCurrentTime(), p,
                                     juce::Time::getCurrentTime(), 1, false);
        }

        double plain (const char* id)
        {
            auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id));
            return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.0;
        }

        /** Pixels per mm along the strings; negative, since X runs right to left on screen. */
        float pxPerMm() { return ill().toPx ({ 1.0f, 0.0f }).x - ill().toPx ({ 0.0f, 0.0f }).x; }
    };
}

/*  workshop-ui.md 4: a nut slot is dragged down per string, 0.05 mm snap,
    0 - 1.2 mm, one undo entry that names the string and both depths; the
    engine's nut depth parameter follows (the three feedbacks). */
LUTHIER_TEST (WorkshopNut, aSlotDragIsOneEntryInRealUnitsAndReachesTheEngine)
{
    BenchPanel b;
    auto& ill = b.ill();
    const int steps = b.processor.getNumUndoSteps();

    // The nut's slot for the low E (engine string 5).
    const auto nut = ill.toPx (ill.getScene().nutPoints[5]);
    ill.mouseDown (b.event (nut));
    CHECK (ill.getSelected() == GuitarRegion::nut && ill.getSelectedString() == 5);

    const double start = b.plain ("setup_nut_depth_6");

    for (int i = 1; i <= 20; ++i)
        ill.mouseDrag (b.event (nut + juce::Point<float> (0.0f, (float) i)));   // 20 px down: 0.2 mm deeper

    ill.mouseUp (b.event (nut + juce::Point<float> (0.0f, 20.0f)));

    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK_MSG (b.processor.getUndoDescription().startsWith ("Set string 6 nut slot"), b.processor.getUndoDescription());
    const auto& depths = b.guitar().setup.nutSlotDepthsMm;
    CHECK (depths.size() > 5 && std::abs (depths[5] - (start + 0.2)) < 0.026);
    CHECK_NEAR (b.plain ("setup_nut_depth_6"), depths[5], 1.0e-3);

    // Keyboard parity: Down deepens by one snap.
    const double now = depths[5];
    ill.keyPressed (juce::KeyPress (juce::KeyPress::downKey));
    CHECK_NEAR (b.guitar().setup.nutSlotDepthsMm[5], juce::jmin (1.2, now + 0.05), 1.0e-6);

    // And it never cuts past 1.2 mm.
    for (int i = 0; i < 40; ++i)
        ill.keyPressed (juce::KeyPress (juce::KeyPress::downKey));
    CHECK_NEAR (b.guitar().setup.nutSlotDepthsMm[5], 1.2, 1.0e-6);
}

/*  workshop-ui.md 4 and gui-integration.md 21: the pick lies on the strings at
    its position and angle, is dragged along the string axis (1 mm snap) and
    turned at its corner (1 degree), one undo entry per drag, and its
    accessible sentence follows guitar-illustration.md 16. */
LUTHIER_TEST (WorkshopAccessories, thePickIsDraggedAndTurnedOnTheBench)
{
    BenchPanel b;
    auto& ill = b.ill();
    b.panel->showCategory ("Pick");
    CHECK (ill.isPickShown());

    const auto o = ill.currentOverlay();
    CHECK (o.pickPositionMm > 0.0f);

    const auto centre = ill.toPx (GuitarRenderer::pickPath (ill.getScene(), o.pickPositionMm, o.pickAngleDeg, o.pickSizeMm)
                                      .getBounds().getCentre());
    const int steps = b.processor.getNumUndoSteps();

    ill.mouseDown (b.event (centre));
    CHECK (ill.getSelected() == GuitarRegion::pick);

    // 12 mm toward the headstock (left on screen), in steps.
    for (int i = 1; i <= 12; ++i)
        ill.mouseDrag (b.event (centre + juce::Point<float> (b.pxPerMm() * (float) i, 0.0f)));
    ill.mouseUp (b.event (centre));

    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    const double scale = ill.getScene().scaleMm;
    CHECK_NEAR (b.plain (ParamIDs::pluckPosition) * scale, (double) std::round (o.pickPositionMm + 12.0f), 1.5);

    // Turned at its corner handle.
    const auto o2 = ill.currentOverlay();
    const auto handle = ill.toPx (GuitarRenderer::pickHandle (ill.getScene(), o2.pickPositionMm, o2.pickAngleDeg, o2.pickSizeMm));
    const double angleBefore = Parameters::pickAngleDegrees (b.plain (ParamIDs::pickAngle));
    ill.mouseDown (b.event (handle));
    ill.mouseDrag (b.event (handle + juce::Point<float> (0.0f, 25.0f)));
    ill.mouseUp (b.event (handle + juce::Point<float> (0.0f, 25.0f)));

    const double angleAfter = Parameters::pickAngleDegrees (b.plain (ParamIDs::pickAngle));
    CHECK (std::abs (angleAfter - angleBefore) >= 1.0);
    CHECK (b.processor.getNumUndoSteps() == steps + 2);

    // Section 16's sentence.
    const auto sentence = ill.describeAccessory (GuitarRegion::pick);
    CHECK_MSG (sentence.startsWith ("Pick: ") && sentence.contains ("mm from saddle, angle ") && sentence.endsWith (" degrees."), sentence);

    // Keyboard parity: Left moves it 1 mm further from the saddle.
    const double before = b.plain (ParamIDs::pluckPosition) * scale;
    ill.keyPressed (juce::KeyPress (juce::KeyPress::leftKey));
    CHECK_NEAR (b.plain (ParamIDs::pluckPosition) * scale, before + 1.0, 0.02);
}

/*  The capo (TODO G / workshop-ui.md 4): drawn at its fret on the illustration
    and on the bench, dragged along the neck a fret at a time (0 - 12), and
    taken off by dragging it past the nut. */
LUTHIER_TEST (WorkshopAccessories, theCapoIsDrawnAndDraggedByFrets)
{
    BenchPanel b;
    b.setPlain (ParamIDs::capoFret, 3.0f);
    auto& ill = b.ill();

    // Drawn: the renderer's overlay changes with it, on any illustration.
    const auto& scene = ill.getScene();
    juce::Image without (juce::Image::ARGB, 800, 400, true, juce::SoftwareImageType()), with (without.createCopy());
    const auto t = GuitarRenderer::fitTransform (scene, { 0.0f, 0.0f, 800.0f, 400.0f });
    {
        juce::Graphics g (without);
        GuitarRenderer::paintOverlay (g, scene, t, GuitarOverlay {});
    }
    {
        GuitarOverlay o;
        o.capoFret = 3;
        juce::Graphics g (with);
        GuitarRenderer::paintOverlay (g, scene, t, o);
    }
    CHECK (digest (with) != digest (without));

    CHECK (ill.currentOverlay().capoFret == 3);
    const auto capo = ill.toPx (GuitarRenderer::capoPath (scene, 3).getBounds().getCentre());
    const int steps = b.processor.getNumUndoSteps();

    ill.mouseDown (b.event (capo));
    CHECK (ill.getSelected() == GuitarRegion::capo);

    // To just behind fret 5.
    const auto fret5 = ill.toPx (scene.stringAt (0, 4.7f));
    ill.mouseDrag (b.event ({ fret5.x, capo.y }));
    ill.mouseUp (b.event ({ fret5.x, capo.y }));

    CHECK_MSG (juce::roundToInt (b.plain (ParamIDs::capoFret)) == 5, juce::String (b.plain (ParamIDs::capoFret)));
    CHECK (b.processor.getNumUndoSteps() == steps + 1);

    // Keyboard: Left moves it a fret toward the nut.
    ill.keyPressed (juce::KeyPress (juce::KeyPress::leftKey));
    CHECK (juce::roundToInt (b.plain (ParamIDs::capoFret)) == 4);

    // Off the end: past the nut takes it off.
    const auto capoNow = ill.toPx (GuitarRenderer::capoPath (scene, 4).getBounds().getCentre());
    const auto pastNut = ill.toPx ({ (float) scene.scaleMm + 30.0f, 0.0f });
    ill.mouseDown (b.event (capoNow));
    ill.mouseDrag (b.event ({ pastNut.x, capoNow.y }));
    ill.mouseUp (b.event ({ pastNut.x, capoNow.y }));
    CHECK (juce::roundToInt (b.plain (ParamIDs::capoFret)) == 0);
}

/*  Slide Mode on: the bar shows on the bench in its material's colour at its
    slant; turning it at its end handle is the slant parameter, one entry. */
LUTHIER_TEST (WorkshopAccessories, theSlideTurnsOnTheBenchAndItsMaterialIsPlayed)
{
    BenchPanel b;
    b.setPlain (ParamIDs::slideGuitar, 1.0f);
    auto& ill = b.ill();

    auto o = ill.currentOverlay();
    CHECK (o.slideFret >= 0.0f);

    const auto handle = ill.toPx (GuitarRenderer::slideHandle (ill.getScene(), o.slideFret, o.slideSlantDeg));
    const int steps = b.processor.getNumUndoSteps();
    ill.mouseDown (b.event (handle));
    CHECK (ill.getSelected() == GuitarRegion::slideBar);
    ill.mouseDrag (b.event (handle + juce::Point<float> (30.0f, 0.0f)));
    ill.mouseUp (b.event (handle + juce::Point<float> (30.0f, 0.0f)));

    CHECK (std::abs (b.plain (ParamIDs::slideSlant)) >= 1.0);
    CHECK (b.processor.getNumUndoSteps() == steps + 1);
    CHECK (ill.describeAccessory (GuitarRegion::slideBar).startsWith ("Slide: "));

    // The fitted slide is the engine's bar (TODO 5b): brass, 150 g.
    PartPtr brass;
    for (const auto& p : b.processor.getPartLibrary().getParts (PartType::slide))
        if (p->text ("material") == "brass")
            brass = p;

    CHECK (brass != nullptr);
    if (brass == nullptr)
        return;

    const auto glassRender = b.render ({ 52 });
    CHECK (b.bench().fitAccessory (brass));
    CHECK (b.processor.getEngine().getSlideEngine().getBar().material == SlideMaterial::brass);
    CHECK_NEAR (b.processor.getEngine().getSlideEngine().getBar().massGrams, 150.0, 1.0e-9);
    CHECK (difference (b.render ({ 52 }), glassRender) > 1.0e-4);

    // It travels with the preset's guitar block, and undo takes it off again.
    CHECK (b.processor.getGuitarBlock().getProperty ("slide", {}).toString() == brass->name);
    b.processor.undo();
    CHECK (b.processor.getEngine().getSlideEngine().getBar().material == SlideMaterial::glass);
}

//==============================================================================
/*  gui-integration.md 21: the WORKSHOP tab carries the range padlock for the
    families its controls use (the setup strip's buzz, the pick, the slide);
    guitar-illustration.md 19: pickup height stops at 0.8 mm unless ranges are
    unlocked. */
LUTHIER_TEST (WorkshopRanges, theTabCarriesAPadlockAndHeightsStopAtStock)
{
    Bench b;
    AdvancedPanel panel (b.processor);
    panel.setSize (1600, 900);

    RangesUi::RangeTabButton* tab = nullptr;
    for (auto* child : panel.getChildren())
        if (auto* t = dynamic_cast<RangesUi::RangeTabButton*> (child))
            if (t->getButtonText() == "WORKSHOP")
                tab = t;

    CHECK_MSG (tab != nullptr, "the WORKSHOP tab has no range padlock");

    if (tab != nullptr)
    {
        CHECK (tab->getPadlockState() == 0);

        RangeState unlocked;
        unlocked.setFamilyAdvanced (RangeFamily::buzz, true);
        b.processor.changeRanges (unlocked, "test");
        CHECK (tab->getPadlockState() == 2);

        b.processor.changeRanges (RangeState(), "test");
        CHECK (tab->getPadlockState() == 0);
    }

    b.bench().setPickupHeights (0, 0.1, 0.1);
    CHECK_NEAR (b.guitar().placements[0].heightTrebleMm, 0.8, 1.0e-9);

    RangeState unlocked;
    unlocked.setFamilyAdvanced (RangeFamily::buzz, true);
    b.processor.changeRanges (unlocked, "test");
    b.bench().setPickupHeights (0, 0.1, 0.1);
    CHECK_NEAR (b.guitar().placements[0].heightTrebleMm, 0.5, 1.0e-9);
}

//==============================================================================
/*  workshop-ui.md 10: the spectrum delta is announced to a screen reader as its
    summary sentence, not left as a curve. */
LUTHIER_TEST (WorkshopSpectrum, theSummaryIsAnnouncedForScreenReaders)
{
    BenchPanel b;
    auto& panel = *b.panel;
    panel.showCategory ("Pickups");

    // Alt-hover a different pickup card: an audition, which requests a delta.
    int other = -1;
    for (int i = 0; i < panel.getDrawerParts().size(); ++i)
        if (panel.getDrawerParts()[i]->name.startsWith ("T-Style Bridge"))
            other = i;

    CHECK (other >= 0);
    if (other < 0)
        return;

    panel.getIllustration().select (GuitarRegion::pickupBridge);
    panel.hoverCard (other, true);
    CHECK (panel.waitForSpectrum (4000));
    panel.hoverCard (-1, false);

    CHECK (panel.getSpectrumSummary().isNotEmpty());
    CHECK (panel.getLastAnnouncement() == panel.getSpectrumSummary());
    CHECK (panel.getDescription().contains (panel.getSpectrumSummary()));
}

//==============================================================================
/*  gui-integration.md 6 at the editor: in Easy mode the header wrench opens the
    bench as an overlay and Escape closes it; W toggles it; in Advanced mode the
    wrench shows the WORKSHOP tab. */
LUTHIER_TEST (WorkshopEditor, theWrenchOpensTheBenchInEasyModeAndTheTabInAdvanced)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    CHECK (editor != nullptr);
    if (editor == nullptr)
        return;

    editor->setVisible (true);
    editor->setSize (1200, 720);

    auto findAll = [] (juce::Component& root, auto* type, auto& out, auto& self) -> void
    {
        using T = std::remove_pointer_t<decltype (type)>;
        for (auto* child : root.getChildren())
        {
            if (auto* t = dynamic_cast<T*> (child))
                out.add (t);
            self (*child, type, out, self);
        }
    };

    juce::Array<OverlayHost*> hosts;
    findAll (*editor, (OverlayHost*) nullptr, hosts, findAll);
    CHECK (hosts.size() == 1);
    if (hosts.isEmpty())
        return;

    auto* host = hosts.getFirst();

    juce::Array<juce::TextButton*> buttons;
    findAll (*editor, (juce::TextButton*) nullptr, buttons, findAll);
    juce::TextButton* wrench = nullptr;
    for (auto* b : buttons)
        if (b->getButtonText() == "Workshop")
            wrench = b;

    CHECK_MSG (wrench != nullptr, "no Workshop button in the header");
    if (wrench == nullptr)
        return;

    CHECK (wrench->onClick != nullptr);
    if (wrench->onClick) wrench->onClick();

    auto* overlay = dynamic_cast<WorkshopOverlay*> (host->getCurrentOverlay());
    CHECK_MSG (overlay != nullptr, "the wrench opened no Workshop overlay in Easy mode");

    if (overlay != nullptr)
    {
        CHECK (overlay->getPanel().isVisible() && overlay->getPanel().getWidth() > 600);
        CHECK (editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
        CHECK (! host->isShowingOverlay());
    }

    // W toggles it (gui-integration.md 17).
    const auto* w = AccessibilitySettings::get().findShortcut ("toggleWorkshop");
    CHECK (w != nullptr);
    if (w != nullptr)
    {
        CHECK (editor->keyPressed (w->key));
        CHECK (dynamic_cast<WorkshopOverlay*> (host->getCurrentOverlay()) != nullptr);
        CHECK (editor->keyPressed (w->key));
        CHECK (! host->isShowingOverlay());
    }

    // Advanced: the wrench is the WORKSHOP tab, not an overlay.
    CHECK (editor->keyPressed (AccessibilitySettings::get().findShortcut ("toggleAdvanced")->key));
    juce::Array<AdvancedPanel*> panels;
    findAll (*editor, (AdvancedPanel*) nullptr, panels, findAll);
    CHECK (panels.size() == 1);

    if (panels.size() == 1)
    {
        panels.getFirst()->setWorkspaceTab (1);
        CHECK (wrench->onClick != nullptr);
    if (wrench->onClick) wrench->onClick();
        CHECK (panels.getFirst()->isWorkshopShowing());
        CHECK (! host->isShowingOverlay());
    }
}

//==============================================================================
/*  workshop-ui.md 1: 900 points and up, the inspector is a column; narrower, a
    drawer that opens beside the illustration with a selection; under 700 the
    categories are a dropdown. Nothing hangs outside the bench at any width. */
LUTHIER_TEST (WorkshopLayout, theBenchCollapsesItsInspectorAndDrawerWhenNarrow)
{
    Bench b;
    WorkshopPanel panel (b.processor);
    panel.setVisible (true);

    auto everythingInside = [&panel]
    {
        for (auto* child : panel.getChildren())
            if (child->isVisible() && ! panel.getLocalBounds().contains (child->getBounds()))
                return false;
        return true;
    };

    panel.setSize (1200, 760);
    CHECK (! panel.isInspectorCollapsed() && panel.isInspectorShowing() && ! panel.areCategoriesADropdown());
    CHECK (everythingInside());

    panel.setSize (800, 760);
    CHECK (panel.isInspectorCollapsed() && ! panel.isInspectorShowing() && ! panel.areCategoriesADropdown());
    panel.getIllustration().select (GuitarRegion::bridge, 0);
    CHECK (panel.isInspectorShowing());
    CHECK (panel.getInspectorTitle().startsWith ("Bridge"));
    CHECK (everythingInside());
    panel.getIllustration().select (GuitarRegion::none);
    CHECK (! panel.isInspectorShowing());

    panel.setSize (660, 700);
    CHECK (panel.areCategoriesADropdown());
    CHECK (everythingInside());
}
