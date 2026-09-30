#pragma once

/*  Performance Assist styles (auto-articulation.md 2).

    One row of constants per style, at Amount 50 %. AutoArticulator scales the
    row by the Amount: time windows and depths by (0.5 + a), probabilities by
    min (1, 2a). The unit tests pin every number here, so changing one is a
    spec change.
*/

#include <juce_core/juce_core.h>

namespace luthier
{

//==============================================================================
/** aa_style's choices, in parameter order. */
enum class AssistStyle
{
    cleanPop = 0,
    blues,
    rock,
    metal,
    jazz,
    country,
    fingerstyle,
    bass,
    numStyles
};

/** aa_rules' bits (section 3): position 0, legato 1, slide 2, vibrato 3,
    attack 4, alternate 5, strum 6, ornament 7, palm mute 8. */
namespace AssistRule
{
    enum : int
    {
        position  = 1 << 0,
        legato    = 1 << 1,
        slide     = 1 << 2,
        vibrato   = 1 << 3,
        attack    = 1 << 4,
        alternate = 1 << 5,
        strum     = 1 << 6,
        ornament  = 1 << 7,
        palmMute  = 1 << 8,

        all       = 511,
        numRules  = 9
    };

    /** "Positions", "Legato", ... for the rule checkboxes and the undo names. */
    const char* getName (int bitIndex) noexcept;

    /** One sentence per rule, for the accessibility labels (8). */
    const char* getDescription (int bitIndex) noexcept;
}

/** 3.8's ornament kinds. */
enum class AssistOrnament { none, bendInto, slideIn };

/** 3.7's chord handling. */
enum class AssistStrumMode
{
    strum,        ///< StrumGesture at the style's crossing
    pinchRoll,    ///< Fingerstyle: thumb first, fingers ascend across the roll
    together      ///< Bass: double-stops sound together
};

//==============================================================================
struct AutoArticulationStyle
{
    const char* name;
    const char* description;           ///< the popover's one line (7.1)

    // 3.2 / 3.3
    double legatoMaxIoiMs;
    int    legatoMaxInterval;          ///< semitones
    double slideMinOverlapMs;
    int    slideMaxInterval;           ///< semitones
    int    legatoChainCap;

    // 3.4
    double vibratoDelayMs;
    double vibratoDepthCents;
    double vibratoRateHz;

    // 3.5
    int    accentVelocity;             ///< MIDI 1-127

    // 3.6: register in semitones above the lowest open string; < 0 is off
    int    palmMuteRegister;
    double palmMuteDepth;
    double chugIoiMs;
    bool   metalFirstNoteMute;

    // 3.7
    double alternateIoiMs;
    double strumCrossingSps;
    bool   upStrokes;                  ///< false: every strum is a downstroke
    bool   swing;                      ///< Blues: the off-beat 8th is swung by the host's position
    AssistStrumMode strumMode;
    double rollMs;                     ///< pinch roll across the fingers

    // 3.8
    double ornamentProbability;
    AssistOrnament ornament;
    bool   bendWholeStep;              ///< Country: always a 2-semitone bend-into
    double bendRiseMs;
    int    slideInFrets;
    double slideInMs;
    double fallProbability;
    double fallSemitones;

    // 3.1
    double openStringBias;
    bool   metalOpenBias;              ///< lowest string +2, others -1
    double lateJoinMs;

    bool   inFree;                     ///< section 11
};

namespace AutoArticulationStyles
{
    constexpr int kNumStyles = (int) AssistStyle::numStyles;

    const AutoArticulationStyle& get (AssistStyle style) noexcept;
    const AutoArticulationStyle& get (int styleIndex) noexcept;

    /** "Clean/Pop", "Blues", ... in parameter order. */
    juce::StringArray getNames();

    /** Section 11: the styles Free plays; a Pro style plays as its nearest one. */
    bool isInFree (int styleIndex) noexcept;
    int nearestFreeStyle (int styleIndex) noexcept;
}

} // namespace luthier
