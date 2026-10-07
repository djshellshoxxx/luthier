#include "AsciiTabWriter.h"

#include <algorithm>
#include <cmath>

namespace luthier
{
namespace
{
    using Type = ScoreTechnique::Type;
    using Density = NotationExportOptions::SymbolDensity;
    constexpr double epsilon = 1.0e-9;

    double barBeats (const ScoreMeasure& measure)
    {
        return juce::jlimit (1, 32, measure.timeSignatureNumerator) * 4.0
             / juce::jlimit (1, 32, measure.timeSignatureDenominator);
    }

    juce::String singleLine (const juce::String& text)
    {
        return text.replaceCharacters ("\r\n\t", "   ");
    }

    juce::String glyph (const ScoreNote& note, Density density)
    {
        juce::String text (note.fret);
        if (density == Density::notesOnly)
            return text;

        // Wrappers precede suffixes, independent of the order capture attached
        // the techniques. A harmonic must not erase an earlier bend/vibrato.
        if (note.hasTechnique (Type::deadNote))
            text = "x";
        else if (note.hasTechnique (Type::naturalHarmonic))
            text = "<" + text + ">";
        else if (note.hasTechnique (Type::artificialHarmonic)
                  || note.hasTechnique (Type::pinchHarmonic)
                  || note.hasTechnique (Type::tapHarmonic))
            text = "[" + text + "]";

        if (note.hasTechnique (Type::ghostNote)) text = "(" + text + ")";
        if (note.tiedFromPrevious) text = "=" + text;
        if (note.hasTechnique (Type::slideIn)) text = "/" + text;
        if (note.hasTechnique (Type::preBend)) text += "pb";
        if (note.hasTechnique (Type::bend)) text += "b";
        if (note.hasTechnique (Type::bendRelease)) text += "r";
        if (note.hasTechnique (Type::hammerOn)) text += "h";
        if (note.hasTechnique (Type::pullOff)) text += "p";
        if (note.hasTechnique (Type::slideUp)) text += "/";
        if (note.hasTechnique (Type::slideDown)) text += "\\";
        for (auto type : {Type::slideLegato, Type::slideShift})
            if (const auto* slide = note.findTechnique (type))
                text += slide->value < note.fret ? "\\" : "/";
        if (note.hasTechnique (Type::slideOut)) text += "/";

        if (density == Density::full)
        {
            if (note.hasTechnique (Type::tap) || note.hasTechnique (Type::tapHarmonic)) text += "t";
            if (note.hasTechnique (Type::vibrato)) text += "~";
            if (note.hasTechnique (Type::trill)) text += "tr";
            if (note.hasTechnique (Type::whammy)) text += "w";
            if (note.hasTechnique (Type::palmMute)) text += "PM";
            if (note.hasTechnique (Type::accent)) text += ">";
            if (note.hasTechnique (Type::staccato)) text += ".";
            if (note.hasTechnique (Type::letRing)) text += "LR";
        }
        return text;
    }

    struct Column
    {
        double beat = 0.0;
        juce::String ruler;
        juce::String chords;
        std::vector<juce::String> strings;
        int width = 1;
        bool muted = false;
    };

    struct Bar
    {
        std::vector<Column> columns;
    };

    // Absolute beat ranges keep a sustained mute visible across bar lines.
    std::vector<juce::Range<double>> muteSpans (const ScoreTrack& track)
    {
        std::vector<juce::Range<double>> spans;
        double start = 0.0;
        for (const auto& measure : track.measures)
        {
            for (const auto* note : measure.collectNotes())
                if (note->hasTechnique (Type::palmMute)
                     && juce::isPositiveAndBelow (note->stringIndex, juce::jlimit (1, kMaxStrings, track.numStrings))
                     && std::isfinite (note->startBeat) && std::isfinite (note->durationBeats)
                     && note->startBeat >= 0.0 && note->durationBeats > 0.0)
                    spans.emplace_back (start + note->startBeat, start + note->startBeat + note->durationBeats);
            start += barBeats (measure);
        }
        return spans;
    }

    Bar layout (const ScoreMeasure& measure, int numStrings, double start,
                const std::vector<juce::Range<double>>& muted, const NotationExportOptions& options)
    {
        Bar bar;
        const auto duration = barBeats (measure);
        const auto notes = measure.collectNotes();
        std::vector<double> positions;
        // Keep the familiar sixteenth grid, adding actual onsets between its
        // ticks instead of rounding two notes onto one cell and overwriting one.
        for (double beat = 0.0; beat < duration - epsilon; beat += 0.25)
            positions.push_back (beat);
        const double pulse = 4.0 / juce::jlimit (1, 32, measure.timeSignatureDenominator);
        for (int beat = 0; beat < juce::jlimit (1, 32, measure.timeSignatureNumerator); ++beat)
            positions.push_back (beat * pulse);
        auto validBeat = [duration] (double beat) { return std::isfinite (beat) && beat >= 0.0 && beat < duration; };
        for (const auto* note : notes)
            if (validBeat (note->startBeat)) positions.push_back (note->startBeat);
        if (options.chordSymbols)
            for (const auto& chord : measure.chordSymbols)
                if (validBeat (chord.first)) positions.push_back (chord.first);
        std::sort (positions.begin(), positions.end());
        positions.erase (std::unique (positions.begin(), positions.end(),
                                     [] (double a, double b) { return std::abs (a - b) < epsilon; }), positions.end());
        for (auto beat : positions)
        {
            Column column;
            column.beat = beat;
            column.strings.resize ((size_t) numStrings);
            const int pulseIndex = (int) std::round (beat / pulse);
            column.ruler = std::abs (beat - pulseIndex * pulse) < epsilon ? juce::String (pulseIndex + 1) : "-";
            if (options.density == Density::full)
                for (const auto& span : muted)
                    if (span.contains (start + beat)) { column.muted = true; break; }
            bar.columns.push_back (std::move (column));
        }
        auto columnFor = [&bar] (double beat) -> Column&
        {
            auto it = std::lower_bound (bar.columns.begin(), bar.columns.end(), beat - epsilon,
                                       [] (const Column& column, double b) { return column.beat < b; });
            return *it;
        };
        for (const auto* note : notes)
        {
            if (! validBeat (note->startBeat) || note->fret < 0
                 || ! juce::isPositiveAndBelow (note->stringIndex, numStrings)) continue;
            auto& cell = columnFor (note->startBeat).strings[(size_t) note->stringIndex];
            if (cell.isNotEmpty()) cell += ";"; // Preserve unusual simultaneous events too.
            cell += glyph (*note, options.density);
        }
        if (options.chordSymbols)
            for (const auto& chord : measure.chordSymbols)
                if (validBeat (chord.first))
                {
                    auto& cell = columnFor (chord.first).chords;
                    if (cell.isNotEmpty()) cell += "/";
                    cell += singleLine (chord.second);
                }
        for (size_t c = 0; c < bar.columns.size(); ++c)
        {
            auto& column = bar.columns[c];
            column.width = juce::jmax (1, column.ruler.length());
            column.width = juce::jmax (column.width, column.chords.length());
            if (column.muted && (c == 0 || ! bar.columns[c - 1].muted))
                column.width = juce::jmax (column.width, 2); // PM
            for (size_t s = 0; s < column.strings.size(); ++s)
            {
                // Adjacent fret tokens need a delimiter (5 then 7 is not 57).
                const bool adjacent = c + 1 < bar.columns.size()
                    && column.strings[s].isNotEmpty() && bar.columns[c + 1].strings[s].isNotEmpty();
                column.width = juce::jmax (column.width, column.strings[s].length() + (adjacent ? 1 : 0));
            }
        }
        return bar;
    }

    juce::String stringName (const ScoreTrack& track, int string)
    {
        juce::String step;
        int alter = 0, octave = 0;
        PerformanceScore::getMusicXmlPitch (track.tuning[(size_t) string], step, alter, octave);
        return (step + (alter == 0 ? "" : "#")).paddedRight (' ', 2);
    }

    // Emit complete cells only. ':' means the same bar continues on the next
    // system; only real measure boundaries use '|'. No fret/glyph is split.
    juce::String system (const ScoreTrack& track, const Bar& bar, size_t first, size_t end,
                         bool includeChords)
    {
        const int strings = juce::jlimit (1, kMaxStrings, track.numStrings);
        juce::String ruler ("   "), mute ("   "), chords ("   ");
        juce::StringArray staff;
        for (int s = 0; s < strings; ++s) staff.add (stringName (track, s) + (first == 0 ? "|" : ":"));
        bool hasMute = false, hasChords = false;
        for (size_t c = first; c < end; ++c)
        {
            const auto& column = bar.columns[c];
            ruler += column.ruler.paddedRight (' ', column.width);
            const bool muteStart = column.muted && (c == first || ! bar.columns[c - 1].muted);
            mute += column.muted ? juce::String (muteStart && column.width >= 2 ? "PM" : "").paddedRight ('-', column.width)
                                 : juce::String::repeatedString (" ", column.width);
            hasMute = hasMute || column.muted;
            chords += column.chords.paddedRight (' ', column.width);
            hasChords = hasChords || column.chords.isNotEmpty();
            for (int s = 0; s < strings; ++s)
                staff.set (s, staff[s] + column.strings[(size_t) s].paddedRight ('-', column.width));
        }
        juce::String out;
        if (hasChords && includeChords) out << chords.trimEnd() << "\n";
        if (hasMute) out << mute.trimEnd() << "\n";
        out << ruler << " \n";
        for (const auto& line : staff) out << line << (end == bar.columns.size() ? "|" : ":") << "\n";
        return out;
    }

    void appendText (juce::String& out, const juce::String& text, int width)
    {
        auto remaining = singleLine (text);
        while (remaining.length() > width)
        {
            out << remaining.substring (0, width) << "\n";
            remaining = remaining.substring (width);
        }
        out << remaining << "\n";
    }
}

juce::String AsciiTabWriter::renderWindow (const ScoreTrack& track, int first, int count,
                                          const NotationExportOptions& options, std::vector<TabColumnMark>* marks)
{
    if (marks != nullptr) marks->clear();
    if (count <= 0 || track.measures.empty()) return {};
    // Preserve the live preview's clamped window contract at either end.
    first = juce::jlimit (0, (int) track.measures.size() - 1, first);
    const int end = first + juce::jmin (count, (int) track.measures.size() - first);
    const int strings = juce::jlimit (1, kMaxStrings, track.numStrings);
    const auto muted = muteSpans (track);
    double start = 0.0;
    for (int m = 0; m < first; ++m) start += barBeats (track.measures[(size_t) m]);

    // A window is one unwrapped staff (the existing preview/live-view contract).
    // Join the per-bar rows, with optional chord names and PM spans above the beat ruler.
    juce::String ruler ("   "), mute ("   "), chords ("   ");
    juce::StringArray staff;
    for (int s = 0; s < strings; ++s) staff.add (stringName (track, s) + "|");
    bool hasMute = false, hasChords = false;
    for (int m = first; m < end; ++m)
    {
        const auto& measure = track.measures[(size_t) m];
        const auto bar = layout (measure, strings, start, muted, options);
        auto rows = juce::StringArray::fromLines (system (track, bar, 0, bar.columns.size(), options.windowChordRow));
        const bool barMuted = std::any_of (bar.columns.begin(), bar.columns.end(), [] (const Column& c) { return c.muted; });
        const bool barChords = options.windowChordRow
                            && std::any_of (bar.columns.begin(), bar.columns.end(), [] (const Column& c) { return c.chords.isNotEmpty(); });
        const int chordRow = barChords ? 0 : -1;
        const int rulerRow = (barChords ? 1 : 0) + (barMuted ? 1 : 0);
        const int muteRow = barMuted ? rulerRow - 1 : -1;
        const int base = ruler.length();
        ruler += rows[rulerRow].substring (3);
        const int barWidth = rows[rulerRow].length() - 3;
        mute += barMuted ? rows[muteRow].substring (3).paddedRight (' ', barWidth)
                         : juce::String::repeatedString (" ", barWidth);
        chords += barChords ? rows[chordRow].substring (3).paddedRight (' ', barWidth)
                            : juce::String::repeatedString (" ", barWidth);
        hasMute = hasMute || barMuted;
        hasChords = hasChords || barChords;
        for (int s = 0; s < strings; ++s) staff.set (s, staff[s] + rows[rulerRow + 1 + s].substring (3));
        if (marks != nullptr)
        {
            int offset = 0;
            for (const auto& column : bar.columns)
            {
                marks->push_back ({ start + column.beat, base + offset, column.width });
                offset += column.width;
            }
        }
        start += barBeats (measure);
    }
    juce::String out;
    if (hasChords) out << chords.trimEnd() << "\n";
    if (hasMute) out << mute.trimEnd() << "\n";
    out << ruler << "\n";
    for (const auto& line : staff) out << line << "\n";
    return out;
}

juce::String AsciiTabWriter::render (const PerformanceScore& score, const NotationExportOptions& options)
{
    const int width = juce::jlimit (40, 400, options.lineWidth);
    const bool ranged = std::isfinite (options.lengthBeats) && options.lengthBeats > 0.0;
    const double from = std::isfinite (options.fromBeat) ? juce::jmax (0.0, options.fromBeat) : 0.0;

    juce::String out;

    // A ranged request is an excerpt (like renderAsciiTabWindow), so it carries
    // only the tab itself, not the whole-piece header. Emitting the header here
    // also folded piece metadata - the tempo among it - into an excerpt that
    // asked for a single span of bars.
    if (! ranged)
    {
        appendText (out, score.getMeta().title, width);
        if (score.getMeta().artist.isNotEmpty()) appendText (out, score.getMeta().artist, width);
        appendText (out, "Tuning: " + score.getMeta().tuningName, width);
        appendText (out, "Tempo: " + juce::String (score.getMeta().tempoBpm, 0) + " bpm    "
                         + juce::String (score.getMeta().timeSignatureNumerator) + "/"
                         + juce::String (score.getMeta().timeSignatureDenominator), width);
        out << "\n";
    }
    for (int t = 0; t < score.getNumTracks(); ++t)
    {
        const auto& track = score.getTrack (t);
        const int strings = juce::jlimit (1, kMaxStrings, track.numStrings);
        const auto muted = muteSpans (track);
        if (score.getNumTracks() > 1) appendText (out, track.name, width);
        if (track.capoFret > 0) appendText (out, "Capo: " + juce::String (track.capoFret), width);
        double start = 0.0;
        for (const auto& measure : track.measures)
        {
            const double endBeat = start + barBeats (measure);
            if (! ranged || (endBeat > from && start < from + options.lengthBeats))
            {
                if (measure.sectionName.isNotEmpty()) appendText (out, "[" + singleLine (measure.sectionName) + "]", width);
                const auto bar = layout (measure, strings, start, muted, options);
                for (size_t first = 0; first < bar.columns.size();)
                {
                    size_t end = first;
                    int used = 4; // two-character string label plus start/end boundary
                    while (end < bar.columns.size() && used + bar.columns[end].width <= width)
                        used += bar.columns[end++].width;
                    // Never lose a pathological over-wide token; document the
                    // exceptional width rather than silently truncating music.
                    if (end == first) ++end;
                    out << system (track, bar, first, end, options.chordSymbols) << "\n";
                    first = end;
                }
            }
            start = endBeat;
        }
    }
    return out;
}
} // namespace luthier
