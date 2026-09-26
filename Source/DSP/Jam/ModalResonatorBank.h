#pragma once

/*  A bank of damped two-pole resonators (jam-mode.md 5).

    Every drum in the Jam kit is one of these, driven by a contact-force pulse
    or a stochastic exciter. A mode is

        y[n] = 2 r cos(w) y[n-1] - r^2 y[n-2] + sin(w) g x[n]

    whose impulse response is g r^n sin((n + 1) w): unit-free amplitude g at
    frequency w, decaying by r per sample (r = 10^(-3 / (T60 fs))).

    Double precision throughout (engine.md 0.1). The modes are stored as
    structure-of-arrays padded to a multiple of four and summed in four lanes,
    which is the shape the compiler turns into 4-wide SIMD (jam-mode 5) without
    intrinsics. A bank that has decayed below -100 dBFS stops running until it
    is struck again (5, "a bank below -100 dBFS is skipped"). A non-finite state
    is clamped out: the bank resets and counts it (13, engine.md 0.3).

    Real-time safe: fixed arrays, no allocation after construction.
*/

#include "../Common/DspCommon.h"
#include <array>
#include <atomic>

namespace luthier
{

class ModalResonatorBank
{
public:
    static constexpr int kMaxModes = 48;

    /** Below this peak level (-100 dBFS) the bank stops running. */
    static constexpr double kSilence = 1.0e-5;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** How many modes are live. Later modes keep their settings. */
    void setNumModes (int n) noexcept;
    int getNumModes() const noexcept { return numModes; }

    /** Sets one mode. A mode at or above 0.45 fs is silenced rather than
        aliased. */
    void setMode (int index, double hz, double t60Seconds, double gain) noexcept;

    /** Moves a mode's frequency, keeping its decay (tension modulation). */
    void setModeFrequency (int index, double hz) noexcept;

    /** Changes a mode's decay, keeping its frequency (chokes, damping). */
    void setModeDecay (int index, double t60Seconds) noexcept;

    void setModeGain (int index, double gain) noexcept;

    double getModeFrequency (int index) const noexcept { return freq[(size_t) index]; }
    double getModeDecay (int index) const noexcept     { return t60[(size_t) index]; }

    /** Adds `x` as the input and returns the summed output for one sample. */
    inline double process (double x) noexcept
    {
        if (! running)
        {
            if (x == 0.0)
                return 0.0;

            running = true;
        }

        const int n = paddedModes;
        double s0 = 0.0, s1 = 0.0, s2 = 0.0, s3 = 0.0;

        for (int m = 0; m < n; m += 4)
        {
            const double y0 = a1[(size_t) m    ] * y1[(size_t) m    ] - a2[(size_t) m    ] * y2[(size_t) m    ] + b[(size_t) m    ] * x;
            const double y1v = a1[(size_t) m + 1] * y1[(size_t) m + 1] - a2[(size_t) m + 1] * y2[(size_t) m + 1] + b[(size_t) m + 1] * x;
            const double y2v = a1[(size_t) m + 2] * y1[(size_t) m + 2] - a2[(size_t) m + 2] * y2[(size_t) m + 2] + b[(size_t) m + 2] * x;
            const double y3 = a1[(size_t) m + 3] * y1[(size_t) m + 3] - a2[(size_t) m + 3] * y2[(size_t) m + 3] + b[(size_t) m + 3] * x;

            y2[(size_t) m    ] = y1[(size_t) m    ]; y1[(size_t) m    ] = y0;
            y2[(size_t) m + 1] = y1[(size_t) m + 1]; y1[(size_t) m + 1] = y1v;
            y2[(size_t) m + 2] = y1[(size_t) m + 2]; y1[(size_t) m + 2] = y2v;
            y2[(size_t) m + 3] = y1[(size_t) m + 3]; y1[(size_t) m + 3] = y3;

            s0 += y0; s1 += y1v; s2 += y2v; s3 += y3;
        }

        return (s0 + s1) + (s2 + s3);
    }

    /** Runs `n` samples: `in` is the excitation (nullptr for none) and the
        summed output is added to `out`. Four modes at a time in lock-step,
        the shape the compiler vectorises. Real-time safe. */
    void processBlock (const double* in, double* out, int n) noexcept;

    /** Call once per block (or per few hundred samples): flushes denormals,
        guards against NaN / Inf and stops a silent bank. Returns false if the
        state had to be reset because it was not finite. */
    bool housekeep() noexcept;

    bool isRunning() const noexcept { return running; }

    /** Peak of the modes' state, a cheap level estimate. */
    double getStateLevel() const noexcept;

    /** Multiplies the ringing state (a steal fade, a choke). */
    void scaleState (double gain) noexcept;

    /** Copies another bank's ringing state and coefficients (voice stealing). */
    void copyFrom (const ModalResonatorBank& other) noexcept;

    /** Non-finite states caught, across every bank (13, the diagnostics counter). */
    static std::atomic<int>& getNanResetCounter() noexcept;

private:
    void updateCoefficients (int index) noexcept;

    double sr = 48000.0;
    int numModes = 0, paddedModes = 0;
    bool running = false;

    alignas (32) std::array<double, kMaxModes> a1 {}, a2 {}, b {}, y1 {}, y2 {};
    std::array<double, kMaxModes> freq {}, t60 {}, gain {};
};

} // namespace luthier
