#pragma once

/*  The E-Bow (ambiguity-resolutions.md 2.2): section 1's per-string feedback
    injection with a fixed narrowband profile.

    Each selected string is driven through a narrow band-pass (Q 30) at the
    chosen partial of its own note, at its excitation point, with a gain that
    falls to nothing as the string reaches the intensity's target level - so a
    held note swells to a steady sustain and stays there (2.4: within 500 ms at
    50%). The drive's source is the string itself, as a real E-Bow's sensing
    coil is, rather than the amp's output: an E-Bow sustains the same through a
    clean amp, a loud one or none (DECISIONS).

    When the E-Bow lets go of a string - switched off, or the note released
    with "held strings" selected - that string is damped, so it is gone within
    200 ms (2.4) instead of ringing on at the level the drive held it at.
*/

#include "../Common/DspCommon.h"

#include <array>

namespace luthier
{

struct EBowSettings
{
    bool enabled = false;
    int stringMask = 0;        ///< bit s = string s; 0 = any string with a held note
    double intensity = 0.5;    ///< 0..1
    int harmonic = 1;          ///< 1 = fundamental .. 5 = fifth partial
};

class EBowDriver
{
public:
    void prepare (double sampleRate) noexcept { sr = sampleRate > 0.0 ? sampleRate : 48000.0; reset(); }
    void reset() noexcept;

    void setSettings (const EBowSettings& s) noexcept;
    const EBowSettings& getSettings() const noexcept { return settings; }

    /** Per block, before the strings. `held` says which strings have a note
        down. Returns, through `letGo`, the strings the E-Bow has just stopped
        driving, for the engine to damp. */
    void beginBlock (const double* stringHz, const double* stringLevels, const bool* held,
                     int numStrings, std::array<bool, kMaxStrings>& letGo) noexcept;

    bool isDriving (int s) const noexcept { return driving[(size_t) juce::jlimit (0, kMaxStrings - 1, s)]; }
    bool isDrivingAny() const noexcept;

    /** The drive for string s from its own output. Audio thread. */
    double process (int s, double stringOutput) noexcept
    {
        if (! driving[(size_t) s])
            return 0.0;

        return sanitise (peaks[(size_t) s].process (stringOutput) * gain[(size_t) s]);
    }

    /** The level a string settles at, for an intensity. */
    static double targetLevelFor (double intensity) noexcept { return 0.02 + 0.10 * juce::jlimit (0.0, 1.0, intensity); }

private:
    double sr = 48000.0;
    EBowSettings settings;

    std::array<Biquad, kMaxStrings> peaks {};
    std::array<double, kMaxStrings> designedHz {};
    std::array<double, kMaxStrings> gain {};
    std::array<bool, kMaxStrings> driving {};
};

} // namespace luthier
