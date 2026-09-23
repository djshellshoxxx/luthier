#include "TuneHarmony.h"

#include <algorithm>
#include <cmath>

namespace luthier
{

using namespace tunetheory;

//==============================================================================
namespace
{
    bool has (uint16_t mask, int interval) noexcept
    {
        return (mask & (uint16_t) (1u << wrapPitchClass (interval))) != 0;
    }

    /** A quality the template table does not know still sounds as something:
        a major triad, rather than silence. */
    uint16_t qualityMaskOrMajor (const ChordCell& cell) noexcept
    {
        const auto mask = getQualityMask (cell.quality);
        return mask != 0 ? mask : (uint16_t) ((1u << 0) | (1u << 4) | (1u << 7));
    }

    bool isDominant (const ChordCell& cell) noexcept
    {
        const auto mask = qualityMaskOrMajor (cell);
        return has (mask, 4) && has (mask, 10);
    }

    bool hasMajorThird (const ChordCell& cell) noexcept  { return has (qualityMaskOrMajor (cell), 4); }
    bool hasMinorThirdOnly (const ChordCell& c) noexcept { const auto m = qualityMaskOrMajor (c); return has (m, 3) && ! has (m, 4); }
    bool hasSeventh (const ChordCell& cell) noexcept
    {
        const auto m = qualityMaskOrMajor (cell);
        return has (m, 10) || has (m, 11) || (has (m, 3) && has (m, 6) && has (m, 9));
    }

    /** The quality a stack of thirds names. */
    juce::String qualityFromStack (int third, int fifth, int seventh, bool withSeventh)
    {
        if (withSeventh)
        {
            if (third == 4 && fifth == 7 && seventh == 11) return "maj7";
            if (third == 4 && fifth == 7 && seventh == 10) return "7";
            if (third == 3 && fifth == 7 && seventh == 10) return "m7";
            if (third == 3 && fifth == 6 && seventh == 10) return "m7b5";
            if (third == 3 && fifth == 6 && seventh == 9)  return "dim7";
            if (third == 3 && fifth == 7 && seventh == 11) return "mMaj7";
            if (third == 4 && fifth == 8 && seventh == 11) return "maj7#5";
            if (third == 4 && fifth == 8 && seventh == 10) return "7#5";
        }

        if (third == 3 && fifth == 7) return "m";
        if (third == 3 && fifth == 6) return "dim";
        if (third == 4 && fifth == 8) return "aug";
        return "";
    }

    juce::String formatMultiplier (double m)
    {
        if (std::abs (m - std::round (m)) < 1.0e-9)
            return juce::String ((int) std::round (m));

        return juce::String (m, 9).trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    }

    bool sameChord (const ChordCell& a, const ChordCell& b) noexcept
    {
        return a.root == b.root && a.quality == b.quality;
    }

    /** Keeps the per-cell settings a substitution should not throw away. */
    ChordCell carrySettings (ChordCell replacement, const ChordCell& original)
    {
        replacement.strumOverride = original.strumOverride;
        replacement.emphasis = original.emphasis;
        return replacement;
    }
}

//==============================================================================
std::vector<ChordSpan> resolveChordSpans (const TuneSection& section, double beatsPerBar)
{
    std::vector<ChordSpan> spans;

    const double bar = beatsPerBar > 0.0 ? beatsPerBar : 4.0;
    const double total = canonical ((double) juce::jmax (0, section.lengthBars) * bar);
    const int numCells = (int) section.chords.size();

    if (numCells == 0 || total <= 0.0)
        return spans;

    const bool lastHolds = section.chords.back().holdsToFill();

    double position = 0.0;
    int cell = 0;

    // Bounded, so a pathological file (thousands of tiny cells in a long
    // section) cannot make this spin.
    for (int guard = 0; position < total - kBeatResolution && guard < 100000; ++guard)
    {
        const auto& c = section.chords[(size_t) cell];
        const bool isLast = cell == numCells - 1;

        double end = total;

        if (! (isLast && lastHolds))
        {
            const double duration = c.durationBeats > 0.0 ? c.durationBeats : bar;
            end = canonical (juce::jmin (total, position + juce::jmax (kBeatResolution, duration)));
        }

        spans.push_back ({ cell, position, end });
        position = end;
        cell = (cell + 1) % numCells;
    }

    return spans;
}

int findChordSpanAt (const std::vector<ChordSpan>& spans, double beat) noexcept
{
    for (size_t i = 0; i < spans.size(); ++i)
        if (beat >= spans[i].startBeat - kBeatResolution * 0.5 && beat < spans[i].endBeat - kBeatResolution * 0.5)
            return (int) i;

    return -1;
}

//==============================================================================
std::vector<int> getChordToneIntervals (const ChordCell& cell)
{
    const auto mask = qualityMaskOrMajor (cell);

    std::vector<int> tones;
    uint16_t used = 0;

    auto take = [&] (int interval)
    {
        tones.push_back (interval);
        used = (uint16_t) (used | (uint16_t) (1u << wrapPitchClass (interval)));
    };

    auto firstPresent = [&] (std::initializer_list<int> candidates)
    {
        for (int c : candidates)
            if (has (mask, c) && ! has (used, c))
                return c;

        return -1;
    };

    take (0);

    // Third, or the suspension that replaces it.
    if (const int third = firstPresent ({ 4, 3, 5, 2 }); third >= 0)
        take (third);

    if (const int fifth = firstPresent ({ 7, 6, 8 }); fifth >= 0)
        take (fifth);

    // Seventh. A diminished seventh's is the bb7 (9), and a sixth chord's sixth
    // takes the seventh's place, which is the same interval either way.
    if (const int seventh = firstPresent ({ 10, 11 }); seventh >= 0)
        take (seventh);
    else if (has (mask, 9) && ! has (used, 9))
        take (9);

    // Tensions go above, in rising order.
    const int top = tones.back();

    for (int interval = 1; interval < 12; ++interval)
    {
        if (has (mask, interval) && ! has (used, interval))
        {
            int placed = interval;

            while (placed <= top)
                placed += 12;

            take (placed);
        }
    }

    for (const auto& e : cell.extensions)
    {
        const int semitones = getExtensionSemitones (e);

        if (semitones < 0 || has (used, semitones))
            continue;

        int placed = semitones;

        while (placed <= tones.back())
            placed += 12;

        take (placed);
    }

    std::sort (tones.begin(), tones.end());
    return tones;
}

std::vector<int> getChordTones (const ChordCell& cell)
{
    std::vector<int> pcs;

    for (int interval : getChordToneIntervals (cell))
        pcs.push_back (wrapPitchClass (cell.root + interval));

    return pcs;
}

uint16_t getChordPitchClassMask (const ChordCell& cell)
{
    uint16_t mask = 0;

    for (int pc : getChordTones (cell))
        mask = (uint16_t) (mask | (uint16_t) (1u << pc));

    if (cell.bass >= 0)
        mask = (uint16_t) (mask | (uint16_t) (1u << wrapPitchClass (cell.bass)));

    return mask;
}

std::vector<int> voiceChord (const ChordCell& cell, int lowestNote, int maxNotes)
{
    std::vector<int> notes;

    if (maxNotes <= 0)
        return notes;

    const int bassPc = cell.bass >= 0 ? wrapPitchClass (cell.bass) : wrapPitchClass (cell.root);
    const int first = lowestNote + wrapPitchClass (bassPc - lowestNote);

    if (first > 127)
        return notes;

    notes.push_back (first);
    uint16_t used = (uint16_t) (1u << bassPc);

    int previous = first;

    for (int interval : getChordToneIntervals (cell))
    {
        if ((int) notes.size() >= maxNotes)
            break;

        const int pc = wrapPitchClass (cell.root + interval);

        if ((used & (uint16_t) (1u << pc)) != 0)
            continue;

        const int pitch = previous + 1 + wrapPitchClass (pc - (previous + 1));

        if (pitch > 127)
            break;

        notes.push_back (pitch);
        used = (uint16_t) (used | (uint16_t) (1u << pc));
        previous = pitch;
    }

    return notes;
}

juce::String getChordSymbol (const ChordCell& cell, bool preferFlats)
{
    auto symbol = spellPitchClass (cell.root, preferFlats) + cell.quality;

    if (! cell.extensions.isEmpty())
        symbol << "(" << cell.extensions.joinIntoString (",") << ")";

    if (cell.bass >= 0 && cell.bass != cell.root)
        symbol << "/" << spellPitchClass (cell.bass, preferFlats);

    return symbol;
}

ChordCell makeTonicChord (int tonic, TuneMode mode)
{
    return makeDiatonicChord (tonic, mode, 1, false, 4.0);
}

//==============================================================================
const char* getProgressionErrorName (ProgressionError error) noexcept
{
    switch (error)
    {
        case ProgressionError::none:                return "None";
        case ProgressionError::unknownRoot:         return "UnknownRoot";
        case ProgressionError::unknownQuality:      return "UnknownQuality";
        case ProgressionError::badSlashBass:        return "BadSlashBass";
        case ProgressionError::badDuration:         return "BadDuration";
        case ProgressionError::unclosedSection:     return "UnclosedSection";
        case ProgressionError::emptySectionName:    return "EmptySectionName";
        case ProgressionError::unexpectedCharacter: return "UnexpectedCharacter";
        case ProgressionError::emptyBar:            return "EmptyBar";
        case ProgressionError::tooManyChords:       return "TooManyChords";
    }

    return "Unknown";
}

juce::String ProgressionParseResult::describe() const
{
    if (ok())
        return {};

    return juce::String (getProgressionErrorName (error)) + " at " + juce::String (position)
             + ": '" + token + "'";
}

//==============================================================================
bool parseChordSymbol (const juce::String& text, ChordCell& result, ProgressionError& error)
{
    const auto t = text.trim();

    int used = 0;
    const int root = parsePitchClass (t, 0, used);

    if (root < 0)
    {
        error = ProgressionError::unknownRoot;
        return false;
    }

    auto rest = t.substring (used);
    int bass = -1;

    // The last slash names the bass, unless what follows it is not a note:
    // "6/9" is a quality, not a slash chord.
    const int slash = rest.lastIndexOfChar ('/');

    if (slash >= 0)
    {
        const auto after = rest.substring (slash + 1);
        int bassUsed = 0;
        const int bassPc = parsePitchClass (after, 0, bassUsed);

        if (bassPc >= 0 && bassUsed == after.length())
        {
            bass = bassPc;
            rest = rest.substring (0, slash);
        }
        else if (after.isEmpty() || juce::CharacterFunctions::isLetter (after[0]))
        {
            error = ProgressionError::badSlashBass;
            return false;
        }
    }

    juce::String quality;
    const int qualityLength = matchQualityPrefix (rest, 0, quality);

    if (qualityLength < 0)
    {
        error = ProgressionError::unknownQuality;
        return false;
    }

    juce::StringArray extensions;
    int pos = qualityLength;

    while (pos < rest.length())
    {
        const auto c = rest[pos];

        if (c == '(' || c == ')' || c == ',' || c == ' ')
        {
            ++pos;
            continue;
        }

        const int start = pos;

        if (rest.substring (pos).startsWith ("add"))
            pos += 3;

        if (pos < rest.length() && (rest[pos] == 'b' || rest[pos] == '#'))
            ++pos;

        while (pos < rest.length() && juce::CharacterFunctions::isDigit (rest[pos]))
            ++pos;

        const auto token = rest.substring (start, pos);

        if (token.isEmpty() || getExtensionSemitones (token) < 0)
        {
            error = ProgressionError::unknownQuality;
            return false;
        }

        extensions.add (token);
    }

    ChordCell cell;
    cell.root = root;
    cell.quality = quality;
    cell.bass = (bass == root) ? -1 : bass;
    cell.extensions = extensions;

    result = cell;
    error = ProgressionError::none;
    return true;
}

//==============================================================================
ProgressionParseResult parseProgression (const juce::String& text, double beatsPerBar)
{
    ProgressionParseResult result;

    const double bar = beatsPerBar > 0.0 ? beatsPerBar : 4.0;
    const bool pipeMode = text.containsChar ('|');

    ParsedSection current;
    int pipesInSection = 0;
    bool barHasContent = false;
    int totalCells = 0;

    struct Token { juce::String text; int position; };
    std::vector<Token> pendingBar;

    juce::String token;
    int tokenStart = -1;
    int parenDepth = 0;

    auto fail = [&result] (ProgressionError e, const juce::String& what, int where)
    {
        if (result.ok())
        {
            result.error = e;
            result.token = what;
            result.position = where;
        }

        return false;
    };

    // Turns one bar's tokens into cells.
    auto flushBar = [&]() -> bool
    {
        if (pendingBar.empty())
            return true;

        const double share = pipeMode ? bar / (double) pendingBar.size() : bar;

        for (const auto& t : pendingBar)
        {
            if (t.text.startsWithChar ('*'))
                return fail (ProgressionError::unexpectedCharacter, t.text, t.position);

            auto symbol = t.text;
            double multiplier = 1.0;
            bool hold = false;

            const int star = t.text.indexOfChar ('*');

            if (star >= 0)
            {
                symbol = t.text.substring (0, star);
                const auto m = t.text.substring (star + 1);

                if (m == "fill")
                {
                    hold = true;
                }
                else
                {
                    const bool numeric = m.isNotEmpty() && m.containsOnly ("0123456789.")
                                           && m.indexOfChar ('.') == m.lastIndexOfChar ('.')
                                           && m != ".";

                    multiplier = numeric ? m.getDoubleValue() : 0.0;

                    if (! numeric || multiplier <= 0.0 || multiplier > 64.0)
                        return fail (ProgressionError::badDuration, t.text, t.position);
                }
            }

            ChordCell cell;
            ProgressionError chordError = ProgressionError::none;

            if (! parseChordSymbol (symbol, cell, chordError))
                return fail (chordError, t.text, t.position);

            cell.durationBeats = hold ? 0.0 : canonical (share * multiplier);

            if (++totalCells > kMaxProgressionChords)
                return fail (ProgressionError::tooManyChords, t.text, t.position);

            current.cells.push_back (cell);
        }

        pendingBar.clear();
        return true;
    };

    auto finishSection = [&]()
    {
        if (current.cells.empty() && current.name.isEmpty())
            return;

        double beats = 0.0;

        for (const auto& c : current.cells)
            beats += c.holdsToFill() ? bar : c.durationBeats;

        current.bars = current.cells.empty() ? 0 : juce::jmax (1, (int) std::ceil (beats / bar - 1.0e-9));
        result.sections.push_back (current);
        current = ParsedSection();
    };

    // Ends the token being read. Outside pipes every chord is its own bar.
    auto flushToken = [&]() -> bool
    {
        if (token.isEmpty())
            return true;

        pendingBar.push_back ({ token, tokenStart });
        barHasContent = true;
        token.clear();
        tokenStart = -1;

        return pipeMode ? true : flushBar();
    };

    const int length = text.length();

    for (int i = 0; i < length; ++i)
    {
        const auto c = text[i];

        if (parenDepth > 0)
        {
            if (c == ')')
                --parenDepth;

            token += juce::String::charToString (c);
            continue;
        }

        if (c == '[')
        {
            if (! flushToken() || ! flushBar())
                return result;

            const int close = text.indexOf (i, "]");

            if (close < 0)
            {
                fail (ProgressionError::unclosedSection, text.substring (i), i);
                return result;
            }

            const auto name = text.substring (i + 1, close).trim();

            if (name.isEmpty())
            {
                fail (ProgressionError::emptySectionName, text.substring (i, close + 1), i);
                return result;
            }

            finishSection();
            current.name = name;
            pipesInSection = 0;
            barHasContent = false;
            i = close;
            continue;
        }

        if (c == ']')
        {
            fail (ProgressionError::unexpectedCharacter, "]", i);
            return result;
        }

        if (c == '|')
        {
            if (! flushToken())
                return result;

            // A pipe with nothing since the previous pipe is an empty bar. The
            // first pipe of a section is a leading bar line, not an empty bar.
            if (pipesInSection > 0 && ! barHasContent)
            {
                fail (ProgressionError::emptyBar, "|", i);
                return result;
            }

            if (! flushBar())
                return result;

            ++pipesInSection;
            barHasContent = false;
            continue;
        }

        if (juce::CharacterFunctions::isWhitespace (c) || c == ',')
        {
            if (! flushToken())
                return result;

            continue;
        }

        if (c == '(')
            ++parenDepth;

        if (token.isEmpty())
            tokenStart = i;

        token += juce::String::charToString (c);
    }

    if (! flushToken() || ! flushBar())
        return result;

    finishSection();
    return result;
}

//==============================================================================
juce::String formatProgression (const std::vector<ChordCell>& cells, double beatsPerBar, bool preferFlats)
{
    const double bar = beatsPerBar > 0.0 ? beatsPerBar : 4.0;
    juce::StringArray tokens;

    for (const auto& cell : cells)
    {
        auto t = getChordSymbol (cell, preferFlats);

        if (cell.holdsToFill())
            t << "*fill";
        else if (std::abs (cell.durationBeats - bar) > kBeatResolution)
            t << "*" << formatMultiplier (cell.durationBeats / bar);

        tokens.add (t);
    }

    return tokens.joinIntoString (" ");
}

ProgressionParseResult applyProgressionText (Tune& tune, int activeSection, const juce::String& text)
{
    auto result = parseProgression (text, tune.getBeatsPerBar());

    if (! result.ok())
        return result;

    // Somewhere for an unnamed progression to go. Named sections find or
    // create their own, so only an unnamed one needs a section made for it.
    if (! tune.isValidSection (activeSection))
    {
        const bool unnamed = ! result.sections.empty() && result.sections.front().name.isEmpty();

        if (unnamed && tune.getNumSections() == 0)
        {
            TuneSection fresh;
            fresh.name = "Verse";
            activeSection = tune.addSection (fresh);
        }
        else
        {
            activeSection = tune.getNumSections() > 0 ? 0 : -1;
        }
    }

    const auto* active = tune.getSection (activeSection);
    const auto activeName = active != nullptr ? active->name : juce::String();
    const auto defaultPattern = active != nullptr ? active->rhythmPatternId : juce::String();
    const auto defaultKit = active != nullptr ? active->genreKitId : juce::String();

    for (const auto& parsed : result.sections)
    {
        const auto name = parsed.name.isEmpty() ? activeName : parsed.name;
        int index = tune.findSection (name);

        if (index < 0)
        {
            TuneSection fresh;
            fresh.name = name;
            fresh.lengthBars = juce::jmax (1, parsed.bars);
            fresh.rhythmPatternId = defaultPattern;
            fresh.genreKitId = defaultKit;
            index = tune.addSection (fresh);

            if (index < 0)
                continue;

            if (! tune.arrangement.setlist.empty())
            {
                TuneSetlistEntry entry;
                entry.section = tune.arrangement.sections[(size_t) index].name;
                tune.arrangement.setlist.push_back (entry);
            }
        }

        tune.setChords (index, parsed.cells);

        if (parsed.bars > tune.arrangement.sections[(size_t) index].lengthBars)
            tune.setSectionLength (index, parsed.bars);
    }

    return result;
}

//==============================================================================
ChordCell makeDiatonicChord (int tonic, TuneMode mode, int degree, bool seventh, double beats)
{
    const auto scale = getScalePitchClasses (tonic, mode);
    const int d = juce::jlimit (0, 6, degree - 1);
    const int root = scale[(size_t) d];

    const int third = wrapPitchClass (scale[(size_t) ((d + 2) % 7)] - root);
    const int fifth = wrapPitchClass (scale[(size_t) ((d + 4) % 7)] - root);
    const int sev   = wrapPitchClass (scale[(size_t) ((d + 6) % 7)] - root);

    return ChordCell::make (root, qualityFromStack (third, fifth, sev, seventh), beats);
}

int getDiatonicDegree (const ChordCell& cell, int tonic, TuneMode mode)
{
    const int degree = getScaleDegree (cell.root, tonic, mode);

    if (degree < 0)
        return 0;

    const auto mask = qualityMaskOrMajor (cell);

    for (std::initializer_list<int> group : { std::initializer_list<int> { 4, 3 },
                                              std::initializer_list<int> { 7, 6, 8 },
                                              std::initializer_list<int> { 10, 11 } })
    {
        for (int interval : group)
        {
            if (has (mask, interval))
            {
                if (! isInScale (cell.root + interval, tonic, mode))
                    return 0;

                break;
            }
        }
    }

    return degree + 1;
}

juce::String getRomanNumeral (const ChordCell& cell, int tonic, TuneMode mode)
{
    const int degree = getDiatonicDegree (cell, tonic, mode);

    if (degree == 0)
        return {};

    static const char* const numerals[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    juce::String numeral (numerals[degree - 1]);

    const auto mask = qualityMaskOrMajor (cell);
    const bool minor = has (mask, 3) && ! has (mask, 4);

    if (minor)
        numeral = numeral.toLowerCase();

    if (minor && has (mask, 6) && ! has (mask, 7))
        numeral << (has (mask, 10) ? "m7b5" : (has (mask, 9) ? "o7" : "o"));
    else if (has (mask, 8) && ! has (mask, 7) && ! minor)
        numeral << "+";

    if (has (mask, 11))
        numeral << "maj7";
    else if (has (mask, 10) && ! (minor && has (mask, 6) && ! has (mask, 7)))
        numeral << "7";

    return numeral;
}

//==============================================================================
std::vector<ChordCell> suggestNextChords (const ChordCell* last, int tonic, TuneMode mode,
                                          const juce::String& genreKit, double beats)
{
    const auto genre = genreKit.toLowerCase();
    const bool blues = genre.contains ("blues");
    const bool jazz = genre.contains ("jazz") || genre.contains ("bossa") || genre.contains ("gypsy")
                        || genre.contains ("samba");

    const auto scale = getScalePitchClasses (tonic, mode);

    auto chordFor = [&] (int degree)
    {
        if (blues && (degree == 1 || degree == 4 || degree == 5))
            return ChordCell::make (scale[(size_t) (degree - 1)], "7", beats);

        return makeDiatonicChord (tonic, mode, degree, jazz, beats);
    };

    // Common moves from each degree; row 0 is "from nothing": I, IV, V.
    static const int nextDegrees[8][3] =
    {
        { 1, 4, 5 },
        { 4, 5, 6 },   // I
        { 5, 7, 4 },   // ii
        { 6, 4, 2 },   // iii
        { 5, 1, 2 },   // IV
        { 1, 6, 4 },   // V
        { 2, 4, 5 },   // vi
        { 1, 3, 6 }    // vii
    };

    std::vector<ChordCell> candidates;

    if (last != nullptr && getDiatonicDegree (*last, tonic, mode) == 0)
    {
        // Out of the key: resolve it like a dominant, down a fifth, then offer
        // the way home.
        const int target = wrapPitchClass (last->root + 5);
        const int degree = getScaleDegree (target, tonic, mode);

        candidates.push_back (degree >= 0 ? chordFor (degree + 1) : ChordCell::make (target, "", beats));
        candidates.push_back (chordFor (1));
        candidates.push_back (chordFor (4));
        candidates.push_back (chordFor (5));
    }
    else
    {
        const int degree = last != nullptr ? getDiatonicDegree (*last, tonic, mode) : 0;

        for (int d : nextDegrees[degree])
            candidates.push_back (chordFor (d));
    }

    // Top up from the whole key in case of duplicates, so there are always three.
    for (int d = 1; d <= 7; ++d)
        candidates.push_back (chordFor (d));

    std::vector<ChordCell> result;

    for (const auto& c : candidates)
    {
        if ((int) result.size() >= 3)
            break;

        const bool duplicate = std::any_of (result.begin(), result.end(),
                                            [&c] (const ChordCell& r) { return sameChord (r, c); });
        const bool repeatsLast = last != nullptr && sameChord (*last, c);

        if (! duplicate && ! repeatsLast)
            result.push_back (c);
    }

    return result;
}

std::vector<ChordSubstitution> suggestSubstitutions (const std::vector<ChordCell>& cells, int index,
                                                     int tonic, TuneMode mode)
{
    std::vector<ChordSubstitution> offers;

    if (! juce::isPositiveAndBelow (index, (int) cells.size()))
        return offers;

    const auto& cell = cells[(size_t) index];
    const bool hasNext = cells.size() > 1;
    const auto& next = cells[(size_t) ((index + 1) % (int) cells.size())];

    if (isDominant (cell))
    {
        auto sub = cell;
        sub.root = wrapPitchClass (cell.root + 6);
        sub.bass = -1;
        offers.push_back ({ "Tritone substitution", { sub } });
    }

    if (hasNext && ! cell.holdsToFill() && cell.durationBeats >= 1.0)
    {
        const double half = canonical (cell.durationBeats * 0.5);

        auto ii = carrySettings (ChordCell::make (next.root + 2, "m7", half), cell);
        auto v  = carrySettings (ChordCell::make (next.root + 7, "7", canonical (cell.durationBeats - half)), cell);
        offers.push_back ({ "ii-V into next chord", { ii, v } });

        const auto dominant = carrySettings (ChordCell::make (next.root + 7, "7", cell.durationBeats), cell);

        if (! sameChord (dominant, cell))
            offers.push_back ({ "Secondary dominant", { dominant } });
    }

    const double duration = cell.durationBeats;

    if (hasMajorThird (cell) && ! isDominant (cell))
        offers.push_back ({ "Relative minor",
                            { carrySettings (ChordCell::make (cell.root + 9, hasSeventh (cell) ? "m7" : "m", duration), cell) } });
    else if (hasMinorThirdOnly (cell))
        offers.push_back ({ "Relative major",
                            { carrySettings (ChordCell::make (cell.root + 3, hasSeventh (cell) ? "maj7" : "", duration), cell) } });

    // Borrowed from the parallel mode: the same degree in minor (or major).
    const int degree = getDiatonicDegree (cell, tonic, mode);

    if (degree > 0)
    {
        const auto parallel = modeHasMinorThird (mode) ? TuneMode::ionian : TuneMode::aeolian;
        auto borrowed = carrySettings (makeDiatonicChord (tonic, parallel, degree, hasSeventh (cell), duration), cell);

        if (! sameChord (borrowed, cell))
            offers.push_back ({ "Modal interchange", { borrowed } });
    }

    return offers;
}

std::vector<ChordCell> reharmonize (const std::vector<ChordCell>& cells, int tonic, TuneMode mode,
                                    const ReharmonizeOptions& options)
{
    std::vector<ChordCell> out = cells;
    const int size = (int) out.size();

    // ---- modal interchange ------------------------------------------------------
    if (options.modalInterchange)
    {
        for (int i = 0; i < size; ++i)
        {
            auto& c = out[(size_t) i];
            const int degree = getDiatonicDegree (c, tonic, mode);

            if (! modeHasMinorThird (mode))
            {
                // The minor iv: IV going home to I.
                const bool toTonic = i + 1 < size && getDiatonicDegree (out[(size_t) i + 1], tonic, mode) == 1;

                if (degree == 4 && hasMajorThird (c) && toTonic)
                    c.quality = hasSeventh (c) ? "m7" : "m";
            }
            else if (degree == 5 && hasMinorThirdOnly (c))
            {
                // The harmonic-minor dominant.
                c.quality = "7";
            }
        }
    }

    // ---- secondary dominants ----------------------------------------------------------
    std::vector<bool> inserted ((size_t) size, false);

    if (options.secondaryDominants && size > 1)
    {
        std::vector<ChordCell> withDominants;
        std::vector<bool> flags;

        for (int i = 0; i < size; ++i)
        {
            const auto& c = out[(size_t) i];

            if (i + 1 < size)
            {
                const auto& target = out[(size_t) i + 1];
                const int targetDegree = getDiatonicDegree (target, tonic, mode);
                const int dominantRoot = wrapPitchClass (target.root + 7);

                if (targetDegree >= 2 && ! c.holdsToFill() && c.durationBeats >= 2.0
                      && c.root != dominantRoot)
                {
                    const double half = canonical (c.durationBeats * 0.5);

                    auto first = c;
                    first.durationBeats = half;

                    auto dominant = carrySettings (ChordCell::make (dominantRoot, "7",
                                                                    canonical (c.durationBeats - half)), c);

                    withDominants.push_back (first);
                    flags.push_back (false);
                    withDominants.push_back (dominant);
                    flags.push_back (true);
                    continue;
                }
            }

            withDominants.push_back (c);
            flags.push_back (false);
        }

        out = std::move (withDominants);
        inserted = std::move (flags);
    }

    // ---- tritone substitutions ------------------------------------------------------------
    if (options.tritoneSubstitutions)
    {
        for (size_t i = 0; i + 1 < out.size(); ++i)
        {
            auto& c = out[i];

            // Only the progression's own dominants: substituting the ones just
            // inserted would undo the secondary-dominant pass.
            if (inserted[i] || ! isDominant (c))
                continue;

            if (wrapPitchClass (out[i + 1].root - c.root) == 5)
            {
                c.root = wrapPitchClass (c.root + 6);
                c.bass = -1;
            }
        }
    }

    return out;
}

bool reharmonizeSection (Tune& tune, int sectionIndex, const ReharmonizeOptions& options)
{
    const auto* section = tune.getSection (sectionIndex);

    if (section == nullptr)
        return false;

    return tune.setChords (sectionIndex, reharmonize (section->chords, tune.meta.keyTonic,
                                                      tune.meta.mode, options));
}

//==============================================================================
namespace
{
    void shiftNotes (std::vector<MelodyNote>& notes, int semitones)
    {
        for (auto& n : notes)
            if (n.pitch.isAbsolute())
                n.pitch.value = juce::jlimit (0, 127, n.pitch.value + semitones);
    }

    template <typename Fn>
    void forEachArrangement (Tune& tune, Fn&& fn)
    {
        fn (tune.arrangement);

        for (auto& v : tune.variations)
            fn (v.arrangement);
    }
}

bool transposeTune (Tune& tune, int semitones)
{
    if (semitones == 0)
        return false;

    forEachArrangement (tune, [semitones] (TuneArrangement& a)
    {
        for (auto& s : a.sections)
        {
            for (auto& c : s.chords)
            {
                c.root = wrapPitchClass (c.root + semitones);

                if (c.bass >= 0)
                    c.bass = wrapPitchClass (c.bass + semitones);
            }

            if (s.melody.has_value())
                shiftNotes (s.melody->notes, semitones);

            shiftNotes (s.bass.notes, semitones);

            for (auto& layer : s.layers)
                shiftNotes (layer.notes, semitones);
        }
    });

    tune.meta.keyTonic = wrapPitchClass (tune.meta.keyTonic + semitones);
    return true;
}

bool shiftMode (Tune& tune, TuneMode newMode, bool followMode)
{
    const auto oldMode = tune.meta.mode;
    const int tonic = tune.meta.keyTonic;

    if (newMode == oldMode)
        return false;

    const auto oldScale = getScalePitchClasses (tonic, oldMode);
    const auto newScale = getScalePitchClasses (tonic, newMode);

    auto moveToNewMode = [&] (int midiNote)
    {
        const int degree = getScaleDegree (midiNote, tonic, oldMode);

        if (degree < 0)
            return midiNote;

        int delta = wrapPitchClass (newScale[(size_t) degree] - oldScale[(size_t) degree]);

        if (delta > 6)
            delta -= 12;

        return juce::jlimit (0, 127, midiNote + delta);
    };

    auto followNotes = [&] (std::vector<MelodyNote>& notes)
    {
        for (auto& n : notes)
            if (n.pitch.isAbsolute())
                n.pitch.value = moveToNewMode (n.pitch.value);
    };

    forEachArrangement (tune, [&] (TuneArrangement& a)
    {
        for (auto& s : a.sections)
        {
            for (auto& c : s.chords)
            {
                const int degree = getDiatonicDegree (c, tonic, oldMode);

                if (degree == 0)
                    continue;

                const auto replacement = makeDiatonicChord (tonic, newMode, degree, hasSeventh (c), c.durationBeats);
                c.root = replacement.root;
                c.quality = replacement.quality;

                if (c.bass >= 0)
                    c.bass = wrapPitchClass (moveToNewMode (c.bass + 60));
            }

            if (followMode)
            {
                if (s.melody.has_value())
                    followNotes (s.melody->notes);

                followNotes (s.bass.notes);

                for (auto& layer : s.layers)
                    followNotes (layer.notes);
            }
        }
    });

    tune.meta.mode = newMode;
    return true;
}

} // namespace luthier
