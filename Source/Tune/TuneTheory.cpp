#include "TuneTheory.h"

#include <cmath>

namespace luthier
{

//==============================================================================
namespace
{
    const char* const kModeNames[] =
        { "ionian", "dorian", "phrygian", "lydian", "mixolydian", "aeolian", "locrian" };

    const std::array<std::array<int, 7>, 7> kModeIntervals
    { {
        { { 0, 2, 4, 5, 7, 9, 11 } },   // ionian
        { { 0, 2, 3, 5, 7, 9, 10 } },   // dorian
        { { 0, 1, 3, 5, 7, 8, 10 } },   // phrygian
        { { 0, 2, 4, 6, 7, 9, 11 } },   // lydian
        { { 0, 2, 4, 5, 7, 9, 10 } },   // mixolydian
        { { 0, 2, 3, 5, 7, 8, 10 } },   // aeolian
        { { 0, 1, 3, 5, 6, 8, 10 } }    // locrian
    } };

    /** Where each mode's tonic sits in its relative major, in semitones. */
    const int kModeOffsetInIonian[7] = { 0, 2, 4, 5, 7, 9, 11 };

    int modeIndex (TuneMode mode) noexcept
    {
        return juce::jlimit (0, 6, (int) mode);
    }

    /** Relative major tonic of a key, which is what its signature is named after. */
    int relativeIonian (int tonic, TuneMode mode) noexcept
    {
        return tunetheory::wrapPitchClass (tonic - kModeOffsetInIonian[modeIndex (mode)]);
    }

    //==========================================================================
    /*  Spellings musicians type that are not template suffixes themselves. Each
        maps onto a suffix ChordDetector defines; getQualityMask() returning 0
        for a target would mean the template table lost that chord, which the
        tests check. */
    struct QualityAlias
    {
        const char* alias;
        const char* canonical;
    };

    const QualityAlias kQualityAliases[] =
    {
        { "maj",      ""      }, { "major",   ""      }, { "M",     ""      },
        { "min",      "m"     }, { "minor",   "m"     }, { "-",     "m"     },
        { "M7",       "maj7"  }, { "Maj7",    "maj7"  }, { "ma7",   "maj7"  },
        { "min7",     "m7"    }, { "-7",      "m7"    },
        { "M9",       "maj9"  }, { "min9",    "m9"    }, { "-9",    "m9"    },
        { "min6",     "m6"    }, { "-6",      "m6"    },
        { "mM7",      "mMaj7" }, { "mmaj7",   "mMaj7" },
        { "o",        "dim"   }, { "o7",      "dim7"  },
        { "+",        "aug"   }, { "+7",      "7#5"   },
        { "sus",      "sus4"  }, { "dom7",    "7"     }
    };

    struct ExtensionSpec
    {
        const char* name;
        int semitones;
    };

    /*  Extensions sit an octave up where a musician would voice them (a ninth is
        14, not 2), so a voicing built from them spreads the way the symbol
        reads. The altered fifths and the sixth stay inside the octave: they
        replace or sit beside chord tones rather than above them. */
    const ExtensionSpec kExtensions[] =
    {
        { "b5",    6 }, { "#5",    8 }, { "6",     9 },
        { "b9",   13 }, { "9",    14 }, { "#9",   15 },
        { "11",   17 }, { "#11",  18 },
        { "b13",  20 }, { "13",   21 },
        { "add2",  2 }, { "add4",  5 },
        { "add9", 14 }, { "add11", 17 }, { "add13", 21 }
    };
}

//==============================================================================
const char* getTuneModeName (TuneMode mode) noexcept
{
    return kModeNames[modeIndex (mode)];
}

bool parseTuneMode (const juce::String& text, TuneMode& result)
{
    const auto t = text.trim();

    if (t.equalsIgnoreCase ("major")) { result = TuneMode::ionian;  return true; }
    if (t.equalsIgnoreCase ("minor")) { result = TuneMode::aeolian; return true; }

    for (int i = 0; i < 7; ++i)
    {
        if (t.equalsIgnoreCase (kModeNames[i]))
        {
            result = (TuneMode) i;
            return true;
        }
    }

    return false;
}

const std::array<int, 7>& getModeIntervals (TuneMode mode) noexcept
{
    return kModeIntervals[(size_t) modeIndex (mode)];
}

bool modeHasMinorThird (TuneMode mode) noexcept
{
    return getModeIntervals (mode)[2] == 3;
}

//==============================================================================
namespace tunetheory
{

double canonical (double value) noexcept
{
    if (! std::isfinite (value))
        return 0.0;

    // A quotient of an integer by an exact power of ten is the correctly
    // rounded double for "n millionths", which is the same double a JSON reader
    // produces from the six-decimal text. Multiplying by kBeatResolution
    // instead would not be.
    double snapped = std::round (value * 1.0e6) / 1.0e6;

    if (snapped == 0.0)
        snapped = 0.0;   // folds -0 into +0, so it never prints as "-0.0"

    return snapped;
}

int wrapPitchClass (int pitch) noexcept
{
    return ((pitch % 12) + 12) % 12;
}

//==============================================================================
int parsePitchClass (const juce::String& text, int start, int& charsUsed)
{
    charsUsed = 0;

    if (start < 0 || start >= text.length())
        return -1;

    static const int naturals[7] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G

    const auto letter = juce::CharacterFunctions::toUpperCase (text[start]);

    if (letter < 'A' || letter > 'G')
        return -1;

    int pitchClass = naturals[(int) (letter - 'A')];
    charsUsed = 1;

    if (start + 1 < text.length())
    {
        const auto accidental = text[start + 1];

        if (accidental == '#')      { pitchClass += 1;  charsUsed = 2; }
        else if (accidental == 'b') { pitchClass += 11; charsUsed = 2; }
    }

    return wrapPitchClass (pitchClass);
}

juce::String spellPitchClass (int pitchClass, bool preferFlats)
{
    static const char* const sharps[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* const flats[12]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

    const int pc = wrapPitchClass (pitchClass);
    return preferFlats ? flats[pc] : sharps[pc];
}

bool keyPrefersFlats (int tonic, TuneMode mode) noexcept
{
    // F, Bb, Eb, Ab and Db majors. Gb/F# is written with sharps: six of either,
    // and sharps is what the chord detector's own names use.
    switch (relativeIonian (tonic, mode))
    {
        case 1: case 3: case 5: case 8: case 10: return true;
        default: return false;
    }
}

int keySignatureAccidentals (int tonic, TuneMode mode) noexcept
{
    static const int byMajorTonic[12] = { 0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5 };
    return byMajorTonic[relativeIonian (tonic, mode)];
}

juce::String formatKey (int tonic, TuneMode mode)
{
    return spellPitchClass (tonic, keyPrefersFlats (tonic, mode))
             + (modeHasMinorThird (mode) ? "m" : "");
}

bool parseKey (const juce::String& text, int& tonic, TuneMode& mode)
{
    const auto t = text.trim();

    int used = 0;
    const int pc = parsePitchClass (t, 0, used);

    if (pc < 0)
        return false;

    const auto rest = t.substring (used).trim();

    TuneMode parsedMode = TuneMode::ionian;

    if (rest.isEmpty() || rest == "M" || rest.equalsIgnoreCase ("maj"))
        parsedMode = TuneMode::ionian;
    else if (rest == "m" || rest.equalsIgnoreCase ("min"))
        parsedMode = TuneMode::aeolian;
    else if (! parseTuneMode (rest, parsedMode))
        return false;

    tonic = pc;
    mode = parsedMode;
    return true;
}

//==============================================================================
std::array<int, 7> getScalePitchClasses (int tonic, TuneMode mode) noexcept
{
    std::array<int, 7> result {};
    const auto& intervals = getModeIntervals (mode);

    for (size_t i = 0; i < 7; ++i)
        result[i] = wrapPitchClass (tonic + intervals[i]);

    return result;
}

uint16_t getScaleMask (int tonic, TuneMode mode) noexcept
{
    uint16_t mask = 0;

    for (int pc : getScalePitchClasses (tonic, mode))
        mask = (uint16_t) (mask | (uint16_t) (1u << pc));

    return mask;
}

uint16_t getPentatonicMask (int tonic, TuneMode mode) noexcept
{
    // Major pentatonic 1 2 3 5 6; minor pentatonic 1 b3 4 5 b7.
    static const int major[5] = { 0, 2, 4, 7, 9 };
    static const int minor[5] = { 0, 3, 5, 7, 10 };

    const int* intervals = modeHasMinorThird (mode) ? minor : major;
    uint16_t mask = 0;

    for (int i = 0; i < 5; ++i)
        mask = (uint16_t) (mask | (uint16_t) (1u << wrapPitchClass (tonic + intervals[i])));

    return mask;
}

uint16_t getBluesMask (int tonic) noexcept
{
    static const int blues[6] = { 0, 3, 5, 6, 7, 10 };
    uint16_t mask = 0;

    for (int i = 0; i < 6; ++i)
        mask = (uint16_t) (mask | (uint16_t) (1u << wrapPitchClass (tonic + blues[i])));

    return mask;
}

bool isInMask (int midiNote, uint16_t mask) noexcept
{
    return (mask & (uint16_t) (1u << wrapPitchClass (midiNote))) != 0;
}

bool isInScale (int midiNote, int tonic, TuneMode mode) noexcept
{
    return isInMask (midiNote, getScaleMask (tonic, mode));
}

int getScaleDegree (int midiNote, int tonic, TuneMode mode) noexcept
{
    const auto pcs = getScalePitchClasses (tonic, mode);
    const int pc = wrapPitchClass (midiNote);

    for (int i = 0; i < 7; ++i)
        if (pcs[(size_t) i] == pc)
            return i;

    return -1;
}

int snapToMask (int midiNote, uint16_t mask) noexcept
{
    if (mask == 0 || isInMask (midiNote, mask))
        return juce::jlimit (0, 127, midiNote);

    for (int distance = 1; distance <= 6; ++distance)
    {
        if (isInMask (midiNote - distance, mask) && midiNote - distance >= 0)
            return midiNote - distance;

        if (isInMask (midiNote + distance, mask) && midiNote + distance <= 127)
            return midiNote + distance;
    }

    return juce::jlimit (0, 127, midiNote);
}

int snapToScale (int midiNote, int tonic, TuneMode mode) noexcept
{
    return snapToMask (midiNote, getScaleMask (tonic, mode));
}

int moveInMask (int midiNote, int steps, uint16_t mask) noexcept
{
    if (mask == 0)
        return juce::jlimit (0, 127, midiNote + steps);

    int pitch = midiNote;
    const int direction = steps >= 0 ? 1 : -1;

    for (int s = 0; s < std::abs (steps); ++s)
    {
        int next = pitch;

        for (int i = 0; i < 12; ++i)
        {
            next += direction;

            if (isInMask (next, mask))
                break;
        }

        if (next < 0 || next > 127)
            break;

        pitch = next;
    }

    return juce::jlimit (0, 127, pitch);
}

int moveByScaleSteps (int midiNote, int steps, int tonic, TuneMode mode) noexcept
{
    return moveInMask (midiNote, steps, getScaleMask (tonic, mode));
}

//==============================================================================
bool isKnownQuality (const juce::String& canonicalSuffix) noexcept
{
    for (int i = 0; i < getNumChordTemplates(); ++i)
        if (canonicalSuffix == getChordTemplate (i).suffix)
            return true;

    return false;
}

uint16_t getQualityMask (const juce::String& canonicalSuffix) noexcept
{
    for (int i = 0; i < getNumChordTemplates(); ++i)
    {
        const auto& t = getChordTemplate (i);

        if (canonicalSuffix == t.suffix)
            return t.intervalMask;
    }

    return 0;
}

bool resolveQuality (const juce::String& nameOrAlias, juce::String& canonicalSuffix)
{
    if (isKnownQuality (nameOrAlias))
    {
        canonicalSuffix = nameOrAlias;
        return true;
    }

    for (const auto& alias : kQualityAliases)
    {
        if (nameOrAlias == alias.alias && isKnownQuality (alias.canonical))
        {
            canonicalSuffix = alias.canonical;
            return true;
        }
    }

    return false;
}

int matchQualityPrefix (const juce::String& text, int start, juce::String& canonicalSuffix)
{
    const auto rest = text.substring (start);

    int bestLength = -1;
    juce::String best;

    for (int i = 0; i < getNumChordTemplates(); ++i)
    {
        const juce::String suffix (getChordTemplate (i).suffix);

        if (suffix.length() > bestLength && rest.startsWith (suffix))
        {
            bestLength = suffix.length();
            best = suffix;
        }
    }

    for (const auto& alias : kQualityAliases)
    {
        const juce::String name (alias.alias);

        if (name.length() > bestLength && rest.startsWith (name) && isKnownQuality (alias.canonical))
        {
            bestLength = name.length();
            best = alias.canonical;
        }
    }

    if (bestLength < 0)
        return -1;

    canonicalSuffix = best;
    return bestLength;
}

juce::String qualityToFileName (const juce::String& canonicalSuffix)
{
    if (canonicalSuffix.isEmpty())  return "maj";
    if (canonicalSuffix == "m")     return "min";
    return canonicalSuffix;
}

int getExtensionSemitones (const juce::String& extension) noexcept
{
    for (const auto& e : kExtensions)
        if (extension == e.name)
            return e.semitones;

    return -1;
}

} // namespace tunetheory

} // namespace luthier
