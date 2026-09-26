#include "PerformanceCapture.h"
#include "../Rhythm/ChordDetector.h"   // MODEL-GAPS: offline chord extraction

#include "../Model/Playing/TuningEngine.h"
#include "../Routing/MidiOutRouter.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace luthier
{

namespace
{
    using Type = ScoreTechnique::Type;

    /** What the engine's own technique tag means in notation. Plucks, strums
        and slide-guitar notes are how a note was played, not a mark on it. */
    bool scoreTypeFor (Technique technique, Type& type) noexcept
    {
        switch (technique)
        {
            case Technique::HammerOn:           type = Type::hammerOn;           return true;
            case Technique::PullOff:            type = Type::pullOff;            return true;
            case Technique::Slide:              type = Type::slideLegato;        return true;
            case Technique::Bend:               type = Type::bend;               return true;
            case Technique::Vibrato:            type = Type::vibrato;            return true;
            case Technique::PalmMute:           type = Type::palmMute;           return true;
            case Technique::MutedPick:          type = Type::deadNote;           return true;
            case Technique::NaturalHarmonic:    type = Type::naturalHarmonic;    return true;
            case Technique::PinchHarmonic:      type = Type::pinchHarmonic;      return true;
            case Technique::ArtificialHarmonic: type = Type::artificialHarmonic; return true;
            case Technique::Tap:                type = Type::tap;                return true;

            case Technique::Pluck:
            case Technique::SlideGuitar:
            case Technique::Strum:
            case Technique::NumTechniques:
            default:
                return false;
        }
    }

    double snap (double beat, double grid, double strength) noexcept
    {
        if (grid <= 0.0)
            return beat;

        const double target = std::round (beat / grid) * grid;
        return beat + (target - beat) * juce::jlimit (0.0, 1.0, strength);
    }

    double hzFor (int midiNote) noexcept
    {
        return 440.0 * std::pow (2.0, (double) (midiNote - 69) / 12.0);
    }

    /** A record's text, which setText always leaves terminated. */
    juce::String textOf (const CaptureRecord& record)
    {
        int length = 0;

        while (length < (int) sizeof (record.text) && record.text[length] != 0)
            ++length;

        return juce::CharPointer_UTF8::isValidString (record.text, length)
                 ? juce::String::fromUTF8 (record.text, length)
                 : juce::String();
    }
}

//==============================================================================
const char* getCaptureStateName (CaptureState captureState) noexcept
{
    switch (captureState)
    {
        case CaptureState::off:     return "off";
        case CaptureState::armed:   return "armed";
        case CaptureState::rolling:
        default:                    return "rolling";
    }
}

//==============================================================================
/*  Where each item of a take sits in beats.

    A take played entirely with the transport running is in the host's own
    quarter notes, moved so its first bar starts at beat zero and bar lines
    fall where the host's do. Anything else - free play, or a take that
    straddles a transport start - is read from samples at one tempo, starting
    at the first note (6.4: "free-play capture is quantisable afterwards"). */
struct PerformanceCapture::Timeline
{
    bool musical = false;
    double originPpq = 0.0;

    juce::int64 anchorSample = 0;
    double anchorBeat = 0.0;

    double bpm = 120.0;
    int numerator = 4;
    int denominator = 4;
    double sampleRate = 48000.0;

    juce::int64 cutoffSample = 0;
    juce::int64 endSample = std::numeric_limits<juce::int64>::max();
    juce::Range<double> ppqRange;

    /** Whether something at this place is in the export's range (MODEL-GAPS). */
    bool includes (bool itemMusical, double ppq, juce::int64 sample) const noexcept
    {
        if (sample < cutoffSample || sample >= endSample)
            return false;

        if (! ppqRange.isEmpty())
            return itemMusical && ppq >= ppqRange.getStart() && ppq < ppqRange.getEnd();

        return true;
    }

    double beatOf (bool itemMusical, double ppq, juce::int64 sample) const noexcept
    {
        if (musical && itemMusical)
            return ppq - originPpq;

        return anchorBeat + (double) (sample - anchorSample) / sampleRate * bpm / 60.0;
    }
};

//==============================================================================
PerformanceCapture::PerformanceCapture()
{
    std::array<int, kMaxStrings> standard { { 64, 59, 55, 50, 45, 40, 0, 0, 0, 0, 0, 0 } };
    setTuning (standard, 6, 0);
    sounding.fill (-1);
}

void PerformanceCapture::prepare (double rate)
{
    sampleRate = rate > 0.0 ? rate : 48000.0;
    clock.sampleRate = sampleRate;

    lastMeterBpm = -1.0;
    lastMeterNumerator = lastMeterDenominator = -1;

    ring.reset();
    clearTake();
}

void PerformanceCapture::setState (CaptureState newState)
{
    state.store ((int) newState, std::memory_order_relaxed);

    if (newState == CaptureState::armed)
    {
        clearTake();
        ring.skipTo (ring.getWriteCount());
        waitingForFirstNote = true;
    }

    // The next block states the meter again, so a take that starts now knows
    // its tempo even though the records that said so earlier were skipped.
    restateMeter.store (true, std::memory_order_relaxed);
}

void PerformanceCapture::setRollingMinutes (double minutes) noexcept
{
    rollingMinutes = juce::jmax (0.1, minutes);
}

void PerformanceCapture::setTuning (const std::array<int, kMaxStrings>& openNotesAtNut, int stringCount,
                                    int capo, juce::uint32 capoStringMask)
{
    numStrings = juce::jlimit (1, kMaxStrings, stringCount);
    capoFret = juce::jmax (0, capo);
    scoreTuning = openNotesAtNut;

    for (int s = 0; s < kMaxStrings; ++s)
    {
        const bool clamped = s < 32 && ((capoStringMask >> s) & 1u) != 0;
        cappedOpenNotes[(size_t) s].store (openNotesAtNut[(size_t) s] + (clamped ? capoFret : 0),
                                           std::memory_order_relaxed);
    }
}

std::array<int, kMaxStrings> PerformanceCapture::getOpenNotes (const TuningEngine& tuning, int stringCount)
{
    std::array<int, kMaxStrings> result {};
    const double concertA = juce::jmax (1.0, tuning.getConcertA());

    for (int s = 0; s < juce::jlimit (0, kMaxStrings, stringCount); ++s)
    {
        const double hz = juce::jmax (1.0, tuning.getStringTuning (s).openFrequencyHz);
        result[(size_t) s] = juce::roundToInt (69.0 + 12.0 * std::log2 (hz / concertA));
    }

    return result;
}

//==============================================================================
CaptureRecord PerformanceCapture::makeRecord (CaptureRecord::Kind kind, int sampleOffset) const noexcept
{
    CaptureRecord record;
    record.kind = kind;

    const int offset = juce::jmax (0, sampleOffset);
    record.sample = clock.blockStartSample + offset;
    record.musical = clock.transportPlaying;

    if (record.musical)
        record.ppq = clock.blockStartPpq
                       + (double) offset * clock.bpm / (60.0 * juce::jmax (1.0, clock.sampleRate));

    return record;
}

void PerformanceCapture::beginBlock (const CaptureClock& newClock) noexcept
{
    clockSample.store (newClock.blockStartSample, std::memory_order_relaxed);   // MODEL-GAPS: the marks' now

    clock = newClock;

    if (restateMeter.exchange (false, std::memory_order_relaxed))
        lastMeterBpm = -1.0;

    if (! isRecording())
    {
        // Turning capture back on restates the meter.
        lastMeterBpm = -1.0;
        return;
    }

    const bool changed = ! juce::approximatelyEqual (lastMeterBpm, clock.bpm)
                           || lastMeterNumerator != clock.timeSigNumerator
                           || lastMeterDenominator != clock.timeSigDenominator;

    if (! changed)
        return;

    auto record = makeRecord (CaptureRecord::Kind::meter, 0);
    record.value = (float) clock.bpm;
    record.code = (juce::uint8) juce::jlimit (1, 255, clock.timeSigNumerator);
    record.code2 = (juce::uint8) juce::jlimit (1, 255, clock.timeSigDenominator);
    ring.push (record);

    lastMeterBpm = clock.bpm;
    lastMeterNumerator = clock.timeSigNumerator;
    lastMeterDenominator = clock.timeSigDenominator;
}

void PerformanceCapture::noteOn (int sampleOffset, int stringIndex, int midiNote, double fret, float velocity,
                                 Technique playedAs, int harmonicPartial) noexcept
{
    if (! isRecording())
        return;

    auto record = makeRecord (CaptureRecord::Kind::noteOn, sampleOffset);
    record.stringIndex = (juce::int8) juce::jlimit (0, kMaxStrings - 1, stringIndex);
    record.midiNote = (juce::uint8) juce::jlimit (0, 127, midiNote);
    record.fret = (float) fret;
    record.value = juce::jlimit (0.0f, 1.0f, velocity);
    record.code = (juce::uint8) playedAs;
    record.code2 = (juce::uint8) juce::jlimit (0, 255, harmonicPartial);
    ring.push (record);
}

void PerformanceCapture::noteOff (int sampleOffset, int stringIndex, bool letRing) noexcept
{
    if (! isRecording())
        return;

    auto record = makeRecord (CaptureRecord::Kind::noteOff, sampleOffset);
    record.stringIndex = (juce::int8) juce::jlimit (0, kMaxStrings - 1, stringIndex);
    record.flags = letRing ? 1 : 0;
    ring.push (record);
}

void PerformanceCapture::bend (int sampleOffset, int stringIndex, double cents) noexcept
{
    if (! isRecording())
        return;

    auto record = makeRecord (CaptureRecord::Kind::bend, sampleOffset);
    record.stringIndex = (juce::int8) juce::jlimit (-1, kMaxStrings - 1, stringIndex);
    record.value = (float) cents;
    ring.push (record);
}

void PerformanceCapture::mark (int sampleOffset, int stringIndex, ScoreTechnique::Type type, double value) noexcept
{
    if (! isRecording())
        return;

    auto record = makeRecord (CaptureRecord::Kind::mark, sampleOffset);
    record.stringIndex = (juce::int8) juce::jlimit (0, kMaxStrings - 1, stringIndex);
    record.code = (juce::uint8) type;
    record.value = (float) value;
    ring.push (record);
}

void PerformanceCapture::chordSymbol (int sampleOffset, const char* name) noexcept
{
    if (! isRecording() || name == nullptr || name[0] == 0)
        return;

    auto record = makeRecord (CaptureRecord::Kind::chord, sampleOffset);
    record.setText (name);
    ring.push (record);
}

void PerformanceCapture::bassTechnique (int sampleOffset, int stringIndex, const char* technique,
                                        double pluckPosition, double force, double fretContact) noexcept
{
    if (! isRecording())
        return;

    auto record = makeRecord (CaptureRecord::Kind::bassTechnique, sampleOffset);
    record.stringIndex = (juce::int8) juce::jlimit (-1, kMaxStrings - 1, stringIndex);
    record.setText (technique);
    record.fret = (float) pluckPosition;
    record.value = (float) juce::jlimit (0.0, 1.0, force);                            // SPEC-SWEEP BT-24
    record.code = (juce::uint8) juce::roundToInt (juce::jlimit (0.0, 1.0, fretContact) * 255.0);
    ring.push (record);
}

void PerformanceCapture::slideBar (int sampleOffset, double fretPosition, const char* pressure) noexcept
{
    if (! isRecording())
        return;

    auto record = makeRecord (CaptureRecord::Kind::slideBar, sampleOffset);
    record.value = (float) fretPosition;
    record.setText (pressure);
    ring.push (record);
}

void PerformanceCapture::captureStringActivity (const StringActivityQueue& activity) noexcept
{
    if (! isRecording())
        return;

    for (int i = 0; i < activity.size(); ++i)
    {
        const auto& e = activity[i];
        const int s = juce::jlimit (0, kMaxStrings - 1, e.stringIndex);

        if (e.isNoteOn)
            noteOn (e.sampleOffset, s, e.midiNote,
                    (double) (e.midiNote - cappedOpenNotes[(size_t) s].load (std::memory_order_relaxed)),
                    e.velocity);
        else
            noteOff (e.sampleOffset, s);
    }
}

//==============================================================================
int PerformanceCapture::drain()
{
    incoming.clear();
    const int count = ring.drain (incoming);

    for (const auto& record : incoming)
        apply (record);

    if (getState() == CaptureState::rolling)
        trimToRollingWindow();

    return count;
}

void PerformanceCapture::apply (const CaptureRecord& record)
{
    newestSample = juce::jmax (newestSample, record.sample);

    using Kind = CaptureRecord::Kind;

    // 6.3: an armed take starts from its first note; the meter is kept so the
    // take knows its tempo.
    if (waitingForFirstNote && record.kind != Kind::noteOn && record.kind != Kind::meter)
        return;

    const int s = juce::jlimit (0, kMaxStrings - 1, (int) record.stringIndex);

    auto closeNote = [this] (int stringIndex, const CaptureRecord& at)
    {
        const int index = sounding[(size_t) stringIndex];

        if (! juce::isPositiveAndBelow (index, (int) notes.size()))
            return;

        auto& note = notes[(size_t) index];
        note.endSample = juce::jmax (note.startSample, at.sample);
        note.endPpq = at.musical ? juce::jmax (note.startPpq, at.ppq) : note.startPpq;
        sounding[(size_t) stringIndex] = -1;
    };

    switch (record.kind)
    {
        case Kind::noteOn:
        {
            waitingForFirstNote = false;

            // One string plays one note: a new note on a sounding string ends the old one.
            closeNote (s, record);

            CapturedNote note;
            note.startSample = record.sample;
            note.musical = record.musical;
            note.startPpq = record.ppq;
            note.endPpq = record.ppq;
            note.stringIndex = s;
            note.midiNote = record.midiNote;
            note.fret = record.fret;
            note.velocity = record.value;
            note.technique = (Technique) juce::jlimit (0, (int) Technique::NumTechniques - 1, (int) record.code);
            note.harmonicPartial = record.code2;

            notes.push_back (std::move (note));
            sounding[(size_t) s] = (int) notes.size() - 1;
            break;
        }

        case Kind::noteOff:
            closeNote (s, record);
            break;

        case Kind::bend:
        {
            // A bend on every string (-1) belongs to each sounding note.
            for (int string = 0; string < kMaxStrings; ++string)
            {
                if (record.stringIndex >= 0 && string != s)
                    continue;

                const int index = sounding[(size_t) string];

                if (juce::isPositiveAndBelow (index, (int) notes.size()))
                    notes[(size_t) index].bend.emplace_back (record.sample, (double) record.value);
            }

            break;
        }

        case Kind::mark:
        {
            const int index = sounding[(size_t) s];

            if (juce::isPositiveAndBelow (index, (int) notes.size())
                  && record.code < (juce::uint8) ScoreTechnique::Type::numTypes)
            {
                ScoreTechnique technique;
                technique.type = (Type) record.code;
                technique.value = record.value;
                notes[(size_t) index].marks.push_back (technique);
            }

            break;
        }

        case Kind::chord:
        {
            CapturedChord chord;
            chord.sample = record.sample;
            chord.musical = record.musical;
            chord.ppq = record.ppq;
            chord.name = textOf (record);

            // notation-export 4: only changes.
            if (chords.empty() || chords.back().name != chord.name)
                chords.push_back (chord);

            break;
        }

        case Kind::meter:
        {
            CapturedMeter meter;
            meter.sample = record.sample;
            meter.musical = record.musical;
            meter.ppq = record.ppq;
            meter.bpm = record.value;
            meter.numerator = record.code;
            meter.denominator = record.code2;
            meters.push_back (meter);
            break;
        }

        case Kind::bassTechnique:
        {
            CapturedEvent captured;
            captured.sample = record.sample;
            captured.musical = record.musical;
            captured.ppq = record.ppq;
            captured.event = LuthierEvent::make (LuthierEventClass::bassTech, record.sample, 1);
            captured.event.set ("tech", textOf (record))
                          .setInt ("str", record.stringIndex)
                          .setReal ("pos", record.fret)
                          .setReal ("force", record.value)                             // SPEC-SWEEP BT-24
                          .setReal ("contact", record.code / 255.0);
            events.push_back (captured);
            break;
        }

        case Kind::slideBar:
        {
            CapturedEvent captured;
            captured.sample = record.sample;
            captured.musical = record.musical;
            captured.ppq = record.ppq;
            captured.event = LuthierEvent::make (LuthierEventClass::slideBar, record.sample);
            captured.event.setReal ("pos", record.value)
                          .set ("pressure", textOf (record));
            events.push_back (captured);
            break;
        }

        default:
            break;
    }
}

void PerformanceCapture::trimToRollingWindow()
{
    const auto cutoff = newestSample - (juce::int64) std::llround (rollingMinutes * 60.0 * sampleRate);

    if (cutoff <= 0)
        return;

    const auto before = notes.size();

    notes.erase (std::remove_if (notes.begin(), notes.end(),
                                 [cutoff] (const CapturedNote& n) { return ! n.isSounding() && n.endSample < cutoff; }),
                 notes.end());

    chords.erase (std::remove_if (chords.begin(), chords.end(),
                                  [cutoff] (const CapturedChord& c) { return c.sample < cutoff; }),
                  chords.end());

    events.erase (std::remove_if (events.begin(), events.end(),
                                  [cutoff] (const CapturedEvent& e) { return e.sample < cutoff; }),
                  events.end());

    // Keep the meter in force at the cutoff; the take still needs a tempo.
    size_t firstKept = 0;

    for (size_t i = 0; i < meters.size(); ++i)
        if (meters[i].sample < cutoff)
            firstKept = i;

    if (firstKept > 0)
        meters.erase (meters.begin(), meters.begin() + (std::ptrdiff_t) firstKept);

    if (notes.size() != before)
        rebuildSoundingIndex();
}

void PerformanceCapture::rebuildSoundingIndex()
{
    sounding.fill (-1);

    for (size_t i = 0; i < notes.size(); ++i)
        if (notes[i].isSounding())
            sounding[(size_t) notes[i].stringIndex] = (int) i;
}

juce::Range<juce::int64> PerformanceCapture::sampleRangeForPpq (juce::Range<double> ppq) const noexcept
{
    juce::int64 lo = std::numeric_limits<juce::int64>::max(), hi = std::numeric_limits<juce::int64>::min();

    for (const auto& n : notes)
        if (n.musical && n.startPpq >= ppq.getStart() && n.startPpq < ppq.getEnd())
        {
            lo = juce::jmin (lo, n.startSample);
            hi = juce::jmax (hi, n.isSounding() ? newestSample : n.endSample);
        }

    return hi > lo ? juce::Range<juce::int64> (lo, hi + 1) : juce::Range<juce::int64>();
}

void PerformanceCapture::markIn()
{
    markInSample = juce::jmax (newestSample, clockSample.load (std::memory_order_relaxed));

    if (markOutSample <= markInSample)
        markOutSample = -1;
}

void PerformanceCapture::markOut()
{
    markOutSample = juce::jmax (newestSample, clockSample.load (std::memory_order_relaxed));
}

void PerformanceCapture::clearMarks()
{
    markInSample = markOutSample = -1;
}

void PerformanceCapture::clearTake()
{
    clearMarks();
    notes.clear();
    chords.clear();
    meters.clear();
    events.clear();
    sounding.fill (-1);
    newestSample = 0;
    waitingForFirstNote = false;
}

//==============================================================================
PerformanceCapture::Timeline PerformanceCapture::makeTimeline (const CaptureScoreOptions& options,
                                                               std::vector<size_t>& selected) const
{
    Timeline timeline;
    timeline.sampleRate = sampleRate;

    timeline.cutoffSample = options.lastSeconds > 0.0
                              ? newestSample - (juce::int64) std::llround (options.lastSeconds * sampleRate)
                              : std::numeric_limits<juce::int64>::min();

    // MODEL-GAPS: the marked region and the current section.
    if (! options.sampleRange.isEmpty())
    {
        timeline.cutoffSample = juce::jmax (timeline.cutoffSample, options.sampleRange.getStart());
        timeline.endSample = options.sampleRange.getEnd();
    }

    timeline.ppqRange = options.ppqRange;

    selected.clear();

    for (size_t i = 0; i < notes.size(); ++i)
        if (timeline.includes (notes[i].musical, notes[i].startPpq, notes[i].startSample))
            selected.push_back (i);

    if (selected.empty())
        return timeline;

    const auto& first = notes[selected.front()];

    // The meter in force when the take starts.
    for (const auto& meter : meters)
    {
        if (meter.sample > first.startSample && &meter != &meters.front())
            break;

        timeline.bpm = meter.bpm;
        timeline.numerator = meter.numerator;
        timeline.denominator = meter.denominator;
    }

    timeline.musical = std::all_of (selected.begin(), selected.end(),
                                    [this] (size_t i) { return notes[i].musical; });

    if (! timeline.musical && options.freeTempoBpm > 0.0)
        timeline.bpm = options.freeTempoBpm;

    timeline.bpm = juce::jlimit (20.0, 300.0, timeline.bpm);
    timeline.anchorSample = first.startSample;

    if (timeline.musical)
    {
        const double barBeats = (double) timeline.numerator * 4.0 / (double) juce::jmax (1, timeline.denominator);
        timeline.originPpq = std::floor (first.startPpq / barBeats + 1.0e-9) * barBeats;
        timeline.anchorBeat = first.startPpq - timeline.originPpq;
    }

    return timeline;
}

void PerformanceCapture::toScore (PerformanceScore& score, const CaptureScoreOptions& options) const
{
    std::vector<size_t> selected;
    const auto timeline = makeTimeline (options, selected);

    score.clear();
    score.beginCapture (timeline.bpm, timeline.numerator, timeline.denominator);
    score.getMeta().title = options.title;

    auto& track = score.getTrack (0);
    track.numStrings = numStrings;
    track.capoFret = capoFret;
    track.tuning = scoreTuning;

    const double grid = juce::jmax (0.0, options.quantiseBeats);

    struct Placed
    {
        size_t index;
        double start, end;
    };

    std::vector<Placed> placed;
    placed.reserve (selected.size());

    double lastBeat = 0.0;

    for (const auto i : selected)
    {
        const auto& note = notes[i];

        double start = timeline.beatOf (note.musical, note.startPpq, note.startSample);
        double end = note.isSounding()
                       ? timeline.beatOf (false, 0.0, newestSample) - timeline.beatOf (false, 0.0, note.startSample) + start
                       : timeline.beatOf (note.musical, note.endPpq, note.endSample);

        start = juce::jmax (0.0, snap (start, grid, options.quantiseStrength));
        end = snap (end, grid, options.quantiseStrength);

        if (end <= start)
            end = start + (grid > 0.0 ? grid : 0.0625);

        placed.push_back ({ i, start, end });
        lastBeat = juce::jmax (lastBeat, end);
    }

    // Starts and ends in time order; an end before a start at the same beat,
    // so a note that ends as the next begins on its string closes first.
    struct Edge
    {
        double beat;
        bool isStart;
        size_t placedIndex;
    };

    std::vector<Edge> edges;
    edges.reserve (placed.size() * 2);

    for (size_t p = 0; p < placed.size(); ++p)
    {
        edges.push_back ({ placed[p].start, true, p });
        edges.push_back ({ placed[p].end, false, p });
    }

    std::stable_sort (edges.begin(), edges.end(), [] (const Edge& a, const Edge& b)
    {
        if (! juce::exactlyEqual (a.beat, b.beat))
            return a.beat < b.beat;

        return ! a.isStart && b.isStart;
    });

    std::array<int, kMaxStrings> onString;
    onString.fill (-1);

    for (const auto& edge : edges)
    {
        const auto& place = placed[edge.placedIndex];
        const auto& note = notes[place.index];
        const int s = juce::jlimit (0, kMaxStrings - 1, note.stringIndex);

        if (! edge.isStart)
        {
            if (onString[(size_t) s] == (int) edge.placedIndex)
            {
                score.noteEnded (s, edge.beat);
                onString[(size_t) s] = -1;
            }

            continue;
        }

        score.noteStarted (s, juce::jmax (0, juce::roundToInt (note.fret)), note.midiNote,
                           hzFor (note.midiNote), note.velocity, edge.beat);
        onString[(size_t) s] = (int) edge.placedIndex;

        // The bend, as notation-export 1's Bend { semitones, curve }.
        double peakCents = 0.0;

        for (const auto& point : note.bend)
            if (std::abs (point.second) > std::abs (peakCents))
                peakCents = point.second;

        const bool bent = std::abs (peakCents) >= 10.0;

        Type type = Type::bend;

        if (scoreTypeFor (note.technique, type) && ! (type == Type::bend && bent))
        {
            ScoreTechnique technique;
            technique.type = type;

            if (type == Type::artificialHarmonic)
                technique.value = note.harmonicPartial;

            score.addTechnique (s, technique);
        }

        for (const auto& markTechnique : note.marks)
            score.addTechnique (s, markTechnique);

        if (bent)
        {
            ScoreTechnique technique;
            technique.type = Type::bend;
            technique.value = peakCents / 100.0;

            const double length = note.isSounding()
                                    ? (double) juce::jmax ((juce::int64) 1, newestSample - note.startSample)
                                    : (double) juce::jmax ((juce::int64) 1, note.endSample - note.startSample);

            for (const auto& [sample, cents] : note.bend)
                technique.curve.emplace_back (juce::jlimit (0.0, 1.0, (double) (sample - note.startSample) / length),
                                              cents / 100.0);

            score.addTechnique (s, technique);
        }
    }

    int chordsWritten = 0;

    for (const auto& chord : chords)
        if (timeline.includes (chord.musical, chord.ppq, chord.sample) && ! selected.empty())
        {
            score.addChordSymbol (juce::jmax (0.0, timeline.beatOf (chord.musical, chord.ppq, chord.sample)), chord.name);
            ++chordsWritten;
        }

    /*  notation-export 4 (MODEL-GAPS): no chord track - a Mono-mode take - so
        the chords are extracted offline: per beat, the pitch classes sounding
        in it, matched against the detector's templates; written where the
        chord changes. */
    if (chordsWritten == 0 && options.extractChordsWhenMissing && ! placed.empty())
    {
        ChordDetector detector;
        ChordSymbol previous;

        for (int beat = 0; beat <= (int) std::ceil (lastBeat); ++beat)
        {
            std::array<int, 32> heard {};
            int count = 0;

            for (const auto& p : placed)
                if (p.start < beat + 1.0 && p.end > (double) beat && count < (int) heard.size())
                    heard[(size_t) count++] = notes[p.index].midiNote;

            if (count == 0)
                continue;

            const auto chord = detector.detect (heard.data(), count);

            if (chord.isKnown() && chord != previous)
            {
                score.addChordSymbol ((double) beat, chord.toString());
                previous = chord;
            }
        }
    }

    score.endCapture (lastBeat);

    // endCapture keeps the track's tuning, but say it again: it is the
    // instrument the take was played on.
    auto& finished = score.getTrack (0);
    finished.numStrings = numStrings;
    finished.capoFret = capoFret;
    finished.tuning = scoreTuning;
}

MidiPerformance PerformanceCapture::toPerformance (double rate, const CaptureScoreOptions& options) const
{
    PerformanceScore score;
    toScore (score, options);

    auto performance = MidiPerformance::fromScore (score, rate);
    performance.getMeta().title = options.title;

    std::vector<size_t> selected;
    const auto timeline = makeTimeline (options, selected);

    if (selected.empty())
        return performance;

    for (const auto& captured : events)
    {
        if (! timeline.includes (captured.musical, captured.ppq, captured.sample))
            continue;

        const double beat = juce::jmax (0.0, timeline.beatOf (captured.musical, captured.ppq, captured.sample));

        auto event = captured.event;
        event.sample = (juce::int64) std::llround (performance.beatToSample (beat));
        performance.addEvent (event);
    }

    return performance;
}

juce::String PerformanceCapture::renderLiveTab (int numBars, NotationExportOptions::SymbolDensity density) const
{
    PerformanceScore score;
    toScore (score);

    const int measures = (int) score.getTrack (0).measures.size();

    if (measures == 0 || notes.empty())
        return {};

    NotationExportOptions options;
    options.density = density;

    const int bars = juce::jlimit (1, 8, numBars);   // notation-export 3: 1-8 bars on screen

    return NotationExporter().renderAsciiTabWindow (score, juce::jmax (0, measures - bars), bars, options);
}

} // namespace luthier
