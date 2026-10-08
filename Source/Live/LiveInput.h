#pragma once

/*  The live surface's MIDI input side (live-performance.md sections 2, 5, 6, 8
    and 9). SPEC-SWEEP: LP-1, LP-11, LP-14, LP-21, LP-26, LP-29, LP-37.

    (The expression-pedal calibration on the MIDI path is Live/ExpressionStage.)

      - LiveActionMap turns a learned CC into a live action: next / previous /
        by-value snapshot, tap tempo, kill switch, panic and setlist steps. The
        audio thread only flips atomics; the message thread carries the actions
        out on its next tick (the kill switch is the exception: it is an atomic
        itself, so it engages inside the block the CC arrived in).

    The CC assignments are user-global, like the pedal calibrations: they
    describe the hardware on the floor, not the sound (see
    docs/coverage/sweep-notes/ui.md).
*/

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <functional>

namespace luthier
{

class KillSwitch;

//==============================================================================
/** The live actions a CC can be assigned to (live-performance 2, 4, 5, 6, 9). */
enum class LiveAction
{
    snapshotNext = 0,
    snapshotPrevious,
    snapshotByValue,
    tapTempo,
    killSwitch,
    panic,
    setlistNext,
    setlistPrevious,
    numActions
};

/** A stable id ("live.snapshotNext") for saving, and a display name. */
const char* getLiveActionId (LiveAction action) noexcept;
const char* getLiveActionName (LiveAction action) noexcept;

//==============================================================================
class LiveActionMap
{
public:
    static constexpr int kNumActions = (int) LiveAction::numActions;

    /** What the message thread does with each action. Set once, by the
        processor. */
    struct Handlers
    {
        std::function<void()> nextSnapshot, previousSnapshot, panic,
                              setlistNext, setlistPrevious;
        std::function<void (int)> recallSnapshot;
        std::function<void (double)> tapAt;          ///< seconds, audio-thread clock
        std::function<void (int)> onCcAssigned;     ///< a CC now belongs to a live action
    };

    LiveActionMap();

    Handlers handlers;

    /** The switch the kill action drives directly from the audio thread. */
    void setKillSwitch (KillSwitch* k) noexcept { killSwitch = k; }

    //==========================================================================
    // Message thread.

    void assign (LiveAction action, int cc);
    void clear (LiveAction action);
    void clearAll();

    /** The CC an action answers to, or -1. */
    int getCcFor (LiveAction action) const noexcept;

    /** The action a CC drives, or -1. */
    int getActionForCc (int cc) const noexcept;

    /** Arms learning: the next CC that arrives is assigned to `action`. */
    void beginLearning (LiveAction action) noexcept;
    void cancelLearning() noexcept;
    int getLearningAction() const noexcept { return learningAction.load (std::memory_order_relaxed); }

    /** Carries out whatever the audio thread asked for since the last call,
        including committing a learned CC. Call from the processor's timer. */
    void service();

    /** Bumped on every change, for a UI to poll. */
    int getVersion() const noexcept { return version.load (std::memory_order_relaxed); }

    //==========================================================================
    // Audio thread.

    /** True when a CC number is the live surface's: assigned, or about to be
        learned. */
    bool wants (int cc) const noexcept;

    /** Acts on one controller. Returns true when the message was consumed. */
    bool handleController (int cc, int value) noexcept;

    //==========================================================================
    juce::var toVar() const;
    void fromVar (const juce::var& state);

    /** ~/Documents/Luthier/config/live-actions.json by default; tests point it
        somewhere temporary. */
    void setConfigFile (const juce::File& f) { configFile = f; }
    const juce::File& getConfigFile() const noexcept { return configFile; }
    static juce::File getDefaultConfigFile();

    bool load();
    bool save() const;

private:
    std::array<std::atomic<int>, 128> actionForCc;
    std::atomic<int> learningAction { -1 };
    std::atomic<int> pendingLearn { -1 };   ///< action * 128 + cc

    std::array<std::atomic<int>, kNumActions> pendingCount;
    std::atomic<int> pendingSnapshotIndex { -1 };
    std::atomic<double> pendingTapSeconds { -1.0 };

    std::atomic<int> version { 0 };

    KillSwitch* killSwitch = nullptr;
    juce::File configFile;

    JUCE_DECLARE_NON_COPYABLE (LiveActionMap)
};

} // namespace luthier
