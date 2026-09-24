#include "TuneMidi.h"
#include "../Export/MidiPerformance.h"
#include "../Support/ThreadProbe.h"

#include <functional>

namespace luthier
{

using namespace tunetheory;

//==============================================================================
const char* getTunePartName (TunePart part) noexcept
{
    switch (part)
    {
        case TunePart::chords:        return "chords";
        case TunePart::melody:        return "melody";
        case TunePart::bass:          return "bass";
        case TunePart::pad:           return "pad";
        case TunePart::arpeggio:      return "arpeggio";
        case TunePart::countermelody: return "countermelody";
        case TunePart::percussion:    return "percussion";
        case TunePart::marker:        return "marker";
        case TunePart::numParts:      break;
    }

    return "unknown";
}

//==============================================================================
namespace
{
    constexpr double kEps = 1.0e-6;

    /** What goes first when events share a position: a note ending before one
        starting (so a repeated note re-strikes), and a controller that belongs
        to the ending note reset before the next note's is set. */
    enum Order
    {
        orderMarker = 0,
        orderNoteOff,
        orderControllerReset,
        orderControllerSet,
        orderNoteOn,
        orderInside
    };

    struct Pending
    {
        TuneEvent event;
        int order = 0;
        int sequence = 0;   // insertion order, so the sort is total and repeatable
    };

    int clampChannel (int channel) noexcept   { return juce::jlimit (1, 16, channel); }
    juce::uint8 velocityByte (int v) noexcept { return (juce::uint8) juce::jlimit (1, 127, v); }

    /** Collects events for one timeline build. */
    struct Builder
    {
        const Tune& tune;
        const TuneMidiOptions& options;
        std::vector<Pending> pending;

        Builder (const Tune& t, const TuneMidiOptions& o) : tune (t), options (o) {}

        void add (double ppq, const juce::MidiMessage& message, TunePart part, int span, int order)
        {
            Pending p;
            p.event.ppq = canonical (ppq);
            p.event.message = message;
            p.event.message.setTimeStamp (p.event.ppq);
            p.event.part = part;
            p.event.spanIndex = span;
            p.order = order;
            p.sequence = (int) pending.size();
            pending.push_back (p);
        }

        void cc (double ppq, int channel, int controller, int value, TunePart part, int span, int order)
        {
            add (ppq, juce::MidiMessage::controllerEvent (clampChannel (channel), controller,
                                                          juce::jlimit (0, 127, value)),
                 part, span, order);
        }

        void note (int channel, int pitch, int velocity, double on, double off, TunePart part, int span)
        {
            if (pitch < 0 || pitch > 127 || off <= on + kEps)
                return;

            const int ch = clampChannel (channel);
            add (on, juce::MidiMessage::noteOn (ch, pitch, velocityByte (velocity)), part, span, orderNoteOn);
            add (off, juce::MidiMessage::noteOff (ch, pitch), part, span, orderNoteOff);
        }

        /** 8 and the tune's swing: an offbeat eighth moves toward the triplet. */
        double swing (double beat) const noexcept
        {
            const double amount = juce::jlimit (0.0, 100.0, tune.meta.swingPercent) / 100.0;

            if (amount <= 0.0)
                return beat;

            const double fraction = beat - std::floor (beat);
            return std::abs (fraction - 0.5) < kEps ? beat + amount / 6.0 : beat;
        }

        /** One written note: resolves its pitch, applies articulation and
            swing, and adds its realism events. `base` is where the section
            occurrence starts; `length` where its notes stop. */
        void writtenNote (const MelodyNote& n, int pitch, NoteArticulation fallback, int channel,
                          TunePart part, int span, double base, double length, double velocityScale)
        {
            if (n.startBeat >= length - kEps)
                return;

            double start = swing (n.startBeat);
            double end = swing (juce::jmin (n.getEndBeat(), length));
            const auto articulation = n.articulation == NoteArticulation::inherit ? fallback : n.articulation;

            if (articulation == NoteArticulation::staccato)
                end = start + (end - start) * 0.5;

            end = juce::jmax (end, start + 0.015625);

            const double on = base + start;
            const double off = base + end;
            const int ch = clampChannel (channel);
            const int velocity = (int) std::lround ((double) n.velocity * velocityScale);

            if (options.includeRealism)
            {
                // Articulation, as the controllers MidiInterpreter maps by
                // default: CC67 palm mute, CC68 legato, CC64 held.
                int articulationCc = -1;

                if (articulation == NoteArticulation::palmMuted)     articulationCc = 67;
                else if (articulation == NoteArticulation::legato)   articulationCc = 68;
                else if (articulation == NoteArticulation::letRing)  articulationCc = 64;

                if (articulationCc >= 0)
                {
                    cc (on, ch, articulationCc, 127, part, span, orderControllerSet);
                    cc (off, ch, articulationCc, 0, part, span, orderControllerReset);
                }

                technique (n.technique, ch, on, off, part, span);
            }

            note (ch, pitch, velocity, on, off, part, span);
        }

        void technique (NoteTechnique t, int ch, double on, double off, TunePart part, int span)
        {
            if (t == NoteTechnique::none)
                return;

            if (options.realismTextMetas)
                add (on, juce::MidiMessage::textMetaEvent (1, juce::String ("LUTHIER: ")
                                                               + juce::String (getNoteTechniqueName (t)).toUpperCase()),
                     part, span, orderControllerSet);

            const double duration = off - on;

            switch (t)
            {
                case NoteTechnique::bend:
                {
                    // A whole-step bend over the first 30% of the note, held,
                    // released with the note.
                    const double range = juce::jmax (0.5, options.bendRangeSemitones);
                    const double full = juce::jlimit (0.0, 1.0, 2.0 / range);

                    for (int k = 1; k <= 4; ++k)
                    {
                        const int wheel = juce::jlimit (0, 16383, 8192 + (int) std::lround (full * 8191.0 * (double) k / 4.0));
                        add (on + duration * 0.3 * (double) k / 4.0, juce::MidiMessage::pitchWheel (ch, wheel),
                             part, span, orderInside);
                    }

                    add (off, juce::MidiMessage::pitchWheel (ch, 8192), part, span, orderControllerReset);
                    break;
                }

                case NoteTechnique::vibrato:
                    cc (on, ch, 1, 0, part, span, orderControllerSet);
                    cc (on + duration * 0.25, ch, 1, 90, part, span, orderInside);
                    cc (off, ch, 1, 0, part, span, orderControllerReset);
                    break;

                case NoteTechnique::slide:
                    cc (on, ch, 65, 127, part, span, orderControllerSet);
                    cc (off, ch, 65, 0, part, span, orderControllerReset);
                    break;

                case NoteTechnique::hammerOn:
                case NoteTechnique::pullOff:
                    cc (on, ch, 68, 127, part, span, orderControllerSet);
                    cc (off, ch, 68, 0, part, span, orderControllerReset);
                    break;

                case NoteTechnique::harmonic:
                    cc (on, ch, 73, 127, part, span, orderControllerSet);
                    cc (off, ch, 73, 0, part, span, orderControllerReset);
                    break;

                case NoteTechnique::none:
                case NoteTechnique::numTechniques:
                    break;
            }
        }
    };

    int layerChannel (const TuneMidiOptions& o, LayerType type) noexcept
    {
        return clampChannel (o.layerChannelBase + (int) type);
    }

    TunePart partFor (LayerType type) noexcept
    {
        switch (type)
        {
            case LayerType::pad:           return TunePart::pad;
            case LayerType::arpeggio:      return TunePart::arpeggio;
            case LayerType::countermelody: return TunePart::countermelody;
            case LayerType::percussion:    return TunePart::percussion;
            case LayerType::numTypes:      break;
        }

        return TunePart::pad;
    }
}

//==============================================================================
TuneTimeline TuneTimeline::build (const Tune& tune, const TuneMidiOptions& options)
{
    TuneTimeline timeline;
    Builder b (tune, options);

    const auto spans = tune.getPlayOrder();
    const double beatsPerBar = tune.getBeatsPerBar();
    const int tonic = tune.meta.keyTonic;
    const auto mode = tune.meta.mode;

    for (size_t si = 0; si < spans.size(); ++si)
    {
        const auto& span = spans[si];
        const int spanIndex = (int) si;
        const auto& section = tune.arrangement.sections[(size_t) span.sectionIndex];
        const auto& rhythm = tune.arrangement.sections[(size_t) tune.getRhythmSourceIndex (span.sectionIndex)];
        const double base = span.startBeat;
        const double length = span.lengthBeats;

        timeline.sections.push_back ({ base, length, spanIndex, span.sectionIndex, section.name });

        // ---- rhythm settings ---------------------------------------------------------
        TuneRhythmChange change;
        change.ppq = base;
        change.spanIndex = spanIndex;
        change.sectionIndex = span.sectionIndex;
        change.patternId = rhythm.rhythmPatternId;
        change.genreKitId = rhythm.genreKitId;
        change.rhythmOn = rhythm.rhythmOn;
        change.feel = rhythm.feel;
        change.strum = rhythm.strum;
        change.stateBoundary = section.stateBoundary;
        timeline.rhythmChanges.push_back (change);

        if (options.includeMarkers)
            b.add (base, juce::MidiMessage::textMetaEvent (6, section.name), TunePart::marker, spanIndex, orderMarker);

        // ---- chords ---------------------------------------------------------------------
        const auto chordSpans = resolveChordSpans (section, beatsPerBar);
        auto currentPattern = rhythm.rhythmPatternId;

        for (const auto& cs : chordSpans)
        {
            const auto& cell = section.chords[(size_t) cs.cellIndex];

            // 1.1 strum_override: the cell's own pattern, then back.
            const auto wanted = cell.strumOverride.isNotEmpty() ? cell.strumOverride : rhythm.rhythmPatternId;

            if (wanted != currentPattern)
            {
                auto cellChange = change;
                cellChange.ppq = canonical (base + cs.startBeat);
                cellChange.patternId = wanted;
                cellChange.stateBoundary = false;
                timeline.rhythmChanges.push_back (cellChange);
                currentPattern = wanted;
            }

            if (! options.includeChords)
                continue;

            const int velocity = cell.emphasis == ChordEmphasis::accent ? 118
                               : cell.emphasis == ChordEmphasis::ghost  ? 48 : 96;

            for (int pitch : voiceChord (cell, options.chordLowestNote))
                b.note (options.chordChannel, pitch, velocity, base + cs.startBeat, base + cs.endBeat,
                        TunePart::chords, spanIndex);
        }

        auto chordAt = [&] (double beat) -> const ChordCell*
        {
            const int i = findChordSpanAt (chordSpans, beat);
            return i >= 0 ? &section.chords[(size_t) chordSpans[(size_t) i].cellIndex] : nullptr;
        };

        auto resolve = [&] (const MelodyNote& n)
        {
            return resolveMelodyPitch (n.pitch, chordAt (n.startBeat), tonic, mode);
        };

        // ---- melody -----------------------------------------------------------------------
        if (options.includeMelody && section.melody.has_value())
        {
            const auto& track = *section.melody;
            const bool improvise = track.source == MelodySource::improvise;
            timeline.improvises = timeline.improvises || improvise;

            // 4.4: each loop pass, and each occurrence within it, plays a new line.
            const auto written = improvise ? generateImprovisedPass (tune, span.sectionIndex,
                                                                     options.improvisePass * 1000 + spanIndex)
                                           : track.notes;

            for (const auto& n : applyMelodyStyle (written, section.style))
                b.writtenNote (n, resolve (n), track.articulationDefault, options.melodyChannel,
                               TunePart::melody, spanIndex, base, length, 1.0);
        }

        // ---- bass -------------------------------------------------------------------------
        if (options.includeBass)
            for (const auto& n : generateBassLine (tune, span.sectionIndex))
                b.writtenNote (n, resolve (n), NoteArticulation::natural, options.bassChannel,
                               TunePart::bass, spanIndex, base, length, 1.0);

        // ---- layers (7) --------------------------------------------------------------------
        if (! options.includeLayers)
            continue;

        for (const auto& layer : section.layers)
        {
            if (! layer.enabled)
                continue;

            const int ch = layerChannel (options, layer.type);
            const double volume = juce::jlimit (0.0, 1.0, layer.volume);

            // Own on/off, volume and pan (7): the level and pan go out as the
            // channel's CC7 and CC10 at every section start.
            b.cc (base, ch, 7, (int) std::lround (volume * 127.0), partFor (layer.type), spanIndex, orderControllerSet);
            b.cc (base, ch, 10, (int) std::lround ((juce::jlimit (-1.0, 1.0, layer.pan) + 1.0) * 63.5),
                  partFor (layer.type), spanIndex, orderControllerSet);

            // A section without chords still has a key.
            std::vector<ChordSpan> harmony = chordSpans;
            const auto tonicChord = makeTonicChord (tonic, mode);

            if (harmony.empty())
                harmony.push_back ({ -1, 0.0, length });

            auto cellOf = [&] (const ChordSpan& cs) -> const ChordCell&
            {
                return cs.cellIndex >= 0 ? section.chords[(size_t) cs.cellIndex] : tonicChord;
            };

            switch (layer.type)
            {
                case LayerType::pad:
                    // Sustained chord tones, soft (7).
                    for (const auto& cs : harmony)
                        for (int pitch : voiceChord (cellOf (cs), 55, 4))
                            b.note (ch, pitch, (int) std::lround (70.0 * volume), base + cs.startBeat,
                                    base + cs.endBeat, TunePart::pad, spanIndex);
                    break;

                case LayerType::arpeggio:
                {
                    // Up and down the chord in eighths. `patternId` names a
                    // fingerpick pattern for a caller that routes this layer
                    // through the rhythm engine instead.
                    static const int shape[6] = { 0, 1, 2, 3, 2, 1 };

                    for (const auto& cs : harmony)
                    {
                        const auto tones = voiceChord (cellOf (cs), 52, 4);

                        if (tones.empty())
                            continue;

                        int k = 0;

                        for (double t = cs.startBeat; t < cs.endBeat - kEps; t = canonical (cs.startBeat + 0.5 * (double) (++k)))
                        {
                            const int pitch = tones[(size_t) (shape[k % 6] % (int) tones.size())];
                            const double start = b.swing (t);
                            const double end = b.swing (juce::jmin (t + 0.5, cs.endBeat));

                            b.note (ch, pitch, (int) std::lround (80.0 * volume), base + start, base + end,
                                    TunePart::arpeggio, spanIndex);
                        }
                    }
                    break;
                }

                case LayerType::countermelody:
                    for (const auto& n : layer.notes)
                        b.writtenNote (n, resolve (n), NoteArticulation::natural, ch, TunePart::countermelody,
                                       spanIndex, base, length, volume);
                    break;

                case LayerType::percussion:
                {
                    // Chuck and palm-mute noise as a groove (7): muted hits on
                    // the backbeat, ghosted offbeats, palm mute held on.
                    b.cc (base, ch, 67, 127, TunePart::percussion, spanIndex, orderControllerSet);

                    int k = 0;

                    for (double t = 0.0; t < length - kEps; t = canonical (0.5 * (double) (++k)))
                    {
                        const auto* chord = chordAt (t);
                        const auto voicing = voiceChord (chord != nullptr ? *chord : tonicChord, 40, 1);

                        if (voicing.empty())
                            continue;

                        const double inBar = std::fmod (t, beatsPerBar);
                        const bool offbeat = std::abs (inBar - std::floor (inBar) - 0.5) < kEps;
                        const bool backbeat = ! offbeat && ((int) std::lround (inBar) % 2) == 1;

                        if (! offbeat && ! backbeat)
                            continue;

                        const double start = b.swing (t);
                        b.note (ch, voicing.front(), (int) std::lround ((backbeat ? 90.0 : 40.0) * volume),
                                base + start, base + start + 0.25, TunePart::percussion, spanIndex);
                    }

                    b.cc (base + length, ch, 67, 0, TunePart::percussion, spanIndex, orderControllerReset);
                    break;
                }

                case LayerType::numTypes:
                    break;
            }
        }
    }

    // A total, repeatable order: position, then Order, then insertion.
    std::sort (b.pending.begin(), b.pending.end(), [] (const Pending& x, const Pending& y)
    {
        if (x.event.ppq != y.event.ppq) return x.event.ppq < y.event.ppq;
        if (x.order != y.order)         return x.order < y.order;
        return x.sequence < y.sequence;
    });

    timeline.events.reserve (b.pending.size());

    for (auto& p : b.pending)
        timeline.events.push_back (std::move (p.event));

    timeline.lengthPpq = tune.getTotalBeats();
    return timeline;
}

int TuneTimeline::findSpanAt (double ppq, bool loop) const noexcept
{
    if (sections.empty() || lengthPpq <= 0.0)
        return -1;

    double position = ppq;

    if (loop)
        position = std::fmod (juce::jmax (0.0, ppq), lengthPpq);

    for (const auto& s : sections)
        if (position >= s.ppq - kEps && position < s.ppq + s.lengthPpq - kEps)
            return s.spanIndex;

    return -1;
}

juce::MidiMessageSequence TuneTimeline::toSequence() const
{
    juce::MidiMessageSequence sequence;

    for (const auto& e : events)
        sequence.addEvent (e.message, 0.0);

    sequence.updateMatchedPairs();
    return sequence;
}

juce::MidiMessageSequence TuneTimeline::toSequence (TunePart part) const
{
    juce::MidiMessageSequence sequence;

    for (const auto& e : events)
        if (e.part == part)
            sequence.addEvent (e.message, 0.0);

    sequence.updateMatchedPairs();
    return sequence;
}

void TuneTimeline::renderBlock (double fromPpq, double toPpq, double samplesPerQuarter, int numSamples,
                                bool loop, juce::MidiBuffer& out) const noexcept
{
    if (numSamples <= 0 || samplesPerQuarter <= 0.0)
        return;

    forEachEventInRange (fromPpq, toPpq, loop, [&] (const TuneEvent& e, double at)
    {
        if (e.message.isMetaEvent())
            return;

        const auto offset = (int) std::llround ((at - fromPpq) * samplesPerQuarter);
        out.addEvent (e.message, juce::jlimit (0, numSamples - 1, offset));
    });
}

//==============================================================================
juce::MidiFile buildTuneMidiFile (const Tune& tune, const TuneMidiFileOptions& options)
{
    auto midiOptions = options.midi;
    midiOptions.includeMarkers = false;   // they go in the meta track instead
    midiOptions.realismTextMetas = midiOptions.includeRealism;

    const auto timeline = TuneTimeline::build (tune, midiOptions);
    const int ticksPerQuarter = juce::jlimit (96, 3840, options.ticksPerQuarter);

    auto ticks = [ticksPerQuarter] (double ppq) { return (double) std::llround (ppq * (double) ticksPerQuarter); };

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ticksPerQuarter);

    // ---- track 0: title, tempo map, time and key signature, markers (midi-export 1)
    {
        juce::MidiMessageSequence meta;

        meta.addEvent (juce::MidiMessage::textMetaEvent (3, tune.meta.title), 0.0);

        if (tune.meta.artist.isNotEmpty())
            meta.addEvent (juce::MidiMessage::textMetaEvent (1, "Artist: " + tune.meta.artist), 0.0);

        meta.addEvent (juce::MidiMessage::tempoMetaEvent (
                           (int) std::lround (60000000.0 / juce::jmax (1.0, tune.meta.tempoBpm))), 0.0);
        meta.addEvent (juce::MidiMessage::timeSignatureMetaEvent (tune.meta.timeSigNumerator,
                                                                  tune.meta.timeSigDenominator), 0.0);
        meta.addEvent (juce::MidiMessage::keySignatureMetaEvent (
                           keySignatureAccidentals (tune.meta.keyTonic, tune.meta.mode),
                           tune.meta.mode == TuneMode::aeolian), 0.0);

        for (const auto& s : timeline.getSections())
            meta.addEvent (juce::MidiMessage::textMetaEvent (6, s.name), ticks (s.ppq));

        file.addTrack (meta);
    }

    // ---- instrument tracks ----------------------------------------------------------
    struct TrackSpec
    {
        juce::String name;
        std::function<bool (const TuneEvent&)> accepts;
    };

    std::vector<TrackSpec> tracks;

    using Split = TuneMidiFileOptions::TrackSplit;

    switch (options.split)
    {
        case Split::single:
            tracks.push_back ({ "Luthier", [] (const TuneEvent&) { return true; } });
            break;

        case Split::perSection:
        {
            const auto& markers = timeline.getSections();

            for (int si = 0; si < tune.getNumSections(); ++si)
            {
                tracks.push_back ({ tune.arrangement.sections[(size_t) si].name,
                                    [si, &markers] (const TuneEvent& e)
                                    {
                                        return juce::isPositiveAndBelow (e.spanIndex, (int) markers.size())
                                                 && markers[(size_t) e.spanIndex].sectionIndex == si;
                                    } });
            }
            break;
        }

        case Split::perInstrument:
        case Split::perString:
            tracks.push_back ({ "Guitar", [] (const TuneEvent& e) { return e.part != TunePart::bass; } });
            tracks.push_back ({ "Bass",   [] (const TuneEvent& e) { return e.part == TunePart::bass; } });
            break;
    }

    for (const auto& spec : tracks)
    {
        juce::MidiMessageSequence sequence;
        uint32_t channels = 0;
        bool hasNotes = false;

        for (const auto& e : timeline.getEvents())
        {
            if (! spec.accepts (e))
                continue;

            if (e.message.getChannel() > 0)
                channels |= (uint32_t) (1u << e.message.getChannel());

            hasNotes = hasNotes || e.message.isNoteOn();
        }

        if (! hasNotes)
            continue;

        sequence.addEvent (juce::MidiMessage::textMetaEvent (3, spec.name), 0.0);

        // midi-export 3: the bend range, by RPN 0, at the top of every track.
        for (int ch = 1; ch <= 16; ++ch)
        {
            if ((channels & (uint32_t) (1u << ch)) == 0)
                continue;

            sequence.addEvent (juce::MidiMessage::controllerEvent (ch, 101, 0), 0.0);
            sequence.addEvent (juce::MidiMessage::controllerEvent (ch, 100, 0), 0.0);
            sequence.addEvent (juce::MidiMessage::controllerEvent (ch, 6,
                                   juce::jlimit (1, 24, (int) std::lround (midiOptions.bendRangeSemitones))), 0.0);
            sequence.addEvent (juce::MidiMessage::controllerEvent (ch, 38, 0), 0.0);
        }

        for (const auto& e : timeline.getEvents())
        {
            if (! spec.accepts (e))
                continue;

            auto message = e.message;
            message.setTimeStamp (ticks (e.ppq));
            sequence.addEvent (message, 0.0);
        }

        sequence.updateMatchedPairs();
        file.addTrack (sequence);
    }

    return file;
}

bool writeTuneMidiFile (const Tune& tune, const juce::File& destination,
                        const TuneMidiFileOptions& options, juce::String& error)
{
    ThreadProbe::noteFileAccess();

    const auto file = buildTuneMidiFile (tune, options);

    if (! destination.getParentDirectory().createDirectory().wasOk())
    {
        error = destination.getParentDirectory().getFullPathName() + ": could not create the folder";
        return false;
    }

    const auto temp = destination.getSiblingFile (destination.getFileName() + ".tmp");
    temp.deleteFile();

    bool written = false;

    {
        juce::FileOutputStream out (temp);

        if (out.openedOk())
        {
            written = file.writeTo (out, 1);
            out.flush();
            written = written && out.getStatus().wasOk();
        }
    }

    if (! written || ! temp.replaceFileIn (destination))
    {
        temp.deleteFile();
        error = destination.getFullPathName() + ": could not be written";
        return false;
    }

    return true;
}

//==============================================================================
MidiPerformance buildTunePerformance (const Tune& tune, double sampleRate, const TuneMidiOptions& options)
{
    auto midiOptions = options;
    midiOptions.includeMarkers = false;    // sections are SECTION events here
    midiOptions.realismTextMetas = false;  // the profile writes its own texts

    const auto timeline = TuneTimeline::build (tune, midiOptions);

    MidiPerformance performance (sampleRate > 0.0 ? sampleRate : 48000.0);
    performance.setTempo (juce::jmax (1.0, tune.meta.tempoBpm));
    performance.setTimeSignature (tune.meta.timeSigNumerator, tune.meta.timeSigDenominator);

    auto& meta = performance.getMeta();
    meta.title = tune.meta.title;
    meta.keySharpsOrFlats = keySignatureAccidentals (tune.meta.keyTonic, tune.meta.mode);
    meta.keyIsMinor = modeHasMinorThird (tune.meta.mode);
    meta.pitchBendRangeSemitones = midiOptions.bendRangeSemitones;
    meta.partNames = { "Guitar", "Bass" };

    auto sampleOf = [&performance] (double ppq)
    {
        return (juce::int64) std::llround (performance.beatToSample (ppq));
    };

    int index = 0;

    for (const auto& s : timeline.getSections())
    {
        auto section = LuthierEvent::make (LuthierEventClass::section, sampleOf (s.ppq));
        section.set ("name", s.name).setInt ("index", index++).set ("edge", "start");
        performance.addEvent (section);
    }

    for (const auto& e : timeline.getEvents())
    {
        if (! MidiPerformance::isChannelVoiceMessage (e.message))
            continue;

        performance.addMessage (sampleOf (e.ppq), e.message, e.part == TunePart::bass ? 1 : 0);
    }

    return performance;
}

//==============================================================================
void buildTuneScore (const Tune& tune, PerformanceScore& score, const TuneScoreOptions& options)
{
    score.clear();
    score.beginCapture (tune.meta.tempoBpm, tune.meta.timeSigNumerator, tune.meta.timeSigDenominator);

    auto& meta = score.getMeta();
    meta.title = tune.meta.title;
    meta.artist = tune.meta.artist;
    meta.key = formatKey (tune.meta.keyTonic, tune.meta.mode);

    const auto& track = score.getTrack (0);
    const int numStrings = juce::jlimit (1, kMaxStrings, track.numStrings);
    const double beatsPerBar = tune.getBeatsPerBar();
    const bool flats = tune.preferFlats();

    const auto spans = tune.getPlayOrder();
    int handPosition = 0;

    for (size_t si = 0; si < spans.size(); ++si)
    {
        const auto& span = spans[si];
        const auto& section = tune.arrangement.sections[(size_t) span.sectionIndex];
        const double base = span.startBeat;
        const auto chordSpans = resolveChordSpans (section, beatsPerBar);

        if (options.includeChordSymbols)
            for (const auto& cs : chordSpans)
                score.addChordSymbol (base + cs.startBeat, getChordSymbol (section.chords[(size_t) cs.cellIndex], flats));

        if (! section.melody.has_value())
            continue;

        const auto& melody = *section.melody;
        auto notes = melody.source == MelodySource::improvise
                       ? generateImprovisedPass (tune, span.sectionIndex, (int) si)
                       : melody.notes;

        std::stable_sort (notes.begin(), notes.end(),
                          [] (const MelodyNote& a, const MelodyNote& b) { return a.startBeat < b.startBeat; });

        std::array<double, kMaxStrings> busyUntil {};
        busyUntil.fill (-1.0);

        for (const auto& n : notes)
        {
            if (n.startBeat >= span.lengthBeats - kEps)
                continue;

            const int ci = findChordSpanAt (chordSpans, n.startBeat);
            const auto* chord = ci >= 0 ? &section.chords[(size_t) chordSpans[(size_t) ci].cellIndex] : nullptr;
            const int pitch = resolveMelodyPitch (n.pitch, chord, tune.meta.keyTonic, tune.meta.mode);

            const double start = base + n.startBeat;
            const double end = base + juce::jmin (n.getEndBeat(), span.lengthBeats);

            // The free string whose fret is nearest where the hand already is.
            int bestString = -1, bestFret = 0, bestCost = 1000;

            for (int s = 0; s < numStrings; ++s)
            {
                const int fret = pitch - track.tuning[(size_t) s];

                if (fret < 0 || fret > options.maxFret || busyUntil[(size_t) s] > start + kEps)
                    continue;

                const int cost = std::abs (fret - handPosition) + (fret > 12 ? 4 : 0);

                if (cost < bestCost)
                {
                    bestCost = cost;
                    bestString = s;
                    bestFret = fret;
                }
            }

            if (bestString < 0)
                continue;   // out of the instrument's range: notation cannot fret it

            const double hz = 440.0 * std::pow (2.0, ((double) pitch - 69.0) / 12.0);
            score.noteStarted (bestString, bestFret, pitch, hz, (double) n.velocity / 127.0, start);

            auto addTechnique = [&] (ScoreTechnique::Type type, double value)
            {
                ScoreTechnique t;
                t.type = type;
                t.value = value;
                score.addTechnique (bestString, t);
            };

            switch (n.technique)
            {
                case NoteTechnique::bend:     addTechnique (ScoreTechnique::Type::bend, 2.0); break;
                case NoteTechnique::slide:    addTechnique (ScoreTechnique::Type::slideLegato, 0.0); break;
                case NoteTechnique::hammerOn: addTechnique (ScoreTechnique::Type::hammerOn, 0.0); break;
                case NoteTechnique::pullOff:  addTechnique (ScoreTechnique::Type::pullOff, 0.0); break;
                case NoteTechnique::vibrato:  addTechnique (ScoreTechnique::Type::vibrato, 0.0); break;
                case NoteTechnique::harmonic: addTechnique (ScoreTechnique::Type::naturalHarmonic, 0.0); break;
                case NoteTechnique::none:
                case NoteTechnique::numTechniques: break;
            }

            const auto articulation = n.articulation == NoteArticulation::inherit ? melody.articulationDefault
                                                                                  : n.articulation;

            if (articulation == NoteArticulation::staccato)       addTechnique (ScoreTechnique::Type::staccato, 0.0);
            else if (articulation == NoteArticulation::palmMuted) addTechnique (ScoreTechnique::Type::palmMute, 1.0);
            else if (articulation == NoteArticulation::letRing)   addTechnique (ScoreTechnique::Type::letRing, 0.0);

            score.noteEnded (bestString, end);
            busyUntil[(size_t) bestString] = end;
            handPosition = bestFret;
        }
    }

    score.endCapture (tune.getTotalBeats());

    // 9.3: "Section headings preserved".
    auto& scoreTrack = score.getTrack (0);

    for (const auto& span : spans)
    {
        const int measure = (int) std::floor (span.startBeat / beatsPerBar + kEps);

        if (juce::isPositiveAndBelow (measure, (int) scoreTrack.measures.size()))
            scoreTrack.measures[(size_t) measure].sectionName = tune.arrangement.sections[(size_t) span.sectionIndex].name;
    }
}

} // namespace luthier
