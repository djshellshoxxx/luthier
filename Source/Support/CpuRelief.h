#pragma once

/*  performance-budget.md 8: the CPU relief ladder.

    Fed each block's load (the block's processing time over its real-time
    budget, 0..1+), it keeps a rolling 200 ms average and climbs one step while
    that average stays above 85%, one step per 200 ms, and comes back down one
    step per second once it has fallen below 70% (the gap is hysteresis, so a
    load hovering at the threshold does not flap). The steps are section 8's:

        1 drop the display FIFO drain rate      5 fewer reverb taps
        2 suspend the scrolling data stream     6 freeze the shadow audition
        3 halve the mod-matrix control rate     7 drop the least-recently-active
        4 halve the NoiseEngine pools             string ("CPU limit" banner)

    Step 7 is audible and opt-out (Options -> Diagnostics, default on): with it
    off the ladder stops at 6.

    The ladder only decides; the engine acts on the steps that have a hook
    (step 4: NoiseEngine::setDegraded). Audio thread for update(), lock-free
    reads from anywhere; no allocation.
*/

#include <atomic>

namespace luthier
{

class CpuRelief
{
public:
    static constexpr int kMaxStep = 7;
    static constexpr double kHighLoad = 0.85;
    static constexpr double kLowLoad = 0.70;
    static constexpr double kWindowSeconds = 0.200;
    static constexpr double kStepUpSeconds = 0.200;
    static constexpr double kStepDownSeconds = 1.0;

    enum Step
    {
        none = 0,
        slowDisplay = 1,
        suspendScrolling = 2,
        halveModRate = 3,
        halveNoisePools = 4,
        fewerReverbTaps = 5,
        freezeAudition = 6,
        dropStrings = 7
    };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** One block: `load` is its processing time / its real-time duration.
        Returns the step now in force. */
    int update (double load, int numSamples) noexcept;

    /** The engine's call, with its measured load: offline or switched off
        (setGloballyEnabled) that measurement counts as idle, so the ladder
        walks back down and stays there. A test's load override still wins. */
    int updateMeasured (double load, int numSamples) noexcept
    {
        if (loadOverride.load (std::memory_order_relaxed) < 0.0
            && (isOffline.load (std::memory_order_relaxed) || ! globallyEnabled().load (std::memory_order_relaxed)))
            load = 0.0;

        return update (load, numSamples);
    }

    int getStep() const noexcept { return step.load (std::memory_order_relaxed); }
    double getAverageLoad() const noexcept { return average.load (std::memory_order_relaxed); }

    /** Step 7 opt-out (default on). */
    void setStringDropAllowed (bool allowed) noexcept { stringDropAllowed.store (allowed, std::memory_order_relaxed); }
    bool isStringDropAllowed() const noexcept { return stringDropAllowed.load (std::memory_order_relaxed); }

    /** Offline (a host bounce, the offline renderer): no relief - there is no
        deadline to miss, and an audible step would be printed into the file. */
    void setOffline (bool offline) noexcept { isOffline.store (offline, std::memory_order_relaxed); }

    /** The test runner turns the ladder off for every instance, so a busy
        machine cannot drop strings in the middle of an unrelated test; a load
        override (below) still drives it. */
    static void setGloballyEnabled (bool enabled) noexcept { globallyEnabled().store (enabled, std::memory_order_relaxed); }

    /** Tests: every later update() sees this load instead of the measured one; negative clears it. */
    void setLoadOverrideForTest (double load) noexcept { loadOverride.store (load, std::memory_order_relaxed); }

    /** The "CPU limit" banner is due while strings are being dropped. */
    bool isCpuLimitBannerDue() const noexcept { return getStep() >= dropStrings; }

private:
    double sr = 48000.0;
    double windowCoeff = 0.0;              // per-sample smoothing, derived from the window
    double aboveSeconds = 0.0, belowSeconds = 0.0;
    std::atomic<double> average { 0.0 };
    std::atomic<int> step { 0 };
    std::atomic<bool> stringDropAllowed { true };
    std::atomic<double> loadOverride { -1.0 };
    std::atomic<bool> isOffline { false };

    static std::atomic<bool>& globallyEnabled() noexcept { static std::atomic<bool> enabled { true }; return enabled; }
};

} // namespace luthier
