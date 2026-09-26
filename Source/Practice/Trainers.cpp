#include "Trainers.h"

#include <algorithm>

namespace luthier
{

namespace
{
    const char* const kPitchClassNames[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    /** The interval names, by semitone count. */
    const char* const kIntervalNames[13] =
    {
        "Unison", "Minor 2nd", "Major 2nd", "Minor 3rd", "Major 3rd", "Perfect 4th",
        "Tritone", "Perfect 5th", "Minor 6th", "Major 6th", "Minor 7th", "Major 7th",
        "Octave"
    };

    struct ChordQuality
    {
        const char* name;
        int intervals[5];
        int count;
    };

    // practice-tools 5 names these eleven.
    const ChordQuality kChordQualities[EarTrainer::kNumChordQualities] =
    {
        { "Major",       { 0, 4, 7,  0,  0 }, 3 },
        { "Minor",       { 0, 3, 7,  0,  0 }, 3 },
        { "Diminished",  { 0, 3, 6,  0,  0 }, 3 },
        { "Augmented",   { 0, 4, 8,  0,  0 }, 3 },
        { "Dominant 7",  { 0, 4, 7, 10,  0 }, 4 },
        { "Major 7",     { 0, 4, 7, 11,  0 }, 4 },
        { "Minor 7",     { 0, 3, 7, 10,  0 }, 4 },
        { "Half-dim 7",  { 0, 3, 6, 10,  0 }, 4 },
        { "Diminished 7",{ 0, 3, 6,  9,  0 }, 4 },
        { "Sus 2",       { 0, 2, 7,  0,  0 }, 3 },
        { "Sus 4",       { 0, 5, 7,  0,  0 }, 3 }
    };

    struct Progression
    {
        const char* name;
        int degrees[12];       ///< Scale degrees, negative for minor chords.
        int count;
    };

    // practice-tools 5: the five named, plus ten more (SPEC-SWEEP PT-38: was 14).
    const Progression kProgressions[EarTrainer::kNumProgressions] =
    {
        { "I-IV-V",            {  1,  4,  5,  0,  0,  0,  0,  0,  0,  0,  0,  0 },  3 },
        { "I-vi-IV-V",         {  1, -6,  4,  5,  0,  0,  0,  0,  0,  0,  0,  0 },  4 },
        { "ii-V-I",            { -2,  5,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0 },  3 },
        { "12-bar blues",      {  1,  1,  1,  1,  4,  4,  1,  1,  5,  4,  1,  5 }, 12 },
        { "Andalusian cadence",{ -6,  5,  4, -3,  0,  0,  0,  0,  0,  0,  0,  0 },  4 },
        { "I-V-vi-IV",         {  1,  5, -6,  4,  0,  0,  0,  0,  0,  0,  0,  0 },  4 },
        { "vi-IV-I-V",         { -6,  4,  1,  5,  0,  0,  0,  0,  0,  0,  0,  0 },  4 },
        { "I-iii-IV-V",        {  1, -3,  4,  5,  0,  0,  0,  0,  0,  0,  0,  0 },  4 },
        { "I-bVII-IV",         {  1,  7,  4,  0,  0,  0,  0,  0,  0,  0,  0,  0 },  3 },
        { "ii-V-I minor",      { -2,  5, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0 },  3 },
        { "I-IV-I-V",          {  1,  4,  1,  5,  0,  0,  0,  0,  0,  0,  0,  0 },  4 },
        { "vi-V-IV-V",         { -6,  5,  4,  5,  0,  0,  0,  0,  0,  0,  0,  0 },  4 },
        { "I-vi-ii-V",         {  1, -6, -2,  5,  0,  0,  0,  0,  0,  0,  0,  0 },  4 },
        { "IV-V-iii-vi",       {  4,  5, -3, -6,  0,  0,  0,  0,  0,  0,  0,  0 },  4 },
        { "I-IV-vi-V",         {  1,  4, -6,  5,  0,  0,  0,  0,  0,  0,  0,  0 },  4 }   // SPEC-SWEEP PT-38
    };

    /** The semitone offset of a major-scale degree from its root. */
    int majorScaleDegreeSemitones (int degree) noexcept
    {
        static const int steps[8] = { 0, 0, 2, 4, 5, 7, 9, 11 };

        return steps[juce::jlimit (1, 7, degree)];
    }
}

//==============================================================================
const char* getScaleTypeName (ScaleType type) noexcept
{
    switch (type)
    {
        case ScaleType::ionian:           return "Ionian (Major)";
        case ScaleType::dorian:           return "Dorian";
        case ScaleType::phrygian:         return "Phrygian";
        case ScaleType::lydian:           return "Lydian";
        case ScaleType::mixolydian:       return "Mixolydian";
        case ScaleType::aeolian:          return "Aeolian (Minor)";
        case ScaleType::locrian:          return "Locrian";
        case ScaleType::harmonicMinor:    return "Harmonic Minor";
        case ScaleType::melodicMinor:     return "Melodic Minor";
        case ScaleType::majorPentatonic:  return "Major Pentatonic";
        case ScaleType::minorPentatonic:  return "Minor Pentatonic";
        case ScaleType::blues:            return "Blues";
        case ScaleType::custom:           return "Custom";
        case ScaleType::numScales:
        default:                          return "Ionian (Major)";
    }
}

int getScaleIntervals (ScaleType type, int* dest, int maxIntervals) noexcept
{
    if (dest == nullptr || maxIntervals <= 0)
        return 0;

    static const int ionian[]          = { 0, 2, 4, 5, 7, 9, 11 };
    static const int dorian[]          = { 0, 2, 3, 5, 7, 9, 10 };
    static const int phrygian[]        = { 0, 1, 3, 5, 7, 8, 10 };
    static const int lydian[]          = { 0, 2, 4, 6, 7, 9, 11 };
    static const int mixolydian[]      = { 0, 2, 4, 5, 7, 9, 10 };
    static const int aeolian[]         = { 0, 2, 3, 5, 7, 8, 10 };
    static const int locrian[]         = { 0, 1, 3, 5, 6, 8, 10 };
    static const int harmonicMinor[]   = { 0, 2, 3, 5, 7, 8, 11 };
    static const int melodicMinor[]    = { 0, 2, 3, 5, 7, 9, 11 };
    static const int majorPentatonic[] = { 0, 2, 4, 7, 9 };
    static const int minorPentatonic[] = { 0, 3, 5, 7, 10 };
    static const int blues[]           = { 0, 3, 5, 6, 7, 10 };

    const int* source = ionian;
    int count = 7;

    switch (type)
    {
        case ScaleType::ionian:          source = ionian;          count = 7; break;
        case ScaleType::dorian:          source = dorian;          count = 7; break;
        case ScaleType::phrygian:        source = phrygian;        count = 7; break;
        case ScaleType::lydian:          source = lydian;          count = 7; break;
        case ScaleType::mixolydian:      source = mixolydian;      count = 7; break;
        case ScaleType::aeolian:         source = aeolian;         count = 7; break;
        case ScaleType::locrian:         source = locrian;         count = 7; break;
        case ScaleType::harmonicMinor:   source = harmonicMinor;   count = 7; break;
        case ScaleType::melodicMinor:    source = melodicMinor;    count = 7; break;
        case ScaleType::majorPentatonic: source = majorPentatonic; count = 5; break;
        case ScaleType::minorPentatonic: source = minorPentatonic; count = 5; break;
        case ScaleType::blues:           source = blues;           count = 6; break;

        case ScaleType::custom:
        case ScaleType::numScales:
        default:
            return 0;
    }

    count = juce::jmin (count, maxIntervals);

    for (int i = 0; i < count; ++i)
        dest[i] = source[i];

    return count;
}

const char* getScaleDegreeName (ScaleType type, int degreeIndex) noexcept
{
    static const char* const heptatonic[7] = { "1", "2", "3", "4", "5", "6", "7" };
    static const char* const pentatonic[5] = { "1", "2", "3", "5", "6" };
    static const char* const minorPent[5]  = { "1", "b3", "4", "5", "b7" };
    static const char* const bluesNames[6] = { "1", "b3", "4", "b5", "5", "b7" };

    switch (type)
    {
        case ScaleType::majorPentatonic:
            return pentatonic[juce::jlimit (0, 4, degreeIndex)];

        case ScaleType::minorPentatonic:
            return minorPent[juce::jlimit (0, 4, degreeIndex)];

        case ScaleType::blues:
            return bluesNames[juce::jlimit (0, 5, degreeIndex)];

        default:
            return heptatonic[juce::jlimit (0, 6, degreeIndex)];
    }
}

//==============================================================================
ScaleTrainer::ScaleTrainer()
{
    rebuild();
}

void ScaleTrainer::setScale (ScaleType type) noexcept
{
    scale = type;
    rebuild();
}

void ScaleTrainer::setCustomIntervals (const int* source, int count) noexcept
{
    numCustomIntervals = juce::jlimit (0, kMaxIntervals, count);

    for (int i = 0; i < numCustomIntervals; ++i)
        customIntervals[(size_t) i] = ((source[i] % 12) + 12) % 12;

    scale = ScaleType::custom;
    rebuild();
}

void ScaleTrainer::rebuild() noexcept
{
    if (scale == ScaleType::custom)
    {
        numIntervals = numCustomIntervals;

        for (int i = 0; i < numIntervals; ++i)
            intervals[(size_t) i] = customIntervals[(size_t) i];
    }
    else
    {
        numIntervals = getScaleIntervals (scale, intervals.data(), kMaxIntervals);
    }
}

bool ScaleTrainer::containsPitchClass (int pitchClass) const noexcept
{
    const int wrapped = ((pitchClass % 12) + 12) % 12;

    for (int i = 0; i < numIntervals; ++i)
        if (((root + intervals[(size_t) i]) % 12) == wrapped)
            return true;

    return false;
}

int ScaleTrainer::getDegreeOf (int pitchClass) const noexcept
{
    const int wrapped = ((pitchClass % 12) + 12) % 12;

    for (int i = 0; i < numIntervals; ++i)
        if (((root + intervals[(size_t) i]) % 12) == wrapped)
            return i + 1;

    return 0;
}

int ScaleTrainer::getPitchClassOfDegree (int degree) const noexcept
{
    if (numIntervals <= 0)
        return -1;

    const int index = juce::jlimit (0, numIntervals - 1, degree - 1);

    return (root + intervals[(size_t) index]) % 12;
}

//==============================================================================
void ScaleTrainer::setNoteRange (int low, int high) noexcept
{
    lowNote = juce::jlimit (0, 127, juce::jmin (low, high));
    highNote = juce::jlimit (0, 127, juce::jmax (low, high));
}

juce::String ScaleTrainer::nextQuestion (juce::Random& random)
{
    // MODEL-GAPS: a session of questionCount questions ends.
    if (isSessionComplete())
    {
        expectedPitchClass = -1;
        question = "Session complete: " + juce::String (correct) + " of " + juce::String (asked) + ".";
        return question;
    }

    if (numIntervals <= 0)
    {
        expectedPitchClass = -1;
        question = "No scale selected.";
        return question;
    }

    const int degree = 1 + random.nextInt (numIntervals);

    expectedPitchClass = getPitchClassOfDegree (degree);

    const juce::String keyName (kPitchClassNames[root]);
    const juce::String scaleName (getScaleTypeName (scale));
    const juce::String degreeName (getScaleDegreeName (scale, degree - 1));

    switch (mode)
    {
        case Mode::quiz:
            question = "Play the " + degreeName + " of " + keyName + " " + scaleName;
            break;

        case Mode::chordToneTrainer:
            // practice-tools 4: the chord-tone trainer asks for the third and
            // the seventh, which are the notes that carry the harmony.
            question = "Play the 3rd and 7th of " + keyName + " " + scaleName;
            expectedPitchClass = getPitchClassOfDegree (3);
            break;

        case Mode::intervalTrainer:
            question = "Name the interval between " + keyName + " and "
                         + juce::String (kPitchClassNames[expectedPitchClass]);
            break;

        case Mode::explore:
        case Mode::numModes:
        default:
            question = keyName + " " + scaleName;
            expectedPitchClass = -1;
            break;
    }

    if (mode != Mode::explore)
        ++asked;

    return question;
}

bool ScaleTrainer::answer (int midiNote)
{
    if (expectedPitchClass < 0)
        return false;

    // MODEL-GAPS: a note outside the set range is not an answer.
    if (midiNote < lowNote || midiNote > highNote)
        return false;

    const bool right = (((midiNote % 12) + 12) % 12) == expectedPitchClass;

    if (right)
        ++correct;

    return right;
}

//==============================================================================
EarTrainer::EarTrainer()
{
    lifetimeAsked.fill (0);
    lifetimeCorrect.fill (0);
}

void EarTrainer::setDifficulty (int level) noexcept
{
    difficulty = juce::jlimit (0, 4, level);
}

void EarTrainer::resetScore() noexcept
{
    correct = 0;
    asked = 0;
    streak = 0;
    rollingRate = 0.5;
}

//==============================================================================
int EarTrainer::buildIntervalQuestion (juce::Random& random, int* notes,
                                       double* beatOffsets, int maxNotes)
{
    if (maxNotes < 2)
        return 0;

    /*  Difficulty decides which intervals are on the table.

        The easy end is the intervals a beginner can already sing - octaves,
        fifths, fourths - and each level adds the ones that are harder to tell
        apart, ending with the tritone and the sevenths. */
    static const int byDifficulty[5][12] =
    {
        { 12,  7,  5,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
        { 12,  7,  5,  4,  3,  0,  0,  0,  0,  0,  0,  0 },
        { 12,  7,  5,  4,  3,  9,  8,  2,  0,  0,  0,  0 },
        { 12,  7,  5,  4,  3,  9,  8,  2,  10, 11, 1,  0 },
        { 12,  7,  5,  4,  3,  9,  8,  2,  10, 11, 1,  6 }
    };

    static const int countsByDifficulty[5] = { 3, 5, 8, 11, 12 };

    const int count = countsByDifficulty[juce::jlimit (0, 4, difficulty)];
    const int semitones = byDifficulty[juce::jlimit (0, 4, difficulty)][random.nextInt (count)];

    presentation = (Presentation) random.nextInt ((int) Presentation::numPresentations);

    const int rootNote = 52 + random.nextInt (12);

    switch (presentation)
    {
        case Presentation::descending:
            notes[0] = rootNote + semitones;
            notes[1] = rootNote;
            beatOffsets[0] = 0.0;
            beatOffsets[1] = 1.0;
            break;

        case Presentation::harmonic:
            notes[0] = rootNote;
            notes[1] = rootNote + semitones;
            beatOffsets[0] = 0.0;
            beatOffsets[1] = 0.0;
            break;

        case Presentation::ascending:
        case Presentation::numPresentations:
        default:
            notes[0] = rootNote;
            notes[1] = rootNote + semitones;
            beatOffsets[0] = 0.0;
            beatOffsets[1] = 1.0;
            break;
    }

    // The choices are every interval the current difficulty admits, so the guess
    // is never narrowed to two by the answer list itself.
    choices.clear();

    juce::Array<int> offered;

    for (int i = 0; i < count; ++i)
        offered.addIfNotAlreadyThere (byDifficulty[juce::jlimit (0, 4, difficulty)][i]);

    offered.sort();

    correctChoice = 0;

    for (int i = 0; i < offered.size(); ++i)
    {
        choices.add (kIntervalNames[juce::jlimit (0, 12, offered[i])]);

        if (offered[i] == semitones)
            correctChoice = i;
    }

    questionText = (presentation == Presentation::harmonic) ? "Name the harmonic interval"
                 : (presentation == Presentation::descending) ? "Name the descending interval"
                 : "Name the ascending interval";

    return 2;
}

int EarTrainer::buildChordQuestion (juce::Random& random, int* notes,
                                    double* beatOffsets, int maxNotes)
{
    // Easier levels offer only the first few qualities, which are the ones that
    // differ most from each other.
    static const int countsByDifficulty[5] = { 2, 4, 6, 9, kNumChordQualities };

    const int available = countsByDifficulty[juce::jlimit (0, 4, difficulty)];
    const int chosen = random.nextInt (available);

    const auto& quality = kChordQualities[chosen];

    const int rootNote = 48 + random.nextInt (12);

    const int count = juce::jmin (maxNotes, quality.count);

    for (int i = 0; i < count; ++i)
    {
        notes[i] = rootNote + quality.intervals[i];

        // Strummed rather than blocked, so it arrives as a guitar chord.
        beatOffsets[i] = (double) i * 0.02;
    }

    choices.clear();

    for (int i = 0; i < available; ++i)
        choices.add (kChordQualities[i].name);

    correctChoice = chosen;
    questionText = "Name the chord quality";

    return count;
}

int EarTrainer::buildProgressionQuestion (juce::Random& random, int* notes,
                                          double* beatOffsets, int maxNotes)
{
    static const int countsByDifficulty[5] = { 3, 5, 8, 11, kNumProgressions };

    const int available = countsByDifficulty[juce::jlimit (0, 4, difficulty)];
    const int chosen = random.nextInt (available);

    const auto& progression = kProgressions[chosen];

    const int keyRoot = 48 + random.nextInt (12);

    int written = 0;

    for (int chord = 0; chord < progression.count && written + 3 <= maxNotes; ++chord)
    {
        const int degree = progression.degrees[chord];
        const bool minor = degree < 0;
        const int absolute = std::abs (degree);

        // A flat-seven chord is a semitone below the octave, not the leading note.
        const int semitones = (absolute == 7 && ! minor)
                                ? 10
                                : majorScaleDegreeSemitones (absolute);

        const int chordRoot = keyRoot + semitones;

        notes[written] = chordRoot;
        beatOffsets[written++] = (double) chord * 2.0;

        notes[written] = chordRoot + (minor ? 3 : 4);
        beatOffsets[written++] = (double) chord * 2.0 + 0.02;

        notes[written] = chordRoot + 7;
        beatOffsets[written++] = (double) chord * 2.0 + 0.04;
    }

    choices.clear();

    for (int i = 0; i < available; ++i)
        choices.add (kProgressions[i].name);

    correctChoice = chosen;
    questionText = "Name the progression";

    return written;
}

void EarTrainer::setNoteRange (int low, int high) noexcept
{
    lowNote = juce::jlimit (0, 127, juce::jmin (low, high));
    highNote = juce::jlimit (0, 127, juce::jmax (low, high));
}

int EarTrainer::nextQuestion (juce::Random& random, int* notes, double* beatOffsets, int maxNotes)
{
    if (notes == nullptr || beatOffsets == nullptr || maxNotes <= 0)
        return 0;

    // MODEL-GAPS: a session of questionCount questions ends.
    if (isSessionComplete())
        return 0;

    ++asked;

    if (juce::isPositiveAndBelow ((int) exercise, (int) Exercise::numExercises))
        ++lifetimeAsked[(size_t) exercise];

    int count = 0;

    switch (exercise)
    {
        case Exercise::chordQuality: count = buildChordQuestion (random, notes, beatOffsets, maxNotes); break;
        case Exercise::progression:  count = buildProgressionQuestion (random, notes, beatOffsets, maxNotes); break;

        case Exercise::interval:
        case Exercise::numExercises:
        default:                     count = buildIntervalQuestion (random, notes, beatOffsets, maxNotes); break;
    }

    // MODEL-GAPS: the whole question moves by octaves into the range, keeping
    // its shape; if it is wider than the range it sits as low as it can.
    if (count > 0)
    {
        int lo = notes[0], hi = notes[0];

        for (int i = 1; i < count; ++i)
        {
            lo = juce::jmin (lo, notes[i]);
            hi = juce::jmax (hi, notes[i]);
        }

        int shift = 0;

        while (lo + shift < lowNote && hi + shift + 12 <= 127)
            shift += 12;

        while (hi + shift > highNote && lo + shift - 12 >= lowNote)
            shift -= 12;

        for (int i = 0; i < count; ++i)
            notes[i] = juce::jlimit (0, 127, notes[i] + shift);
    }

    return count;
}

bool EarTrainer::answer (int choiceIndex)
{
    const bool right = (choiceIndex == correctChoice);

    if (right)
    {
        ++correct;
        ++streak;

        if (juce::isPositiveAndBelow ((int) exercise, (int) Exercise::numExercises))
            ++lifetimeCorrect[(size_t) exercise];
    }
    else
    {
        streak = 0;
    }

    // A twelve-question memory: fast enough to follow a player warming up,
    // slow enough that one unlucky guess does not drop the difficulty.
    rollingRate = rollingRate * (11.0 / 12.0) + (right ? 1.0 : 0.0) / 12.0;

    if (adaptive)
        adapt (right);

    return right;
}

void EarTrainer::adapt (bool) noexcept
{
    /*  practice-tools 5: the difficulty follows the success rate.

        The target is about three quarters right. Below sixty percent the
        questions are too hard to learn from; above eighty-five they are too easy
        to learn from. The gap between the two thresholds stops the level
        oscillating on every answer. */
    if (asked < 6)
        return;

    if (rollingRate > 0.85 && difficulty < 4)
    {
        setDifficulty (difficulty + 1);
        rollingRate = 0.7;
    }
    else if (rollingRate < 0.6 && difficulty > 0)
    {
        setDifficulty (difficulty - 1);
        rollingRate = 0.7;
    }
}

//==============================================================================
juce::var EarTrainer::statsToVar() const
{
    auto* root = new juce::DynamicObject();

    root->setProperty ("difficulty", difficulty);
    root->setProperty ("adaptive", adaptive);
    root->setProperty ("sessionAsked", asked);
    root->setProperty ("sessionCorrect", correct);

    juce::Array<juce::var> askedArray, correctArray;

    for (int i = 0; i < (int) Exercise::numExercises; ++i)
    {
        askedArray.add (lifetimeAsked[(size_t) i]);
        correctArray.add (lifetimeCorrect[(size_t) i]);
    }

    root->setProperty ("lifetimeAsked", askedArray);
    root->setProperty ("lifetimeCorrect", correctArray);

    return { root };
}

void EarTrainer::statsFromVar (const juce::var& state)
{
    auto* root = state.getDynamicObject();

    if (root == nullptr)
        return;

    setDifficulty ((int) root->getProperty ("difficulty"));
    adaptive = root->hasProperty ("adaptive") ? (bool) root->getProperty ("adaptive") : true;

    if (const auto* askedArray = root->getProperty ("lifetimeAsked").getArray())
        for (int i = 0; i < juce::jmin ((int) Exercise::numExercises, askedArray->size()); ++i)
            lifetimeAsked[(size_t) i] = juce::jmax (0, (int) (*askedArray)[i]);

    if (const auto* correctArray = root->getProperty ("lifetimeCorrect").getArray())
        for (int i = 0; i < juce::jmin ((int) Exercise::numExercises, correctArray->size()); ++i)
            lifetimeCorrect[(size_t) i] = juce::jmax (0, (int) (*correctArray)[i]);
}

juce::File EarTrainer::getStatsFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("Practice")
             .getChildFile ("stats.json");
}

bool EarTrainer::saveStats() const
{
    const auto file = getStatsFile();

    file.getParentDirectory().createDirectory();

    return file.replaceWithText (juce::JSON::toString (statsToVar(), false));
}

bool EarTrainer::loadStats()
{
    const auto file = getStatsFile();

    if (! file.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());

    if (parsed.getDynamicObject() == nullptr)
        return false;

    statsFromVar (parsed);
    return true;
}

//==============================================================================
namespace
{
    /** Reads a pitch class off the front of a chord symbol, and how many
        characters it used. Returns -1 if the text does not start with a note. */
    int parseRoot (const juce::String& text, int& charactersUsed)
    {
        charactersUsed = 0;

        if (text.isEmpty())
            return -1;

        static const int naturals[7] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G

        const auto letter = juce::CharacterFunctions::toUpperCase (text[0]);

        if (letter < 'A' || letter > 'G')
            return -1;

        int pitchClass = naturals[letter - 'A'];
        charactersUsed = 1;

        if (text.length() > 1)
        {
            if (text[1] == '#')      { pitchClass = (pitchClass + 1) % 12; charactersUsed = 2; }
            else if (text[1] == 'b') { pitchClass = (pitchClass + 11) % 12; charactersUsed = 2; }
        }

        return pitchClass;
    }
}

bool ProgressionLooper::parse (const juce::String& text)
{
    if (text.isEmpty())
        return false;

    std::vector<Chord> parsed;
    int parsedRepeats = 1;

    // "Am - F - C - G x4": chords separated by dashes, an optional repeat count.
    auto working = text.trim();

    const int repeatMarker = working.lastIndexOfIgnoreCase ("x");

    if (repeatMarker > 0)
    {
        const auto tail = working.substring (repeatMarker + 1).trim();

        if (tail.containsOnly ("0123456789") && tail.isNotEmpty())
        {
            parsedRepeats = juce::jlimit (1, 64, tail.getIntValue());
            working = working.substring (0, repeatMarker).trim();
        }
    }

    auto tokens = juce::StringArray::fromTokens (working, "-|,", "");
    tokens.trim();
    tokens.removeEmptyStrings();

    for (const auto& token : tokens)
    {
        if ((int) parsed.size() >= kMaxChords)
            break;

        Chord chord;
        chord.symbol = token;

        int used = 0;
        chord.rootPitchClass = parseRoot (token, used);

        if (chord.rootPitchClass < 0)
            continue;

        auto suffix = token.substring (used);

        // A slash chord names its bass after the slash.
        const int slash = suffix.indexOfChar ('/');

        if (slash >= 0)
        {
            int bassUsed = 0;
            chord.bassPitchClass = parseRoot (suffix.substring (slash + 1), bassUsed);
            suffix = suffix.substring (0, slash);
        }

        suffix = suffix.trim();

        // The qualities a progression is actually written with. Longest first,
        // so "maj7" is not read as "maj".
        struct Suffix { const char* text; int intervals[5]; int count; };

        static const Suffix kSuffixes[] =
        {
            { "maj7", { 0, 4, 7, 11, 0 }, 4 },
            { "m7b5",{ 0, 3, 6, 10, 0 }, 4 },
            { "dim7",{ 0, 3, 6,  9, 0 }, 4 },
            { "sus2",{ 0, 2, 7,  0, 0 }, 3 },
            { "sus4",{ 0, 5, 7,  0, 0 }, 3 },
            { "add9",{ 0, 4, 7, 14, 0 }, 4 },
            { "min7",{ 0, 3, 7, 10, 0 }, 4 },
            { "maj", { 0, 4, 7,  0, 0 }, 3 },
            { "dim", { 0, 3, 6,  0, 0 }, 3 },
            { "aug", { 0, 4, 8,  0, 0 }, 3 },
            { "m7",  { 0, 3, 7, 10, 0 }, 4 },
            { "m6",  { 0, 3, 7,  9, 0 }, 4 },
            { "m9",  { 0, 3, 7, 10, 14 }, 5 },
            { "min", { 0, 3, 7,  0, 0 }, 3 },
            { "11",  { 0, 4, 7, 10, 17 }, 5 },
            { "13",  { 0, 4, 7, 10, 21 }, 5 },
            { "9",   { 0, 4, 7, 10, 14 }, 5 },
            { "7",   { 0, 4, 7, 10, 0 }, 4 },
            { "6",   { 0, 4, 7,  9, 0 }, 4 },
            { "5",   { 0, 7, 0,  0, 0 }, 2 },
            { "m",   { 0, 3, 7,  0, 0 }, 3 },
            { "",    { 0, 4, 7,  0, 0 }, 3 }
        };

        for (const auto& candidate : kSuffixes)
        {
            const juce::String candidateText (candidate.text);

            if (candidateText.isEmpty() || suffix.startsWithIgnoreCase (candidateText))
            {
                chord.numIntervals = candidate.count;

                for (int i = 0; i < candidate.count; ++i)
                    chord.intervals[(size_t) i] = candidate.intervals[i];

                break;
            }
        }

        parsed.push_back (chord);
    }

    if (parsed.empty())
        return false;

    chords = std::move (parsed);
    repeats = parsedRepeats;

    return true;
}

const ProgressionLooper::Chord& ProgressionLooper::getChord (int index) const noexcept
{
    static const Chord empty;

    return juce::isPositiveAndBelow (index, (int) chords.size())
             ? chords[(size_t) index] : empty;
}

double ProgressionLooper::getTotalBeats() const noexcept
{
    double total = 0.0;

    for (const auto& chord : chords)
        total += chord.beats;

    return total * (double) juce::jmax (1, repeats);
}

int ProgressionLooper::getChordAtBeat (double beats) const noexcept
{
    if (chords.empty())
        return -1;

    double cycle = 0.0;

    for (const auto& chord : chords)
        cycle += chord.beats;

    if (cycle <= 0.0)
        return -1;

    double position = std::fmod (juce::jmax (0.0, beats), cycle);
    double accumulated = 0.0;

    for (size_t i = 0; i < chords.size(); ++i)
    {
        accumulated += chords[i].beats;

        if (position < accumulated)
            return (int) i;
    }

    return (int) chords.size() - 1;
}

int ProgressionLooper::getMidiNotes (int chordIndex, int* dest, int maxNotes, int octave) const noexcept
{
    if (dest == nullptr || maxNotes <= 0
          || ! juce::isPositiveAndBelow (chordIndex, (int) chords.size()))
        return 0;

    const auto& chord = chords[(size_t) chordIndex];

    // Octave 3 puts the root around the bottom of a guitar's range, which is
    // where a rhythm part sits.
    const int base = juce::jlimit (0, 9, octave) * 12 + chord.rootPitchClass;

    int written = 0;

    if (chord.bassPitchClass >= 0 && written < maxNotes)
    {
        int bass = juce::jlimit (0, 9, octave) * 12 + chord.bassPitchClass;

        // The bass has to be below the root, or it is not a slash chord.
        if (bass >= base)
            bass -= 12;

        dest[written++] = juce::jlimit (0, 127, bass);
    }

    for (int i = 0; i < chord.numIntervals && written < maxNotes; ++i)
        dest[written++] = juce::jlimit (0, 127, base + chord.intervals[(size_t) i]);

    return written;
}

juce::String ProgressionLooper::toString() const
{
    juce::StringArray symbols;

    for (const auto& chord : chords)
        symbols.add (chord.symbol);

    auto text = symbols.joinIntoString (" - ");

    if (repeats > 1)
        text += " x" + juce::String (repeats);

    return text;
}

} // namespace luthier
