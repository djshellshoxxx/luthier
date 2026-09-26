#pragma once

/*  First run, and "Restore first-run experience" (onboarding.md 5, 7, 8, 9, 12;
    spec/TODO.md 14c).

    Three things:

      FIRST-RUN DEFAULTS (5) - on the first launch, the settings that follow
          the operating system: reduced motion, the high-contrast palette, a
          125 % UI scale on a display scaled past 150 %, and the locale when it
          is one of the fifteen that ship. Applied once, by the editor, and
          never again: a later OS change is the person's to make in Options.

      FIRST SESSION (8, 9) - the TUNE and Workshop hints appear only "in their
          first session". That is this process, from the moment the defaults
          were applied; a host that is closed and reopened is a second session.

      RESTORE (12) - clears the user-global settings, so the next launch
          behaves as if freshly installed: UiPreferences (every one-time flag
          in it, including onboarding 7's `ranges_first_unlock_explained`,
          which TODO 14c names; the last workspace tab; the range preferences;
          the MIDI export defaults) and AccessibilitySettings (palette, scale,
          motion, verbosity, font, locale, every rebound shortcut). The preset,
          guitar, tune and part libraries are files the person made and are not
          touched, nor is anything under Presets, Guitars, Tunes or Parts.

    How a first run is recognised. The installer's `.installed_version` marker
    (installer.md 6) does not exist yet, so: ui.json asks for one (`first_run_
    pending`, which Restore writes), or neither ui.json nor accessibility.json
    has ever been written. A person upgrading from a build without this who
    has either file has made choices, and those are kept - the defaults are
    only ever applied over defaults.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Accessibility/Accessibility.h"

namespace luthier
{

namespace FirstRun
{
    /** UiPreferences keys. */
    inline constexpr const char* kCompletedKey = "first_run_completed";
    inline constexpr const char* kPendingKey   = "first_run_pending";

    //==========================================================================
    /** What the operating system says about the person (onboarding 5). */
    struct OsPreferences
    {
        bool highContrast = false;
        bool reducedMotion = false;
        double displayScale = 1.0;   ///< the primary display, 1.0 = 100 %
        juce::String language;       ///< ISO 639, "pt"
        juce::String region;         ///< ISO 3166, "BR"; may be empty
    };

    /** Reads them. Where a platform cannot say, the answer is the default
        (off, 100 %, English), which is what onboarding 5 asks for anyway. */
    OsPreferences readOsPreferences();

    /** What onboarding 5 makes of them. Pure, so it is testable. */
    struct Defaults
    {
        PaletteId palette = PaletteId::defaultDark;
        bool reducedMotion = false;
        double uiScale = 1.0;
        juce::String locale { "en" };
    };

    Defaults defaultsFor (const OsPreferences& os);

    /** The ship locale (accessibility.md 6) an OS language and region match,
        or "en". An exact tag first ("en-GB"), then the tag that covers the
        region ("es-419", "zh-TW"), then the bare language ("fr"). */
    juce::String matchShipLocale (const juce::String& language, const juce::String& region);

    //==========================================================================
    /** True when this launch is a first run (see the file comment). */
    bool isFirstRun();

    /** Applies defaultsFor(os) and records the run, if this is a first run.
        Returns true if it did. Message thread; the editor calls it once on
        construction. Every later call in the process is a no-op. */
    bool applyIfFirstRun (const OsPreferences& os);
    bool applyIfFirstRun();

    /** True from the moment applyIfFirstRun applied, for the rest of the
        process: onboarding 8 and 9's "first session". */
    bool isFirstSession() noexcept;

    /** Tests only: sets the process-wide flags. `checked` true makes every
        later applyIfFirstRun a no-op, which is how the test runner keeps an
        editor built by an unrelated test from applying the machine's OS
        preferences to the settings files mid-run. */
    void setStateForTesting (bool checked, bool firstSession) noexcept;

    //==========================================================================
    /** onboarding 12: clears the user-global settings and asks the next launch
        to behave as a first run. Libraries are not touched. The caller confirms
        first (onboarding 12: "Confirms with a modal"). */
    void restoreFirstRunExperience();
}

} // namespace luthier
