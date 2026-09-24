#pragma once

/*  A one-time inline hint (onboarding.md 8 and 9).

    "If the user opens the TUNE tab in their first session, a one-time inline
    hint appears at the top of the tab ... Dismissible." Section 9 says the
    same of the Workshop bench's header. One component serves both: a strip of
    text with a Got it button, which the host lays out above its content while
    it is visible.

    Owed means two things: this is the first session (FirstRun::isFirstSession,
    so a person who meets the TUNE tab on day five is not lectured), and the
    hint has never been shown (a UiPreferences flag, so a second instance or a
    second editor does not show it again). It is recorded as shown the moment
    it appears, the same rule as the range explainer: a host killed with the
    hint on screen has still shown it. "Restore first-run experience" empties
    UiPreferences, which re-arms it along with everything else one-time.

    The host calls showIfDue() from its timer while it is on screen. That is
    what "opens" means here: the tab chosen, the overlay raised, or Advanced
    mode switched on with the tab already chosen - three routes, one check.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"

namespace luthier
{

class FirstEncounterHint : public juce::Component
{
public:
    /** UiPreferences keys and onboarding's words. {key:...} is the live
        binding (HelpContent::resolveKeys), so a rebound export key is the one
        the hint names. */
    static constexpr const char* kTuneKey  = "first_encounter_tune_shown";
    static constexpr const char* kTuneText =
        "Type a chord progression like \"Am F C G\", hit play, and you have a tune. Add a melody in "
        "one click. When you're ready, {key:export} exports.";

    static constexpr const char* kWorkshopKey  = "first_encounter_workshop_shown";
    static constexpr const char* kWorkshopText =
        "This is every part of your guitar. Click any part to swap it. Try Alt-hover on a card to hear "
        "it before committing.";

    /** Height the host reserves while the hint shows. */
    static constexpr int kHeight = 40;

    FirstEncounterHint (const juce::String& preferenceKey, const juce::String& text);

    /** Shows the hint if it is owed, and records it as shown. Returns true if
        it is on show after the call. Cheap when it is not owed: an atomic and
        a map lookup, so a 30 Hz timer can call it. */
    bool showIfDue();

    /** Got it. The host re-lays out through onShownOrDismissed. */
    void dismiss();

    /** Replaces the text; for the Workshop, which adds "Escape closes." only
        where Escape does close it (the Easy-mode overlay). */
    void setText (const juce::String& newText);

    /** The text as shown, with keys resolved. */
    juce::String getText() const;

    juce::Button& getDismissButton() noexcept { return dismissButton; }

    /** Called when the hint appears or goes, so the host can re-lay out. */
    std::function<void()> onShownOrDismissed;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String preferenceKey, text;
    juce::TextButton dismissButton { "Got it" };
    juce::Rectangle<int> textBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FirstEncounterHint)
};

} // namespace luthier
