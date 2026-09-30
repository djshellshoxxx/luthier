/*  The Techniques GUI (gui-techniques-updates.md 12), the undo classes
    (engine-technique-layer.md 8) and the preset browser chip (7).
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/EasyPanel.h"
#include "../UI/Overlays.h"
#include "../Presets/PresetLibrary.h"   // FEAT-BROWSER
#include "../UI/UiPreferences.h"
#include "../UI/FretboardComponent.h"
#include "../UI/Techniques/TechniquesPanel.h"
#include "../UI/Techniques/TechniquePillRow.h"
#include "../UI/Techniques/TechniqueOverlay.h"
#include "../UI/Techniques/TechniqueMirrors.h"
#include "../UI/CharacterPanel.h"
#include "../Accessibility/Accessibility.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    struct PreservedPreferences
    {
        PreservedPreferences()
            : file (UiPreferences::getConfigFile()),
              existed (file.existsAsFile()),
              contents (existed ? file.loadFileAsString() : juce::String())
        {
        }

        ~PreservedPreferences()
        {
            if (existed)
                file.replaceWithText (contents);
            else
                file.deleteFile();

            UiPreferences::get().reset();
            UiPreferences::get().load();
        }

        juce::File file;
        bool existed;
        juce::String contents;
    };

    std::unique_ptr<LuthierAudioProcessor> makeProcessor()
    {
        auto p = std::make_unique<LuthierAudioProcessor>();
        p->prepareToPlay (kSr, kBlock);
        return p;
    }

    float plainOf (LuthierAudioProcessor& p, const juce::String& id)
    {
        return TechniqueUndo::getPlain (p, id);
    }

    template <typename T>
    void collect (juce::Component& root, juce::Array<T*>& found)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* match = dynamic_cast<T*> (child))
                found.add (match);

            collect (*child, found);
        }
    }

    juce::StringArray attachedIds (juce::Component& root)
    {
        juce::StringArray ids;
        juce::Array<LearnTarget*> targets;

        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            for (auto* child : c.getChildren())
            {
                if (auto* t = dynamic_cast<LearnTarget*> (child))
                    ids.addIfNotAlreadyThere (t->getLearnParameterId());

                walk (*child);
            }
        };

        walk (root);
        return ids;
    }
}

//==============================================================================
/*  0.2 / 1: the tab sits between CONTROLLERS and HELP, with seven sub-tabs in
    the rail's order. */
LUTHIER_TEST (TechniquesUi, theTabAndItsRail)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor();

    AdvancedPanel panel (*processor);
    panel.setSize (1600, 900);

    const int n = panel.getNumWorkspaceTabs();
    CHECK (panel.getWorkspaceTabName (n - 1) == "HELP");
    CHECK (panel.getWorkspaceTabName (n - 2) == "TECHNIQUES");
    CHECK (panel.getWorkspaceTabName (n - 3) == "CONTROLLERS");
    CHECK (panel.setWorkspaceTabNamed ("TECHNIQUES"));
    CHECK (panel.getTechniquesPanel() != nullptr);

    const char* const rail[] = { "SCRAPE", "SLIDE", "SLAP", "MUTE", "TAP", "BEND", "CASCADE" };

    for (int i = 0; i < TechniquesPanel::kNumSubTabs; ++i)
        CHECK (juce::String (TechniquesPanel::getSubTabName (i)) == rail[i]);
}

/*  12: "Every sub-tab renders with correct controls at 1280x800." Each page
    lays out with its controls inside it, and carries the parameters its spec
    names - SCRAPE and SLAP every existing scrape_* and slap_* / pop_* /
    ghost_* / double_thump_* parameter. */
LUTHIER_TEST (TechniquesUi, everySubTabRendersItsControls)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor();

    AdvancedPanel advanced (*processor);
    advanced.setSize (1280, 800);
    advanced.setWorkspaceTabNamed ("TECHNIQUES");
    auto& techniques = *advanced.getTechniquesPanel();

    juce::StringArray all;

    for (auto* p : processor->getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            all.add (withId->paramID);

    auto idsWithPrefix = [&all] (std::initializer_list<const char*> prefixes)
    {
        juce::StringArray ids;

        for (const auto& id : all)
            for (auto prefix : prefixes)
                if (id.startsWith (prefix))
                    ids.add (id);

        return ids;
    };

    const juce::StringArray expected[] =
    {
        idsWithPrefix ({ "scrape_" }),
        idsWithPrefix ({ "slide_pos", "slide_slant_", "slide_pressure_", "slide_contact", "slide_speed", "slide_auto", "slide_gesture" }),
        idsWithPrefix ({ "slap_", "pop_", "ghost_", "double_thump_" }),
        idsWithPrefix ({ "mute_" }),
        idsWithPrefix ({ "tap_" }),
        idsWithPrefix ({ "bend_" }),
    };

    for (int i = 0; i < TechniquesPanel::kNumSubTabs; ++i)
    {
        techniques.showSubTab (i);
        auto* page = techniques.getPage (i);
        CHECK (page != nullptr && page->isVisible());

        if (page == nullptr)
            continue;

        // Rendered: something laid out, nothing outside the page's width.
        int laidOut = 0;

        for (auto* child : page->getChildren())
        {
            if (! child->isVisible() || child->getBounds().isEmpty())
                continue;

            ++laidOut;
            CHECK_MSG (child->getRight() <= page->getWidth(), juce::String (TechniquesPanel::getSubTabName (i)) + " spills out");
        }

        CHECK_MSG (laidOut > 0, juce::String (TechniquesPanel::getSubTabName (i)) + " shows nothing");

        juce::Image image (juce::Image::ARGB, 1280, 800, true);
        juce::Graphics g (image);
        techniques.paintEntireComponent (g, false);

        if (i < TechniqueTable::count)
        {
            const auto shown = attachedIds (*page);
            juce::Array<StringMaskSelector*> masks;
            collect (*page, masks);

            for (const auto& id : expected[i])
            {
                // The arm switch is the pill on top; the string masks are the string selectors;
                // bend_range is the interpreter's pitch-bend range, not this tab's.
                if (id == TechniqueTable::get ((TechniqueSlot) i).armParameterId || id == "bend_range")
                    continue;

                if (id.endsWith ("_string_mask"))
                {
                    CHECK_MSG (masks.size() == 1, juce::String (TechniquesPanel::getSubTabName (i)) + " has no string selector");
                    continue;
                }

                CHECK_MSG (shown.contains (id), juce::String (TechniquesPanel::getSubTabName (i)) + " has no control for " + id);
            }

            // 1: the arm pill on top.
            CHECK (techniques.getArmPill (i) != nullptr && techniques.getArmPill (i)->isVisible());
        }
    }
}

/*  12: "Every pill in Playing strip arms / disarms its technique on single
    click." And 9: Space does the same, Enter opens the sub-tab. Each arm is
    one technique-arm undo entry, never grouped (8). */
LUTHIER_TEST (TechniquesUi, thePillsArmOnAClick)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor();

    EasyPanel easy (*processor);
    easy.setSize (1200, 720);
    auto& row = easy.getTechniquePills();

    int opened = -1;
    row.onOpenSubTab = [&opened] (int i) { opened = i; };

    for (int i = 0; i < TechniqueTable::count; ++i)
    {
        const auto slot = (TechniqueSlot) i;
        auto& pill = row.getPill (slot);
        const auto id = juce::String (TechniqueTable::get (slot).armParameterId);

        CHECK (plainOf (*processor, id) < 0.5f);

        const int before = processor->getNumUndoSteps();

        // A single click: down and up without a hold.
        const auto centre = pill.getLocalBounds().getCentre().toFloat();
        const juce::MouseEvent down (juce::Desktop::getInstance().getMainMouseSource(), centre, juce::ModifierKeys(),
                                     1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &pill, &pill, juce::Time::getCurrentTime(), centre,
                                     juce::Time::getCurrentTime(), 1, false);
        pill.mouseDown (down);
        pill.mouseUp (down);

        CHECK_MSG (plainOf (*processor, id) > 0.5f, id + " was not armed by a click");
        CHECK (pill.isArmed());
        CHECK (pill.getAccessibleName() == juce::String (TechniqueTable::get (slot).spokenName) + " technique, armed");
        CHECK (processor->getNumUndoSteps() == before + 1);
        CHECK (processor->getUndoDescription().startsWith ("Arm "));

        // Space disarms: a second, separate entry.
        CHECK (pill.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
        CHECK (plainOf (*processor, id) < 0.5f);
        CHECK (processor->getNumUndoSteps() == before + 2);
        CHECK (pill.getAccessibleName().endsWith ("not armed"));

        // Undo re-arms.
        processor->undo();
        CHECK (plainOf (*processor, id) > 0.5f);
        processor->undo();

        // Enter: the sub-tab.
        CHECK (pill.keyPressed (juce::KeyPress (juce::KeyPress::returnKey)));
        CHECK (opened == i);

        // 9: focusable, and a toggle to a screen reader.
        CHECK (pill.getWantsKeyboardFocus());
        CHECK (pill.getAccessibilityHandler() == nullptr
               || pill.getAccessibilityHandler()->getRole() == juce::AccessibilityRole::toggleButton);
    }
}

/*  12: "Long-press opens the popover; Escape closes." */
LUTHIER_TEST (TechniquesUi, holdOpensThePopoverAndEscapeClosesIt)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor();

    EasyPanel easy (*processor);
    easy.setSize (1200, 720);
    auto& row = easy.getTechniquePills();

    for (int i = 0; i < TechniqueTable::count; ++i)
    {
        const auto slot = (TechniqueSlot) i;
        row.getPill (slot).onHold (slot);   // what the timer calls after kHoldMs held

        auto* popover = row.getPopover();
        CHECK (popover != nullptr && popover->getSlot() == slot);

        if (popover == nullptr)
            continue;

        CHECK (popover->getParentComponent() != nullptr);

        // 2: "the top 3-5 controls for that technique".
        const int controls = popover->getControls().getParameterIds().size();
        CHECK_MSG (controls >= 3 && controls <= 5, juce::String (controls) + " controls in the "
                     + TechniqueTable::get (slot).name + " popover");

        // Non-modal.
        CHECK (! popover->isCurrentlyModal());

        CHECK (popover->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
        CHECK (row.getPopover() == nullptr);
    }
}

/*  2: right-click opens the full sub-tab in Advanced mode - through the
    editor, which switches mode and tab. */
LUTHIER_TEST (TechniquesUi, rightClickOpensTheSubTabInAdvanced)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor();
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
    CHECK (editor != nullptr);

    if (editor == nullptr)
        return;

    editor->setSize (1600, 900);

    juce::Array<EasyPanel*> easies;
    juce::Array<AdvancedPanel*> advanceds;
    collect (*editor, easies);
    collect (*editor, advanceds);
    CHECK (easies.size() == 1 && advanceds.size() == 1);

    if (easies.isEmpty() || advanceds.isEmpty())
        return;

    easies[0]->getTechniquePills().onOpenSubTab ((int) TechniqueSlot::bend);

    auto& advanced = *advanceds[0];
    CHECK (advanced.isVisible());
    CHECK (advanced.getWorkspaceTabName (advanced.getWorkspaceTab()) == "TECHNIQUES");
    CHECK (advanced.getTechniquesPanel()->getSubTab() == (int) TechniqueSlot::bend);
}

/*  12: "CASCADE view correctly reflects live technique arm state." And the
    live cells follow the engine's resolver. */
LUTHIER_TEST (TechniquesUi, theCascadeViewFollowsTheEngine)
{
    auto processor = makeProcessor();

    CascadeView view (*processor);
    view.setSize (500, 240);

    for (int i = 0; i < TechniqueTable::count; ++i)
        CHECK (! view.isArmedRow ((TechniqueSlot) i));

    TechniqueUndo::setArmed (*processor, TechniqueSlot::mute, true);
    TechniqueUndo::setArmed (*processor, TechniqueSlot::bend, true);
    view.refresh();
    CHECK (view.isArmedRow (TechniqueSlot::mute) && view.isArmedRow (TechniqueSlot::bend));
    CHECK (! view.isArmedRow (TechniqueSlot::tap));

    // A tap on the G shows in its cell.
    TechniqueUndo::setArmed (*processor, TechniqueSlot::tap, true);
    TechniqueUndo::setParameter (*processor, ParamIDs::tapSource, 2.0f);
    processor->getParameterBridge().applyAllNow();

    TapGesture g;
    g.stringIndex = 2;
    g.fret = 9.0;
    g.durationMs = 0.0;
    processor->getEngine().getTechniqueLayer().tap.requestGesture (g);

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer none;

    for (int b = 0; b < 3; ++b)
        processor->getEngine().processBlock (buffer, none);

    view.refresh();
    CHECK (view.isActiveCell (TechniqueSlot::tap, 2));
    CHECK (! view.isActiveCell (TechniqueSlot::tap, 3));
}

/*  12: "Fretboard overlays render within 2 ms live-overlay budget." With
    every overlay busy: taps, a bend, the mute zone and a slap. */
LUTHIER_TEST (TechniquesUi, theOverlaysDrawInsideTheirBudget)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor();
    auto& engine = processor->getEngine();

    for (int l = 0; l < TechniqueOverlay::numLayers; ++l)
        TechniqueOverlay::setLayerEnabled ((TechniqueOverlay::Layer) l, true);

    MuteSettings mute;
    mute.armed = true;
    mute.masterMode = (int) MuteType::palmHeavy;
    engine.setMuteSettings (mute);

    TapSettings tap;
    tap.armed = true;
    tap.source = TapSource::fretboard;
    engine.setTapSettings (tap);

    BendSettings bend;
    bend.armed = true;
    bend.vibratoSource = VibratoSource::off;
    engine.setBendSettings (bend);

    for (int s = 0; s < 4; ++s)
    {
        TapGesture g;
        g.stringIndex = s;
        g.fret = 7.0 + s;
        g.durationMs = 0.0;
        engine.getTechniqueLayer().tap.requestGesture (g);
    }

    NoteOnEvent e;
    e.stringIndex = 5;
    e.fretPosition = 5.0;
    e.pitchHz = engine.getTuningEngine().computeFrequency (5, 5.0, 0.0);
    engine.triggerNoteNow (e);

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::pitchWheel (1, 8192 + 4000), 0);

    for (int b = 0; b < 4; ++b)
    {
        engine.processBlock (buffer, m);
        m.clear();
    }

    FretboardComponent board (*processor);
    board.setSize (900, 160);

    // The fretboard's own timer would fill its live state; one frame of it.
    auto* overlay = board.getTechniqueOverlay();
    CHECK (overlay != nullptr);

    if (overlay == nullptr)
        return;

    overlay->refresh();

    juce::Image image (juce::Image::ARGB, 900, 160, true);
    double worst = 0.0;

    for (int frame = 0; frame < 20; ++frame)
    {
        juce::Graphics g (image);
        overlay->paintEntireComponent (g, false);
        worst = juce::jmax (worst, overlay->getLastPaintMs());
    }

    CHECK (overlay->getLastDrawnCount (TechniqueOverlay::tapMarkers) == 4);
    CHECK (overlay->getLastDrawnCount (TechniqueOverlay::muteZone) == 1);
    CHECK_MSG (worst < 2.0, "the overlays took " + juce::String (worst, 3) + " ms");

    // Each is independently toggleable.
    TechniqueOverlay::setLayerEnabled (TechniqueOverlay::tapMarkers, false);
    {
        juce::Graphics g (image);
        overlay->paintEntireComponent (g, false);
    }
    CHECK (overlay->getLastDrawnCount (TechniqueOverlay::tapMarkers) == 0);
    CHECK (overlay->getLastDrawnCount (TechniqueOverlay::muteZone) == 1);
}

/*  2 (fretboard source) / 4: a click on the fretboard with the tap layer on
    taps rather than picks; the mouse-up lifts it. */
LUTHIER_TEST (TechniquesUi, theFretboardTapsWhenTheTapLayerIsOn)
{
    auto processor = makeProcessor();
    auto& layer = processor->getEngine().getTechniqueLayer();

    TapSettings tap;
    tap.armed = true;
    tap.source = TapSource::fretboard;
    processor->getEngine().setTapSettings (tap);

    CHECK (TechniqueOverlay::handleMouseDown (*processor, 2, 7.4, 22));

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer none;
    processor->getEngine().processBlock (buffer, none);
    CHECK (layer.tap.isTapping (2));
    CHECK_NEAR (layer.tap.soundingFret (2, 0.0), 7.0, 1.0e-9);   // fret snap

    TechniqueOverlay::handleMouseUp (*processor);
    processor->getEngine().processBlock (buffer, none);
    CHECK (! layer.tap.isTapping (2));

    // Off: a click is a pick, as ever.
    processor->getEngine().setTapSettings (TapSettings {});
    CHECK (! TechniqueOverlay::handleMouseDown (*processor, 2, 7.4, 22));
    TechniqueOverlay::handleMouseUp (*processor);
}

/*  8: the undo classes' grouping. technique-param: the same parameter within
    200 ms is one entry; a different one is another. mute-grid-paint: the
    same cell within 200 ms is one entry. */
LUTHIER_TEST (TechniquesUi, theUndoClassesGroupAsSpecified)
{
    auto processor = makeProcessor();
    TechniqueUndo::resetMergeWindow();

    const int start = processor->getNumUndoSteps();

    TechniqueUndo::setParameter (*processor, ParamIDs::tapFlick, 0.2f);
    TechniqueUndo::setParameter (*processor, ParamIDs::tapFlick, 0.3f);
    TechniqueUndo::setParameter (*processor, ParamIDs::tapFlick, 0.4f);
    CHECK (processor->getNumUndoSteps() == start + 1);

    TechniqueUndo::setParameter (*processor, ParamIDs::tapDuration, 300.0f);
    CHECK (processor->getNumUndoSteps() == start + 2);

    processor->undo();
    processor->undo();
    CHECK_NEAR (plainOf (*processor, ParamIDs::tapFlick), 0.5f, 1.0e-4f);   // back past the whole merged run

    TechniqueUndo::resetMergeWindow();
    const int paintStart = processor->getNumUndoSteps();
    TechniqueUndo::paintLiveMuteStep (*processor, 3, MuteType::palmHeavy);
    TechniqueUndo::paintLiveMuteStep (*processor, 3, MuteType::ghost);
    CHECK (processor->getNumUndoSteps() == paintStart + 1);
    TechniqueUndo::paintLiveMuteStep (*processor, 4, MuteType::ghost);
    CHECK (processor->getNumUndoSteps() == paintStart + 2);

    // Past the window, the same cell is a new entry.
    TechniqueUndo::resetMergeWindow();
    TechniqueUndo::paintLiveMuteStep (*processor, 4, MuteType::chuka);
    CHECK (processor->getNumUndoSteps() == paintStart + 3);

    // technique-arm never groups, even back to back.
    const int armStart = processor->getNumUndoSteps();
    TechniqueUndo::setArmed (*processor, TechniqueSlot::scrape, true);
    TechniqueUndo::setArmed (*processor, TechniqueSlot::scrape, false);
    CHECK (processor->getNumUndoSteps() == armStart + 2);

    // Live gestures are not undoable: a tap request pushes nothing.
    const int gestureStart = processor->getNumUndoSteps();
    processor->getEngine().getTechniqueLayer().tap.requestGesture (TapGesture {});
    processor->getEngine().getTechniqueTriggers().request (TechniqueId::slap, 0, true);
    CHECK (processor->getNumUndoSteps() == gestureStart);
}

/*  7: "Uses Techniques" filters the browser to presets that arm one, and the
    multi-select narrows it. */
LUTHIER_TEST (TechniquesUi, thePresetChipFilters)
{
    auto processor = makeProcessor();
    auto& manager = processor->getPresetManager();
    manager.refresh();

    int arming = 0;

    for (int i = 0; i < manager.getNumPresets(); ++i)
        if (! manager.getPreset (i)->armedTechniques.isEmpty())
            ++arming;

    CHECK_MSG (arming >= 20, juce::String (arming) + " presets arm a technique");

    // FEAT-BROWSER: the "Uses Techniques" filter lives in PresetSearch::Filters;
    // the rows are index entries, each carrying the manager's PresetInfo.
    PresetBrowserPanel browser (*processor);
    browser.setSize (780, 560);
    browser.overlayShown();
    processor->getPresetLibrary().refreshSynchronously();
    browser.refilter();

    const auto& index = processor->getPresetLibrary().getIndex();
    const auto visible = [&] { return (int) browser.getResults().size(); };
    const auto armedOf = [&] (int row) -> const juce::StringArray& { return index[browser.getResults()[(size_t) row]].info.armedTechniques; };

    const int all = visible();

    browser.getFilters().usesTechniques = true;
    browser.refilter();
    CHECK (visible() == arming);
    CHECK (visible() < all);

    for (int row = 0; row < visible(); ++row)
        CHECK (! armedOf (row).isEmpty());

    browser.getFilters().techniques[PresetFeatures::tap] = true;
    browser.refilter();
    const int tapping = visible();
    CHECK (tapping > 0 && tapping < arming);

    for (int row = 0; row < tapping; ++row)
        CHECK (armedOf (row).contains (ParamIDs::tapArmed));

    browser.getFilters() = {};
    browser.refilter();
    CHECK (visible() == all);
}

/*  8: the onboarding tour's Techniques stop has anchors to point at. */
LUTHIER_TEST (TechniquesUi, theOnboardingAnchorsAreThere)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor();

    AdvancedPanel panel (*processor);
    panel.setSize (1600, 900);

    std::function<juce::Component* (juce::Component&, const juce::String&)> find =
        [&find] (juce::Component& root, const juce::String& id) -> juce::Component*
    {
        if (root.getComponentID() == id)
            return &root;

        for (auto* child : root.getChildren())
            if (auto* hit = find (*child, id))
                return hit;

        return nullptr;
    };

    // The tour opens the tab, then points (the tab's panel joins the tree when shown).
    CHECK (panel.setWorkspaceTabNamed ("TECHNIQUES"));
    CHECK (find (panel, TechniquesPanel::kOnboardingAnchor) == panel.getTechniquesPanel());
    CHECK (find (panel, TechniquesPanel::kOnboardingCascadeAnchor) != nullptr);

    // Leaving it again does not disturb the tabs.
    panel.setWorkspaceTab (0);
    CHECK (panel.getWorkspaceTab() == 0);
}

/*  5: the CHARACTER mirrors are the same parameters as the sub-tabs. */
LUTHIER_TEST (TechniquesUi, theCharacterMirrorsAttachTheSameParameters)
{
    auto processor = makeProcessor();

    CharacterPanel character (*processor);
    character.setSize (500, character.preferredHeight());

    juce::Array<TechniqueMirrors*> mirrors;
    collect (character, mirrors);
    CHECK (mirrors.size() == 1);

    if (mirrors.isEmpty())
        return;

    const auto ids = mirrors[0]->getParameterIds();

    for (auto id : { ParamIDs::tapStrengthCurve, ParamIDs::tapFlick, ParamIDs::tapAutoPullOff,
                     ParamIDs::bendGlobalRange, ParamIDs::bendVibratoSource })
        CHECK_MSG (ids.contains (id), juce::String ("CHARACTER has no mirror of ") + id);

    // The slide section expands.
    CHECK (! mirrors[0]->isSlideExpanded());
    mirrors[0]->getSlideExpander().setToggleState (true, juce::dontSendNotification);
    mirrors[0]->getSlideExpander().onClick();
    CHECK (mirrors[0]->isSlideExpanded());
}

/*  9: under reduced motion the pill's firing dot is a state, not a pulse. */
LUTHIER_TEST (TechniquesUi, reducedMotionStopsThePulse)
{
    auto processor = makeProcessor();
    auto& settings = AccessibilitySettings::get();
    const bool was = settings.isReducedMotion();
    settings.setReducedMotion (true);

    TechniquePill pill (*processor, TechniqueSlot::mute);
    TechniqueUndo::setArmed (*processor, TechniqueSlot::mute, true);
    pill.refresh();

    // The overlay's fades are instant too.
    CHECK (settings.isReducedMotion());
    settings.setReducedMotion (was);
}
