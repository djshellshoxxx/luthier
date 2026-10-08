#pragma once

/*  fingerstyle-attack.md (REALISM-B): what touches the string and how it lets
    go, per string - and string-interaction.md's settings, which share the
    right hand's plumbing into the engine.

    Plain data and pure functions: the engine resolves a note's tool here at
    note-on, the bridge fills the settings from the parameters, and the tests
    call the same functions the engine does.
*/

#include "PlayingEvents.h"
#include "../../DSP/String/Excitation.h"
#include <array>

namespace luthier
{

//==============================================================================
/** 6: rh_string_tool_N. Saved as a choice index: append only. */
enum class RhTool { global = 0, pick, finger, thumb, thumbpick, slap, pop, numTools };

/** 6: rh_stroke. */
enum class RhStroke { free = 0, rest, automatic, numStrokes };

/** 4: rh_style. */
enum class RhStyle { custom = 0, pick, fingerstyle, classical, travis, hybrid, slapPop, numStyles };

inline bool isThumbClass (RhTool t) noexcept { return t == RhTool::thumb || t == RhTool::thumbpick || t == RhTool::slap; }
inline bool isFingerClass (RhTool t) noexcept { return t == RhTool::finger || t == RhTool::pop; }

inline juce::StringArray rhToolNames()   { return { "Global", "Pick", "Finger", "Thumb", "Thumbpick", "Slap", "Pop" }; }
inline juce::StringArray rhStrokeNames() { return { "Free", "Rest", "Auto" }; }
inline juce::StringArray rhStyleNames()  { return { "Custom", "Pick", "Fingerstyle", "Classical", "Travis", "Hybrid", "Slap & Pop" }; }

//==============================================================================
struct RightHandSettings
{
    double fleshReleaseMs = 0.0723;     ///< finger_flesh_release_ms: 2.20 kHz
    double nailReleaseMs = 0.0227;      ///< finger_nail_release_ms: 7.01 kHz
    double thumbPositionOffset = 0.04;  ///< thumb_position_offset, fraction of the string
    double restStrokeDamping = 0.8;     ///< rest_stroke_damping
    RhStroke stroke = RhStroke::free;
    RhStyle style = RhStyle::custom;
    std::array<RhTool, 6> stringTool { RhTool::global, RhTool::global, RhTool::global,
                                       RhTool::global, RhTool::global, RhTool::global };
    double thumbPalmMute = 0.0;         ///< thumb_palm_mute
    double hybridSnap = 0.3;            ///< hybrid_snap

    /*  bass-techniques.md 11's finger_alternation_variation and rest_stroke,
        reused (fingerstyle-attack 6), not duplicated. Until that spec lands
        they have no parameter and these stay at their neutral values. */
    double alternationVariation = 0.0;
    bool bassRestStroke = false;

    /** 3: strings 7-12 follow their course (12-string) or the lowest assigned
        string (7- and 8-string), as setup_nut_depth does. */
    RhTool toolForString (int stringIndex, bool twelveString, int course) const noexcept
    {
        if (twelveString)
            return stringTool[(size_t) juce::jlimit (0, 5, course)];

        return stringTool[(size_t) juce::jlimit (0, 5, stringIndex)];
    }
};

/** A style's table (4): the per-string tools it writes. Custom writes nothing. */
struct RhStyleTable
{
    bool writes = false;
    RhTool treble = RhTool::global;     ///< strings 1-3
    RhTool bass = RhTool::global;       ///< strings 4-6
};

inline RhStyleTable styleTable (RhStyle style) noexcept
{
    switch (style)
    {
        case RhStyle::pick:        return { true, RhTool::global, RhTool::global };
        case RhStyle::fingerstyle: return { true, RhTool::finger, RhTool::thumb };
        case RhStyle::classical:   return { true, RhTool::finger, RhTool::thumb };
        case RhStyle::travis:      return { true, RhTool::finger, RhTool::thumbpick };
        case RhStyle::hybrid:      return { true, RhTool::finger, RhTool::pick };
        case RhStyle::slapPop:     return { true, RhTool::pop, RhTool::slap };
        case RhStyle::custom:
        case RhStyle::numStyles:
        default:                   return {};
    }
}

//==============================================================================
/** string-interaction.md 7, in natural units. */
struct StringInteractionSettings
{
    double airAmount = 1.0;           ///< coupling_air_amount
    double palmSpreadMm = 35.0;       ///< palm_mute_spread
    double adjacentMute = 0.6;        ///< adjacent_mute_amount
    double releaseStaggerMs = 12.0;   ///< release_stagger_ms
    double releaseStaggerBias = 0.0;  ///< release_stagger_bias: + treble first, - bass first
    double apertureScale = 1.0;       ///< pickup_aperture_scale
    double mutedThumpLevel = 0.5;     ///< muted_thump_level

    /*  3: the fretting-hand mute style, 1.0 rock spread, 0.1 classical
        fingertip. muting-rhythm.md's MuteSettings::frettingStyle owns it; until
        that lands the bridge derives it from the right-hand style (Classical
        arches the fingers). */
    double frettingStyle = 1.0;
};

/** 2: the palm's coverage weight for a string `d` strings from its centre, at
    width W (strings). */
inline double palmWeight (double d, double widthStrings) noexcept
{
    const double half = 0.5 * juce::jmax (0.0, widthStrings);
    d = std::abs (d);

    if (d <= half)
        return 1.0;

    if (d < half + 1.0)
    {
        const double c = std::cos (0.5 * juce::MathConstants<double>::pi * (d - half));
        return c * c;
    }

    return 0.0;
}

/** 4: one string's release delay, u in [0, 1] times the stagger. `rank` is 0..1
    with the treble first (string 0 is the high E). */
inline double staggerDelayMs (double staggerMs, double bias, double rngValue, double trebleFirstRank) noexcept
{
    const double b = juce::jlimit (-1.0, 1.0, bias);
    const double rank = b >= 0.0 ? trebleFirstRank : 1.0 - trebleFirstRank;
    const double u = (1.0 - std::abs (b)) * juce::jlimit (0.0, 1.0, rngValue) + std::abs (b) * rank;
    return juce::jmax (0.0, staggerMs) * u;
}

/** 5: a whole-step bend moves the string 7 mm at the fret. */
inline double lateralMmForBend (double bendCents) noexcept
{
    return 7.0 * std::sqrt (std::abs (bendCents) / 200.0);
}

/** The bridge string spacing s_b for a family (2), in mm. */
inline double bridgeStringSpacingMm (bool acoustic, bool classical, bool bass) noexcept
{
    return bass ? 19.0 : classical ? 11.5 : acoustic ? 11.0 : 10.5;
}

} // namespace luthier
