/*  First run, "Restore first-run experience" and the first-encounter hints
    (onboarding.md 5, 7, 8, 9, 12; spec/TODO.md 14c).

    These write the user's real config files - ui.json and accessibility.json
    under Documents/Luthier/config - because that is where the behaviour lives.
    PreservedSettings puts both files, the in-memory settings and the locale
    back as they were, whatever the test did.

    The last three tests need the integration edits (DiagnosticsPage's Restore
    button, the hints in TunePanel and WorkshopPanel, the practice drawer in the
    plugin state) and fail until they are in.
*/

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Accessibility/Accessibility.h"
#include "../Accessibility/Localisation.h"
#include "../Presets/PresetManager.h"
#include "../UI/FirstEncounterHint.h"
#include "../UI/FirstRun.h"
#include "../UI/OptionsPages.h"
#include "../UI/RangesUi.h"
#include "../UI/TunePanel.h"
#include "../UI/UiPreferences.h"
#include "../UI/WorkshopPanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    struct PreservedSettings
    {
        PreservedSettings()
            : uiFile (UiPreferences::getConfigFile()),
              a11yFile (AccessibilitySettings::getConfigFile()),
              uiExisted (uiFile.existsAsFile()),
              a11yExisted (a11yFile.existsAsFile()),
              uiText (uiExisted ? uiFile.loadFileAsString() : juce::String()),
              a11yText (a11yExisted ? a11yFile.loadFileAsString() : juce::String()),
              a11yState (AccessibilitySettings::get().toVar())
        {
        }

        ~PreservedSettings()
        {
            if (uiExisted) uiFile.replaceWithText (uiText);
            else           uiFile.deleteFile();

            if (a11yExisted) a11yFile.replaceWithText (a11yText);
            else             a11yFile.deleteFile();

            UiPreferences::get().reset();
            UiPreferences::get().load();

            // toVar carries the locale, so this puts that back too.
            AccessibilitySettings::get().fromVar (a11yState);
            AccessibilitySettings::get().dispatchPendingMessages();

            FirstRun::setStateForTesting (true, false);
        }

        juce::File uiFile, a11yFile;
        bool uiExisted, a11yExisted;
        juce::String uiText, a11yText;
        juce::var a11yState;
    };

    /** A machine that has never run Luthier: no settings files, nothing in memory. */
    void makeFreshInstall()
    {
        UiPreferences::getConfigFile().deleteFile();
        AccessibilitySettings::getConfigFile().deleteFile();
        UiPreferences::get().reset();
        FirstRun::setStateForTesting (false, false);
    }

    FirstRun::OsPreferences os (bool highContrast, bool reducedMotion, double scale,
                                const char* language, const char* region)
    {
        FirstRun::OsPreferences p;
        p.highContrast = highContrast;
        p.reducedMotion = reducedMotion;
        p.displayScale = scale;
        p.language = language;
        p.region = region;
        return p;
    }
}

//==============================================================================
/*  onboarding 5, line by line, as a pure mapping. */
LUTHIER_TEST (FirstRun, theOsPreferencesMapToSectionFivesDefaults)
{
    const auto plain = FirstRun::defaultsFor (os (false, false, 1.0, "en", "US"));
    CHECK (plain.palette == PaletteId::defaultDark);
    CHECK (! plain.reducedMotion);
    CHECK (plain.uiScale == 1.0);
    CHECK (plain.locale == "en");

    const auto accessible = FirstRun::defaultsFor (os (true, true, 2.0, "en", "US"));
    CHECK_MSG (accessible.palette == PaletteId::highContrast, "high-contrast OS mode did not pick the High contrast palette");
    CHECK (accessible.reducedMotion);
    CHECK_MSG (accessible.uiScale == 1.25, "a 200 % display did not snap to 125 %");

    // "over 150 %": 150 itself is not over.
    CHECK (FirstRun::defaultsFor (os (false, false, 1.5, "en", "")).uiScale == 1.0);
    CHECK (FirstRun::defaultsFor (os (false, false, 1.75, "en", "")).uiScale == 1.25);

    // "matches OS locale if in ship set; otherwise en".
    const struct { const char* language; const char* region; const char* expected; } cases[] =
    {
        { "en", "GB", "en-GB" }, { "en", "AU", "en" },     { "pt", "BR", "pt-BR" }, { "pt", "PT", "en" },
        { "es", "ES", "es" },    { "es", "MX", "es-419" }, { "es", "US", "es-419" },
        { "zh", "CN", "zh-CN" }, { "zh", "TW", "zh-TW" },  { "zh", "HK", "zh-TW" }, { "zh", "SG", "zh-CN" },
        { "fr", "CA", "fr" },    { "DE", "at", "de" },     { "ja", "JP", "ja" },    { "sv", "SE", "en" },
        { "", "", "en" }
    };

    for (const auto& c : cases)
        CHECK_MSG (FirstRun::matchShipLocale (c.language, c.region) == c.expected,
                   juce::String (c.language) + "-" + c.region + " matched "
                     + FirstRun::matchShipLocale (c.language, c.region) + ", not " + c.expected);
}

//==============================================================================
/*  Applied once, on a fresh install, and never over a person's own choices. */
LUTHIER_TEST (FirstRun, appliesOnceOnAFreshInstallAndNeverAgain)
{
    PreservedSettings preserved;
    auto& settings = AccessibilitySettings::get();

    // --- a fresh install ---------------------------------------------------------------
    makeFreshInstall();
    CHECK (FirstRun::isFirstRun());
    CHECK (! FirstRun::isFirstSession());

    CHECK (FirstRun::applyIfFirstRun (os (true, true, 2.0, "de", "DE")));
    CHECK (settings.getPalette() == PaletteId::highContrast);
    CHECK (settings.isReducedMotion());
    CHECK (settings.getUiScale() == 1.25);

    // The locale is applied through Localisation::setLocale, which needs the
    // locale's catalog beside the plugin (Resources/i18n/de.json). Only English
    // is compiled in and no catalog ships in this tree yet, so the live locale
    // is "de" where the catalog exists and stays "en" where it does not; the
    // mapping itself is checked above (DECISIONS: FirstRun locale).
    CHECK (Localisation::get().getLocale() == "de" || Localisation::get().getLocale() == "en");
    CHECK_MSG (FirstRun::isFirstSession(), "the session that applied the defaults is not the first session");
    CHECK (UiPreferences::get().getBool (FirstRun::kCompletedKey, false));

    // A second editor in the same process applies nothing.
    CHECK (! FirstRun::applyIfFirstRun (os (false, false, 1.0, "en", "US")));

    // --- the next launch: the person's choices stand -----------------------------------
    settings.setPalette (PaletteId::light);
    settings.save();
    FirstRun::setStateForTesting (false, false);

    CHECK (! FirstRun::isFirstRun());
    CHECK (! FirstRun::applyIfFirstRun (os (true, false, 1.0, "en", "US")));
    CHECK_MSG (settings.getPalette() == PaletteId::light, "a later launch overwrote a chosen palette");
    CHECK (! FirstRun::isFirstSession());

    // --- an upgrade from a build without first-run tracking -----------------------------
    // Settings on disk and no completed flag: they are the person's, so nothing
    // is applied and the question is settled.
    UiPreferences::get().reset();
    UiPreferences::getConfigFile().deleteFile();
    FirstRun::setStateForTesting (false, false);

    CHECK (AccessibilitySettings::getConfigFile().existsAsFile());
    CHECK (! FirstRun::applyIfFirstRun (os (true, true, 2.0, "fr", "FR")));
    CHECK (settings.getPalette() == PaletteId::light);
    CHECK (UiPreferences::get().getBool (FirstRun::kCompletedKey, false));
}

//==============================================================================
/*  onboarding 12 and TODO 14c: Restore clears every user-global setting,
    including the range explainer's flag (onboarding 7), makes the next launch a
    first run, and leaves the libraries alone. */
LUTHIER_TEST (FirstRun, restoreClearsTheSettingsAndTheOneTimeFlagsAndKeepsTheLibraries)
{
    PreservedSettings preserved;
    auto& settings = AccessibilitySettings::get();
    auto& preferences = UiPreferences::get();

    // Someone who has used the plugin for a while.
    preferences.setBool (FirstRun::kCompletedKey, true);
    preferences.setBool (RangesUi::kExplainerShownKey, true);
    preferences.setBool (FirstEncounterHint::kTuneKey, true);
    preferences.setBool (FirstEncounterHint::kWorkshopKey, true);
    preferences.setString ("advanced.workspaceTabName", "TONE MATCH");

    settings.setPalette (PaletteId::light);
    settings.setUiScale (1.5);
    settings.setVerbosity (AccessibilitySettings::Verbosity::verbose);
    const juce::KeyPress newKey ('k', juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier, 0);
    CHECK (settings.rebind ("panic", newKey));
    Localisation::get().setLocale ("fr");
    settings.save();

    // A library file that must survive.
    const auto sentinel = PresetManager::getUserPresetFolder().getChildFile ("__first_run_sentinel.luthierpreset");
    sentinel.getParentDirectory().createDirectory();
    CHECK (sentinel.replaceWithText ("{}"));

    FirstRun::restoreFirstRunExperience();

    // TODO 14c.
    CHECK_MSG (! preferences.getBool (RangesUi::kExplainerShownKey, false),
               "Restore left ranges_first_unlock_explained set, so the explainer will never show again");

    CHECK (! preferences.getBool (FirstEncounterHint::kTuneKey, false));
    CHECK (! preferences.getBool (FirstEncounterHint::kWorkshopKey, false));
    CHECK (preferences.getString ("advanced.workspaceTabName", {}).isEmpty());

    CHECK (settings.getPalette() == PaletteId::defaultDark);
    CHECK (settings.getUiScale() == 1.0);
    CHECK_MSG (settings.getVerbosity() == AccessibilitySettings::Verbosity::standard,
               "Restore left verbosity at something other than a fresh install's Standard");
    CHECK (settings.findShortcut ("panic") != nullptr && ! settings.findShortcut ("panic")->isRebound());
    CHECK (Localisation::get().getLocale() == "en");

    // On disk as well as in memory: the next launch reads the files.
    CHECK (UiPreferences::getConfigFile().loadFileAsString().contains (FirstRun::kPendingKey));
    CHECK (! AccessibilitySettings::getConfigFile().loadFileAsString().contains ("\"panic\""));

    CHECK_MSG (sentinel.existsAsFile(), "Restore deleted a file from the preset library");
    sentinel.deleteFile();

    // The next launch is a first run, and applying it settles it.
    FirstRun::setStateForTesting (false, false);
    CHECK (FirstRun::isFirstRun());
    CHECK (FirstRun::applyIfFirstRun (os (false, false, 1.0, "en", "GB")));
    CHECK (Localisation::get().getLocale() == "en-GB" || Localisation::get().getLocale() == "en");   // as above
    CHECK (! preferences.getBool (FirstRun::kPendingKey, false));
    CHECK (! FirstRun::isFirstRun());
}

//==============================================================================
/*  onboarding 7: the explainer's words are the spec's, and the button is
    "OK, got it". Showing it needs a window on screen, which a console runner
    does not have, so what is checked here is the text and the flag Restore
    clears (above). */
LUTHIER_TEST (FirstRun, theRangeExplainerSaysSectionSevensWords)
{
    CHECK (juce::String (RangesUi::kExplainerText)
             == "This control has a stock range that matches real guitars and an advanced range for "
                "exaggerated effects. You're leaving the stock range. Values marked with * play back the "
                "same; presets with any advanced values show a padlock icon. Options -> Ranges lets you "
                "set the default.");

    CHECK (juce::String (RangesUi::kExplainerShownKey) == "ranges_first_unlock_explained");
}

//==============================================================================
/*  onboarding 8 and 9: once, in the first session, dismissible. */
LUTHIER_TEST (FirstEncounterHint, showsOnceAndOnlyInTheFirstSession)
{
    PreservedSettings preserved;

    // Not the first session: nothing, however new the hint is.
    UiPreferences::get().setBool (FirstEncounterHint::kTuneKey, false);
    FirstRun::setStateForTesting (true, false);

    FirstEncounterHint early (FirstEncounterHint::kTuneKey, FirstEncounterHint::kTuneText);
    CHECK (! early.showIfDue());
    CHECK (! early.isVisible());

    // The first session.
    makeFreshInstall();
    CHECK (FirstRun::applyIfFirstRun (os (false, false, 1.0, "en", "US")));

    FirstEncounterHint hint (FirstEncounterHint::kTuneKey, FirstEncounterHint::kTuneText);
    int changes = 0;
    hint.onShownOrDismissed = [&changes] { ++changes; };

    CHECK (hint.showIfDue());
    CHECK (hint.isVisible() && changes == 1);
    CHECK (UiPreferences::get().getBool (FirstEncounterHint::kTuneKey, false));

    // The export key is the live binding, not a placeholder.
    CHECK (hint.getText().contains (AccessibilitySettings::get().findShortcut ("export")->key.getTextDescription()));
    CHECK (! hint.getText().contains ("{key:"));

    // Got it.
    CHECK (hint.getDismissButton().onClick != nullptr);
    hint.getDismissButton().onClick();
    CHECK (! hint.isVisible() && changes == 2);

    // Once: not again here, nor in another panel or instance.
    CHECK (! hint.showIfDue());

    FirstEncounterHint again (FirstEncounterHint::kTuneKey, FirstEncounterHint::kTuneText);
    CHECK (! again.showIfDue());

    // The Workshop's is separate.
    FirstEncounterHint workshop (FirstEncounterHint::kWorkshopKey, FirstEncounterHint::kWorkshopText);
    CHECK (workshop.showIfDue());
}

//==============================================================================
/*  Integration: the Diagnostics page's Restore does the restore, and tells the
    processor the restored range preference (it cannot read UiPreferences). The
    button itself opens a confirmation, which a console runner cannot answer, so
    the test calls what the confirmation calls. */
LUTHIER_TEST (FirstRun, theDiagnosticsPageRestores)
{
    PreservedSettings preserved;
    auto processor = std::make_unique<LuthierAudioProcessor>();

    UiPreferences::get().setBool (RangesUi::kExplainerShownKey, true);
    RangesUi::setRandomiseRespectsStock (*processor, false);
    CHECK (! processor->getRandomiseRespectsStock());

    DiagnosticsPage page (*processor);
    CHECK (page.getRestoreFirstRunButton().onClick != nullptr);

    page.restoreFirstRun();

    CHECK (! UiPreferences::get().getBool (RangesUi::kExplainerShownKey, false));
    CHECK (FirstRun::isFirstRun());
    CHECK_MSG (processor->getRandomiseRespectsStock(), "the processor kept the pre-restore range preference");
}

//==============================================================================
/*  Integration: the hints sit at the top of the TUNE tab and under the bench
    header, and the Workshop says "Escape closes." only where Escape does close
    it - the Easy-mode overlay. */
LUTHIER_TEST (FirstEncounterHint, theTuneTabAndTheBenchCarryTheirHints)
{
    PreservedSettings preserved;
    auto processor = std::make_unique<LuthierAudioProcessor>();

    makeFreshInstall();
    CHECK (FirstRun::applyIfFirstRun (os (false, false, 1.0, "en", "US")));

    {
        TunePanel tune (*processor, processor->getTunePlayer(), processor->getTuneSession());
        tune.setSize (480, tune.getPreferredHeight());
        const int before = tune.getHeight();

        tune.showFirstEncounterHintIfDue();

        auto& hint = tune.getFirstEncounterHint();
        CHECK (hint.isVisible());
        CHECK_MSG (tune.getHeight() >= before + FirstEncounterHint::kHeight,
                   "the TUNE tab did not make room for its hint");
        CHECK_MSG (hint.getY() < tune.getTitleEditor().getY(), "the hint is not at the top of the tab");

        hint.getDismissButton().onClick();
        CHECK (! hint.isVisible());
        CHECK (tune.getHeight() == before);
    }

    {
        WorkshopOverlay overlay (*processor);
        overlay.setSize (1180, 720);
        overlay.getPanel().showFirstEncounterHintIfDue();

        auto& hint = overlay.getPanel().getFirstEncounterHint();
        CHECK (hint.isVisible());
        CHECK_MSG (hint.getText().endsWith ("Escape closes."), "the overlay's hint does not say Escape closes it");
        CHECK (! hint.getBounds().isEmpty());
    }

    {
        // Shown once already (above), so a fresh key for the tab's own check.
        UiPreferences::get().setBool (FirstEncounterHint::kWorkshopKey, false);

        WorkshopPanel bench (*processor);
        bench.setSize (1000, 600);
        bench.showFirstEncounterHintIfDue();

        CHECK (bench.getFirstEncounterHint().isVisible());
        CHECK_MSG (! bench.getFirstEncounterHint().getText().contains ("Escape"),
                   "the WORKSHOP tab's hint promises an Escape that does not close the tab");
    }
}

//==============================================================================
/*  Integration: onboarding 11, "restore last ... practice drawer state". The
    drawer's open state travels in the plugin state like the window size, so a
    session closed with the drawer open reopens with it open. */
LUTHIER_TEST (ReturningUser, thePracticeDrawerComesBackAsItWasLeft)
{
    juce::MemoryBlock state;

    {
        auto processor = std::make_unique<LuthierAudioProcessor>();
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
        CHECK (editor != nullptr);

        if (editor == nullptr)
            return;

        editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);

        const auto* toggle = AccessibilitySettings::get().findShortcut ("togglePractice");
        CHECK (toggle != nullptr && editor->keyPressed (toggle->key));

        editor.reset();
        processor->getStateInformation (state);
    }

    auto reopened = std::make_unique<LuthierAudioProcessor>();
    reopened->setStateInformation (state.getData(), (int) state.getSize());
    CHECK (reopened->getUiState().practiceDrawerOpen);

    std::unique_ptr<juce::AudioProcessorEditor> editor (reopened->createEditor());

    if (editor == nullptr)
        return;

    PracticePanel* drawer = nullptr;

    for (auto* child : editor->getChildren())
        if (auto* p = dynamic_cast<PracticePanel*> (child))
            drawer = p;

    CHECK_MSG (drawer != nullptr && drawer->isOpen(), "the practice drawer reopened closed");
}
