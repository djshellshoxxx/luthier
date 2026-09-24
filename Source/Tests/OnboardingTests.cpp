/*  The welcome banner, the tour, the first-week hints and the per-panel ? icons
    (onboarding.md 1-4, 10, 11, 13, 14; gui-integration.md 16, 20).
    TUNE-HELP-ONBOARDING workstream.

    These write the real ui.json (that is where the behaviour lives);
    PreservedUi puts it back afterwards.
*/

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Presets/PresetManager.h"
#include "../UI/FirstRun.h"
#include "../UI/HelpContent.h"
#include "../UI/Onboarding.h"
#include "../UI/PanelHelpButton.h"
#include "../UI/UiPreferences.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    struct PreservedUi
    {
        PreservedUi()
            : file (UiPreferences::getConfigFile()),
              existed (file.existsAsFile()),
              text (existed ? file.loadFileAsString() : juce::String())
        {
            UiPreferences::get().reset();
            Onboarding::resetProcessStateForTesting();
            Onboarding::setClockForTesting (nullptr);
        }

        ~PreservedUi()
        {
            if (existed) file.replaceWithText (text);
            else         file.deleteFile();

            UiPreferences::get().reset();
            UiPreferences::get().load();

            Onboarding::setClockForTesting (nullptr);
            Onboarding::resetProcessStateForTesting();
            Onboarding::setAutomaticForTesting (false);
            FirstRun::setStateForTesting (true, false);
        }

        juce::File file;
        bool existed;
        juce::String text;
    };

    /** A clock the test moves. */
    struct FakeClock
    {
        FakeClock() { Onboarding::setClockForTesting ([this] { return now; }); }
        ~FakeClock() { Onboarding::setClockForTesting (nullptr); }

        void advanceDays (double days) { now = now + juce::RelativeTime::days (days); }

        juce::Time now { 2026, 8, 1, 12, 0 };
    };

    /** A launch: a new process that counts itself once. */
    void launch (const juce::String& version = Onboarding::getCurrentVersion())
    {
        Onboarding::resetProcessStateForTesting();
        Onboarding::recordLaunch (version);
    }

    std::unique_ptr<LuthierAudioProcessorEditor> makeEditor (LuthierAudioProcessor& processor, int w, int h)
    {
        auto editor = std::make_unique<LuthierAudioProcessorEditor> (processor);
        editor->setSize (w, h);
        return editor;
    }

    bool renderHasSound (LuthierAudioProcessor& processor)
    {
        processor.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (processor.getTotalNumOutputChannels(), 512);
        float peak = 0.0f;

        for (int block = 0; block < 40; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);

            buffer.clear();
            processor.processBlock (buffer, midi);
            peak = juce::jmax (peak, buffer.getMagnitude (0, 0, buffer.getNumSamples()));
        }

        return peak > 1.0e-4f;
    }
}

//==============================================================================
/*  onboarding 2 and 11: the tour offer comes back after Maybe later, three
    offers in all; Yes and Don't ask again settle it. */
LUTHIER_TEST (Onboarding, theWelcomeBannerOffersTheTourUpToThreeTimes)
{
    PreservedUi preserved;
    const juce::String v ("1.0.0");

    CHECK (Onboarding::getWelcomeDue (v) == Onboarding::Welcome::tourOffer);

    for (int i = 0; i < Onboarding::kMaxTourOffers; ++i)
    {
        CHECK_MSG (Onboarding::getWelcomeDue (v) == Onboarding::Welcome::tourOffer,
                   "offer " + juce::String (i + 1) + " did not come back after Maybe later");
        Onboarding::noteWelcomeShown (Onboarding::Welcome::tourOffer, v);
        Onboarding::answerWelcome (Onboarding::Answer::later);
    }

    CHECK_MSG (Onboarding::getWelcomeDue (v) == Onboarding::Welcome::none, "a fourth tour offer appeared");

    // Don't ask again: permanently.
    UiPreferences::get().reset();
    Onboarding::noteWelcomeShown (Onboarding::Welcome::tourOffer, v);
    Onboarding::answerWelcome (Onboarding::Answer::never);
    CHECK (Onboarding::getWelcomeDue (v) == Onboarding::Welcome::none);

    // Yes: the tour was taken, the offer stops.
    UiPreferences::get().reset();
    Onboarding::noteWelcomeShown (Onboarding::Welcome::tourOffer, v);
    Onboarding::answerWelcome (Onboarding::Answer::yes);
    CHECK (Onboarding::getWelcomeDue (v) == Onboarding::Welcome::none);
}

//==============================================================================
/*  onboarding 13 and 14: install A, use it, upgrade to B - the banner comes
    back once, and the person's data is untouched. */
LUTHIER_TEST (Onboarding, anUpgradeWelcomesOnceAndKeepsTheUsersData)
{
    PreservedUi preserved;
    auto& preferences = UiPreferences::get();

    launch ("1.0.0");
    Onboarding::noteWelcomeShown (Onboarding::Welcome::tourOffer, "1.0.0");
    Onboarding::answerWelcome (Onboarding::Answer::never);
    preferences.setString ("advanced.workspaceTabName", "TUNE");
    Onboarding::markTabOpened ("TUNE");

    CHECK (Onboarding::getWelcomeDue ("1.0.0") == Onboarding::Welcome::none);

    // Version B.
    launch ("1.1.0");
    CHECK_MSG (Onboarding::getWelcomeDue ("1.1.0") == Onboarding::Welcome::upgrade,
               "the upgrade did not bring the welcome banner back");
    Onboarding::noteWelcomeShown (Onboarding::Welcome::upgrade, "1.1.0");

    launch ("1.1.0");
    CHECK_MSG (Onboarding::getWelcomeDue ("1.1.0") == Onboarding::Welcome::none, "the upgrade banner came back twice");

    CHECK (preferences.getString ("advanced.workspaceTabName", {}) == "TUNE");
    CHECK (Onboarding::wasTabOpened ("TUNE"));
    CHECK (preferences.getBool (Onboarding::kWelcomeDontAskKey, false));

    // The banner's words.
    WelcomeBanner banner;
    banner.showFor (Onboarding::Welcome::upgrade, "1.1.0");
    CHECK (banner.getMessage() == "Version 1.1.0 installed. What's new?");
    CHECK (HelpContent::findTopic ("whats-new") >= 0);

    banner.showFor (Onboarding::Welcome::tourOffer, "1.1.0");
    CHECK (banner.getMessage() == "Welcome to Luthier. Take the 2-minute tour?");
    CHECK (banner.getFirstButton().getButtonText() == "Yes");
    CHECK (banner.getSecondButton().getButtonText() == "Maybe later");
    CHECK (banner.getThirdButton().getButtonText() == "Don't ask again");
}

//==============================================================================
/*  onboarding 4: "7 launches or 7 calendar days, whichever first". */
LUTHIER_TEST (Onboarding, theDiscoveryWeekIsSevenLaunchesOrSevenDays)
{
    PreservedUi preserved;
    FakeClock clock;

    CHECK_MSG (! Onboarding::isDiscoveryWeek(), "hints were due before any launch");

    for (int i = 1; i <= Onboarding::kDiscoveryLaunches; ++i)
    {
        launch();
        CHECK_MSG (Onboarding::isDiscoveryWeek(), "launch " + juce::String (i) + " was outside the week");
    }

    // A second editor in the same process is not a launch.
    Onboarding::recordLaunch (Onboarding::getCurrentVersion());
    CHECK (Onboarding::getLaunchCount() == Onboarding::kDiscoveryLaunches);

    launch();
    CHECK_MSG (! Onboarding::isDiscoveryWeek(), "the eighth launch still showed hints");

    // Seven days, with few launches.
    UiPreferences::get().reset();
    launch();
    clock.advanceDays (6.9);
    launch();
    CHECK (Onboarding::isDiscoveryWeek());
    clock.advanceDays (0.2);
    CHECK_MSG (! Onboarding::isDiscoveryWeek(), "the eighth day still showed hints");

    // Hints are once each.
    UiPreferences::get().reset();
    launch();
    CHECK (Onboarding::isHintDue ("x"));
    Onboarding::markSeen ("x");
    CHECK (! Onboarding::isHintDue ("x"));
    CHECK (Onboarding::isTabDotDue ("TONE MATCH"));
    Onboarding::markTabOpened ("Tone Match");
    CHECK (! Onboarding::isTabDotDue ("TONE MATCH"));
}

//==============================================================================
/*  onboarding 3: twelve stops in the spec's order, the Techniques stop only
    when the tab exists. */
LUTHIER_TEST (Onboarding, theTourHasTwelveStopsInTheSpecsOrder)
{
    const auto steps = Onboarding::getTourSteps (false);
    const juce::StringArray expected { "play", "preset", "mode", "guitar", "rig", "workspace", "workshop",
                                       "tune", "snapshots", "practice", "slide", "options" };

    CHECK (steps.size() == 12);

    for (int i = 0; i < expected.size() && i < (int) steps.size(); ++i)
        CHECK_MSG (steps[(size_t) i].id == expected[i], "stop " + juce::String (i + 1) + " is " + steps[(size_t) i].id);

    CHECK (steps[0].text == "Hit a key on your MIDI keyboard. The output LED should light.");
    CHECK (steps[7].text == "Type a chord progression, hit play. Add a melody in one click if you want to.");

    const auto withTechniques = Onboarding::getTourSteps (true);
    CHECK (withTechniques.size() == 13);
    CHECK (withTechniques[6].id == "techniques");
    CHECK (withTechniques[7].id == "workshop");

    // The preset the tour suggests is in the factory bank, and so is the one a fresh install starts on.
    auto processor = std::make_unique<LuthierAudioProcessor>();
    CHECK (processor->getPresetManager().indexOfPreset (Onboarding::kTourSuggestedPreset) >= 0);
    CHECK (processor->getPresetManager().indexOfPreset (Onboarding::kFirstRunPreset) >= 0);
}

//==============================================================================
/*  The callout placement: beside the target, inside the window, never over it. */
LUTHIER_TEST (Onboarding, theCalloutSitsBesideItsTargetInsideTheWindow)
{
    const juce::Rectangle<int> area (0, 0, 1200, 720);
    const juce::Point<int> size (TourOverlay::kCalloutWidth, TourOverlay::kCalloutHeight);

    const juce::Rectangle<int> targets[] =
    {
        { 10, 10, 20, 20 },        // top-left LED
        { 1150, 5, 40, 30 },       // top-right switch
        { 500, 690, 200, 24 },     // bottom drawer chevron
        { 0, 60, 300, 640 },       // a tall column
        { 900, 100, 290, 600 },    // column 4
        { 0, 0, 1200, 720 }        // the whole window
    };

    for (const auto& t : targets)
    {
        const auto c = TourOverlay::placeCallout (t, area, size);
        CHECK_MSG (area.contains (c), "callout outside the window for " + t.toString());

        if (t != area)
            CHECK_MSG (! c.intersects (t), "callout covers its target " + t.toString());
    }

    const auto centred = TourOverlay::placeCallout ({}, area, size);
    CHECK (centred.getCentre() == area.getCentre());
}

//==============================================================================
/*  onboarding 14: "Tour walks through all 12 steps without misalignment at
    every UI scale." Window sizes stand in for UI scale here: the editor lays
    out in its own points and the host scales the whole window. At the
    narrowest, Advanced is unavailable and the column stops are centred rather
    than pointing at nothing. */
LUTHIER_TEST (Onboarding, theTourWalksEveryStopAtEverySize)
{
    PreservedUi preserved;

    const juce::Point<int> sizes[] = { { 940, 560 }, { 1200, 720 }, { 1600, 960 }, { 2400, 1440 } };

    for (const auto size : sizes)
    {
        auto processor = std::make_unique<LuthierAudioProcessor>();
        auto editor = makeEditor (*processor, size.x, size.y);
        const auto label = juce::String (size.x) + "x" + juce::String (size.y);
        const bool advancedFits = size.x >= AdvancedPanel::minimumUsableWidth;

        editor->startTour();
        auto& tour = editor->getTour();
        CHECK (tour.isRunning());

        int pointed = 0;

        while (tour.isRunning() && ! tour.isShowingEnd())
        {
            const auto* step = tour.getCurrentStep();
            const auto target = tour.getTargetBounds();
            const auto callout = tour.getCalloutBounds();

            CHECK_MSG (editor->getLocalBounds().contains (callout),
                       label + " " + step->id + ": the callout leaves the window");

            if (! target.isEmpty())
            {
                ++pointed;
                CHECK_MSG (editor->getLocalBounds().contains (target), label + " " + step->id + ": target off screen");

                if (target.getWidth() < size.x / 2 && target.getHeight() < size.y / 2)
                    CHECK_MSG (! callout.intersects (target), label + " " + step->id + ": the callout covers its target");
            }
            else
            {
                CHECK_MSG (callout.getCentre().getDistanceFrom (editor->getLocalBounds().getCentre()) <= 2,
                           label + " " + step->id + ": a stop with no target is not centred");
            }

            tour.next();
        }

        CHECK (tour.isShowingEnd());
        CHECK (tour.getCalloutText() == Onboarding::kTourEndText);

        // Every stop points somewhere where Advanced fits; at 940 the header
        // stops, the guitar, the rig and the drawer still do.
        CHECK_MSG (pointed >= (advancedFits ? 12 : 7), label + ": only " + juce::String (pointed) + " stops found their target");

        tour.next();
        CHECK (! tour.isRunning());
    }
}

//==============================================================================
/*  onboarding 3 and 14: Escape ends it, Back goes back, and a skipped tour
    leaves the plugin playable. */
LUTHIER_TEST (Onboarding, skippingTheTourLeavesThePluginPlayable)
{
    PreservedUi preserved;
    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto editor = makeEditor (*processor, 1200, 720);

    editor->startTour();
    auto& tour = editor->getTour();

    tour.next();
    tour.next();
    CHECK (tour.getStepIndex() == 2);
    tour.back();
    CHECK (tour.getStepIndex() == 1);

    CHECK (editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK_MSG (! tour.isRunning(), "Escape did not end the tour");

    editor->startTour();
    tour.getSkipButton().onClick();
    CHECK (! tour.isRunning());

    CHECK_MSG (renderHasSound (*processor), "the plugin made no sound after the tour was skipped");
}

//==============================================================================
/*  onboarding 2: the banner on a fresh install, and Yes starts the tour; Help
    -> Take the tour starts it any time. */
LUTHIER_TEST (Onboarding, theBannerAndHelpBothStartTheTour)
{
    PreservedUi preserved;
    Onboarding::setAutomaticForTesting (true);

    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto editor = makeEditor (*processor, 1200, 720);

    auto& banner = editor->getWelcomeBanner();
    CHECK_MSG (banner.isVisible(), "no welcome banner on a fresh install");
    CHECK (banner.getKind() == Onboarding::Welcome::tourOffer);
    CHECK (Onboarding::getLaunchCount() == 1);
    CHECK_MSG (banner.getBounds().getY() >= Metrics::headerHeight, "the banner is not under the header");

    banner.getFirstButton().onClick();
    CHECK (! banner.isVisible());
    CHECK_MSG (editor->getTour().isRunning(), "Yes did not start the tour");
    CHECK (Onboarding::getWelcomeDue (Onboarding::getCurrentVersion()) == Onboarding::Welcome::none);

    editor->getTour().skip();

    // A second window in the same process: no second banner.
    auto second = makeEditor (*processor, 1200, 720);
    CHECK (! second->getWelcomeBanner().isVisible());

    Onboarding::setAutomaticForTesting (false);

    // Help -> Take the tour, from the HELP tab.
    auto* advancedPanel = dynamic_cast<AdvancedPanel*> (Onboarding::findComponent (*editor, [] (juce::Component& c)
                                                        { return dynamic_cast<AdvancedPanel*> (&c) != nullptr; }));
    auto* helpTab = advancedPanel != nullptr ? advancedPanel->getHelpTab() : nullptr;

    CHECK (helpTab != nullptr);

    if (helpTab != nullptr)
    {
        CHECK (helpTab->onTakeTour != nullptr);
        helpTab->getTourButton().onClick();
        CHECK_MSG (editor->getTour().isRunning(), "Help -> Take the tour did nothing");
    }
}

//==============================================================================
/*  onboarding 4: the first-week hints are on the things the spec names, and go
    quiet when seen or opened, and after the week. */
LUTHIER_TEST (Onboarding, theFirstWeekMarksTheWrenchTheTabsAndTheHelpIcons)
{
    PreservedUi preserved;
    FakeClock clock;
    launch();

    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->getUiState().advancedMode = true;
    auto editor = makeEditor (*processor, 1400, 840);

    auto& layer = editor->getDiscoveryLayer();
    layer.update();

    auto has = [&layer] (DiscoveryLayer::Kind kind, const juce::String& key)
    {
        for (const auto& m : layer.getMarks())
            if (m.kind == kind && m.key == key)
                return true;

        return false;
    };

    CHECK_MSG (has (DiscoveryLayer::Kind::pulse, "workshop_wrench"), "the wrench does not pulse");
    CHECK_MSG (has (DiscoveryLayer::Kind::pulse, "slide_glyph"), "the Slide glyph does not pulse");
    CHECK_MSG (has (DiscoveryLayer::Kind::pulse, "tune_tab"), "the TUNE tab does not pulse");
    CHECK_MSG (has (DiscoveryLayer::Kind::tabDot, "TONE MATCH"), "an unopened tab has no dot");

    bool helpPulse = false;

    for (const auto& m : layer.getMarks())
        helpPulse = helpPulse || m.key.startsWith ("help_icon_");

    CHECK_MSG (helpPulse, "no ? icon pulses on first sight");

    // A tab opened loses its dot.
    Onboarding::markTabOpened ("TONE MATCH");
    layer.update();
    CHECK (! has (DiscoveryLayer::Kind::tabDot, "TONE MATCH"));

    // A pulse that has run is seen, and does not come back.
    Onboarding::markSeen ("workshop_wrench");
    layer.update();
    CHECK (! has (DiscoveryLayer::Kind::pulse, "workshop_wrench"));

    // After the week, nothing.
    clock.advanceDays (8.0);
    layer.update();
    CHECK_MSG (layer.getMarks().empty(), "hints still showing after the first week");

    // The marks never take a click.
    bool self = true, children = true;
    layer.getInterceptsMouseClicks (self, children);
    CHECK (! self && ! children);
}

//==============================================================================
/*  onboarding 4: "Fresh sound in one click." on the first Randomize hover. */
LUTHIER_TEST (Onboarding, theRandomiseTooltipShowsOnTheFirstHoverOnly)
{
    PreservedUi preserved;
    launch();

    juce::TextButton button ("Randomise");
    button.setTooltip ("Randomise the sound");

    DiscoveryTooltip tip;
    tip.attachTo (&button);

    tip.entered();
    CHECK (button.getTooltip() == DiscoveryTooltip::kText);
    tip.exited();
    CHECK (button.getTooltip() == "Randomise the sound");

    tip.entered();
    CHECK_MSG (button.getTooltip() == "Randomise the sound", "the first-hover tooltip showed twice");

    // The editor attaches it to Easy mode's Randomise.
    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto editor = makeEditor (*processor, 1200, 720);
    CHECK (editor->getRandomiseTooltip().getButton() != nullptr);
    CHECK (editor->getRandomiseTooltip().getButton()->getButtonText() == "Randomise");
}

//==============================================================================
/*  gui-integration 20: every panel with more than one row of controls has a ?
    that opens Help pinned to that panel - every column section, the workspace
    and the Easy strips - and each lands on a real topic. */
LUTHIER_TEST (Onboarding, everyPanelsHelpIconOpensItsOwnTopic)
{
    PreservedUi preserved;
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->getUiState().advancedMode = true;
    auto editor = makeEditor (*processor, 1400, 840);

    auto* advanced = dynamic_cast<AdvancedPanel*> (Onboarding::findComponent (*editor, [] (juce::Component& c)
                                                   { return dynamic_cast<AdvancedPanel*> (&c) != nullptr; }));
    CHECK (advanced != nullptr);

    if (advanced == nullptr)
        return;

    const auto buttons = advanced->getHelpButtons();
    CHECK_MSG (buttons.size() >= 20, "only " + juce::String ((int) buttons.size()) + " ? icons in Advanced");

    for (auto* b : buttons)
    {
        if (b->getTopic() == "HELP")
            continue;

        CHECK_MSG (HelpContent::findTopic (b->getTopic()) >= 0, "the ? on " + b->getTopic() + " finds no topic");
    }

    // A section's ? opens the HELP tab on its topic.
    auto* first = buttons.front();
    const auto expected = HelpContent::getTopic (HelpContent::findTopic (first->getTopic())).id;
    first->clicked();

    CHECK (advanced->getWorkspaceTabName (advanced->getWorkspaceTab()) == "HELP");
    CHECK (advanced->getHelpTab() != nullptr && advanced->getHelpTab()->getShownTopicId() == expected);

    // The workspace ? follows the tab.
    advanced->setWorkspaceTabNamed ("TUNE");
    CHECK (advanced->getWorkspaceHelpButton().getTopic() == "TUNE");
    CHECK (advanced->getWorkspaceHelpButton().isVisible());
    advanced->setWorkspaceTabNamed ("HELP");
    CHECK (! advanced->getWorkspaceHelpButton().isVisible());

    // The Easy strips.
    EasyPanel easy (*processor);
    juce::String opened;
    easy.onOpenHelp = [&opened] (const juce::String& t) { opened = t; };

    for (auto* b : easy.getHelpButtons())
    {
        CHECK_MSG (HelpContent::findTopic (b->getTopic()) >= 0, "the Easy ? on " + b->getTopic() + " finds no topic");
        b->clicked();
        CHECK (opened == b->getTopic());
    }
}

//==============================================================================
/*  onboarding 1 and 14: "Fresh install produces expected default state." */
LUTHIER_TEST (Onboarding, aFreshInstallStartsWhereSectionOneSays)
{
    PreservedUi preserved;
    const auto a11yFile = AccessibilitySettings::getConfigFile();
    const bool a11yExisted = a11yFile.existsAsFile();
    const auto a11yText = a11yExisted ? a11yFile.loadFileAsString() : juce::String();
    const auto a11yState = AccessibilitySettings::get().toVar();

    UiPreferences::getConfigFile().deleteFile();
    a11yFile.deleteFile();
    UiPreferences::get().reset();
    FirstRun::setStateForTesting (false, false);

    {
        auto processor = std::make_unique<LuthierAudioProcessor>();
        auto editor = makeEditor (*processor, 1200, 720);

        CHECK_MSG (processor->getPresetManager().getCurrentPresetName() == Onboarding::kFirstRunPreset,
                   "a fresh install started on " + processor->getPresetManager().getCurrentPresetName());
        CHECK (! processor->getUiState().advancedMode);
        CHECK (! processor->isLiveMode());
        CHECK (! processor->getUiState().practiceDrawerOpen);
        CHECK (FirstRun::isFirstSession());
    }

    if (a11yExisted) a11yFile.replaceWithText (a11yText);
    else             a11yFile.deleteFile();

    AccessibilitySettings::get().fromVar (a11yState);
    AccessibilitySettings::get().dispatchPendingMessages();
}
