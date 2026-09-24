/*  Options -> Appearance (gui-integration.md 5) and the live displays
    (visual-polish.md 4 and 5; ui-wiring.md 10 and 11; gui-integration 12 and
    21), workstream VISUAL-WORKSHOP-QA. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../Accessibility/Accessibility.h"
#include "../UI/Widgets.h"
#include "../UI/NoiseGroups.h"
#include "../UI/UiPreferences.h"
#include "../UI/Theme.h"
#include "../UI/StageTouches.h"
#include "../UI/AudioPathView.h"
#include "../UI/OptionsPages.h"
#include "../UI/RangesUi.h"
#include "../UI/HelpContent.h"
#include "../UI/NewFeatureDots.h"
#include "../UI/ModMatrixPanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    struct SettingsScope
    {
        PaletteId palette = AccessibilitySettings::get().getPalette();
        int accent = AccessibilitySettings::get().getAccent();
        bool reduced = AccessibilitySettings::get().isReducedMotion();
        bool stream = DataStreamDisplay::isEnabledByUser();
        bool strip = NoiseEventStrip::isEnabledByUser();

        ~SettingsScope()
        {
            auto& s = AccessibilitySettings::get();
            s.setAccent (accent);
            s.setPalette (palette);
            s.setReducedMotion (reduced);
            DataStreamDisplay::setEnabledByUser (stream);
            NoiseEventStrip::setEnabledByUser (strip);
        }
    };
}

//==============================================================================
/*  visual-polish.md 7: "every accent option on every palette meets 4.5:1 for
    text" - and the guitar's finish, whatever it is, after adjustment. */
LUTHIER_TEST (Accent, everyChoiceMeetsContrastOnEveryPalette)
{
    SettingsScope scope;
    auto& settings = AccessibilitySettings::get();

    const juce::Colour finishes[] = { juce::Colour (0xff0f0f0f), juce::Colour (0xfff5f1e8), juce::Colour (0xff7a2e1b),
                                      juce::Colour (0xff96b8d2), juce::Colour (0xffb22820) };

    for (int p = 0; p < (int) PaletteId::numPalettes; ++p)
    {
        settings.setPalette ((PaletteId) p);

        for (int a = 0; a < AccessibilitySettings::kNumAccents; ++a)
        {
            settings.setAccent (a);
            const double c = AccessibilitySettings::accentContrast (settings.getColours().accent, settings.getColours());
            CHECK_MSG (c >= 4.5, juce::String (getPaletteName ((PaletteId) p)) + " / " + AccessibilitySettings::getAccentNames()[a]
                                     + ": " + juce::String (c, 2) + " to 1");
        }

        settings.setAccent (AccessibilitySettings::kFollowGuitar);

        for (auto finish : finishes)
        {
            settings.setGuitarAccentSource (finish);
            const double c = AccessibilitySettings::accentContrast (settings.getColours().accent, settings.getColours());
            CHECK_MSG (c >= 4.5, "the guitar's " + finish.toDisplayString (false) + " on "
                                     + getPaletteName ((PaletteId) p) + ": " + juce::String (c, 2));
        }
    }

    // Choice 0 is the palette's own brass, untouched.
    settings.setPalette (PaletteId::defaultDark);
    settings.setAccent (0);
    CHECK (settings.getColours().accent == PaletteColours().accent);

    // A choice survives a palette change, and is saved with the settings.
    settings.setAccent (4);
    const auto blue = settings.getColours().accent;
    settings.setPalette (PaletteId::light);
    CHECK (settings.getColours().accent != PaletteColours().accent);
    CHECK ((int) settings.toVar().getProperty ("accent", 0) == 4);
    settings.setPalette (PaletteId::defaultDark);
    CHECK (settings.getColours().accent == blue);
}

/*  The accent reaches the window: choosing one repaints the plugin in it, and
    "Follow the guitar" follows a finish change. */
LUTHIER_TEST (Accent, theWindowTakesTheAccentAndFollowsTheGuitar)
{
    SettingsScope scope;
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    CHECK (editor != nullptr);
    if (editor == nullptr)
        return;

    auto& settings = AccessibilitySettings::get();
    settings.setAccent (2);
    settings.dispatchPendingMessages();
    CHECK (Palette::accent == settings.getColours().accent);
    CHECK (Palette::accent != PaletteColours().accent);

    settings.setAccent (AccessibilitySettings::kFollowGuitar);
    settings.setGuitarAccentSource (juce::Colour (0xff425878));   // Lake Placid Blue
    settings.dispatchPendingMessages();
    CHECK (Palette::accent == AccessibilitySettings::accentFor (AccessibilitySettings::kFollowGuitar, settings.getColours(),
                                                                juce::Colour (0xff425878)));
}

//==============================================================================
/*  gui-integration 12 and ui-wiring 11: the footer's data stream keeps 200
    lines, stops 500 ms after the last record, holds still under reduced
    motion, and Options -> Appearance hides it. */
LUTHIER_TEST (DataStream, itKeeps200StopsAfter500msAndHonoursReducedMotion)
{
    SettingsScope scope;
    AccessibilitySettings::get().setReducedMotion (false);

    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    juce::Component parent;
    DataStreamDisplay stream;
    parent.addAndMakeVisible (stream);
    stream.setSource (&processor);
    DataStreamDisplay::setEnabledByUser (true);

    auto play = [&processor] (int blocks)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 40 + (b % 24), 0.8f), 0);
            midi.addEvent (juce::MidiMessage::noteOff (1, 40 + (b % 24)), 256);
            processor.processBlock (buffer, midi);
        }
    };

    for (int round = 0; round < 40; ++round)
    {
        play (4);
        stream.update (1000.0 + round);
    }

    CHECK (stream.isScrolling());
    CHECK (stream.getNumLinesKept() > 0 && stream.getNumLinesKept() <= DataStreamDisplay::kMaxLines);

    // Nothing new: still scrolling at 499 ms, stopped at 500.
    stream.update (1039.0 + 499.0);
    CHECK (stream.isScrolling());
    stream.update (1039.0 + 500.0);
    CHECK (! stream.isScrolling());

    // Reduced motion: new records do not move it.
    AccessibilitySettings::get().setReducedMotion (true);
    const int kept = stream.getNumLinesKept();
    play (8);
    stream.update (5000.0);
    CHECK (! stream.isScrolling());
    CHECK (stream.getNumLinesKept() == kept);

    // Appearance hides it.
    DataStreamDisplay::setEnabledByUser (false);
    stream.update (6000.0);
    CHECK (! stream.isVisible());
}

/*  gui-integration 21 / ui-wiring 10: under reduced motion the noise strip is a
    static count per kind; Appearance hides it. */
LUTHIER_TEST (NoiseStrip, reducedMotionShowsAStaticCountAndAppearanceHidesIt)
{
    SettingsScope scope;
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    juce::Component parent;
    NoiseEventStrip strip (processor);
    parent.addAndMakeVisible (strip);
    strip.setSize (300, NoiseEventStrip::preferredHeight);

    // Playing with the pick click up makes events.
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (ParamIDs::pickClickAmount)))
        p->setValueNotifyingHost (1.0f);
    processor.getParameterBridge().applyAllNow();

    juce::AudioBuffer<float> buffer (2, 512);
    for (int b = 0; b < 20; ++b)
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 52 + (b % 5), 1.0f), 0);
        processor.processBlock (buffer, midi);
    }

    strip.pollNow();
    int total = 0;
    for (auto c : strip.getClassCounts())
        total += c;
    CHECK_MSG (total > 0, "no noise events to count");

    AccessibilitySettings::get().setReducedMotion (true);
    juce::Image a (juce::Image::ARGB, 300, NoiseEventStrip::preferredHeight, true, juce::SoftwareImageType());
    juce::Image b (a.createCopy());
    {
        juce::Graphics g (a);
        strip.paintEntireComponent (g, true);
    }
    juce::Thread::sleep (60);
    {
        juce::Graphics g (b);
        strip.paintEntireComponent (g, true);
    }

    bool same = true;
    for (int y = 0; y < a.getHeight() && same; ++y)
        for (int x = 0; x < a.getWidth() && same; ++x)
            same = a.getPixelAt (x, y) == b.getPixelAt (x, y);
    CHECK_MSG (same, "the reduced-motion strip moved between two paints");

    NoiseEventStrip::setEnabledByUser (false);
    strip.timerCallbackForTest();
    CHECK (! strip.isVisible());
}

//==============================================================================
/*  visual-polish.md 4: the VU needle reads the master output with VU
    ballistics (300 ms to settle, 0 VU at -18 dBFS) and greys when stale; the
    room light warms with the wet level and widens with the room. */
LUTHIER_TEST (StageTouches, theVuNeedleHasBallisticsAndGreysWhenStale)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    VuMeter vu (processor);

    const double zeroVu = juce::Decibels::decibelsToGain (VuMeter::kReferenceDbfs);
    double t = 0.0;
    vu.update (zeroVu, t);

    // After 100 ms it is on its way, after 300 ms within half a dB of 0 VU.
    for (; t < 100.0; t += 10.0)
        vu.update (zeroVu, t);
    CHECK (vu.getNeedleVu() < -1.0 && vu.getNeedleVu() > VuMeter::kMinVu);

    for (; t < 300.0; t += 10.0)
        vu.update (zeroVu, t);
    CHECK_NEAR (vu.getNeedleVu(), 0.0, 0.5);
    CHECK (! vu.isStale());

    // An unchanging, non-silent reading for over a second is stale.
    vu.update (zeroVu, t + 1500.0);
    CHECK (vu.isStale());

    // The scale is VU's, compressed at the bottom: -10 sits well below halfway.
    CHECK (VuMeter::scalePosition (-10.0) < 0.3f && VuMeter::scalePosition (0.0) > 0.6f);
}

LUTHIER_TEST (StageTouches, theRoomLightFollowsSizeAndWet)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    RoomLight light (processor);

    auto set = [&processor] (const char* id, float normalised)
    {
        processor.getState().getParameter (id)->setValueNotifyingHost (normalised);
    };

    set (ParamIDs::roomOn, 1.0f);
    set (ParamIDs::roomBlend, 0.1f);
    set (ParamIDs::roomSize, 0.0f);
    light.refresh();
    const float dryWarmth = light.getWarmth(), smallSpread = light.getSpread();

    set (ParamIDs::roomBlend, 0.8f);
    set (ParamIDs::roomSize, 1.0f);
    light.refresh();
    CHECK (light.getWarmth() > dryWarmth);
    CHECK (light.getSpread() > smallSpread);

    set (ParamIDs::roomOn, 0.0f);
    light.refresh();
    CHECK (light.getWarmth() == 0.0f);
}

//==============================================================================
/*  gui-integration 20: Options -> Diagnostics shows what is on the audio path
    right now, and 5: it mirrors the Workshop / Slide / advanced-ranges flags. */
LUTHIER_TEST (Diagnostics, theAudioPathShowsWhatIsSoundingAndTheFlags)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    AudioPathView view (processor);

    auto stage = [&view] (const juce::String& name)
    {
        for (auto& s : view.readStages())
            if (s.name == name)
                return s;
        return AudioPathView::Stage {};
    };

    auto set = [&processor] (const juce::String& id, float plain)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id));
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    };

    set (ParamIDs::cabOn, 1.0f);
    CHECK (stage ("Cabinet").active);
    set (ParamIDs::cabOn, 0.0f);
    CHECK (! stage ("Cabinet").active);

    CHECK (! stage ("Pre FX").active);
    set (ParamIDs::slotType (false, 0), 1.0f);
    set (ParamIDs::slotBypass (false, 0), 0.0f);
    CHECK (stage ("Pre FX").active && stage ("Pre FX").detail == "1 pedal");

    CHECK (view.describeFlags().contains ("Slide Mode: off"));
    set (ParamIDs::slideGuitar, 1.0f);
    CHECK (view.describeFlags().contains ("Slide Mode: on"));

    RangeState unlocked;
    unlocked.setFamilyAdvanced (RangeFamily::amp, true);
    processor.changeRanges (unlocked, "test");
    CHECK (view.describeFlags().contains ("Advanced ranges: amp"));
    CHECK (view.describeFlags().contains ("Workshop edit: no"));
}

//==============================================================================
/*  gui-integration 16 items 12-13: every control's menu ends with its
    read-only automation ID, and a control a shortcut drives offers "Show in
    Options -> Shortcuts", which opens the table on that action's row. */
LUTHIER_TEST (ContextMenu, everyParameterShowsItsAutomationIdAndBoundOnesTheirShortcut)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto hasItem = [] (const juce::PopupMenu& menu, int id, juce::String* text = nullptr, bool* enabled = nullptr)
    {
        for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
            if (it.getItem().itemID == id)
            {
                if (text != nullptr)    *text = it.getItem().text;
                if (enabled != nullptr) *enabled = it.getItem().isEnabled;
                return true;
            }
        return false;
    };

    int checked = 0;

    for (auto* p : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
        {
            const auto menu = buildParameterContextMenu (processor, withId->paramID);
            juce::String text;
            bool enabled = true;
            CHECK_MSG (hasItem (menu, kAutomationIdMenuId, &text, &enabled) && text.endsWith (withId->paramID) && ! enabled,
                       withId->paramID + " has no read-only automation ID item");

            const bool bound = shortcutActionForParameter (withId->paramID).isNotEmpty();
            CHECK_MSG (hasItem (menu, kShowShortcutMenuId) == bound, withId->paramID + ": the shortcut item is wrong");
            ++checked;
        }

    CHECK (checked > 400);

    // The one bound control: Slide Mode, S.
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    CHECK (editor != nullptr && showShortcutInOptions != nullptr);

    if (editor != nullptr && showShortcutInOptions != nullptr)
    {
        editor->setSize (1200, 720);
        showShortcutInOptions ("toggleSlideMode");

        AccessibilityPage* page = nullptr;
        std::function<void (juce::Component&)> find = [&] (juce::Component& c)
        {
            for (auto* child : c.getChildren())
            {
                if (auto* a = dynamic_cast<AccessibilityPage*> (child))
                    page = a;
                find (*child);
            }
        };
        find (*editor);

        CHECK (page != nullptr && page->isVisible());
        if (page != nullptr)
            CHECK_MSG (page->getShortcutFilter() == "Toggle Slide Mode", page->getShortcutFilter());
    }
}

//==============================================================================
/*  gui-integration 22: "the warning-colour arc portion appears when a knob
    passes the stock max, disappears when returned" - counted in pixels. */
LUTHIER_TEST (RangeMarking, theWarningArcAppearsPastStockAndGoesWhenReturned)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    LuthierLookAndFeel lookAndFeel;
    juce::Component root;
    root.setLookAndFeel (&lookAndFeel);
    LuthierKnob knob ("Gain");
    root.addAndMakeVisible (knob);
    knob.setBounds (0, 0, 90, 110);
    knob.attachTo (processor, ParamIDs::ampGain);

    RangeState unlocked;
    unlocked.setFamilyAdvanced (RangeFamily::amp, true);
    processor.changeRanges (unlocked, "test");
    RangesUi::resyncControls (root);

    auto set = [&processor] (float plain)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (ParamIDs::ampGain));
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    };

    auto warningPixels = [&knob]
    {
        juce::Image image (juce::Image::ARGB, knob.getWidth(), knob.getHeight(), true, juce::SoftwareImageType());
        {
            juce::Graphics g (image);
            knob.paintEntireComponent (g, true);
        }

        int n = 0;
        for (int y = 0; y < image.getHeight(); ++y)
            for (int x = 0; x < image.getWidth(); ++x)
            {
                const auto c = image.getPixelAt (x, y);
                const auto w = Palette::warning;
                if (std::abs (c.getRed() - w.getRed()) < 24 && std::abs (c.getGreen() - w.getGreen()) < 24
                    && std::abs (c.getBlue() - w.getBlue()) < 24 && c.getAlpha() > 200)
                    ++n;
            }
        return n;
    };

    set (0.8f);
    const int inside = warningPixels();
    set (1.6f);
    const int past = warningPixels();
    set (0.8f);
    const int back = warningPixels();

    CHECK_MSG (past > inside + 10, "no warning arc past stock (" + juce::String (inside) + " -> " + juce::String (past) + ")");
    CHECK_MSG (back <= inside + 2, "the warning arc stayed after the value came back (" + juce::String (back) + ")");

    root.removeChildComponent (&knob);
    root.setLookAndFeel (nullptr);
}

//==============================================================================
/*  ui-wiring.md 21: every control attached to a parameter has an accessible
    name, in Easy and in Advanced. */
LUTHIER_TEST (ScreenReader, everyAttachedControlHasAName)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    CHECK (editor != nullptr);
    if (editor == nullptr)
        return;

    editor->setSize (1600, 900);

    int named = 0, unnamed = 0;
    juce::StringArray missing;

    std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            juce::Component* inner = nullptr;
            juce::String id;

            if (auto* k = dynamic_cast<LuthierKnob*> (child))        { inner = &k->getSlider(); id = k->getParameterId(); }

            if (inner != nullptr && id.isNotEmpty())
            {
                if (inner->getTitle().isNotEmpty()) ++named;
                else { ++unnamed; missing.addIfNotAlreadyThere (id); }
            }

            walk (*child);
        }
    };

    walk (*editor);
    editor->keyPressed (AccessibilitySettings::get().findShortcut ("toggleAdvanced")->key);
    walk (*editor);

    CHECK (named > 50);
    CHECK_MSG (unnamed == 0, "unnamed: " + missing.joinIntoString (", "));
}

//==============================================================================
/*  gui-integration 20: every panel with more than one row of controls has a
    `?` that opens Help pinned to that panel's docs - the Advanced columns'
    sections, the shown workspace tab, and the Easy cards and strips. */
LUTHIER_TEST (PanelHelp, everyPanelsQuestionMarkOpensItsOwnTopic)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    CHECK (editor != nullptr && openHelpForPanel != nullptr);
    if (editor == nullptr)
        return;

    editor->setSize (1600, 900);

    auto collectButtons = [&editor]
    {
        juce::Array<PanelHelpButton*> found;
        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            for (auto* child : c.getChildren())
            {
                if (auto* b = dynamic_cast<PanelHelpButton*> (child); b != nullptr && b->isVisible())
                    found.add (b);
                walk (*child);
            }
        };
        walk (*editor);
        return found;
    };

    const auto easy = collectButtons();
    CHECK_MSG (easy.size() >= 8, "Easy has " + juce::String (easy.size()) + " help buttons");

    editor->keyPressed (AccessibilitySettings::get().findShortcut ("toggleAdvanced")->key);
    const auto advanced = collectButtons();
    CHECK_MSG (advanced.size() >= 10, "Advanced has " + juce::String (advanced.size()) + " help buttons");

    juce::StringArray unmapped;

    for (auto* b : easy)
        if (HelpContent::findTopic (b->getPanelName()) < 0)
            unmapped.addIfNotAlreadyThere (b->getPanelName());

    for (auto* b : advanced)
        if (HelpContent::findTopic (b->getPanelName()) < 0)
            unmapped.addIfNotAlreadyThere (b->getPanelName());

    CHECK_MSG (unmapped.isEmpty(), "no help topic for: " + unmapped.joinIntoString (", "));
}

//==============================================================================
/*  gui-integration 22: "Reflow: instantiate at 800, 1000, 1280, 1600, 1920,
    2560 window widths at 75 - 200 % UI scale; verify no clipping." The window
    stops at 940 (its minimum; 800 is below it, DECISIONS), Advanced needs
    1000, and the UI scale is the editor's scale factor, so the logical layout
    is the same at every scale: the check is that the factor is applied, and
    that at every width no visible control hangs outside its parent. */
LUTHIER_TEST (Reflow, noControlHangsOutsideItsParentAtAnyWidthOrScale)
{
    SettingsScope scope;
    const double originalScale = AccessibilitySettings::get().getUiScale();

    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    CHECK (editor != nullptr);
    if (editor == nullptr)
        return;

    editor->setVisible (true);

    juce::StringArray problems;

    std::function<void (juce::Component&, const juce::String&)> walk = [&] (juce::Component& c, const juce::String& where)
    {
        for (auto* child : c.getChildren())
        {
            if (! child->isVisible())
                continue;

            // A viewport's content is meant to be larger than the viewport; a
            // list's rows are its own business.
            const bool scrolled = dynamic_cast<juce::Viewport*> (c.getParentComponent()) != nullptr
                               || dynamic_cast<juce::Viewport*> (&c) != nullptr
                               || c.findParentComponentOfClass<juce::ListBox>() != nullptr
                               || dynamic_cast<juce::ListBox*> (&c) != nullptr;

            if (! scrolled && ! c.getLocalBounds().expanded (1).contains (child->getBounds())
                && child->getWidth() > 0 && child->getHeight() > 0)
                problems.addIfNotAlreadyThere (where + ": " + juce::String (typeid (*child).name()).substring (0, 60)
                                               + " '" + child->getTitle() + child->getName() + "' "
                                               + child->getBounds().toString() + " outside " + c.getLocalBounds().toString());

            walk (*child, where);
        }
    };

    for (bool advanced : { false, true })
    {
        if (advanced)
            editor->keyPressed (AccessibilitySettings::get().findShortcut ("toggleAdvanced")->key);

        for (int width : { 940, 1000, 1280, 1600, 1920, 2560 })
        {
            if (advanced && width < 1000)
                continue;

            editor->setSize (width, juce::jmin (1600, juce::roundToInt ((float) width * 0.5625f)));
            walk (*editor, juce::String (advanced ? "Advanced " : "Easy ") + juce::String (width));
        }
    }

    for (int i = 0; i < juce::jmin (12, problems.size()); ++i)
        ctx.fail (problems[i]);

    CHECK_MSG (problems.isEmpty(), juce::String (problems.size()) + " controls hang outside their parents");

    // The scale is applied, at every step accessibility.md 4 offers.
    for (double scale : AccessibilitySettings::kScales)
    {
        AccessibilitySettings::get().setUiScale (scale);
        AccessibilitySettings::get().dispatchPendingMessages();
        CHECK_NEAR (editor->getTransform().getScaleFactor(), (float) scale, 1.0e-3f);
    }

    AccessibilitySettings::get().setUiScale (originalScale);
    AccessibilitySettings::get().dispatchPendingMessages();
}

//==============================================================================
/*  gui-integration 20: a feature added after 1.0 carries a NEW dot on its entry
    point for a week after the first launch of the version that introduced it. */
LUTHIER_TEST (NewDots, anEntryPointIsMarkedForItsFirstWeekOnly)
{
    const std::vector<NewFeatureDots::Entry> table { { "TECHNIQUES", "9.9.9-test" } };
    const auto key = juce::String ("newFeatures.firstLaunch.9.9.9-test");
    const auto saved = UiPreferences::get().getString (key, {});
    UiPreferences::get().setString (key, {});

    const auto start = juce::Time::getCurrentTime();
    CHECK (NewFeatureDots::isNew ("TECHNIQUES", "9.9.9-test", start, table));   // before the first launch is noted
    NewFeatureDots::noteLaunch ("9.9.9-test", start);
    CHECK (NewFeatureDots::isNew ("TECHNIQUES", "9.9.9-test", start + juce::RelativeTime::days (6.9), table));
    CHECK (! NewFeatureDots::isNew ("TECHNIQUES", "9.9.9-test", start + juce::RelativeTime::days (7.1), table));
    CHECK (! NewFeatureDots::isNew ("TECHNIQUES", "9.9.10", start, table));
    CHECK (! NewFeatureDots::isNew ("MOD", "9.9.9-test", start, table));

    // Applied to a button with that text, and drawn.
    juce::Component root;
    juce::TextButton techniques ("TECHNIQUES"), mod ("MOD");
    root.addAndMakeVisible (techniques);
    root.addAndMakeVisible (mod);
    NewFeatureDots::apply (root, "9.9.9-test", start + juce::RelativeTime::days (1.0), table);
    CHECK ((bool) techniques.getProperties()[NewFeatureDots::kProperty]);
    CHECK (! (bool) mod.getProperties()[NewFeatureDots::kProperty]);

    // 1.0 introduced everything it has: nothing is marked in this build.
    CHECK (NewFeatureDots::getTable().empty());

    UiPreferences::get().setString (key, saved);
}

//==============================================================================
/*  gui-integration 11.2 / ui-wiring 12: a MOD source card dragged onto a
    control makes a route at 25% depth, one undo entry; the card is the drag
    source and every attached knob and slider a target. */
LUTHIER_TEST (DragToModulate, aDroppedSourceRoutesAt25PercentAsOneEntry)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto description = ModSourceCard::dragDescriptionFor (ModSourceSlots::lfoBase + 1);
    CHECK (modSourceSlotFromDrag (description) == ModSourceSlots::lfoBase + 1);
    CHECK (modSourceSlotFromDrag ("something else") == -1);

    juce::Component root;
    LuthierKnob knob ("Gain");
    root.addAndMakeVisible (knob);
    knob.attachTo (processor, ParamIDs::ampGain);

    juce::DragAndDropTarget::SourceDetails details (description, nullptr, {});
    CHECK (knob.isInterestedInDragSource (details));
    CHECK (! knob.isInterestedInDragSource (juce::DragAndDropTarget::SourceDetails ("text", nullptr, {})));

    auto& matrix = processor.getModMatrix();
    const int routesBefore = matrix.getRouteCountForDestination (ParamIDs::ampGain);
    const int steps = processor.getNumUndoSteps();

    knob.itemDropped (details);

    CHECK (matrix.getRouteCountForDestination (ParamIDs::ampGain) == routesBefore + 1);
    CHECK (processor.getNumUndoSteps() == steps + 1);
    CHECK (processor.getUndoDescription().contains ("at 25%"));

    bool found = false;
    for (const auto& r : matrix.getRoutes())
        if (r.destinationId == ParamIDs::ampGain)
            found = found || std::abs (r.depth - 0.25f) < 1.0e-6f;
    CHECK (found);

    processor.undo();
    CHECK (matrix.getRouteCountForDestination (ParamIDs::ampGain) == routesBefore);
}
