#pragma once

/*  The change tracker of output-normalization.md 4.1 and 2.3.

    Decides, on the audio thread, when the sound configuration has changed
    enough to need a new loudness calibration. It is plain integers and
    atomics: no FIFO, no signal, no allocation after construction. The
    calibrator's worker polls requestSerial.

    - A Config parameter whose value changed this block, written by anyone but
      a performance writer (snapshot recall and morph, preset morph: see
      PerformanceWriteScope), sets dirtyAt to the block's timeline sample.
    - When dirty and 250 ms of timeline have passed since the last change, the
      request fires: requestSerial increments and the request's timeline
      sample is recorded. The fire sample is exact (dirtyAt + 250 ms), not the
      block it was noticed in, so the offline gain curve does not depend on
      the block size (ON-16).
    - markConfigDirty (true) fires at the next block with no debounce: the
      discrete events of 2.3.1 (preset and guitar load, part swap, IR, pedal
      type, mod route, switching normalization on, oversampling, undo/redo).
      A few Config parameters are discrete events themselves (the guitar type,
      the pedal-slot types and oversampling) and fire immediately too.

    Spec note: 4.1 puts this inside ParameterBridge. It is its own class,
    owned by the processor and fed from processBlock, so the bridge (a file
    every workstream edits) only gains nothing. It watches every parameter
    through its own listener, which is what the bridge's lastWrite would have
    given it.
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include "../DSP/Master/LoudnessRoles.h"

#include <atomic>
#include <vector>

namespace luthier
{

//==============================================================================
/*  Writes made inside one of these are tagged "performance" (3.2): snapshot
    recall and morph (SnapshotBank::applyBlend) and preset morph
    (PresetMorph::apply). The change tracker ignores them, so a louder lead
    snapshot stays louder. Thread-local, so it tags exactly the writes made on
    the thread that holds it. */
class PerformanceWriteScope
{
public:
    PerformanceWriteScope() noexcept  { ++depth(); }
    ~PerformanceWriteScope() noexcept { --depth(); }

    static bool isActive() noexcept { return depth() > 0; }

private:
    static int& depth() noexcept
    {
        static thread_local int d = 0;
        return d;
    }

    JUCE_DECLARE_NON_COPYABLE (PerformanceWriteScope)
};

//==============================================================================
class ConfigChangeTracker : private juce::AudioProcessorParameter::Listener
{
public:
    /** 2.3.2: timeline after the last Config change before a request. */
    static constexpr double kDebounceSeconds = 0.250;

    explicit ConfigChangeTracker (juce::AudioProcessor& processor);
    ~ConfigChangeTracker() override;

    /** Every parameter's role, by parameter index (built from LoudnessRoles). */
    LoudnessRole getRole (int parameterIndex) const noexcept;

    /** Whether the tracker runs at all. Off, process() only keeps its view of
        the values current, so turning it on does not see a stale change. */
    void setEnabled (bool shouldTrack) noexcept { tracking.store (shouldTrack, std::memory_order_release); }

    /** 2.3.1: any thread. `immediate` fires at the next block. */
    void markConfigDirty (bool immediate) noexcept;

    /** Audio thread, once per block before the audio. Returns true when a
        request fired in this block; `fireSample` is its exact timeline sample. */
    bool process (std::int64_t timelineStart, int numSamples, double sampleRate, std::int64_t& fireSample) noexcept;

    /** The worker's poll word: bumped once per request. */
    std::uint32_t getRequestSerial() const noexcept { return requestSerial.load (std::memory_order_acquire); }
    std::int64_t getRequestTimeline() const noexcept { return requestTimeline.load (std::memory_order_acquire); }

    /** Diagnostics and tests: requests fired since construction. */
    int getNumRequests() const noexcept { return (int) requestSerial.load (std::memory_order_relaxed); }
    bool isDirty() const noexcept { return dirty; }

private:
    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int, bool) override {}

    juce::AudioProcessor& processor;

    std::vector<juce::AudioProcessorParameter*> params;
    std::vector<std::uint8_t> role;
    std::vector<std::uint8_t> immediateEvent;
    std::vector<float> lastSeen;
    std::unique_ptr<std::atomic<std::uint8_t>[]> performanceWrite;   ///< last write came from a PerformanceWriteScope

    std::atomic<bool> tracking { false };
    std::atomic<bool> immediatePending { false };

    bool dirty = false;
    bool needResync = true;
    std::int64_t dirtyAt = 0;

    std::atomic<std::uint32_t> requestSerial { 0 };
    std::atomic<std::int64_t> requestTimeline { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfigChangeTracker)
};

} // namespace luthier
