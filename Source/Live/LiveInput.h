#pragma once

/*  The live surface's MIDI input side (live-performance.md sections 2, 5, 6, 8
    and 9). SPEC-SWEEP: LP-1, LP-11, LP-14, LP-21, LP-26, LP-29, LP-33, LP-34,
    LP-37.

    Two jobs, both on the audio thread's MIDI path and both lock-free.

      - ExpressionInput runs at the very top of the MIDI chain. It lets the
        calibration wizard see the pedal (observe) and rewrites every calibrated
        CC through its calibration (map), so MIDI Learn, the modulation matrix
        and everything else downstream see a pedal that reaches 0 and 127.

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

class ExpressionCalibrationSet;
class KillSwitch;

//==============================================================================
/** live-performance 8: the calibration applied to the incoming CC stream. */
class ExpressionInput
{
public:
    ExpressionInput();

    /** Message thread: copies the calibrations into the lock-free lookup the
        audio thread reads. Cheap enough to call on every change. */
    void rebuild (const ExpressionCalibrationSet& set);

    /** Audio thread. Records what the wizard is waiting for and rewrites the
        values of calibrated CCs in place. */
    void processMidi (juce::MidiBuffer& midi) noexcept;

    /** Message thread: hands the wizard the extremes the audio thread saw since
        the last call. Returns true when anything was fed to it. */
    bool feedWizard (ExpressionCalibrationSet& set);

    /** Message thread: rebuilds the table when the set has changed since the
        last rebuild. */
    void syncWith (const ExpressionCalibrationSet& set);

    bool isCalibrated (int cc) const noexcept;

    /** The value a raw CC turns into, for tests and the Options page. */
    int mapForTest (int cc, int raw) const noexcept;

private:
    std::array<std::atomic<bool>, 128> calibrated;
    std::array<std::atomic<std::uint8_t>, 128 * 128> table;

    std::atomic<int> wizardCc { -1 };
    std::atomic<int> seenMin { 128 }, seenMax { -1 };

    juce::String lastSignature;

    JUCE_DECLARE_NON_COPYABLE (ExpressionInput)
};

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
