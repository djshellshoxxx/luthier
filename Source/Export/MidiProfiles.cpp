#include "MidiProfiles.h"

#include "../Support/MidiCapture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>

namespace luthier
{

namespace
{
    /** The header's signature: the non-commercial ID, then the profile's name. */
    constexpr juce::uint8 kHeaderSignature[] = { 0x7D, 'L', 'U', 'T', 'H', 'I', 'E', 'R' };

    /** More than any performance, few enough that a hostile file cannot make
        import eat the machine. */
    constexpr size_t kMaxEvents = 4000000;

    //==========================================================================
    /** "Byte 0x0001A2 (418)" - the line reference a refusal banner shows (12). */
    juce::String byteRef (size_t offset)
    {
        return "Byte 0x" + juce::String::toHexString ((juce::int64) offset).paddedLeft ('0', 6).toUpperCase()
                 + " (" + juce::String ((juce::int64) offset) + ")";
    }

    juce::uint32 readBigEndian32 (const juce::uint8* p) noexcept
    {
        return ((juce::uint32) p[0] << 24) | ((juce::uint32) p[1] << 16)
             | ((juce::uint32) p[2] << 8) | (juce::uint32) p[3];
    }

    int readBigEndian16 (const juce::uint8* p) noexcept
    {
        return ((int) p[0] << 8) | (int) p[1];
    }

    /** A standard-MIDI-file variable-length number: at most four bytes. */
    bool readVarLen (const juce::uint8* data, size_t& position, size_t end, juce::uint32& value) noexcept
    {
        value = 0;

        for (int i = 0; i < 4; ++i)
        {
            if (position >= end)
                return false;

            const auto byte = data[position++];
            value = (value << 7) | (juce::uint32) (byte & 0x7F);

            if ((byte & 0x80) == 0)
                return true;
        }

        return false;
    }

    void appendVarLen (std::vector<juce::uint8>& out, juce::uint32 value)
    {
        juce::uint8 buffer[5];
        int count = 0;

        buffer[count++] = (juce::uint8) (value & 0x7F);

        while ((value >>= 7) != 0 && count < 5)
            buffer[count++] = (juce::uint8) ((value & 0x7F) | 0x80);

        while (count > 0)
            out.push_back (buffer[--count]);
    }

    juce::MidiMessage makeMeta (int type, const std::vector<juce::uint8>& payload)
    {
        std::vector<juce::uint8> bytes { 0xFF, (juce::uint8) type };
        appendVarLen (bytes, (juce::uint32) payload.size());
        bytes.insert (bytes.end(), payload.begin(), payload.end());
        return juce::MidiMessage (bytes.data(), (int) bytes.size());
    }

    juce::MidiMessage makeText (const juce::String& text)
    {
        return juce::MidiMessage::textMetaEvent (1, text);
    }

    /** A meta's text: UTF-8 when it is, Latin-1 when it is not, never an assertion. */
    juce::String readText (const juce::uint8* data, const MidiProfiles::SmfEvent& event)
    {
        if (event.dataSize == 0)
            return {};

        const auto* text = reinterpret_cast<const char*> (data + event.dataOffset);

        if (juce::CharPointer_UTF8::isValidString (text, (int) event.dataSize))
            return juce::String::fromUTF8 (text, (int) event.dataSize);

        juce::String latin;

        for (size_t i = 0; i < event.dataSize; ++i)
            latin += (juce::juce_wchar) data[event.dataOffset + i];

        return latin;
    }

    int dataBytesFor (juce::uint8 status) noexcept
    {
        return ((status & 0xF0) == 0xC0 || (status & 0xF0) == 0xD0) ? 1 : 2;
    }

    //==========================================================================
    /*  The file's clock: ticks to seconds to samples, through the tempo map as
        the file states it (microseconds per quarter, on ticks).

        Export builds it from the performance, quantised exactly as the file
        will hold it, and import builds it from the file, so both ends run the
        same arithmetic on the same numbers. That is what makes dt exact: the
        sample import computes for a tick is the one export subtracted. */
    class FileTempoMap
    {
    public:
        struct Segment
        {
            juce::int64 tick = 0;
            int microsecondsPerQuarter = 500000;
            double startSeconds = 0.0;
            double secondsPerTick = 0.0;
        };

        FileTempoMap (int ppqToUse, double rate)
            : ppq (juce::jmax (1, ppqToUse)), sampleRate (rate)
        {
        }

        void add (juce::int64 tick, int microsecondsPerQuarter)
        {
            Segment segment;
            segment.tick = juce::jmax ((juce::int64) 0, tick);
            segment.microsecondsPerQuarter = juce::jlimit (1, 0xFFFFFF, microsecondsPerQuarter);
            entries.push_back (segment);
        }

        /** Sorts, keeps the last change at any one tick, and starts at tick zero
            (at 120 bpm, the standard's default, if nothing says otherwise). */
        void finalise()
        {
            std::stable_sort (entries.begin(), entries.end(),
                              [] (const Segment& a, const Segment& b) { return a.tick < b.tick; });

            std::vector<Segment> merged;

            for (const auto& entry : entries)
            {
                if (! merged.empty() && merged.back().tick == entry.tick)
                    merged.back() = entry;
                else
                    merged.push_back (entry);
            }

            if (merged.empty() || merged.front().tick != 0)
                merged.insert (merged.begin(), Segment {});

            double seconds = 0.0;

            for (size_t i = 0; i < merged.size(); ++i)
            {
                auto& segment = merged[i];
                segment.secondsPerTick = (double) segment.microsecondsPerQuarter * 1.0e-6 / (double) ppq;

                if (i > 0)
                    seconds += (double) (segment.tick - merged[i - 1].tick) * merged[i - 1].secondsPerTick;

                segment.startSeconds = seconds;
            }

            segments = std::move (merged);
        }

        const std::vector<Segment>& getSegments() const noexcept { return segments; }

        double tickToSeconds (double tick) const noexcept
        {
            size_t index = 0;

            for (size_t i = 1; i < segments.size() && (double) segments[i].tick <= tick; ++i)
                index = i;

            const auto& segment = segments[index];
            return segment.startSeconds + (tick - (double) segment.tick) * segment.secondsPerTick;
        }

        double secondsToTick (double seconds) const noexcept
        {
            size_t index = 0;

            for (size_t i = 1; i < segments.size() && segments[i].startSeconds <= seconds; ++i)
                index = i;

            const auto& segment = segments[index];
            return (double) segment.tick + (seconds - segment.startSeconds) / segment.secondsPerTick;
        }

        juce::int64 tickToSample (juce::int64 tick) const noexcept
        {
            return (juce::int64) std::llround (tickToSeconds ((double) tick) * sampleRate);
        }

        juce::int64 sampleToTick (juce::int64 sample) const noexcept
        {
            return juce::jmax ((juce::int64) 0,
                               (juce::int64) std::llround (secondsToTick ((double) sample / sampleRate)));
        }

    private:
        int ppq;
        double sampleRate;
        std::vector<Segment> entries, segments;
    };

    FileTempoMap makeFileTempoMap (const MidiPerformance& performance, int ppq)
    {
        FileTempoMap map (ppq, performance.getSampleRate());

        for (const auto& change : performance.getTempoMap())
            map.add ((juce::int64) std::llround (change.beat * (double) ppq),
                     juce::roundToInt (60000000.0 / change.bpm));

        map.finalise();
        return map;
    }

    //==========================================================================
    struct LuthierHeader
    {
        double sampleRate = 0.0;
        int ppq = 0;
        juce::int64 messages = -1;
        juce::int64 events = -1;
        double bendRange = 2.0;
        juce::StringArray partNames;
        juce::String guitarName, presetName, characterSeed;
    };

    juce::MidiMessage makeHeader (const juce::String& fields)
    {
        std::vector<juce::uint8> payload (std::begin (kHeaderSignature), std::end (kHeaderSignature));
        payload.push_back (LuthierEvents::kWireVersion);

        const auto* text = fields.toRawUTF8();
        const auto length = (size_t) fields.getNumBytesAsUTF8();
        const auto start = payload.size();

        for (size_t i = 0; i < length; ++i)
            payload.push_back ((juce::uint8) (text[i] & 0x7F));

        payload.push_back (LuthierEvents::checksum (payload.data() + start, length));

        return makeMeta (0x7F, payload);
    }

    bool isHeader (const MidiProfiles::SmfEvent& event, const juce::uint8* data) noexcept
    {
        return event.isMeta() && event.metaType == 0x7F
            && event.dataSize >= sizeof (kHeaderSignature) + 2
            && std::memcmp (data + event.dataOffset, kHeaderSignature, sizeof (kHeaderSignature)) == 0;
    }

    bool parseHeader (const juce::uint8* bytes, size_t size, LuthierHeader& header, juce::String& error)
    {
        const size_t signatureLength = sizeof (kHeaderSignature);
        const auto version = bytes[signatureLength];

        if (version == 0 || version > LuthierEvents::kWireVersion)
        {
            error = "the LUTHIER header is in format " + juce::String ((int) version)
                      + ", from a later version of Luthier";
            return false;
        }

        const auto* text = bytes + signatureLength + 1;
        const size_t length = size - signatureLength - 2;

        for (size_t i = 0; i < length; ++i)
        {
            if (text[i] < 0x20 || text[i] > 0x7E)
            {
                error = "the LUTHIER header holds a byte that is not text";
                return false;
            }
        }

        if (LuthierEvents::checksum (text, length) != bytes[size - 1])
        {
            error = "the LUTHIER header checksum does not match its contents";
            return false;
        }

        const auto fields = juce::String::fromUTF8 (reinterpret_cast<const char*> (text), (int) length);

        for (const auto& token : juce::StringArray::fromTokens (fields, " ", ""))
        {
            if (token.isEmpty())
                continue;

            const int equals = token.indexOfChar ('=');
            const auto key = token.substring (0, juce::jmax (0, equals));
            const auto raw = token.substring (equals + 1);

            juce::String value;
            double real = 0.0;
            juce::int64 whole = 0;

            if (equals <= 0 || ! LuthierEvents::unescapeValue (raw, value))
            {
                error = "the LUTHIER header field \"" + token.substring (0, 40) + "\" is malformed";
                return false;
            }

            const auto bad = [&error, &token]
            {
                error = "the LUTHIER header field \"" + token.substring (0, 40) + "\" has a bad value";
                return false;
            };

            if (key == "sr")
            {
                if (! LuthierEvents::parseReal (value, real) || real <= 0.0)
                    return bad();

                header.sampleRate = real;
            }
            else if (key == "ppq")
            {
                if (! LuthierEvents::parseInt (value, whole) || whole <= 0)
                    return bad();

                header.ppq = (int) whole;
            }
            else if (key == "messages" || key == "events")
            {
                if (! LuthierEvents::parseInt (value, whole) || whole < 0)
                    return bad();

                (key == "messages" ? header.messages : header.events) = whole;
            }
            else if (key == "bend")
            {
                if (! LuthierEvents::parseReal (value, real) || real < 0.0)
                    return bad();

                header.bendRange = real;
            }
            else if (key == "parts")
            {
                // Split before unescaping: a comma inside a name is %2C.
                header.partNames.clear();

                for (const auto& piece : juce::StringArray::fromTokens (raw, ",", ""))
                {
                    juce::String name;

                    if (! LuthierEvents::unescapeValue (piece, name))
                        return bad();

                    header.partNames.add (name);
                }
            }
            else if (key == "guitar")
            {
                header.guitarName = value;
            }
            else if (key == "preset")
            {
                header.presetName = value;
            }
            else if (key == "seed")
            {
                header.characterSeed = value;
            }

            // profile, split, sysex and anything a later version adds are for
            // people and inspectors; nothing here depends on them.
        }

        if (header.sampleRate <= 0.0)
        {
            error = "the LUTHIER header has no sample rate";
            return false;
        }

        return true;
    }

    //==========================================================================
    struct TimingTag
    {
        juce::int64 dt = 0;
        int part = -1;
        juce::int64 n = -1;
        bool pending = false;
        size_t offset = 0;
    };

    bool isTimingTag (const juce::String& text)
    {
        const juce::String tag (LuthierEvents::kTimingTag);
        return text == tag || text.startsWith (tag + " ");
    }

    bool parseTimingTag (const juce::String& text, TimingTag& tag, juce::String& error)
    {
        tag = TimingTag {};

        const auto tokens = juce::StringArray::fromTokens (text, " ", "");

        for (int i = 1; i < tokens.size(); ++i)
        {
            const auto& token = tokens[i];
            const auto key = token.upToFirstOccurrenceOf ("=", false, false);
            juce::int64 number = 0;

            if (! token.containsChar ('=')
                  || ! LuthierEvents::parseInt (token.fromFirstOccurrenceOf ("=", false, false), number))
            {
                error = "LUTHIER-AT field \"" + token.substring (0, 40) + "\" is malformed";
                return false;
            }

            if (key == "dt")
            {
                tag.dt = number;
            }
            else if (key == "part")
            {
                if (number < 0 || number > 15)
                {
                    error = "LUTHIER-AT part " + juce::String (number) + " is not 0 to 15";
                    return false;
                }

                tag.part = (int) number;
            }
            else if (key == "n")
            {
                if (number < 0)
                {
                    error = "LUTHIER-AT order " + juce::String (number) + " is negative";
                    return false;
                }

                tag.n = number;
            }
        }

        return true;
    }

    bool parseBeginMarker (const juce::String& text, juce::String& className, int& schema, juce::String& error)
    {
        const auto tokens = juce::StringArray::fromTokens (
            text.substring (juce::String (LuthierEvents::kBeginMarker).length()), " ", "");

        juce::int64 version = 0;

        if (tokens.size() != 2 || ! LuthierEvents::isValidClassName (tokens[0])
              || ! LuthierEvents::parseInt (tokens[1], version) || version < 1 || version > 9999)
        {
            error = "\"" + text.substring (0, 60) + "\" is not a LUTHIER-BEGIN marker";
            return false;
        }

        className = tokens[0];
        schema = (int) version;
        return true;
    }

    /** One LUTHIER-BEGIN ... LUTHIER-END group as it is read. */
    struct OpenEvent
    {
        bool open = false;
        juce::String className;
        int schema = 1;
        juce::int64 tick = 0;
        size_t offset = 0;

        bool hasText = false;
        juce::String text;
        size_t textOffset = 0;

        bool hasSysEx = false;
        size_t sysExOffset = 0;      ///< the data after F0, without the closing F7
        size_t sysExSize = 0;
        size_t sysExEventOffset = 0;
    };

    bool finishEvent (const OpenEvent& open, const juce::uint8* data, const FileTempoMap& tempo,
                      bool exactTiming, LuthierEvent& event, juce::String& error, juce::int64& errorAt)
    {
        auto fail = [&error, &errorAt] (size_t offset, const juce::String& why)
        {
            error = byteRef (offset) + ": " + why;
            errorAt = (juce::int64) offset;
            return false;
        };

        if (! open.hasText && ! open.hasSysEx)
            return fail (open.offset, "LUTHIER-BEGIN " + open.className + " holds neither a text nor a SysEx copy");

        const bool knownClass = LuthierEvents::getClassForName (open.className) != LuthierEventClass::unknown;

        LuthierEvent fromSysEx, fromText;
        juce::int64 dtSysEx = 0, dtText = 0;
        juce::String why;
        bool haveSysEx = false, haveText = false;

        if (open.hasSysEx)
        {
            const auto* bytes = data + open.sysExOffset;
            const int size = (int) open.sysExSize;

            // A checksum that does not match is damage, whatever the class, and
            // a damaged event is refused rather than guessed at.
            if (! LuthierEvents::hasValidChecksum (bytes, size))
                return fail (open.sysExEventOffset, "the " + open.className + " event's SysEx checksum does not match");

            haveSysEx = LuthierEvents::decodeSysEx (bytes, size, fromSysEx, dtSysEx, why);

            if (! haveSysEx && knownClass)
                return fail (open.sysExEventOffset, why);
        }

        if (open.hasText)
        {
            haveText = LuthierEvents::decodeText (open.text, open.schema, fromText, dtText, why);

            if (! haveText && knownClass)
                return fail (open.textOffset, why);
        }

        // 2.3: both copies are the same event. If they are not, one is damaged
        // and there is no telling which.
        if (haveSysEx && haveText && (! fromText.hasSameContent (fromSysEx) || dtText != dtSysEx))
            return fail (open.textOffset, "the text and SysEx copies of a " + open.className + " event disagree");

        if (haveSysEx || haveText)
        {
            event = haveSysEx ? fromSysEx : fromText;
            const auto dt = haveSysEx ? dtSysEx : dtText;

            if (event.className != open.className || event.schemaVersion != open.schema)
                return fail (open.offset, "LUTHIER-BEGIN says " + open.className + " " + juce::String (open.schema)
                                            + " but the event is " + event.className + " "
                                            + juce::String (event.schemaVersion));

            event.sample = juce::jmax ((juce::int64) 0, tempo.tickToSample (open.tick) + (exactTiming ? dt : 0));
            return true;
        }

        // A later version's class that is not tagged text: its bytes are kept
        // and written back as they came (2.2). Its timing is its tick.
        event = LuthierEvent {};
        event.eventClass = LuthierEventClass::unknown;
        event.className = open.className;
        event.schemaVersion = open.schema;
        event.sample = tempo.tickToSample (open.tick);

        if (open.hasText)
            event.opaqueText = open.text;

        if (open.hasSysEx)
            event.opaqueSysEx.append (data + open.sysExOffset, open.sysExSize);

        return true;
    }

    //==========================================================================
    /*  Parsing one MTrk chunk. Every length is checked against the bytes that
        are really there before it is used, and every failure names its byte. */
    bool parseTrack (const juce::uint8* data, size_t start, size_t end, MidiProfiles::SmfTrack& track,
                     size_t& totalEvents, juce::String& error, juce::int64& errorByteOffset)
    {
        auto fail = [&error, &errorByteOffset] (size_t offset, const juce::String& why)
        {
            error = byteRef (offset) + ": " + why;
            errorByteOffset = (juce::int64) offset;
            return false;
        };

        size_t position = start;
        juce::int64 tick = 0;
        juce::uint8 running = 0;
        bool ended = false;

        while (position < end)
        {
            if (ended)
                return fail (position, "there is data after the end-of-track event");

            if (++totalEvents > kMaxEvents)
                return fail (position, "the file holds more than " + juce::String ((juce::int64) kMaxEvents) + " events");

            MidiProfiles::SmfEvent event;
            event.offset = position;

            juce::uint32 delta = 0;

            if (! readVarLen (data, position, end, delta))
                return fail (event.offset, "a delta-time is malformed or cut short");

            tick += (juce::int64) delta;
            event.tick = tick;

            if (position >= end)
                return fail (position, "a delta-time has no event after it");

            const auto first = data[position];

            if (first == 0xFF)
            {
                if (end - position < 2)
                    return fail (position, "a meta event is cut short");

                const auto type = data[position + 1];

                if (type >= 0x80)
                    return fail (position + 1, "a meta event's type is above 0x7F");

                position += 2;

                const auto lengthAt = position;
                juce::uint32 length = 0;

                if (! readVarLen (data, position, end, length) || (size_t) length > end - position)
                    return fail (lengthAt, "a meta event's length runs past the end of its track");

                event.status = 0xFF;
                event.metaType = type;
                event.dataOffset = position;
                event.dataSize = length;
                position += length;
                running = 0;

                if (type == 0x2F)
                {
                    if (length != 0)
                        return fail (lengthAt, "the end-of-track event has a length");

                    ended = true;
                }
            }
            else if (first == 0xF0 || first == 0xF7)
            {
                ++position;

                const auto lengthAt = position;
                juce::uint32 length = 0;

                if (! readVarLen (data, position, end, length) || (size_t) length > end - position)
                    return fail (lengthAt, "a SysEx length runs past the end of its track");

                // Inside a SysEx every byte but a closing F7 is seven-bit.
                for (size_t i = 0; i < (size_t) length; ++i)
                {
                    const auto byte = data[position + i];

                    if (byte >= 0x80 && ! (byte == 0xF7 && i + 1 == (size_t) length))
                        return fail (position + i, "a SysEx data byte is above 0x7F");
                }

                event.status = first;
                event.dataOffset = position;
                event.dataSize = length;
                position += length;
                running = 0;
            }
            else
            {
                juce::uint8 status = first;

                if (first >= 0x80)
                {
                    if (first >= 0xF0)
                        return fail (position, "a system message cannot appear in a MIDI file");

                    ++position;
                    running = first;
                }
                else
                {
                    if (running == 0)
                        return fail (position, "a data byte has no status byte before it");

                    status = running;
                }

                const auto numData = (size_t) dataBytesFor (status);

                if (end - position < numData)
                    return fail (position, "a channel message is cut short");

                for (size_t i = 0; i < numData; ++i)
                    if (data[position + i] >= 0x80)
                        return fail (position + i, "a channel message's data byte is above 0x7F");

                event.status = status;
                event.data1 = data[position];
                event.data2 = numData > 1 ? data[position + 1] : (juce::uint8) 0;
                position += numData;
            }

            event.length = position - event.offset;
            track.events.push_back (event);
        }

        if (! ended)
            return fail (end > start ? end - 1 : start, "the track has no end-of-track event");

        return true;
    }

    //==========================================================================
    struct PlannedTrack
    {
        juce::String name;
        std::vector<size_t> messages;   ///< indices into the performance's messages
        std::vector<size_t> events;     ///< indices into the performance's events
    };

    std::vector<PlannedTrack> planTracks (const MidiPerformance& performance, MidiTrackSplit split,
                                          const std::vector<size_t>& written)
    {
        const auto& messages = performance.getMessages();
        const auto& events = performance.getEvents();
        const auto& meta = performance.getMeta();

        auto singleTrack = [&]
        {
            PlannedTrack all;
            all.name = meta.title.isNotEmpty() ? meta.title : juce::String ("Guitar");

            for (size_t i = 0; i < messages.size(); ++i)
                all.messages.push_back (i);

            all.events = written;
            return std::vector<PlannedTrack> { all };
        };

        std::map<int, PlannedTrack> byKey;

        switch (split)
        {
            case MidiTrackSplit::perInstrument:
            {
                for (size_t i = 0; i < messages.size(); ++i)
                    byKey[messages[i].part].messages.push_back (i);

                for (const auto i : written)
                    byKey[events[i].part].events.push_back (i);

                for (auto& [part, track] : byKey)
                    track.name = part < meta.partNames.size() ? meta.partNames[part]
                                                               : "Part " + juce::String (part + 1);
                break;
            }

            case MidiTrackSplit::perString:
            {
                // 1: one channel per string, so one track per channel, as
                // NotationExporter lays a guitar out.
                for (size_t i = 0; i < messages.size(); ++i)
                    byKey[messages[i].message.getChannel()].messages.push_back (i);

                for (const auto i : written)
                {
                    const int channel = events[i].has ("ch") ? (int) events[i].getInt ("ch") : -1;
                    const int key = byKey.count (channel) > 0 ? channel
                                  : (byKey.empty() ? 1 : byKey.begin()->first);
                    byKey[key].events.push_back (i);
                }

                for (auto& [channel, track] : byKey)
                    track.name = "String " + juce::String (channel);
                break;
            }

            case MidiTrackSplit::perSection:
            {
                std::vector<std::pair<juce::int64, juce::String>> sections;

                for (const auto& event : events)
                    if (event.eventClass == LuthierEventClass::section && event.get ("edge") == "start")
                        sections.emplace_back (event.sample, event.get ("name"));

                if (sections.empty())
                    return singleTrack();

                auto sectionAt = [&sections] (juce::int64 sample)
                {
                    int index = -1;

                    for (size_t s = 0; s < sections.size() && sections[s].first <= sample; ++s)
                        index = (int) s;

                    return index;
                };

                // A note-off goes with its note-on, or a host would see a note
                // that never ends in one track and an orphan release in the next.
                std::array<std::array<int, 128>, 16> noteTrack;

                for (auto& row : noteTrack)
                    row.fill (-2);

                for (size_t i = 0; i < messages.size(); ++i)
                {
                    const auto& message = messages[i].message;
                    const auto channel = (size_t) juce::jlimit (0, 15, message.getChannel() - 1);
                    int key = sectionAt (messages[i].sample);

                    if (message.isNoteOn())
                    {
                        noteTrack[channel][(size_t) message.getNoteNumber()] = key;
                    }
                    else if (message.isNoteOff())
                    {
                        auto& owner = noteTrack[channel][(size_t) message.getNoteNumber()];

                        if (owner != -2)
                        {
                            key = owner;
                            owner = -2;
                        }
                    }

                    byKey[key].messages.push_back (i);
                }

                for (const auto i : written)
                    byKey[sectionAt (events[i].sample)].events.push_back (i);

                for (auto& [index, track] : byKey)
                {
                    const auto name = index >= 0 ? sections[(size_t) index].second : juce::String();
                    track.name = index < 0 ? juce::String ("Start")
                               : name.isNotEmpty() ? name
                               : "Section " + juce::String (index + 1);
                }

                break;
            }

            case MidiTrackSplit::single:
            default:
                return singleTrack();
        }

        std::vector<PlannedTrack> tracks;

        for (auto& [key, track] : byKey)
            if (! track.messages.empty() || ! track.events.empty())
                tracks.push_back (std::move (track));

        return tracks.empty() ? singleTrack() : tracks;
    }

    //==========================================================================
    juce::String headerFields (const MidiPerformance& performance, const MidiExportOptions& options,
                               int ppq, size_t numMessages, size_t numEvents)
    {
        const auto& meta = performance.getMeta();

        juce::StringArray fields;
        fields.add ("profile=luthier");
        fields.add ("sr=" + LuthierEvents::formatReal (performance.getSampleRate()));
        fields.add ("ppq=" + juce::String (ppq));
        fields.add ("split=" + juce::String (MidiProfiles::getSplitName (options.split)));
        fields.add ("sysex=" + juce::String (options.sysExRedundancy ? 1 : 0));
        fields.add ("messages=" + juce::String ((juce::int64) numMessages));
        fields.add ("events=" + juce::String ((juce::int64) numEvents));
        fields.add ("bend=" + LuthierEvents::formatReal (meta.pitchBendRangeSemitones));

        juce::StringArray parts;

        for (const auto& name : meta.partNames)
            parts.add (LuthierEvents::escapeValue (name).replace (",", "%2C"));

        fields.add ("parts=" + parts.joinIntoString (","));

        // midi-export 11: included for round-trip fidelity unless stripped.
        if (! options.stripIdentifiers)
        {
            if (meta.guitarName.isNotEmpty())
                fields.add ("guitar=" + LuthierEvents::escapeValue (meta.guitarName));

            if (meta.presetName.isNotEmpty())
                fields.add ("preset=" + LuthierEvents::escapeValue (meta.presetName));

            if (meta.characterSeed.isNotEmpty())
                fields.add ("seed=" + LuthierEvents::escapeValue (meta.characterSeed));
        }

        return fields.joinIntoString (" ");
    }

    void addAt (juce::MidiMessageSequence& sequence, juce::MidiMessage message, juce::int64 tick)
    {
        message.setTimeStamp ((double) tick);
        sequence.addEvent (message);
    }

    juce::MidiMessageSequence buildMetaTrack (const MidiPerformance& performance, const MidiExportOptions& options,
                                              const FileTempoMap& tempo, int ppq, bool luthier,
                                              size_t numMessages, size_t numEvents)
    {
        juce::MidiMessageSequence track;
        const auto& meta = performance.getMeta();

        // 2: the header is the meta track's first message.
        if (luthier)
            addAt (track, makeHeader (headerFields (performance, options, ppq, numMessages, numEvents)), 0);

        if (meta.title.isNotEmpty())
            addAt (track, juce::MidiMessage::textMetaEvent (3, meta.title), 0);

        if (meta.copyright.isNotEmpty())
            addAt (track, juce::MidiMessage::textMetaEvent (2, meta.copyright), 0);

        addAt (track, juce::MidiMessage::keySignatureMetaEvent (juce::jlimit (-7, 7, meta.keySharpsOrFlats),
                                                                meta.keyIsMinor), 0);

        for (const auto& signature : performance.getTimeSignatures())
            addAt (track, juce::MidiMessage::timeSignatureMetaEvent (signature.numerator, signature.denominator),
                   (juce::int64) std::llround (signature.beat * (double) ppq));

        for (const auto& segment : tempo.getSegments())
            addAt (track, juce::MidiMessage::tempoMetaEvent (segment.microsecondsPerQuarter), segment.tick);

        return track;
    }

    void writeExtensionEvent (juce::MidiMessageSequence& track, const LuthierEvent& event,
                              const MidiExportOptions& options, const FileTempoMap& tempo, bool luthier)
    {
        const auto tick = tempo.sampleToTick (event.sample);

        if (! luthier)
        {
            // 3: realism for a person to read. No timing to the sample - the
            // Generic profile has none - and never an identifier (11).
            LuthierEvents::WriteOptions text;
            text.writeTiming = false;
            text.stripIdentifiers = true;

            addAt (track, makeText (LuthierEvents::encodeText (event, text)), tick);
            return;
        }

        LuthierEvents::WriteOptions wire;
        wire.sampleCorrection = event.sample - tempo.tickToSample (tick);
        wire.stripIdentifiers = options.stripIdentifiers;

        addAt (track, makeText (juce::String (LuthierEvents::kBeginMarker) + event.className + " "
                                  + juce::String (event.schemaVersion)), tick);

        if (event.isOpaque())
        {
            if (event.opaqueText.isNotEmpty())
                addAt (track, makeText (event.opaqueText), tick);

            // An unknown class's SysEx is the only copy when it came without
            // text, so it is kept even with SysEx redundancy off.
            if (event.opaqueSysEx.getSize() > 0 && (options.sysExRedundancy || event.opaqueText.isEmpty()))
                addAt (track, juce::MidiMessage::createSysExMessage (event.opaqueSysEx.getData(),
                                                                     (int) event.opaqueSysEx.getSize()), tick);
        }
        else
        {
            addAt (track, makeText (LuthierEvents::encodeText (event, wire)), tick);

            if (options.sysExRedundancy)
                addAt (track, LuthierEvents::encodeSysEx (event, wire), tick);
        }

        addAt (track, makeText (LuthierEvents::kEndMarker), tick);
    }

    juce::MidiMessageSequence buildEventTrack (const MidiPerformance& performance, const PlannedTrack& plan,
                                               const MidiExportOptions& options, const FileTempoMap& tempo,
                                               bool luthier, bool multiTrack)
    {
        juce::MidiMessageSequence track;

        const auto& messages = performance.getMessages();
        const auto& events = performance.getEvents();

        addAt (track, juce::MidiMessage::textMetaEvent (3, plan.name), 0);

        // 3: the pitch-bend range, at the start of the track, on every channel
        // it uses: RPN 0 through CC 101 / 100, the value through CC 6 / 38,
        // then the null RPN so a stray data entry cannot change it.
        std::array<bool, 16> used {};

        for (const auto index : plan.messages)
            used[(size_t) juce::jlimit (0, 15, messages[index].message.getChannel() - 1)] = true;

        const double range = juce::jlimit (0.0, 127.99, performance.getMeta().pitchBendRangeSemitones);
        const int semitones = (int) std::floor (range);
        const int cents = juce::jlimit (0, 99, juce::roundToInt ((range - (double) semitones) * 100.0));

        for (int channel = 1; channel <= 16; ++channel)
        {
            if (! used[(size_t) channel - 1])
                continue;

            addAt (track, juce::MidiMessage::controllerEvent (channel, 101, 0), 0);
            addAt (track, juce::MidiMessage::controllerEvent (channel, 100, 0), 0);
            addAt (track, juce::MidiMessage::controllerEvent (channel, 6, semitones), 0);
            addAt (track, juce::MidiMessage::controllerEvent (channel, 38, cents), 0);
            addAt (track, juce::MidiMessage::controllerEvent (channel, 101, 127), 0);
            addAt (track, juce::MidiMessage::controllerEvent (channel, 100, 127), 0);
        }

        // Messages and events merged by sample, messages first at a tie. Ticks
        // never run backwards as samples advance, so the sequence stays in this
        // order and every LUTHIER-AT tag sits directly before its message.
        size_t m = 0, e = 0;

        while (m < plan.messages.size() || e < plan.events.size())
        {
            const bool takeMessage = e >= plan.events.size()
                                       || (m < plan.messages.size()
                                            && messages[plan.messages[m]].sample <= events[plan.events[e]].sample);

            if (! takeMessage)
            {
                writeExtensionEvent (track, events[plan.events[e++]], options, tempo, luthier);
                continue;
            }

            const auto index = plan.messages[m++];
            const auto& entry = messages[index];
            const auto tick = tempo.sampleToTick (entry.sample);

            if (luthier)
            {
                const auto dt = entry.sample - tempo.tickToSample (tick);

                juce::String tag;

                if (dt != 0)
                    tag << " dt=" << juce::String (dt);

                if (entry.part != 0)
                    tag << " part=" << juce::String (entry.part);

                if (multiTrack)
                    tag << " n=" << juce::String ((juce::int64) index);

                if (tag.isNotEmpty())
                    addAt (track, makeText (juce::String (LuthierEvents::kTimingTag) + tag), tick);
            }

            addAt (track, entry.message, tick);
        }

        return track;
    }
}

//==============================================================================
bool MidiExportOptions::includesClass (LuthierEventClass eventClass) const noexcept
{
    if (eventClass == LuthierEventClass::unknown)
        return true;

    return (classMask & (juce::uint32) (1u << (unsigned int) eventClass)) != 0;
}

void MidiExportOptions::setClassIncluded (LuthierEventClass eventClass, bool shouldInclude) noexcept
{
    if (eventClass == LuthierEventClass::unknown)
        return;

    const auto bit = (juce::uint32) (1u << (unsigned int) eventClass);
    classMask = shouldInclude ? (classMask | bit) : (classMask & ~bit);
}

//==============================================================================
namespace MidiProfiles
{

const char* getProfileName (MidiProfile profile) noexcept
{
    return profile == MidiProfile::generic ? "generic" : "luthier";
}

const char* getSplitName (MidiTrackSplit split) noexcept
{
    switch (split)
    {
        case MidiTrackSplit::perSection:    return "section";
        case MidiTrackSplit::perInstrument: return "instrument";
        case MidiTrackSplit::perString:     return "string";
        case MidiTrackSplit::single:
        default:                            return "single";
    }
}

bool profileFromName (const juce::String& name, MidiProfile& profile) noexcept
{
    if (name.equalsIgnoreCase ("luthier"))  { profile = MidiProfile::luthier; return true; }
    if (name.equalsIgnoreCase ("generic"))  { profile = MidiProfile::generic; return true; }
    return false;
}

bool splitFromName (const juce::String& name, MidiTrackSplit& split) noexcept
{
    for (auto candidate : { MidiTrackSplit::single, MidiTrackSplit::perSection,
                            MidiTrackSplit::perInstrument, MidiTrackSplit::perString })
    {
        if (name.equalsIgnoreCase (getSplitName (candidate)))
        {
            split = candidate;
            return true;
        }
    }

    return false;
}

//==============================================================================
juce::MidiMessage SmfEvent::toMessage (const juce::uint8* file) const
{
    if (isChannel())
        return dataBytesFor (status) == 1 ? juce::MidiMessage ((int) status, (int) data1)
                                          : juce::MidiMessage ((int) status, (int) data1, (int) data2);

    if (isMeta())
    {
        std::vector<juce::uint8> bytes { 0xFF, (juce::uint8) metaType };
        appendVarLen (bytes, (juce::uint32) dataSize);
        bytes.insert (bytes.end(), file + dataOffset, file + dataOffset + dataSize);
        return juce::MidiMessage (bytes.data(), (int) bytes.size());
    }

    // JUCE holds a SysEx as F0 <data> F7, without the file's length.
    const bool closed = dataSize > 0 && file[dataOffset + dataSize - 1] == 0xF7;
    return juce::MidiMessage::createSysExMessage (file + dataOffset, (int) (closed ? dataSize - 1 : dataSize));
}

bool parseSmf (const void* rawData, size_t numBytes, SmfFile& result,
               juce::String& error, juce::int64& errorByteOffset)
{
    const auto* data = static_cast<const juce::uint8*> (rawData);

    auto fail = [&error, &errorByteOffset] (size_t offset, const juce::String& why)
    {
        error = byteRef (offset) + ": " + why;
        errorByteOffset = (juce::int64) offset;
        return false;
    };

    result = SmfFile {};

    if (data == nullptr || numBytes < 14)
        return fail (0, "the file is too short to be MIDI");

    if (std::memcmp (data, "MThd", 4) != 0)
        return fail (0, "this is not a standard MIDI file (it does not start with MThd)");

    const auto headerLength = (size_t) readBigEndian32 (data + 4);

    if (headerLength < 6 || headerLength > numBytes - 8)
        return fail (4, "the MThd header length is wrong");

    const int format = readBigEndian16 (data + 8);
    const int declaredTracks = readBigEndian16 (data + 10);
    const int division = readBigEndian16 (data + 12);

    if (format > 1)
        return fail (8, "format " + juce::String (format) + " MIDI files are not supported");

    if ((division & 0x8000) != 0)
        return fail (12, "SMPTE timing is not supported; the file needs ticks per quarter note");

    if (division == 0)
        return fail (12, "the file has zero ticks per quarter note");

    result.format = format;
    result.ppq = division;

    size_t position = 8 + headerLength;
    size_t totalEvents = 0;

    while (position < numBytes)
    {
        if (numBytes - position < 8)
            return fail (position, "a chunk header is cut short");

        const auto chunkLength = (size_t) readBigEndian32 (data + position + 4);

        if (chunkLength > numBytes - position - 8)
            return fail (position + 4, "a chunk's length runs past the end of the file");

        if (std::memcmp (data + position, "MTrk", 4) == 0)
        {
            SmfTrack track;
            track.chunkOffset = position;
            track.chunkLength = chunkLength;

            if (! parseTrack (data, position + 8, position + 8 + chunkLength, track, totalEvents,
                              error, errorByteOffset))
                return false;

            result.tracks.push_back (std::move (track));
        }

        // Any other chunk is skipped, as the standard tells a reader to.
        position += 8 + chunkLength;
    }

    if ((int) result.tracks.size() != declaredTracks)
        return fail (10, "the header promises " + juce::String (declaredTracks) + " tracks but the file holds "
                           + juce::String ((int) result.tracks.size()));

    if (result.tracks.empty())
        return fail (10, "the file holds no tracks");

    return true;
}

//==============================================================================
juce::MemoryBlock exportToMemory (const MidiPerformance& source, const MidiExportOptions& options)
{
    MidiPerformance extracted;
    const MidiPerformance* chosen = &source;

    if (! options.range.isEmpty())
    {
        extracted = source.extractRange (options.range);
        chosen = &extracted;
    }

    const auto& performance = *chosen;

    const bool luthier = options.profile == MidiProfile::luthier;
    const int ppq = options.getPpq();
    const auto tempo = makeFileTempoMap (performance, ppq);

    // Which extension events go in: the profile's classes, and in Generic only
    // with realism on and only those that can be described (3, 4.1).
    std::vector<size_t> written;
    const auto& events = performance.getEvents();

    for (size_t i = 0; i < events.size(); ++i)
    {
        const auto& event = events[i];

        if (! options.includesClass (event.eventClass))
            continue;

        if (! luthier && (! options.includeRealism || event.isOpaque()))
            continue;

        written.push_back (i);
    }

    const auto plan = planTracks (performance, options.split, written);
    const bool multiTrack = plan.size() > 1;

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ppq);

    file.addTrack (buildMetaTrack (performance, options, tempo, ppq, luthier,
                                   performance.getMessages().size(), written.size()));

    for (const auto& track : plan)
        file.addTrack (buildEventTrack (performance, track, options, tempo, luthier, multiTrack));

    juce::MemoryOutputStream out;
    file.writeTo (out, 1);
    return out.getMemoryBlock();
}

bool exportToFile (const MidiPerformance& performance, const MidiExportOptions& options,
                   const juce::File& destination, juce::String* error)
{
    const auto bytes = exportToMemory (performance, options);

    destination.getParentDirectory().createDirectory();

    juce::TemporaryFile temp (destination);

    if (! temp.getFile().replaceWithData (bytes.getData(), bytes.getSize())
          || ! temp.overwriteTargetFileWithTemporary())
    {
        if (error != nullptr)
            *error = "Could not write " + destination.getFullPathName();

        return false;
    }

    return true;
}

//==============================================================================
MidiImportResult importFromMemory (const void* rawData, size_t numBytes,
                                   MidiPerformance& destination, double sampleRate)
{
    MidiImportResult result;
    const auto* data = static_cast<const juce::uint8*> (rawData);

    auto refuse = [&result] (const juce::String& why, juce::int64 offset)
    {
        result.ok = false;
        result.error = why;
        result.errorByteOffset = offset;
        return result;
    };

    SmfFile smf;
    juce::String error;
    juce::int64 errorAt = -1;

    if (! parseSmf (rawData, numBytes, smf, error, errorAt))
        return refuse (error, errorAt);

    result.ppq = smf.ppq;
    result.numTracks = (int) smf.tracks.size();

    const double rate = (std::isfinite (sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;

    // ---- 5: the header decides the profile ------------------------------------------
    LuthierHeader header;
    bool luthier = false;
    size_t headerOffset = 0;

    if (! smf.tracks.front().events.empty() && isHeader (smf.tracks.front().events.front(), data))
    {
        const auto& event = smf.tracks.front().events.front();
        headerOffset = event.offset;

        if (! parseHeader (data + event.dataOffset, event.dataSize, header, error))
            return refuse (byteRef (event.offset) + ": " + error, (juce::int64) event.offset);

        luthier = true;
    }

    result.detectedProfile = luthier ? MidiProfile::luthier : MidiProfile::generic;

    // ---- the tempo map, from every track ------------------------------------------------
    FileTempoMap tempo (smf.ppq, rate);

    for (const auto& track : smf.tracks)
    {
        for (const auto& event : track.events)
        {
            if (! event.isMeta() || event.metaType != 0x51)
                continue;

            if (event.dataSize != 3)
                return refuse (byteRef (event.offset) + ": a tempo event must hold three bytes",
                               (juce::int64) event.offset);

            const auto* bytes = data + event.dataOffset;
            const int microseconds = ((int) bytes[0] << 16) | ((int) bytes[1] << 8) | (int) bytes[2];

            if (microseconds == 0)
                return refuse (byteRef (event.offset) + ": a tempo event says zero microseconds per beat",
                               (juce::int64) event.offset);

            tempo.add (event.tick, microseconds);
        }
    }

    tempo.finalise();

    MidiPerformance performance (rate);

    {
        const auto& segments = tempo.getSegments();
        performance.setTempo (60000000.0 / (double) segments.front().microsecondsPerQuarter);

        for (size_t i = 1; i < segments.size(); ++i)
            performance.addTempoChange ((double) segments[i].tick / (double) smf.ppq,
                                        60000000.0 / (double) segments[i].microsecondsPerQuarter);
    }

    // dt is samples at the rate the file was written at; at any other rate the
    // tick is the best there is (0.4: beat-accurate in ticks).
    const bool exactTiming = luthier && std::abs (header.sampleRate - rate) < 1.0e-6;

    if (luthier && ! exactTiming)
        result.warnings.add ("This file was written at " + LuthierEvents::formatReal (header.sampleRate)
                               + " Hz; at " + LuthierEvents::formatReal (rate)
                               + " Hz its timing is to the nearest tick rather than the sample.");

    // ---- track 0: title, copyright, metre and key --------------------------------------
    auto& meta = performance.getMeta();
    bool haveTitle = false, haveSignature = false;

    for (const auto& event : smf.tracks.front().events)
    {
        if (! event.isMeta())
            continue;

        if (event.metaType == 0x03 && ! haveTitle)
        {
            meta.title = readText (data, event);
            haveTitle = true;
        }
        else if (event.metaType == 0x02)
        {
            meta.copyright = readText (data, event);
        }
        else if (event.metaType == 0x58 && event.dataSize >= 2)
        {
            const int numerator = data[event.dataOffset];
            const int denominator = 1 << juce::jmin (5, (int) data[event.dataOffset + 1]);
            const double beat = (double) event.tick / (double) smf.ppq;

            if (! haveSignature && event.tick == 0)
                performance.setTimeSignature (numerator, denominator);
            else
                performance.addTimeSignature (beat, numerator, denominator);

            haveSignature = true;
        }
        else if (event.metaType == 0x59 && event.dataSize >= 2)
        {
            meta.keySharpsOrFlats = juce::jlimit (-7, 7, (int) (juce::int8) data[event.dataOffset]);
            meta.keyIsMinor = data[event.dataOffset + 1] != 0;
        }
    }

    if (luthier)
    {
        meta.guitarName = header.guitarName;
        meta.presetName = header.presetName;
        meta.characterSeed = header.characterSeed;
        meta.pitchBendRangeSemitones = header.bendRange;

        if (! header.partNames.isEmpty())
            meta.partNames = header.partNames;
    }

    // ---- every track's channel messages and extension events ---------------------------
    struct Gathered
    {
        juce::int64 sample;
        juce::MidiMessage message;
        int part;
        juce::int64 order;
    };

    std::vector<Gathered> gathered;
    bool everyMessageOrdered = true;
    juce::int64 eventCount = 0;

    // The RPN block export writes at the top of a track: CC 101 0, 100 0,
    // 6 n, 38 n, 101 127, 100 127. It is the file's, not the performance's.
    static constexpr int rpnController[] = { 101, 100, 6, 38, 101, 100 };
    static constexpr int rpnValue[] = { 0, 0, -1, -1, 127, 127 };

    for (const auto& track : smf.tracks)
    {
        struct RpnState
        {
            int stage = 0;
            bool done = false;
            int semitones = 0, cents = 0;
            std::vector<const SmfEvent*> held;
        };

        std::array<RpnState, 16> rpn;
        TimingTag tag;
        OpenEvent open;
        int trackPart = 0;

        auto gather = [&] (const SmfEvent& event, bool useTag)
        {
            auto sample = tempo.tickToSample (event.tick);
            int part = luthier ? 0 : trackPart;
            juce::int64 order = -1;

            if (luthier && useTag && tag.pending)
            {
                if (exactTiming)
                    sample += tag.dt;

                if (tag.part >= 0)
                    part = tag.part;

                order = tag.n;
            }

            if (useTag)
                tag = TimingTag {};

            if (order < 0)
                everyMessageOrdered = false;

            gathered.push_back ({ juce::jmax ((juce::int64) 0, sample), event.toMessage (data), part, order });
        };

        for (const auto& event : track.events)
        {
            if (event.isChannel())
            {
                auto& state = rpn[(size_t) (event.status & 0x0F)];

                if (! state.done)
                {
                    const auto stage = (size_t) state.stage;
                    const bool matches = event.tick == 0 && (event.status & 0xF0) == 0xB0
                                           && (int) event.data1 == rpnController[stage]
                                           && (rpnValue[stage] < 0 || (int) event.data2 == rpnValue[stage]);

                    if (matches)
                    {
                        if (stage == 2) state.semitones = event.data2;
                        if (stage == 3) state.cents = event.data2;

                        state.held.push_back (&event);

                        if (++state.stage == 6)
                        {
                            state.done = true;
                            state.held.clear();

                            if (! luthier)
                                meta.pitchBendRangeSemitones = state.semitones + state.cents / 100.0;
                        }

                        continue;
                    }

                    // Not the block after all: what was held back was music.
                    state.done = true;

                    for (const auto* held : state.held)
                        gather (*held, false);

                    state.held.clear();
                }

                gather (event, true);
                continue;
            }

            if (! event.isMeta() && ! event.isSysEx())
                continue;

            if (event.isMeta() && event.metaType == 0x03)
            {
                // Generic: a track named for the bass is the bass (4.1).
                if (! luthier && readText (data, event).startsWithIgnoreCase ("bass"))
                    trackPart = 1;

                continue;
            }

            if (event.isMeta() && event.metaType == 0x01)
            {
                const auto text = readText (data, event);

                if (! luthier)
                {
                    if (text.startsWith (LuthierEvents::kTextPrefix))
                        ++result.ignoredRealismTexts;

                    continue;
                }

                if (isTimingTag (text))
                {
                    if (tag.pending)
                        return refuse (byteRef (tag.offset) + ": a LUTHIER-AT tag is not followed by a message",
                                       (juce::int64) tag.offset);

                    if (! parseTimingTag (text, tag, error))
                        return refuse (byteRef (event.offset) + ": " + error, (juce::int64) event.offset);

                    tag.pending = true;
                    tag.offset = event.offset;
                    continue;
                }

                if (text.startsWith (LuthierEvents::kBeginMarker))
                {
                    if (open.open)
                        return refuse (byteRef (event.offset) + ": LUTHIER-BEGIN inside the event opened at "
                                         + byteRef (open.offset), (juce::int64) event.offset);

                    open = OpenEvent {};

                    if (! parseBeginMarker (text, open.className, open.schema, error))
                        return refuse (byteRef (event.offset) + ": " + error, (juce::int64) event.offset);

                    open.open = true;
                    open.tick = event.tick;
                    open.offset = event.offset;
                    continue;
                }

                if (text == LuthierEvents::kEndMarker)
                {
                    if (! open.open)
                        return refuse (byteRef (event.offset) + ": LUTHIER-END with no LUTHIER-BEGIN",
                                       (juce::int64) event.offset);

                    LuthierEvent finished;

                    if (! finishEvent (open, data, tempo, exactTiming, finished, error, errorAt))
                        return refuse (error, errorAt);

                    performance.addEvent (finished);
                    ++eventCount;
                    open = OpenEvent {};
                    continue;
                }

                if (text.startsWith (LuthierEvents::kTextPrefix))
                {
                    if (! open.open)
                    {
                        ++result.ignoredRealismTexts;
                        continue;
                    }

                    if (open.hasText)
                        return refuse (byteRef (event.offset) + ": a " + open.className
                                         + " event has two text copies", (juce::int64) event.offset);

                    open.hasText = true;
                    open.text = text;
                    open.textOffset = event.offset;
                }

                continue;
            }

            if (event.isSysEx() && luthier && open.open)
            {
                const auto* bytes = data + event.dataOffset;
                const bool closed = event.dataSize > 0 && bytes[event.dataSize - 1] == 0xF7;
                const int size = (int) (closed ? event.dataSize - 1 : event.dataSize);

                if (! LuthierEvents::isLuthierSysEx (bytes, size))
                    continue;

                if (! closed)
                    return refuse (byteRef (event.offset) + ": an event SysEx is not closed with F7",
                                   (juce::int64) event.offset);

                if (open.hasSysEx)
                    return refuse (byteRef (event.offset) + ": a " + open.className
                                     + " event has two SysEx copies", (juce::int64) event.offset);

                open.hasSysEx = true;
                open.sysExOffset = event.dataOffset;
                open.sysExSize = (size_t) size;
                open.sysExEventOffset = event.offset;
            }
        }

        if (open.open)
            return refuse (byteRef (open.offset) + ": LUTHIER-BEGIN " + open.className + " has no LUTHIER-END",
                           (juce::int64) open.offset);

        if (tag.pending)
            return refuse (byteRef (tag.offset) + ": a LUTHIER-AT tag is not followed by a message",
                           (juce::int64) tag.offset);

        // An RPN block cut short at the end of a track was music too.
        for (auto& state : rpn)
            for (const auto* held : state.held)
                gather (*held, false);
    }

    // ---- the counts in the header are a last check that nothing went missing -------
    if (luthier)
    {
        if (header.messages >= 0 && header.messages != (juce::int64) gathered.size())
            return refuse (byteRef (headerOffset) + ": the header counts " + juce::String (header.messages)
                             + " messages but the file holds " + juce::String ((juce::int64) gathered.size()),
                           (juce::int64) headerOffset);

        if (header.events >= 0 && header.events != eventCount)
            return refuse (byteRef (headerOffset) + ": the header counts " + juce::String (header.events)
                             + " Luthier events but the file holds " + juce::String (eventCount),
                           (juce::int64) headerOffset);
    }

    // ---- the messages in the order they were played ----------------------------------
    // A split file tags every message with its place; otherwise the sample, with
    // ties kept in track then file order, is the order.
    if (luthier && everyMessageOrdered && ! gathered.empty())
        std::stable_sort (gathered.begin(), gathered.end(),
                          [] (const Gathered& a, const Gathered& b) { return a.order < b.order; });
    else
        std::stable_sort (gathered.begin(), gathered.end(),
                          [] (const Gathered& a, const Gathered& b) { return a.sample < b.sample; });

    for (const auto& entry : gathered)
        performance.addMessage (entry.sample, entry.message, entry.part);

    // ---- what the user should be told (9, 10) ------------------------------------------
    for (const auto& event : performance.getEvents())
    {
        if (event.eventClass == LuthierEventClass::unknown)
        {
            result.warnings.addIfNotAlreadyThere (event.className + " events come from a later version of"
                                                  " Luthier; they are kept and saved back unchanged.");
            continue;
        }

        const int supported = LuthierEvents::getSchemaVersion (event.eventClass);

        if (event.schemaVersion > supported)
            result.warnings.addIfNotAlreadyThere (event.className + " events use schema "
                                                  + juce::String (event.schemaVersion) + "; this version reads "
                                                  + juce::String (supported) + " and keeps the rest unchanged.");

        for (const auto& spec : LuthierEvents::getFields (event.eventClass))
            if (! event.has (spec.key))
                result.defaultedFields.addIfNotAlreadyThere (event.className + "." + spec.key);

        if (event.eventClass == LuthierEventClass::ranges && event.getInt ("on") != 0)
        {
            const double value = event.getReal ("value");

            if (value < event.getReal ("min") || value > event.getReal ("max"))
                result.warnings.addIfNotAlreadyThere ("Advanced range: " + event.get ("param") + " goes to "
                                                      + LuthierEvents::formatReal (value)
                                                      + ", outside its stock range. Check it before applying.");
        }
    }

    destination = std::move (performance);
    result.ok = true;
    return result;
}

MidiImportResult importFromFile (const juce::File& source, MidiPerformance& destination, double sampleRate)
{
    juce::MemoryBlock bytes;

    if (! source.existsAsFile() || ! source.loadFileAsData (bytes))
    {
        MidiImportResult result;
        result.error = "Could not read " + source.getFullPathName();
        return result;
    }

    return importFromMemory (bytes.getData(), bytes.getSize(), destination, sampleRate);
}

//==============================================================================
juce::File writeDragOutFile (const MidiPerformance& performance, bool forceGeneric,
                             const MidiExportOptions& defaults)
{
    auto options = defaults;
    options.profile = forceGeneric ? MidiProfile::generic : MidiProfile::luthier;

    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Luthier Drag");

    if (folder.createDirectory().failed())
        return {};

    const auto stem = MidiCapture::makeDefaultFileName().upToLastOccurrenceOf (".", false, false);
    const auto file = folder.getNonexistentChildFile (juce::File::createLegalFileName (stem), ".mid", false);

    return exportToFile (performance, options, file) ? file : juce::File();
}

juce::String describeOpeningBar (const MidiPerformance& source, const MidiExportOptions& options)
{
    MidiPerformance extracted;
    const MidiPerformance* chosen = &source;

    if (! options.range.isEmpty())
    {
        extracted = source.extractRange (options.range);
        chosen = &extracted;
    }

    const auto& performance = *chosen;

    juce::int64 start = 0;
    juce::String sectionName;

    for (const auto& event : performance.getEvents())
    {
        if (event.eventClass == LuthierEventClass::section && event.get ("edge") == "start")
        {
            start = event.sample;
            sectionName = event.get ("name");
            break;
        }
    }

    const double startBeat = performance.sampleToBeat ((double) start);
    const auto signature = performance.getTimeSignatureAt (startBeat);
    const double barBeats = (double) signature.numerator * 4.0 / (double) signature.denominator;
    const auto end = (juce::int64) std::llround (performance.beatToSample (startBeat + barBeats));

    juce::StringArray notes;
    int bends = 0, controllers = 0;

    for (const auto& entry : performance.getMessages())
    {
        if (entry.sample < start)
            continue;

        if (entry.sample >= end)
            break;

        if (entry.message.isNoteOn())
            notes.add (PerformanceScore::getNoteName (entry.message.getNoteNumber()));
        else if (entry.message.isPitchWheel())
            ++bends;
        else if (entry.message.isController())
            ++controllers;
    }

    std::array<int, LuthierEvents::kNumClasses + 1> counts {};

    for (const auto& event : performance.getEvents())
        if (event.sample >= start && event.sample < end)
            ++counts[(size_t) event.eventClass];

    juce::String text;
    text << "Bar 1";

    if (sectionName.isNotEmpty())
        text << " of " << sectionName;

    text << ": " << juce::String (signature.numerator) << "/" << juce::String (signature.denominator)
         << " at " << juce::String (performance.getTempoAt (startBeat), 1) << " bpm, "
         << juce::String (notes.size()) << (notes.size() == 1 ? " note" : " notes");

    if (! notes.isEmpty())
        text << " (" << notes.joinIntoString (" ") << ")";

    if (bends > 0)
        text << ", " << juce::String (bends) << " pitch bend";

    if (controllers > 0)
        text << ", " << juce::String (controllers) << (controllers == 1 ? " controller" : " controllers");

    const bool luthier = options.profile == MidiProfile::luthier;

    if (luthier || options.includeRealism)
    {
        juce::StringArray classes;

        for (int c = 0; c <= LuthierEvents::kNumClasses; ++c)
        {
            const auto eventClass = (LuthierEventClass) c;

            if (counts[(size_t) c] > 0 && options.includesClass (eventClass))
                classes.add (juce::String (LuthierEvents::getClassName (eventClass))
                               + " x" + juce::String (counts[(size_t) c]));
        }

        if (! classes.isEmpty())
            text << "; " << (luthier ? "Luthier events: " : "realism text: ") << classes.joinIntoString (", ");
    }

    return text;
}

//==============================================================================
juce::var profileToVar (const MidiExportOptions& options, const juce::String& name)
{
    auto* config = new juce::DynamicObject();
    config->setProperty ("profile", getProfileName (options.profile));
    config->setProperty ("ppq", options.getPpq());
    config->setProperty ("split", getSplitName (options.split));
    config->setProperty ("includeRealism", options.includeRealism);
    config->setProperty ("sysex", options.sysExRedundancy);
    config->setProperty ("stripIdentifiers", options.stripIdentifiers);

    juce::Array<juce::var> classes;

    for (int c = 0; c < LuthierEvents::kNumClasses; ++c)
        if (options.includesClass ((LuthierEventClass) c))
            classes.add (juce::var (LuthierEvents::getClassName ((LuthierEventClass) c)));

    config->setProperty ("classes", classes);

    auto* meta = new juce::DynamicObject();
    meta->setProperty ("name", name);
    meta->setProperty ("app", "Luthier");

    auto* root = new juce::DynamicObject();
    root->setProperty ("magic", kProfileMagic);
    root->setProperty ("schema", kProfileSchema);
    root->setProperty ("meta", juce::var (meta));
    root->setProperty ("config", juce::var (config));

    return juce::var (root);
}

bool profileFromVar (const juce::var& profile, MidiExportOptions& options, juce::String* name,
                     juce::String* error, juce::StringArray* warnings)
{
    auto fail = [error] (const juce::String& why)
    {
        if (error != nullptr)
            *error = why;

        return false;
    };

    auto warn = [warnings] (const juce::String& what)
    {
        if (warnings != nullptr)
            warnings->add (what);
    };

    if (! profile.isObject() || profile.getProperty ("magic", {}).toString() != kProfileMagic)
        return fail ("This is not a Luthier MIDI profile.");

    const int schema = (int) profile.getProperty ("schema", 0);

    if (schema < 1)
        return fail ("The MIDI profile has no schema version.");

    if (schema > kProfileSchema)
        warn ("This MIDI profile comes from a later version of Luthier; settings this version lacks are ignored.");

    const auto config = profile.getProperty ("config", {});

    if (! config.isObject())
        return fail ("The MIDI profile has no config.");

    MidiExportOptions result;

    if (config.hasProperty ("profile") && ! profileFromName (config["profile"].toString(), result.profile))
        warn ("Unknown profile \"" + config["profile"].toString() + "\"; using Luthier.");

    result.ppq = juce::jlimit (MidiExportOptions::kMinPpq, MidiExportOptions::kMaxPpq,
                               (int) config.getProperty ("ppq", MidiExportOptions::kDefaultPpq));

    if (config.hasProperty ("split") && ! splitFromName (config["split"].toString(), result.split))
        warn ("Unknown track split \"" + config["split"].toString() + "\"; using a single track.");

    result.includeRealism = (bool) config.getProperty ("includeRealism", false);
    result.sysExRedundancy = (bool) config.getProperty ("sysex", true);
    result.stripIdentifiers = (bool) config.getProperty ("stripIdentifiers", false);

    const auto classList = config.getProperty ("classes", {});

    if (const auto* classes = classList.getArray())
    {
        result.classMask = 0;

        for (const auto& entry : *classes)
        {
            const auto className = entry.toString();
            const auto eventClass = LuthierEvents::getClassForName (className);

            if (eventClass == LuthierEventClass::unknown)
                warn ("Unknown event class \"" + className + "\" ignored.");
            else
                result.setClassIncluded (eventClass, true);
        }
    }

    if (name != nullptr)
        *name = profile.getProperty ("meta", {}).getProperty ("name", {}).toString();

    options = result;
    return true;
}

bool saveProfile (const juce::File& file, const MidiExportOptions& options, const juce::String& name)
{
    return file.replaceWithText (juce::JSON::toString (profileToVar (options, name)));
}

bool loadProfile (const juce::File& file, MidiExportOptions& options, juce::String* name,
                  juce::String* error, juce::StringArray* warnings)
{
    if (! file.existsAsFile())
    {
        if (error != nullptr)
            *error = "Could not find " + file.getFullPathName();

        return false;
    }

    juce::var parsed;
    const auto parseResult = juce::JSON::parse (file.loadFileAsString(), parsed);

    if (parseResult.failed())
    {
        if (error != nullptr)
            *error = "The MIDI profile is not valid JSON: " + parseResult.getErrorMessage();

        return false;
    }

    return profileFromVar (parsed, options, name, error, warnings);
}

} // namespace MidiProfiles

} // namespace luthier
