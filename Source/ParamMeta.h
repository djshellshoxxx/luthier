#pragma once

/*
    SPEC-SWEEP (ui-wiring UW-10, §1): every parameter's unit and category.

    The unit comes from the parameter itself - a bool is a switch, a choice an
    index, an int a count, and a float carries its unit as its label ("dB",
    "Hz", "mm"; no label means a 0..1 amount) - so it can never drift from
    what the host shows. The category is the family its id starts with
    ("amp", "pickup", "slide"), which is how the parameter tree is organised.
*/

#include <juce_audio_processors/juce_audio_processors.h>

namespace luthier
{

enum class ParamUnit
{
    unknown = 0,
    amount,      ///< 0..1, no unit
    hz, db, ms, seconds, percent, semitones, cents, henry, farad, ohm, mm, metres,
    degrees, celsius, multiplier, stringsPerSecond, hours, count, index, boolean
};

namespace ParamMeta
{
    ParamUnit getUnit (const juce::AudioProcessorParameter& parameter);
    juce::String getUnitSuffix (ParamUnit unit);
    juce::String getCategory (const juce::String& parameterId);

    /** The unit named in plain words, for a screen reader ("decibels"). */
    juce::String getUnitName (ParamUnit unit);
}

} // namespace luthier
