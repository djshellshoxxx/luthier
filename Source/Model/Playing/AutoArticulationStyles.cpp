#include "AutoArticulationStyles.h"

namespace luthier
{

namespace
{
    // auto-articulation.md 2, one row per style, at Amount 50 %.
    //
    //  name, description,
    //  legato IOI / interval, slide overlap / interval, chain cap,
    //  vibrato delay / depth / rate,
    //  accent velocity,
    //  palm-mute register / depth, chug IOI, metal first note,
    //  alternate IOI, strum crossing, up-strokes, swing, strum mode, roll,
    //  ornament probability / kind, whole step, rise, slide-in frets / ms, fall probability / semitones,
    //  open bias, metal bias, late join,
    //  in Free
    constexpr AutoArticulationStyle kStyles[] =
    {
        { "Clean/Pop", "Clean slurs, gentle vibrato on long notes, strums that alternate with the beat.",
          250.0, 3, 40.0, 2, 3,
          350.0, 12.0, 5.2,
          105,
          -1, 0.0, 300.0, false,
          200.0, 220.0, true, false, AssistStrumMode::strum, 15.0,
          0.0, AssistOrnament::none, false, 110.0, 1, 45.0, 0.0, 2.0,
          1.0, false, 40.0,
          true },

        { "Blues", "Wide vibrato, bends into phrase starts, falls off long notes, swung strums.",
          400.0, 3, 25.0, 4, 4,
          220.0, 30.0, 5.5,
          100,
          -1, 0.0, 300.0, false,
          200.0, 180.0, true, true, AssistStrumMode::strum, 15.0,
          0.30, AssistOrnament::bendInto, false, 110.0, 1, 45.0, 0.20, 2.0,
          0.0, false, 40.0,
          false },

        { "Rock", "Palm-muted low chugs, alternate picking, bends into phrase starts.",
          300.0, 4, 35.0, 3, 4,
          260.0, 25.0, 5.8,
          96,
          7, 0.55, 300.0, false,
          200.0, 260.0, true, false, AssistStrumMode::strum, 15.0,
          0.15, AssistOrnament::bendInto, false, 90.0, 1, 45.0, 0.10, 2.0,
          0.0, false, 40.0,
          true },

        { "Metal", "Tight palm-muted chugs, downstroke strums, long legato runs.",
          220.0, 5, 50.0, 2, 6,
          200.0, 35.0, 6.5,
          90,
          10, 0.75, 350.0, true,
          220.0, 400.0, false, false, AssistStrumMode::strum, 15.0,
          0.10, AssistOrnament::bendInto, false, 70.0, 1, 45.0, 0.0, 2.0,
          0.0, true, 40.0,
          false },

        { "Jazz", "Fretted positions, slow light vibrato, slide-ins, downstroke comping.",
          350.0, 3, 30.0, 2, 3,
          400.0, 8.0, 4.8,
          110,
          -1, 0.0, 300.0, false,
          200.0, 600.0, false, false, AssistStrumMode::strum, 15.0,
          0.15, AssistOrnament::slideIn, false, 110.0, 1, 45.0, 0.0, 2.0,
          -2.0, false, 40.0,
          false },

        { "Country", "Open strings, whole-step bends into notes, light chicken-pickin' mutes.",
          260.0, 3, 30.0, 5, 3,
          380.0, 10.0, 5.5,
          100,
          5, 0.35, 300.0, false,
          200.0, 240.0, true, false, AssistStrumMode::strum, 15.0,
          0.25, AssistOrnament::bendInto, true, 140.0, 1, 45.0, 0.15, 1.0,
          1.0, false, 40.0,
          false },

        { "Fingerstyle", "Thumb then fingers on chords, open strings, soft slurs.",
          350.0, 3, 40.0, 2, 3,
          400.0, 10.0, 5.0,
          108,
          -1, 0.0, 300.0, false,
          200.0, 220.0, true, false, AssistStrumMode::pinchRoll, 15.0,
          0.0, AssistOrnament::none, false, 110.0, 1, 45.0, 0.0, 2.0,
          2.0, false, 60.0,
          true },

        { "Bass", "Slides into notes, long slurs, double-stops together.",
          250.0, 5, 40.0, 5, 3,
          450.0, 8.0, 4.8,
          105,
          -1, 0.0, 300.0, false,
          200.0, 220.0, false, false, AssistStrumMode::together, 15.0,
          0.10, AssistOrnament::slideIn, false, 110.0, 2, 60.0, 0.0, 2.0,
          1.0, false, 40.0,
          true },
    };

    static_assert (sizeof (kStyles) / sizeof (kStyles[0]) == (size_t) AssistStyle::numStyles,
                   "one row per style");

    constexpr const char* kRuleNames[] =
    {
        "Positions", "Legato", "Slides", "Vibrato", "Attack",
        "Alt picking", "Strums", "Ornaments", "Palm mute"
    };

    constexpr const char* kRuleDescriptions[] =
    {
        "Picks the string and position a guitarist would, keeping the hand in one box.",
        "Turns overlapping notes on one string into hammer-ons and pull-offs.",
        "Turns long overlaps and position shifts into slides.",
        "Adds delayed vibrato to held lead notes.",
        "Brightens accented notes and softens quiet ones.",
        "Alternates down- and up-strokes on fast picked notes.",
        "Strums chords down and up with the beat.",
        "Adds the occasional bend into a note, slide-in or fall.",
        "Palm-mutes low repeated notes and chugs."
    };
}

//==============================================================================
const char* AssistRule::getName (int bitIndex) noexcept
{
    return juce::isPositiveAndBelow (bitIndex, (int) numRules) ? kRuleNames[bitIndex] : "";
}

const char* AssistRule::getDescription (int bitIndex) noexcept
{
    return juce::isPositiveAndBelow (bitIndex, (int) numRules) ? kRuleDescriptions[bitIndex] : "";
}

const AutoArticulationStyle& AutoArticulationStyles::get (AssistStyle style) noexcept
{
    return get ((int) style);
}

const AutoArticulationStyle& AutoArticulationStyles::get (int styleIndex) noexcept
{
    return kStyles[juce::jlimit (0, kNumStyles - 1, styleIndex)];
}

juce::StringArray AutoArticulationStyles::getNames()
{
    juce::StringArray names;

    for (const auto& s : kStyles)
        names.add (s.name);

    return names;
}

bool AutoArticulationStyles::isInFree (int styleIndex) noexcept
{
    return get (styleIndex).inFree;
}

int AutoArticulationStyles::nearestFreeStyle (int styleIndex) noexcept
{
    // Section 11: Blues and Metal play as Rock, Jazz and Country as Clean/Pop.
    switch ((AssistStyle) juce::jlimit (0, kNumStyles - 1, styleIndex))
    {
        case AssistStyle::blues:
        case AssistStyle::metal:   return (int) AssistStyle::rock;
        case AssistStyle::jazz:
        case AssistStyle::country: return (int) AssistStyle::cleanPop;
        case AssistStyle::cleanPop:
        case AssistStyle::rock:
        case AssistStyle::fingerstyle:
        case AssistStyle::bass:
        case AssistStyle::numStyles:
        default:                   return juce::jlimit (0, kNumStyles - 1, styleIndex);
    }
}

} // namespace luthier
