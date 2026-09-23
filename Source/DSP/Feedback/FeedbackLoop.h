#pragma once

/*  Acoustic feedback as a physical loop (ambiguity-resolutions.md 1).

        excitation_feedback[s] = k_couple(s) * H_s(f) * amp_out(t - delay)

    The amp's output reaches the strings through the air: attenuated with
    distance and by the speaker's directivity (k_couple), delayed by the block
    that breaks the loop plus the time sound takes to cross the gap, and heard
    by each ringing string only near its own note (H_s, a peak at the note or
    at an octave of it). It goes in at the excitation point, so everything
    between the string and the amp - pickups, the guitar's circuit, the volume
    knob - is inside the loop: rolling the volume back kills feedback because
    it lowers the loop gain, exactly as it does on a real guitar.

    At amount 0 the whole thing is skipped (1.2: zero CPU, bit-identical).
    Nothing here allocates after prepare(). Audio thread, except prepare().
*/

#include "../Common/DspCommon.h"

#include <array>
#include <vector>

namespace luthier
{

struct FeedbackSettings
{
    double amount = 0.0;           ///< 0..1 (feedback_amount / 100)
    double distanceMetres = 0.5;   ///< guitar to speaker, 0..3
    double angleDegrees = 0.0;     ///< 0 = facing the speaker
    double focus = 0.6;            ///< 0..1: how narrowly each string hears its note
    int octaveBias = 0;            ///< -2..+2: the loop locks to f0 * 2^bias

    bool operator== (const FeedbackSettings& o) const noexcept
    {
        return amount == o.amount && distanceMetres == o.distanceMetres && angleDegrees == o.angleDegrees
            && focus == o.focus && octaveBias == o.octaveBias;
    }
};

class FeedbackLoop
{
public:
    static constexpr double kSpeedOfSound = 343.0;
    static constexpr double kMaxDistance = 3.0;

    /** The loudest the injection gets; a soft limit, so a runaway loop saturates
        like a real one instead of growing without bound. */
    static constexpr double kInjectionCeiling = 0.02;

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    void setSettings (const FeedbackSettings& s) noexcept;
    const FeedbackSettings& getSettings() const noexcept { return settings; }

    bool isActive() const noexcept { return settings.amount > 0.0; }

    /** Per block, before the strings: which strings are ringing, at what pitch,
        and which are wound (a heavy wound string is moved less by the air). */
    void beginBlock (const double* stringHz, const double* stringLevels, const bool* wound,
                     int numStrings) noexcept;

    /** The amp's output from `delay` samples ago, for sample `i` of this block. */
    double delayedAmp (int i) const noexcept
    {
        const int size = (int) ring.size();

        if (size == 0)
            return 0.0;

        int index = blockStartWrite - delaySamples + i;

        while (index < 0)
            index += size;

        return ring[(size_t) (index % size)];
    }

    /** String s's share of `ampSample`, at its excitation point. */
    double process (int s, double ampSample) noexcept;

    /** After the amp: this block's output, for the blocks to come. */
    void pushAmpOutput (const double* amp, int numSamples) noexcept;

    /** 1.3's indicator: how much of the ceiling the injection is using, 0..1. */
    double getActivity() const noexcept { return juce::jlimit (0.0, 1.0, injectionEnv / kInjectionCeiling); }

    /** The loop has taken over: the injection is sustaining a string by itself.
        A loud high-gain rig at full amount settles near 19% of the ceiling (the
        amp saturates first); a clean amp at 20% stays under 2%. 8% is between. */
    bool isResonant() const noexcept { return injectionEnv > 0.08 * kInjectionCeiling; }

    /** k_couple for a string, for the tests. */
    double getCoupling (int s) const noexcept { return couple[(size_t) juce::jlimit (0, kMaxStrings - 1, s)]; }

    int getDelaySamples() const noexcept { return delaySamples; }

private:
    void updateCoupling() noexcept;

    double sr = 48000.0;
    int maxBlock = 512;

    FeedbackSettings settings;

    std::vector<double> ring;
    int writePos = 0;
    int blockStartWrite = 0;
    int delaySamples = 512;

    std::array<Biquad, kMaxStrings> peaks {};
    std::array<double, kMaxStrings> designedHz {};
    std::array<bool, kMaxStrings> ringing {};
    std::array<bool, kMaxStrings> woundString {};
    std::array<double, kMaxStrings> couple {};

    double injectionEnv = 0.0, injectionRelease = 0.0;
};

} // namespace luthier
