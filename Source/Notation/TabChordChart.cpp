#include "TabChordChart.h"

#include <algorithm>
#include <cmath>

namespace luthier
{
namespace
{
    int letterPc (juce::juce_wchar c) noexcept
    {
        switch (c)
        {
            case 'C': return 0; case 'D': return 2; case 'E': return 4; case 'F': return 5;
            case 'G': return 7; case 'A': return 9; case 'B': return 11; default: return -1;
        }
    }

    /** Root letter and accidental at the start of `s`; returns characters used, or 0. */
    int parseRoot (const juce::String& s, int start, int& pc)
    {
        if (start >= s.length())
            return 0;

        pc = letterPc (s[start]);
        if (pc < 0)
            return 0;

        int used = 1;
        if (start + 1 < s.length())
        {
            const auto a = s[start + 1];
            if (a == '#' || a == 0x266F)      { pc = (pc + 1) % 12;  used = 2; }
            else if (a == 'b' || a == 0x266D) { pc = (pc + 11) % 12; used = 2; }
        }
        return used;
    }

    /** Reduces the text after the root to one of the shapes this knows. Empty on
        failure is "" (major), so success is signalled through `ok`. */
    juce::String normaliseQuality (juce::String q, bool& ok)
    {
        ok = true;

        // Parenthesised extensions: C(add9), G7(b9)
        q = q.removeCharacters ("()");
        const auto original = q;

        if (q.isEmpty() || q == "M" || q == "maj" || q == "major")
            return {};

        // Words guitarists write.
        q = q.replace ("major", "maj").replace ("minor", "m").replace ("min", "m")
             .replace ("Maj", "maj").replace ("MAJ", "maj").replace ("\xC2\xB0", "dim")
             .replace ("\xCE\x94", "maj7").replace ("+", "aug").replace ("\xC3\xB8", "m7");
        q = q.replace ("-", "m");

        if (q == "5")                          return "5";
        if (q == "m" )                         return "m";
        if (q.startsWith ("maj"))
            return (q == "maj" || q == "maj5") ? juce::String() : juce::String ("maj7");
        if (q == "M7" || q == "M9" || q == "M11" || q == "M13") return "maj7";
        if (q.startsWith ("dim"))              return "dim";
        if (q.startsWith ("aug"))              return "aug";
        if (q.startsWith ("sus2"))             return "sus2";
        if (q.startsWith ("sus"))              return "sus4";
        if (q.startsWith ("7sus"))             return "sus4";
        if (q == "6" || q == "69" || q == "6/9") return "6";
        if (q == "m6")                         return "m";
        if (q.startsWith ("add"))              return "add9";
        if (q == "9" || q == "11" || q == "13") return "9";
        if (q.startsWith ("m7") || q.startsWith ("m9") || q.startsWith ("m11") || q.startsWith ("m13")) return "m7";
        if (q.startsWith ("m") && q.length() > 1 && juce::CharacterFunctions::isDigit (q[1])) return "m7";
        if (q.startsWith ("m") && (q.contains ("add") || q.contains ("b5"))) return "m";
        if (q.startsWith ("7") || q.startsWith ("9") || q.startsWith ("11") || q.startsWith ("13")) return "7";
        if (q.startsWith ("b5") || q.startsWith ("#5") || q.startsWith ("b9") || q.startsWith ("#9")) return "7";

        ok = false;
        juce::ignoreUnused (original);
        return {};
    }

    bool isFiller (const juce::String& t)
    {
        if (t.isEmpty()) return true;
        const auto lower = t.toLowerCase();
        if (lower == "n.c." || lower == "nc" || lower == "n.c" || lower == "%" || lower == "/" || lower == "-"
            || lower == "|" || lower == "||" || lower == ":" || lower == "|:" || lower == ":|" || lower == "." || lower == "*")
            return true;
        if (lower.containsOnly ("|:/-.*"))
            return true;
        return false;
    }

    /** "x2", "(x2)", "2x", "x 3" repeat markers. Returns the count or 0. */
    int repeatCount (const juce::String& token)
    {
        auto t = token.toLowerCase().removeCharacters ("()[]");
        if (t.startsWith ("x") && t.length() > 1 && t.substring (1).containsOnly ("0123456789"))
            return juce::jlimit (1, 16, t.substring (1).getIntValue());
        if (t.endsWithChar ('x') && t.length() > 1 && t.dropLastCharacters (1).containsOnly ("0123456789"))
            return juce::jlimit (1, 16, t.dropLastCharacters (1).getIntValue());
        return 0;
    }

    juce::StringArray tokens (const juce::String& line)
    {
        juce::StringArray out;
        juce::String current;
        int steps = 0;
        for (auto p = line.getCharPointer(); ! p.isEmpty() && ++steps < 20000;)
        {
            const auto c = p.getAndAdvance();
            if (juce::CharacterFunctions::isWhitespace (c) || c == ',' || c == ';')
            {
                if (current.isNotEmpty()) out.add (current);
                current = {};
            }
            else
                current += juce::String::charToString (c);
        }
        if (current.isNotEmpty()) out.add (current);
        return out;
    }

    /** Removes UG/ChordPro markup, returning chords found inline (in order). */
    juce::String stripMarkup (juce::String line, std::vector<juce::String>& inlineChords)
    {
        line = line.replace ("[ch]", " ").replace ("[/ch]", " ").replace ("[CH]", " ").replace ("[/CH]", " ")
                   .replace ("[tab]", " ").replace ("[/tab]", " ");

        // ChordPro [Am] and any remaining [..] groups.
        juce::String out;
        int i = 0;
        const int n = juce::jmin (line.length(), 4000);
        while (i < n)
        {
            if (line[i] == '[')
            {
                const int close = line.indexOfChar (i, ']');
                if (close > i && close - i <= 12)
                {
                    const auto inner = line.substring (i + 1, close);
                    TabChordChart::Chord c;
                    if (TabChordChart::parseChord (inner, c))
                    {
                        inlineChords.push_back (inner);
                        i = close + 1;
                        continue;
                    }
                    i = close + 1;   // section tag such as [Verse]: dropped
                    continue;
                }
            }
            out += juce::String::charToString (line[i]);
            ++i;
        }
        return out;
    }
}

//==============================================================================
bool TabChordChart::parseChord (const juce::String& tokenIn, Chord& out)
{
    auto token = tokenIn.trim();
    if (token.isEmpty() || token.length() > 14)
        return false;

    token = token.removeCharacters ("()");

    int rootPc = 0;
    const int rootLen = parseRoot (token, 0, rootPc);
    if (rootLen == 0)
        return false;

    auto rest = token.substring (rootLen);
    int bass = -1;

    const int slash = rest.lastIndexOfChar ('/');
    if (slash >= 0)
    {
        int b = 0;
        const auto bassText = rest.substring (slash + 1);
        const int used = parseRoot (bassText, 0, b);
        if (used > 0 && used == bassText.length())
        {
            bass = b;
            rest = rest.substring (0, slash);
        }
        else if (rest.substring (slash) != "/9")   // 6/9 is a quality, not a bass
            return false;
    }

    bool ok = true;
    const auto quality = normaliseQuality (rest, ok);
    if (! ok)
        return false;

    out.name = tokenIn.trim();
    out.rootPitchClass = rootPc;
    out.bassPitchClass = (bass == rootPc ? -1 : bass);
    out.quality = quality;
    return true;
}

std::vector<TabChordChart::Chord> TabChordChart::extractChords (const juce::String& text)
{
    std::vector<Chord> result;
    const auto lines = juce::StringArray::fromLines (text);

    for (const auto& raw : lines)
    {
        if (raw.length() > 4000 || (int) result.size() >= kMaxChords)
            continue;

        std::vector<juce::String> inlineChords;
        const auto stripped = stripMarkup (raw, inlineChords);
        const auto trimmed = stripped.trim();

        std::vector<Chord> lineChords;
        int repeats = 1;

        if (! inlineChords.empty())
        {
            // [Am]Hello [G]world: the chords are the content.
            for (const auto& c : inlineChords)
            {
                Chord chord;
                if (parseChord (c, chord))
                    lineChords.push_back (chord);
            }
        }
        else
        {
            auto toks = tokens (trimmed);
            if (toks.isEmpty())
                continue;

            // "Intro: Am F C G" - a short label ending in a colon.
            if (toks.size() > 1 && toks[0].length() <= 12 && toks[0].endsWithChar (':')
                && toks[0].dropLastCharacters (1).containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 "))
                toks.remove (0);

            int chordTokens = 0, otherTokens = 0;
            std::vector<Chord> found;

            for (const auto& t : toks)
            {
                Chord chord;
                if (parseChord (t, chord))
                {
                    ++chordTokens;
                    found.push_back (chord);
                }
                else if (const int r = repeatCount (t))
                    repeats = r;
                else if (! isFiller (t))
                    ++otherTokens;
            }

            if (chordTokens == 0 || otherTokens > 0)
                continue;

            // A lone bare letter ("A", "E") is as likely a word as a chord.
            if (chordTokens == 1 && toks.size() == 1 && found.front().name.length() == 1
                && inlineChords.empty() && ! raw.contains ("[ch]"))
                continue;

            lineChords = found;
        }

        for (int r = 0; r < repeats && (int) result.size() < kMaxChords; ++r)
            for (const auto& c : lineChords)
                if ((int) result.size() < kMaxChords)
                    result.push_back (c);
    }

    return result;
}

//==============================================================================
std::vector<int> TabChordChart::shapeFor (const Chord& chord, int numStrings)
{
    // Open / standard shapes, low E to high e, -1 = muted.
    struct Open { const char* name; int frets[6]; };
    static const Open opens[] =
    {
        { "C",     { -1, 3, 2, 0, 1, 0 } }, { "D",     { -1, -1, 0, 2, 3, 2 } }, { "E",     { 0, 2, 2, 1, 0, 0 } },
        { "G",     { 3, 2, 0, 0, 0, 3 } },  { "A",     { -1, 0, 2, 2, 2, 0 } },  { "F",     { -1, -1, 3, 2, 1, 1 } },
        { "B",     { -1, 2, 4, 4, 4, 2 } },
        { "Am",    { -1, 0, 2, 2, 1, 0 } }, { "Dm",    { -1, -1, 0, 2, 3, 1 } }, { "Em",    { 0, 2, 2, 0, 0, 0 } },
        { "Bm",    { -1, 2, 4, 4, 3, 2 } }, { "Fm",    { 1, 3, 3, 1, 1, 1 } },
        { "C7",    { -1, 3, 2, 3, 1, 0 } }, { "D7",    { -1, -1, 0, 2, 1, 2 } }, { "E7",    { 0, 2, 0, 1, 0, 0 } },
        { "G7",    { 3, 2, 0, 0, 0, 1 } },  { "A7",    { -1, 0, 2, 0, 2, 0 } },  { "B7",    { -1, 2, 1, 2, 0, 2 } },
        { "Am7",   { -1, 0, 2, 0, 1, 0 } }, { "Dm7",   { -1, -1, 0, 2, 1, 1 } }, { "Em7",   { 0, 2, 0, 0, 0, 0 } },
        { "Cmaj7", { -1, 3, 2, 0, 0, 0 } }, { "Dmaj7", { -1, -1, 0, 2, 2, 2 } }, { "Fmaj7", { -1, -1, 3, 2, 1, 0 } },
        { "Gmaj7", { 3, 2, 0, 0, 0, 2 } },  { "Amaj7", { -1, 0, 2, 1, 2, 0 } },  { "Emaj7", { 0, 2, 1, 1, 0, 0 } },
        { "Dsus4", { -1, -1, 0, 2, 3, 3 } },{ "Dsus2", { -1, -1, 0, 2, 3, 0 } }, { "Asus4", { -1, 0, 2, 2, 3, 0 } },
        { "Asus2", { -1, 0, 2, 2, 0, 0 } }, { "Esus4", { 0, 2, 2, 2, 0, 0 } },   { "Gsus4", { 3, 3, 0, 0, 1, 3 } },
        { "Cadd9", { -1, 3, 2, 0, 3, 0 } }, { "Gadd9", { 3, 0, 0, 2, 0, 3 } },   { "Dadd9", { -1, -1, 0, 2, 3, 0 } },
        { "E9",    { 0, 2, 0, 1, 0, 2 } },  { "A9",    { -1, 0, 2, 1, 0, 0 } },  { "D6",    { -1, -1, 0, 2, 0, 2 } },
        { "A6",    { -1, 0, 2, 2, 2, 2 } }, { "E6",    { 0, 2, 2, 1, 2, 0 } },   { "Edim",  { 0, 1, 2, 0, -1, -1 } },
        { "E5",    { 0, 2, 2, -1, -1, -1 } },{ "A5",   { -1, 0, 2, 2, -1, -1 } },{ "D5",    { -1, -1, 0, 2, 3, -1 } },
        { "G5",    { 3, 5, 5, -1, -1, -1 } },{ "C5",   { -1, 3, 5, 5, -1, -1 } }
    };

    static const char* const names[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    static const char* const sharpNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    const int root = chord.rootPitchClass;
    const juce::String q = chord.quality;
    const juce::String key1 = juce::String (names[root]) + q;
    const juce::String key2 = juce::String (sharpNames[root]) + q;

    std::vector<int> frets6;

    for (const auto& o : opens)
        if (key1 == o.name || key2 == o.name)
        {
            frets6.assign (o.frets, o.frets + 6);
            break;
        }

    if (frets6.empty())
    {
        // Moveable barre shapes (offset from the barre fret; -1 muted).
        struct Shape { const char* quality; int e[6]; int a[6]; };
        static const Shape shapes[] =
        {
            { "",     { 0, 2, 2, 1, 0, 0 },   { -1, 0, 2, 2, 2, 0 } },
            { "m",    { 0, 2, 2, 0, 0, 0 },   { -1, 0, 2, 2, 1, 0 } },
            { "7",    { 0, 2, 0, 1, 0, 0 },   { -1, 0, 2, 0, 2, 0 } },
            { "m7",   { 0, 2, 0, 0, 0, 0 },   { -1, 0, 2, 0, 1, 0 } },
            { "maj7", { 0, 2, 1, 1, 0, 0 },   { -1, 0, 2, 1, 2, 0 } },
            { "sus4", { 0, 2, 2, 2, 0, 0 },   { -1, 0, 2, 2, 3, 0 } },
            { "sus2", { 0, 2, 4, 4, 0, 0 },   { -1, 0, 2, 2, 0, 0 } },
            { "dim",  { 0, 1, 2, 0, -1, -1 }, { -1, 0, 1, 2, 1, -1 } },
            { "aug",  { 0, 3, 2, 1, 1, 0 },   { -1, 0, 3, 2, 2, 1 } },
            { "6",    { 0, 2, 2, 1, 2, 0 },   { -1, 0, 2, 2, 2, 2 } },
            { "9",    { 0, 2, 0, 1, 0, 2 },   { -1, 0, 2, 1, 0, 0 } },
            { "5",    { 0, 2, 2, -1, -1, -1 },{ -1, 0, 2, 2, -1, -1 } },
            { "add9", { 0, 2, 2, 1, 0, 2 },   { -1, 0, 2, 4, 2, 0 } }
        };

        const Shape* shape = &shapes[0];
        for (const auto& s : shapes)
            if (q == s.quality)
                shape = &s;

        const int eFret = (root - 4 + 12) % 12;     // root on the low E string
        const int aFret = (root - 9 + 12) % 12;     // root on the A string
        const bool useE = eFret <= aFret + 1 || aFret > 7;
        const int base = useE ? eFret : aFret;
        const int* rel = useE ? shape->e : shape->a;

        frets6.resize (6);
        for (int i = 0; i < 6; ++i)
            frets6[(size_t) i] = rel[i] < 0 ? -1 : rel[i] + base;

        // Keep it on the neck: an open shape one octave up when it would run past fret 12.
        const int highest = *std::max_element (frets6.begin(), frets6.end());
        if (highest > 14)
            for (auto& f : frets6)
                if (f >= 12) f -= 12;
    }

    // Slash chord: put the bass note on the lowest string that can reach it.
    if (chord.bassPitchClass >= 0)
    {
        const int onE = (chord.bassPitchClass - 4 + 12) % 12;
        const int onA = (chord.bassPitchClass - 9 + 12) % 12;
        if (onE <= 7)
        {
            frets6[0] = onE;
        }
        else
        {
            frets6[0] = -1;
            frets6[1] = onA;
        }
    }

    // Highest string first, as the score stores it.
    std::vector<int> out ((size_t) juce::jlimit (1, 12, numStrings), -1);
    const int n = (int) out.size();

    if (n == 6)
    {
        for (int i = 0; i < 6; ++i)
            out[(size_t) i] = frets6[(size_t) (5 - i)];
    }
    else if (n < 6)
    {
        // Bass or short-scale: the lowest n strings of the shape's lowest notes.
        for (int i = 0; i < n; ++i)
            out[(size_t) i] = frets6[(size_t) (n - 1 - i)];
    }
    else
    {
        for (int i = 0; i < 6; ++i)
            out[(size_t) (i + (n - 6))] = frets6[(size_t) (5 - i)];
    }

    return out;
}

//==============================================================================
bool TabChordChart::read (const juce::String& text, PerformanceScore& destination,
                          TabImportDiagnostics* diagnostics,
                          const std::vector<int>& tuningHighFirst, int capo, double tempoBpm)
{
    const auto chords = extractChords (text);

    if (chords.empty())
        return false;

    destination.clear();
    destination.beginCapture (tempoBpm >= 20.0 && tempoBpm <= 300.0 ? tempoBpm : 90.0, 4, 4);
    auto& track = destination.getTrack (0);

    track.numStrings = 6;
    track.tuning = { { 64, 59, 55, 50, 45, 40, 0, 0, 0, 0, 0, 0 } };

    if (tuningHighFirst.size() == 6)
        for (size_t i = 0; i < 6; ++i)
            track.tuning[i] = juce::jlimit (0, 127, tuningHighFirst[i]);

    track.capoFret = juce::jlimit (0, 12, capo);

    int notes = 0;
    double beat = 0.0;

    for (const auto& chord : chords)
    {
        const auto shape = shapeFor (chord, track.numStrings);
        destination.addChordSymbol (beat, chord.name);

        for (const double offset : { 0.0, 2.0 })
        {
            for (int s = 0; s < track.numStrings; ++s)
            {
                const int fret = shape[(size_t) s];
                if (fret < 0)
                    continue;

                const int midi = juce::jlimit (0, 127, track.tuning[(size_t) s] + track.capoFret + fret);
                destination.noteStarted (s, fret, midi, 440.0 * std::pow (2.0, (midi - 69) / 12.0),
                                         offset == 0.0 ? 0.8 : 0.65, beat + offset);
            }

            for (int s = 0; s < track.numStrings; ++s)
                destination.noteEnded (s, beat + offset + 2.0);

            for (int s = 0; s < track.numStrings; ++s)
                notes += shape[(size_t) s] >= 0 ? 1 : 0;
        }

        beat += 4.0;
    }

    destination.endCapture (beat);

    if (diagnostics != nullptr)
    {
        diagnostics->chordChartBars = (int) chords.size();
        diagnostics->measures = (int) chords.size();
        diagnostics->notes = notes;
        diagnostics->numStrings = track.numStrings;
        diagnostics->warnings.add ("No tab found: " + juce::String ((int) chords.size())
                                   + " chord(s) were turned into one strummed bar each.");
    }

    return true;
}

} // namespace luthier
