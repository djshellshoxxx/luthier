#include "MidiPerformance.h"

#include "../Support/MidiCapture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace luthier
{

namespace
{
    using Type = ScoreTechnique::Type;

    struct TechniqueInfo
    {
        Type type;
        const char* token;              ///< its name on the wire
        LuthierEventClass carrier;      ///< NOTE means a flag on the note's NOTE event
    };

    constexpr TechniqueInfo kTechniques[] =
    {
        { Type::bend,               "bend",        LuthierEventClass::bend },
        { Type::bendRelease,        "bendrelease", LuthierEventClass::bend },
        { Type::preBend,            "prebend",     LuthierEventClass::bend },
        { Type::slideUp,            "slideup",     LuthierEventClass::slide },
        { Type::slideDown,          "slidedown",   LuthierEventClass::slide },
        { Type::slideLegato,        "slidelegato", LuthierEventClass::slide },
        { Type::slideShift,         "slideshift",  LuthierEventClass::slide },
        { Type::slideIn,            "slidein",     LuthierEventClass::slide },
        { Type::slideOut,           "slideout",    LuthierEventClass::slide },
        { Type::hammerOn,           "hammer",      LuthierEventClass::note },
        { Type::pullOff,            "pull",        LuthierEventClass::note },
        { Type::palmMute,           "pm",          LuthierEventClass::note },
        { Type::deadNote,           "dead",        LuthierEventClass::note },
        { Type::naturalHarmonic,    "natural",     LuthierEventClass::note },
        { Type::pinchHarmonic,      "pinch",       LuthierEventClass::note },
        { Type::artificialHarmonic, "artificial",  LuthierEventClass::note },
        { Type::tapHarmonic,        "tapharm",     LuthierEventClass::note },
        { Type::tap,                "tap",         LuthierEventClass::note },
        { Type::vibrato,            "vibrato",     LuthierEventClass::vibrato },
        { Type::trill,              "trill",       LuthierEventClass::note },
        { Type::whammy,             "whammy",      LuthierEventClass::whammy },
        { Type::ghostNote,          "ghost",       LuthierEventClass::note },
        { Type::accent,             "accent",      LuthierEventClass::note },
        { Type::staccato,           "staccato",    LuthierEventClass::note },
        { Type::letRing,            "letring",     LuthierEventClass::note },
    };

    static_assert (sizeof (kTechniques) / sizeof (kTechniques[0]) == (size_t) Type::numTypes,
                   "every notation technique needs a wire name");

    const TechniqueInfo& infoFor (Type type) noexcept
    {
        for (const auto& info : kTechniques)
            if (info.type == type)
                return info;

        return kTechniques[0];
    }

    const TechniqueInfo* infoForToken (const juce::String& token) noexcept
    {
        for (const auto& info : kTechniques)
            if (token == info.token)
                return &info;

        return nullptr;
    }

    double sanitiseBpm (double bpm) noexcept
    {
        return std::isfinite (bpm) ? juce::jlimit (1.0, 1000.0, bpm) : 120.0;
    }

    int sanitiseDenominator (int denominator) noexcept
    {
        return (denominator == 1 || denominator == 2 || denominator == 4
                  || denominator == 8 || denominator == 16 || denominator == 32) ? denominator : 4;
    }

    juce::int64 toSample (const MidiPerformance& performance, double beat)
    {
        return (juce::int64) std::llround (performance.beatToSample (beat));
    }

    /** NotationExporter's wheel scaling, so a bend lands on the same value in both files. */
    int wheelFor (double semitones, double rangeSemitones) noexcept
    {
        return juce::jlimit (0, 16383, 8192 + (int) (semitones / juce::jmax (0.01, rangeSemitones) * 8192.0));
    }

    juce::String encodeCurve (const std::vector<std::pair<double, double>>& curve)
    {
        juce::StringArray points;

        for (const auto& [position, semitones] : curve)
            points.add (LuthierEvents::formatReal (position) + ":" + LuthierEvents::formatReal (semitones));

        return points.joinIntoString (",");
    }

    std::vector<std::pair<double, double>> decodeCurve (const juce::String& text)
    {
        std::vector<std::pair<double, double>> curve;

        for (const auto& point : juce::StringArray::fromTokens (text, ",", ""))
        {
            double position = 0.0, semitones = 0.0;

            if (LuthierEvents::parseReal (point.upToFirstOccurrenceOf (":", false, false), position)
                  && LuthierEvents::parseReal (point.fromFirstOccurrenceOf (":", false, false), semitones))
                curve.emplace_back (position, semitones);
        }

        return curve;
    }

    /** "pm", "trill:7", "artificial:12:0.5" - a flag and the values it needs. */
    juce::String encodeFlag (const ScoreTechnique& technique)
    {
        juce::String text (infoFor (technique.type).token);

        const bool hasValue = ! juce::exactlyEqual (technique.value, 0.0);
        const bool hasSecond = ! juce::exactlyEqual (technique.secondValue, 0.0);

        if (hasValue || hasSecond)
            text << ":" << LuthierEvents::formatReal (technique.value);

        if (hasSecond)
            text << ":" << LuthierEvents::formatReal (technique.secondValue);

        return text;
    }

    bool decodeFlag (const juce::String& text, ScoreTechnique& technique)
    {
        const auto pieces = juce::StringArray::fromTokens (text, ":", "");

        if (pieces.isEmpty())
            return false;

        const auto* info = infoForToken (pieces[0]);

        if (info == nullptr)
            return false;

        technique = ScoreTechnique {};
        technique.type = info->type;

        if (pieces.size() > 1)
            LuthierEvents::parseReal (pieces[1], technique.value);

        if (pieces.size() > 2)
            LuthierEvents::parseReal (pieces[2], technique.secondValue);

        return true;
    }

    /** The first event of a class at a sample for a channel and key. Events are
        sorted by sample, so this is a binary search and a short walk. */
    const LuthierEvent* findNoteEvent (const std::vector<LuthierEvent>& events,
                                       juce::int64 sample, int channel, int key)
    {
        auto it = std::lower_bound (events.begin(), events.end(), sample,
                                    [] (const LuthierEvent& e, juce::int64 s) { return e.sample < s; });

        for (; it != events.end() && it->sample == sample; ++it)
            if (it->eventClass == LuthierEventClass::note
                  && it->getInt ("ch") == channel && it->getInt ("key") == key)
                return &*it;

        return nullptr;
    }

    std::vector<ScoreTechnique> techniquesFor (const std::vector<LuthierEvent>& events,
                                               juce::int64 sample, int channel, int key)
    {
        std::vector<ScoreTechnique> result;

        auto it = std::lower_bound (events.begin(), events.end(), sample,
                                    [] (const LuthierEvent& e, juce::int64 s) { return e.sample < s; });

        for (; it != events.end() && it->sample == sample; ++it)
        {
            const auto& event = *it;

            if (event.getInt ("ch") != channel || event.getInt ("key") != key)
                continue;

            if (event.eventClass == LuthierEventClass::note)
            {
                for (const auto& flag : juce::StringArray::fromTokens (event.get ("flags"), ",", ""))
                {
                    ScoreTechnique technique;

                    if (decodeFlag (flag, technique))
                        result.push_back (technique);
                }
            }
            else if (event.eventClass == LuthierEventClass::bend
                       || event.eventClass == LuthierEventClass::slide
                       || event.eventClass == LuthierEventClass::vibrato
                       || event.eventClass == LuthierEventClass::whammy)
            {
                const auto* info = infoForToken (event.get ("tech"));

                if (info == nullptr)
                    continue;

                ScoreTechnique technique;
                technique.type = info->type;
                technique.value = event.getReal ("value");
                technique.secondValue = event.getReal ("second");
                technique.curve = decodeCurve (event.get ("curve"));
                result.push_back (technique);
            }
        }

        return result;
    }

    /** One score note as channel messages and extension events. The message
        layout is NotationExporter::writeMidi's: CC 68 around a hammer-on or
        pull-off, pitch bend for bends and whammy, the wheel re-centred at the
        note's end so the next note does not inherit it. */
    void addScoreNote (MidiPerformance& performance, const ScoreNote& note, double measureStart, int part)
    {
        const int channel = juce::jlimit (1, 16, note.stringIndex + 1);
        const int key = juce::jlimit (0, 127, note.midiNote);

        const double startBeat = measureStart + note.startBeat;
        const double lengthBeats = juce::jmax (0.0, note.durationBeats);
        const double endBeat = startBeat + lengthBeats;

        const auto on = toSample (performance, startBeat);
        const auto off = juce::jmax (on + 1, toSample (performance, endBeat));
        const double durationMs = (performance.beatToSeconds (endBeat)
                                     - performance.beatToSeconds (startBeat)) * 1000.0;

        const auto velocity = (juce::uint8) juce::jlimit (1, 127, (int) (note.velocity * 127.0));
        const double bendRange = performance.getMeta().pitchBendRangeSemitones;

        const bool legato = note.hasTechnique (Type::hammerOn) || note.hasTechnique (Type::pullOff);

        /*  riff-library 6.1 (FEAT-RIFFS fix a): the flags a plain MIDI player
            can act on also go out as controllers, as TuneMidi writes them -
            CC67 palm mute, CC72 pinch harmonic, CC73 natural harmonic - set
            before the note and reset after it, so a DAW playing the file
            back through Luthier keeps them. The NOTE flags stay the record. */
        const bool palmMute = note.hasTechnique (Type::palmMute);
        const bool pinch = note.hasTechnique (Type::pinchHarmonic);
        const bool natural = note.hasTechnique (Type::naturalHarmonic);

        if (palmMute) performance.addMessage (on, juce::MidiMessage::controllerEvent (channel, 67, 127), part);
        if (pinch)    performance.addMessage (on, juce::MidiMessage::controllerEvent (channel, 72, 127), part);
        if (natural)  performance.addMessage (on, juce::MidiMessage::controllerEvent (channel, 73, 127), part);

        if (legato)
            performance.addMessage (on, juce::MidiMessage::controllerEvent (channel, 68, 127), part);

        performance.addMessage (on, juce::MidiMessage::noteOn (channel, key, velocity), part);

        auto noteEvent = LuthierEvent::make (LuthierEventClass::note, on, part);
        noteEvent.setInt ("ch", channel).setInt ("key", key)
                 .setInt ("str", note.stringIndex).setInt ("fret", note.fret);

        juce::StringArray flags;
        std::vector<LuthierEvent> techniqueEvents;
        bool wheelMoved = false;

        for (const auto& technique : note.techniques)
        {
            const auto& info = infoFor (technique.type);

            if (info.carrier == LuthierEventClass::note)
            {
                flags.add (encodeFlag (technique));
                continue;
            }

            auto event = LuthierEvent::make (info.carrier, on, part);
            event.setInt ("ch", channel).setInt ("key", key).setInt ("str", note.stringIndex);

            switch (info.carrier)
            {
                case LuthierEventClass::bend:
                {
                    const bool release = technique.type == Type::bendRelease;
                    const bool pre = technique.type == Type::preBend;

                    event.set ("art", release ? "release"
                                    : pre ? "prebend"
                                    : std::abs (technique.value) >= 1.5 ? "whole" : "half");
                    event.setReal ("from", (release || pre) ? technique.value : 0.0);
                    event.setReal ("to", (release || pre) ? technique.secondValue : technique.value);
                    event.setReal ("dur", durationMs);
                    break;
                }

                case LuthierEventClass::slide:
                {
                    static constexpr const char* kinds[] = { "up", "down", "legato", "shift", "in", "out" };
                    const int kind = juce::jlimit (0, 5, (int) technique.type - (int) Type::slideUp);

                    event.set ("kind", kinds[kind]);
                    event.setInt ("from", note.fret);
                    event.setInt ("to", technique.value > 0.0 ? juce::roundToInt (technique.value) : note.fret);
                    event.setReal ("dur", durationMs);
                    break;
                }

                case LuthierEventClass::vibrato:
                    // PerformanceScore: a vibrato's value is its rate.
                    event.setReal ("rate", technique.value > 0.0 ? technique.value : 5.0);
                    event.setReal ("depth", technique.secondValue);
                    event.setReal ("delay", 0.0);
                    event.set ("style", "finger");
                    break;

                case LuthierEventClass::whammy:
                    event.setReal ("target", technique.value);
                    event.setReal ("dur", durationMs);
                    break;

                default:
                    break;
            }

            event.set ("tech", info.token)
                 .setReal ("value", technique.value)
                 .setReal ("second", technique.secondValue)
                 .set ("curve", encodeCurve (technique.curve));

            techniqueEvents.push_back (std::move (event));

            const bool isWheel = technique.type == Type::bend
                                   || technique.type == Type::whammy
                                   || technique.type == Type::preBend;

            if (! isWheel)
                continue;

            if (! technique.curve.empty())
            {
                for (const auto& [position, semitones] : technique.curve)
                    performance.addMessage (toSample (performance, startBeat + position * lengthBeats),
                                            juce::MidiMessage::pitchWheel (channel, wheelFor (semitones, bendRange)),
                                            part);
            }
            else
            {
                performance.addMessage (on, juce::MidiMessage::pitchWheel (channel, wheelFor (technique.value, bendRange)),
                                        part);
            }

            wheelMoved = true;
        }

        noteEvent.set ("flags", flags.joinIntoString (","));
        performance.addEvent (noteEvent);

        for (const auto& event : techniqueEvents)
            performance.addEvent (event);

        if (wheelMoved)
            performance.addMessage (off, juce::MidiMessage::pitchWheel (channel, 8192), part);

        performance.addMessage (off, juce::MidiMessage::noteOff (channel, key), part);

        if (legato)
            performance.addMessage (off, juce::MidiMessage::controllerEvent (channel, 68, 0), part);

        if (natural)  performance.addMessage (off, juce::MidiMessage::controllerEvent (channel, 73, 0), part);
        if (pinch)    performance.addMessage (off, juce::MidiMessage::controllerEvent (channel, 72, 0), part);
        if (palmMute) performance.addMessage (off, juce::MidiMessage::controllerEvent (channel, 67, 0), part);
    }
}

//==============================================================================
MidiPerformance::MidiPerformance (double rate)
{
    setSampleRate (rate);
    clear();
}

void MidiPerformance::setSampleRate (double newSampleRate) noexcept
{
    sampleRate = (std::isfinite (newSampleRate) && newSampleRate > 0.0) ? newSampleRate : 48000.0;
}

void MidiPerformance::clear()
{
    meta = Meta {};
    tempoMap = { PerformanceTempo {} };
    timeSignatures = { PerformanceTimeSignature {} };
    messages.clear();
    events.clear();
}

//==============================================================================
void MidiPerformance::setTempo (double bpm)
{
    tempoMap.clear();
    tempoMap.push_back ({ 0.0, sanitiseBpm (bpm) });
}

void MidiPerformance::addTempoChange (double beat, double bpm)
{
    const PerformanceTempo change { juce::jmax (0.0, beat), sanitiseBpm (bpm) };

    for (auto& existing : tempoMap)
    {
        if (juce::exactlyEqual (existing.beat, change.beat))
        {
            existing.bpm = change.bpm;
            return;
        }
    }

    tempoMap.insert (std::upper_bound (tempoMap.begin(), tempoMap.end(), change.beat,
                                       [] (double b, const PerformanceTempo& t) { return b < t.beat; }),
                     change);
}

double MidiPerformance::getTempoAt (double beat) const noexcept
{
    double bpm = tempoMap.empty() ? 120.0 : tempoMap.front().bpm;

    for (const auto& change : tempoMap)
        if (change.beat <= beat)
            bpm = change.bpm;

    return bpm;
}

void MidiPerformance::setTimeSignature (int numerator, int denominator)
{
    timeSignatures.clear();
    timeSignatures.push_back ({ 0.0, juce::jlimit (1, 32, numerator), sanitiseDenominator (denominator) });
}

void MidiPerformance::addTimeSignature (double beat, int numerator, int denominator)
{
    const PerformanceTimeSignature change { juce::jmax (0.0, beat), juce::jlimit (1, 32, numerator),
                                            sanitiseDenominator (denominator) };

    for (auto& existing : timeSignatures)
    {
        if (juce::exactlyEqual (existing.beat, change.beat))
        {
            existing = change;
            return;
        }
    }

    timeSignatures.insert (std::upper_bound (timeSignatures.begin(), timeSignatures.end(), change.beat,
                                             [] (double b, const PerformanceTimeSignature& t) { return b < t.beat; }),
                           change);
}

PerformanceTimeSignature MidiPerformance::getTimeSignatureAt (double beat) const noexcept
{
    PerformanceTimeSignature result = timeSignatures.empty() ? PerformanceTimeSignature {}
                                                             : timeSignatures.front();

    for (const auto& change : timeSignatures)
        if (change.beat <= beat)
            result = change;

    return result;
}

double MidiPerformance::beatToSeconds (double beat) const noexcept
{
    double seconds = 0.0;

    for (size_t i = 0; i < tempoMap.size(); ++i)
    {
        const double segmentStart = i == 0 ? 0.0 : tempoMap[i].beat;
        const double secondsPerBeat = 60.0 / tempoMap[i].bpm;
        const bool last = i + 1 == tempoMap.size();

        if (last || beat <= tempoMap[i + 1].beat)
            return seconds + (beat - segmentStart) * secondsPerBeat;

        seconds += (tempoMap[i + 1].beat - segmentStart) * secondsPerBeat;
    }

    return beat * 0.5;   // an empty map is 120 bpm
}

double MidiPerformance::secondsToBeat (double seconds) const noexcept
{
    double segmentSeconds = 0.0;

    for (size_t i = 0; i < tempoMap.size(); ++i)
    {
        const double segmentStart = i == 0 ? 0.0 : tempoMap[i].beat;
        const double secondsPerBeat = 60.0 / tempoMap[i].bpm;
        const bool last = i + 1 == tempoMap.size();
        const double segmentLength = last ? 0.0 : (tempoMap[i + 1].beat - segmentStart) * secondsPerBeat;

        if (last || seconds <= segmentSeconds + segmentLength)
            return segmentStart + (seconds - segmentSeconds) / secondsPerBeat;

        segmentSeconds += segmentLength;
    }

    return seconds * 2.0;
}

//==============================================================================
bool MidiPerformance::isChannelVoiceMessage (const juce::MidiMessage& message) noexcept
{
    const int size = message.getRawDataSize();

    if (size < 2)
        return false;

    const auto status = message.getRawData()[0];

    return status >= 0x80 && status < 0xF0
        && size == juce::MidiMessage::getMessageLengthFromFirstByte (status);
}

bool MidiPerformance::addMessage (juce::int64 sample, const juce::MidiMessage& message, int part)
{
    if (! isChannelVoiceMessage (message))
        return false;

    PerformanceMessage entry;
    entry.sample = juce::jmax ((juce::int64) 0, sample);
    entry.message = message;
    entry.message.setTimeStamp ((double) entry.sample);
    entry.part = juce::jlimit (0, 15, part);

    if (messages.empty() || messages.back().sample <= entry.sample)
    {
        messages.push_back (std::move (entry));
    }
    else
    {
        const auto at = std::upper_bound (messages.begin(), messages.end(), entry.sample,
                                          [] (juce::int64 s, const PerformanceMessage& m) { return s < m.sample; });
        messages.insert (at, std::move (entry));
    }

    return true;
}

void MidiPerformance::addEvent (const LuthierEvent& event)
{
    auto entry = event;
    entry.sample = juce::jmax ((juce::int64) 0, entry.sample);
    entry.part = juce::jlimit (0, 15, entry.part);

    if (events.empty() || events.back().sample <= entry.sample)
    {
        events.push_back (std::move (entry));
    }
    else
    {
        const auto at = std::upper_bound (events.begin(), events.end(), entry.sample,
                                          [] (juce::int64 s, const LuthierEvent& e) { return s < e.sample; });
        events.insert (at, std::move (entry));
    }
}

int MidiPerformance::countEvents (LuthierEventClass eventClass) const noexcept
{
    return (int) std::count_if (events.begin(), events.end(),
                                [eventClass] (const LuthierEvent& e) { return e.eventClass == eventClass; });
}

juce::int64 MidiPerformance::getLengthInSamples() const noexcept
{
    juce::int64 last = -1;

    if (! messages.empty())
        last = messages.back().sample;

    if (! events.empty())
        last = juce::jmax (last, events.back().sample);

    return last + 1;
}

//==============================================================================
juce::Range<juce::int64> MidiPerformance::getLastSecondsRange (double seconds) const noexcept
{
    const auto length = getLengthInSamples();
    const auto span = (juce::int64) std::llround (juce::jmax (0.0, seconds) * sampleRate);

    return { juce::jmax ((juce::int64) 0, length - span), length };
}

juce::Range<juce::int64> MidiPerformance::getSectionRange (const juce::String& sectionName) const
{
    for (size_t i = 0; i < events.size(); ++i)
    {
        const auto& event = events[i];

        if (event.eventClass != LuthierEventClass::section
              || event.get ("edge") != "start" || event.get ("name") != sectionName)
            continue;

        auto end = getLengthInSamples();

        for (size_t j = i + 1; j < events.size(); ++j)
        {
            if (events[j].eventClass == LuthierEventClass::section && events[j].sample > event.sample)
            {
                end = events[j].sample;
                break;
            }
        }

        return { event.sample, juce::jmax (event.sample, end) };
    }

    return {};
}

juce::StringArray MidiPerformance::getSectionNames() const
{
    juce::StringArray names;

    for (const auto& event : events)
        if (event.eventClass == LuthierEventClass::section && event.get ("edge") == "start")
            names.addIfNotAlreadyThere (event.get ("name"));

    return names;
}

MidiPerformance MidiPerformance::extractRange (juce::Range<juce::int64> range) const
{
    if (range.isEmpty())
        return *this;

    const auto start = juce::jmax ((juce::int64) 0, range.getStart());
    const auto end = juce::jmax (start, range.getEnd());

    MidiPerformance result (sampleRate);
    result.meta = meta;

    // ---- the tempo and metre in force, moved to beat zero ------------------------
    const double startBeat = sampleToBeat ((double) start);
    const double endBeat = sampleToBeat ((double) end);

    result.tempoMap = { { 0.0, getTempoAt (startBeat) } };

    for (const auto& change : tempoMap)
        if (change.beat > startBeat && change.beat < endBeat)
            result.tempoMap.push_back ({ change.beat - startBeat, change.bpm });

    const auto signature = getTimeSignatureAt (startBeat);
    result.timeSignatures = { { 0.0, signature.numerator, signature.denominator } };

    for (const auto& change : timeSignatures)
        if (change.beat > startBeat && change.beat < endBeat)
            result.timeSignatures.push_back ({ change.beat - startBeat, change.numerator, change.denominator });

    // ---- the controllers in force at the start, restated at zero -----------------
    struct ChannelState
    {
        std::array<int, 128> cc;
        int wheel = -1, program = -1, pressure = -1;
        int part = 0;
    };

    std::array<ChannelState, 16> state;

    for (auto& s : state)
        s.cc.fill (-1);

    size_t index = 0;

    for (; index < messages.size() && messages[index].sample < start; ++index)
    {
        const auto& message = messages[index].message;
        auto& s = state[(size_t) juce::jlimit (0, 15, message.getChannel() - 1)];
        s.part = messages[index].part;

        if (message.isController())
            s.cc[(size_t) message.getControllerNumber()] = message.getControllerValue();
        else if (message.isPitchWheel())
            s.wheel = message.getPitchWheelValue();
        else if (message.isProgramChange())
            s.program = message.getProgramChangeNumber();
        else if (message.isChannelPressure())
            s.pressure = message.getChannelPressureValue();
    }

    for (int channel = 1; channel <= 16; ++channel)
    {
        const auto& s = state[(size_t) channel - 1];

        if (s.program >= 0)
            result.addMessage (0, juce::MidiMessage::programChange (channel, s.program), s.part);

        for (int cc = 0; cc < 128; ++cc)
            if (s.cc[(size_t) cc] >= 0)
                result.addMessage (0, juce::MidiMessage::controllerEvent (channel, cc, s.cc[(size_t) cc]), s.part);

        if (s.wheel >= 0 && s.wheel != 8192)
            result.addMessage (0, juce::MidiMessage::pitchWheel (channel, s.wheel), s.part);

        if (s.pressure > 0)
            result.addMessage (0, juce::MidiMessage::channelPressureChange (channel, s.pressure), s.part);
    }

    // ---- the stretch itself --------------------------------------------------------
    std::array<std::array<int, 128>, 16> sounding;   // the part of a held note, or -1

    for (auto& row : sounding)
        row.fill (-1);

    for (; index < messages.size() && messages[index].sample < end; ++index)
    {
        const auto& entry = messages[index];
        const auto& message = entry.message;
        const auto channel = (size_t) juce::jlimit (0, 15, message.getChannel() - 1);
        const auto at = entry.sample - start;

        if (message.isNoteOn())
        {
            sounding[channel][(size_t) message.getNoteNumber()] = entry.part;
            result.addMessage (at, message, entry.part);
        }
        else if (message.isNoteOff())
        {
            auto& held = sounding[channel][(size_t) message.getNoteNumber()];

            // A note-off for a note that began before the range is left out
            // with its note-on.
            if (held < 0)
                continue;

            result.addMessage (at, message, entry.part);
            held = -1;
        }
        else
        {
            result.addMessage (at, message, entry.part);
        }
    }

    for (int channel = 0; channel < 16; ++channel)
        for (int key = 0; key < 128; ++key)
            if (sounding[(size_t) channel][(size_t) key] >= 0)
                result.addMessage (end - start, juce::MidiMessage::noteOff (channel + 1, key),
                                   sounding[(size_t) channel][(size_t) key]);

    for (const auto& event : events)
    {
        if (event.sample >= start && event.sample < end)
        {
            auto copy = event;
            copy.sample -= start;
            result.addEvent (copy);
        }
    }

    return result;
}

//==============================================================================
void MidiPerformance::renderBlock (juce::MidiBuffer& destination, juce::int64 blockStart,
                                   int numSamples, size_t& cursor) const
{
    const auto blockEnd = blockStart + juce::jmax (0, numSamples);

    // Anything before the block was missed (a seek, or a cursor started late).
    while (cursor < messages.size() && messages[cursor].sample < blockStart)
        ++cursor;

    while (cursor < messages.size() && messages[cursor].sample < blockEnd)
    {
        const auto& entry = messages[cursor++];
        destination.addEvent (entry.message, (int) (entry.sample - blockStart));
    }
}

//==============================================================================
MidiPerformance MidiPerformance::fromScore (const PerformanceScore& score, double rate)
{
    MidiPerformance performance (rate);

    const auto& scoreMeta = score.getMeta();

    performance.meta.title = scoreMeta.title;
    performance.setTempo (scoreMeta.tempoBpm);
    performance.setTimeSignature (scoreMeta.timeSignatureNumerator, scoreMeta.timeSignatureDenominator);

    if (score.getNumTracks() == 0)
        return performance;

    // NotationExporter lays every measure out at the score's own metre, and so
    // does this, so a bar starts on the same beat in both files.
    const double beatsPerMeasure = (double) scoreMeta.timeSignatureNumerator * 4.0
                                     / (double) juce::jmax (1, scoreMeta.timeSignatureDenominator);

    const auto& first = score.getTrack (0);

    // The tempo map first: every sample below depends on it.
    for (size_t m = 1; m < first.measures.size(); ++m)
        if (first.measures[m].tempoChange > 0.0)
            performance.addTempoChange ((double) m * beatsPerMeasure, first.measures[m].tempoChange);

    int sectionIndex = 0;

    for (size_t m = 0; m < first.measures.size(); ++m)
    {
        if (first.measures[m].sectionName.isEmpty())
            continue;

        auto section = LuthierEvent::make (LuthierEventClass::section,
                                           toSample (performance, (double) m * beatsPerMeasure));
        section.set ("name", first.measures[m].sectionName)
               .setInt ("index", sectionIndex++)
               .set ("edge", "start");
        performance.addEvent (section);
    }

    for (int t = 0; t < score.getNumTracks(); ++t)
    {
        const auto& track = score.getTrack (t);
        const int part = juce::jlimit (0, 15, t);

        for (size_t m = 0; m < track.measures.size(); ++m)
            for (const auto* note : track.measures[m].collectNotes())
                addScoreNote (performance, *note, (double) m * beatsPerMeasure, part);
    }

    return performance;
}

void MidiPerformance::toScore (PerformanceScore& score) const
{
    const auto tuning = score.getTrack (0).tuning;
    const int numStrings = juce::jlimit (1, kMaxStrings, score.getTrack (0).numStrings);

    const auto signature = getTimeSignatureAt (0.0);
    score.beginCapture (getTempoAt (0.0), signature.numerator, signature.denominator);

    if (meta.title.isNotEmpty())
        score.getMeta().title = meta.title;

    // The string each sounding (channel, key) was given, and the note each
    // string holds, so a late note-off cannot end a newer note on its string.
    std::array<std::array<int, 128>, 16> stringOf;

    for (auto& row : stringOf)
        row.fill (-1);

    std::array<int, kMaxStrings> holding;
    holding.fill (-1);

    for (const auto& entry : messages)
    {
        if (entry.part != 0)
            continue;

        const auto& message = entry.message;
        const int channel = message.getChannel();

        if (channel < 1 || channel > 16)
            continue;

        const double beat = sampleToBeat ((double) entry.sample);
        const int key = message.getNoteNumber();
        const int noteId = (channel - 1) * 128 + key;

        if (message.isNoteOn())
        {
            const auto* noteEvent = findNoteEvent (events, entry.sample, channel, key);

            int stringIndex = noteEvent != nullptr ? (int) noteEvent->getInt ("str") : -1;

            if (! juce::isPositiveAndBelow (stringIndex, numStrings))
                stringIndex = juce::isPositiveAndBelow (channel - 1, numStrings) ? channel - 1 : 0;

            int fret = noteEvent != nullptr ? (int) noteEvent->getInt ("fret") : -1;

            if (fret < 0)
                fret = juce::jmax (0, key - tuning[(size_t) stringIndex]);

            const double hz = 440.0 * std::pow (2.0, (double) (key - 69) / 12.0);

            score.noteStarted (stringIndex, fret, key, hz, (double) message.getVelocity() / 127.0, beat);

            stringOf[(size_t) channel - 1][(size_t) key] = stringIndex;
            holding[(size_t) stringIndex] = noteId;

            for (const auto& technique : techniquesFor (events, entry.sample, channel, key))
                score.addTechnique (stringIndex, technique);
        }
        else if (message.isNoteOff())
        {
            auto& stringIndex = stringOf[(size_t) channel - 1][(size_t) key];

            if (stringIndex >= 0 && holding[(size_t) stringIndex] == noteId)
            {
                score.noteEnded (stringIndex, beat);
                holding[(size_t) stringIndex] = -1;
            }

            stringIndex = -1;
        }
    }

    score.endCapture (sampleToBeat ((double) getLengthInSamples()));
}

MidiPerformance MidiPerformance::fromCapture (const MidiCapture& capture, double rate, double tempoBpm)
{
    MidiPerformance performance (rate);
    performance.setTempo (tempoBpm);
    performance.meta.title = "Luthier Capture";

    const auto file = capture.buildMidiFile (tempoBpm);

    if (file.getNumTracks() < 2)
        return performance;

    // MidiCapture writes seconds * (bpm / 60) * 960 as a fractional tick, from
    // the exact sample each event arrived at; this is the inverse, so the
    // performance keeps the capture's own sample timing.
    const double ticksPerSecond = (tempoBpm / 60.0) * 960.0;

    if (ticksPerSecond <= 0.0)
        return performance;

    for (const auto* holder : *file.getTrack (1))
    {
        const auto& message = holder->message;
        const auto sample = (juce::int64) std::llround (message.getTimeStamp() / ticksPerSecond * performance.sampleRate);
        performance.addMessage (sample, message);
    }

    return performance;
}

//==============================================================================
bool MidiPerformance::isEquivalentTo (const MidiPerformance& other, juce::String* firstDifference) const
{
    auto differ = [firstDifference] (const juce::String& why)
    {
        if (firstDifference != nullptr)
            *firstDifference = why;

        return false;
    };

    if (messages.size() != other.messages.size())
        return differ (juce::String ((int) messages.size()) + " messages against "
                         + juce::String ((int) other.messages.size()));

    for (size_t i = 0; i < messages.size(); ++i)
    {
        const auto& a = messages[i];
        const auto& b = other.messages[i];

        const bool sameBytes = a.message.getRawDataSize() == b.message.getRawDataSize()
                                 && std::memcmp (a.message.getRawData(), b.message.getRawData(),
                                                 (size_t) a.message.getRawDataSize()) == 0;

        if (a.sample != b.sample || a.part != b.part || ! sameBytes)
            return differ ("message " + juce::String ((int) i) + ": "
                             + a.message.getDescription() + " at " + juce::String (a.sample)
                             + " (part " + juce::String (a.part) + ") against "
                             + b.message.getDescription() + " at " + juce::String (b.sample)
                             + " (part " + juce::String (b.part) + ")");
    }

    if (events.size() != other.events.size())
        return differ (juce::String ((int) events.size()) + " extension events against "
                         + juce::String ((int) other.events.size()));

    // Events at one sample may come back in a different order from a file split
    // over several tracks; each event stands alone, so that order means nothing.
    auto describe = [] (const std::vector<LuthierEvent>& list)
    {
        juce::StringArray lines;

        for (const auto& e : list)
        {
            juce::String line;
            line << juce::String (e.sample) << " part=" << juce::String (e.part) << " "
                 << e.className << " " << juce::String (e.schemaVersion);

            for (const auto& [key, value] : e.fields)
                line << " " << key << "=" << LuthierEvents::escapeValue (value);

            if (e.opaqueSysEx.getSize() > 0)
                line << " sysex=" << juce::String::toHexString (e.opaqueSysEx.getData(), (int) e.opaqueSysEx.getSize(), 0);

            if (e.opaqueText.isNotEmpty())
                line << " text=" << LuthierEvents::escapeValue (e.opaqueText);

            lines.add (line);
        }

        lines.sort (false);
        return lines;
    };

    const auto mine = describe (events);
    const auto theirs = describe (other.events);

    for (int i = 0; i < mine.size(); ++i)
        if (mine[i] != theirs[i])
            return differ ("extension event " + mine[i] + " against " + theirs[i]);

    return true;
}

} // namespace luthier
