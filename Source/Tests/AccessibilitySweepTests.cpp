/*  SPEC-SWEEP: accessibility.md 1-4 on the painted widgets, the knobs and the
    overlays (A11Y-7, A11Y-9, A11Y-10, A11Y-11, A11Y-13, A11Y-14, A11Y-48).
    Groups: ScreenReader, Keyboard, Editor. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../UI/LiveStrip.h"
#include "../UI/Overlays.h"
#include "../UI/Widgets.h"

#include <set>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    template <typename T>
    void collectAll (juce::Component& root, juce::Array<T*>& found)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* match = dynamic_cast<T*> (child))
                found.add (match);

            collectAll<T> (*child, found);
        }
    }

    bool onScreen (juce::Component* c, juce::Component* root)
    {
        if (c->getWidth() <= 0 || c->getHeight() <= 0)
            return false;

        for (auto* p = c; p != nullptr; p = p->getParentComponent())
        {
            if (! p->isVisible())
                return false;

            if (p == root)
                return true;
        }

        return false;
    }
}

//==============================================================================
/*  A11Y-7: the output meter is a read-only value in dBFS. */
LUTHIER_TEST (ScreenReader, theMeterReportsItsPeakInDbfs)
{
    LuthierAudioProcessor processor;
    LevelMeter meter;
    meter.setSource (&processor);
    meter.setSize (20, 100);

    CHECK (meter.getTitle() == "Output level");

    // Built directly: JUCE only creates it for a component on a peer.
    auto owned = meter.createAccessibilityHandler();
    auto* handler = owned.get();
    CHECK (handler != nullptr);

    if (handler == nullptr)
        return;

    CHECK (handler->getRole() == juce::AccessibilityRole::progressBar);

    auto* value = handler->getValueInterface();
    CHECK (value != nullptr && value->isReadOnly());

    if (value != nullptr)
    {
        const auto text = value->getCurrentValueAsString();
        CHECK_MSG (text == "silent" || text.endsWith ("dBFS"), "the meter read \"" + text + "\"");
    }
}

/*  A11Y-9: the snapshot strip names each of its eight pads, and pressing one
    through the accessibility action recalls it. */
LUTHIER_TEST (ScreenReader, theSnapshotStripNamesEachSnapshot)
{
    LuthierAudioProcessor processor;
    processor.captureSnapshot (0, "Verse");
    processor.captureSnapshot (1, "Chorus");
    processor.getSnapshots().setCrossfadeMs (0.0);

    SnapshotStrip strip (processor);
    strip.setSize (800, 44);
    strip.refresh();

    for (int slot = 0; slot < SnapshotStrip::kButtonsShown; ++slot)
    {
        auto* accessor = strip.getSlotAccessor (slot);
        CHECK (accessor != nullptr);

        if (accessor == nullptr)
            continue;

        CHECK_MSG (accessor->getTitle().startsWith ("Snapshot " + juce::String (slot + 1)),
                   "slot " + juce::String (slot) + " is titled \"" + accessor->getTitle() + "\"");
        CHECK (accessor->getWantsKeyboardFocus());
        CHECK (accessor->getBounds().getWidth() > 0);
    }

    CHECK (strip.getSlotAccessor (1)->getTitle().contains ("Chorus"));
    CHECK (strip.getSlotAccessor (5)->getTitle().contains ("empty"));

    // The press a screen reader's action sends is the button's click (JUCE
    // builds the handler only once the strip is on a peer).
    strip.getSlotAccessor (1)->onClick();
    processor.getSnapshots().advancePending();
    CHECK (processor.getSnapshots().getCurrentSnapshot() == 1);
}

//==============================================================================
/*  A11Y-13 / A11Y-14: a knob takes focus, and the arrows step 1%, Shift 0.1%,
    Ctrl/Cmd 10%. */
LUTHIER_TEST (Accessibility, arrowKeysStepFineAndCoarse)
{
    LuthierAudioProcessor processor;

    LuthierKnob knob ("Gain");
    knob.attachTo (processor, ParamIDs::ampGain);
    knob.setSize (60, 80);

    auto& slider = knob.getSlider();
    CHECK (slider.getWantsKeyboardFocus());

    const auto range = slider.getRange();
    slider.setValue (range.getStart() + range.getLength() * 0.5, juce::sendNotificationSync);

    auto delta = [&] (const juce::KeyPress& key)
    {
        const double before = slider.getValue();
        CHECK (slider.keyPressed (key));
        return slider.getValue() - before;
    };

    const double normal = delta (juce::KeyPress (juce::KeyPress::upKey));
    const double fine   = delta (juce::KeyPress (juce::KeyPress::upKey, juce::ModifierKeys::shiftModifier, 0));
    const double coarse = delta (juce::KeyPress (juce::KeyPress::upKey, juce::ModifierKeys::commandModifier, 0));

    CHECK_MSG (fine > 0.0 && normal > fine && coarse > normal,
               "fine " + juce::String (fine) + ", normal " + juce::String (normal)
                 + ", coarse " + juce::String (coarse));
    CHECK_NEAR (normal, range.getLength() * 0.01, range.getLength() * 0.002);

    CHECK (delta (juce::KeyPress (juce::KeyPress::downKey)) < 0.0);

    CHECK (slider.keyPressed (juce::KeyPress (juce::KeyPress::endKey)));
    CHECK_NEAR (slider.getValue(), range.getEnd(), 1.0e-6);

    // And the parameter followed.
    CHECK (processor.getState().getParameter (ParamIDs::ampGain)->getValue() > 0.99f);
}

/*  A11Y-48: every visible knob in the default view is on the Tab walk. */
LUTHIER_TEST (Keyboard, aTabWalkReachesEveryKnob)
{
    LuthierAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
        return;

    editor->setVisible (true);
    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);

    auto traverser = editor->createKeyboardFocusTraverser();
    CHECK (traverser != nullptr);

    if (traverser == nullptr)
        return;

    // Walk the traverser the way Tab does, container by container.
    std::set<juce::Component*> reachable;
    std::function<void (juce::Component*)> walk = [&] (juce::Component* container)
    {
        auto t = container->createKeyboardFocusTraverser();

        if (t == nullptr)
            return;

        for (auto* c : t->getAllComponents (container))
        {
            if (! reachable.insert (c).second)
                continue;

            if (c->isKeyboardFocusContainer())
                walk (c);
        }
    };

    walk (editor.get());

    juce::Array<LuthierKnob*> knobs;
    collectAll<LuthierKnob> (*editor, knobs);

    int visible = 0, missing = 0;
    juce::String firstMissing;

    for (auto* knob : knobs)
    {
        if (! onScreen (knob, editor.get()))
            continue;

        ++visible;

        if (reachable.count (&knob->getSlider()) == 0)
        {
            if (missing++ == 0)
                firstMissing = knob->getParameterId();
        }
    }

    CHECK_MSG (visible > 0, "no knobs on screen");
    CHECK_MSG (missing == 0, juce::String (missing) + " of " + juce::String (visible)
                               + " knobs are not on the Tab walk, first " + firstMissing);
}

//==============================================================================
/*  A11Y-10 / A11Y-11 without a window: the host opens, remembers no launcher
    when nothing had focus, and dismisses cleanly. The focus hand-off itself
    needs a desktop peer, which the headless run does not have. */
LUTHIER_TEST (Editor, anOverlayOpensAndDismissesWithoutALauncher)
{
    LuthierAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
        return;

    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);

    juce::Array<OverlayHost*> hosts;
    collectAll<OverlayHost> (*editor, hosts);
    CHECK (hosts.size() == 1);

    if (hosts.isEmpty())
        return;

    auto* host = hosts.getFirst();
    const auto* binding = AccessibilitySettings::get().findShortcut ("options");
    CHECK (binding != nullptr && editor->keyPressed (binding->key));
    CHECK (host->isShowingOverlay());
    CHECK (host->getLauncher() == nullptr);
    CHECK (host->getCurrentOverlay()->getTitle().isNotEmpty());

    host->dismiss();
    CHECK (! host->isShowingOverlay());
}

/*  A11Y-43: the verbosity setting gates what is spoken. */
LUTHIER_TEST (Accessibility, verbosityGatesAnnouncements)
{
    using V = AccessibilitySettings::Verbosity;
    using A = AccessibleSetup::Announcement;

    CHECK (AccessibleSetup::shouldAnnounce (A::error, V::minimal));
    CHECK (! AccessibleSetup::shouldAnnounce (A::standard, V::minimal));
    CHECK (! AccessibleSetup::shouldAnnounce (A::valueChange, V::minimal));

    CHECK (AccessibleSetup::shouldAnnounce (A::error, V::standard));
    CHECK (AccessibleSetup::shouldAnnounce (A::standard, V::standard));
    CHECK (! AccessibleSetup::shouldAnnounce (A::valueChange, V::standard));

    CHECK (AccessibleSetup::shouldAnnounce (A::error, V::verbose));
    CHECK (AccessibleSetup::shouldAnnounce (A::standard, V::verbose));
    CHECK (AccessibleSetup::shouldAnnounce (A::valueChange, V::verbose));
}

/*  A11Y-25: the meter changes shape, not only colour - a thin bar when quiet,
    a bracket when over. Compared in greyscale-free terms: pixel footprints. */
LUTHIER_TEST (Accessibility, theMeterChangesShapeNotOnlyColour)
{
    LevelMeter meter;
    meter.setSize (20, 112);

    auto render = [&meter] (float level, float peakDb)
    {
        meter.setLevelsForTest (level, level, peakDb);
        return meter.createComponentSnapshot (meter.getLocalBounds(), true, 1.0f);
    };

    // Filled width on a row near the bottom of the left bar.
    auto filledWidth = [] (const juce::Image& image, int y)
    {
        int n = 0;
        const auto background = image.getPixelAt (0, 20);

        for (int x = 0; x < image.getWidth() / 2; ++x)
            if (image.getPixelAt (x, y) != background)
                ++n;

        return n;
    };

    const auto quiet = render (0.5f, -30.0f);    // -30 dBFS
    const auto loud  = render (0.95f, -3.0f);    // -3 dBFS

    const int y = quiet.getHeight() - 4;
    CHECK_MSG (filledWidth (quiet, y) < filledWidth (loud, y),
               "quiet " + juce::String (filledWidth (quiet, y)) + " px, loud " + juce::String (filledWidth (loud, y)) + " px");

    // Over: the top edge of the bar area gains a bracket.
    const auto over = render (1.0f, 1.0f);
    const auto notOver = render (1.0f, -0.5f);

    int differing = 0;
    for (int x = 0; x < over.getWidth(); ++x)
        if (over.getPixelAt (x, 12) != notOver.getPixelAt (x, 12))
            ++differing;

    CHECK_MSG (differing > 0, "no bracket at 0 dBFS+");
}

//==============================================================================
/*  A11Y-46: the LOCALIZATION page switches the locale and the fallback, and a
    custom catalog folder is honoured. */
#include "../UI/OptionsPages.h"
#include "../Accessibility/Localisation.h"

LUTHIER_TEST (Editor, theLocalizationPageSwitchesLocaleAndFallback)
{
    auto& loc = Localisation::get();
    const auto savedLocale = loc.getLocale();
    const auto savedFallback = loc.getFallbackLocale();
    const auto savedSettings = AccessibilitySettings::get().toVar();

    // A custom catalog for German, so the switch has something to load.
    juce::TemporaryFile holder;
    const auto dir = holder.getFile();
    dir.createDirectory();
    CHECK (dir.getChildFile ("de.json").replaceWithText (R"({ "a11y.dialog.opened": "Dialog geoeffnet" })"));
    loc.setCustomCatalogDirectory (dir);

    LuthierAudioProcessor processor;
    LocalizationPage page (processor);
    page.setSize (600, 500);

    juce::Array<juce::ComboBox*> boxes;
    collectAll<juce::ComboBox> (page, boxes);
    CHECK (boxes.size() >= 2);

    const auto& locales = Localisation::getShipLocales();
    int german = -1;
    for (size_t i = 0; i < locales.size(); ++i)
        if (locales[i].code == "de")
            german = (int) i + 1;

    CHECK (german > 0);

    if (boxes.size() >= 2 && german > 0)
    {
        boxes[0]->setSelectedId (german, juce::sendNotificationSync);
        CHECK_MSG (loc.getLocale() == "de", "locale is " + loc.getLocale());

        boxes[1]->setSelectedId (german, juce::sendNotificationSync);
        CHECK (loc.getFallbackLocale() == "de");
    }

    loc.setCustomCatalogDirectory ({});
    loc.setFallbackLocale (savedFallback);
    loc.setLocale (savedLocale);
    AccessibilitySettings::get().fromVar (savedSettings);
    AccessibilitySettings::get().save();
    dir.deleteRecursively();
}

//==============================================================================
/*  A11Y-24: the six palettes ship as Resources/Themes/*.json (copied beside
    every binary with the rest of Resources), and they are exactly the built-in
    ones. */
LUTHIER_TEST (Accessibility, theShippedThemeFilesMatchTheBuiltIns)
{
    // LUTHIER_WRITE_THEMES=<absolute path to Resources/Themes> regenerates them.
    if (const auto target = juce::SystemStats::getEnvironmentVariable ("LUTHIER_WRITE_THEMES", {});
        juce::File::isAbsolutePath (target))
        CHECK (AccessibilitySettings::writeBuiltInPalettes (juce::File (target)));

    juce::TemporaryFile holder;
    const auto fresh = holder.getFile();
    CHECK (AccessibilitySettings::writeBuiltInPalettes (fresh));

    const auto shipped = AccessibilitySettings::getThemeDirectory();

    for (int i = 0; i < (int) PaletteId::numPalettes; ++i)
    {
        const auto name = juce::String (getPaletteName ((PaletteId) i)) + ".json";
        const auto installed = shipped.getChildFile (name);

        CHECK_MSG (installed.existsAsFile(), name + " is not beside the binary");
        CHECK_MSG (installed.loadFileAsString() == fresh.getChildFile (name).loadFileAsString(),
                   name + " differs from the built-in palette");
    }

    fresh.deleteRecursively();
}
