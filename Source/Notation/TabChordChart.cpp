#include "TabChordChart.h"
#include "AsciiTabReader.h"

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
    return extractChords (text, -1);
}

namespace
{
    /** "1 4 5 1", "6m 4 1 5", "2- 5 1": Nashville numbers; a chord per token when a key is known. */
    bool nashvilleLine (const juce::StringArray& toks, int keyRoot, std::vector<TabChordChart::Chord>& out)
    {
        if (keyRoot < 0 || toks.size() < 2)
            return false;
        static const int degreeOffsets[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static const char* const names[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        std::vector<TabChordChart::Chord> found;
        for (const auto& t : toks)
        {
            if (t.isEmpty() || t.length() > 6 || t[0] < '1' || t[0] > '7')
                return false;
            auto rest = t.substring (1);
            int offset = 0;
            if (rest.startsWithChar ('#')) { offset = 1; rest = rest.substring (1); }
            else if (rest.startsWithChar ('b')) { offset = -1; rest = rest.substring (1); }
            const int pc = ((keyRoot + degreeOffsets[t[0] - '1'] + offset) % 12 + 12) % 12;
            juce::String quality = rest == "-" ? juce::String ("m") : rest;
            TabChordChart::Chord chord;
            if (! TabChordChart::parseChord (juce::String (names[pc]) + quality, chord))
                return false;
            found.push_back (chord);
        }
        out = found;
        return true;
    }
}

std::vector<TabChordChart::Chord> TabChordChart::extractChords (const juce::String& text, int keyRootPitchClass)
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

            // A chord legend line ("Am x02210  G 320003") defines shapes; it is not the progression.
            if (! extractDiagrams (raw).empty())
                continue;

            if (nashvilleLine (toks, keyRootPitchClass, lineChords))
            {
                for (int r = 0; r < repeats && (int) result.size() < kMaxChords; ++r)
                    for (const auto& c : lineChords)
                        if ((int) result.size() < kMaxChords)
                            result.push_back (c);
                continue;
            }

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
std::vector<int> TabChordChart::voicingFor (const Chord& chord, const std::vector<int>& tuningHighFirst, int capo)
{
    const int n = juce::jlimit (1, 12, (int) tuningHighFirst.size());
    std::vector<int> out ((size_t) n, -1);

    // Chord tones, by quality.
    const int root = chord.rootPitchClass;
    const auto q = chord.quality;
    std::vector<int> tones { root };
    const bool minor = q == "m" || q == "m7";
    const bool dim = q == "dim";
    if (q == "sus2") tones.push_back ((root + 2) % 12);
    else if (q == "sus4") tones.push_back ((root + 5) % 12);
    else if (q != "5") tones.push_back ((root + (minor || dim ? 3 : 4)) % 12);
    tones.push_back ((root + (dim ? 6 : q == "aug" ? 8 : 7)) % 12);
    if (q == "7" || q == "m7" || q == "9") tones.push_back ((root + 10) % 12);
    if (q == "maj7") tones.push_back ((root + 11) % 12);
    if (q == "6") tones.push_back ((root + 9) % 12);
    if (q == "9" || q == "add9") tones.push_back ((root + 2) % 12);
    const int bass = chord.bassPitchClass >= 0 ? chord.bassPitchClass : root;

    const auto isTone = [&] (int pc) { return std::find (tones.begin(), tones.end(), ((pc % 12) + 12) % 12) != tones.end(); };

    double bestCost = 1.0e9;
    for (int base = 0; base <= 9; ++base)
    {
        std::vector<int> frets ((size_t) n, -1);
        double cost = 0.0;
        int sounding = 0;
        bool hasRoot = false, hasThird = tones.size() < 2;
        for (int s = 0; s < n; ++s)
        {
            const int open = tuningHighFirst[(size_t) s] + capo;
            int chosen = -1;
            if (isTone (open)) chosen = 0;
            for (int f = base; f <= base + 3 && chosen < 0; ++f)
                if (f > 0 && isTone (open + f)) chosen = f;
            frets[(size_t) s] = chosen;
            if (chosen >= 0)
            {
                ++sounding;
                cost += chosen * 0.3;
                if (((open + chosen) % 12) == root) hasRoot = true;
                if (tones.size() >= 2 && ((open + chosen) % 12) == tones[1]) hasThird = true;
            }
            else
                cost += 1.5;
        }
        // The lowest sounding string should carry the bass.
        for (int s = n - 1; s >= 0; --s)
            if (frets[(size_t) s] >= 0)
            {
                if (((tuningHighFirst[(size_t) s] + capo + frets[(size_t) s]) % 12) != bass) cost += 2.0;
                break;
            }
        if (! hasRoot) cost += 6.0;
        if (! hasThird) cost += 3.0;
        if (sounding < juce::jmin (3, n)) cost += 10.0;
        cost += base * 0.2;
        if (cost < bestCost) { bestCost = cost; out = frets; }
    }
    return out;
}

std::vector<std::pair<juce::String, std::vector<int>>> TabChordChart::extractDiagrams (const juce::String& text)
{
    std::vector<std::pair<juce::String, std::vector<int>>> out;
    const auto lines = juce::StringArray::fromLines (text);

    // One diagram token: "x02210", "x-0-2-2-1-0", "x.0.2.2.1.0", "(x32010)", "X02210".
    const auto diagramOf = [] (juce::String t, std::vector<int>& frets) -> bool
    {
        t = t.removeCharacters ("()[]-.,_");
        if (t.length() < 4 || t.length() > 8) return false;
        frets.clear();
        for (int i = 0; i < t.length(); ++i)
        {
            const auto c = t[i];
            if (c == 'x' || c == 'X') frets.push_back (-1);
            else if (c >= '0' && c <= '9') frets.push_back ((int) (c - '0'));
            else return false;
        }
        return true;
    };

    for (const auto& raw : lines)
    {
        if (raw.length() > 400 || out.size() >= 256) continue;
        auto line = raw.trim();
        const auto lower = line.toLowerCase();

        // ChordPro: {define: Am base-fret 1 frets x 0 2 2 1 0}
        if (lower.startsWith ("{define") || lower.startsWith ("{chord"))
        {
            const int colon = line.indexOfChar (':');
            const int fretsAt = lower.indexOf ("frets");
            if (colon < 0 || fretsAt < 0) continue;
            const auto name = line.substring (colon + 1, fretsAt).upToFirstOccurrenceOf ("base", false, true).trim();
            int baseFret = 1;
            const int baseAt = lower.indexOf ("base-fret");
            if (baseAt >= 0) baseFret = juce::jlimit (1, 24, line.substring (baseAt + 9).trim().getIntValue());
            auto rest = line.substring (fretsAt + 5).upToFirstOccurrenceOf ("}", false, false).upToFirstOccurrenceOf ("fingers", false, true);
            std::vector<int> frets;
            for (const auto& t : tokens (rest))
            {
                if (t == "x" || t == "X" || t == "-") frets.push_back (-1);
                else if (t.containsOnly ("0123456789")) frets.push_back (t.getIntValue() == 0 ? 0 : t.getIntValue() + baseFret - 1);
            }
            Chord chord;
            if (frets.size() >= 4 && frets.size() <= 8 && parseChord (name, chord))
                out.push_back ({ chord.name, frets });
            continue;
        }

        // "Am: x02210", "Am = x 0 2 2 1 0", "Am (x02210)", "Am x02210  G 320003".
        auto toks = tokens (line.replace (":", " ").replace ("=", " ").replace (" - ", " "));
        for (int i = 0; i + 1 < toks.size(); ++i)
        {
            Chord chord;
            if (! parseChord (toks[i], chord)) continue;
            std::vector<int> frets;
            if (diagramOf (toks[i + 1], frets))
            {
                out.push_back ({ chord.name, frets });
                ++i;
                continue;
            }
            // Six separate cells: x 0 2 2 1 0
            frets.clear();
            int j = i + 1;
            while (j < toks.size() && frets.size() < 8
                   && (toks[j] == "x" || toks[j] == "X" || (toks[j].length() <= 2 && toks[j].containsOnly ("0123456789"))))
            {
                frets.push_back (toks[j] == "x" || toks[j] == "X" ? -1 : toks[j].getIntValue());
                ++j;
            }
            if (frets.size() >= 4)
            {
                out.push_back ({ chord.name, frets });
                i = j - 1;
            }
        }
    }
    return out;
}

bool TabChordChart::extractStrumPattern (const juce::String& text, std::vector<int>& pattern)
{
    pattern.clear();
    for (const auto& raw : juce::StringArray::fromLines (text))
    {
        const auto lower = raw.trim().toLowerCase();
        if (raw.length() > 200 || ! (lower.startsWith ("strum") || lower.startsWith ("pattern") || lower.startsWith ("rhythm")))
            continue;
        const int colon = raw.indexOfChar (':');
        if (colon < 0) continue;
        const auto body = raw.substring (colon + 1).trim();
        // "D DU UDU": eight character cells, a space is a rest (the UG convention);
        // anything else is one slot per glyph.
        const bool cells = body.length() == 8 || body.length() == 16;
        std::vector<int> found;
        for (auto p = body.getCharPointer(); ! p.isEmpty();)
        {
            const auto c = p.getAndAdvance();
            if (c == 'D' || c == 'd' || c == 'v' || c == 'V' || c == 0x2193) found.push_back (1);
            else if (c == 'U' || c == 'u' || c == '^' || c == 0x2191)        found.push_back (-1);
            else if (c == 'x' || c == 'X')                                    found.push_back (2);
            else if (c == '-' || c == '.' || c == '_')                        found.push_back (0);
            else if (c == ' ' || c == '\t')                                   { if (cells) found.push_back (0); continue; }
            else if (c == ',' || c == '|')                                    continue;
            else { found.clear(); break; }
            if (found.size() > 64) { found.clear(); break; }
        }
        if (found.size() >= 2)
        {
            pattern = found;
            return true;
        }
    }
    return false;
}

bool TabChordChart::extractPickingPattern (const juce::String& text, std::vector<int>& order)
{
    order.clear();
    for (const auto& raw : juce::StringArray::fromLines (text))
    {
        const auto lower = raw.trim().toLowerCase();
        if (raw.length() > 200 || ! (lower.startsWith ("pick") || lower.startsWith ("finger") || lower.startsWith ("arpeggio")))
            continue;
        const int colon = raw.indexOfChar (':');
        if (colon < 0) continue;
        std::vector<int> found;
        for (const auto& t : tokens (raw.substring (colon + 1)))
        {
            const auto l = t.toLowerCase();
            if (l == "p" || l == "t" || l == "thumb") found.push_back (0);     // bass string of the shape
            else if (l == "i" || l == "1")              found.push_back (1);   // the third string (G)
            else if (l == "m" || l == "2")              found.push_back (2);   // B
            else if (l == "a" || l == "3")              found.push_back (3);   // e
            else if (l == "-" || l == ".")              found.push_back (-1);
            else { found.clear(); break; }
            if (found.size() > 32) { found.clear(); break; }
        }
        if (found.size() >= 2)
        {
            order = found;
            return true;
        }
    }
    return false;
}

//==============================================================================
bool TabChordChart::read (const juce::String& text, PerformanceScore& destination,
                          TabImportDiagnostics* diagnostics,
                          const std::vector<int>& tuningHighFirst, int capo, double tempoBpm)
{
    // A "Key:" header makes Nashville numbers readable and names the key.
    int keyRoot = -1; bool keyMinor = false;
    juce::String keyName;
    for (const auto& raw : juce::StringArray::fromLines (text))
        if (raw.length() <= 120 && AsciiTabReader::parseKeyStatement (raw, keyRoot, keyMinor))
        {
            keyName = AsciiTabReader::keyName (keyRoot, keyMinor);
            break;
        }

    const auto chords = extractChords (text, keyRoot);

    if (chords.empty())
        return false;

    destination.clear();
    destination.beginCapture (tempoBpm >= 20.0 && tempoBpm <= 300.0 ? tempoBpm : 90.0, 4, 4);
    destination.getMeta().key = keyName;
    auto& track = destination.getTrack (0);

    track.numStrings = 6;
    track.tuning = { { 64, 59, 55, 50, 45, 40, 0, 0, 0, 0, 0, 0 } };

    std::vector<int> tuning { 64, 59, 55, 50, 45, 40 };
    if (tuningHighFirst.size() >= 4 && tuningHighFirst.size() <= 8)
    {
        tuning = tuningHighFirst;
        track.numStrings = (int) tuning.size();
        for (size_t i = 0; i < tuning.size(); ++i)
            track.tuning[i] = juce::jlimit (0, 127, tuning[i]);
    }
    const bool standardSix = tuning == std::vector<int> { 64, 59, 55, 50, 45, 40 };

    track.capoFret = juce::jlimit (0, 12, capo);

    // Inline diagrams win over names; otherwise the shape is looked up (standard)
    // or searched on the actual tuning.
    const auto diagrams = extractDiagrams (text);
    const auto shapeOf = [&] (const Chord& chord) -> std::vector<int>
    {
        for (const auto& d : diagrams)
            if (d.first == chord.name && (int) d.second.size() == track.numStrings)
            {
                std::vector<int> highFirst (d.second.rbegin(), d.second.rend());   // diagrams are written low to high
                return highFirst;
            }
        return standardSix ? shapeFor (chord, track.numStrings) : voicingFor (chord, tuning, track.capoFret);
    };

    std::vector<int> strumPattern, picking;
    const bool hasStrum = extractStrumPattern (text, strumPattern);
    const bool hasPicking = ! hasStrum && extractPickingPattern (text, picking);

    int notes = 0;
    double beat = 0.0;

    const auto strike = [&] (const std::vector<int>& shape, double at, double length, double velocity,
                             ScoreTechnique::Type stroke, bool muted, int onlyString)
    {
        for (int s = 0; s < track.numStrings; ++s)
        {
            const int fret = shape[(size_t) s];
            if (fret < 0 || (onlyString >= 0 && s != onlyString))
                continue;

            const int midi = juce::jlimit (0, 127, track.tuning[(size_t) s] + track.capoFret + fret);
            destination.noteStarted (s, fret, midi, 440.0 * std::pow (2.0, (midi - 69) / 12.0), velocity, at);
            if (stroke == ScoreTechnique::Type::pickStrokeUp || stroke == ScoreTechnique::Type::pickStrokeDown)
            {
                ScoreTechnique t; t.type = stroke;
                destination.addTechnique (s, t);
            }
            if (muted)
            {
                ScoreTechnique t; t.type = ScoreTechnique::Type::deadNote;
                destination.addTechnique (s, t);
            }
            destination.noteEnded (s, at + length);
            ++notes;
        }
    };

    for (const auto& chord : chords)
    {
        const auto shape = shapeOf (chord);
        destination.addChordSymbol (beat, chord.name);

        if (hasStrum)
        {
            const double slot = 4.0 / (double) strumPattern.size();
            for (size_t i = 0; i < strumPattern.size(); ++i)
            {
                const int p = strumPattern[i];
                if (p == 0) continue;
                const bool up = p < 0, mute = p == 2;
                const double length = mute ? juce::jmin (slot, 0.25) : slot;
                strike (shape, beat + slot * (double) i, length, up ? 0.6 : (i == 0 ? 0.85 : 0.7),
                        up ? ScoreTechnique::Type::pickStrokeUp : ScoreTechnique::Type::pickStrokeDown, mute, -1);
            }
        }
        else if (hasPicking)
        {
            int bassString = -1;
            for (int s = track.numStrings - 1; s >= 0; --s)
                if (shape[(size_t) s] >= 0) { bassString = s; break; }
            const double slot = 4.0 / (double) std::max<size_t> (4, picking.size());
            for (size_t i = 0; i < picking.size() && slot * (double) i < 4.0; ++i)
            {
                const int finger = picking[i];
                if (finger < 0) continue;
                int str = finger == 0 ? bassString : juce::jlimit (0, track.numStrings - 1, 3 - finger);
                if (str < 0 || shape[(size_t) str] < 0)
                    for (int s = 0; s < track.numStrings; ++s) if (shape[(size_t) s] >= 0) { str = s; break; }
                if (str >= 0 && shape[(size_t) str] >= 0)
                    strike (shape, beat + slot * (double) i, 4.0 - slot * (double) i, finger == 0 ? 0.8 : 0.65,
                            ScoreTechnique::Type::numTypes, false, str);
            }
        }
        else
        {
            strike (shape, beat, 2.0, 0.8, ScoreTechnique::Type::pickStrokeDown, false, -1);
            strike (shape, beat + 2.0, 2.0, 0.65, ScoreTechnique::Type::pickStrokeDown, false, -1);
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
                                   + " chord(s) were turned into one strummed bar each"
                                   + (hasStrum ? " following the strumming pattern." : hasPicking ? " following the picking pattern." : ".")
                                   + (diagrams.empty() ? juce::String() : " " + juce::String ((int) diagrams.size()) + " chord diagram(s) used."));
    }

    return true;
}

} // namespace luthier
