#pragma once

/*  The audio-thread half of output normalization (output-normalization.md
    2.1, 2.2, 4.1).

    A calibrated static gain, never an auto-gain: the gain only moves when the
    configuration's calibration says so, the switch flips, or the target
    changes, and every such move is a 300 ms linear-in-dB glide.

    The glide is a pure function of the absolute timeline position. It is
    evaluated at 32-sample segment boundaries aligned to the timeline, with the
    linear gain interpolated inside each segment, so the gain curve does not
    depend on the host's block size (ON-16).

    Cross-thread traffic is lock-free and allocation-free:
      - enabled, target and a pending "load snap" are atomics;
      - a calibration result is one packed std::atomic<uint64_t>:
        serial (32 bits) | gain in centi-dB (24 bits, signed) | flags (8 bits).

    isActive() is false while disabled with the gain resting at 0 dB, and then
    MasterBus runs its pre-normalization loop untouched (ground rule 0.1).
*/

#include <atomic>
#include <cstdint>
#include <cmath>

namespace luthier
{

class LoudnessNormalizer
{
public:
    static constexpr int kSegment = 32;
    static constexpr double kGlideSeconds = 0.300;

    static constexpr int kMinGainCentiDb = -1200;   ///< 2.1: -12 dB
    static constexpr int kMaxGainCentiDb = 2400;    ///< 2.1: +24 dB

    /** 4.2's result flags. */
    enum Flags : std::uint8_t
    {
        flagClampedHigh  = 1 << 0,
        flagClampedLow   = 1 << 1,
        flagUnmeasurable = 1 << 2,
        flagEstimate     = 1 << 3,
        flagFailed       = 1 << 4
    };

    /** 2.1: clamp (target - measured, -12 dB, +24 dB), rounded to 0.01 dB. */
    static std::int32_t gainForMeasurement (double targetLufs, double measuredLufs, std::uint8_t& flags) noexcept;

    static std::uint64_t pack (std::uint32_t serial, std::int32_t gainCentiDb, std::uint8_t flags) noexcept;
    static void unpack (std::uint64_t word, std::uint32_t& serial, std::int32_t& gainCentiDb, std::uint8_t& flags) noexcept;

    //==========================================================================
    void prepare (double sampleRate) noexcept;

    /** Drops any glide: the gain snaps to where it is heading. */
    void reset() noexcept;

    //==========================================================================
    // Any thread.

    void setEnabled (bool shouldBeEnabled) noexcept { enabled.store (shouldBeEnabled, std::memory_order_release); }
    bool isEnabled() const noexcept { return enabled.load (std::memory_order_acquire); }

    /** For the status readout; the gain itself comes with each result. */
    void setTargetLufs (double lufs) noexcept { targetLufs.store (lufs, std::memory_order_relaxed); }
    double getTargetLufs() const noexcept { return targetLufs.load (std::memory_order_relaxed); }

    /** 4.2 step 4: one atomic word. A result whose serial is not newer than the
        last one applied is ignored. Unmeasurable and failed results carry no
        gain and leave the current gain where it is. */
    void publishResult (std::uint32_t serial, std::int32_t gainCentiDb, std::uint8_t flags) noexcept;

    /** 2.2's cached-load path: the next block starts at this gain with no
        glide, so a new preset never plays at the old preset's gain. */
    void applyLoadGain (std::int32_t gainCentiDb) noexcept;

    /** 6: restore snaps to the stored gain from the first block. */
    void snapTo (std::int32_t gainCentiDb) noexcept { applyLoadGain (gainCentiDb); }

    /** Audio thread: a result applied in the next beginBlock snaps rather
        than glides. The offline path of a preset load (4.6): cached or not,
        the load's gain lands with the load, so a render does not depend on
        the cache. */
    void snapNextResult() noexcept { snapNext = true; }

    /** The last packed word published (for tests and the offline wait). */
    std::uint64_t getPublishedWord() const noexcept { return published.load (std::memory_order_acquire); }
    std::uint32_t getPublishedSerial() const noexcept;

    //==========================================================================
    // Audio thread.

    /** Reads the cross-thread state at a block boundary. `timelineStart` is the
        block's first sample on the timeline. A result whose glide should start
        at a known timeline sample inside the block (the offline wait, 4.6)
        passes it as `resultStartSample`; -1 starts at the block. */
    void beginBlock (std::int64_t timelineStart, std::int64_t resultStartSample = -1) noexcept;

    /** True while enabled, or while a glide back towards 0 dB is unfinished.
        Valid after beginBlock. */
    bool isActive() const noexcept { return active; }

    /** The gain for the next sample, linear. Advances the timeline by one. */
    double next() noexcept;

    /** The dB value the gain is heading to, and where it is now. */
    double getTargetGainDb() const noexcept { return endDb; }
    double getCurrentGainDb() const noexcept { return currentDb.load (std::memory_order_relaxed); }

    /** True while a glide is running (for the readout's "measuring" hold). */
    bool isGliding() const noexcept { return gliding; }

    /** The gain the calibration asked for (ignoring on/off), in dB. */
    double getCalibratedGainDb() const noexcept { return calibratedDb; }

    std::int64_t getTimeline() const noexcept { return timeline; }

private:
    double dbAt (std::int64_t t) const noexcept;
    void startGlide (double toDb, std::int64_t at) noexcept;
    void loadSegment (std::int64_t segmentStart) noexcept;

    double sr = 48000.0;
    std::int64_t glideLength = 14400;

    std::atomic<bool> enabled { false };
    std::atomic<double> targetLufs { -18.0 };
    std::atomic<std::uint64_t> published { 0 };
    std::atomic<std::int32_t> pendingLoad { INT32_MIN };

    // Audio-thread state.
    std::uint32_t appliedSerial = 0;
    bool wasEnabled = false;
    bool snapNext = false;
    bool active = false;
    bool gliding = false;
    double calibratedDb = 0.0;       ///< last calibration's gain (applies while enabled)

    double startDb = 0.0, endDb = 0.0;
    std::int64_t glideStart = 0;
    std::int64_t timeline = 0;

    std::int64_t segStart = INT64_MIN;
    double segGainA = 1.0, segGainB = 1.0;

    std::atomic<double> currentDb { 0.0 };
};

} // namespace luthier
