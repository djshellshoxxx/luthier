#include "LuthierMidiEvents.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace luthier
{

namespace
{
    struct ClassInfo
    {
        LuthierEventClass eventClass;
        const char* name;
        int schema;
    };

    /** midi-export 2.1, in the spec's order. Every class is at schema 1 in this
        version; a class that changes its fields moves to 2 and keeps reading 1. */
    constexpr ClassInfo kClasses[] =
    {
        { LuthierEventClass::note,      "NOTE",      1 },
        { LuthierEventClass::bend,      "BEND",      1 },
        { LuthierEventClass::slide,     "SLIDE",     1 },
        { LuthierEventClass::vibrato,   "VIBRATO",   1 },
        { LuthierEventClass::whammy,    "WHAMMY",    1 },
        { LuthierEventClass::strum,     "STRUM",     1 },
        { LuthierEventClass::rasgueado, "RASGUEADO", 1 },
        { LuthierEventClass::pick,      "PICK",      1 },
        { LuthierEventClass::squeak,    "SQUEAK",    1 },
        { LuthierEventClass::buzz,      "BUZZ",      1 },
        { LuthierEventClass::slideBar,  "SLIDE_BAR", 1 },
        { LuthierEventClass::clank,     "CLANK",     1 },
        { LuthierEventClass::character, "CHARACTER", 1 },
        { LuthierEventClass::workshop,  "WORKSHOP",  1 },
        { LuthierEventClass::bassTech,  "BASS_TECH", 1 },
        { LuthierEventClass::ranges,    "RANGES",    1 },
        { LuthierEventClass::snapshot,  "SNAPSHOT",  1 },
        { LuthierEventClass::section,   "SECTION",   1 },
    };

    static_assert (sizeof (kClasses) / sizeof (kClasses[0]) == (size_t) LuthierEvents::kNumClasses,
                   "every event class needs a name");

    /** dt and part are the event's own time and part, never ordinary fields. */
    bool isReservedKey (const juce::String& key)
    {
        return key == "dt" || key == "part";
    }

    int hexValue (char c) noexcept
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    }

    /** " dt=.. part=.. key=value ..." - everything after the class (and schema). */
    juce::String encodeFields (const LuthierEvent& event, const LuthierEvents::WriteOptions& options)
    {
        juce::String text;

        if (options.writeTiming)
        {
            if (options.sampleCorrection != 0)
                text << " dt=" << juce::String (options.sampleCorrection);

            if (event.part != 0)
                text << " part=" << juce::String (event.part);
        }

        for (const auto& [key, value] : event.fields)
        {
            if (options.stripIdentifiers)
                if (const auto* spec = LuthierEvents::findField (event.eventClass, key))
                    if (spec->isIdentifier)
                        continue;

            text << " " << key << "=" << LuthierEvents::escapeValue (value);
        }

        return text;
    }
}

//==============================================================================
LuthierEvent LuthierEvent::make (LuthierEventClass eventClass, juce::int64 sample, int part)
{
    LuthierEvent event;
    event.eventClass = eventClass;
    event.className = LuthierEvents::getClassName (eventClass);
    event.schemaVersion = LuthierEvents::getSchemaVersion (eventClass);
    event.sample = sample;
    event.part = part;
    return event;
}

bool LuthierEvent::has (const juce::String& key) const noexcept
{
    for (const auto& field : fields)
        if (field.first == key)
            return true;

    return false;
}

juce::String LuthierEvent::get (const juce::String& key) const
{
    for (const auto& field : fields)
        if (field.first == key)
            return field.second;

    if (const auto* spec = LuthierEvents::findField (eventClass, key))
        return spec->defaultValue;

    return {};
}

juce::int64 LuthierEvent::getInt (const juce::String& key) const
{
    juce::int64 value = 0;

    if (LuthierEvents::parseInt (get (key), value))
        return value;

    // A real where an integer was expected still means something.
    double real = 0.0;
    return LuthierEvents::parseReal (get (key), real) ? (juce::int64) std::llround (real) : 0;
}

double LuthierEvent::getReal (const juce::String& key) const
{
    double value = 0.0;
    return LuthierEvents::parseReal (get (key), value) ? value : 0.0;
}

LuthierEvent& LuthierEvent::set (const juce::String& key, const juce::String& value)
{
    // dt and part are written from `sample` and `part`; a field of that name
    // would be read back as the event's timing.
    jassert (LuthierEvents::isValidKey (key) && ! isReservedKey (key));

    if (! LuthierEvents::isValidKey (key) || isReservedKey (key))
        return *this;

    for (auto& field : fields)
    {
        if (field.first == key)
        {
            field.second = value;
            return *this;
        }
    }

    fields.emplace_back (key, value);
    return *this;
}

LuthierEvent& LuthierEvent::setInt (const juce::String& key, juce::int64 value)
{
    return set (key, juce::String (value));
}

LuthierEvent& LuthierEvent::setReal (const juce::String& key, double value)
{
    return set (key, LuthierEvents::formatReal (value));
}

void LuthierEvent::remove (const juce::String& key)
{
    fields.erase (std::remove_if (fields.begin(), fields.end(),
                                  [&key] (const auto& field) { return field.first == key; }),
                  fields.end());
}

bool LuthierEvent::hasSameContent (const LuthierEvent& other) const
{
    return className == other.className
        && schemaVersion == other.schemaVersion
        && sample == other.sample
        && part == other.part
        && fields == other.fields
        && opaqueSysEx == other.opaqueSysEx
        && opaqueText == other.opaqueText;
}

//==============================================================================
namespace LuthierEvents
{

const char* getClassName (LuthierEventClass eventClass) noexcept
{
    for (const auto& info : kClasses)
        if (info.eventClass == eventClass)
            return info.name;

    return "UNKNOWN";
}

LuthierEventClass getClassForName (const juce::String& name) noexcept
{
    for (const auto& info : kClasses)
        if (name == info.name)
            return info.eventClass;

    return LuthierEventClass::unknown;
}

int getSchemaVersion (LuthierEventClass eventClass) noexcept
{
    for (const auto& info : kClasses)
        if (info.eventClass == eventClass)
            return info.schema;

    return 1;
}

const std::vector<LuthierFieldSpec>& getFields (LuthierEventClass eventClass) noexcept
{
    /*  The documented fields (docs/MIDI_EXPORT_LUTHIER_PROFILE.md). Units: ms
        for durations and delays, semitones for pitch, cents for vibrato depth,
        frets for positions along the neck, 0..1 for intensities, and a string
        index where 0 is the highest string, -1 for none.

        The four technique classes also carry tech, value, second and curve: the
        notation technique they came from, exactly, so a score survives a MIDI
        round trip (9, "two serialisations of the same data"). */
    static const std::vector<LuthierFieldSpec> note
    {
        { "ch", "1", false }, { "key", "60", false }, { "str", "-1", false },
        { "fret", "-1", false }, { "flags", "", false }
    };

    static const std::vector<LuthierFieldSpec> bend
    {
        { "ch", "1", false }, { "key", "60", false }, { "str", "-1", false },
        { "art", "whole", false }, { "from", "0", false }, { "to", "0", false },
        { "dur", "0", false }, { "tech", "bend", false }, { "value", "0", false },
        { "second", "0", false }, { "curve", "", false }
    };

    static const std::vector<LuthierFieldSpec> slide
    {
        { "ch", "1", false }, { "key", "60", false }, { "str", "-1", false },
        { "kind", "legato", false }, { "from", "0", false }, { "to", "0", false },
        { "dur", "0", false }, { "tech", "slidelegato", false }, { "value", "0", false },
        { "second", "0", false }, { "curve", "", false }
    };

    static const std::vector<LuthierFieldSpec> vibrato
    {
        { "ch", "1", false }, { "key", "60", false }, { "str", "-1", false },
        { "rate", "5", false }, { "depth", "0", false }, { "delay", "0", false },
        { "style", "finger", false }, { "tech", "vibrato", false }, { "value", "0", false },
        { "second", "0", false }, { "curve", "", false }
    };

    static const std::vector<LuthierFieldSpec> whammy
    {
        { "ch", "1", false }, { "key", "60", false }, { "str", "-1", false },
        { "target", "0", false }, { "dur", "0", false }, { "tech", "whammy", false },
        { "value", "0", false }, { "second", "0", false }, { "curve", "", false }
    };

    static const std::vector<LuthierFieldSpec> strum
    {
        { "dir", "down", false }, { "cv", "0", false }, { "striker", "pick", false },
        { "mute", "0", false }, { "mask", "0", false }
    };

    static const std::vector<LuthierFieldSpec> rasgueado
    {
        // seq: finger:dir:ms:velocity for each sub-strum, comma separated.
        { "dir", "down", false }, { "seq", "", false }
    };

    static const std::vector<LuthierFieldSpec> pick
    {
        { "ch", "1", false }, { "str", "-1", false }, { "material", "celluloid", false },
        { "thick", "0.71", false }, { "angle", "0", false }, { "tip", "standard", false },
        { "wear", "0", false }, { "chirp", "0", false }
    };

    static const std::vector<LuthierFieldSpec> squeak
    {
        // midi-export 2.1's fields, plus string-squeak 11's string and start
        // and end positions (frets). Its "level" is the intensity.
        { "trigger", "slide", false }, { "str", "-1", false }, { "start", "0", false },
        { "end", "0", false }, { "dur", "0", false },
        { "intensity", "0", false }, { "material", "nickel", false }
    };

    static const std::vector<LuthierFieldSpec> buzz
    {
        { "str", "-1", false }, { "fret", "0", false }, { "dur", "0", false },
        { "intensity", "0", false }, { "sitar", "0", false }
    };

    static const std::vector<LuthierFieldSpec> slideBar
    {
        // slide-guitar 8: the bar's position over time, as ms:frets points
        // from the event, as well as where it is now.
        { "pos", "0", false }, { "path", "", false }, { "pressure", "light", false },
        { "slant", "0", false }, { "material", "glass", false }
    };

    static const std::vector<LuthierFieldSpec> clank
    {
        { "trigger", "fret", false }, { "mask", "0", false }, { "intensity", "0", false }
    };

    static const std::vector<LuthierFieldSpec> character
    {
        // midi-export 11: the character seed identifies a user's guitar.
        { "what", "seed", false }, { "seed", "0", true },
        { "temp", "20", false }, { "humidity", "45", false }
    };

    static const std::vector<LuthierFieldSpec> workshop
    {
        { "slot", "", false }, { "fit", "", false }, { "was", "", false }
    };

    static const std::vector<LuthierFieldSpec> bassTech
    {
        { "tech", "pluck", false }, { "str", "-1", false },
        { "pos", "0.5", false }, { "force", "0.8", false },
        { "contact", "0.8", false }   // SPEC-SWEEP BT-24: the fret contact (slap_fret_contact)
    };

    static const std::vector<LuthierFieldSpec> ranges
    {
        // min and max are the stock range; a value outside it warns on import (9).
        { "param", "", false }, { "on", "0", false }, { "value", "0", false },
        { "min", "0", false }, { "max", "1", false }
    };

    static const std::vector<LuthierFieldSpec> snapshot
    {
        { "slot", "0", false }, { "name", "", false }, { "morph", "0", false }
    };

    static const std::vector<LuthierFieldSpec> section
    {
        { "name", "", false }, { "index", "0", false }, { "edge", "start", false }
    };

    static const std::vector<LuthierFieldSpec> none;

    switch (eventClass)
    {
        case LuthierEventClass::note:       return note;
        case LuthierEventClass::bend:       return bend;
        case LuthierEventClass::slide:      return slide;
        case LuthierEventClass::vibrato:    return vibrato;
        case LuthierEventClass::whammy:     return whammy;
        case LuthierEventClass::strum:      return strum;
        case LuthierEventClass::rasgueado:  return rasgueado;
        case LuthierEventClass::pick:       return pick;
        case LuthierEventClass::squeak:     return squeak;
        case LuthierEventClass::buzz:       return buzz;
        case LuthierEventClass::slideBar:   return slideBar;
        case LuthierEventClass::clank:      return clank;
        case LuthierEventClass::character:  return character;
        case LuthierEventClass::workshop:   return workshop;
        case LuthierEventClass::bassTech:   return bassTech;
        case LuthierEventClass::ranges:     return ranges;
        case LuthierEventClass::snapshot:   return snapshot;
        case LuthierEventClass::section:    return section;
        case LuthierEventClass::unknown:
        default:                            return none;
    }
}

const LuthierFieldSpec* findField (LuthierEventClass eventClass, const juce::String& key) noexcept
{
    for (const auto& spec : getFields (eventClass))
        if (key == spec.key)
            return &spec;

    return nullptr;
}

bool isValidClassName (const juce::String& name) noexcept
{
    return name.isNotEmpty() && name.length() <= 32
        && name.containsOnly ("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
}

bool isValidKey (const juce::String& key) noexcept
{
    return key.isNotEmpty() && key.length() <= 32
        && key.containsOnly ("abcdefghijklmnopqrstuvwxyz0123456789_");
}

//==============================================================================
juce::String formatReal (double value)
{
    if (! std::isfinite (value))
        return "0";

    if (juce::exactlyEqual (value, std::floor (value)) && std::abs (value) < 1.0e15)
        return juce::String ((juce::int64) value);

    // Six significant figures reads back exactly for almost every value a
    // person types; the rest get seventeen, which is exact for any double.
    const juce::String shortForm (value);
    double readBack = 0.0;

    if (parseReal (shortForm, readBack) && juce::exactlyEqual (readBack, value))
        return shortForm;

    return juce::String (value, 16, true);
}

bool parseReal (const juce::String& text, double& result)
{
    if (text.isEmpty() || text.length() > 40
          || ! text.containsOnly ("0123456789+-.eE")
          || ! text.containsAnyOf ("0123456789"))
        return false;

    result = text.getDoubleValue();
    return std::isfinite (result);
}

bool parseInt (const juce::String& text, juce::int64& result)
{
    const auto digits = text.startsWithChar ('-') ? text.substring (1) : text;

    if (digits.isEmpty() || digits.length() > 18 || ! digits.containsOnly ("0123456789"))
        return false;

    result = text.getLargeIntValue();
    return true;
}

juce::String escapeValue (const juce::String& value)
{
    static constexpr const char* hex = "0123456789ABCDEF";

    std::string out;

    for (auto* p = reinterpret_cast<const unsigned char*> (value.toRawUTF8()); *p != 0; ++p)
    {
        const auto c = *p;

        if (c > 0x20 && c < 0x7F && c != '%' && c != '=')
        {
            out += (char) c;
        }
        else
        {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }

    return juce::String (out);
}

bool unescapeValue (const juce::String& text, juce::String& result)
{
    std::string bytes;

    for (const char* p = text.toRawUTF8(); *p != 0;)
    {
        const auto c = (unsigned char) *p;

        if (c == '%')
        {
            const int high = hexValue (p[1]);
            const int low = high >= 0 ? hexValue (p[2]) : -1;

            // %00 would end the string early wherever it was read back.
            if (high < 0 || low < 0 || (high == 0 && low == 0))
                return false;

            bytes += (char) ((high << 4) | low);
            p += 3;
        }
        else if (c > 0x20 && c < 0x7F && c != '=')
        {
            bytes += (char) c;
            ++p;
        }
        else
        {
            return false;
        }
    }

    if (bytes.empty())
    {
        result = {};
        return true;
    }

    if (! juce::CharPointer_UTF8::isValidString (bytes.c_str(), (int) bytes.size()))
        return false;

    result = juce::String::fromUTF8 (bytes.c_str(), (int) bytes.size());
    return true;
}

juce::uint8 checksum (const void* data, size_t numBytes) noexcept
{
    unsigned int sum = 0;
    const auto* bytes = static_cast<const juce::uint8*> (data);

    for (size_t i = 0; i < numBytes; ++i)
        sum += bytes[i];

    return (juce::uint8) (sum & 0x7F);
}

//==============================================================================
juce::String encodePayload (const LuthierEvent& event, const WriteOptions& options)
{
    return event.className + " " + juce::String (event.schemaVersion) + encodeFields (event, options);
}

juce::String encodeText (const LuthierEvent& event, const WriteOptions& options)
{
    return juce::String (kTextPrefix) + event.className + encodeFields (event, options);
}

juce::MidiMessage encodeSysEx (const LuthierEvent& event, const WriteOptions& options)
{
    const auto payload = encodePayload (event, options);
    const auto* text = payload.toRawUTF8();
    const auto length = (size_t) payload.getNumBytesAsUTF8();

    std::vector<juce::uint8> data;
    data.reserve (length + 5);

    data.push_back (kManufacturerId);
    data.push_back ((juce::uint8) 'L');
    data.push_back ((juce::uint8) 'T');
    data.push_back (kWireVersion);

    // The payload is ASCII by construction (escapeValue), so the mask never
    // changes a byte; it only guarantees the SysEx stays legal if it ever did.
    for (size_t i = 0; i < length; ++i)
        data.push_back ((juce::uint8) (text[i] & 0x7F));

    data.push_back (checksum (data.data() + 4, length));

    return juce::MidiMessage::createSysExMessage (data.data(), (int) data.size());
}

//==============================================================================
bool decodePayload (const juce::String& payload, LuthierEvent& event,
                    juce::int64& sampleCorrection, juce::String& error)
{
    sampleCorrection = 0;

    // Single spaces only: an empty token means a doubled, leading or trailing
    // space, which this writer never produces.
    const auto tokens = juce::StringArray::fromTokens (payload, " ", "");

    if (tokens.size() < 2 || tokens.contains (juce::String()))
    {
        error = "a LUTHIER event needs a class and a schema, separated by single spaces";
        return false;
    }

    if (! isValidClassName (tokens[0]))
    {
        error = "\"" + tokens[0].substring (0, 40) + "\" is not an event class name";
        return false;
    }

    juce::int64 schema = 0;

    if (! parseInt (tokens[1], schema) || schema < 1 || schema > 9999)
    {
        error = "\"" + tokens[1].substring (0, 40) + "\" is not a schema version";
        return false;
    }

    LuthierEvent result;
    result.className = tokens[0];
    result.eventClass = getClassForName (tokens[0]);
    result.schemaVersion = (int) schema;

    for (int i = 2; i < tokens.size(); ++i)
    {
        const auto& token = tokens[i];
        const int equals = token.indexOfChar ('=');

        juce::String value;

        if (equals <= 0 || ! isValidKey (token.substring (0, equals))
              || ! unescapeValue (token.substring (equals + 1), value))
        {
            error = "field \"" + token.substring (0, 40) + "\" is malformed";
            return false;
        }

        const auto key = token.substring (0, equals);

        if (key == "dt")
        {
            if (! parseInt (value, sampleCorrection))
            {
                error = "dt \"" + value.substring (0, 40) + "\" is not a whole number of samples";
                return false;
            }
        }
        else if (key == "part")
        {
            juce::int64 part = 0;

            if (! parseInt (value, part) || part < 0 || part > 15)
            {
                error = "part \"" + value.substring (0, 40) + "\" is not 0 to 15";
                return false;
            }

            result.part = (int) part;
        }
        else
        {
            if (result.has (key))
            {
                error = "field \"" + key + "\" appears twice";
                return false;
            }

            result.fields.emplace_back (key, value);
        }
    }

    event = std::move (result);
    return true;
}

bool decodeText (const juce::String& text, int schemaVersion, LuthierEvent& event,
                 juce::int64& sampleCorrection, juce::String& error)
{
    const juce::String prefix (kTextPrefix);

    if (! text.startsWith (prefix))
    {
        error = "not a LUTHIER text event";
        return false;
    }

    // "LUTHIER: STRUM dir=down" is the payload with its schema taken out, so
    // putting the marker's schema back makes it one again.
    const auto body = text.substring (prefix.length());
    const int space = body.indexOfChar (' ');
    const auto className = space < 0 ? body : body.substring (0, space);
    const auto rest = space < 0 ? juce::String() : body.substring (space);

    return decodePayload (className + " " + juce::String (schemaVersion) + rest,
                          event, sampleCorrection, error);
}

bool isLuthierSysEx (const juce::uint8* data, int numBytes) noexcept
{
    return data != nullptr && numBytes >= 3
        && data[0] == kManufacturerId && data[1] == (juce::uint8) 'L' && data[2] == (juce::uint8) 'T';
}

bool hasValidChecksum (const juce::uint8* data, int numBytes) noexcept
{
    return isLuthierSysEx (data, numBytes) && numBytes >= 6
        && checksum (data + 4, (size_t) (numBytes - 5)) == data[numBytes - 1];
}

bool decodeSysEx (const juce::uint8* data, int numBytes, LuthierEvent& event,
                  juce::int64& sampleCorrection, juce::String& error)
{
    if (! isLuthierSysEx (data, numBytes))
    {
        error = "not a Luthier event SysEx";
        return false;
    }

    if (numBytes < 6)
    {
        error = "the event SysEx is truncated";
        return false;
    }

    if (data[3] == 0 || data[3] > kWireVersion)
    {
        error = "the event SysEx uses wire format " + juce::String ((int) data[3])
                  + ", which this version does not read";
        return false;
    }

    const auto* payload = data + 4;
    const int length = numBytes - 5;

    for (int i = 0; i < length; ++i)
    {
        if (payload[i] < 0x20 || payload[i] > 0x7E)
        {
            error = "the event SysEx holds a byte that is not text";
            return false;
        }
    }

    if (checksum (payload, (size_t) length) != data[numBytes - 1])
    {
        error = "the event SysEx checksum does not match its contents";
        return false;
    }

    return decodePayload (juce::String::fromUTF8 (reinterpret_cast<const char*> (payload), length),
                          event, sampleCorrection, error);
}

} // namespace LuthierEvents

} // namespace luthier
