#pragma once

/*
    SPEC-SWEEP (controllers CT-2/CT-4/CT-7/CT-10/CT-17, input-routing IR-5).

    The active controller profile, as the audio thread sees it.

    A ControllerProfile holds strings and a std::vector (its pitch curve), so it
    cannot cross to the audio thread as it is. ControllerRtSettings is the same
    information as plain numbers; the message thread builds one when a profile is
    chosen and publishes it through a TripleBuffer, and the audio thread applies
    it to the MidiInterpreter at the top of the next block. The UI used to call
    ControllerProfileLibrary::apply straight into the interpreter the audio
    thread was reading.

    The stage also carries the profile's latency budget and shifts incoming
    note events earlier by it (CT-4): a controller that reports a note 12 ms late
    has it moved 12 ms earlier, clamped to the start of the block, with the
    remainder of a latency longer than the block carried as an earlier start of
    the next note in the same block (it cannot sound before it arrived).
*/

#include "ControllerProfile.h"
#include "../Support/TripleBuffer.h"

#include <atomic>

namespace luthier
{

struct ControllerRtSettings
{
    ControllerMode mode = ControllerMode::standard;
    std::array<int, kMaxStrings> channelForString {};
    std::array<double, kMaxStrings> stringBendSemis {};
    double bendSemis = 2.0;
    std::array<MidiTarget, 128> ccMap {};
    double pitchDeadZoneCents = 0.0;
    double minimumNoteDurationMs = 0.0;
    int mpeMasterChannel = 1;
    std::array<float, MidiInterpreter::kPitchCurvePoints> pitchCurve {};
    int numPitchCurvePoints = 0;
    double latencyMs = 0.0;

    static ControllerRtSettings fromProfile (const ControllerProfile& profile) noexcept;

    /** Everything a profile says becomes interpreter state here. Real-time safe. */
    void applyTo (MidiInterpreter& interpreter) const noexcept;

    /** What the profile implies for the host-visible mpe_enabled / bend_range
        parameters, which the parameter bridge writes every block. */
    bool impliesMpe() const noexcept { return mode == ControllerMode::mpe; }
};

class ControllerStage
{
public:
    ControllerStage();

    //==========================================================================
    // Message thread

    /** Publishes @p profile for the audio thread and remembers its id. */
    void setProfile (const ControllerProfile& profile);

    /** The id of the profile last set, or empty (none chosen: Generic MIDI). */
    juce::String getProfileId() const;

    //==========================================================================
    // Audio thread

    /** Applies a newly published profile to @p interpreter, once. */
    void applyPending (MidiInterpreter& interpreter) noexcept;

    /** CT-4: moves note-ons and note-offs earlier by the profile's latency.
        Real-time safe; @p scratch must have been sized in prepare. */
    void compensateLatency (juce::MidiBuffer& midi, int numSamples, double sampleRate,
                            juce::MidiBuffer& scratch) const noexcept;

    /** The active profile's latency budget in ms (any thread). */
    double getLatencyMs() const noexcept { return latencyMs.load (std::memory_order_relaxed); }

    void setLatencyCompensationEnabled (bool on) noexcept { compensate.store (on); }
    bool isLatencyCompensationEnabled() const noexcept { return compensate.load(); }

private:
    mutable juce::CriticalSection writeLock;
    juce::String profileId;

    TripleBuffer<ControllerRtSettings> settings;
    std::atomic<juce::uint32> publishedGeneration { 0 };
    juce::uint32 appliedGeneration = 0;

    std::atomic<double> latencyMs { 0.0 };
    std::atomic<bool> compensate { true };
};

} // namespace luthier
