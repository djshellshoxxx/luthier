#include "NotationExport.h"
#include "AsciiTabWriter.h" // FEAT2-TAB
#include "TabFingering.h"  // tab-import-export 8
#include "../Export/MidiProfiles.h"

#include <algorithm>
#include <cmath>

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

            for (size_t noteIndex = 0; noteIndex < voice.notes.size(); ++noteIndex)
            {
                const auto& note = voice.notes[noteIndex];

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
                const bool isChordMember = noteIndex > 0
                    && std::abs (voice.notes[noteIndex - 1].startBeat - note.startBeat) < 1.0e-6;

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

    xml << "          </Properties>\n"
        << "        </Staff>\n"
        << "      </Staves>\n"
        << "    </Track>\n"
        << "  </Tracks>\n"
        << "  <MasterBars>\n";

    juce::StringArray diagrammedChords;

    int beatId = 0;
    int noteId = 0;

    juce::String beatsXml, notesXml, rhythmsXml;

    for (int measureIndex = firstMeasure; measureIndex <= lastMeasure; ++measureIndex)
    {
        if (! juce::isPositiveAndBelow (measureIndex, (int) track.measures.size()))
            break;

        const auto& measure = track.measures[(size_t) measureIndex];

        xml << "    <MasterBar>\n"
            << "      <Time>" << measure.timeSignatureNumerator << "/"
            << measure.timeSignatureDenominator << "</Time>\n";

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

        // ---- the beats and notes of this bar ---------------------------------------
        for (const auto* note : measure.collectNotes())
        {
            beatsXml << "    <Beat id=\"" << beatId << "\">\n"
                     << "      <Notes>" << noteId << "</Notes>\n"
                     << "      <Rhythm ref=\"" << beatId << "\"/>\n"
                     << "    </Beat>\n";

            rhythmsXml << "    <Rhythm id=\"" << beatId << "\">\n"
                       << "      <NoteValue>" << noteTypeForBeats (note->durationBeats)
                       << "</NoteValue>\n"
                       << "      <AugmentationDot count=\"" << dotsForBeats (note->durationBeats)
                       << "\"/>\n"
                       << "    </Rhythm>\n";

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
                        // Whammy is a bar event in Guitar Pro, so it is written on
                        // the beat rather than on the note.
                        beatsXml << "    <!-- whammy " << juce::String (technique.value, 2)
                                 << " on beat " << beatId << " -->\n";
                        break;

                    default:
                        break;
                }
            }

            notesXml << "      </Properties>\n"
                     << "    </Note>\n";

            ++beatId;
            ++noteId;
        }
    }

    xml << "  </MasterBars>\n"
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
    // parsed. `.gp5`, `.gp` and `.ptb` are binary and proprietary, and are
    // reported as unsupported rather than failed.
    return extension == ".txt" || extension == ".tab"
        || extension == ".musicxml" || extension == ".xml"
        || extension == ".mid" || extension == ".midi";
}

bool NotationImporter::read (const juce::File& file, PerformanceScore& destination)
{
    lastError.clear();

    if (! file.existsAsFile())
    {
        lastError = "No such file: " + file.getFullPathName();
        return false;
    }

    const auto extension = file.getFileExtension().toLowerCase();

    if (extension == ".gp5" || extension == ".gp" || extension == ".gpx" || extension == ".ptb")
    {
        lastError = "Luthier reads ASCII tab and MusicXML. "
                    + extension.substring (1).toUpperCase()
                    + " is a proprietary binary format; export it as MusicXML "
                      "from Guitar Pro and open that instead.";
        return false;
    }

    if (extension == ".mid" || extension == ".midi")
        return readMidi (file, destination);

    const auto text = file.loadFileAsString();

    if (extension == ".musicxml" || extension == ".xml")
        return readMusicXml (text, destination);

    return readAsciiTab (text, destination);
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
    MidiPerformance performance (rate);
    const auto result = MidiProfiles::importFromMemory (data, numBytes, performance, rate);

    if (! result.ok)
    {
        lastError = result.error.isNotEmpty() ? result.error : juce::String ("Not a MIDI file.");
        return false;
    }

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
    AsciiTabReader reader;
    const bool ok = reader.read (text, destination, &lastDiagnostics);

    if (diagnostics != nullptr)
        *diagnostics = lastDiagnostics;

    if (! ok)
        lastError = reader.getLastError();

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

    auto* part = xml->getChildByName ("part");

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

        if (auto* direction = firstMeasure->getChildByName ("direction"))
            if (auto* sound = direction->getChildByName ("sound"))
                tempo = sound->getDoubleAttribute ("tempo", 120.0);
    }

    destination.beginCapture (tempo, juce::jmax (1, numerator), juce::jmax (1, denominator));

    auto& track = destination.getTrack (0);

    int divisions = 480;
    double beat = 0.0;
    int notesRead = 0;

    const double beatsPerMeasure = (double) juce::jmax (1, numerator) * 4.0
                                     / (double) juce::jmax (1, denominator);

    int measureIndex = 0;

    for (auto* measure : part->getChildWithTagNameIterator ("measure"))
    {
        if (auto* attributes = measure->getChildByName ("attributes"))
        {
            divisions = juce::jmax (1, attributes->getChildElementAllSubText ("divisions",
                                                                              "480").getIntValue());

            if (auto* details = attributes->getChildByName ("staff-details"))
            {
                track.numStrings = juce::jlimit (
                    1, kMaxStrings,
                    details->getChildElementAllSubText ("staff-lines", "6").getIntValue());
            }
        }

        beat = (double) measureIndex * beatsPerMeasure;
        double voiceBeat = beat;
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
            const int stringIndex = (stringNumber > 0)
                                      ? juce::jlimit (0, kMaxStrings - 1, stringNumber - 1)
                                      : 0;

            int midiNote = 60;

            if (auto* pitch = element->getChildByName ("pitch"))
            {
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
                                         (octave + 1) * 12 + offsets[letterIndex] + alter);
            }

            // A <chord/> note sounds with the note before it; voiceBeat has
            // already moved past that one.
            const double startBeat = isChord ? lastNoteStart : voiceBeat;
            lastNoteStart = startBeat;

            destination.noteStarted (stringIndex, fret, midiNote, 440.0, 0.8, startBeat);

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

            destination.noteEnded (stringIndex, startBeat + juce::jmax (0.0625, duration));

            ++notesRead;

            if (! isChord)
                voiceBeat += duration;
        }

        ++measureIndex;
    }

    destination.endCapture ((double) measureIndex * beatsPerMeasure);

    if (notesRead == 0)
    {
        lastError = "That MusicXML file contains no notes.";
        return false;
    }

    return true;
}

} // namespace luthier
