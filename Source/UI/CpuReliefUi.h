#pragma once

/*  The user-facing half of performance-budget.md 8's CPU relief ladder.

    The ladder itself (Support/CpuRelief) runs on the audio thread. What the
    user sees and chooses lives here:

    - relief 7 ("drop least-recently-active string audio") is opt-out in
      Options -> Diagnostics, default on, remembered per user;
    - while relief 7 is in force a "CPU limit" banner says so, once per
      episode, and is withdrawn when the load falls back.

    The processor cannot read UiPreferences, so the editor tells it the saved
    choice when it opens (the same arrangement as randomiseRespectsStock).
*/

#include <juce_core/juce_core.h>

namespace luthier
{

class LuthierAudioProcessor;

namespace CpuReliefUi
{
    /** Options -> Diagnostics' opt-out, from the user's preferences (default on). */
    bool isStringDropAllowed();

    /** Saves the choice and tells the processor's ladder. */
    void setStringDropAllowed (LuthierAudioProcessor& processor, bool allowed);

    /** Tells the processor the saved choice (the editor, on opening). */
    void applySavedChoice (LuthierAudioProcessor& processor);

    /** What the banner should do this tick, given whether it is showing. */
    enum class BannerAction { none, post, withdraw };
    BannerAction bannerAction (bool due, bool showing) noexcept;

    /** The banner's words. */
    juce::String bannerMessage();

    constexpr const char* kBannerId = "cpu-limit";
}

} // namespace luthier
