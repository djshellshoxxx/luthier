// OWNER: WP1 (frozen interface I-1; changes only via errata request to the WP1 owner)
#pragma once

// Static description of a reference pedal and its parameters. Pure data: no
// DSP, no GUI. Spec: spec/reference-pedalboard/specs/BOARD_CORE.md sec 11.
// Compiled into the headless renderer, so nothing here may pull in GUI code.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace luthier::reference
{

constexpr int kMaxParams     = 32;   // descriptor capacity per pedal (state, MIDI-learn reach)
constexpr int kMaxLaneParams = 16;   // host-automation lanes ref_s{nn}_p01..p16 (BOARD_CORE 9.1)

enum class ChannelMode : uint8_t { Mono, Stereo, TrueStereo, MonoInStereoOut };
enum class BypassMode  : uint8_t { True, Buffered, DSP };
enum class ParamKind   : uint8_t { Continuous, Discrete, Toggle, Momentary, Trigger, Tempo, Enum };
enum class Taper       : uint8_t { Linear, LogA, LogB, ReverseLog, Skew, Detent, Custom };

enum ParamFlag : uint16_t
{
    PF_None = 0,
    PF_Engage = 1,          // footswitch: aliases the slot "enabled", never stored in the pedal
    PF_Config = 2,          // structural (variant, quality, stereo mode): state only, no lane
    PF_Ext = 4,             // virtual extension (no hardware control): state + MIDI only
    PF_Hidden = 8,
    PF_Automatable = 16,
    PF_ExprTarget = 32,
    PF_TempoSync = 64,
    PF_OwnSmoothing = 128   // pedal glides the value itself (e.g. delay time)
};

struct ParamDesc
{
    const char* id;           // snake_case, MUST start with PedalDesc.paramPrefix
    const char* label;        // front-panel text (original wording; physical label names)
    const char* unit;
    ParamKind kind;
    Taper taper;
    float min, max, def, skew;
    int steps;                // discrete count (Enum/Discrete), else 0
    const char* const* choices;   // Enum labels, `steps` entries
    uint16_t flags;           // ParamFlag bitset
    float smoothMs;           // default 20
    int16_t midiCcDefault;    // pedal-LOCAL profile CC (-1 none), MIDI_EXPRESSION 9

    bool has (ParamFlag f) const noexcept { return (flags & (uint16_t) f) != 0; }
    bool isDiscreteKind() const noexcept
    {
        return kind == ParamKind::Discrete || kind == ParamKind::Enum;
    }
    // Kinds that never take a host-automation lane (BOARD_CORE 9.1).
    bool isLaneExcluded() const noexcept
    {
        return has (PF_Engage) || has (PF_Config) || has (PF_Ext)
            || kind == ParamKind::Trigger || kind == ParamKind::Momentary;
    }

    // Normalised 0..1 -> natural units. Same semantics as the legacy
    // PedalParam (Source/DSP/Effects/Pedal.h) so tapers match:
    //   Linear                    min + (max-min) * n
    //   Skew / Detent / Custom    n^(1/skew) shaping (skew==1 is linear)
    //   LogA   (min > 0)          min * (max/min)^n   (frequency-style, geometric)
    //   LogB                      audio-pot taper (10% of travel at mid): (10^(2n)-1)/99
    //   ReverseLog                mirrored LogB: 1 - LogB(1-n)
    //   Discrete                  min..max snapped to equally spaced steps
    //   Enum                      round(n * (steps-1)), 0-based index
    //   Toggle / Momentary / Trigger   n >= 0.5 ? 1 : 0
    // Detent and Custom are linear here; the pedal may snap or remap itself.
    double fromNormalised (double n) const noexcept
    {
        n = std::isfinite (n) ? std::clamp (n, 0.0, 1.0) : 0.0;

        if (kind == ParamKind::Toggle || kind == ParamKind::Momentary || kind == ParamKind::Trigger)
            return n >= 0.5 ? 1.0 : 0.0;

        if (kind == ParamKind::Enum && steps > 1)
            return (double) std::clamp ((int) std::lround (n * (steps - 1)), 0, steps - 1);
        if (kind == ParamKind::Discrete)
        {
            if (steps < 2 || !std::isfinite(min) || !std::isfinite(max) || !(min < max))
                return std::isfinite(min) ? (double)min : 0.0;
            const auto position = std::round(n * (steps - 1)) / (steps - 1);
            return (double)min + ((double)max - min) * position;
        }

        double shaped = n;
        switch (taper)
        {
            case Taper::LogA:
                if (min > 0.0f && max > min)
                    return (double) min * std::pow ((double) max / (double) min, n);
                shaped = skewed (n);
                break;
            case Taper::LogB:        shaped = (std::pow (10.0, 2.0 * n) - 1.0) / 99.0; break;
            case Taper::ReverseLog:  shaped = 1.0 - (std::pow (10.0, 2.0 * (1.0 - n)) - 1.0) / 99.0; break;
            case Taper::Linear:      break;
            case Taper::Skew:
            case Taper::Detent:
            case Taper::Custom:      shaped = skewed (n); break;
        }
        return (double) min + ((double) max - (double) min) * shaped;
    }

    // Natural units -> normalised 0..1; exact inverse of fromNormalised
    // (round trip within 1e-6 for continuous kinds, A-PED).
    double toNormalised (double v) const noexcept
    {
        if (! std::isfinite (v))
            return 0.0;

        if (kind == ParamKind::Toggle || kind == ParamKind::Momentary || kind == ParamKind::Trigger)
            return v >= 0.5 ? 1.0 : 0.0;

        if (kind == ParamKind::Enum && steps > 1)
            return std::clamp (v / (double) (steps - 1), 0.0, 1.0);
        if (kind == ParamKind::Discrete)
        {
            if (steps < 2 || !std::isfinite(min) || !std::isfinite(max) || !(min < max)) return 0.0;
            const auto position = std::clamp((v - min) / ((double)max - min), 0.0, 1.0);
            return std::round(position * (steps - 1)) / (steps - 1);
        }

        const double range = (double) max - (double) min;
        if (std::abs (range) < 1.0e-12)
            return 0.0;

        if (taper == Taper::LogA && min > 0.0f && max > min)
        {
            const double t = std::log (std::clamp (v, (double) min, (double) max) / (double) min)
                           / std::log ((double) max / (double) min);
            return std::clamp (t, 0.0, 1.0);
        }

        const double t = std::clamp ((v - (double) min) / range, 0.0, 1.0);
        switch (taper)
        {
            case Taper::LogB:        return std::clamp (std::log10 (t * 99.0 + 1.0) * 0.5, 0.0, 1.0);
            case Taper::ReverseLog:  return std::clamp (1.0 - std::log10 ((1.0 - t) * 99.0 + 1.0) * 0.5, 0.0, 1.0);
            case Taper::Linear:      return t;
            default:                 return unskewed (t);
        }
    }

    float defaultNormalised() const noexcept { return (float) toNormalised ((double) def); }

private:
    double skewed (double n) const noexcept
    {
        return skew == 1.0f ? n : std::pow (n, 1.0 / std::max (0.01, (double) skew));
    }
    double unskewed (double t) const noexcept
    {
        return skew == 1.0f ? t : std::pow (t, std::max (0.01, (double) skew));
    }
};

struct PedalDesc
{
    const char* modelId;      // "generic.overdrive", "boss-ds1" (hyphens allowed; immutable, serialised)
    const char* paramPrefix;  // modelId with '.'/'-' -> '_' EXCEPT generic pedals (gen_<short>_); trailing '_' included; unique registry-wide
    uint16_t revision;
    const char* displayName;
    const char* family;
    const char* inspiredBy;   // nullptr for generic pedals
    ChannelMode ch;
    BypassMode bypass;
    int numParams;
    const ParamDesc* params;  // static storage
    int maxLatencySamples;
    bool hasTails;
    bool oversampled;
    bool wantsRawMidi;        // true only if IPedal::onMidi() is needed (BOARD_CORE 9.5)
    uint32_t cpuUnits;        // declared model budget, not a measured result by itself
    uint32_t cpuUnitsPerPercent = 1; // explicit scale; e.g.1000 means300 units =0.3%
};

} // namespace luthier::reference
