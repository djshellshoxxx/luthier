#include "NotationExport.h"
#include "AsciiTabWriter.h" // FEAT2-TAB
#include "TabFingering.h"  // tab-import-export 8
#include "GuitarProLegacyReader.h"
#include "TabImportPipeline.h"
#include "../Export/MidiProfiles.h"

#include <algorithm>
#include <cmath>
#include <functional>   // SPEC-SWEEP NE-4
#include <map>

namespace luthier
{

//==============================================================================
const char* getNotationFormatName (NotationFormat format) noexcept
{
    switch (format)
    {
        case NotationFormat::musicXml:  return "MusicXML";
        case NotationFormat::guitarPro: return "Guitar Pro";
        case NotationFormat::asciiTab:  return "ASCII Tab";
        case NotationFormat::midi:      return "MIDI";
        case NotationFormat::numFormats:
        default:                        return "MusicXML";
    }
}

const char* getNotationFormatExtension (NotationFormat format) noexcept
{
    switch (format)
    {
        case NotationFormat::musicXml:  return ".musicxml";
        case NotationFormat::guitarPro: return ".gp";
        case NotationFormat::asciiTab:  return ".txt";
        case NotationFormat::midi:      return ".mid";
        case NotationFormat::numFormats:
        default:                        return ".musicxml";
    }
}

const char* getNotationFormatLoss (NotationFormat format) noexcept
{
    switch (format)
    {
        case NotationFormat::musicXml:
            return "Whammy events become pitch-bend text directions: MusicXML has "
                   "no native whammy element.";

        case NotationFormat::guitarPro:
            return "No loss for supported techniques.";

        case NotationFormat::asciiTab:
            return "Bend curves flatten to symbolic bends.";

        case NotationFormat::midi:
            return "Technique metadata beyond pitch bend and legato is dropped.";

        case NotationFormat::numFormats:
        default:
            return "";
    }
}

//==============================================================================
namespace
{
    /** XML-escapes a string. Nothing in a score is trusted to be XML-safe: a
        track can be named after a preset, and a preset can be named anything. */
    juce::String escapeXml (const juce::String& text)
    {
        return text.replace ("&", "&amp;")
                   .replace ("<", "&lt;")
                   .replace (">", "&gt;")
                   .replace ("\"", "&quot;")
                   .replace ("'", "&apos;");
    }

    /** The MusicXML note type for a duration in beats. */
    juce::String noteTypeForBeats (double beats)
    {
        struct Entry { double beats; const char* name; };

        static const Entry kTypes[] =
        {
            { 8.0,    "breve"   },
            { 4.0,    "whole"   },
            { 2.0,    "half"    },
            { 1.0,    "quarter" },
            { 0.5,    "eighth"  },
            { 0.25,   "16th"    },
            { 0.125,  "32nd"    },
            { 0.0625, "64th"    }
        };

        // The longest type that is not longer than the note; dots are handled by
        // the caller, which knows whether the remainder is worth notating.
        for (const auto& entry : kTypes)
            if (beats >= entry.beats * 0.99)
                return entry.name;

        return "64th";
    }

    /** How many dots a duration wants, given its base type. */
    int dotsForBeats (double beats)
    {
        const auto base = noteTypeForBeats (beats);

        struct Entry { const char* name; double beats; };

        static const Entry kBeats[] =
        {
            { "breve", 8.0 }, { "whole", 4.0 }, { "half", 2.0 }, { "quarter", 1.0 },
            { "eighth", 0.5 }, { "16th", 0.25 }, { "32nd", 0.125 }, { "64th", 0.0625 }
        };

        double baseBeats = 0.0625;

        for (const auto& entry : kBeats)
            if (base == entry.name)
                baseBeats = entry.beats;

        if (beats >= baseBeats * 1.75 * 0.99)
            return 2;

        if (beats >= baseBeats * 1.5 * 0.99)
            return 1;

        return 0;
    }

    /** The measures of a track that fall inside the requested range. */
    void resolveRange (const PerformanceScore& score, const ScoreTrack& track,
                       const NotationExportOptions& options,
                       int& firstMeasure, int& lastMeasure)
    {
        firstMeasure = 0;
        lastMeasure = (int) track.measures.size() - 1;

        if (options.lengthBeats <= 0.0)
            return;

        const auto& meta = score.getMeta();

        const double beatsPerMeasure = (double) meta.timeSignatureNumerator * 4.0
                                         / (double) juce::jmax (1, meta.timeSignatureDenominator);

        if (beatsPerMeasure <= 0.0)
            return;

        firstMeasure = juce::jlimit (0, lastMeasure, (int) (options.fromBeat / beatsPerMeasure));

        lastMeasure = juce::jlimit (
            firstMeasure, lastMeasure,
            (int) std::ceil ((options.fromBeat + options.lengthBeats) / beatsPerMeasure) - 1);
    }
}

//==============================================================================
bool NotationExporter::write (const PerformanceScore& score, NotationFormat format,
                              const juce::File& destination,
                              const NotationExportOptions& options)
{
    lastError.clear();

    if (score.getTotalNoteCount() == 0)
    {
        lastError = "There is nothing captured to export.";
        return false;
    }

    destination.getParentDirectory().createDirectory();

    switch (format)
    {
        case NotationFormat::musicXml:
        {
            const auto xml = renderMusicXml (score, options);

            if (! destination.replaceWithText (xml))
            {
                lastError = "Could not write " + destination.getFullPathName();
                return false;
            }

            return true;
        }

        case NotationFormat::asciiTab:
        {
            const auto text = renderAsciiTab (score, options);

            if (! destination.replaceWithText (text))
            {
                lastError = "Could not write " + destination.getFullPathName();
                return false;
            }

            return true;
        }

        case NotationFormat::midi:
            if (! writeMidi (score, destination, options))
            {
                if (lastError.isEmpty())
                    lastError = "Could not write the MIDI file.";

                return false;
            }

            return true;

        case NotationFormat::guitarPro:
            return writeGuitarPro (score, destination, options);

        case NotationFormat::numFormats:
        default:
            lastError = "Unknown format.";
            return false;
    }
}

//==============================================================================
juce::String NotationExporter::renderMusicXml (const PerformanceScore& score,
                                               const NotationExportOptions& options) const
{
    const auto& meta = score.getMeta();
    const auto& track = score.getTrack (0);

    // Task X (notation-export.md 2.1): a standard staff, no tab, for the
    // reader who does not read fret numbers - decided once, used both in the
    // attributes block below and per note.
    const bool standardStaff = options.staffMode == NotationExportOptions::StaffMode::standardStaff;

    int firstMeasure = 0, lastMeasure = 0;
    resolveRange (score, track, options, firstMeasure, lastMeasure);

    juce::String xml;
    xml.preallocateBytes (16384);

    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\" "
           "\"http://www.musicxml.org/dtds/partwise.dtd\">\n"
        << "<score-partwise version=\"4.0\">\n"
        << "  <work><work-title>" << escapeXml (meta.title) << "</work-title></work>\n"
        << "  <identification>\n"
        << "    <creator type=\"composer\">" << escapeXml (meta.artist) << "</creator>\n"
        << "    <encoding><software>Luthier</software></encoding>\n"
        << "  </identification>\n"
        << "  <part-list>\n"
        << "    <score-part id=\"P1\"><part-name>" << escapeXml (track.name)
        << "</part-name></score-part>\n"
        << "  </part-list>\n"
        << "  <part id=\"P1\">\n";

    // Divisions per quarter note. A high number keeps triplets and sixteenths
    // exact in integer arithmetic, which is what MusicXML wants.
    constexpr int divisions = 480;

    juce::StringArray seenChords;

    for (int measureIndex = firstMeasure; measureIndex <= lastMeasure; ++measureIndex)
    {
        if (! juce::isPositiveAndBelow (measureIndex, (int) track.measures.size()))
            break;

        const auto& measure = track.measures[(size_t) measureIndex];

        xml << "    <measure number=\"" << (measureIndex - firstMeasure + 1) << "\">\n";

        if (measureIndex == firstMeasure)
        {
            xml << "      <attributes>\n"
                << "        <divisions>" << divisions << "</divisions>\n"
                << "        <key><fifths>0</fifths></key>\n"
                << "        <time><beats>" << measure.timeSignatureNumerator
                << "</beats><beat-type>" << measure.timeSignatureDenominator
                << "</beat-type></time>\n";

            if (standardStaff)
            {
                // Guitar standard notation is treble clef sounding an octave
                // below what is printed - clef-octave-change - or a written
                // middle C would sit in the guitar's low-string territory.
                // Real staff notation, no tab: no staff-details/tuning, that
                // block exists to make fret numbers meaningful and there are
                // none here.
                xml << "        <clef><sign>G</sign><line>2</line>"
                       "<clef-octave-change>-1</clef-octave-change></clef>\n";
            }
            else
            {
                xml << "        <clef><sign>TAB</sign><line>5</line></clef>\n"
                    << "        <staff-details>\n"
                    << "          <staff-lines>" << track.numStrings << "</staff-lines>\n";

                // notation-export 2.1: the tuning is part of the staff, or the frets
                // mean nothing on reimport.
                for (int s = 0; s < track.numStrings; ++s)
                {
                    juce::String step;
                    int alter = 0, octave = 0;

                    // MusicXML numbers strings from the lowest, the opposite of the
                    // score's own convention, so the tuning is written in reverse.
                    const int scoreString = track.numStrings - 1 - s;

                    PerformanceScore::getMusicXmlPitch (track.tuning[(size_t) scoreString],
                                                        step, alter, octave);

                    xml << "          <staff-tuning line=\"" << (s + 1) << "\">"
                        << "<tuning-step>" << step << "</tuning-step>";

                    if (alter != 0)
                        xml << "<tuning-alter>" << alter << "</tuning-alter>";

                    xml << "<tuning-octave>" << octave << "</tuning-octave></staff-tuning>\n";
                }

                if (track.capoFret > 0)
                    xml << "          <capo>" << track.capoFret << "</capo>\n";

                xml << "        </staff-details>\n";
            }

            xml << "      </attributes>\n"
                << "      <direction placement=\"above\">\n"
                << "        <direction-type><metronome><beat-unit>quarter</beat-unit>"
                << "<per-minute>" << juce::String (meta.tempoBpm, 1)
                << "</per-minute></metronome></direction-type>\n"
                << "        <sound tempo=\"" << juce::String (meta.tempoBpm, 1) << "\"/>\n"
                << "      </direction>\n";
        }

        // ---- chord symbols ---------------------------------------------------------
        if (options.chordSymbols)
        {
            for (const auto& [beat, symbol] : measure.chordSymbols)
            {
                juce::ignoreUnused (beat);

                int rootAlter = 0;
                juce::String rootStep = symbol.substring (0, 1);

                if (symbol.length() > 1 && symbol[1] == '#')
                    rootAlter = 1;
                else if (symbol.length() > 1 && symbol[1] == 'b')
                    rootAlter = -1;

                xml << "      <harmony>\n"
                    << "        <root><root-step>" << escapeXml (rootStep) << "</root-step>";

                if (rootAlter != 0)
                    xml << "<root-alter>" << rootAlter << "</root-alter>";

                xml << "</root>\n"
                    << "        <kind text=\"" << escapeXml (symbol) << "\">other</kind>\n"
                    << "      </harmony>\n";

                seenChords.addIfNotAlreadyThere (symbol);
            }
        }

        // ---- notes -------------------------------------------------------------------
        double writtenTo = 0.0;

        for (size_t voiceIndex = 0; voiceIndex < measure.voices.size(); ++voiceIndex)
        {
            const auto& voice = measure.voices[voiceIndex];

            if (voiceIndex > 0 && writtenTo > 0.0)
            {
                // A second voice restarts at the top of the measure.
                xml << "      <backup><duration>"
                    << (int) std::round (writtenTo * divisions) << "</duration></backup>\n";

                writtenTo = 0.0;
            }

            double graceBeats = 0.0;   // SPEC-SWEEP NE-9: the grace note before this one took this long

            for (size_t noteIndex = 0; noteIndex < voice.notes.size(); ++noteIndex)
            {
                const auto& noteAsPlayed = voice.notes[noteIndex];

                /*  SPEC-SWEEP NE-9 (notation-export 2.1): a very short note that a
                    hammer-on or pull-off on the same string follows straight away
                    is an ornament - written as a grace note slurred to its target,
                    which takes the grace's time. */
                const auto isGrace = [&voice] (size_t i)
                {
                    const auto& n = voice.notes[i];

                    if (i + 1 >= voice.notes.size() || n.durationBeats >= 0.25 - 1.0e-6)
                        return false;

                    if (i > 0 && std::abs (voice.notes[i - 1].startBeat - n.startBeat) < 1.0e-6)
                        return false;

                    const auto& next = voice.notes[i + 1];
                    return next.stringIndex == n.stringIndex
                           && std::abs (next.startBeat - (n.startBeat + n.durationBeats)) < 1.0e-6
                           && (next.hasTechnique (ScoreTechnique::Type::hammerOn)
                                 || next.hasTechnique (ScoreTechnique::Type::pullOff));
                };

                if (isGrace (noteIndex))
                {
                    if (noteAsPlayed.startBeat > writtenTo + 1.0e-6)
                    {
                        const double restBeats = noteAsPlayed.startBeat - writtenTo;
                        xml << "      <note>\n        <rest/>\n"
                            << "        <duration>" << (int) std::round (restBeats * divisions) << "</duration>\n"
                            << "        <voice>" << (voiceIndex + 1) << "</voice>\n"
                            << "        <type>" << noteTypeForBeats (restBeats) << "</type>\n      </note>\n";
                        writtenTo = noteAsPlayed.startBeat;
                    }

                    juce::String graceStep;
                    int graceAlter = 0, graceOctave = 0;
                    PerformanceScore::getMusicXmlPitch (noteAsPlayed.midiNote, graceStep, graceAlter, graceOctave);

                    xml << "      <note>\n        <grace slash=\"yes\"/>\n"
                        << "        <pitch><step>" << graceStep << "</step>";

                    if (graceAlter != 0)
                        xml << "<alter>" << graceAlter << "</alter>";

                    xml << "<octave>" << graceOctave << "</octave></pitch>\n"
                        << "        <voice>" << (voiceIndex + 1) << "</voice>\n"
                        << "        <type>16th</type>\n"
                        << "        <notations>\n          <technical>\n"
                        << "            <string>" << (noteAsPlayed.stringIndex + 1) << "</string>\n"
                        << "            <fret>" << noteAsPlayed.fret << "</fret>\n"
                        << "          </technical>\n"
                        << "          <slur type=\"start\"/>\n"
                        << "        </notations>\n      </note>\n";

                    graceBeats = noteAsPlayed.durationBeats;
                    continue;
                }

                // The grace's target starts where the grace did and lasts as long as both.
                auto note = noteAsPlayed;
                const bool graceTarget = graceBeats > 0.0;

                if (graceTarget)
                {
                    note.startBeat -= graceBeats;
                    note.durationBeats += graceBeats;
                    graceBeats = 0.0;
                }

                // A rest, where the voice is silent before this note.
                if (note.startBeat > writtenTo + 1.0e-6)
                {
                    const double restBeats = note.startBeat - writtenTo;

                    xml << "      <note>\n"
                        << "        <rest/>\n"
                        << "        <duration>" << (int) std::round (restBeats * divisions)
                        << "</duration>\n"
                        << "        <voice>" << (voiceIndex + 1) << "</voice>\n"
                        << "        <type>" << noteTypeForBeats (restBeats) << "</type>\n"
                        << "      </note>\n";

                    writtenTo = note.startBeat;
                }

                // Notes that start together are a chord.
                const bool isChordMember = noteIndex > 0 && ! graceTarget
                    && std::abs (voice.notes[noteIndex - 1].startBeat - noteAsPlayed.startBeat) < 1.0e-6;

                juce::String step;
                int alter = 0, octave = 0;

                // The standard staff writes a note an octave higher than it
                // sounds, matching the clef's clef-octave-change above.
                PerformanceScore::getMusicXmlPitch (standardStaff ? note.midiNote + 12 : note.midiNote,
                                                    step, alter, octave);

                xml << "      <note>\n";

                if (isChordMember)
                    xml << "        <chord/>\n";

                xml << "        <pitch><step>" << step << "</step>";

                if (alter != 0)
                    xml << "<alter>" << alter << "</alter>";

                xml << "<octave>" << octave << "</octave></pitch>\n"
                    << "        <duration>" << (int) std::round (note.durationBeats * divisions)
                    << "</duration>\n"
                    << "        <voice>" << (voiceIndex + 1) << "</voice>\n"
                    << "        <type>" << noteTypeForBeats (note.durationBeats) << "</type>\n";

                for (int dot = 0; dot < dotsForBeats (note.durationBeats); ++dot)
                    xml << "        <dot/>\n";

                // ---- technical: string, fret, and the techniques ------------------
                // The standard staff has no fret numbers to hang these on, so
                // it skips the whole <technical> group (Task X: distinct from
                // the ASCII/GP tab lane, which is where this detail belongs).
                xml << "        <notations>\n";

                if (! standardStaff)
                {
                    xml << "          <technical>\n"
                        // MusicXML numbers strings from 1 = the highest, as the
                        // score indexes them from 0 = the highest.
                        << "            <string>" << (note.stringIndex + 1)
                        << "</string>\n"
                        << "            <fret>" << note.fret << "</fret>\n";

                    for (const auto& technique : note.techniques)
                    {
                        switch (technique.type)
                        {
                            case ScoreTechnique::Type::bend:
                                xml << "            <bend><bend-alter>"
                                    << juce::String (technique.value, 2)
                                    << "</bend-alter></bend>\n";
                                break;

                            case ScoreTechnique::Type::bendRelease:
                                xml << "            <bend><bend-alter>"
                                    << juce::String (technique.value, 2)
                                    << "</bend-alter><release/></bend>\n";
                                break;

                            case ScoreTechnique::Type::preBend:
                                xml << "            <bend><bend-alter>"
                                    << juce::String (technique.value, 2)
                                    << "</bend-alter><pre-bend/></bend>\n";
                                break;

                            case ScoreTechnique::Type::hammerOn:
                                xml << "            <hammer-on type=\"start\"/>\n";
                                break;

                            case ScoreTechnique::Type::pullOff:
                                xml << "            <pull-off type=\"start\"/>\n";
                                break;

                            case ScoreTechnique::Type::palmMute:
                                xml << "            <other-technical>palm-mute</other-technical>\n";
                                break;

                            case ScoreTechnique::Type::naturalHarmonic:
                                xml << "            <harmonic><natural/></harmonic>\n";
                                break;

                            case ScoreTechnique::Type::artificialHarmonic:
                            case ScoreTechnique::Type::pinchHarmonic:
                            case ScoreTechnique::Type::tapHarmonic:
                                xml << "            <harmonic><artificial/></harmonic>\n";
                                break;

                            case ScoreTechnique::Type::tap:
                                xml << "            <tap/>\n";
                                break;

                            case ScoreTechnique::Type::deadNote:
                                xml << "            <other-technical>dead-note</other-technical>\n";
                                break;

                            // auto-articulation.md 9 (FEAT-ASSIST): pick strokes.
                            case ScoreTechnique::Type::pickStrokeUp:
                                xml << "            <up-bow/>\n";
                                break;

                            case ScoreTechnique::Type::pickStrokeDown:
                                xml << "            <down-bow/>\n";
                                break;

                            default:
                                break;
                        }
                    }

                    xml << "          </technical>\n";
                }

                if (graceTarget)
                    xml << "          <slur type=\"stop\"/>\n";   // SPEC-SWEEP NE-9

                for (const auto& technique : note.techniques)
                {
                    switch (technique.type)
                    {
                        case ScoreTechnique::Type::slideUp:
                        case ScoreTechnique::Type::slideDown:
                        case ScoreTechnique::Type::slideShift:
                            xml << "          <slide type=\"start\"/>\n";
                            break;

                        case ScoreTechnique::Type::slideLegato:
                            xml << "          <glissando type=\"start\"/>\n";
                            break;

                        case ScoreTechnique::Type::vibrato:
                            xml << "          <ornaments><wavy-line type=\"start\"/></ornaments>\n";
                            break;

                        case ScoreTechnique::Type::trill:
                            xml << "          <ornaments><trill-mark/></ornaments>\n";
                            break;

                        case ScoreTechnique::Type::accent:
                            xml << "          <articulations><accent/></articulations>\n";
                            break;

                        case ScoreTechnique::Type::staccato:
                            xml << "          <articulations><staccato/></articulations>\n";
                            break;

                        default:
                            break;
                    }
                }

                xml << "        </notations>\n";

                /*  notation-export 2.1: MusicXML has no whammy element, so a
                    whammy event is written as a text direction. That is the
                    documented round-trip loss for this format - the information
                    survives for a human reader, not for a parser. */
                if (const auto* whammy = note.findTechnique (ScoreTechnique::Type::whammy))
                    xml << "      </note>\n"
                        << "      <direction placement=\"below\"><direction-type><words>"
                        << "whammy " << juce::String (whammy->value, 2) << " st"
                        << "</words></direction-type></direction>\n"
                        << "      <note><rest/><duration>0</duration><voice>"
                        << (voiceIndex + 1) << "</voice>\n";

                xml << "      </note>\n";

                if (! isChordMember)
                    writtenTo = note.startBeat + note.durationBeats;
            }
        }

        xml << "    </measure>\n";
    }

    xml << "  </part>\n</score-partwise>\n";

    return xml;
}

//==============================================================================
juce::String NotationExporter::renderAsciiTabWindow (const PerformanceScore& score,
                                                     int firstMeasure, int numMeasures,
                                                     const NotationExportOptions& options) const
{
    // FEAT2-TAB: one layout for live view, preview and file export.
    if (score.getNumTracks() == 0) return {};
    return AsciiTabWriter::renderWindow (score.getTrack (0), firstMeasure, numMeasures, options);
}

juce::String NotationExporter::renderAsciiTabWindow (const PerformanceScore& score,
                                                     int firstMeasure, int numMeasures,
                                                     const NotationExportOptions& options,
                                                     std::vector<TabColumnMark>& marks) const
{
    marks.clear();
    if (score.getNumTracks() == 0) return {};
    return AsciiTabWriter::renderWindow (score.getTrack (0), firstMeasure, numMeasures, options, &marks);
}

juce::String NotationExporter::renderAsciiTab (const PerformanceScore& score,
                                               const NotationExportOptions& options) const
{
    // FEAT2-TAB: reuse the captured score, including its per-note techniques.
    return AsciiTabWriter::render (score, options);
}

//==============================================================================
bool NotationExporter::writeMidi (const PerformanceScore& score, const juce::File& destination,
                                  const NotationExportOptions& options) const
{
    const auto& meta = score.getMeta();
    const auto& track = score.getTrack (0);

    int firstMeasure = 0, lastMeasure = 0;
    resolveRange (score, track, options, firstMeasure, lastMeasure);

    constexpr int ticksPerQuarter = 960;

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ticksPerQuarter);

    // ---- tempo and time signature track -------------------------------------------
    {
        juce::MidiMessageSequence meta0;

        meta0.addEvent (juce::MidiMessage::tempoMetaEvent (
            (int) (60000000.0 / juce::jmax (1.0, meta.tempoBpm))), 0.0);

        meta0.addEvent (juce::MidiMessage::timeSignatureMetaEvent (
            meta.timeSignatureNumerator, meta.timeSignatureDenominator), 0.0);

        meta0.addEvent (juce::MidiMessage::textMetaEvent (3, meta.title), 0.0);

        file.addTrack (meta0);
    }

    const double beatsPerMeasure = (double) meta.timeSignatureNumerator * 4.0
                                     / (double) juce::jmax (1, meta.timeSignatureDenominator);

    /*  notation-export 2.4: one track per string, up to sixteen.

        That is what makes the export a guitar part rather than a piano part: a
        DAW that shows one track per string shows the fingering, and a re-import
        knows which string each note was on without guessing. */
    const int numStrings = juce::jlimit (1, 16, track.numStrings);

    for (int stringIndex = 0; stringIndex < numStrings; ++stringIndex)
    {
        juce::MidiMessageSequence sequence;

        const int channel = juce::jlimit (1, 16, stringIndex + 1);

        sequence.addEvent (juce::MidiMessage::textMetaEvent (
            3, "String " + juce::String (stringIndex + 1) + " ("
                 + PerformanceScore::getNoteName (track.tuning[(size_t) stringIndex]) + ")"), 0.0);

        // Guitar Pro reads string and fret from RPN messages, so they go out
        // ahead of the notes.
        sequence.addEvent (juce::MidiMessage::controllerEvent (channel, 101, 0), 0.0);
        sequence.addEvent (juce::MidiMessage::controllerEvent (channel, 100, 0), 0.0);
        sequence.addEvent (juce::MidiMessage::controllerEvent (channel, 6, 2), 0.0);

        bool wroteAnything = false;

        for (int measureIndex = firstMeasure; measureIndex <= lastMeasure; ++measureIndex)
        {
            if (! juce::isPositiveAndBelow (measureIndex, (int) track.measures.size()))
                break;

            const auto& measure = track.measures[(size_t) measureIndex];
            const double measureStart = (double) (measureIndex - firstMeasure) * beatsPerMeasure;

            for (const auto* note : measure.collectNotes())
            {
                if (note->stringIndex != stringIndex)
                    continue;

                const double startTicks = (measureStart + note->startBeat) * ticksPerQuarter;
                const double endTicks = startTicks + note->durationBeats * ticksPerQuarter;

                // CC 68 is legato, which is how a hammer-on or pull-off survives
                // into a MIDI file at all.
                const bool legato = note->hasTechnique (ScoreTechnique::Type::hammerOn)
                                      || note->hasTechnique (ScoreTechnique::Type::pullOff);

                // At the note-on's own tick, as MidiPerformance does: a tick
                // earlier put it before the previous legato note's CC 68 off
                // (at that note's end, which is this start), so in 5h7p5 the
                // third note's legato was switched off before it and re-plucked.
                // Added later, it lands after that off at the same tick.
                if (legato)
                    sequence.addEvent (juce::MidiMessage::controllerEvent (channel, 68, 127),
                                       startTicks);

                sequence.addEvent (juce::MidiMessage::noteOn (
                    channel, note->midiNote,
                    (juce::uint8) juce::jlimit (1, 127, (int) (note->velocity * 127.0))),
                    startTicks);

                // Bends and whammy both become pitch bend, which is all MIDI has.
                // tab-import-export 8.3: so do slides (a glide over the last
                // quarter of the note, or into it) and vibrato (a sine), within
                // the two-semitone range the RPN declared.
                const auto wheelFor = [] (double semitones)
                {
                    return juce::jlimit (0, 16383, 8192 + (int) (juce::jlimit (-2.0, 2.0, semitones) / 2.0 * 8192.0));
                };

                for (const auto& technique : note->techniques)
                {
                    using T = ScoreTechnique::Type;
                    const bool isBend = technique.type == T::bend || technique.type == T::whammy
                                          || technique.type == T::preBend || technique.type == T::bendRelease;

                    if (technique.type == T::slideLegato || technique.type == T::slideShift
                         || technique.type == T::slideUp || technique.type == T::slideOut)
                    {
                        const double target = technique.value > 0.0 ? technique.value
                                            : (technique.type == T::slideOut ? note->fret - 5.0 : note->fret + 5.0);
                        const double semis = target - (double) note->fret;
                        const double from = startTicks + 0.75 * note->durationBeats * ticksPerQuarter;

                        for (int k = 0; k <= 4; ++k)
                            sequence.addEvent (juce::MidiMessage::pitchWheel (channel, wheelFor (semis * k / 4.0)),
                                               from + (endTicks - from) * k / 4.0);

                        sequence.addEvent (juce::MidiMessage::pitchWheel (channel, 8192), endTicks);
                        continue;
                    }

                    if (technique.type == T::slideIn)
                    {
                        const double origin = technique.value > 0.0 ? technique.value : juce::jmax (0.0, note->fret - 3.0);
                        const double semis = origin - (double) note->fret;
                        const double to = startTicks + 0.25 * note->durationBeats * ticksPerQuarter;

                        for (int k = 0; k <= 4; ++k)
                            sequence.addEvent (juce::MidiMessage::pitchWheel (channel, wheelFor (semis * (4 - k) / 4.0)),
                                               startTicks + (to - startTicks) * k / 4.0);
                        continue;
                    }

                    if (technique.type == T::vibrato)
                    {
                        const double rateHz = technique.value > 0.0 ? technique.value : 5.5;
                        const double depthSemis = (technique.secondValue > 0.0 ? technique.secondValue : 30.0) / 100.0;
                        const double ticksPerSecond = ticksPerQuarter * juce::jmax (1.0, meta.tempoBpm) / 60.0;
                        const double step = ticksPerSecond / (rateHz * 8.0);
                        const double from = startTicks + 0.25 * note->durationBeats * ticksPerQuarter;

                        int k = 0;
                        for (double at = from; at < endTicks && k < 192; at += step, ++k)
                            sequence.addEvent (juce::MidiMessage::pitchWheel (
                                channel, wheelFor (depthSemis * std::sin (juce::MathConstants<double>::twoPi * k / 8.0))), at);

                        sequence.addEvent (juce::MidiMessage::pitchWheel (channel, 8192), endTicks);
                        continue;
                    }

                    if (! isBend)
                        continue;

                    if (! technique.curve.empty())
                    {
                        for (const auto& [position, semitones] : technique.curve)
                        {
                            const double at = startTicks
                                                + position * note->durationBeats * ticksPerQuarter;

                            // A bend range of two semitones is what the RPN above
                            // declared, so the wheel is scaled to match.
                            const int wheel = juce::jlimit (0, 16383,
                                8192 + (int) (semitones / 2.0 * 8192.0));

                            sequence.addEvent (juce::MidiMessage::pitchWheel (channel, wheel), at);
                        }
                    }
                    else
                    {
                        const int wheel = juce::jlimit (0, 16383,
                            8192 + (int) (technique.value / 2.0 * 8192.0));

                        sequence.addEvent (juce::MidiMessage::pitchWheel (channel, wheel),
                                           startTicks + 1.0);
                    }

                    // And back to centre, or the next note on this string inherits
                    // the bend.
                    sequence.addEvent (juce::MidiMessage::pitchWheel (channel, 8192), endTicks);
                }

                sequence.addEvent (juce::MidiMessage::noteOff (channel, note->midiNote), endTicks);

                if (legato)
                    sequence.addEvent (juce::MidiMessage::controllerEvent (channel, 68, 0), endTicks);

                wroteAnything = true;
            }
        }

        if (wroteAnything)
        {
            sequence.updateMatchedPairs();
            file.addTrack (sequence);
        }
    }

    destination.deleteFile();

    std::unique_ptr<juce::FileOutputStream> stream (destination.createOutputStream());

    if (stream == nullptr)
    {
        lastError = "Could not open " + destination.getFullPathName() + " for writing.";
        return false;
    }

    return file.writeTo (*stream);
}

//==============================================================================
juce::String NotationExporter::renderGuitarProXml (const PerformanceScore& score,
                                                   const NotationExportOptions& options) const
{
    const auto& meta = score.getMeta();
    const auto& track = score.getTrack (0);

    int firstMeasure = 0, lastMeasure = 0;
    resolveRange (score, track, options, firstMeasure, lastMeasure);

    juce::String xml;
    xml.preallocateBytes (16384);

    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<GPIF>\n"
        << "  <GPVersion>7</GPVersion>\n"
        << "  <Score>\n"
        << "    <Title>" << escapeXml (meta.title) << "</Title>\n"
        << "    <Artist>" << escapeXml (meta.artist) << "</Artist>\n"
        << "    <Tempo>" << juce::String (meta.tempoBpm, 1) << "</Tempo>\n"
        << "  </Score>\n"
        << "  <Tracks>\n"
        << "    <Track id=\"0\">\n"
        << "      <Name>" << escapeXml (track.name) << "</Name>\n"
        << "      <Staves>\n"
        << "        <Staff>\n"
        << "          <Properties>\n"
        << "            <Property name=\"Tuning\"><Pitches>";

    // GPIF lists the tuning from the lowest string upward.
    for (int s = track.numStrings - 1; s >= 0; --s)
        xml << track.tuning[(size_t) s] << (s > 0 ? " " : "");

    xml << "</Pitches></Property>\n";

    if (track.capoFret > 0)
        xml << "            <Property name=\"CapoFret\"><Fret>" << track.capoFret
            << "</Fret></Property>\n";

    /*  SPEC-SWEEP NE-13 (notation-export 2.2): real chord diagrams. Each chord's
        diagram is the voicing played where it first occurs - the string and
        fret of every note starting within half a beat of the symbol. */
    juce::StringArray chordNames;
    juce::String diagramsXml;

    if (options.chordDiagrams)
    {
        for (int m = firstMeasure; m <= lastMeasure && juce::isPositiveAndBelow (m, (int) track.measures.size()); ++m)
        {
            const auto& measure = track.measures[(size_t) m];

            for (const auto& [beat, symbol] : measure.chordSymbols)
            {
                if (chordNames.contains (symbol))
                    continue;

                std::vector<std::pair<int, int>> frets;   // (GPIF string, fret)
                int lowest = 99;

                for (const auto* note : measure.collectNotes())
                    if (note->startBeat >= beat - 1.0e-6 && note->startBeat < beat + 0.5)
                    {
                        frets.emplace_back (track.numStrings - 1 - note->stringIndex, note->fret);

                        if (note->fret > 0)
                            lowest = juce::jmin (lowest, note->fret);
                    }

                const int baseFret = (lowest == 99 || lowest <= 3) ? 0 : lowest - 1;

                diagramsXml << "              <Item id=\"" << chordNames.size() << "\" name=\"" << escapeXml (symbol) << "\">\n"
                            << "                <Diagram stringCount=\"" << track.numStrings
                            << "\" fretCount=\"5\" baseFret=\"" << baseFret << "\">\n";

                for (const auto& [string, fret] : frets)
                    diagramsXml << "                  <Fret string=\"" << string << "\" fret=\"" << (fret - baseFret) << "\"/>\n";

                diagramsXml << "                </Diagram>\n"
                            << "              </Item>\n";

                chordNames.add (symbol);
            }
        }
    }

    xml << "          </Properties>\n"
        << "        </Staff>\n"
        << "      </Staves>\n";

    if (chordNames.size() > 0)
        xml << "      <Properties>\n"
            << "        <Property name=\"DiagramCollection\">\n"
            << "          <Items>\n" << diagramsXml
            << "          </Items>\n"
            << "        </Property>\n"
            << "      </Properties>\n";

    xml << "    </Track>\n"
        << "  </Tracks>\n"
        << "  <MasterBars>\n";

    juce::StringArray diagrammedChords;

    int beatId = 0;
    int noteId = 0;
    int barId = 0;
    int voiceId = 0;

    juce::String beatsXml, notesXml, rhythmsXml, barsXml, voicesXml;

    // GPIF's note values, from the MusicXML names this writer already uses.
    const auto gpNoteValue = [] (double beats)
    {
        const auto type = noteTypeForBeats (beats);

        if (type == "breve" || type == "whole") return juce::String ("Whole");
        if (type == "half")                     return juce::String ("Half");
        if (type == "quarter")                  return juce::String ("Quarter");
        if (type == "eighth")                   return juce::String ("Eighth");
        return type;   // 16th, 32nd, 64th
    };

    const auto addRhythm = [&rhythmsXml, &gpNoteValue] (int id, double beats)
    {
        rhythmsXml << "    <Rhythm id=\"" << id << "\">\n"
                   << "      <NoteValue>" << gpNoteValue (beats) << "</NoteValue>\n";

        if (const int dots = dotsForBeats (beats); dots > 0)
            rhythmsXml << "      <AugmentationDot count=\"" << dots << "\"/>\n";

        rhythmsXml << "    </Rhythm>\n";
    };

    for (int measureIndex = firstMeasure; measureIndex <= lastMeasure; ++measureIndex)
    {
        if (! juce::isPositiveAndBelow (measureIndex, (int) track.measures.size()))
            break;

        const auto& measure = track.measures[(size_t) measureIndex];

        xml << "    <MasterBar>\n"
            << "      <Time>" << measure.timeSignatureNumerator << "/"
            << measure.timeSignatureDenominator << "</Time>\n"
            << "      <Bars>" << barId << "</Bars>\n";   // SPEC-SWEEP NE-4: one bar per track

        if (options.chordDiagrams)
        {
            for (const auto& [beat, symbol] : measure.chordSymbols)
            {
                juce::ignoreUnused (beat);

                // notation-export 2.2: a diagram at the first occurrence of each
                // chord, not at every one.
                if (! diagrammedChords.contains (symbol))
                {
                    diagrammedChords.add (symbol);
                    xml << "      <Chord firstOccurrence=\"true\">" << escapeXml (symbol)
                        << "</Chord>\n";
                }
                else
                {
                    xml << "      <Chord>" << escapeXml (symbol) << "</Chord>\n";
                }
            }
        }

        xml << "    </MasterBar>\n";

        /*  SPEC-SWEEP NE-4 (notation-export 2.2): GPIF's structure - the bar
            lists its voices, a voice its beats, a beat its notes (all the
            notes struck together) and its rhythm; a gap is a beat with no
            notes. It was one beat per note with no bars or voices, which put a
            chord's notes one after another and no reader could place them. */
        juce::String voiceList;
        juce::StringArray chordsPlaced;   // SPEC-SWEEP NE-13: the first note at a symbol carries it

        for (size_t v = 0; v < 4; ++v)
        {
            if (v >= measure.voices.size() || measure.voices[v].notes.empty())
            {
                voiceList << (v > 0 ? " " : "") << "-1";
                continue;
            }

            const auto& voice = measure.voices[v];
            juce::String beatList;
            double position = 0.0;

            for (size_t i = 0; i < voice.notes.size();)
            {
                // The notes that start together are one beat.
                size_t end = i + 1;

                while (end < voice.notes.size() && std::abs (voice.notes[end].startBeat - voice.notes[i].startBeat) < 1.0e-6)
                    ++end;

                const auto& first = voice.notes[i];

                if (first.startBeat > position + 1.0e-6)
                {
                    const double gap = first.startBeat - position;
                    beatsXml << "    <Beat id=\"" << beatId << "\">\n"
                             << "      <Rhythm ref=\"" << beatId << "\"/>\n"
                             << "    </Beat>\n";
                    addRhythm (beatId, gap);
                    beatList << (beatList.isEmpty() ? "" : " ") << beatId++;
                }

                double length = first.durationBeats;

                if (end < voice.notes.size())
                    length = juce::jmin (length, voice.notes[end].startBeat - first.startBeat);

                length = juce::jmax (0.0625, length);

                juce::String noteList;

                for (size_t k = i; k < end; ++k)
                    noteList << (k > i ? " " : "") << (noteId + (int) (k - i));

                beatsXml << "    <Beat id=\"" << beatId << "\">\n"
                         << "      <Notes>" << noteList << "</Notes>\n"
                         << "      <Rhythm ref=\"" << beatId << "\"/>\n";

                // SPEC-SWEEP NE-13: the beat names its chord's diagram.
                for (const auto& [beat, symbol] : measure.chordSymbols)
                    if (std::abs (first.startBeat - beat) < 1.0e-6 && ! chordsPlaced.contains (symbol + "@" + juce::String (beat))
                          && chordNames.contains (symbol))
                    {
                        beatsXml << "      <Chord>" << chordNames.indexOf (symbol) << "</Chord>\n";
                        chordsPlaced.add (symbol + "@" + juce::String (beat));
                    }

                // SPEC-SWEEP NE-12: whammy is a beat property in GPIF, with its
                // curve's first, middle and last points (100 = a whole tone).
                for (size_t k = i; k < end; ++k)
                {
                    if (const auto* whammy = voice.notes[k].findTechnique (ScoreTechnique::Type::whammy))
                    {
                        const auto valueAt = [whammy] (size_t index)
                        {
                            return whammy->curve.empty() ? whammy->value
                                                         : whammy->curve[juce::jmin (index, whammy->curve.size() - 1)].second;
                        };

                        const auto middle = whammy->curve.empty() ? (size_t) 0 : whammy->curve.size() / 2;
                        const auto last = whammy->curve.empty() ? (size_t) 0 : whammy->curve.size() - 1;

                        beatsXml << "      <Properties>\n"
                                 << "        <Property name=\"WhammyBar\"><Enable/></Property>\n"
                                 << "        <Property name=\"WhammyBarOriginValue\"><Float>" << juce::String (valueAt (0) * 50.0, 2) << "</Float></Property>\n"
                                 << "        <Property name=\"WhammyBarMiddleValue\"><Float>" << juce::String (valueAt (middle) * 50.0, 2) << "</Float></Property>\n"
                                 << "        <Property name=\"WhammyBarDestinationValue\"><Float>" << juce::String (valueAt (last) * 50.0, 2) << "</Float></Property>\n"
                                 << "      </Properties>\n";
                        break;
                    }
                }

                beatsXml << "    </Beat>\n";
                addRhythm (beatId, length);
                beatList << (beatList.isEmpty() ? "" : " ") << beatId++;

                for (size_t k = i; k < end; ++k)
                {
                    const auto* note = &voice.notes[k];

                    notesXml << "    <Note id=\"" << noteId << "\">\n"
                             << "      <Properties>\n"
                             << "        <Property name=\"String\"><String>"
                             << (track.numStrings - 1 - note->stringIndex) << "</String></Property>\n"
                             << "        <Property name=\"Fret\"><Fret>" << note->fret
                             << "</Fret></Property>\n";

                    // notation-export 2.2 promises full technique fidelity here.
                    for (const auto& technique : note->techniques)
                    {
                        switch (technique.type)
                        {
                            case ScoreTechnique::Type::bend:
                            case ScoreTechnique::Type::bendRelease:
                            case ScoreTechnique::Type::preBend:
                                notesXml << "        <Property name=\"Bended\"><Enable/></Property>\n"
                                         << "        <Property name=\"BendDestinationValue\"><Float>"
                                         << juce::String (technique.value * 50.0, 2)
                                         << "</Float></Property>\n";
                                break;

                            case ScoreTechnique::Type::slideUp:
                                notesXml << "        <Property name=\"Slide\"><Flags>1</Flags></Property>\n";
                                break;

                            case ScoreTechnique::Type::slideDown:
                                notesXml << "        <Property name=\"Slide\"><Flags>2</Flags></Property>\n";
                                break;

                            case ScoreTechnique::Type::slideLegato:
                                notesXml << "        <Property name=\"Slide\"><Flags>4</Flags></Property>\n";
                                break;

                            case ScoreTechnique::Type::slideShift:
                                notesXml << "        <Property name=\"Slide\"><Flags>8</Flags></Property>\n";
                                break;

                            case ScoreTechnique::Type::slideIn:
                                notesXml << "        <Property name=\"Slide\"><Flags>16</Flags></Property>\n";
                                break;

                            case ScoreTechnique::Type::slideOut:
                                notesXml << "        <Property name=\"Slide\"><Flags>32</Flags></Property>\n";
                                break;

                            case ScoreTechnique::Type::hammerOn:
                            case ScoreTechnique::Type::pullOff:
                                notesXml << "        <Property name=\"HopoOrigin\"><Enable/></Property>\n";
                                break;

                            case ScoreTechnique::Type::palmMute:
                                notesXml << "        <Property name=\"PalmMuted\"><Enable/></Property>\n";
                                break;

                            case ScoreTechnique::Type::naturalHarmonic:
                                notesXml << "        <Property name=\"HarmonicType\">"
                                         << "<HType>Natural</HType></Property>\n";
                                break;

                            case ScoreTechnique::Type::pinchHarmonic:
                                notesXml << "        <Property name=\"HarmonicType\">"
                                         << "<HType>Pinch</HType></Property>\n";
                                break;

                            case ScoreTechnique::Type::artificialHarmonic:
                                notesXml << "        <Property name=\"HarmonicType\">"
                                         << "<HType>Artificial</HType></Property>\n";
                                break;

                            case ScoreTechnique::Type::tapHarmonic:
                                notesXml << "        <Property name=\"HarmonicType\">"
                                         << "<HType>Tap</HType></Property>\n";
                                break;

                            case ScoreTechnique::Type::tap:
                                notesXml << "        <Property name=\"Tapped\"><Enable/></Property>\n";
                                break;

                            case ScoreTechnique::Type::vibrato:
                                notesXml << "        <Property name=\"Vibrato\"><Enable/></Property>\n";
                                break;

                            case ScoreTechnique::Type::deadNote:
                                notesXml << "        <Property name=\"Muted\"><Enable/></Property>\n";
                                break;

                            // auto-articulation.md 9 (FEAT-ASSIST): Guitar Pro's pickstroke.
                            case ScoreTechnique::Type::pickStrokeUp:
                                notesXml << "        <Property name=\"PickStroke\"><Direction>Up</Direction></Property>\n";
                                break;

                            case ScoreTechnique::Type::pickStrokeDown:
                                notesXml << "        <Property name=\"PickStroke\"><Direction>Down</Direction></Property>\n";
                                break;

                            case ScoreTechnique::Type::whammy:
                                // Whammy is a bar event in Guitar Pro: written on the beat
                                // above (SPEC-SWEEP NE-12 - it was an XML comment).
                                break;

                            // SPEC-SWEEP NE-12: the rest of the note-level techniques GPIF has.
                            case ScoreTechnique::Type::letRing:
                                notesXml << "        <Property name=\"LetRing\"><Enable/></Property>\n";
                                break;

                            case ScoreTechnique::Type::ghostNote:
                                notesXml << "        <Property name=\"AntiAccent\"><Enable/></Property>\n";
                                break;

                            case ScoreTechnique::Type::accent:
                                notesXml << "        <Property name=\"Accent\"><Flags>1</Flags></Property>\n";
                                break;

                            case ScoreTechnique::Type::staccato:
                                notesXml << "        <Property name=\"Accent\"><Flags>4</Flags></Property>\n";
                                break;

                            case ScoreTechnique::Type::trill:
                                notesXml << "        <Property name=\"Trill\"><Fret>"
                                         << juce::roundToInt (technique.value) << "</Fret></Property>\n";
                                break;

                            default:
                                break;
                        }
                    }

                    notesXml << "      </Properties>\n"
                             << "    </Note>\n";

                    ++noteId;
                }

                position = first.startBeat + length;
                i = end;
            }

            voicesXml << "    <Voice id=\"" << voiceId << "\">\n"
                      << "      <Beats>" << beatList << "</Beats>\n"
                      << "    </Voice>\n";

            voiceList << (v > 0 ? " " : "") << voiceId++;
        }

        barsXml << "    <Bar id=\"" << barId << "\">\n"
                << "      <Voices>" << voiceList << "</Voices>\n"
                << "    </Bar>\n";

        ++barId;
    }

    xml << "  </MasterBars>\n"
        << "  <Bars>\n" << barsXml << "  </Bars>\n"       // SPEC-SWEEP NE-4
        << "  <Voices>\n" << voicesXml << "  </Voices>\n"
        << "  <Beats>\n" << beatsXml << "  </Beats>\n"
        << "  <Notes>\n" << notesXml << "  </Notes>\n"
        << "  <Rhythms>\n" << rhythmsXml << "  </Rhythms>\n"
        << "</GPIF>\n";

    return xml;
}

bool NotationExporter::writeGuitarPro (const PerformanceScore& score, const juce::File& destination,
                                       const NotationExportOptions& options)
{
    const auto xml = renderGuitarProXml (score, options);

    destination.deleteFile();

    /*  A `.gp` file is a zip with the GPIF score inside it.

        JUCE can write a zip, so the container is real rather than approximated.
        Guitar Pro 7 and 8 open the result; older versions read `.gp5`, which is
        a different, binary format this does not write, and the export dialog
        says so rather than producing a file that silently will not open. */
    std::unique_ptr<juce::FileOutputStream> stream (destination.createOutputStream());

    if (stream == nullptr)
    {
        lastError = "Could not open " + destination.getFullPathName() + " for writing.";
        return false;
    }

    juce::ZipFile::Builder builder;

    auto xmlData = std::make_unique<juce::MemoryInputStream> (
        xml.toRawUTF8(), xml.getNumBytesAsUTF8(), true);

    builder.addEntry (xmlData.release(), 9, "Content/score.gpif",
                      juce::Time::getCurrentTime());

    double progress = 0.0;

    if (! builder.writeToStream (*stream, &progress))
    {
        lastError = "Could not write the Guitar Pro bundle.";
        return false;
    }

    return true;
}

//==============================================================================
bool NotationImporter::canRead (const juce::File& file)
{
    const auto extension = file.getFileExtension().toLowerCase();

    // Stated rather than implied: these are the formats that are actually
    // parsed. `.gpx` (Guitar Pro 6) and `.ptb` (PowerTab) are proprietary and
    // are reported as unsupported, with what to do instead, rather than failed.
    return extension == ".txt" || extension == ".tab"
        || extension == ".md" || extension == ".html" || extension == ".htm"
        || extension == ".musicxml" || extension == ".xml" || extension == ".mxl"
        || extension == ".mid" || extension == ".midi"
        || extension == ".gp3" || extension == ".gp4" || extension == ".gp5"
        || extension == ".gp";   // SPEC-SWEEP NE-4
}

NotationImporter::FileKind NotationImporter::detectKind (const juce::File& file)
{
    const auto extension = file.getFileExtension().toLowerCase();

    juce::uint8 head[64] = {};
    size_t got = 0;

    if (auto stream = file.createInputStream())
        got = (size_t) juce::jmax (0, stream->read (head, (int) sizeof (head)));

    if (GuitarProLegacyReader::looksLikeLegacyGuitarPro (head, got))   return FileKind::guitarProLegacy;
    if (GuitarProLegacyReader::looksLikeGpx (head, got))               return FileKind::guitarProGpx;
    if (GuitarProLegacyReader::looksLikePowerTab (head, got))          return FileKind::powerTab;
    if (got >= 4 && std::memcmp (head, "MThd", 4) == 0)                return FileKind::midi;

    if (got >= 4 && head[0] == 'P' && head[1] == 'K' && head[2] == 3 && head[3] == 4)
    {
        if (extension == ".mxl")
            return FileKind::compressedMusicXml;

        juce::ZipFile zip (file);
        for (int i = 0; i < zip.getNumEntries(); ++i)
            if (auto* entry = zip.getEntry (i))
                if (entry->filename.endsWithIgnoreCase ("score.gpif"))
                    return FileKind::guitarPro7;

        return FileKind::compressedMusicXml;
    }

    if (extension == ".gp3" || extension == ".gp4" || extension == ".gp5") return FileKind::guitarProLegacy;
    if (extension == ".gpx")                                                return FileKind::guitarProGpx;
    if (extension == ".ptb")                                                return FileKind::powerTab;
    if (extension == ".gp")                                                 return FileKind::guitarPro7;
    if (extension == ".mid" || extension == ".midi")                        return FileKind::midi;
    if (extension == ".mxl")                                                return FileKind::compressedMusicXml;

    if (extension == ".musicxml" || extension == ".xml")
        return FileKind::musicXml;

    const juce::String text = juce::String::fromUTF8 ((const char*) head, (int) got).trimStart();
    if (text.startsWith ("<?xml") && (text.contains ("score-") || extension.isEmpty()))
        return FileKind::musicXml;

    return FileKind::asciiTab;
}

bool NotationImporter::read (const juce::File& file, PerformanceScore& destination)
{
    lastError.clear();
    lastDiagnostics = {};

    if (! file.existsAsFile())
    {
        lastError = "No such file: " + file.getFullPathName();
        return false;
    }

    switch (detectKind (file))
    {
        case FileKind::guitarPro7:
            return readGuitarPro (file, destination);   // SPEC-SWEEP NE-4

        case FileKind::compressedMusicXml:
            return readCompressedMusicXml (file, destination);

        case FileKind::guitarProLegacy:
        {
            juce::MemoryBlock bytes;
            if (! file.loadFileAsData (bytes))
            {
                lastError = "Could not read " + file.getFullPathName();
                return false;
            }
            return readGuitarProLegacy (bytes.getData(), bytes.getSize(), destination);
        }

        case FileKind::guitarProGpx:
            lastError = "Guitar Pro 6 (.gpx) is a proprietary container Luthier cannot open. "
                        "In Guitar Pro, use File > Save As to write a .gp (Guitar Pro 7/8) or .gp5 file, "
                        "or export MusicXML, and open that instead.";
            return false;

        case FileKind::powerTab:
            lastError = "PowerTab (.ptb) is a proprietary binary format Luthier cannot open. "
                        "Export it as MusicXML, MIDI or ASCII tab from PowerTab and open that instead.";
            return false;

        case FileKind::midi:
            return readMidi (file, destination);

        case FileKind::musicXml:
            return readMusicXml (file.loadFileAsString(), destination);

        case FileKind::asciiTab:
        case FileKind::unknown:
            break;
    }

    return readAsciiTab (file.loadFileAsString(), destination);
}

bool NotationImporter::readGuitarProLegacy (const void* data, size_t numBytes, PerformanceScore& destination)
{
    lastError.clear();
    lastDiagnostics = {};

    if (! GuitarProLegacyReader::looksLikeLegacyGuitarPro (data, numBytes))
    {
        lastError = "That does not look like a Guitar Pro 3, 4 or 5 file. "
                    "If it came from Guitar Pro, export it as MusicXML from there and open that instead.";
        return false;
    }

    GuitarProLegacyReader reader;
    const bool ok = reader.read (data, numBytes, destination, &lastDiagnostics, preferredTrack);

    if (! ok)
        lastError = reader.getLastError();

    return ok;
}

bool NotationImporter::readCompressedMusicXml (const juce::File& file, PerformanceScore& destination)
{
    lastError.clear();
    lastDiagnostics = {};

    juce::ZipFile zip (file);
    constexpr juce::int64 kMaxEntryBytes = 64ll * 1024 * 1024;

    auto readEntry = [&] (int index) -> juce::String
    {
        const auto* entry = zip.getEntry (index);
        if (entry == nullptr || entry->uncompressedSize > kMaxEntryBytes)
            return {};

        std::unique_ptr<juce::InputStream> stream (zip.createStreamForEntry (index));
        return stream != nullptr ? stream->readEntireStreamAsString() : juce::String();
    };

    juce::String rootPath;

    for (int i = 0; i < zip.getNumEntries(); ++i)
        if (auto* entry = zip.getEntry (i))
            if (entry->filename.equalsIgnoreCase ("META-INF/container.xml"))
                if (auto xml = juce::parseXML (readEntry (i)))
                    if (auto* roots = xml->getChildByName ("rootfiles"))
                        if (auto* root = roots->getChildByName ("rootfile"))
                            rootPath = root->getStringAttribute ("full-path");

    int index = rootPath.isNotEmpty() ? zip.getIndexOfFileName (rootPath, true) : -1;

    for (int i = 0; index < 0 && i < zip.getNumEntries(); ++i)
        if (auto* entry = zip.getEntry (i))
            if (! entry->filename.startsWithIgnoreCase ("META-INF")
                && (entry->filename.endsWithIgnoreCase (".xml") || entry->filename.endsWithIgnoreCase (".musicxml")))
                index = i;

    if (index < 0)
    {
        lastError = file.getFileName() + " is not a compressed MusicXML file (no score inside).";
        return false;
    }

    const auto text = readEntry (index);

    if (text.isEmpty())
    {
        lastError = "The score inside " + file.getFileName() + " is empty or too large to open.";
        return false;
    }

    return readMusicXml (text, destination);
}

//==============================================================================
namespace
{
    /** A generic multi-track or multi-channel MIDI file is a whole band. The tab
        wants one guitar-like part: the part with a guitar/bass program, else the
        busiest pitched one. Drums (channel 10) never become tab. The conductor
        events (tempo, meter, key) are kept. Returns false when the file already
        is one part, or cannot be re-read. */
    bool chooseMelodicPart (const void* data, size_t numBytes, juce::MemoryBlock& out, juce::String& note)
    {
        if (numBytes > 32u * 1024u * 1024u)
            return false;

        juce::MemoryInputStream in (data, numBytes, false);
        juce::MidiFile file;

        if (! file.readFrom (in) || file.getTimeFormat() <= 0 || file.getNumTracks() > 256)
            return false;

        struct Group { int track = 0, channel = 0, notes = 0, program = -1; };
        std::map<std::pair<int, int>, Group> groups;
        int drumNotes = 0;

        for (int t = 0; t < file.getNumTracks(); ++t)
        {
            const auto* seq = file.getTrack (t);
            for (int i = 0; seq != nullptr && i < seq->getNumEvents(); ++i)
            {
                const auto& m = seq->getEventPointer (i)->message;

                // NotationExporter's one-track-per-string layout is one part, not a band.
                if (m.isTrackNameEvent() && m.getTextFromTextMetaEvent().startsWith ("String "))
                    return false;

                const int ch = m.getChannel();
                if (ch < 1)
                    continue;

                auto& g = groups[{ t, ch }];
                g.track = t;
                g.channel = ch;

                if (m.isNoteOn())
                {
                    if (ch == 10) ++drumNotes; else ++g.notes;
                }
                else if (m.isProgramChange() && g.program < 0)
                    g.program = m.getProgramChangeNumber();
            }
        }

        std::vector<Group> candidates;
        for (const auto& [key, g] : groups)
            if (g.notes > 0 && g.channel != 10)
                candidates.push_back (g);

        if (candidates.empty() || (candidates.size() == 1 && drumNotes == 0))
            return false;

        const auto weight = [] (const Group& g)
        {
            double w = g.notes;
            if (g.program >= 24 && g.program <= 31) w += 1.0e6;      // guitars
            else if (g.program >= 32 && g.program <= 39) w += 5.0e5; // basses
            return w;
        };

        const Group* best = &candidates.front();
        for (const auto& g : candidates)
            if (weight (g) > weight (*best))
                best = &g;

        juce::MidiFile rebuilt;
        rebuilt.setTicksPerQuarterNote (file.getTimeFormat());

        juce::MidiMessageSequence conductor, part;
        for (int t = 0; t < file.getNumTracks(); ++t)
        {
            const auto* seq = file.getTrack (t);
            for (int i = 0; seq != nullptr && i < seq->getNumEvents(); ++i)
            {
                const auto& m = seq->getEventPointer (i)->message;
                if (m.isTempoMetaEvent() || m.isTimeSignatureMetaEvent() || m.isKeySignatureMetaEvent())
                    conductor.addEvent (m);
                else if (t == best->track && m.getChannel() == best->channel)
                    part.addEvent (m);
            }
        }

        conductor.updateMatchedPairs();
        part.updateMatchedPairs();
        rebuilt.addTrack (conductor);
        rebuilt.addTrack (part);

        juce::MemoryOutputStream stream;
        if (! rebuilt.writeTo (stream, 1))
            return false;

        out = stream.getMemoryBlock();
        note = "Read track " + juce::String (best->track + 1) + ", channel " + juce::String (best->channel)
               + " of a " + juce::String ((int) candidates.size() + (drumNotes > 0 ? 1 : 0))
               + "-part MIDI file" + (drumNotes > 0 ? " (drums ignored)." : ".");
        return true;
    }
}

//==============================================================================
bool NotationImporter::readMidi (const juce::File& file, PerformanceScore& destination)
{
    lastError.clear();
    lastDiagnostics = {};

    juce::MemoryBlock bytes;

    if (! file.existsAsFile() || ! file.loadFileAsData (bytes))
    {
        lastError = "Could not read " + file.getFullPathName();
        return false;
    }

    return readMidi (bytes.getData(), bytes.getSize(), destination);
}

bool NotationImporter::readMidi (const void* data, size_t numBytes, PerformanceScore& destination)
{
    lastError.clear();
    lastDiagnostics = {};

    constexpr double rate = 48000.0;
    auto performancePtr = std::make_unique<MidiPerformance> (rate);
    auto result = MidiProfiles::importFromMemory (data, numBytes, *performancePtr, rate);

    if (! result.ok)
    {
        lastError = result.error.isNotEmpty() ? result.error : juce::String ("Not a MIDI file.");
        return false;
    }

    // A generic multi-part file: take the guitar-like part, not the whole band.
    juce::String partNote;
    if (result.detectedProfile != MidiProfile::luthier)
    {
        juce::MemoryBlock part;
        if (chooseMelodicPart (data, numBytes, part, partNote))
        {
            auto narrowed = std::make_unique<MidiPerformance> (rate);
            auto narrowedResult = MidiProfiles::importFromMemory (part.getData(), part.getSize(), *narrowed, rate);

            if (narrowedResult.ok)
            {
                performancePtr = std::move (narrowed);
                result = std::move (narrowedResult);
            }
            else
                partNote.clear();
        }
    }

    auto& performance = *performancePtr;

    destination.clear();

    /*  tab-import-export 8.1: the instrument. A generic file says nothing
        about it, so the range of the notes decides: a part that lives below
        the low E and never climbs past C4 is a bass. */
    {
        int lowest = 128, highest = -1;
        for (const auto& entry : performance.getMessages())
        {
            if (entry.message.isNoteOn())
            {
                lowest = juce::jmin (lowest, entry.message.getNoteNumber());
                highest = juce::jmax (highest, entry.message.getNoteNumber());
            }
        }

        auto& track = destination.getTrack (0);
        if (highest >= 0 && highest <= 60 && lowest < 40)
        {
            track.numStrings = 4;
            track.tuning = { { 43, 38, 33, 28, 0, 0, 0, 0, 0, 0, 0, 0 } };
            track.name = "Bass";
            destination.getMeta().tuningName = "Bass";
        }
    }

    performance.toScore (destination);

    if (destination.getTotalNoteCount() == 0)
    {
        lastError = "No notes in that MIDI file.";
        return false;
    }

    const bool luthierProfile = result.detectedProfile == MidiProfile::luthier;
    TabFingering::Result fingering;

    if (! luthierProfile && ! TabFingering::isPlausible (destination))
    {
        fingering = TabFingering::assign (destination);
        lastDiagnostics.warnings.add ("Generic MIDI: strings and frets are a best guess (tab-import-export 8)");
    }

    if (fingering.notesClamped > 0)
        lastDiagnostics.warnings.add (juce::String (fingering.notesClamped)
                                        + " note(s) outside the instrument's range were moved onto it");

    for (const auto& w : result.warnings)
        lastDiagnostics.warnings.add (w);

    if (partNote.isNotEmpty())
        lastDiagnostics.warnings.add (partNote);

    const auto& track = destination.getTrack (0);
    lastDiagnostics.notes = destination.getTotalNoteCount();
    lastDiagnostics.measures = (int) track.measures.size();
    lastDiagnostics.numStrings = track.numStrings;
    lastDiagnostics.tuningFromHeader = luthierProfile;
    lastDiagnostics.tempoFromHeader = true;
    lastDiagnostics.timeSignatureFromHeader = true;
    lastDiagnostics.systems = 1;
    lastDiagnostics.staffLines = track.numStrings;

    return true;
}

bool NotationImporter::readAsciiTab (const juce::String& text, PerformanceScore& destination,
                                     TabImportDiagnostics* diagnostics)
{
    lastError.clear();

    // tab-import-export 7: the dialect-tolerant reader; what it skipped is
    // kept so the panel can say how much of the page was read.
    // The whole-document recovery pipeline (markdown/HTML wrappers, unicode
    // dashes, numbered or reversed strings, UG chord markup, chord-only charts,
    // size and time limits) is the one reader; it wraps the plain reader.
    TabImportPipeline pipeline;
    TabImportDiagnostics recovered;
    const bool ok = pipeline.read (text, destination, &recovered);

    lastDiagnostics = recovered;

    if (diagnostics != nullptr)
        *diagnostics = lastDiagnostics;

    if (! ok)
        lastError = pipeline.getLastError();

    return ok;
}

bool NotationImporter::readMusicXml (const juce::String& text, PerformanceScore& destination)
{
    lastError.clear();
    lastDiagnostics = {};

    auto xml = juce::parseXML (text);

    if (xml == nullptr)
    {
        lastError = "That file is not valid XML.";
        return false;
    }

    if (xml->hasTagName ("score-timewise"))
    {
        lastError = "That MusicXML file is time-wise (score-timewise), which Luthier does not read. "
                    "Re-export it as part-wise MusicXML (the default in most notation programs).";
        return false;
    }

    // Which part: the first with string/fret data (a tab part), else the first
    // pitched part that is not percussion, else the first.
    const juce::XmlElement* part = nullptr;
    {
        const juce::XmlElement* firstPitched = nullptr;
        int partCount = 0;

        for (auto* candidate : xml->getChildWithTagNameIterator ("part"))
        {
            ++partCount;
            if (partCount > 256)
                break;

            bool hasFret = false, hasPitch = false, percussion = false;
            int scanned = 0;

            for (auto* m : candidate->getChildWithTagNameIterator ("measure"))
            {
                if (++scanned > 64)
                    break;

                if (auto* a = m->getChildByName ("attributes"))
                    if (auto* clef = a->getChildByName ("clef"))
                        if (clef->getChildElementAllSubText ("sign", {}).equalsIgnoreCase ("percussion"))
                            percussion = true;

                for (auto* n : m->getChildWithTagNameIterator ("note"))
                {
                    if (n->getChildByName ("pitch") != nullptr) hasPitch = true;
                    if (auto* nt = n->getChildByName ("notations"))
                        if (auto* tech = nt->getChildByName ("technical"))
                            if (tech->getChildByName ("fret") != nullptr)
                                hasFret = true;
                }

                if (hasFret)
                    break;
            }

            if (hasFret && ! percussion)
            {
                part = candidate;
                break;
            }

            if (hasPitch && ! percussion && firstPitched == nullptr)
                firstPitched = candidate;
        }

        if (part == nullptr)
            part = firstPitched != nullptr ? firstPitched : xml->getChildByName ("part");
    }

    if (part == nullptr)
    {
        lastError = "That MusicXML file has no part to read.";
        return false;
    }

    destination.clear();

    double tempo = 120.0;
    int numerator = 4, denominator = 4;

    if (auto* firstMeasure = part->getChildByName ("measure"))
    {
        if (auto* attributes = firstMeasure->getChildByName ("attributes"))
            if (auto* time = attributes->getChildByName ("time"))
            {
                numerator = time->getChildElementAllSubText ("beats", "4").getIntValue();
                denominator = time->getChildElementAllSubText ("beat-type", "4").getIntValue();
            }

        for (auto* direction : firstMeasure->getChildWithTagNameIterator ("direction"))
        {
            if (auto* sound = direction->getChildByName ("sound"))
                if (sound->hasAttribute ("tempo"))
                {
                    tempo = sound->getDoubleAttribute ("tempo", 120.0);
                    break;
                }
        }

        tempo = juce::jlimit (20.0, 400.0, tempo);
    }

    destination.beginCapture (tempo, juce::jmax (1, numerator), juce::jmax (1, denominator));

    auto& track = destination.getTrack (0);

    int divisions = 480;
    double beat = 0.0;
    int notesRead = 0;
    int transposeSemitones = 0;
    bool anyFretData = false, anyFretless = false;
    std::vector<int> tuningByLine;               // MusicXML line 1 = lowest string

    // A note is held open until the next one on its string (or the end), so a
    // tie can lengthen it.
    struct Open { bool open = false; double end = 0.0; };
    std::array<Open, kMaxStrings> openNotes {};

    auto flushString = [&] (int stringIndex, double at)
    {
        auto& o = openNotes[(size_t) juce::jlimit (0, kMaxStrings - 1, stringIndex)];
        if (o.open)
        {
            destination.noteEnded (stringIndex, juce::jmin (o.end, at));
            o.open = false;
        }
    };

    const double beatsPerMeasure = (double) juce::jmax (1, numerator) * 4.0
                                     / (double) juce::jmax (1, denominator);

    int measureIndex = 0;

    for (auto* measure : part->getChildWithTagNameIterator ("measure"))
    {
        if (auto* attributes = measure->getChildByName ("attributes"))
        {
            divisions = juce::jmax (1, attributes->getChildElementAllSubText ("divisions",
                                                                              "480").getIntValue());

            if (auto* transpose = attributes->getChildByName ("transpose"))
                transposeSemitones = juce::jlimit (-48, 48,
                                                   transpose->getChildElementAllSubText ("chromatic", "0").getIntValue()
                                                   + 12 * transpose->getChildElementAllSubText ("octave-change", "0").getIntValue());

            if (auto* details = attributes->getChildByName ("staff-details"))
            {
                track.numStrings = juce::jlimit (
                    1, kMaxStrings,
                    details->getChildElementAllSubText ("staff-lines", "6").getIntValue());

                tuningByLine.assign ((size_t) track.numStrings + 1, -1);

                for (auto* st : details->getChildWithTagNameIterator ("staff-tuning"))
                {
                    const int line = st->getIntAttribute ("line", 0);
                    if (line < 1 || line > track.numStrings)
                        continue;

                    static const int offs[7] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
                    const auto stepName = st->getChildElementAllSubText ("tuning-step", "E");
                    const int idx = juce::jlimit (0, 6, (int) (stepName.isEmpty() ? 4 : stepName[0] - 'A'));
                    const int midi = (st->getChildElementAllSubText ("tuning-octave", "2").getIntValue() + 1) * 12
                                       + offs[idx] + st->getChildElementAllSubText ("tuning-alter", "0").getIntValue();
                    tuningByLine[(size_t) line] = juce::jlimit (0, 127, midi);
                }

                bool complete = ! tuningByLine.empty();
                for (int line = 1; line <= track.numStrings && complete; ++line)
                    complete = tuningByLine[(size_t) line] >= 0;

                if (complete)
                    for (int line = 1; line <= track.numStrings; ++line)
                        track.tuning[(size_t) (track.numStrings - line)] = tuningByLine[(size_t) line];

                if (auto* capo = details->getChildByName ("capo"))
                    track.capoFret = juce::jlimit (0, 24, capo->getAllSubText().getIntValue());
            }
        }

        beat = (double) measureIndex * beatsPerMeasure;
        double voiceBeat = beat;
        double pendingGraceBeats = 0.0;   // SPEC-SWEEP NE-9
        double lastNoteStart = beat;   // where a <chord/> note starts

        for (auto* element : measure->getChildIterator())
        {
            if (element->hasTagName ("backup"))
            {
                const double duration =
                    element->getChildElementAllSubText ("duration", "0").getDoubleValue()
                      / (double) divisions;

                voiceBeat = juce::jmax (beat, voiceBeat - duration);
                continue;
            }

            if (element->hasTagName ("forward"))
            {
                voiceBeat += element->getChildElementAllSubText ("duration", "0").getDoubleValue()
                               / (double) divisions;
                continue;
            }

            if (! element->hasTagName ("note"))
                continue;

            const double duration =
                element->getChildElementAllSubText ("duration", "0").getDoubleValue()
                  / (double) divisions;

            const bool isChord = element->getChildByName ("chord") != nullptr;
            const bool isRest = element->getChildByName ("rest") != nullptr;

            if (isRest)
            {
                voiceBeat += duration;
                continue;
            }

            int stringNumber = 0, fret = 0;

            if (auto* notations = element->getChildByName ("notations"))
            {
                if (auto* technical = notations->getChildByName ("technical"))
                {
                    stringNumber = technical->getChildElementAllSubText ("string", "0").getIntValue();
                    fret = technical->getChildElementAllSubText ("fret", "0").getIntValue();
                }
            }

            // MusicXML numbers strings from 1 = the highest, the score from
            // 0 = the highest.
            if (stringNumber > 0) anyFretData = true; else anyFretless = true;

            const int stringIndex = (stringNumber > 0)
                                      ? juce::jlimit (0, juce::jmax (1, track.numStrings) - 1, stringNumber - 1)
                                      : 0;

            int midiNote = 60;
            bool havePitch = false;

            if (auto* pitch = element->getChildByName ("pitch"))
            {
                havePitch = true;
                const auto step = pitch->getChildElementAllSubText ("step", "C");
                const int alter = pitch->getChildElementAllSubText ("alter", "0").getIntValue();
                const int octave = pitch->getChildElementAllSubText ("octave", "4").getIntValue();

                static const char* const letters = "CDEFGAB";
                static const int offsets[7] = { 0, 2, 4, 5, 7, 9, 11 };

                int letterIndex = 0;

                for (int i = 0; i < 7; ++i)
                    if (step[0] == letters[i])
                        letterIndex = i;

                midiNote = juce::jlimit (0, 127,
                                         (octave + 1) * 12 + offsets[letterIndex] + alter + transposeSemitones);
            }
            else if (stringNumber > 0)
            {
                midiNote = juce::jlimit (0, 127, track.tuning[(size_t) stringIndex] + track.capoFret + fret);
            }

            juce::ignoreUnused (havePitch);

            bool tieStop = false;
            for (auto* tie : element->getChildWithTagNameIterator ("tie"))
                if (tie->getStringAttribute ("type") == "stop")
                    tieStop = true;
            if (auto* notations = element->getChildByName ("notations"))
                for (auto* tied : notations->getChildWithTagNameIterator ("tied"))
                    if (tied->getStringAttribute ("type") == "stop")
                        tieStop = true;

            // SPEC-SWEEP NE-9: a grace note takes the first eighth of a beat
            // of the note it leads into, as the writer made it.
            if (element->getChildByName ("grace") != nullptr)
            {
                constexpr double graceLength = 0.125;
                flushString (stringIndex, voiceBeat);
                destination.noteStarted (stringIndex, fret, midiNote, 440.0, 0.8, voiceBeat);
                destination.noteEnded (stringIndex, voiceBeat + graceLength);
                pendingGraceBeats = graceLength;
                ++notesRead;
                continue;
            }

            // A <chord/> note sounds with the note before it; voiceBeat has
            // already moved past that one.
            double startBeat = isChord ? lastNoteStart : voiceBeat;
            double soundingBeats = duration;

            if (! isChord && pendingGraceBeats > 0.0)
            {
                startBeat += pendingGraceBeats;
                soundingBeats = juce::jmax (0.0625, duration - pendingGraceBeats);
                pendingGraceBeats = 0.0;
            }

            lastNoteStart = startBeat;

            // A tied note lengthens the one it continues rather than restarting.
            if (tieStop)
            {
                auto& o = openNotes[(size_t) stringIndex];
                if (o.open && std::abs (o.end - startBeat) < 1.0e-3)
                {
                    o.end = startBeat + juce::jmax (0.0625, soundingBeats);
                    if (! isChord)
                        voiceBeat = startBeat + soundingBeats;
                    continue;
                }
            }

            flushString (stringIndex, startBeat);
            destination.noteStarted (stringIndex, fret, midiNote, 440.0 * std::pow (2.0, (midiNote - 69) / 12.0), 0.8, startBeat);

            // The techniques MusicXML carries, read back.
            if (auto* notations = element->getChildByName ("notations"))
            {
                if (auto* technical = notations->getChildByName ("technical"))
                {
                    if (technical->getChildByName ("hammer-on") != nullptr)
                        destination.addTechnique (stringIndex, { ScoreTechnique::Type::hammerOn });

                    if (technical->getChildByName ("pull-off") != nullptr)
                        destination.addTechnique (stringIndex, { ScoreTechnique::Type::pullOff });

                    if (technical->getChildByName ("tap") != nullptr)
                        destination.addTechnique (stringIndex, { ScoreTechnique::Type::tap });

                    if (auto* bend = technical->getChildByName ("bend"))
                    {
                        ScoreTechnique technique;
                        technique.type = (bend->getChildByName ("pre-bend") != nullptr)
                                           ? ScoreTechnique::Type::preBend
                                           : (bend->getChildByName ("release") != nullptr)
                                               ? ScoreTechnique::Type::bendRelease
                                               : ScoreTechnique::Type::bend;

                        technique.value = bend->getChildElementAllSubText ("bend-alter",
                                                                          "0").getDoubleValue();

                        destination.addTechnique (stringIndex, technique);
                    }

                    if (auto* harmonic = technical->getChildByName ("harmonic"))
                        destination.addTechnique (
                            stringIndex,
                            { harmonic->getChildByName ("natural") != nullptr
                                ? ScoreTechnique::Type::naturalHarmonic
                                : ScoreTechnique::Type::artificialHarmonic });
                }

                if (notations->getChildByName ("slide") != nullptr)
                    destination.addTechnique (stringIndex, { ScoreTechnique::Type::slideShift });

                if (notations->getChildByName ("glissando") != nullptr)
                    destination.addTechnique (stringIndex, { ScoreTechnique::Type::slideLegato });
            }

            openNotes[(size_t) stringIndex].open = true;
            openNotes[(size_t) stringIndex].end = startBeat + juce::jmax (0.0625, soundingBeats);

            ++notesRead;

            if (! isChord)
                voiceBeat = startBeat + soundingBeats;   // SPEC-SWEEP NE-9: the grace's time included
        }

        ++measureIndex;
    }

    for (int i = 0; i < kMaxStrings; ++i)
        flushString (i, 1.0e12);

    destination.endCapture ((double) measureIndex * beatsPerMeasure);

    if (notesRead == 0)
    {
        lastError = "That MusicXML file contains no notes.";
        return false;
    }

    // Notation-only MusicXML (MuseScore, Finale, Sibelius staff exports) has
    // pitches and no strings: finger it the way a MIDI import is.
    if (anyFretless && ! anyFretData)
    {
        const auto result = TabFingering::assign (destination, 0, 24);
        lastDiagnostics.notes = notesRead;
        lastDiagnostics.warnings.add ("That MusicXML has no string/fret data; "
                                      + juce::String (result.notesFingered) + " notes were fingered automatically.");
    }

    return true;
}

//==============================================================================
// SPEC-SWEEP NE-4: Guitar Pro 7/8.
//==============================================================================
bool NotationImporter::readGuitarPro (const juce::File& file, PerformanceScore& destination)
{
    juce::ZipFile zip (file);

    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        const auto* entry = zip.getEntry (i);

        if (entry == nullptr || ! entry->filename.endsWithIgnoreCase ("score.gpif"))
            continue;

        if (entry->uncompressedSize > 64ll * 1024 * 1024)
        {
            lastError = file.getFileName() + " holds a score too large to open.";
            return false;
        }

        std::unique_ptr<juce::InputStream> stream (zip.createStreamForEntry (i));

        if (stream == nullptr)
            break;

        return readGpif (stream->readEntireStreamAsString(), destination);
    }

    lastError = file.getFileName() + " is not a Guitar Pro 7/8 file (no Content/score.gpif inside).";
    return false;
}

bool NotationImporter::readGpif (const juce::String& text, PerformanceScore& destination)
{
    lastError.clear();

    const auto root = juce::parseXML (text);

    if (root == nullptr || ! root->hasTagName ("GPIF"))
    {
        lastError = "The Guitar Pro score could not be read.";
        return false;
    }

    // Index each list by id.
    auto indexById = [&root] (const char* listName, const char* itemName)
    {
        std::map<int, const juce::XmlElement*> items;

        if (auto* list = root->getChildByName (listName))
            for (auto* item : list->getChildWithTagNameIterator (itemName))
                items[item->getIntAttribute ("id", -1)] = item;

        return items;
    };

    const auto bars = indexById ("Bars", "Bar");
    const auto voices = indexById ("Voices", "Voice");
    const auto beats = indexById ("Beats", "Beat");
    const auto notes = indexById ("Notes", "Note");
    const auto rhythms = indexById ("Rhythms", "Rhythm");

    auto ids = [] (const juce::String& list)
    {
        std::vector<int> out;

        for (const auto& token : juce::StringArray::fromTokens (list, " ", ""))
            if (token.isNotEmpty())
                out.push_back (token.getIntValue());

        return out;
    };

    auto property = [] (const juce::XmlElement* owner, const char* name) -> const juce::XmlElement*
    {
        if (owner == nullptr)
            return nullptr;

        if (auto* properties = owner->getChildByName ("Properties"))
            for (auto* p : properties->getChildWithTagNameIterator ("Property"))
                if (p->getStringAttribute ("name") == name)
                    return p;

        return nullptr;
    };

    // ---- the score's header ---------------------------------------------------------
    const double tempo = juce::jlimit (20.0, 400.0, root->getChildByName ("Score") != nullptr
                                                      ? root->getChildByName ("Score")->getChildElementAllSubText ("Tempo", "120").getDoubleValue()
                                                      : 120.0);
    int numerator = 4, denominator = 4;

    if (auto* masterBars = root->getChildByName ("MasterBars"))
        if (auto* first = masterBars->getChildByName ("MasterBar"))
        {
            const auto time = juce::StringArray::fromTokens (first->getChildElementAllSubText ("Time", "4/4"), "/", "");

            if (time.size() == 2)
                numerator = juce::jmax (1, time[0].getIntValue()), denominator = juce::jmax (1, time[1].getIntValue());
        }

    destination.beginCapture (tempo > 0.0 ? tempo : 120.0, numerator, denominator);

    if (auto* score = root->getChildByName ("Score"))
    {
        destination.getMeta().title = score->getChildElementAllSubText ("Title", {});
        destination.getMeta().artist = score->getChildElementAllSubText ("Artist", {});
    }

    auto& track = destination.getTrack (0);
    std::vector<int> pitches;   // GPIF string 0 is the lowest

    if (auto* tracks = root->getChildByName ("Tracks"))
        if (auto* firstTrack = tracks->getChildByName ("Track"))
        {
            track.name = firstTrack->getChildElementAllSubText ("Name", track.name);

            // The tuning, from the staff's properties.
            std::function<void (const juce::XmlElement&)> findTuning = [&] (const juce::XmlElement& e)
            {
                if (e.hasTagName ("Property") && e.getStringAttribute ("name") == "Tuning")
                    pitches = ids (e.getChildElementAllSubText ("Pitches", {}));
                else if (e.hasTagName ("Property") && e.getStringAttribute ("name") == "CapoFret")
                    track.capoFret = e.getChildElementAllSubText ("Fret", "0").getIntValue();

                for (auto* child : e.getChildIterator())
                    findTuning (*child);
            };

            findTuning (*firstTrack);
        }

    if (pitches.empty())
        pitches = { 40, 45, 50, 55, 59, 64 };

    track.numStrings = juce::jlimit (1, kMaxStrings, (int) pitches.size());

    for (int s = 0; s < track.numStrings; ++s)
        track.tuning[(size_t) s] = pitches[(size_t) (track.numStrings - 1 - s)];

    // ---- the bars ----------------------------------------------------------------------
    const auto noteValueBeats = [] (const juce::XmlElement* rhythm)
    {
        if (rhythm == nullptr)
            return 1.0;

        const auto value = rhythm->getChildElementAllSubText ("NoteValue", "Quarter");
        double beats = 1.0;

        if (value == "Whole")        beats = 4.0;
        else if (value == "Half")    beats = 2.0;
        else if (value == "Quarter") beats = 1.0;
        else if (value == "Eighth")  beats = 0.5;
        else if (value == "16th")    beats = 0.25;
        else if (value == "32nd")    beats = 0.125;
        else if (value == "64th")    beats = 0.0625;

        const int dots = rhythm->getChildByName ("AugmentationDot") != nullptr
                           ? rhythm->getChildByName ("AugmentationDot")->getIntAttribute ("count", 0) : 0;

        double add = beats * 0.5, total = beats;

        for (int d = 0; d < dots; ++d, add *= 0.5)
            total += add;

        return total;
    };

    int notesRead = 0;
    double barStart = 0.0;

    if (auto* masterBars = root->getChildByName ("MasterBars"))
    {
        for (auto* masterBar : masterBars->getChildWithTagNameIterator ("MasterBar"))
        {
            const auto time = juce::StringArray::fromTokens (masterBar->getChildElementAllSubText ("Time", "4/4"), "/", "");
            const double barBeats = time.size() == 2 ? juce::jmax (1, time[0].getIntValue()) * 4.0 / juce::jmax (1, time[1].getIntValue())
                                                     : 4.0;

            const auto barIds = ids (masterBar->getChildElementAllSubText ("Bars", {}));

            if (! barIds.empty() && bars.count (barIds.front()) > 0)
            {
                for (const int voiceIdNumber : ids (bars.at (barIds.front())->getChildElementAllSubText ("Voices", {})))
                {
                    if (voiceIdNumber < 0 || voices.count (voiceIdNumber) == 0)
                        continue;

                    double position = barStart;

                    for (const int beatIdNumber : ids (voices.at (voiceIdNumber)->getChildElementAllSubText ("Beats", {})))
                    {
                        if (beats.count (beatIdNumber) == 0)
                            continue;

                        const auto* beat = beats.at (beatIdNumber);
                        const juce::XmlElement* rhythm = nullptr;

                        if (auto* ref = beat->getChildByName ("Rhythm"))
                            if (rhythms.count (ref->getIntAttribute ("ref", -1)) > 0)
                                rhythm = rhythms.at (ref->getIntAttribute ("ref", -1));

                        const double length = noteValueBeats (rhythm);

                        for (const int noteIdNumber : ids (beat->getChildElementAllSubText ("Notes", {})))
                        {
                            if (notes.count (noteIdNumber) == 0)
                                continue;

                            const auto* note = notes.at (noteIdNumber);
                            const auto* stringProperty = property (note, "String");
                            const auto* fretProperty = property (note, "Fret");

                            if (stringProperty == nullptr || fretProperty == nullptr)
                                continue;

                            const int gpString = juce::jlimit (0, track.numStrings - 1,
                                                               stringProperty->getChildElementAllSubText ("String", "0").getIntValue());
                            const int fret = fretProperty->getChildElementAllSubText ("Fret", "0").getIntValue();
                            const int stringIndex = track.numStrings - 1 - gpString;
                            const int midi = juce::jlimit (0, 127, pitches[(size_t) gpString] + fret);

                            destination.noteStarted (stringIndex, fret, midi, 440.0 * std::pow (2.0, (midi - 69) / 12.0),
                                                     0.8, position);

                            // The note techniques this writer (and GP7) name.
                            if (auto* bend = property (note, "BendDestinationValue"))
                            {
                                ScoreTechnique t;
                                t.type = ScoreTechnique::Type::bend;
                                t.value = bend->getChildElementAllSubText ("Float", "0").getDoubleValue() / 50.0;
                                destination.addTechnique (stringIndex, t);
                            }

                            if (auto* slide = property (note, "Slide"))
                            {
                                const int flags = slide->getChildElementAllSubText ("Flags", "0").getIntValue();
                                const std::pair<int, ScoreTechnique::Type> slides[] =
                                {
                                    { 1, ScoreTechnique::Type::slideUp }, { 2, ScoreTechnique::Type::slideDown },
                                    { 4, ScoreTechnique::Type::slideLegato }, { 8, ScoreTechnique::Type::slideShift },
                                    { 16, ScoreTechnique::Type::slideIn }, { 32, ScoreTechnique::Type::slideOut }
                                };

                                for (const auto& [bit, type] : slides)
                                    if ((flags & bit) != 0)
                                        destination.addTechnique (stringIndex, { type });
                            }

                            if (property (note, "HopoOrigin") != nullptr)
                                destination.addTechnique (stringIndex, { ScoreTechnique::Type::hammerOn });

                            if (property (note, "PalmMuted") != nullptr)
                                destination.addTechnique (stringIndex, { ScoreTechnique::Type::palmMute });

                            if (property (note, "Muted") != nullptr)
                                destination.addTechnique (stringIndex, { ScoreTechnique::Type::deadNote });

                            if (property (note, "Tapped") != nullptr)
                                destination.addTechnique (stringIndex, { ScoreTechnique::Type::tap });

                            if (property (note, "Vibrato") != nullptr)
                                destination.addTechnique (stringIndex, { ScoreTechnique::Type::vibrato });

                            if (property (note, "LetRing") != nullptr)
                                destination.addTechnique (stringIndex, { ScoreTechnique::Type::letRing });

                            if (auto* harmonic = property (note, "HarmonicType"))
                            {
                                const auto type = harmonic->getChildElementAllSubText ("HType", "Natural");
                                destination.addTechnique (stringIndex, { type == "Natural" ? ScoreTechnique::Type::naturalHarmonic
                                                                       : type == "Pinch"   ? ScoreTechnique::Type::pinchHarmonic
                                                                       : type == "Tap"     ? ScoreTechnique::Type::tapHarmonic
                                                                                           : ScoreTechnique::Type::artificialHarmonic });
                            }

                            destination.noteEnded (stringIndex, position + length);
                            ++notesRead;
                        }

                        position += length;
                    }
                }
            }

            barStart += barBeats;
        }
    }

    destination.endCapture (barStart);

    if (notesRead == 0)
    {
        lastError = "That Guitar Pro file contains no notes Luthier can read.";
        return false;
    }

    return true;
}

} // namespace luthier
