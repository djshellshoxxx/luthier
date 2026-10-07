#include "AsciiTabReader.h"

#include <cstring>

#include <algorithm>
#include <cmath>
#include <string>

namespace luthier
{

//==============================================================================
bool TabImportDiagnostics::isPartial() const noexcept
{
    return skippedLines > 0 || ignoredGlyphs > 0 || splitFrets > 0 || ! warnings.isEmpty();
}

juce::String TabImportDiagnostics::summary() const
{
    if (notes == 0)
    {
        juce::String s ("No tablature found");
        if (skippedLines > 0)
            s << " (" << skippedLines << " line" << (skippedLines == 1 ? "" : "s") << " skipped)";
        return s + ".";
    }

    juce::String s;
    s << "Loaded " << measures << " bar" << (measures == 1 ? "" : "s") << ", "
      << notes << " note" << (notes == 1 ? "" : "s") << " (" << numStrings << " strings)";

    if (skippedLines > 0)
        s << "; " << skippedLines << " line" << (skippedLines == 1 ? "" : "s") << " skipped";

    if (ignoredGlyphs > 0)
        s << "; " << ignoredGlyphs << " unknown symbol" << (ignoredGlyphs == 1 ? "" : "s") << " ignored";

    if (repeatsUnrolled > 0)
        s << "; " << repeatsUnrolled << " repeat" << (repeatsUnrolled == 1 ? "" : "s") << " unrolled";

    if (tuningFromStringNames)
        s << "; tuning from string names";
    else if (! tuningFromHeader)
        s << "; standard tuning assumed";

    return s + ".";
}

//==============================================================================
namespace
{
    using Type = ScoreTechnique::Type;
    using Line = std::vector<juce::juce_wchar>;

    constexpr int kMaxWarnings = 24;
    constexpr int kMaxFretValue = 36;
    constexpr int kMaxRepeat = 16;
    constexpr int kMaxMeasuresPerSystem = 512;

    juce::juce_wchar at (const Line& l, int i) noexcept
    {
        return juce::isPositiveAndBelow (i, (int) l.size()) ? l[(size_t) i] : (juce::juce_wchar) 0;
    }

    bool isDigit (juce::juce_wchar c) noexcept { return c >= '0' && c <= '9'; }
    bool isSpace (juce::juce_wchar c) noexcept { return c == ' ' || c == '\t'; }
    bool isFill  (juce::juce_wchar c) noexcept { return c == '-' || isSpace (c); }

    Line toLine (const juce::String& s)
    {
        Line l;
        for (auto t = s.getCharPointer(); ! t.isEmpty(); ++t)
            l.push_back (*t);
        // Trailing whitespace carries nothing.
        while (! l.empty() && isSpace (l.back()))
            l.pop_back();
        return l;
    }

    juce::String toString (const Line& l, int from, int to)
    {
        juce::String s;
        for (int i = juce::jmax (0, from); i < juce::jmin (to, (int) l.size()); ++i)
            s += juce::String::charToString (l[(size_t) i]);
        return s;
    }

    void warn (TabImportDiagnostics* d, const juce::String& text)
    {
        if (d == nullptr)
            return;
        if (d->warnings.size() < kMaxWarnings)
            d->warnings.add (text);
        else if (d->warnings.size() == kMaxWarnings)
            d->warnings.add ("...");
    }

    //==========================================================================
    // Note names (tuning headers and per-string prefixes).

    int pitchClassOfLetter (juce::juce_wchar c) noexcept
    {
        switch ((int) juce::CharacterFunctions::toUpperCase (c))
        {
            case 'C': return 0;  case 'D': return 2;  case 'E': return 4; case 'F': return 5;
            case 'G': return 7;  case 'A': return 9;  case 'B': return 11;
            case 'H': return 11;   // German / Scandinavian spelling of B
            default:  return -1;
        }
    }

    struct NameToken { int pitchClass = 0; int octave = -1; bool lower = false; };

    /** Tokenises "E A D G B E", "DADGAD", "Eb Ab Db Gb Bb Eb" or "E2 A2 D3 G3 B3 E4".
        Empty when anything that is not a note name, separator or accidental shows up. */
    std::vector<NameToken> tokeniseNames (const juce::String& text)
    {
        const auto l = toLine (text);
        std::vector<NameToken> out;

        bool anyUpper = false, germanH = false;
        for (auto c : l)
        {
            if (pitchClassOfLetter (c) >= 0 && juce::CharacterFunctions::isUpperCase (c))
                anyUpper = true;
            if (c == 'H' || c == 'h')
                germanH = true;     // "E A D G H E": H is B and a bare B is Bb
        }

        for (int i = 0; i < (int) l.size();)
        {
            const auto c = l[(size_t) i];

            if (c == '(' || c == '[')
                break;                        // "(standard)" style trailer

            if (isSpace (c) || c == ',' || c == '-' || c == '/' || c == '|' || c == '.' || c == ':')
            {
                ++i;
                continue;
            }

            const int pc = pitchClassOfLetter (c);
            if (pc < 0)
                return {};

            NameToken t;
            t.pitchClass = (germanH && (c == 'B' || c == 'b')) ? 10 : pc;
            t.lower = juce::CharacterFunctions::isLowerCase (c);
            ++i;

            if (at (l, i) == '#')                     { t.pitchClass = (pc + 1) % 12;  ++i; }
            else if (at (l, i) == 'b' && anyUpper)    { t.pitchClass = (pc + 11) % 12; ++i; }

            if (isDigit (at (l, i)))
            {
                t.octave = (int) (at (l, i) - '0');
                ++i;
            }

            out.push_back (t);
            if (out.size() > (size_t) kMaxStrings)
                return {};
        }

        return out;
    }

    /** Assigns octaves to a highest-first list of names: the top string is
        anchored near the instrument's usual top note and each lower string is
        the nearest lower octave of its pitch class (tab-import-export 7.2). */
    std::vector<int> assignOctaves (const std::vector<NameToken>& highFirst)
    {
        std::vector<int> midi;
        if (highFirst.empty())
            return midi;

        const int n = (int) highFirst.size();

        // Bass-shaped when a four/five-string set tops out on G (EADG, BEADG);
        // a five-line guitar tab tops out on E or B.
        const bool bassLike = (n == 4 || n == 5) && highFirst.front().pitchClass == 7;
        const int topReference = bassLike ? 43 : 64;

        int previous = -1;
        for (int s = 0; s < n; ++s)
        {
            const auto& t = highFirst[(size_t) s];
            int note;

            if (t.octave >= 0)
            {
                note = juce::jlimit (0, 127, (t.octave + 1) * 12 + t.pitchClass);
            }
            else if (previous < 0)
            {
                // Nearest octave of the pitch class to the reference.
                note = t.pitchClass;
                while (note + 12 <= topReference + 6) note += 12;
                if (std::abs (note - topReference) > std::abs (note - 12 - topReference) && note >= 12)
                    note -= 12;
            }
            else
            {
                // The largest note of this pitch class strictly below the previous string.
                note = t.pitchClass;
                while (note + 12 < previous) note += 12;
                if (note >= previous) note -= 12;
                if (note < 0) note = t.pitchClass;
            }

            midi.push_back (juce::jlimit (0, 127, note));
            previous = midi.back();
        }

        return midi;
    }

    int spanOf (const std::vector<int>& midi)
    {
        return midi.empty() ? 0 : midi.front() - midi.back();
    }

    //==========================================================================
    // Named tunings recognised from a "Tuning:" header (tab-import-export 7.1).
    // Step offsets ("half step down", "1 1/2 steps down") are not in the table:
    // stepsDownSemitones reads them and they shift whatever base this names.
    struct TabTuning { const char* match; const char* name; int count; int notes[kMaxStrings]; };

    const TabTuning* matchTuning (const juce::String& lower)
    {
        static const TabTuning table[] = {
            { "drop c#",            "Drop C#", 6, { 63, 58, 54, 49, 44, 37 } },
            { "drop db",            "Drop Db", 6, { 63, 58, 54, 49, 44, 37 } },
            { "drop c",             "Drop C", 6, { 62, 57, 53, 48, 43, 36 } },
            { "drop b",             "Drop B", 6, { 61, 56, 52, 47, 42, 35 } },
            { "drop a#",            "Drop A#", 6, { 60, 55, 51, 46, 41, 34 } },
            { "drop a",             "Drop A", 7, { 64, 59, 55, 50, 45, 40, 33 } },
            { "drop g#",            "Drop G#", 6, { 59, 54, 50, 45, 40, 32 } },
            { "drop g",             "Drop G", 6, { 58, 53, 49, 44, 39, 31 } },
            { "drop e",             "Drop E", 8, { 64, 59, 55, 50, 45, 40, 35, 28 } },
            { "double drop d",      "Double Drop D", 6, { 62, 59, 55, 50, 45, 38 } },
            { "drop d",             "Drop D", 6, { 64, 59, 55, 50, 45, 38 } },
            { "dadgad",             "DADGAD", 6, { 62, 57, 55, 50, 45, 38 } },
            { "dadf#ad",            "Open D", 6, { 62, 57, 54, 50, 45, 38 } },
            { "open gm",            "Open Gm", 6, { 62, 58, 55, 50, 43, 38 } },
            { "open dm",            "Open Dm", 6, { 62, 57, 53, 50, 45, 38 } },
            { "open em",            "Open Em", 6, { 64, 59, 55, 52, 47, 40 } },
            { "open g",             "Open G", 6, { 62, 59, 55, 50, 43, 38 } },
            { "open d",             "Open D", 6, { 62, 57, 54, 50, 45, 38 } },
            { "open e",             "Open E", 6, { 64, 59, 56, 52, 47, 40 } },
            { "open a",             "Open A", 6, { 64, 61, 57, 52, 45, 40 } },
            { "open c",             "Open C", 6, { 64, 60, 55, 48, 43, 36 } },
            { "nashville",          "Nashville", 6, { 64, 59, 67, 62, 57, 52 } },
            { "eb standard",        "Eb Standard", 6, { 63, 58, 54, 49, 44, 39 } },
            { "e flat standard",    "Eb Standard", 6, { 63, 58, 54, 49, 44, 39 } },
            { "e flat",             "Eb Standard", 6, { 63, 58, 54, 49, 44, 39 } },
            { "d# standard",        "Eb Standard", 6, { 63, 58, 54, 49, 44, 39 } },
            { "d standard",         "D Standard", 6, { 62, 57, 53, 48, 43, 38 } },
            { "db standard",        "C# Standard", 6, { 61, 56, 52, 47, 42, 37 } },
            { "c# standard",        "C# Standard", 6, { 61, 56, 52, 47, 42, 37 } },
            { "c standard",         "C Standard", 6, { 60, 55, 51, 46, 41, 36 } },
            { "b standard",         "B Standard", 7, { 64, 59, 55, 50, 45, 40, 35 } },
            { "baritone",           "Baritone", 6, { 59, 54, 50, 45, 40, 35 } },
            { "7 string",           "7-string", 7, { 64, 59, 55, 50, 45, 40, 35 } },
            { "7-string",           "7-string", 7, { 64, 59, 55, 50, 45, 40, 35 } },
            { "seven string",       "7-string", 7, { 64, 59, 55, 50, 45, 40, 35 } },
            { "8 string",           "8-string", 8, { 64, 59, 55, 50, 45, 40, 35, 30 } },
            { "8-string",           "8-string", 8, { 64, 59, 55, 50, 45, 40, 35, 30 } },
            { "eight string",       "8-string", 8, { 64, 59, 55, 50, 45, 40, 35, 30 } },
            { "6 string bass",      "6-string bass", 6, { 48, 43, 38, 33, 28, 23 } },
            { "6-string bass",      "6-string bass", 6, { 48, 43, 38, 33, 28, 23 } },
            { "5 string bass",      "5-string bass", 5, { 43, 38, 33, 28, 23 } },
            { "5-string bass",      "5-string bass", 5, { 43, 38, 33, 28, 23 } },
            { "bass",               "Bass", 4, { 43, 38, 33, 28 } },
            { "ukulele",            "Ukulele", 4, { 69, 64, 60, 67 } },
            { "uke",                "Ukulele", 4, { 69, 64, 60, 67 } },
            { "mandolin",           "Mandolin", 4, { 76, 69, 62, 55 } },
            { "banjo",              "Banjo", 5, { 62, 59, 55, 50, 67 } },
            { "standard",           "Standard", 6, { 64, 59, 55, 50, 45, 40 } },
            { "normal tuning",      "Standard", 6, { 64, 59, 55, 50, 45, 40 } },
            { "eadgbe",             "Standard", 6, { 64, 59, 55, 50, 45, 40 } },
        };

        for (const auto& t : table)
            if (lower.contains (t.match))
                return &t;

        return nullptr;
    }

    /** "half step down", "1/2 step down", "1 step down", "1.5 steps down",
        "one and a half steps down", "2 whole steps down", "down a half step",
        "3 semitones down", "tuned down a tone": the semitones to lower a
        tuning by; 0 when the line says nothing of the kind. */
    int stepsDownSemitones (const juce::String& lower)
    {
        if (! (lower.contains ("down") || lower.contains ("lower") || lower.contains ("flat ")))
            return 0;
        if (lower.contains ("step up") || lower.contains ("steps up") || lower.contains ("tuned up"))
            return 0;

        const bool semitoneUnit = lower.contains ("semitone") || lower.contains ("half-tone") || lower.contains ("halftone");
        const bool stepUnit = lower.contains ("step") || lower.contains ("tone") || lower.contains ("whole") || lower.contains ("half");
        if (! semitoneUnit && ! stepUnit)
            return 0;

        // Compound amounts first ("1 1/2" is not 1), then a bare number, then words.
        double steps = -1.0;
        if (lower.contains ("one and a half") || lower.contains ("1 and a half") || lower.contains ("1 1/2") || lower.contains ("1.5")
             || lower.contains ("1-1/2") || lower.contains ("three half"))
            steps = 1.5;
        else if (lower.contains ("two and a half") || lower.contains ("2 1/2") || lower.contains ("2.5"))
            steps = 2.5;

        // A bare number: "2 steps down", "3 semitones down".
        for (int i = 0; i < lower.length() && steps < 0.0; ++i)
        {
            if (! isDigit (lower[i]) || (i > 0 && (isDigit (lower[i - 1]) || lower[i - 1] == '/' || lower[i - 1] == '.')))
                continue;
            if (i + 1 < lower.length() && (lower[i + 1] == '/' || isDigit (lower[i + 1]) || lower[i + 1] == '-'))
                continue;                      // "1/2" is a word below; "8-string" is not an amount
            const int v = (int) lower[i] - '0';
            if (v >= 1 && v <= 6)
                steps = (double) v;
            break;
        }

        if (steps < 0.0)
        {
            if (lower.contains ("half") || lower.contains ("1/2") || lower.contains ("semitone"))
                steps = 0.5;
            else if (lower.contains ("whole") || lower.contains ("full") || lower.contains ("one ") || lower.contains ("a step") || lower.contains ("a tone"))
                steps = 1.0;
            else if (lower.contains ("two ")) steps = 2.0;
            else if (lower.contains ("three ")) steps = 3.0;
        }

        if (steps < 0.0)
            return 0;

        const int semis = semitoneUnit && ! lower.contains ("step") ? (int) std::round (steps)
                                                                    : (int) std::round (steps * 2.0);
        return juce::jlimit (0, 12, semis);
    }

    /** A Roman numeral after a keyword: "Capo III" -> 3, "Capo: IV" -> 4; -1 for none. */
    int romanNumeralIn (const juce::String& lower, const juce::String& keyword)
    {
        const int k = lower.indexOf (keyword);
        if (k < 0) return -1;
        auto rest = lower.substring (k + keyword.length()).trim();
        if (rest.startsWithChar (':') || rest.startsWithChar ('=') || rest.startsWithChar ('-'))
            rest = rest.substring (1).trim();
        if (rest.startsWith ("on ")) rest = rest.substring (3).trim();
        if (rest.startsWith ("at ")) rest = rest.substring (3).trim();
        if (rest.startsWith ("fret ")) rest = rest.substring (5).trim();

        juce::String numeral;
        for (int i = 0; i < rest.length() && i < 5; ++i)
        {
            const auto c = rest[i];
            if (c == 'i' || c == 'v' || c == 'x') numeral += juce::String::charToString (c);
            else break;
        }
        if (numeral.isEmpty()) return -1;
        if (numeral.length() < rest.length() && juce::CharacterFunctions::isLetter (rest[numeral.length()]))
            return -1;

        static const char* const table[] = { "i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix", "x", "xi", "xii" };
        for (int n = 0; n < 12; ++n)
            if (numeral == table[n]) return n + 1;
        return -1;
    }

    const char* const kTuningKeywords[] = { "tuning", "tuned", "tune ", "stimmung", "afina", "accordage", "accordatura" };

    //==========================================================================
    // Line classification.

    enum class LineKind { empty, staff, annotation, chords, header, other };

    struct LineInfo
    {
        LineKind kind = LineKind::other;
        Line text;
        int bodyStart = 0;        ///< first body column (after the string label and its bar)
        int bodyEnd = 0;          ///< one past the last body column (the closing bar, if any)
        int namePitchClass = -1;  ///< the string-name prefix, if any
        int nameOctave = -1;
        bool nameLower = false;
        int repeatCount = 0;      ///< a trailing "x4" after the closing bar
        std::vector<std::pair<int, int>> muteSpans;   ///< PM annotation: raw column ranges
        bool letRing = false;
    };

    bool isTabGlyph (juce::juce_wchar c) noexcept
    {
        static const juce::String allowed ("-|0123456789 hpbrsvtxXHPBRSTV~^/\\<>[]()*.=:,_LMwfulon+");
        return c < 128 && allowed.containsChar (c);
    }

    int repeatCountIn (const juce::String& trimmedLower)
    {
        // "x4", "(x4)", "4x", "x 4", "play 4 times", "repeat 3x", "3 times"
        const auto digits = trimmedLower.retainCharacters ("0123456789");
        if (digits.isEmpty() || digits.length() > 2)
            return 0;

        const auto letters = trimmedLower.retainCharacters ("abcdefghijklmnopqrstuvwxyz");
        const bool shaped = letters == "x" || letters == "times" || letters == "playtimes"
                            || letters == "repeatx" || letters == "repeat" || letters == "xtimes"
                            || letters == "repeattimes";

        if (! shaped)
            return 0;

        return juce::jlimit (0, kMaxRepeat, digits.getIntValue());
    }

    bool looksLikeChordToken (const juce::String& token)
    {
        if (token == "N.C." || token == "NC" || token == "N.C" || token == "(N.C.)")
            return true;

        if (token.isEmpty() || pitchClassOfLetter (token[0]) < 0
             || ! juce::CharacterFunctions::isUpperCase (token[0]))
            return false;

        static const juce::String tail ("#bmajindsugMA+-0123456789/°øCDEFGAB()");
        for (int i = 1; i < token.length(); ++i)
            if (! tail.containsChar (token[i]))
                return false;

        return true;
    }

    LineInfo classify (const juce::String& raw)
    {
        LineInfo info;
        info.text = toLine (raw);
        const auto& l = info.text;
        const int n = (int) l.size();

        const auto trimmed = raw.trim();
        if (trimmed.isEmpty())
        {
            info.kind = LineKind::empty;
            return info;
        }

        const auto lower = trimmed.toLowerCase();

        // ---- annotation rows above a staff --------------------------------------------
        if (lower.startsWith ("pm") || lower.startsWith ("p.m") || lower.startsWith ("let ring")
             || lower.startsWith ("n.h") || lower.startsWith ("a.h") || lower.startsWith ("p.h")
             || lower.startsWith ("t.h") || lower.startsWith ("h.h") || lower == "full" || lower == "1/2")
        {
            info.kind = LineKind::annotation;
            info.letRing = lower.startsWith ("let ring");

            if (lower.startsWith ("pm") || lower.startsWith ("p.m"))
            {
                // Every "PM" starts a span that runs over the dashes/dots after it.
                for (int i = 0; i < n; ++i)
                {
                    if ((l[(size_t) i] == 'P' || l[(size_t) i] == 'p')
                         && (at (l, i + 1) == 'M' || at (l, i + 1) == 'm' || at (l, i + 1) == '.'))
                    {
                        int j = i + 1;
                        while (j < n && (at (l, j) == 'M' || at (l, j) == 'm' || at (l, j) == '.')) ++j;
                        int end = j;
                        while (j < n && (at (l, j) == '-' || at (l, j) == '.' || at (l, j) == '|' || isSpace (at (l, j))))
                        {
                            if (at (l, j) != ' ') end = j + 1;
                            ++j;
                        }
                        info.muteSpans.emplace_back (i, juce::jmax (end, i + 4));
                        i = j;
                    }
                }
            }
            return info;
        }

        // A row of bend amounts, tap/slap marks or vibrato waves only.
        {
            bool onlyMarks = true, any = false;
            for (auto c : l)
            {
                if (isSpace (c)) continue;
                any = true;
                static const juce::String marks ("^~.vV*TtPSfulh1/234()");
                if (c >= 128 || ! marks.containsChar (c)) { onlyMarks = false; break; }
            }
            if (any && onlyMarks && ! raw.containsChar ('-') && ! raw.containsChar ('|'))
            {
                info.kind = LineKind::annotation;
                return info;
            }
        }

        // ---- staff lines --------------------------------------------------------------
        int p = 0;
        while (p < n && isSpace (l[(size_t) p])) ++p;

        int bodyStart = -1;

        // A string name: "e|", "B|", "D#|", "Eb|", "E2|", "e:", "E -----".
        if (p < n && pitchClassOfLetter (l[(size_t) p]) >= 0)
        {
            int q = p + 1;
            int pc = pitchClassOfLetter (l[(size_t) p]);
            const bool lowerCase = juce::CharacterFunctions::isLowerCase (l[(size_t) p]);
            int octave = -1;

            if (at (l, q) == '#')      { pc = (pc + 1) % 12; ++q; }
            else if (at (l, q) == 'b') { pc = (pc + 11) % 12; ++q; }

            if (isDigit (at (l, q)) && ! isDigit (at (l, q + 1)) && (at (l, q + 1) == '|' || at (l, q + 1) == ':'))
            {
                octave = (int) (at (l, q) - '0');
                ++q;
            }

            int spaces = 0;
            while (isSpace (at (l, q)) && spaces < 2) { ++q; ++spaces; }

            const auto d = at (l, q);
            if (d == '|' || d == ':')
            {
                bodyStart = q + 1;
                info.namePitchClass = pc; info.nameOctave = octave; info.nameLower = lowerCase;
            }
            else if (d == '-' && spaces > 0)
            {
                bodyStart = q;
                info.namePitchClass = pc; info.nameOctave = octave; info.nameLower = lowerCase;
            }
            else if (d == '-' && (q == p + 1))
            {
                // "E---3---": a bare letter straight into the fill.
                bodyStart = q;
                info.namePitchClass = pc; info.nameOctave = octave; info.nameLower = lowerCase;
            }
        }

        // A generic label before an early bar: "S1|", "Gtr|", "  |".
        if (bodyStart < 0)
        {
            for (int i = p; i < n && i < p + 8; ++i)
            {
                if (l[(size_t) i] == '|')
                {
                    bool labelOk = true;
                    for (int k = p; k < i; ++k)
                        if (l[(size_t) k] == '-' || isDigit (l[(size_t) k])) labelOk = false;
                    if (labelOk) bodyStart = i + 1;
                    break;
                }
                if (l[(size_t) i] == '-')
                    break;
            }
        }

        if (bodyStart < 0 && p < n && l[(size_t) p] == '-')
            bodyStart = p;

        if (bodyStart >= 0)
        {
            // The body runs to the last bar line when there is one past the start.
            int bodyEnd = n;
            for (int i = n - 1; i > bodyStart; --i)
                if (l[(size_t) i] == '|') { bodyEnd = i; break; }

            int dashes = 0, allowed = 0, digits = 0, bars = 0, dead = 0;
            for (int i = bodyStart; i < bodyEnd; ++i)
            {
                const auto c = l[(size_t) i];
                if (isFill (c)) ++dashes;
                if (isTabGlyph (c)) ++allowed;
                if (isDigit (c)) ++digits;
                if (c == '|') ++bars;
                if (c == 'x' || c == 'X') ++dead;
            }

            const int length = bodyEnd - bodyStart;
            const bool hasContent = digits > 0 || dead > 0 || bars > 0 || bodyEnd < n;
            const bool labelled = info.namePitchClass >= 0 || bodyStart > p;

            if (length >= 3 && allowed * 10 >= length * 9 && dashes * 4 >= length
                 && (hasContent || labelled))
            {
                info.kind = LineKind::staff;
                info.bodyStart = bodyStart;
                info.bodyEnd = bodyEnd;

                if (bodyEnd < n)
                    info.repeatCount = repeatCountIn (toString (l, bodyEnd + 1, n).trim().toLowerCase());

                return info;
            }
        }

        // ---- chord names over lyrics --------------------------------------------------
        {
            juce::StringArray tokens;
            tokens.addTokens (trimmed, " \t", "");
            tokens.removeEmptyStrings();
            bool allChords = ! tokens.isEmpty();
            for (const auto& t : tokens)
                if (! looksLikeChordToken (t)) { allChords = false; break; }
            if (allChords)
            {
                info.kind = LineKind::chords;
                return info;
            }
        }

        info.kind = LineKind::other;
        return info;
    }

    //==========================================================================
    // Header lines.

    struct Header
    {
        double tempo = 120.0;
        int numerator = 4, denominator = 4;
        int capo = 0;
        juce::String tuningName { "Standard" };
        std::vector<int> tuning;        ///< highest first; empty until a header sets it
        juce::String key;               ///< "Am", "G": the page's own Key: header
        bool tuningSet = false, tempoSet = false, timeSet = false, capoSet = false, keySet = false;
    };

    int firstNumber (const juce::String& s, int lo, int hi)
    {
        int value = 0, digits = 0;
        for (int i = 0; i <= s.length(); ++i)
        {
            if (i < s.length() && isDigit (s[i]))
            {
                value = juce::jmin (100000, value * 10 + ((int) s[i] - '0'));
                ++digits;
            }
            else if (digits > 0)
            {
                if (value >= lo && value <= hi) return value;
                value = 0; digits = 0;
            }
        }
        return -1;
    }

    bool readTimeSignature (const juce::String& s, int& num, int& den)
    {
        const int slash = s.indexOfChar ('/');
        if (slash <= 0) return false;

        int a = 0, i = slash - 1;
        while (i >= 0 && isSpace (s[i])) --i;
        int mult = 1;
        while (i >= 0 && isDigit (s[i]) && mult <= 10) { a += (s[i] - '0') * mult; mult *= 10; --i; }
        if (mult == 1) return false;

        int b = 0, j = slash + 1;
        while (j < s.length() && isSpace (s[j])) ++j;
        int bd = 0;
        while (j < s.length() && isDigit (s[j]) && bd < 2) { b = b * 10 + (s[j] - '0'); ++j; ++bd; }
        if (bd == 0) return false;

        if (! (b == 2 || b == 4 || b == 8 || b == 16) || a < 1 || a > 32)
            return false;

        num = a; den = b;
        return true;
    }

    /** True when the line was a header and was applied. */
    bool readHeader (const juce::String& raw, Header& h, TabImportDiagnostics* d)
    {
        const auto line = raw.trim();
        const auto lower = line.toLowerCase();
        bool used = false;

        const auto valueAfterKeyword = [&] (const juce::String& keyword)
        {
            const int k = lower.indexOf (keyword);
            if (k < 0) return juce::String();
            auto v = line.substring (k + keyword.length()).trim();
            if (v.startsWithChar (':') || v.startsWithChar ('=') || v.startsWithChar ('-'))
                v = v.substring (1).trim();
            return v;
        };

        if (AsciiTabReader::looksLikeTuningStatement (lower))
        {
            std::vector<int> midi;
            juce::String name;
            if (AsciiTabReader::parseTuningStatement (line, midi, name))
            {
                h.tuning = midi;
                h.tuningName = name;
                h.tuningSet = true;
                used = true;
            }
            else
            {
                juce::String value = valueAfterKeyword ("tuning");
                if (value.isEmpty()) value = valueAfterKeyword ("tuned");
                if (value.isNotEmpty())
                {
                    warn (d, "tuning \"" + value + "\" not recognised; standard assumed");
                    h.tuningName = value;
                    used = true;
                }
            }
        }

        if (lower.contains ("capo") || lower.contains ("capodastre") || lower.contains ("cejilla"))
        {
            int n = firstNumber (line, 0, 24);
            if (n < 0) n = romanNumeralIn (lower, "capo");
            h.capo = (lower.contains ("no capo") || lower.contains ("none") || lower.contains ("without")) ? 0 : juce::jmax (0, n);
            h.capoSet = true;
            used = true;
        }

        // "Key: Am", "Key of G", "Key - F#m", "Tonart: A-Moll", "Tonalidad: Sol".
        {
            int root = -1; bool minor = false;
            if (AsciiTabReader::parseKeyStatement (line, root, minor))
            {
                h.key = AsciiTabReader::keyName (root, minor);
                h.keySet = true;
                used = true;
            }
        }

        if (lower.contains ("tempo") || lower.contains ("bpm") || lower.startsWith ("q =")
             || lower.startsWith ("q=") || line.containsChar ((juce::juce_wchar) 0x2669))
        {
            const int n = firstNumber (line, 20, 300);
            if (n > 0) { h.tempo = n; h.tempoSet = true; }
            used = true;
        }

        {
            int num = 4, den = 4;
            const bool wholeLineIsMeter = lower.length() <= 6 && lower.containsChar ('/');
            // "Tempo: 1/4 = 120" names a note value, not a metre.
            const int slash = lower.indexOfChar ('/');
            const bool fractionIsNoteValue = slash > 0 && lower.substring (slash + 1).trim().length() > 1
                                              && lower.substring (slash + 2).trimStart().startsWithChar ('=');
            if ((lower.contains ("time") || lower.contains ("meter") || lower.contains ("metre")
                  || lower.contains ("tempo") || wholeLineIsMeter)
                 && ! fractionIsNoteValue
                 && readTimeSignature (line, num, den))
            {
                h.numerator = num; h.denominator = den; h.timeSet = true;
                used = true;
            }
        }

        return used;
    }

    //==========================================================================
    // Tokens read from a staff line, before they become notes.

    struct Token
    {
        int line = 0;             ///< line within the system (0 = top string)
        int bodyColumn = 0;
        int rawColumn = 0;
        int measure = 0;          ///< measure within the system
        double beat = 0.0;        ///< within the measure
        int fret = 0;
        bool dead = false;
        bool tie = false;
        std::vector<ScoreTechnique> techniques;
        double duration = 0.25;
    };

    struct Pending
    {
        enum Kind { none, hammer, pull, legato, tap, slap, pop, slideIn, slideInFromAbove } kind = none;
    };

    ScoreTechnique make (Type t, double v = 0.0, double v2 = 0.0)
    {
        ScoreTechnique s;
        s.type = t; s.value = v; s.secondValue = v2;
        return s;
    }

    bool hasType (const std::vector<ScoreTechnique>& v, Type t)
    {
        for (const auto& s : v) if (s.type == t) return true;
        return false;
    }

    /** Reads a bend amount after 'b' / '^' / "pb": "9" (target fret), "full",
        "1/2", "1 1/2", "(full)". Returns semitones; 2 when nothing was written. */
    double readBendAmount (const Line& l, int& c, int end, int fret)
    {
        const bool paren = at (l, c) == '(';
        if (paren) ++c;

        double semis = 2.0;
        int digits = 0, target = 0;

        while (c < end && isDigit (at (l, c)) && digits < 2)
        {
            target = target * 10 + (int) (at (l, c) - '0');
            ++c; ++digits;
        }

        if (digits > 0)
        {
            if (at (l, c) == '/' && isDigit (at (l, c + 1)))
            {
                // "1/2", "1/4", "3/4"
                const int den = (int) (at (l, c + 1) - '0');
                c += 2;
                semis = den > 0 ? 2.0 * (double) target / (double) den : 1.0;

                // "1 1/2" was read as "1" then " 1/2": already handled below.
            }
            else if (at (l, c) == ' ' && isDigit (at (l, c + 1)) && at (l, c + 2) == '/' && isDigit (at (l, c + 3)))
            {
                const int num = (int) (at (l, c + 1) - '0');
                const int den = (int) (at (l, c + 3) - '0');
                c += 4;
                semis = 2.0 * (double) target + (den > 0 ? 2.0 * (double) num / (double) den : 0.0);
            }
            else
            {
                semis = target > fret ? (double) (target - fret) : (double) target;
            }
        }
        else
        {
            const auto word = toString (l, c, c + 5).toLowerCase();
            if (word.startsWith ("full"))       { semis = 2.0; c += 4; }
            else if (word.startsWith ("whole")) { semis = 2.0; c += 5; }
            else if (word.startsWith ("half"))  { semis = 1.0; c += 4; }
        }

        if (paren && at (l, c) == ')') ++c;

        return juce::jlimit (0.25, 24.0, semis);
    }

    int peekDigits (const Line& l, int c, int end)
    {
        int value = 0, digits = 0;
        while (c < end && isDigit (at (l, c)) && digits < 2)
        {
            value = value * 10 + (int) (at (l, c) - '0');
            ++c; ++digits;
        }
        return digits > 0 ? value : -1;
    }

    //==========================================================================
    /*  Rhythm and count lines above a staff ("1 e & a 2 e & a", "q q e e h").
        Each symbol's column becomes a (column, beat) anchor, and the notes of
        the system are placed by interpolating between anchors, so a tab whose
        spacing does not follow the rhythm still plays in time. Conservative:
        a line that is not clearly a rhythm line, or whose anchors do not cover
        the notes, changes nothing. */
    struct RhythmAnchor { int column; double beat; };

    std::vector<RhythmAnchor> parseRhythmAnchors (const juce::String& raw, double beatsPerMeasure)
    {
        std::vector<RhythmAnchor> anchors;

        if (raw.length() > 600 || beatsPerMeasure <= 0.0)
            return anchors;

        const std::string text (raw.toRawUTF8());
        struct Tok { int column; std::string word; };
        std::vector<Tok> toks;

        for (size_t i = 0; i < text.size();)
        {
            if (text[i] == ' ' || text[i] == '\t') { ++i; continue; }
            const size_t from = i;
            while (i < text.size() && text[i] != ' ' && text[i] != '\t') ++i;
            toks.push_back ({ (int) from, text.substr (from, i - from) });
            if (toks.size() > 200) return anchors;
        }

        if (toks.size() < 3)
            return anchors;

        // ---- counting: 1 e & a 2 ... ------------------------------------------------
        bool allCount = true;
        int numbers = 0, subs = 0;
        for (const auto& t : toks)
        {
            const auto& w = t.word;
            if (w.size() == 1 && w[0] >= '1' && w[0] <= '9') ++numbers;
            else if (w == "&" || w == "+" || w == "e" || w == "a" || w == "and" || w == "E" || w == "A") ++subs;
            else { allCount = false; break; }
        }

        if (allCount && numbers >= 2 && subs >= 1)
        {
            int measure = 0, lastNumber = 0;
            double lastBase = 0.0;
            for (const auto& t : toks)
            {
                const auto& w = t.word;
                double beat;
                if (w.size() == 1 && w[0] >= '1' && w[0] <= '9')
                {
                    const int n = w[0] - '0';
                    if (n <= lastNumber) ++measure;
                    lastNumber = n;
                    lastBase = (double) (n - 1);
                    beat = measure * beatsPerMeasure + lastBase;
                }
                else
                {
                    if (lastNumber == 0) return {};
                    const double frac = (w == "&" || w == "+" || w == "and") ? 0.5
                                      : (w == "e" || w == "E") ? 0.25 : 0.75;
                    beat = measure * beatsPerMeasure + lastBase + frac;
                }

                if (! anchors.empty() && beat <= anchors.back().beat + 1.0e-9)
                    return {};
                anchors.push_back ({ t.column, beat });
            }
            return anchors;
        }

        // ---- durations: w h q e s t, with "." (dotted) or "3" (triplet) ---------------
        double cursor = 0.0;
        int symbols = 0;
        for (const auto& t : toks)
        {
            const auto& w = t.word;
            if (w.empty() || w.size() > 3)
                return {};

            double length;
            switch (w[0])
            {
                case 'w': length = 4.0; break;
                case 'h': length = 2.0; break;
                case 'q': length = 1.0; break;
                case 'e': length = 0.5; break;
                case 's': length = 0.25; break;
                case 't': length = 0.125; break;
                default: return {};
            }

            for (size_t k = 1; k < w.size(); ++k)
            {
                if (w[k] == '.') length *= 1.5;
                else if (w[k] == '3') length *= 2.0 / 3.0;
                else return {};
            }

            anchors.push_back ({ t.column, cursor });
            cursor += length;
            ++symbols;
        }

        // A line of "e e e" is as likely to be string names or chord letters.
        if (symbols < 3)
            return {};

        return anchors;
    }

    //==========================================================================
    struct ChordMark { int measure = 0; double beat = 0.0; juce::String name; };

    struct System
    {
        std::vector<LineInfo*> lines;
        std::vector<Token> tokens;
        std::vector<ChordMark> chords;      ///< chord names written above the staff
        int measuresOnLines = 1;
        int repeatStart = -1, repeatEnd = -1, repeatCount = 0;
        bool rhythmApplied = false;
    };

    //==========================================================================
    // Lines above a staff: chord names, strumming patterns and accents
    // (universal tab player, item 2). Each glyph is placed by its column: the
    // note column nearest to it (within three columns, preferring the one at
    // or after it) says which beat it belongs to.

    struct ColumnMark { int column; juce::String text; };

    std::vector<ColumnMark> columnTokens (const juce::String& raw)
    {
        std::vector<ColumnMark> out;
        const auto l = toLine (raw);
        for (int i = 0; i < (int) l.size() && out.size() < 256;)
        {
            if (isSpace (l[(size_t) i])) { ++i; continue; }
            const int from = i;
            while (i < (int) l.size() && ! isSpace (l[(size_t) i])) ++i;
            out.push_back ({ from, toString (l, from, i) });
        }
        return out;
    }

    /** The token(s) at the note column nearest `column`; empty when none is within reach. */
    std::vector<Token*> tokensNearColumn (System& sys, int column, int reach = 3)
    {
        int bestDistance = reach + 1, bestColumn = -1;
        for (const auto& t : sys.tokens)
        {
            const int distance = std::abs (t.rawColumn - column);
            // A name is written at or just before the note it labels: prefer rightwards.
            const int scored = t.rawColumn >= column ? distance : distance + 1;
            if (scored < bestDistance) { bestDistance = scored; bestColumn = t.rawColumn; }
        }

        std::vector<Token*> out;
        if (bestColumn < 0) return out;
        for (auto& t : sys.tokens)
            if (t.rawColumn == bestColumn)
                out.push_back (&t);
        return out;
    }

    /** "Am      G       C": chord names over the staff become chord symbols on the beats under them. */
    int readChordLine (System& sys, const juce::String& raw)
    {
        int placed = 0;
        for (const auto& mark : columnTokens (raw))
        {
            if (! looksLikeChordToken (mark.text)) continue;
            const auto near = tokensNearColumn (sys, mark.column, 4);
            if (near.empty()) continue;
            if (sys.chords.size() >= 512) break;
            sys.chords.push_back ({ near.front()->measure, near.front()->beat, mark.text });
            ++placed;
        }
        return placed;
    }

    enum class StrumGlyph { none, down, up, mute, rest, accent };

    StrumGlyph strumGlyphFor (const juce::String& g)
    {
        if (g == "D" || g == "d" || g == "v" || g == "V" || g == juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x93"))) return StrumGlyph::down;
        if (g == "U" || g == "u" || g == "^" || g == juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x91")))  return StrumGlyph::up;
        if (g == "x" || g == "X") return StrumGlyph::mute;
        if (g == "-" || g == "." || g == "_") return StrumGlyph::rest;
        if (g == ">") return StrumGlyph::accent;
        return StrumGlyph::none;
    }

    /** Splits "DU" / "D-DU" / "↓↑" into one glyph per column; empty unless every glyph is a strum glyph. */
    std::vector<ColumnMark> strumGlyphs (const juce::String& raw, int& strokes)
    {
        std::vector<ColumnMark> out;
        strokes = 0;
        auto text = raw;
        // "Strumming: D DU UDU" / "Pattern - D D U U D U": the glyphs follow the label.
        const int colon = text.indexOfChar (':');
        int offset = 0;
        if (colon >= 0 && colon <= 24 && text.substring (0, colon).trim().containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ ."))
        {
            offset = colon + 1;
            text = text.substring (offset);
        }
        for (const auto& mark : columnTokens (text))
        {
            int column = mark.column + offset;
            for (auto t = mark.text.getCharPointer(); ! t.isEmpty(); ++t)
            {
                const auto g = juce::String::charToString (*t);
                const auto kind = strumGlyphFor (g);
                if (kind == StrumGlyph::none) return {};
                if (kind == StrumGlyph::down || kind == StrumGlyph::up || kind == StrumGlyph::mute) ++strokes;
                out.push_back ({ column, g });
                ++column;
                if (out.size() > 512) return {};
            }
        }
        return out;
    }

    bool isStrumLine (const juce::String& raw)
    {
        int strokes = 0;
        const auto glyphs = strumGlyphs (raw, strokes);
        if (glyphs.empty() || strokes < 2) return false;
        // "D D D D" over a staff is a strum line, not four D chords, when nothing else is on it.
        return true;
    }

    /** A strumming line: every stroke marks the notes under it with a pick direction; "x" mutes them. */
    int readStrumLine (System& sys, const juce::String& raw)
    {
        int strokes = 0;
        int applied = 0;
        for (const auto& g : strumGlyphs (raw, strokes))
        {
            const auto kind = strumGlyphFor (g.text);
            if (kind == StrumGlyph::none || kind == StrumGlyph::rest) continue;
            for (auto* t : tokensNearColumn (sys, g.column, 2))
            {
                if (kind == StrumGlyph::mute)
                {
                    t->dead = true;
                    if (! hasType (t->techniques, Type::deadNote)) t->techniques.push_back (make (Type::deadNote));
                }
                else if (kind == StrumGlyph::accent)
                {
                    if (! hasType (t->techniques, Type::accent)) t->techniques.push_back (make (Type::accent));
                }
                else
                {
                    const auto type = kind == StrumGlyph::up ? Type::pickStrokeUp : Type::pickStrokeDown;
                    if (! hasType (t->techniques, type)) t->techniques.push_back (make (type));
                }
                ++applied;
            }
        }
        return applied;
    }

    /** A row of ">" above the staff accents the notes under each mark. */
    bool isAccentLine (const juce::String& raw)
    {
        const auto t = raw.trim();
        return t.isNotEmpty() && t.containsOnly ("> ") && t.containsChar ('>');
    }

    void parseSystem (System& sys, double beatsPerMeasure, TabImportDiagnostics* d,
                      const std::vector<LineInfo*>& annotationsAbove,
                      const std::vector<RhythmAnchor>* rhythm = nullptr)
    {
        const int numLines = (int) sys.lines.size();

        // Width of the widest body, in body-relative columns.
        int width = 0;
        for (const auto* li : sys.lines)
            width = juce::jmax (width, li->bodyEnd - li->bodyStart);

        /*  A two-digit fret on any string widens that column on every string
            (the writer's convention and what a hand-written tab does by
            accident), so the second digit's column carries no time. */
        std::vector<bool> widened ((size_t) width + 2, false);

        for (const auto* li : sys.lines)
        {
            const auto& l = li->text;
            for (int c = li->bodyStart; c + 1 < li->bodyEnd; ++c)
            {
                if (isDigit (at (l, c)) && isDigit (at (l, c + 1)) && ! isDigit (at (l, c - 1)))
                {
                    const int value = (int) (at (l, c) - '0') * 10 + (int) (at (l, c + 1) - '0');
                    if (value <= 24)
                        widened[(size_t) (c + 1 - li->bodyStart)] = true;
                }
            }
        }

        std::vector<int> prefix ((size_t) width + 3, 0);
        for (int k = 0; k < width + 2; ++k)
            prefix[(size_t) k + 1] = prefix[(size_t) k] + (widened[(size_t) k] ? 0 : 1);

        const auto slots = [&] (int a, int b)
        {
            a = juce::jlimit (0, width + 2, a);
            b = juce::jlimit (0, width + 2, b);
            return b > a ? prefix[(size_t) b] - prefix[(size_t) a] : 0;
        };

        const int beatsInt = (int) std::round (beatsPerMeasure);

        for (int row = 0; row < numLines; ++row)
        {
            const auto& li = *sys.lines[(size_t) row];
            const auto& l = li.text;
            const int bs = li.bodyStart, be = li.bodyEnd;

            int c = bs;
            int measure = 0;
            int measureStart = bs;

            // "|:" written as the leading bar of the line.
            if (at (l, c) == ':') { sys.repeatStart = 0; ++c; measureStart = c; }
            if (at (l, c) == '|') { ++c; measureStart = c; if (at (l, c) == ':') { sys.repeatStart = 0; ++c; measureStart = c; } }

            // The closing bar with ":|" before it.
            if (be < (int) l.size() && at (l, be - 1) == ':')
                sys.repeatEnd = -2;   // resolved once the measure count is known

            Pending pending;
            Token* previous = nullptr;

            const auto measureEndFor = [&] (int from)
            {
                for (int k = from; k < be; ++k)
                    if (at (l, k) == '|') return k;
                return be;
            };

            int measureEnd = measureEndFor (c);

            while (c < be)
            {
                const auto ch = at (l, c);

                if (ch == '|')
                {
                    // Consecutive bars are one boundary ("||"); ":|" ends a repeat, "|:" starts one.
                    const bool endsRepeat = at (l, c - 1) == ':';
                    ++c;
                    while (c < be && (at (l, c) == '|')) ++c;

                    if (endsRepeat) sys.repeatEnd = measure;
                    if (at (l, c) == ':') { ++c; sys.repeatStart = measure + 1; }

                    if (c < be)
                    {
                        ++measure;
                        measureStart = c;
                        measureEnd = measureEndFor (c);
                        pending = {};
                    }
                    continue;
                }

                if (ch == ':' || ch == ',' || isFill (ch))
                {
                    ++c;
                    continue;
                }

                const bool dead = ch == 'x' || ch == 'X';

                if (! dead && ! isDigit (ch))
                {
                    // Connectors and wrappers before a note.
                    switch ((int) ch)
                    {
                        case 'h': case 'H': pending.kind = Pending::hammer;  break;
                        case 'p': case 'P':
                            if (at (l, c + 1) == 'M' || at (l, c + 1) == 'm' || at (l, c + 1) == '.') { c += 2; while (at (l, c) == '.') ++c; continue; }
                            pending.kind = (ch == 'P') ? Pending::pop : Pending::pull; break;
                        case '^':           pending.kind = Pending::legato;  break;
                        case 't': case 'T': pending.kind = Pending::tap;     break;
                        case 's':           pending.kind = Pending::slideIn; break;
                        case 'S':           pending.kind = Pending::slap;    break;
                        case '/':           pending.kind = Pending::slideIn; break;
                        case '\\':          pending.kind = Pending::slideInFromAbove; break;
                        case '<': case '[': case '(': case '=': case '_': case '*': case '~':
                        case '>': case ']': case ')': case '.': case 'v': case 'V':
                            break;                       // wrappers are read by the note; stray marks are harmless
                        default:
                            if (d != nullptr) ++d->ignoredGlyphs;
                            break;
                    }
                    ++c;
                    continue;
                }

                // ---- a note ------------------------------------------------------------
                Token t;
                t.line = row;
                t.bodyColumn = c - bs;
                t.rawColumn = c;
                t.measure = measure;

                {
                    // Closed by a bar line on this line, or by the line's closing bar.
                    const bool closed = measureEnd < be || be < (int) l.size();
                    const int total = slots (measureStart - bs, measureEnd - bs);
                    const int before = slots (measureStart - bs, c - bs);

                    if (closed && total > 0)
                    {
                        int perBeat = 0;
                        if (beatsInt > 0 && total % beatsInt == 0)
                        {
                            const int q = total / beatsInt;
                            if (q == 1 || q == 2 || q == 3 || q == 4 || q == 6 || q == 8 || q == 12 || q == 16)
                                perBeat = q;
                        }
                        t.beat = perBeat > 0 ? (double) before / (double) perBeat
                                             : (double) before / (double) total * beatsPerMeasure;
                        t.beat = juce::jlimit (0.0, juce::jmax (0.0, beatsPerMeasure - 0.0625), t.beat);
                    }
                    else
                    {
                        t.beat = (double) before / 4.0;
                    }
                }

                // Wrappers immediately to the left.
                bool openedAngle = false, openedSquare = false, openedParen = false;
                for (int p = c - 1; p >= measureStart; --p)
                {
                    const auto pc = at (l, p);
                    if (pc == '<')      { t.techniques.push_back (make (Type::naturalHarmonic)); openedAngle = true; }
                    else if (pc == '[') { t.techniques.push_back (make (Type::artificialHarmonic)); openedSquare = true; }
                    else if (pc == '(') { t.techniques.push_back (make (Type::ghostNote)); openedParen = true; }
                    else if (pc == '=' || pc == '_') { t.tie = true; }
                    else break;
                }

                if (dead)
                {
                    t.dead = true;
                    t.techniques.push_back (make (Type::deadNote));
                    ++c;
                }
                else
                {
                    t.fret = (int) (ch - '0');
                    ++c;

                    if (isDigit (at (l, c)))
                    {
                        // A pair up to 24 is one fret; "35" is far more often
                        // frets 3 and 5 run together than the 35th fret.
                        const int pair = t.fret * 10 + (int) (at (l, c) - '0');
                        if (pair <= 24) { t.fret = pair; ++c; }
                        else if (d != nullptr) ++d->splitFrets;
                    }
                }

                t.fret = juce::jlimit (0, kMaxFretValue, t.fret);

                // What the previous note's connector promised this one.
                switch (pending.kind)
                {
                    case Pending::hammer:  t.techniques.push_back (make (Type::hammerOn)); break;
                    case Pending::pull:    t.techniques.push_back (make (Type::pullOff)); break;
                    case Pending::legato:
                        t.techniques.push_back (make (previous != nullptr && t.fret < previous->fret ? Type::pullOff : Type::hammerOn));
                        break;
                    case Pending::tap:     t.techniques.push_back (make (Type::tap)); break;
                    case Pending::slap:    t.techniques.push_back (make (Type::slap)); break;
                    case Pending::pop:     t.techniques.push_back (make (Type::pop)); break;
                    case Pending::slideIn: t.techniques.push_back (make (Type::slideIn)); break;   // from below (compiler default)
                    case Pending::slideInFromAbove:
                        t.techniques.push_back (make (Type::slideIn, (double) juce::jmin (kMaxFretValue, t.fret + 3)));
                        break;
                    case Pending::none:    break;
                }
                pending = {};

                // ---- suffix glyphs -----------------------------------------------------
                while (c < be)
                {
                    const auto s = at (l, c);
                    const auto next = at (l, c + 1);

                    if (s == '|' || s == ',' || isFill (s) || isDigit (s) || s == 'x' || s == 'X' || s == ':')
                        break;

                    if (s == '>' && openedAngle)  { openedAngle = false;  ++c; continue; }
                    if (s == ']' && openedSquare) { openedSquare = false; ++c; continue; }
                    if (s == ')' && openedParen)  { openedParen = false;  ++c; continue; }
                    if (s == ']' || s == ')' || s == '=' || s == '_') { ++c; continue; }

                    const bool digitFollows = isDigit (next);

                    // Two-letter glyphs first.
                    if ((s == 'p' || s == 'P') && (next == 'b' || next == 'B'))
                    {
                        c += 2;
                        const double semis = readBendAmount (l, c, be, t.fret);
                        double release = semis;
                        if (at (l, c) == 'r') { ++c; release = 0.0; if (isDigit (at (l, c))) readBendAmount (l, c, be, t.fret); }
                        t.techniques.push_back (make (Type::preBend, semis, release));
                        continue;
                    }
                    if ((s == 'P' || s == 'p') && (next == 'M' || next == 'm' || next == '.'))
                    {
                        c += 2; while (at (l, c) == '.' || at (l, c) == 'M') ++c;
                        t.techniques.push_back (make (Type::palmMute));
                        continue;
                    }
                    if ((s == 't' || s == 'T') && next == 'r')
                    {
                        c += 2;
                        const int target = peekDigits (l, c, be);
                        if (target >= 0) { while (isDigit (at (l, c))) ++c; }
                        // The trill's other fret, as the compiler reads it.
                        t.techniques.push_back (make (Type::trill, (double) (target >= 0 ? target : juce::jmin (kMaxFretValue, t.fret + 2))));
                        continue;
                    }
                    if (s == 'L' && next == 'R') { c += 2; t.techniques.push_back (make (Type::letRing)); continue; }
                    if ((s == 'p' || s == 'P') && next == 'h') { c += 2; t.techniques.push_back (make (Type::pinchHarmonic)); continue; }
                    if (s == 'a' && next == 'h') { c += 2; t.techniques.push_back (make (Type::artificialHarmonic)); continue; }
                    if (s == 'n' && next == 'h') { c += 2; t.techniques.push_back (make (Type::naturalHarmonic)); continue; }
                    if (s == 't' && next == 'h') { c += 2; t.techniques.push_back (make (Type::tapHarmonic)); continue; }

                    switch ((int) s)
                    {
                        case 'b': case 'B':
                        {
                            ++c;
                            const double semis = readBendAmount (l, c, be, t.fret);
                            if (at (l, c) == 'r')
                            {
                                ++c;
                                double release = 0.0;
                                if (isDigit (at (l, c)) || at (l, c) == '(')
                                {
                                    // "7b9r8": a partial release lands above the fret.
                                    const int target = peekDigits (l, c, be);
                                    readBendAmount (l, c, be, t.fret);
                                    if (target > t.fret && target - t.fret < semis)
                                        release = (double) (target - t.fret);
                                }
                                t.techniques.push_back (make (Type::bendRelease, semis, release));
                            }
                            else
                                t.techniques.push_back (make (Type::bend, semis));
                            continue;
                        }
                        case 'r': case 'R':
                        {
                            ++c;
                            if (isDigit (at (l, c))) readBendAmount (l, c, be, t.fret);
                            if (! hasType (t.techniques, Type::bendRelease))
                                t.techniques.push_back (make (Type::bendRelease, 2.0, 0.0));
                            continue;
                        }
                        case '^':
                        {
                            ++c;
                            if (digitFollows) pending.kind = Pending::legato;
                            else t.techniques.push_back (make (Type::bend, readBendAmount (l, c, be, t.fret)));
                            continue;
                        }
                        case 'h': case 'H':
                            ++c;
                            if (digitFollows || next == '(' || next == '<' || next == '[') pending.kind = Pending::hammer;
                            else t.techniques.push_back (make (Type::hammerOn));
                            continue;
                        case 'p':
                            ++c;
                            if (digitFollows || next == '(' || next == '<' || next == '[') pending.kind = Pending::pull;
                            else t.techniques.push_back (make (Type::pullOff));
                            continue;
                        case 'P':
                            ++c;
                            t.techniques.push_back (make (Type::pop));
                            continue;
                        case 'S':
                            ++c;
                            t.techniques.push_back (make (Type::slap));
                            continue;
                        case 't': case 'T':
                            ++c;
                            if (digitFollows || next == '(' || next == '<' || next == '[') pending.kind = Pending::tap;
                            else t.techniques.push_back (make (Type::tap));
                            continue;
                        case '/': case '\\': case 's':
                        {
                            ++c;
                            /*  "5/7", "7\5", "5s7": a legato slide from this note
                                to the next, which is what the compiler plays as an
                                unpicked arrival at the target fret (riff-library
                                5.1.3). A slide off the end of a note ("9\") is a
                                slideOut towards a fret five below (or above). */
                            const int target = peekDigits (l, c, be);
                            if (target >= 0)
                                t.techniques.push_back (make (Type::slideLegato, (double) target));
                            else
                                t.techniques.push_back (make (Type::slideOut,
                                                              (double) juce::jlimit (0, kMaxFretValue, t.fret + (s == '\\' ? -5 : 5))));
                            continue;
                        }
                        case '~': case 'v': case 'V':
                            while (c < be && (at (l, c) == '~' || at (l, c) == 'v' || at (l, c) == 'V')) ++c;
                            if (! hasType (t.techniques, Type::vibrato))
                                t.techniques.push_back (make (Type::vibrato));   // the compiler's default rate and depth
                            continue;
                        case '*':
                            ++c;
                            if (! hasType (t.techniques, Type::naturalHarmonic))
                                t.techniques.push_back (make (Type::naturalHarmonic));
                            continue;
                        case 'w': ++c; t.techniques.push_back (make (Type::whammy, -2.0)); continue;
                        case '>': ++c; t.techniques.push_back (make (Type::accent));   continue;
                        case '.': ++c; t.techniques.push_back (make (Type::staccato)); continue;
                        case '<': case '[': case '(':
                            // The next note's wrapper: leave it for the note.
                            goto endSuffix;
                        default:
                            ++c;
                            if (d != nullptr) ++d->ignoredGlyphs;
                            continue;
                    }
                }
            endSuffix:

                // Annotations above the staff: PM spans and let-ring.
                for (const auto* a : annotationsAbove)
                {
                    for (const auto& span : a->muteSpans)
                        if (t.rawColumn >= span.first && t.rawColumn < span.second
                             && ! hasType (t.techniques, Type::palmMute))
                            t.techniques.push_back (make (Type::palmMute));

                    if (a->letRing && ! hasType (t.techniques, Type::letRing))
                        t.techniques.push_back (make (Type::letRing));
                }

                sys.tokens.push_back (t);
                previous = &sys.tokens.back();
            }

            sys.measuresOnLines = juce::jmax (sys.measuresOnLines, measure + 1);
            sys.repeatCount = juce::jmax (sys.repeatCount, li.repeatCount);
        }

        if (sys.repeatEnd == -2)
            sys.repeatEnd = sys.measuresOnLines - 1;

        // A rhythm/count line above the staff places the notes in time.
        if (rhythm != nullptr && rhythm->size() >= 3 && ! sys.tokens.empty())
        {
            const auto& anchors = *rhythm;
            bool covered = true;
            for (const auto& t : sys.tokens)
                if (t.rawColumn < anchors.front().column - 1 || t.rawColumn > anchors.back().column + 2)
                    covered = false;

            if (covered)
            {
                for (auto& t : sys.tokens)
                {
                    double beat = anchors.back().beat;

                    // A symbol written within one column of the fret is that fret's.
                    int nearest = -1, nearestDistance = 2;
                    for (size_t k = 0; k < anchors.size(); ++k)
                    {
                        const int dist = std::abs (anchors[k].column - t.rawColumn);
                        if (dist < nearestDistance)
                            nearest = (int) k, nearestDistance = dist;
                    }

                    if (nearest >= 0)
                        beat = anchors[(size_t) nearest].beat;
                    else
                        for (size_t k = 0; k + 1 < anchors.size(); ++k)
                            if (t.rawColumn < anchors[k + 1].column)
                            {
                                const double span = (double) (anchors[k + 1].column - anchors[k].column);
                                const double f = span > 0.0 ? juce::jlimit (0.0, 1.0, (double) (t.rawColumn - anchors[k].column) / span) : 0.0;
                                beat = anchors[k].beat + f * (anchors[k + 1].beat - anchors[k].beat);
                                break;
                            }

                    const int m = juce::jmax (0, (int) std::floor (beat / beatsPerMeasure + 1.0e-6));
                    t.measure = m;
                    t.beat = juce::jlimit (0.0, juce::jmax (0.0, beatsPerMeasure - 0.0625),
                                           beat - (double) m * beatsPerMeasure);
                }

                sys.rhythmApplied = true;
            }
        }

        // Durations: to the next note on the same string, within reason.
        std::vector<int> order (sys.tokens.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = (int) i;
        std::sort (order.begin(), order.end(), [&] (int a, int b)
        {
            const auto& ta = sys.tokens[(size_t) a];
            const auto& tb = sys.tokens[(size_t) b];
            if (ta.line != tb.line) return ta.line < tb.line;
            if (ta.measure != tb.measure) return ta.measure < tb.measure;
            return ta.beat < tb.beat;
        });

        for (size_t k = 0; k < order.size(); ++k)
        {
            auto& t = sys.tokens[(size_t) order[k]];
            const double abs = t.measure * beatsPerMeasure + t.beat;
            double duration;

            if (k + 1 < order.size() && sys.tokens[(size_t) order[k + 1]].line == t.line)
            {
                const auto& n = sys.tokens[(size_t) order[k + 1]];
                duration = (n.measure * beatsPerMeasure + n.beat) - abs;
            }
            else
                duration = (t.measure + 1) * beatsPerMeasure - abs;

            t.duration = t.dead ? 0.25 : juce::jlimit (0.25, 4.0, duration);
        }

        // Ties: a tied token extends the previous note on its string instead of sounding.
        for (size_t k = 1; k < order.size(); ++k)
        {
            auto& t = sys.tokens[(size_t) order[k]];
            auto& p = sys.tokens[(size_t) order[k - 1]];
            if (t.tie && p.line == t.line && ! p.tie)
            {
                const double tAbs = t.measure * beatsPerMeasure + t.beat;
                const double pAbs = p.measure * beatsPerMeasure + p.beat;
                p.duration = juce::jlimit (0.25, 8.0, (tAbs - pAbs) + t.duration);
            }
        }
    }
}

//==============================================================================
bool AsciiTabReader::parseTuningNames (const juce::String& text, std::vector<int>& midiHighFirst,
                                       bool lowToHigh)
{
    midiHighFirst.clear();

    auto tokens = tokeniseNames (text);
    if (tokens.size() < 3 || tokens.size() > (size_t) kMaxStrings)
        return false;

    // Which way does the list read? Explicit octaves settle it; otherwise the
    // orientation with the smaller overall span is the one a guitarist wrote.
    bool highFirst = ! lowToHigh;

    if (tokens.front().octave >= 0 && tokens.back().octave >= 0)
    {
        highFirst = (tokens.front().octave * 12 + tokens.front().pitchClass)
                      > (tokens.back().octave * 12 + tokens.back().pitchClass);
    }
    else if (lowToHigh)
    {
        auto reversed = tokens;
        std::reverse (reversed.begin(), reversed.end());
        const auto a = assignOctaves (reversed);   // list read low-to-high
        const auto b = assignOctaves (tokens);     // list read high-to-low
        highFirst = spanOf (b) < spanOf (a);

        // "e B G D A E": a lowercase first name and an uppercase last one is
        // the high-to-low convention written out.
        if (tokens.front().lower && ! tokens.back().lower && tokens.front().pitchClass == tokens.back().pitchClass)
            highFirst = true;
    }

    if (! highFirst)
        std::reverse (tokens.begin(), tokens.end());

    midiHighFirst = assignOctaves (tokens);
    return ! midiHighFirst.empty();
}

//==============================================================================
bool AsciiTabReader::read (const juce::String& text, PerformanceScore& destination,
                           TabImportDiagnostics* diagnostics)
{
    lastError.clear();
    destination.clear();

    TabImportDiagnostics local;
    TabImportDiagnostics* d = diagnostics != nullptr ? diagnostics : &local;
    *d = {};

    // fromLines handles \n, \r\n and lone \r.
    const auto rawLines = juce::StringArray::fromLines (text);

    // ---- classify every line -------------------------------------------------------
    std::vector<LineInfo> lines;
    lines.reserve ((size_t) rawLines.size());

    Header header;

    for (int i = 0; i < rawLines.size(); ++i)
    {
        auto info = classify (rawLines[i]);

        if (info.kind != LineKind::empty)
            ++d->totalLines;

        if (info.kind == LineKind::other || info.kind == LineKind::chords)
        {
            if (readHeader (rawLines[i], header, d))
            {
                info.kind = LineKind::header;
                ++d->headerLines;
            }
        }

        lines.push_back (std::move (info));
    }

    // ---- systems: runs of consecutive staff lines ----------------------------------
    struct Block { int first, count; };
    std::vector<Block> blocks;

    for (int i = 0; i < (int) lines.size();)
    {
        if (lines[(size_t) i].kind != LineKind::staff) { ++i; continue; }
        int j = i;
        while (j < (int) lines.size() && lines[(size_t) j].kind == LineKind::staff) ++j;
        blocks.push_back ({ i, j - i });
        i = j;
    }

    // A bare note list before the first staff, with as many names as the
    // first system has strings, is the tuning ("E A D G B e").
    const int firstStaff = blocks.empty() ? (int) lines.size() : blocks.front().first;
    const int firstCount = blocks.empty() ? 0 : blocks.front().count;

    if (! header.tuningSet)
    {
        for (int i = 0; i < firstStaff; ++i)
        {
            auto& li = lines[(size_t) i];
            if (li.kind != LineKind::chords && li.kind != LineKind::other) continue;

            std::vector<int> midi;
            const auto trimmed = rawLines[i].trim();
            if (trimmed.length() <= 40 && parseTuningNames (trimmed, midi, true)
                 && (int) midi.size() == firstCount && firstCount >= 4)
            {
                header.tuning = midi;
                header.tuningName = trimmed;
                header.tuningSet = true;
                li.kind = LineKind::header;
                ++d->headerLines;
                break;
            }
        }
    }

    // ---- the string names in front of the first full system --------------------------
    if (! header.tuningSet && ! blocks.empty())
    {
        for (const auto& b : blocks)
        {
            if (b.count < 4) continue;

            std::vector<NameToken> names;
            bool all = true;
            for (int k = 0; k < b.count; ++k)
            {
                const auto& li = lines[(size_t) (b.first + k)];
                if (li.namePitchClass < 0) { all = false; break; }
                NameToken t; t.pitchClass = li.namePitchClass; t.octave = li.nameOctave; t.lower = li.nameLower;
                names.push_back (t);
            }

            if (all)
            {
                header.tuning = assignOctaves (names);
                header.tuningSet = false;
                d->tuningFromStringNames = true;

                // Name it when it is one of the known ones, else spell it out.
                juce::String spelled;
                for (int k = b.count - 1; k >= 0; --k)
                    spelled += PerformanceScore::getNoteName (header.tuning[(size_t) k]).dropLastCharacters (1)
                               .replace ("-", "") + (k > 0 ? " " : "");
                header.tuningName = spelled.retainCharacters ("ABCDEFG# ").trim();
                if (header.tuning == std::vector<int> { 64, 59, 55, 50, 45, 40 }) header.tuningName = "Standard";
                else if (header.tuning == std::vector<int> { 64, 59, 55, 50, 45, 38 }) header.tuningName = "Drop D";
            }
            break;
        }
    }
    else if (header.tuningSet)
        d->tuningFromHeader = true;

    d->tempoFromHeader = header.tempoSet;
    d->timeSignatureFromHeader = header.timeSet;

    // ---- the track -----------------------------------------------------------------
    destination.beginCapture (header.tempo, header.numerator, header.denominator);

    int numStrings = header.tuning.empty() ? 6 : (int) header.tuning.size();
    for (const auto& b : blocks)
        if (b.count >= 2 || (b.count == 1 && blocks.size() == 1))
            numStrings = juce::jmax (numStrings, b.count);
    numStrings = juce::jlimit (1, kMaxStrings, numStrings);

    {
        auto& track = destination.getTrack (0);
        track.tuning.fill (0);

        std::vector<int> tuning = header.tuning.empty() ? std::vector<int> { 64, 59, 55, 50, 45, 40 } : header.tuning;

        // Strings below what the header named continue down in fourths.
        while ((int) tuning.size() < numStrings)
            tuning.push_back (juce::jmax (0, tuning.back() - 5));

        for (int s = 0; s < numStrings; ++s)
            track.tuning[(size_t) s] = juce::jlimit (0, 127, tuning[(size_t) s]);

        track.capoFret = juce::jlimit (0, 24, header.capo);
        track.numStrings = numStrings;
        track.name = numStrings <= 5 && track.tuning[0] <= 50 ? "Bass" : "Guitar";
        destination.getMeta().tuningName = header.tuningName;
        destination.getMeta().key = header.key;   // the page's own Key: header, or empty
    }

    const double beatsPerMeasure = (double) juce::jmax (1, header.numerator) * 4.0
                                     / (double) juce::jmax (1, header.denominator);

    // ---- parse and lay out each system -----------------------------------------------
    int measureNumber = 0;
    int totalNotes = 0;
    const auto& track = destination.getTrack (0);

    for (const auto& b : blocks)
    {
        System sys;
        for (int k = 0; k < b.count; ++k)
            sys.lines.push_back (&lines[(size_t) (b.first + k)]);

        // Annotation rows directly above the block (PM, let ring).
        std::vector<LineInfo*> above;
        for (int i = b.first - 1; i >= 0 && i >= b.first - 3; --i)
        {
            auto& li = lines[(size_t) i];
            if (li.kind == LineKind::annotation) { above.push_back (&li); ++d->annotationLines; }
            else if (li.kind != LineKind::empty) break;
        }

        // A rhythm or count line directly above the staff sets the timing.
        std::vector<RhythmAnchor> rhythm;
        int rhythmLine = -1;
        for (int i = b.first - 1; i >= 0 && i >= b.first - 2; --i)
        {
            if (lines[(size_t) i].kind == LineKind::empty || lines[(size_t) i].kind == LineKind::staff)
                break;

            auto anchors = parseRhythmAnchors (rawLines[i], beatsPerMeasure);
            if (anchors.size() >= 3)
            {
                rhythm = std::move (anchors);
                rhythmLine = i;
                break;
            }
        }

        parseSystem (sys, beatsPerMeasure, d, above, rhythm.empty() ? nullptr : &rhythm);

        if (rhythmLine >= 0 && sys.rhythmApplied)
            lines[(size_t) rhythmLine].kind = LineKind::header;

        // Chord names, strumming patterns and accents above the staff (item 2).
        for (int i = b.first - 1; i >= 0 && i >= b.first - 5; --i)
        {
            auto& li = lines[(size_t) i];
            if (li.kind == LineKind::staff || li.kind == LineKind::header) break;
            if (li.kind == LineKind::empty || i == rhythmLine) continue;

            const auto& raw = rawLines[i];
            if (isStrumLine (raw))
            {
                if (readStrumLine (sys, raw) > 0) { li.kind = LineKind::header; ++d->annotationLines; }
            }
            else if (li.kind == LineKind::chords)
            {
                if (readChordLine (sys, raw) > 0) { li.kind = LineKind::header; ++d->headerLines; }
            }
            else if (isAccentLine (raw))
            {
                int applied = 0;
                for (const auto& mark : columnTokens (raw))
                    for (auto* t : tokensNearColumn (sys, mark.column, 2))
                        if (! hasType (t->techniques, Type::accent)) { t->techniques.push_back (make (Type::accent)); ++applied; }
                if (applied > 0) { li.kind = LineKind::header; ++d->annotationLines; }
            }
        }

        // A repeat count on the line after the block: "x4", "(x3)", "play 3 times".
        for (int i = b.first + b.count; i < (int) lines.size() && i <= b.first + b.count + 1; ++i)
        {
            const auto& li = lines[(size_t) i];
            if (li.kind == LineKind::empty) continue;
            if (li.kind == LineKind::other || li.kind == LineKind::annotation)
            {
                const int n = repeatCountIn (rawLines[i].trim().toLowerCase());
                if (n > 1) { sys.repeatCount = juce::jmax (sys.repeatCount, n); lines[(size_t) i].kind = LineKind::header; }
            }
            break;
        }

        const bool singleEmptyLine = b.count == 1 && sys.tokens.empty();
        if (singleEmptyLine)
        {
            ++d->skippedLines;
            warn (d, "line " + juce::String (b.first + 1) + ": lone staff line without notes skipped");
            continue;
        }

        ++d->systems;
        d->staffLines += b.count;

        if (b.count != numStrings)
            warn (d, "system " + juce::String (d->systems) + " has " + juce::String (b.count)
                       + " strings, read as the top " + juce::String (b.count) + " of " + juce::String (numStrings));

        // Measures the system spans: the bar lines, or the beats of an open measure.
        int measuresInSystem = sys.measuresOnLines;
        for (const auto& t : sys.tokens)
            measuresInSystem = juce::jmax (measuresInSystem, t.measure + 1 + (int) (t.beat / beatsPerMeasure));
        measuresInSystem = juce::jlimit (1, kMaxMeasuresPerSystem, measuresInSystem);

        // The order the measures play in, with repeats unrolled.
        std::vector<int> playOrder;
        {
            int count = sys.repeatCount;
            int from = sys.repeatStart, to = sys.repeatEnd;

            if (to >= 0 && from < 0) from = 0;
            if (from >= 0 && to < 0) to = measuresInSystem - 1;
            if (from >= 0 && count < 2) count = 2;
            if (count >= 2 && from < 0) { from = 0; to = measuresInSystem - 1; }

            from = juce::jlimit (0, measuresInSystem - 1, from);
            to = juce::jlimit (from, measuresInSystem - 1, to);
            count = juce::jlimit (1, kMaxRepeat, count);

            for (int m = 0; m < measuresInSystem; ++m)
            {
                playOrder.push_back (m);
                if (m == to && count > 1)
                {
                    for (int r = 1; r < count && (int) playOrder.size() < kMaxMeasuresPerSystem; ++r)
                        for (int k = from; k <= to; ++k)
                            playOrder.push_back (k);
                }
            }

            if (count > 1)
                ++d->repeatsUnrolled;
        }

        // ---- emit --------------------------------------------------------------------
        for (size_t p = 0; p < playOrder.size(); ++p)
        {
            const int m = playOrder[p];
            const double measureBase = (double) (measureNumber + (int) p) * beatsPerMeasure;

            for (const auto& c : sys.chords)
                if (c.measure == m)
                    destination.addChordSymbol (measureBase + c.beat, c.name);

            for (const auto& t : sys.tokens)
            {
                if (t.measure != m || t.tie)
                    continue;

                const int stringIndex = t.line;
                if (stringIndex >= numStrings || stringIndex >= kMaxStrings)
                    continue;

                const double beat = measureBase + t.beat;
                const int open = track.tuning[(size_t) stringIndex] + track.capoFret;
                const int midi = juce::jlimit (0, 127, open + t.fret);
                const double hz = 440.0 * std::pow (2.0, (midi - 69) / 12.0);

                destination.noteStarted (stringIndex, t.fret, midi, hz, 0.8, beat);

                for (const auto& tech : t.techniques)
                    destination.addTechnique (stringIndex, tech);

                destination.noteEnded (stringIndex, beat + t.duration);
                ++totalNotes;
            }
        }

        measureNumber += (int) playOrder.size();
    }

    // ---- what was left on the floor ------------------------------------------------
    for (size_t i = 0; i < lines.size(); ++i)
    {
        const auto& li = lines[i];
        if (li.kind == LineKind::other)
        {
            ++d->skippedLines;
            warn (d, "line " + juce::String ((int) i + 1) + ": skipped (not tab)");
        }
        else if (li.kind == LineKind::chords)
        {
            ++d->skippedLines;
            warn (d, "line " + juce::String ((int) i + 1) + ": skipped (chord names)");
        }
        else if (li.kind == LineKind::annotation)
        {
            // Counted where a block used it; a stray one is skipped.
            bool used = false;
            for (const auto& b : blocks)
                if ((int) i < b.first && (int) i >= b.first - 3) used = true;
            if (! used) { ++d->skippedLines; ++d->annotationLines; }
        }
    }

    destination.endCapture ((double) juce::jmax (1, measureNumber) * beatsPerMeasure);

    d->measures = measureNumber;
    d->notes = totalNotes;
    d->numStrings = numStrings;

    if (totalNotes == 0)
    {
        lastError = d->summary();
        return false;
    }

    return true;
}

//==============================================================================
bool AsciiTabReader::looksLikeTuningStatement (const juce::String& lower)
{
    for (const auto* k : kTuningKeywords)
        if (lower.contains (k))
            return true;

    // Japanese pages: チューニング.
    if (lower.contains (juce::String (juce::CharPointer_UTF8 ("\xe3\x83\x81\xe3\x83\xa5\xe3\x83\xbc\xe3\x83\x8b\xe3\x83\xb3\xe3\x82\xb0"))))
        return true;

    static const char* const names[] = { "drop d", "drop c", "drop b", "drop a", "dadgad", "open g", "open d", "open e",
                                         "open a", "open c", "standard tuning", "standard (", "normal tuning", "baritone",
                                         "eadgbe", "half-step", "whole-step",
                                         "step down", "steps down", "half step", "whole step", "semitone", "eb standard",
                                         "d standard", "c# standard", "c standard", "b standard" };
    for (const auto* n : names)
        if (lower.contains (n))
            return true;
    return false;
}

bool AsciiTabReader::parseTuningStatement (const juce::String& text, std::vector<int>& midiHighFirst,
                                           juce::String& canonicalName)
{
    midiHighFirst.clear();
    canonicalName.clear();

    const auto line = text.trim();
    if (line.isEmpty() || line.length() > 200)
        return false;
    const auto lower = line.toLowerCase();
    const int offset = stepsDownSemitones (lower);

    std::vector<int> base;
    juce::String name;

    if (const auto* t = matchTuning (lower))
    {
        base.assign (t->notes, t->notes + t->count);
        name = t->name;
    }
    else
    {
        // A note list: after the colon / equals / keyword, or in parentheses.
        juce::StringArray candidates;
        const int colon = line.indexOfAnyOf (":=");
        if (colon >= 0) candidates.add (line.substring (colon + 1));
        const int open = line.indexOfChar ('('), close = line.lastIndexOfChar (')');
        if (open >= 0 && close > open) candidates.add (line.substring (open + 1, close));
        for (const auto* k : kTuningKeywords)
        {
            const int at = lower.indexOf (k);
            if (at >= 0) { candidates.add (line.substring (at + (int) std::strlen (k))); candidates.add (line.substring (0, at)); }
        }
        if (candidates.isEmpty()) candidates.add (line);

        for (auto c : candidates)
        {
            c = c.trim();
            // "E A D G B E (half step down)": the list is before the parenthesis.
            const int paren = c.indexOfChar ('(');
            if (paren > 0) c = c.substring (0, paren).trim();
            while (c.startsWithChar (':') || c.startsWithChar ('=') || c.startsWithChar ('-')) c = c.substring (1).trim();
            if (c.isEmpty() || c.length() > 60) continue;
            // Strip a trailing "..., half step down" clause.
            const int comma = c.indexOfChar (',');
            if (comma > 0 && stepsDownSemitones (c.substring (comma).toLowerCase()) > 0) c = c.substring (0, comma).trim();
            if (parseTuningNames (c, base, true))
            {
                name = c;
                break;
            }
            base.clear();

            // "Tuning: Eb", "Tuning: D": standard, lowered to that top string.
            const auto single = tokeniseNames (c);
            if (single.size() == 1 && single.front().octave < 0 && c.length() <= 3)
            {
                const int shift = ((4 - single.front().pitchClass) % 12 + 12) % 12;
                if (shift <= 6)
                {
                    base = { 64, 59, 55, 50, 45, 40 };
                    for (auto& m : base) m -= shift;
                    static const char* const lowered[] = { "Standard", "Eb Standard", "D Standard", "C# Standard",
                                                           "C Standard", "B Standard (6-string)", "Bb Standard" };
                    name = lowered[shift];
                    break;
                }
            }
        }

        if (base.empty() && offset > 0)
        {
            base = { 64, 59, 55, 50, 45, 40 };     // "tuned down a half step" alone means standard, lowered
            name = "Standard";
        }
    }

    if (base.empty())
        return false;

    if (offset > 0)
    {
        for (auto& m : base)
            m = juce::jlimit (0, 127, m - offset);

        if (name == "Standard")
        {
            static const char* const lowered[] = { "Standard", "Eb Standard", "D Standard", "C# Standard", "C Standard" };
            name = offset <= 4 ? juce::String (lowered[offset])
                               : juce::String ("Standard, ") + juce::String (offset) + " semitones down";
        }
        else
        {
            name << ", " << (offset == 1 ? juce::String ("half step down")
                           : offset == 2 ? juce::String ("whole step down")
                                         : juce::String (offset) + " semitones down");
        }
    }

    midiHighFirst = std::move (base);
    canonicalName = name;
    return true;
}

bool AsciiTabReader::parseKeyStatement (const juce::String& text, int& rootPitchClass, bool& minor)
{
    const auto line = text.trim();
    const auto lower = line.toLowerCase();
    static const char* const keywords[] = { "key of ", "key:", "key =", "key=", "key -", "key ", "tonart", "tonalidad",
                                            "tonalite", "tonalit\xc3\xa9", "tonalit\xc3\xa0", "in the key" };
    int at = -1, len = 0;
    for (const auto* k : keywords)
    {
        const juce::String kw { juce::CharPointer_UTF8 (k) };
        const int i = lower.indexOf (kw);
        if (i >= 0 && (i == 0 || ! juce::CharacterFunctions::isLetter (lower[i - 1]))) { at = i; len = kw.length(); break; }
    }
    if (at < 0 || line.length() > 120)
        return false;

    auto rest = line.substring (at + len).trim();
    while (rest.startsWithChar (':') || rest.startsWithChar ('=') || rest.startsWithChar ('-')) rest = rest.substring (1).trim();
    if (rest.startsWith ("of ")) rest = rest.substring (3).trim();
    if (rest.isEmpty())
        return false;

    // Solfège roots (French / Spanish / Italian pages).
    static const struct { const char* name; int pc; } solfege[] = {
        { "do#", 1 }, { "reb", 1 }, { "re#", 3 }, { "mib", 3 }, { "fa#", 6 }, { "solb", 6 }, { "sol#", 8 }, { "lab", 8 },
        { "la#", 10 }, { "sib", 10 }, { "do", 0 }, { "re", 2 }, { "mi", 4 }, { "fa", 5 }, { "sol", 7 }, { "la", 9 }, { "si", 11 } };
    const auto restLower = rest.toLowerCase();
    int root = -1, consumed = 0;
    const int pc = pitchClassOfLetter (rest[0]);
    if (pc >= 0 && (rest.length() == 1 || ! juce::CharacterFunctions::isLetter (rest[1]) || rest[1] == 'b' || rest[1] == 'm' || rest[1] == 'M' || rest[1] == 'd'))
    {
        root = pc; consumed = 1;
        if (rest.length() > 1 && (rest[1] == '#' || rest[1] == (juce::juce_wchar) 0x266f)) { root = (root + 1) % 12; consumed = 2; }
        else if (rest.length() > 1 && (rest[1] == 'b' || rest[1] == (juce::juce_wchar) 0x266d)) { root = (root + 11) % 12; consumed = 2; }
    }
    else
    {
        for (const auto& sf : solfege)
            if (restLower.startsWith (sf.name)) { root = sf.pc; consumed = (int) std::strlen (sf.name); break; }
    }
    if (root < 0)
        return false;

    const auto quality = restLower.substring (consumed).trim();
    minor = quality.startsWith ("m") && ! quality.startsWith ("maj") && ! quality.startsWith ("major");
    if (quality.startsWith ("-") || quality.startsWith ("moll") || quality.startsWith ("minor") || quality.startsWith ("menor") || quality.startsWith ("mineur"))
        minor = true;
    if (quality.startsWith ("dur") || quality.startsWith ("mayor") || quality.startsWith ("majeur"))
        minor = false;

    rootPitchClass = root;
    return true;
}

juce::String AsciiTabReader::keyName (int rootPitchClass, bool minor)
{
    static const char* const names[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    static const char* const minorNames[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
    const int pc = ((rootPitchClass % 12) + 12) % 12;
    return juce::String (minor ? minorNames[pc] : names[pc]) + (minor ? "m" : "");
}

} // namespace luthier
