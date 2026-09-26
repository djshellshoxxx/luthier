#pragma once

/*  The scale trainer, the ear trainer and the progression looper
    (practice-tools.md sections 4, 5 and 7).

    All three are control-only: they decide what to ask and score what comes
    back, and never touch the audio path. Exercises are played by handing the
    caller a set of notes to sound through the instrument, which is what
    practice-tools 5 means by ear training happening in context - the intervals
    are heard on the guitar the player has built, not on a sine.

    The adaptive difficulty is the part worth reading. A trainer that asks the
    same questions regardless of how the player is doing is a quiz, not a
    teacher; this one tracks a rolling success rate and moves the difficulty to
    keep the player at about three quarters right, which is where practice stops
    being either boring or demoralising.
*/

#include "../DSP/Common/DspCommon.h"

#include <array>
#include <vector>

namespace luthier
{

//==============================================================================
/** The scales the trainer knows (practice-tools 4). */
enum class ScaleType
{
    ionian = 0, dorian, phrygian, lydian, mixolydian, aeolian, locrian,
    harmonicMinor, melodicMinor,
    majorPentatonic, minorPentatonic, blues,
    custom,
    numScales
};

const char* getScaleTypeName (ScaleType type) noexcept;

/** The semitone intervals of a scale, from its root. Returns how many. */
int getScaleIntervals (ScaleType type, int* dest, int maxIntervals) noexcept;

/** The degree names of a scale, for the overlay. */
const char* getScaleDegreeName (ScaleType type, int degreeIndex) noexcept;

//==============================================================================
/** A scale in a key, and the questions that can be asked about it. */
class ScaleTrainer
{
public:
    static constexpr int kMaxIntervals = 12;

    enum class Mode { explore = 0, quiz, intervalTrainer, chordToneTrainer, numModes };

    ScaleTrainer();

    void setKey (int pitchClass) noexcept { root = ((pitchClass % 12) + 12) % 12; }
    int getKey() const noexcept { return root; }

    void setScale (ScaleType type) noexcept;
    ScaleType getScale() const noexcept { return scale; }

    /** practice-tools 4: custom scales are an interval list. */
    void setCustomIntervals (const int* intervals, int count) noexcept;

    void setMode (Mode m) noexcept { mode = m; }
    Mode getMode() const noexcept { return mode; }

    //==========================================================================
    /** True when this pitch class is in the current scale. */
    bool containsPitchClass (int pitchClass) const noexcept;

    /** Which degree a pitch class is, 1-based, or 0 if it is not in the scale. */
    int getDegreeOf (int pitchClass) const noexcept;

    /** The pitch class of a degree, 1-based. */
    int getPitchClassOfDegree (int degree) const noexcept;

    int getNumDegrees() const noexcept { return numIntervals; }

    //==========================================================================
    /** Poses the next question. Returns its text; the answer is stored. */
    juce::String nextQuestion (juce::Random& random);

    /** The pitch class the current question wants, or -1 if there is none. */
    int getExpectedPitchClass() const noexcept { return expectedPitchClass; }

    /** Scores a played note. Returns true if it was right. */
    bool answer (int midiNote);

    juce::String getCurrentQuestion() const { return question; }

    //==========================================================================
    int getScore() const noexcept { return correct; }
    int getAsked() const noexcept { return asked; }

    double getSuccessRate() const noexcept
    {
        return asked > 0 ? (double) correct / (double) asked : 0.0;
    }

    void resetScore() noexcept { correct = 0; asked = 0; }

    //==========================================================================
    /*  The PRACTICE tab's trainer setup (practice-tools 11.2, MODEL-GAPS TODO
        11): the notes that count as answers, and how many questions a session
        is. A note outside the range is not an answer - it is not scored either
        way. 0 questions is an open-ended session. */
    void setNoteRange (int lowNote, int highNote) noexcept;
    int getLowNote() const noexcept { return lowNote; }
    int getHighNote() const noexcept { return highNote; }

    void setQuestionCount (int count) noexcept { questionCount = juce::jmax (0, count); }
    int getQuestionCount() const noexcept { return questionCount; }
    bool isSessionComplete() const noexcept { return questionCount > 0 && asked >= questionCount; }

private:
    void rebuild() noexcept;

    int lowNote = 0, highNote = 127, questionCount = 0;

    int root = 0;
    ScaleType scale = ScaleType::ionian;
    Mode mode = Mode::explore;

    std::array<int, kMaxIntervals> intervals {};
    int numIntervals = 7;

    std::array<int, kMaxIntervals> customIntervals {};
    int numCustomIntervals = 0;

    juce::String question;
    int expectedPitchClass = -1;

    int correct = 0, asked = 0;
};

//==============================================================================
/** Ear training (practice-tools.md section 5). */
class EarTrainer
{
public:
    enum class Exercise { interval = 0, chordQuality, progression, numExercises };

    /** How an interval is presented. */
    enum class Presentation { ascending = 0, descending, harmonic, numPresentations };

    /** practice-tools 5: eleven chord qualities. */
    static constexpr int kNumChordQualities = 11;

    /** practice-tools 5: five named progressions and ten more (SPEC-SWEEP PT-38). */
    static constexpr int kNumProgressions = 15;

    static constexpr int kMaxNotesInQuestion = 16;

    EarTrainer();

    void setExercise (Exercise e) noexcept { exercise = e; }
    Exercise getExercise() const noexcept { return exercise; }

    /** 0 is easiest. The trainer moves this itself unless it is locked. */
    void setDifficulty (int level) noexcept;
    int getDifficulty() const noexcept { return difficulty; }

    void setAdaptive (bool shouldAdapt) noexcept { adaptive = shouldAdapt; }
    bool isAdaptive() const noexcept { return adaptive; }

    //==========================================================================
    /** Chooses the next question. Returns the number of notes to play, written
        into `notes` as MIDI note numbers with their offsets in beats. */
    int nextQuestion (juce::Random& random, int* notes, double* beatOffsets, int maxNotes);

    /** The answer choices to offer, in order. The right one is at
        getCorrectChoice(). */
    juce::StringArray getChoices() const { return choices; }
    int getCorrectChoice() const noexcept { return correctChoice; }

    juce::String getQuestionText() const { return questionText; }

    /** Scores a choice. Returns true if it was right, and moves the difficulty
        if the trainer is adaptive. */
    bool answer (int choiceIndex);

    //==========================================================================
    int getStreak() const noexcept { return streak; }
    int getAsked() const noexcept { return asked; }
    int getCorrect() const noexcept { return correct; }

    double getRollingSuccessRate() const noexcept { return rollingRate; }

    void resetScore() noexcept;

    /*  The PRACTICE tab's trainer setup (MODEL-GAPS TODO 11): every note a
        question plays is moved by octaves into this range where it fits; a
        session is this many questions (0 is open-ended). */
    void setNoteRange (int lowNote, int highNote) noexcept;
    int getLowNote() const noexcept { return lowNote; }
    int getHighNote() const noexcept { return highNote; }

    void setQuestionCount (int count) noexcept { questionCount = juce::jmax (0, count); }
    int getQuestionCount() const noexcept { return questionCount; }
    bool isSessionComplete() const noexcept { return questionCount > 0 && asked >= questionCount; }

    //==========================================================================
    /** practice-tools 5: session stats live in the user's practice folder. */
    juce::var statsToVar() const;
    void statsFromVar (const juce::var& state);

    bool saveStats() const;
    bool loadStats();

    static juce::File getStatsFile();

private:
    int buildIntervalQuestion (juce::Random& random, int* notes, double* beatOffsets, int maxNotes);
    int buildChordQuestion (juce::Random& random, int* notes, double* beatOffsets, int maxNotes);
    int buildProgressionQuestion (juce::Random& random, int* notes, double* beatOffsets, int maxNotes);

    void adapt (bool wasCorrect) noexcept;

    Exercise exercise = Exercise::interval;
    Presentation presentation = Presentation::ascending;

    int difficulty = 0;
    bool adaptive = true;

    juce::StringArray choices;
    int correctChoice = 0;
    juce::String questionText;

    int correct = 0, asked = 0, streak = 0;
    int lowNote = 0, highNote = 127, questionCount = 0;

    /*  A rolling rate rather than a lifetime one.

        The point of adapting is to follow how the player is doing now. A
        lifetime average over a thousand questions barely moves, so a player who
        has just improved would stay on easy questions for hours. */
    double rollingRate = 0.5;

    /** Per-exercise totals, kept across sessions. */
    std::array<int, (size_t) Exercise::numExercises> lifetimeAsked {};
    std::array<int, (size_t) Exercise::numExercises> lifetimeCorrect {};
};

//==============================================================================
/** The chord progression looper (practice-tools.md section 7).

    Parses a progression written the way a musician writes one - `Am - F - C - G
    x4` - into a list of chords with a bar count each. Voicing and strumming them
    is the rhythm engine's job; this only works out what to play. */
class ProgressionLooper
{
public:
    static constexpr int kMaxChords = 64;

    struct Chord
    {
        juce::String symbol;
        int rootPitchClass = 0;
        int bassPitchClass = -1;

        /** The chord's notes as semitone intervals from the root. */
        std::array<int, 6> intervals {};
        int numIntervals = 0;

        /** How many beats it lasts. */
        double beats = 4.0;
    };

    /** Parses a progression. Returns false and leaves the previous one in place
        if nothing could be read from the text. */
    bool parse (const juce::String& text);

    int getNumChords() const noexcept { return (int) chords.size(); }
    const Chord& getChord (int index) const noexcept;

    int getRepeats() const noexcept { return repeats; }

    double getTotalBeats() const noexcept;

    /** Which chord is sounding at a given beat, wrapping round the progression.
        Returns -1 when there is no progression. */
    int getChordAtBeat (double beats) const noexcept;

    /** The MIDI notes of a chord, in an octave that sits on a guitar. Returns
        how many were written. */
    int getMidiNotes (int chordIndex, int* dest, int maxNotes, int octave = 3) const noexcept;

    juce::String toString() const;

private:
    std::vector<Chord> chords;
    int repeats = 1;
};

} // namespace luthier
