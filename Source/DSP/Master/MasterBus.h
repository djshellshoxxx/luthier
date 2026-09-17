#pragma once

/*  Master bus (engine spec 15).

    Master gain, a safety limiter at -0.3 dBFS, a final DC blocker, and the
    metering the UI reads (peak, RMS and a short-term LUFS estimate).

    The limiter is a safety device, not an effect: it is transparent until the
    signal would actually clip, which is what "safety only" means.
*/

#include "../Common/DspCommon.h"
#include <atomic>
#include <vector>

namespace luthier
{

class MasterBus
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void setGainDb (double db) noexcept;
    double getGainDb() const noexcept { return gainDb; }

    void setLimiterEnabled (bool e) noexcept { limiterEnabled = e; }
    bool isLimiterEnabled() const noexcept { return limiterEnabled; }

    void processBlock (juce::AudioBuffer<float>& buffer) noexcept;

    //==========================================================================
    // Metering. Written on the audio thread, read on the message thread.

    double getPeakLeft()  const noexcept { return peakL.load (std::memory_order_relaxed); }
    double getPeakRight() const noexcept { return peakR.load (std::memory_order_relaxed); }
    double getRmsLeft()   const noexcept { return rmsL.load (std::memory_order_relaxed); }
    double getRmsRight()  const noexcept { return rmsR.load (std::memory_order_relaxed); }
    double getLufs()      const noexcept { return lufs.load (std::memory_order_relaxed); }

    /** Peak level in dBFS across both channels, for the header LED. */
    double getPeakDb() const noexcept
    {
        return gainToDb (juce::jmax (getPeakLeft(), getPeakRight()));
    }

    /** True while the limiter is actually reducing gain. */
    bool isClipping() const noexcept { return clipping.load (std::memory_order_relaxed); }

    /** How much the limiter is pulling down right now, in dB. */
    double getGainReductionDb() const noexcept { return grDb.load (std::memory_order_relaxed); }

    void resetMeters() noexcept;

private:
    static constexpr double kCeilingDb = -0.3;

    double sr = 44100.0;

    ExpSmoother gainSmooth;
    double gainDb = 0.0;

    bool limiterEnabled = true;
    double ceilingLinear = 0.966;
    double limiterEnv = 0.0;
    double limiterAttack = 0.0;
    double limiterRelease = 0.0;

    // Lookahead keeps the limiter from ever letting a transient through.
    std::vector<float> lookL, lookR;
    int lookSize = 0, lookIndex = 0, lookDelay = 0;

    DCBlocker dcL, dcR;

    // LUFS: K-weighting (a shelf plus a highpass) feeding a 400 ms window.
    Biquad kShelfL, kShelfR, kHpL, kHpR;
    double lufsAccum = 0.0;
    int lufsCount = 0, lufsWindow = 17640;

    std::atomic<double> peakL { 0.0 }, peakR { 0.0 };
    std::atomic<double> rmsL { 0.0 }, rmsR { 0.0 };
    std::atomic<double> lufs { -70.0 };
    std::atomic<double> grDb { 0.0 };
    std::atomic<bool> clipping { false };

    JUCE_LEAK_DETECTOR (MasterBus)
};

} // namespace luthier
