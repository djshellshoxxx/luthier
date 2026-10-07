#include "ParamMeta.h"

namespace luthier::ParamMeta
{

ParamUnit getUnit (const juce::AudioProcessorParameter& parameter)
{
    if (dynamic_cast<const juce::AudioParameterBool*> (&parameter) != nullptr)   return ParamUnit::boolean;
    if (dynamic_cast<const juce::AudioParameterChoice*> (&parameter) != nullptr) return ParamUnit::index;
    if (dynamic_cast<const juce::AudioParameterInt*> (&parameter) != nullptr)    return ParamUnit::count;

    const auto label = parameter.getLabel().trim();

    if (label.isEmpty())                                  return ParamUnit::amount;
    if (label == "Hz" || label == "kHz")                  return ParamUnit::hz;
    if (label == "dB")                                    return ParamUnit::db;
    if (label == "ms")                                    return ParamUnit::ms;
    if (label == "s")                                     return ParamUnit::seconds;
    if (label.startsWith ("%"))                           return ParamUnit::percent;
    if (label == "st" || label == "semitones")            return ParamUnit::semitones;
    if (label == "cents" || label == "ct" || label == "c/fret")            return ParamUnit::cents;
    if (label == "H" || label == "mH")                    return ParamUnit::henry;
    if (label == "F" || label == "nF" || label == "pF" || label == "uF") return ParamUnit::farad;
    if (label.endsWithIgnoreCase ("ohm") || label.containsChar (0x03a9) || label == "k") return ParamUnit::ohm;
    if (label == "mm")                                    return ParamUnit::mm;
    if (label == "ct/s")                                  return ParamUnit::centsPerSecond;
    if (label == "cm")                                    return ParamUnit::centimetres;
    if (label == "u")                                     return ParamUnit::amount;  // normalised plate/body units
    if (label == "m")                                     return ParamUnit::metres;
    if (label == "deg")                                   return ParamUnit::degrees;
    if (label == "C")                                     return ParamUnit::celsius;
    if (label == "x")                                     return ParamUnit::multiplier;
    if (label == "sps")                                   return ParamUnit::stringsPerSecond;
    if (label == "h")                                     return ParamUnit::hours;
    if (label == "frets" || label == "fret" || label == "bars" || label == "steps") return ParamUnit::count;

    return ParamUnit::unknown;
}

juce::String getUnitSuffix (ParamUnit unit)
{
    switch (unit)
    {
        case ParamUnit::hz:               return "Hz";
        case ParamUnit::db:               return "dB";
        case ParamUnit::ms:               return "ms";
        case ParamUnit::seconds:          return "s";
        case ParamUnit::percent:          return "%";
        case ParamUnit::semitones:        return "st";
        case ParamUnit::cents:            return "cents";
        case ParamUnit::henry:            return "H";
        case ParamUnit::farad:            return "F";
        case ParamUnit::ohm:              return "ohm";
        case ParamUnit::mm:               return "mm";
        case ParamUnit::metres:           return "m";
        case ParamUnit::centimetres:      return "cm";
        case ParamUnit::centsPerSecond:   return "ct/s";
        case ParamUnit::degrees:          return "deg";
        case ParamUnit::celsius:          return "C";
        case ParamUnit::multiplier:       return "x";
        case ParamUnit::stringsPerSecond: return "sps";
        case ParamUnit::hours:            return "h";
        case ParamUnit::amount: case ParamUnit::count: case ParamUnit::index:
        case ParamUnit::boolean: case ParamUnit::unknown:
        default:                          return {};
    }
}

juce::String getUnitName (ParamUnit unit)
{
    switch (unit)
    {
        case ParamUnit::amount:           return "amount";
        case ParamUnit::hz:               return "hertz";
        case ParamUnit::db:               return "decibels";
        case ParamUnit::ms:               return "milliseconds";
        case ParamUnit::seconds:          return "seconds";
        case ParamUnit::percent:          return "percent";
        case ParamUnit::semitones:        return "semitones";
        case ParamUnit::cents:            return "cents";
        case ParamUnit::henry:            return "henries";
        case ParamUnit::farad:            return "farads";
        case ParamUnit::ohm:              return "ohms";
        case ParamUnit::mm:               return "millimetres";
        case ParamUnit::metres:           return "metres";
        case ParamUnit::centimetres:      return "centimetres";
        case ParamUnit::centsPerSecond:   return "cents per second";
        case ParamUnit::degrees:          return "degrees";
        case ParamUnit::celsius:          return "degrees Celsius";
        case ParamUnit::multiplier:       return "times";
        case ParamUnit::stringsPerSecond: return "strings per second";
        case ParamUnit::hours:            return "hours";
        case ParamUnit::count:            return "count";
        case ParamUnit::index:            return "choice";
        case ParamUnit::boolean:          return "switch";
        case ParamUnit::unknown:
        default:                          return {};
    }
}

juce::String getCategory (const juce::String& parameterId)
{
    const auto head = parameterId.upToFirstOccurrenceOf ("_", false, false);
    return head.isNotEmpty() ? head : parameterId;
}

} // namespace luthier::ParamMeta
