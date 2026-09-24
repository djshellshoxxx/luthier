#include "TuneImport.h"
#include "TuneHarmony.h"
#include "../Support/ThreadProbe.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace luthier
{

using namespace tunetheory;

namespace
{
    constexpr double kEps = 1.0e-6;

    /** Onsets this close count as one strum: an eighth of a beat is 62 ms at
        120 bpm, twice ChordDetector's burst window, so a lazily strummed chord
        is still one chord and a fast arpeggio is still notes. */
    constexpr double kStrumWindowBeats = 0.125;

    /** Two tempo metas closer than this are the same tempo written twice. */
    constexpr double kTempoTolerance = 0.01;

    //==========================================================================
    struct RawNote
    {
        double start = 0.0, end = 0.0;    ///< Quarter notes from the file's start.
        int pitch = 60, velocity = 100;
        NoteArticulation articulation = NoteArticulation::inherit;
        NoteTechnique technique = NoteTechnique::none;
    };

    /** The notes of one track on one channel: the unit the importer classifies.
        A track that plays two channels is two parts. */
    struct Part
    {
        int track = 0, channel = 1;
        juce::String name;
        std::vector<RawNote> notes;
        int volumeCc = -1, panCc = -1;    ///< The first CC7 / CC10 seen, or -1.

        juce::String describe() const
        {
            return name.isNotEmpty() ? "'" + name + "'"
                                     : "track " + juce::String (track + 1) + " channel " + juce::String (channel);
        }

        double meanPitch() const
        {
            if (notes.empty())
                return 60.0;

            double sum = 0.0;

            for (const auto& n : notes)
                sum += (double) n.pitch;

            return sum / (double) notes.size();
        }

        /** True when most notes start with two or more others sounding: a
            chord track, strummed or block. Notes are in start order. */
        bool isPolyphonic() const
        {
            int chordal = 0;

            for (size_t i = 0; i < notes.size(); ++i)
            {
                const double at = notes[i].start + kStrumWindowBeats;
                int sounding = 0;

                // Backwards over anything that started earlier and may still
                // sound, forwards over the rest of this strum.
                for (size_t j = i + 1; j-- > 0;)
                {
                    if (notes[j].start < notes[i].start - 16.0)
                        break;

                    if (notes[j].end > at + kEps)
                        ++sounding;
                }

                for (size_t j = i + 1; j < notes.size() && notes[j].start <= at + kEps; ++j)
                    if (notes[j].end > at + kEps)
                        ++sounding;

                if (sounding >= 3)
                    ++chordal;
            }

            return ! notes.empty() && chordal * 2 >= (int) notes.size();
        }
    };

    struct Marker
    {
        double ppq = 0.0;
        juce::String text;
    };

    /** A chord symbol found in a text meta: some files carry their chart that way. */
    struct ChartEntry
    {
        double ppq = 0.0;
        ChordCell cell;
    };

    /** Everything read from the file before any decision is made about it. */
    struct Parsed
    {
        juce::String title, artist;
        std::vector<double> tempos;               ///< Distinct, in file order.
        int timeSigNumerator = 0, timeSigDenominator = 0;
        bool hasKey = false, keyMinor = false;
        int keyAccidentals = 0;
        std::vector<Marker> markers;
        std::vector<Part> parts;
        std::vector<ChartEntry> chart;
        juce::StringArray trackNames;
        bool luthierMetas = false;               ///< Saw a "LUTHIER: ..." technique meta.
        double lastNoteEnd = 0.0;

        int numNotes() const
        {
            int n = 0;

            for (const auto& p : parts)
                n += (int) p.notes.size();

            return n;
        }
    };

    //==========================================================================
    /** "LUTHIER: HAMMER_ON" (TuneMidi writes getNoteTechniqueName upper-cased). */
    NoteTechnique techniqueFromMeta (const juce::String& text)
    {
        if (! text.startsWithIgnoreCase ("LUTHIER:"))
            return NoteTechnique::none;

        NoteTechnique technique = NoteTechnique::none;
        const auto name = text.fromFirstOccurrenceOf (":", false, false).trim().toLowerCase();
        return parseNoteTechnique (name, technique) ? technique : NoteTechnique::none;
    }

    void parseTrack (const juce::MidiMessageSequence& track, int trackIndex, double ticksPerQuarter, Parsed& p)
    {
        juce::String trackName;
        std::map<int, size_t> partByChannel;

        struct CcState
        {
            int volume = -1, pan = -1, held = 0, palmMute = 0, legato = 0, slide = 0, harmonic = 0;
            double modWheelSetAt = -1.0;   ///< When CC1 was last written (vibrato writes it at the note-on).
        };
        std::array<CcState, 17> cc {};

        /*  TuneMidi's "LUTHIER: BEND" metas sit at the note-on's tick but carry
            no channel, and in a single-track file the chord notes share that
            tick. So a meta waits here until a note-on whose channel's
            controllers agree with it: CC65 for a slide, CC73 for a harmonic,
            CC68 for a hammer-on or pull-off, CC1 for vibrato. A bend has no
            controller at the onset and goes to the first note there without
            other evidence. */
        struct PendingTechnique { double ppq; NoteTechnique technique; };
        std::vector<PendingTechnique> pending;

        auto partFor = [&] (int channel) -> Part&
        {
            auto found = partByChannel.find (channel);

            if (found != partByChannel.end())
                return p.parts[found->second];

            Part part;
            part.track = trackIndex;
            part.channel = channel;
            part.name = trackName;
            part.volumeCc = cc[(size_t) channel].volume;
            part.panCc = cc[(size_t) channel].pan;
            partByChannel[channel] = p.parts.size();
            p.parts.push_back (part);
            return p.parts.back();
        };

        for (int i = 0; i < track.getNumEvents(); ++i)
        {
            const auto& m = track.getEventPointer (i)->message;
            const double ppq = m.getTimeStamp() / ticksPerQuarter;

            if (m.isMetaEvent())
            {
                if (m.isTempoMetaEvent())
                {
                    const double bpm = 60.0 / juce::jmax (1.0e-6, m.getTempoSecondsPerQuarterNote());

                    if (p.tempos.empty() || std::abs (p.tempos.back() - bpm) > kTempoTolerance)
                        p.tempos.push_back (bpm);
                }
                else if (m.isTimeSignatureMetaEvent())
                {
                    if (p.timeSigNumerator == 0)
                        m.getTimeSignatureInfo (p.timeSigNumerator, p.timeSigDenominator);
                }
                else if (m.isKeySignatureMetaEvent())
                {
                    if (! p.hasKey)
                    {
                        p.hasKey = true;
                        p.keyAccidentals = m.getKeySignatureNumberOfSharpsOrFlats();
                        p.keyMinor = ! m.isKeySignatureMajorKey();
                    }
                }
                else if (m.isTrackNameEvent())
                {
                    const auto text = m.getTextFromTextMetaEvent().trim();

                    if (trackName.isEmpty())
                        trackName = text;

                    // The meta track's name is the title (midi-export 1); a
                    // format-0 file's only track names both, which is fine.
                    if (trackIndex == 0 && p.title.isEmpty())
                        p.title = text;

                    // A part created before its name arrived (rare) picks it up.
                    for (auto& [channel, index] : partByChannel)
                        if (p.parts[index].name.isEmpty())
                            p.parts[index].name = trackName;
                }
                else if (m.isTextMetaEvent())
                {
                    const auto type = m.getMetaEventType();
                    const auto text = m.getTextFromTextMetaEvent().trim();

                    if (type == 6)
                    {
                        p.markers.push_back ({ ppq, text });
                    }
                    else if (type == 1 && text.startsWith ("Artist: "))
                    {
                        if (p.artist.isEmpty())
                            p.artist = text.fromFirstOccurrenceOf ("Artist: ", false, false);
                    }
                    else if (const auto technique = techniqueFromMeta (text); technique != NoteTechnique::none)
                    {
                        p.luthierMetas = true;
                        pending.push_back ({ ppq, technique });
                    }
                    else if (type == 1 && text.isNotEmpty())
                    {
                        // A chord chart as text events. Text only, not lyrics:
                        // a sung "a" is not an A major.
                        ChordCell cell;
                        ProgressionError error = ProgressionError::none;

                        if (parseChordSymbol (text, cell, error))
                            p.chart.push_back ({ ppq, cell });
                    }
                }

                continue;
            }

            const int channel = m.getChannel();

            if (channel < 1 || channel > 16)
                continue;

            auto& state = cc[(size_t) channel];

            if (m.isController())
            {
                const int value = m.getControllerValue();

                switch (m.getControllerNumber())
                {
                    case 7:  if (state.volume < 0) state.volume = value; break;
                    case 10: if (state.pan < 0) state.pan = value; break;
                    case 1:  state.modWheelSetAt = ppq; break;
                    case 64: state.held = value; break;
                    case 65: state.slide = value; break;
                    case 67: state.palmMute = value; break;
                    case 68: state.legato = value; break;
                    case 73: state.harmonic = value; break;
                    default: break;
                }

                if (auto found = partByChannel.find (channel); found != partByChannel.end())
                {
                    auto& part = p.parts[found->second];

                    if (part.volumeCc < 0) part.volumeCc = state.volume;
                    if (part.panCc < 0)    part.panCc = state.pan;
                }

                continue;
            }

            if (! m.isNoteOn())
                continue;

            const double endTick = track.getTimeOfMatchingKeyUp (i);

            if (endTick <= m.getTimeStamp())
                continue;   // no note-off, or a zero-length note

            RawNote n;
            n.start = ppq;
            n.end = endTick / ticksPerQuarter;
            n.pitch = m.getNoteNumber();
            n.velocity = juce::jlimit (1, 127, (int) m.getVelocity());

            // The technique meta TuneMidi wrote just ahead of the note-on, if
            // this note's channel bears it out.
            pending.erase (std::remove_if (pending.begin(), pending.end(),
                                           [ppq] (const PendingTechnique& t) { return t.ppq < ppq - kEps; }),
                           pending.end());

            const bool slideCc = state.slide >= 64;
            const bool harmonicCc = state.harmonic >= 64;
            const bool legatoCc = state.legato >= 64;
            const bool modWheelHere = std::abs (state.modWheelSetAt - ppq) < kEps;

            for (auto it = pending.begin(); it != pending.end(); ++it)
            {
                if (std::abs (it->ppq - ppq) >= kEps)
                    continue;

                bool fits = false;

                switch (it->technique)
                {
                    case NoteTechnique::slide:     fits = slideCc; break;
                    case NoteTechnique::harmonic:  fits = harmonicCc; break;
                    case NoteTechnique::hammerOn:
                    case NoteTechnique::pullOff:   fits = legatoCc; break;
                    case NoteTechnique::vibrato:   fits = modWheelHere; break;
                    case NoteTechnique::bend:      fits = ! (slideCc || harmonicCc || legatoCc || modWheelHere); break;
                    case NoteTechnique::none:
                    case NoteTechnique::numTechniques: break;
                }

                if (fits)
                {
                    n.technique = it->technique;
                    pending.erase (it);
                    break;
                }
            }

            // The controllers TuneMidi wrote for the articulation. A hammer-on
            // or pull-off is CC68 too, the legato controller; the technique
            // explains it, so it is not also an articulation.
            const bool legatoIsTechnique = n.technique == NoteTechnique::hammerOn || n.technique == NoteTechnique::pullOff;

            if (state.palmMute >= 64)                     n.articulation = NoteArticulation::palmMuted;
            else if (legatoCc && ! legatoIsTechnique)     n.articulation = NoteArticulation::legato;
            else if (state.held >= 64)                    n.articulation = NoteArticulation::letRing;

            partFor (channel).notes.push_back (n);
            p.lastNoteEnd = juce::jmax (p.lastNoteEnd, n.end);
        }

        if (trackName.isNotEmpty())
            p.trackNames.add (trackName);
    }

    //==========================================================================
    /** A file Luthier wrote (TuneMidi): instrument tracks named Guitar / Bass
        (or Luthier for a single track), section-named tracks for a per-section
        split, or its technique metas. */
    bool looksLikeLuthierLayout (const Parsed& p)
    {
        if (p.luthierMetas)
            return true;

        for (const auto& name : p.trackNames)
            if (name.equalsIgnoreCase ("Luthier") || name.equalsIgnoreCase ("Guitar") || name.equalsIgnoreCase ("Bass"))
                return true;

        if (p.markers.empty())
            return false;

        bool anyNamed = false;

        for (const auto& part : p.parts)
        {
            if (part.name.isEmpty() || part.notes.empty())
                continue;

            anyNamed = true;
            bool isMarker = false;

            for (const auto& marker : p.markers)
                isMarker = isMarker || marker.text == part.name;

            if (! isMarker)
                return false;
        }

        return anyNamed;
    }

    using PartList = std::vector<const Part*>;

    struct Roles
    {
        PartList chords, melody, bass, extras;
        std::array<PartList, (size_t) LayerType::numTypes> layers;
        bool luthierLayout = false;
        bool chordsFromChart = false;
    };

    Roles classify (const Parsed& p, const TuneImportOptions& options, juce::StringArray& warnings)
    {
        Roles roles;
        roles.luthierLayout = looksLikeLuthierLayout (p);

        if (roles.luthierLayout)
        {
            const auto& midi = options.midi;

            for (const auto& part : p.parts)
            {
                if (part.notes.empty())
                    continue;

                const int layer = part.channel - midi.layerChannelBase;

                if (part.channel == midi.chordChannel)       roles.chords.push_back (&part);
                else if (part.channel == midi.melodyChannel) roles.melody.push_back (&part);
                else if (part.channel == midi.bassChannel)   roles.bass.push_back (&part);
                else if (juce::isPositiveAndBelow (layer, (int) LayerType::numTypes))
                    roles.layers[(size_t) layer].push_back (&part);
                else
                {
                    roles.extras.push_back (&part);
                    warnings.add (part.describe() + " is on channel " + juce::String (part.channel)
                                  + ", which is not one of Luthier's; it plays as the countermelody layer.");
                }
            }

            return roles;
        }

        for (const auto& part : p.parts)
        {
            if (part.notes.empty())
                continue;

            if (part.channel == 10)
            {
                warnings.add (part.describe() + " (channel 10, drums) was skipped.");
                continue;
            }

            const bool polyphonic = part.isPolyphonic();
            const bool bassy = part.meanPitch() < 40.0 || part.name.containsIgnoreCase ("bass");

            if (polyphonic && ! bassy && roles.chords.empty())
                roles.chords.push_back (&part);
            else if (bassy && roles.bass.empty())
                roles.bass.push_back (&part);
            else if (! polyphonic && roles.melody.empty())
                roles.melody.push_back (&part);
            else
                roles.extras.push_back (&part);
        }

        if (roles.chords.empty() && p.chart.size() >= 2)
            roles.chordsFromChart = true;

        juce::StringArray found;

        if (! roles.chords.empty())      found.add ("chords from " + roles.chords.front()->describe());
        else if (roles.chordsFromChart)  found.add ("chords from the file's text events");
        if (! roles.melody.empty())      found.add ("melody from " + roles.melody.front()->describe());
        if (! roles.bass.empty())        found.add ("bass from " + roles.bass.front()->describe());

        if (! found.isEmpty())
            warnings.add ("Tracks were sorted by what they play: " + found.joinIntoString (", ") + ".");

        if (! roles.chords.empty())
        {
            if (roles.extras.empty())
                warnings.add ("Chords are strummed by each section's rhythm pattern; the original chord track "
                              + roles.chords.front()->describe() + " is kept as a muted countermelody layer.");
            else
                warnings.add ("Chords are strummed by each section's rhythm pattern; the original chord track "
                              + roles.chords.front()->describe() + " was not kept, because other tracks fill the layer.");
        }

        for (const auto* extra : roles.extras)
            warnings.add (extra->describe() + " plays verbatim as the countermelody layer.");

        return roles;
    }

    //==========================================================================
    /** One section-to-be: where it lies in the file and how long it is. */
    struct Region
    {
        double start = 0.0;          ///< Quarter notes from the file's start.
        double lengthBeats = 0.0;    ///< lengthBars * beats per bar.
        int lengthBars = 1;
        juce::String name;
        juce::String markerText;     ///< For folding repeats; empty without markers.
    };

    std::vector<Region> buildRegions (Parsed& p, double beatsPerBar, const TuneImportOptions& options,
                                      juce::StringArray& warnings)
    {
        std::vector<Region> regions;

        auto clampBars = [&] (int bars, const juce::String& name)
        {
            if (bars <= Tune::kMaxBars)
                return bars;

            warnings.add ("Section '" + name + "' is " + juce::String (bars) + " bars; cut to "
                          + juce::String (Tune::kMaxBars) + ".");
            return Tune::kMaxBars;
        };

        if (p.markers.empty())
        {
            const int bars = juce::jmax (1, (int) std::ceil ((p.lastNoteEnd - kEps) / beatsPerBar));

            if (bars <= options.singleSectionMaxBars)
            {
                regions.push_back ({ 0.0, (double) bars * beatsPerBar, bars, "Section", {} });
                warnings.add ("No section markers; the file is one " + juce::String (bars) + "-bar section.");
                return regions;
            }

            const int chunk = juce::jlimit (1, Tune::kMaxBars, options.chunkBars);

            for (int bar = 0; bar < bars; bar += chunk)
            {
                if ((int) regions.size() >= Tune::kMaxSections)
                {
                    warnings.add ("Only the first " + juce::String (Tune::kMaxSections) + " sections were imported.");
                    break;
                }

                const int length = juce::jmin (chunk, bars - bar);
                regions.push_back ({ (double) bar * beatsPerBar, (double) length * beatsPerBar, length,
                                     "Part " + juce::String (regions.size() + 1), {} });
            }

            warnings.add ("No section markers; the file was cut into " + juce::String (chunk) + "-bar parts.");
            return regions;
        }

        std::stable_sort (p.markers.begin(), p.markers.end(),
                          [] (const Marker& a, const Marker& b) { return a.ppq < b.ppq; });

        // Two markers at one place are one boundary; the first names it.
        p.markers.erase (std::unique (p.markers.begin(), p.markers.end(),
                                      [] (const Marker& a, const Marker& b) { return std::abs (a.ppq - b.ppq) < kEps; }),
                         p.markers.end());

        if (p.markers.front().ppq > kEps)
        {
            bool notesBefore = false;

            for (const auto& part : p.parts)
                for (const auto& n : part.notes)
                    notesBefore = notesBefore || n.start < p.markers.front().ppq - kEps;

            if (notesBefore)
            {
                p.markers.insert (p.markers.begin(), { 0.0, "Intro" });
                warnings.add ("Notes before the first marker were put in an Intro section.");
            }
            else
            {
                // Nothing plays before the first marker: the tune starts there.
                const double shift = p.markers.front().ppq;

                for (auto& marker : p.markers) marker.ppq -= shift;
                for (auto& entry : p.chart)    entry.ppq -= shift;

                for (auto& part : p.parts)
                    for (auto& n : part.notes)
                    {
                        n.start -= shift;
                        n.end -= shift;
                    }

                p.lastNoteEnd -= shift;
            }
        }

        for (size_t i = 0; i < p.markers.size(); ++i)
        {
            if ((int) regions.size() >= Tune::kMaxSections)
            {
                warnings.add ("Only the first " + juce::String (Tune::kMaxSections) + " sections were imported.");
                break;
            }

            const double start = p.markers[i].ppq;
            const double end = i + 1 < p.markers.size() ? p.markers[i + 1].ppq
                                                        : juce::jmax (p.lastNoteEnd, start + beatsPerBar);
            const auto name = p.markers[i].text.isNotEmpty() ? p.markers[i].text : juce::String ("Section");

            int bars = juce::jmax (1, (int) std::lround ((end - start) / beatsPerBar));
            bars = clampBars (bars, name);

            if (std::abs ((end - start) - (double) bars * beatsPerBar) > 0.01)
                warnings.add ("Section '" + name + "' spans " + juce::String ((end - start) / beatsPerBar, 2)
                              + " bars; rounded to " + juce::String (bars) + ".");

            regions.push_back ({ start, (double) bars * beatsPerBar, bars, name, p.markers[i].text });
        }

        return regions;
    }

    //==========================================================================
    /** The notes of some parts inside a region, section-relative. A note across
        the region's edge is cut there: the part inside this region here, the
        rest in the next (what TuneSession::drainRecording does with a note held
        past a section's end). */
    std::vector<MelodyNote> notesIn (const PartList& parts, const Region& r)
    {
        std::vector<MelodyNote> notes;
        const double end = r.start + r.lengthBeats;

        for (const auto* part : parts)
        {
            for (const auto& n : part->notes)
            {
                if (n.start >= end - kEps || n.end <= r.start + kEps)
                    continue;

                const double from = juce::jmax (n.start, r.start) - r.start;
                const double to = juce::jmin (n.end, end) - r.start;

                if (to - from < kEps)
                    continue;

                auto note = MelodyNote::make (from, to - from, n.pitch, n.velocity);
                note.articulation = n.articulation;
                note.technique = n.technique;
                note.locked = true;   // it was written, not generated (tune-builder 0.3)
                notes.push_back (note);
            }
        }

        std::stable_sort (notes.begin(), notes.end(), [] (const MelodyNote& a, const MelodyNote& b)
        {
            return a.startBeat < b.startBeat || (a.startBeat == b.startBeat && a.pitch.value < b.pitch.value);
        });

        return notes;
    }

    /** Record's quantise, on notes that were exact: only when asked. */
    std::vector<MelodyNote> quantised (const std::vector<MelodyNote>& exact, const Tune& tune, int sectionIndex,
                                       const TuneImportOptions& options)
    {
        if (! options.quantise)
            return exact;

        std::vector<RecordedNote> recorded;

        for (const auto& n : exact)
            recorded.push_back ({ n.startBeat, n.getEndBeat(), n.pitch.value, n.velocity });

        return quantiseRecording (recorded, options.grid, tune, sectionIndex, false, false);
    }

    //==========================================================================
    const char* extensionNameFor (int interval) noexcept
    {
        switch (interval)
        {
            case 1:  return "b9";
            case 2:  return "9";
            case 3:  return "#9";
            case 5:  return "11";
            case 6:  return "#11";
            case 8:  return "b13";
            case 9:  return "13";
            default: return nullptr;
        }
    }

    /** The detector's symbol as a cell: the template's suffix is the quality
        (TuneTheory's vocabulary is the template table), the bass when it is a
        slash chord, and the unexplained intervals as extensions. */
    bool toChordCell (const ChordSymbol& symbol, ChordCell& cell)
    {
        if (! symbol.isKnown())
            return false;

        cell = ChordCell::make (symbol.root, getChordTemplate (symbol.templateIndex).suffix, 1.0);

        if (symbol.isSlash())
            cell.bass = symbol.bass;

        for (int i = 0; i < symbol.numExtensions; ++i)
            if (const auto* name = extensionNameFor (symbol.extensions[(size_t) i]))
                if (getExtensionSemitones (name) >= 0)
                    cell.extensions.add (name);

        return true;
    }

    bool sameChord (const ChordCell& a, const ChordCell& b)
    {
        return a.root == b.root && a.quality == b.quality && a.bass == b.bass && a.extensions == b.extensions;
    }

    struct Run
    {
        ChordCell cell;
        double beats = 0.0;
        bool known = false;
    };

    std::vector<ChordCell> cellsFromRuns (std::vector<Run>& runs, const juce::String& sectionName,
                                          bool& warnedAboutGaps, juce::StringArray& warnings)
    {
        std::vector<ChordCell> cells;

        // A gap before the first chord: the first chord takes it, because cells
        // lay out from the section's start and cannot rest.
        if (runs.size() > 1 && ! runs.front().known)
        {
            runs[1].beats += runs.front().beats;
            runs.erase (runs.begin());

            if (! warnedAboutGaps)
            {
                warnings.add ("The first chord of '" + sectionName + "' was moved back to the section's start.");
                warnedAboutGaps = true;
            }
        }

        for (auto& run : runs)
        {
            if (! run.known)
                continue;

            run.cell.durationBeats = canonical (run.beats);
            cells.push_back (run.cell);
        }

        return cells;
    }

    /** Beat by beat, what the chord parts hold, through the rhythm engine's
        detector; runs of the same chord become one cell. Silence continues the
        chord before it (a cell holds until the next), and an unrecognisable
        stack does too rather than becoming a chord nobody played. */
    std::vector<ChordCell> detectChords (const PartList& parts, const Region& r, bool& warnedAboutGaps,
                                         juce::StringArray& warnings)
    {
        ChordDetector detector;
        std::vector<Run> runs;
        std::vector<int> pitches;

        auto collect = [&] (double from, double to)
        {
            pitches.clear();

            for (const auto* part : parts)
                for (const auto& n : part->notes)
                    if (n.start < to && n.end > from + kEps)
                        pitches.push_back (n.pitch);
        };

        auto distinctPitchClasses = [&]
        {
            uint16_t mask = 0;

            for (int pitch : pitches)
                mask = (uint16_t) (mask | (uint16_t) (1u << wrapPitchClass (pitch)));

            return juce::countNumberOfBits ((juce::uint32) mask);
        };

        for (double t = 0.0; t < r.lengthBeats - kEps; t += 1.0)
        {
            const double step = juce::jmin (1.0, r.lengthBeats - t);
            const double at = r.start + t;

            // What sounds just after the beat (the strum has landed); failing
            // a chord there, anything in the beat.
            collect (at - kEps, at + kStrumWindowBeats + kEps);

            if (distinctPitchClasses() < 2)
                collect (at, at + step);

            ChordCell cell;
            const bool known = toChordCell (detector.detect (pitches.data(), (int) pitches.size()), cell);

            if (! known)
            {
                if (runs.empty())
                    runs.push_back ({ {}, step, false });
                else
                    runs.back().beats += step;
            }
            else if (! runs.empty() && runs.back().known && sameChord (runs.back().cell, cell))
            {
                runs.back().beats += step;
            }
            else
            {
                runs.push_back ({ cell, step, true });
            }
        }

        return cellsFromRuns (runs, r.name, warnedAboutGaps, warnings);
    }

    /** The chart's symbols inside the region, each held to the next. */
    std::vector<ChordCell> chartChords (const std::vector<ChartEntry>& chart, const Region& r, bool& warnedAboutGaps,
                                        juce::StringArray& warnings)
    {
        std::vector<Run> runs;
        const double end = r.start + r.lengthBeats;

        for (size_t i = 0; i < chart.size(); ++i)
        {
            const auto& entry = chart[i];

            if (entry.ppq < r.start - kEps || entry.ppq >= end - kEps)
                continue;

            const double next = i + 1 < chart.size() ? juce::jmin (end, chart[i + 1].ppq) : end;

            if (runs.empty() && entry.ppq > r.start + kEps)
                runs.push_back ({ {}, entry.ppq - r.start, false });

            runs.push_back ({ entry.cell, juce::jmax (0.0, next - entry.ppq), true });
        }

        return cellsFromRuns (runs, r.name, warnedAboutGaps, warnings);
    }

    //==========================================================================
    bool sameSectionIgnoringName (const TuneSection& a, const TuneSection& b)
    {
        auto renamed = a;
        renamed.name = b.name;
        return renamed == b;
    }

    double ccToUnit (int value, double fallback) noexcept
    {
        return value < 0 ? fallback : canonical ((double) juce::jlimit (0, 127, value) / 127.0);
    }

    double ccToPan (int value) noexcept
    {
        return value < 0 ? 0.0 : canonical (juce::jlimit (-1.0, 1.0, (double) juce::jlimit (0, 127, value) / 63.5 - 1.0));
    }
}

//==============================================================================
bool importMidi (const juce::MidiFile& midi, const juce::String& sourceName, Tune& out,
                 const TuneImportOptions& options, juce::String& error, juce::StringArray* warningsOut)
{
    juce::StringArray warnings;
    const auto name = sourceName.isNotEmpty() ? sourceName : juce::String ("The file");

    // midi-export 1: musical time only. SMPTE frames say nothing about beats,
    // and a tune is made of bars.
    const int timeFormat = midi.getTimeFormat();

    if (timeFormat <= 0)
    {
        error = name + " uses SMPTE (frame) timing; Luthier imports MIDI files in beats only.";
        return false;
    }

    if (midi.getNumTracks() <= 0)
    {
        error = name + " has no tracks.";
        return false;
    }

    Parsed parsed;

    for (int t = 0; t < midi.getNumTracks(); ++t)
        if (const auto* track = midi.getTrack (t))
            parseTrack (*track, t, (double) timeFormat, parsed);

    if (parsed.numNotes() == 0)
    {
        error = name + " has no notes.";
        return false;
    }

    // ---- meta ------------------------------------------------------------------------
    Tune tune;
    // A bare file name is not an absolute path, so no juce::File here.
    tune.meta.title = parsed.title.isNotEmpty() ? parsed.title
                    : sourceName.containsChar ('.') ? sourceName.upToLastOccurrenceOf (".", false, false)
                                                    : sourceName;

    if (tune.meta.title.isEmpty())
        tune.meta.title = "Imported Tune";

    tune.meta.artist = parsed.artist;

    if (parsed.tempos.empty())
        warnings.add ("No tempo in " + name + "; " + juce::String (tune.meta.tempoBpm, 0) + " bpm assumed.");
    else
    {
        tune.setTempo (parsed.tempos.front());

        if (parsed.tempos.size() > 1)
            warnings.add (name + " changes tempo " + juce::String (parsed.tempos.size() - 1)
                          + " time(s); the first, " + juce::String (tune.meta.tempoBpm, 0) + " bpm, is used throughout.");
    }

    if (parsed.timeSigNumerator == 0)
        warnings.add ("No time signature in " + name + "; 4/4 assumed.");
    else if (! tune.setTimeSignature (parsed.timeSigNumerator, parsed.timeSigDenominator)
               && ! (parsed.timeSigNumerator == 4 && parsed.timeSigDenominator == 4))
        warnings.add (name + " is in " + juce::String (parsed.timeSigNumerator) + "/"
                      + juce::String (parsed.timeSigDenominator) + ", which the Tune Builder cannot hold; 4/4 assumed.");

    if (parsed.hasKey)
    {
        // Sharps go up in fifths from C, flats down; a minor key sits a minor
        // third below its relative major.
        const int major = wrapPitchClass (parsed.keyAccidentals * 7);
        tune.setKey (parsed.keyMinor ? wrapPitchClass (major - 3) : major,
                     parsed.keyMinor ? TuneMode::aeolian : TuneMode::ionian);
    }
    else
        warnings.add ("No key signature in " + name + "; C major assumed.");

    const auto now = juce::Time::getCurrentTime().toISO8601 (true);
    tune.meta.created = now;
    tune.meta.modified = now;

    // ---- tracks and sections -------------------------------------------------------------
    const auto roles = classify (parsed, options, warnings);
    const auto regions = buildRegions (parsed, tune.getBeatsPerBar(), options, warnings);

    std::map<juce::String, std::vector<int>> sectionsByMarker;
    std::vector<TuneSetlistEntry> setlist;
    bool warnedAboutGaps = false;
    bool anyFolded = false;

    for (const auto& r : regions)
    {
        TuneSection s;
        s.name = r.name;
        s.lengthBars = r.lengthBars;
        s.rhythmPatternId = options.rhythmPatternId;
        s.genreKitId = options.genreKitId;

        if (! roles.chords.empty())
            s.chords = detectChords (roles.chords, r, warnedAboutGaps, warnings);
        else if (roles.chordsFromChart)
            s.chords = chartChords (parsed.chart, r, warnedAboutGaps, warnings);

        // The melody and bass are quantised against the tune, so they are set
        // once the section is in it (below); gather the exact notes first.
        const auto melody = notesIn (roles.melody, r);
        const auto bass = notesIn (roles.bass, r);

        if (! melody.empty())
        {
            MelodyTrack track;
            track.source = MelodySource::record;
            track.notes = melody;
            s.melody = track;
        }

        if (! bass.empty())
        {
            s.bass.mode = BassMode::manual;
            s.bass.notes = bass;
        }

        if (roles.luthierLayout)
        {
            for (size_t type = 0; type < roles.layers.size(); ++type)
            {
                const auto& parts = roles.layers[type];

                if (parts.empty())
                    continue;

                // A generated layer's notes say only that it was on; the
                // countermelody's are the layer.
                auto notes = notesIn (parts, r);

                if (notes.empty())
                    continue;

                TuneLayer layer;
                layer.type = (LayerType) type;
                layer.enabled = true;
                layer.volume = ccToUnit (parts.front()->volumeCc, 0.8);
                layer.pan = ccToPan (parts.front()->panCc);

                if (layer.type == LayerType::countermelody)
                {
                    // TuneMidi scales the layer's velocities by its volume on
                    // the way out; back they come, so the written line and its
                    // level are what they were rather than the level twice.
                    const double scale = juce::jmax (0.01, layer.volume);

                    for (auto& n : notes)
                        n.velocity = juce::jlimit (1, 127, (int) std::lround ((double) n.velocity / scale));

                    layer.notes = std::move (notes);
                }

                s.layers.push_back (layer);
            }
        }

        // Anything else plays verbatim as the countermelody; failing that, the
        // chord track is kept there, muted, so the strum can be compared with it.
        auto* counter = [&]() -> TuneLayer*
        {
            for (auto& layer : s.layers)
                if (layer.type == LayerType::countermelody)
                    return &layer;

            return nullptr;
        }();

        if (! roles.extras.empty())
        {
            auto extra = notesIn (roles.extras, r);

            if (! extra.empty())
            {
                if (counter == nullptr)
                {
                    TuneLayer layer;
                    layer.type = LayerType::countermelody;
                    layer.volume = 1.0;   // verbatim: the file's velocities, unscaled
                    s.layers.push_back (layer);
                    counter = &s.layers.back();
                }

                counter->enabled = true;
                counter->notes.insert (counter->notes.end(), extra.begin(), extra.end());
            }
        }
        else if (! roles.luthierLayout && ! roles.chords.empty() && counter == nullptr)
        {
            auto original = notesIn (roles.chords, r);

            if (! original.empty())
            {
                TuneLayer layer;
                layer.type = LayerType::countermelody;
                layer.enabled = false;
                layer.volume = 1.0;
                layer.notes = std::move (original);
                s.layers.push_back (layer);
            }
        }

        // ---- fold a repeat: the same marker text and the same music is the same
        //      section played again (Verse x2), not a second Verse.
        int index = -1;

        if (r.markerText.isNotEmpty())
            for (int existing : sectionsByMarker[r.markerText])
                if (sameSectionIgnoringName (s, tune.arrangement.sections[(size_t) existing]))
                {
                    index = existing;
                    anyFolded = true;
                    break;
                }

        if (index < 0)
        {
            index = tune.addSection (s);

            if (index < 0)
            {
                warnings.add ("Only the first " + juce::String (tune.getNumSections()) + " sections were imported.");
                break;
            }

            if (r.markerText.isNotEmpty())
                sectionsByMarker[r.markerText].push_back (index);

            if (options.quantise)
            {
                if (! melody.empty())
                    tune.setMelodyNotes (index, quantised (melody, tune, index, options), MelodySource::record);

                if (! bass.empty())
                    tune.setBassNotes (index, quantised (bass, tune, index, options));
            }
        }

        const auto sectionName = tune.arrangement.sections[(size_t) index].name;

        if (! setlist.empty() && setlist.back().section == sectionName && setlist.back().repeats < Tune::kMaxRepeats)
            ++setlist.back().repeats;
        else
            setlist.push_back ({ sectionName, 1 });
    }

    // A setlist that plays each section once, in order, is what no setlist means.
    if (anyFolded)
        tune.setSetlist (setlist);

    if (tune.getNumSections() == 0)
    {
        error = name + " has no notes inside its sections.";
        return false;
    }

    // Notes past the last section are lost; say so.
    {
        const auto& last = regions.back();
        int lost = 0;

        for (const auto& part : parsed.parts)
            if (part.channel != 10 || roles.luthierLayout)
                for (const auto& n : part.notes)
                    if (n.start >= last.start + last.lengthBeats - kEps)
                        ++lost;

        if (lost > 0)
            warnings.add (juce::String (lost) + " note(s) after the last section were dropped.");
    }

    out = std::move (tune);

    if (warningsOut != nullptr)
        warningsOut->addArray (warnings);

    return true;
}

bool importMidiData (const void* data, size_t numBytes, const juce::String& sourceName, Tune& out,
                     const TuneImportOptions& options, juce::String& error, juce::StringArray* warnings)
{
    juce::MidiFile midi;
    juce::MemoryInputStream stream (data, numBytes, false);

    if (data == nullptr || numBytes == 0 || ! midi.readFrom (stream, true))
    {
        error = (sourceName.isNotEmpty() ? sourceName : juce::String ("The file")) + " is not a MIDI file.";
        return false;
    }

    return importMidi (midi, sourceName, out, options, error, warnings);
}

bool importMidiFile (const juce::File& file, Tune& out, const TuneImportOptions& options,
                     juce::String& error, juce::StringArray* warnings)
{
    ThreadProbe::noteFileAccess();

    // error-recovery 1: the named failure, and nothing changes.
    if (! file.existsAsFile())
    {
        error = file.getFileName() + " not found.";
        return false;
    }

    juce::MemoryBlock data;

    if (! file.loadFileAsData (data))
    {
        error = "Cannot read " + file.getFileName() + ".";
        return false;
    }

    return importMidiData (data.getData(), data.getSize(), file.getFileName(), out, options, error, warnings);
}

} // namespace luthier
