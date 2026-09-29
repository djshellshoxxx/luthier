#pragma once

/*  Snapshot banks and morphing (live-performance.md sections 1, 2 and 3).

    A snapshot is a whole parameter state under a label, stored inside the preset.
    Recalling one is the live performer's equivalent of changing preset, and the
    thing that matters about it is that it must not be audible as a switch: held
    notes keep ringing, tails keep decaying, and the amp crossfades rather than
    jumping.

    Two things make that possible here:

      1. A recall writes parameters, and nothing else. It never resets a module,
         never clears a delay line and never stops a note, so everything that was
         ringing carries on ringing. That is what live-performance 1 means by
         "held notes are not retriggered" and by tails being preserved.

      2. Continuous parameters are moved to their new values over
         `crossfadeMs` rather than set outright, and discrete ones are switched
         once, at the halfway point. A choice parameter has no meaningful value
         between two settings, so interpolating it would sweep through every amp
         model on the way past; switching it once at the midpoint is both
         cheaper and what the spec asks for.

    Morphing is the same interpolation driven by a continuous source instead of
    by a timer. The two share `applyBlend`, so a morph position of 0.5 and a
    recall halfway through its crossfade produce identical parameter values.
*/

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>
#include <atomic>

namespace luthier
{

//==============================================================================
/** How a morph travels from A to B (live-performance 3). */
enum class MorphCurve
{
    linear = 0,
    sCurve,
    exponential,
    bezier,        ///< Four-point, the control points below.
    numCurves
};

const char* getMorphCurveName (MorphCurve curve) noexcept;

/** Maps a raw 0..1 morph position through the curve. */
double applyMorphCurve (double position, MorphCurve curve,
                        double bezierX1 = 0.25, double bezierY1 = 0.1,
                        double bezierX2 = 0.75, double bezierY2 = 0.9) noexcept;

//==============================================================================
/** One snapshot (live-performance 1). */
struct Snapshot
{
    /** live-performance 1: a label of up to 32 characters. */
    static constexpr int kMaxLabelLength = 32;

    /** Sixteen colour tags, matching the theme's accents. */
    static constexpr int kNumColourTags = 16;

    juce::String label;
    int colourTag = 0;

    /** Parameter id -> normalised value. The same shape a preset stores, so the
        two can be compared and copied without a conversion step. */
    juce::var parameters;

    /** The state of the systems that do not live in the parameter tree. */
    juce::var modMatrix;
    juce::var rhythm;
    juce::var bypasses;

    bool isEmpty() const noexcept { return parameters.getDynamicObject() == nullptr; }

    juce::var toVar() const;
    static Snapshot fromVar (const juce::var& state);
};

//==============================================================================
/** The bank of snapshots a preset carries, plus the recall and morph machinery
    that drives them. */
class SnapshotBank : public juce::ChangeBroadcaster
{
public:
    /** live-performance 1: a preset holds up to 128 snapshots, which is also
        exactly the range of a MIDI program change. */
    static constexpr int kMaxSnapshots = 128;

    explicit SnapshotBank (juce::AudioProcessor& processor);
    ~SnapshotBank() override;

    //==========================================================================
    int getNumSnapshots() const noexcept { return (int) snapshots.size(); }

    const Snapshot& getSnapshot (int index) const noexcept;

    /** Replaces a snapshot, growing the bank with empty snapshots if needed.
        Returns false only if the index is out of the bank's range. */
    bool setSnapshot (int index, const Snapshot& snapshot);

    /** Captures the live parameter state into a snapshot at `index`. */
    bool capture (int index, const juce::String& label = {}, int colourTag = -1);

    bool remove (int index);
    void clear();

    void setLabel (int index, const juce::String& label);
    void setColourTag (int index, int colourTag);

    //==========================================================================
    /** live-performance 1: the crossfade a recall uses, 0 to 500 ms. */
    void setCrossfadeMs (double ms) noexcept;
    double getCrossfadeMs() const noexcept { return crossfadeMs; }

    /** Starts a recall. Returns false if the index holds no snapshot.

        With a crossfade of zero this applies the snapshot outright and finishes
        before returning; otherwise `advance` carries it the rest of the way. */
    bool recall (int index);

    /** Message thread. Moves an in-flight recall along by this much time. */
    void advance (double secondsElapsed);

    /** Audio thread: adds a block's duration to the time the next
        advancePending() applies. The crossfade keeps the audio thread's clock,
        but its work - a String-keyed read of every parameter, the midpoint's
        module fromVar calls, the change message - is done on the message
        thread, which also owns recallFrom and the snapshot list; doing it on
        the audio thread raced recall() and state restores (use-after-free). */
    void noteAudioTime (double seconds) noexcept
    {
        auto current = pendingSeconds.load (std::memory_order_relaxed);
        while (! pendingSeconds.compare_exchange_weak (current, current + seconds,
                                                       std::memory_order_relaxed)) {}
    }

    /** Message thread (the processor's timer): applies the audio time noted
        since the last call. */
    void advancePending()
    {
        const double seconds = pendingSeconds.exchange (0.0, std::memory_order_relaxed);

        if (recallActive)
            advance (seconds);
    }

    bool isRecalling() const noexcept { return recallActive; }
    int getCurrentSnapshot() const noexcept { return currentIndex; }

    /** How far through the current recall, 0 to 1. */
    double getRecallProgress() const noexcept { return recallPosition; }

    //==========================================================================
    // Morph (live-performance 3).

    void setMorphSlots (int slotA, int slotB);
    int getMorphSlotA() const noexcept { return morphA; }
    int getMorphSlotB() const noexcept { return morphB; }

    void setMorphEnabled (bool shouldMorph);
    bool isMorphEnabled() const noexcept { return morphEnabled; }

    void setMorphCurve (MorphCurve curve) noexcept { morphCurve = curve; }
    MorphCurve getMorphCurve() const noexcept { return morphCurve; }

    void setBezierControlPoints (double x1, double y1, double x2, double y2) noexcept;

    /** Moves the morph. Ignored when morphing is off or the slots are empty. */
    void setMorphPosition (double position);
    double getMorphPosition() const noexcept { return morphPosition; }

    /** live-performance 3: a parameter can be excluded from the morph, in which
        case it takes snapshot A's value throughout. */
    void setParameterExcludedFromMorph (const juce::String& parameterId, bool excluded);
    bool isParameterExcludedFromMorph (const juce::String& parameterId) const;
    juce::StringArray getMorphExclusions() const { return morphExclusions; }

    //==========================================================================
    /** True for parameters that have no meaningful value between two settings:
        choices and booleans. These hard-switch at the halfway point. */
    static bool isDiscrete (const juce::AudioProcessorParameter& parameter) noexcept;

    //==========================================================================
    /*  Called when a recall reaches the point at which the systems that do not
        live in the parameter tree - the modulation matrix, the rhythm engine and
        the effect bypasses - should take their new state. That is the crossfade
        midpoint, or immediately when the crossfade is zero.

        The bank stores those blobs but deliberately knows nothing about their
        shape: the processor owns those modules, so it is the processor that
        unpacks them. Adding a module to a snapshot never means editing this
        class. */
    std::function<void (const Snapshot&)> onNonParameterState;

    //==========================================================================
    juce::var toVar() const;
    void fromVar (const juce::var& state);

private:
    /** Writes `blend` of the way from `from` to `to` into the parameter tree.

        Discrete parameters take `to` once blend passes 0.5 and `from` before
        that. Continuous ones interpolate. A parameter that only one side names
        is left alone, so a snapshot saved by an older build cannot zero out a
        parameter it never knew about. */
    void applyBlend (const juce::var& from, const juce::var& to, double blend,
                     bool honourExclusions);

    /** Writes a snapshot's non-parameter state - the matrix, the rhythm engine
        and the bypasses. Done once, at the crossfade midpoint. */
    void applyNonParameterState (const Snapshot& snapshot);

    void growTo (int index);

    juce::AudioProcessor& processor;

    std::vector<Snapshot> snapshots;

    double crossfadeMs = 30.0;

    // --- recall -------------------------------------------------------------------
    bool recallActive = false;
    std::atomic<double> pendingSeconds { 0.0 };
    bool recallMidpointDone = false;
    double recallPosition = 0.0;
    double recallSeconds = 0.0;
    int currentIndex = 0;
    int recallTarget = 0;

    /** Where every parameter stood when the recall began. Interpolating from a
        stored start rather than from the live value keeps the fade linear even
        though the live value is what is being written. */
    juce::var recallFrom;

    // --- morph --------------------------------------------------------------------
    bool morphEnabled = false;
    int morphA = 0, morphB = 1;
    double morphPosition = 0.0;
    MorphCurve morphCurve = MorphCurve::linear;
    double bezier[4] = { 0.25, 0.1, 0.75, 0.9 };

    juce::StringArray morphExclusions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnapshotBank)
};

} // namespace luthier
