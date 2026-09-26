#pragma once

/*  cpu-quality-modes.md 2.1: the one definition of what each CPU quality level
    changes. The engine and the UI read these numbers only from here; a source
    scan (CQ-01) fails the build if a cap is written as a literal anywhere else.

    Nothing here is stored in a preset (ground rule 0.1). The level belongs to
    the machine and the person: PerformanceSettings holds the global choice,
    UiState::qualityOverride the per-instance one, and QualityController turns
    both (plus Auto and offline rendering) into the level the audio thread runs.
*/

#include <cstdint>

namespace luthier
{

/** The three levels the engine can run at. Auto is a way of choosing one. */
enum class QualityLevel : int { High = 0, Medium = 1, Low = 2 };

/** What the person picked in Options -> AUDIO (the global setting). */
enum class QualityChoice : int { High = 0, Medium = 1, Low = 2, Auto = 3 };

/** The per-instance override (cpu-quality-modes 3), saved with uiState. */
enum class QualityOverride : int { Global = 0, High = 1, Medium = 2, Low = 3, Auto = 4 };

/** How much the UI may move (cpu-quality-modes 6). */
enum class MotionLevel : int { Full = 0, Limited = 1, Off = 2 };

//==============================================================================
struct QualityProfile
{
    QualityLevel level = QualityLevel::High;

    /** Oversampling caps. 0 = no cap (the `oversample` parameter decides). */
    int ampOversamplingCap = 0;
    int driveOversamplingCap = 0;

    /** Dispersion allpass stages. `fullStages` is StringEngine's own count;
        at or above `fourStageFromHz` the cap is 4, at or above
        `twoStageFromHz` it is 2. 0 disables that rule. */
    int fullDispersionStages = 8;
    double fourStageFromHz = 0.0;
    double twoStageFromHz = 0.0;

    /** Convolution length caps in seconds. 0 = the full response. */
    double bodyIrSeconds = 0.0;
    double cabinetIrSeconds = 0.0;

    /** Body modal bank: how many modes run, and how many of the lowest are
        always kept whatever their energy. */
    int bodyModes = 48;
    int bodyModesAlwaysKept = 8;

    /** Room early-reflection taps (RoomEngine::kNumTaps is 16). */
    int roomTaps = 16;

    /** NoiseEngine pools halved. */
    bool noiseDegraded = false;

    /** Mod-matrix control interval, as a multiple of its 128-sample base.
        Stays 1 while any LFO runs above `modFastLfoHz`. */
    int modIntervalMultiplier = 1;
    double modFastLfoHz = 20.0;

    /** Idle-string sleep (2.4). */
    bool idleSleep = false;

    /** Ring-out truncation of released strings: dB below the note's peak at
        which the tail fades to sleep (0 = off), and the most strings allowed
        to ring out at once (0 = no limit). */
    double ringOutDb = 0.0;
    int maxRingingOut = 0;

    /** UI (6): the motion level and the live-readout rate cap (0 = none). */
    MotionLevel motion = MotionLevel::Full;
    int liveReadoutMaxHz = 0;

    //==========================================================================
    static constexpr QualityProfile forLevel (QualityLevel l) noexcept
    {
        QualityProfile p;
        p.level = l;

        if (l == QualityLevel::Medium)
        {
            p.ampOversamplingCap = 2;
            p.driveOversamplingCap = 2;
            p.fourStageFromHz = 330.0;
            p.bodyIrSeconds = 1.5;
            p.cabinetIrSeconds = 0.250;
            p.bodyModes = 32;
            p.idleSleep = true;
            p.ringOutDb = -80.0;
            p.motion = MotionLevel::Limited;
        }
        else if (l == QualityLevel::Low)
        {
            p.ampOversamplingCap = 2;
            p.driveOversamplingCap = 1;
            p.fourStageFromHz = 165.0;
            p.twoStageFromHz = 440.0;
            p.bodyIrSeconds = 0.75;
            p.cabinetIrSeconds = 0.120;
            p.bodyModes = 20;
            p.roomTaps = 8;
            p.noiseDegraded = true;
            p.modIntervalMultiplier = 2;
            p.idleSleep = true;
            p.ringOutDb = -60.0;
            p.maxRingingOut = 8;
            p.motion = MotionLevel::Off;
            p.liveReadoutMaxHz = 10;
        }

        return p;
    }

    /** A drive pedal whose soft clipping aliases above -50 dBc at 1x keeps 2x
        at Low (CQ-15). `pedalType` is a PedalType index. */
    static constexpr int driveCapForPedal (int pedalType, int cap) noexcept
    {
        (void) pedalType;
        return cap;
    }

    /** min(nominal, cap): the factor a stage really runs at. 1x stays 1x. */
    static constexpr int capFactor (int nominal, int cap) noexcept
    {
        return (cap <= 0 || nominal <= cap) ? nominal : cap;
    }

    /** The dispersion stage count for a note at `f0` Hz under this profile. */
    constexpr int dispersionStagesFor (double f0) const noexcept
    {
        return dispersionStagesFor (f0, fourStageFromHz, twoStageFromHz, fullDispersionStages);
    }

    static constexpr int dispersionStagesFor (double f0, double fourFromHz, double twoFromHz, int full) noexcept
    {
        const int capped = (twoFromHz > 0.0 && f0 >= twoFromHz)   ? 2
                         : (fourFromHz > 0.0 && f0 >= fourFromHz) ? 4
                                                                  : full;
        return capped < full ? capped : full;
    }

    /** Silence rules for the engine's hard switch (2.5). */
    static constexpr double kHardSwitchSilenceSeconds = 0.050;
    static constexpr double kHardSwitchSilenceDb = -90.0;

    /** Crossfade lengths (2.2, 2.3, 2.5). */
    static constexpr double kOversamplerFadeSeconds = 0.010;

    /** The new path runs this long unheard before its fade-in starts, so its
        latency pad and half-band filters have filled (a pad starting empty
        would otherwise step from zero inside the crossfade). */
    static constexpr int kSwitchSettleSamples = 32;
    static constexpr double kConvolutionFadeSeconds = 0.020;
    static constexpr double kDroppedVoiceRampSeconds = 0.020;

    /** Truncation tail rule (2.3): energy after the cut <= -40 dB of the total,
        then a 50 ms raised-cosine fade. */
    static constexpr double kTailEnergyDb = -40.0;
    static constexpr double kTailFadeSeconds = 0.050;

    /** CQ-31: the variants' memory, at most 12 MB per instance - the body's
        response gets 8 MB, each cabinet mic 2 MB. */
    static constexpr double kBodyVariantBudgetMegabytes = 8.0;
    static constexpr double kCabinetMicVariantBudgetMegabytes = 2.0;

    /** Idle-string sleep thresholds (2.4). */
    static constexpr double kSleepLevel = 1.0e-5;
    static constexpr double kSleepCouplingLevel = 1.0e-7;
    static constexpr double kSleepAfterSeconds = 0.100;
    static constexpr double kRingOutFadeSeconds = 0.030;

    /** E3 (7): the least-recently-excited ringing string fades over 10 ms. */
    static constexpr double kEmergencyFadeSeconds = 0.010;
};

inline const char* qualityLevelKey (QualityLevel l) noexcept
{
    switch (l)
    {
        case QualityLevel::Medium: return "medium";
        case QualityLevel::Low:    return "low";
        case QualityLevel::High:
        default:                   return "high";
    }
}

inline const char* qualityChoiceKey (QualityChoice c) noexcept
{
    switch (c)
    {
        case QualityChoice::Medium: return "medium";
        case QualityChoice::Low:    return "low";
        case QualityChoice::Auto:   return "auto";
        case QualityChoice::High:
        default:                    return "high";
    }
}

inline const char* qualityOverrideKey (QualityOverride o) noexcept
{
    switch (o)
    {
        case QualityOverride::High:   return "high";
        case QualityOverride::Medium: return "medium";
        case QualityOverride::Low:    return "low";
        case QualityOverride::Auto:   return "auto";
        case QualityOverride::Global:
        default:                      return "global";
    }
}

} // namespace luthier
