#pragma once

/*  Rubric chord voicer (ambiguity-resolutions.md 4, extending rhythm-engine.md 3).

    ChordVoicer ranks fingerings by a cost of its own invention. This one ranks
    them by the rubric section 4 writes down, and nothing else:

      4.1  six constraints every candidate must pass, or it is not a candidate;
      4.2  a weighted score, higher is better, with the weights as given;
      4.3  a bias per voicing style, including Bass for the bass family;
      4.4  a bonus for staying close to the previous chord's voicing;
      4.5  the capo, which TuningEngine already reports per string;
      4.6  a fixed tie-break order, so the same inputs give the same voicing.

    It takes the same call as ChordVoicer::voice and returns the same
    ChordVoicing, so it can stand wherever ChordVoicer stands. Two readings of
    the notes it is given:

      - PitchMode::exact places the pitches it was given and no others. This is
        MidiInterpreter's case: the player chose those notes, and a chord played
        on a keyboard must sound at the pitches it was played at.
      - PitchMode::chordTones treats them as a chord - a set of pitch classes -
        and may put any chord tone in any octave on any string, doubling where
        the rubric pays for it. This is the rhythm engine's case (rhythm-engine
        3 takes a chord symbol, not a stack of pitches), and it is the reading
        4.2's open-string, mute and duplicate terms were written for.

    When no fingering passes 4.1 the answer is an explicit "unplayable"
    (4.7): an empty voicing with playable == false, and getLastOutcome() says
    so. There is no silent best effort.

    Real time: nothing allocates. Every table is a fixed array in the object,
    sized by kMaxStrings, and the search is bounded by a node budget - see
    kMaxSearchNodes for the worst case.
*/

#include "ChordVoicer.h"

#include <array>
#include <cstdint>

namespace luthier
{

//==============================================================================
/** The voicing styles of 4.3. The first nine are in the same order as the
    rhythm engine's VoicingStyle, so one converts to the other with a cast;
    `bass` is new, and belongs at the end of that enum too. */
enum class RubricStyle
{
    open = 0, barre, triad, shell, drop2, drop3, power, rootless, wide,
    bass,
    numStyles
};

/** What the Bass style voices (4.3, 4.7). 4.3 pays +8 for "root only or root +
    fifth", and 4.7 wants a C7 to give the root alone in one style and root plus
    fifth in another, so the pattern says which of the two shapes the +8 goes
    to. These are tune-builder.md 6's bass-line options that make sense for a
    single voicing; Walking voices the root, and the approach bonus rewards
    reaching it by step. */
enum class RubricBassPattern { root, rootFifth, walking };

enum class RubricPitchMode { exact, chordTones };

enum class RubricOutcome
{
    none,        ///< Nothing was asked for: no notes, or no tuning.
    voiced,      ///< A fingering passed every constraint of 4.1.
    unplayable   ///< Notes were asked for and nothing passed 4.1 (4.7).
};

//==============================================================================
/** The weights of 4.2 - 4.4, as the spec writes them. The voicer scores in
    tenths so that equal scores are exactly equal (4.6 depends on it), and every
    weight here is a whole number of tenths. */
struct RubricWeights
{
    // ---- 4.2 ---------------------------------------------------------------
    static constexpr double openBonus               = 3.0;   // per open string
    static constexpr double handMovePenalty         = 0.4;   // per fret from the hint
    static constexpr double barrePenalty            = 2.0;
    static constexpr double mutePenalty             = 4.0;   // per muted string
    static constexpr double dupNotePenalty          = 1.0;   // per duplicated pitch class
    static constexpr double extensionDroppedPenalty = 3.0;   // per chord tone left out

    // ---- 4.3 ---------------------------------------------------------------
    static constexpr double openStyleOpenString   = 6.0;
    static constexpr double openStyleRootLow      = 3.0;     // root on string 5 or 6
    static constexpr double openStyleBarre        = -2.0;
    static constexpr double barreStyleBarre       = 5.0;
    static constexpr double barreStyleRootLowest  = 2.0;     // root on string 6
    static constexpr double triadMatch            = 6.0;
    static constexpr double triadMiss             = -3.0;
    static constexpr double shellMatch            = 7.0;
    static constexpr double shellMiss             = -5.0;
    static constexpr double dropMatch             = 5.0;
    static constexpr double powerMatch            = 8.0;
    static constexpr double powerMiss             = -6.0;
    static constexpr double rootlessMatch         = 6.0;
    static constexpr double rootlessMiss          = -6.0;
    static constexpr double wideMatch             = 4.0;
    static constexpr double bassMatch             = 8.0;
    static constexpr double bassWalkingApproach   = 2.0;
    static constexpr double bassWiderThanOctave   = -6.0;

    // ---- 4.4 ---------------------------------------------------------------
    static constexpr double commonNote            = 2.0;
    static constexpr double commonPosition        = 1.0;
    static constexpr double positionJump          = -2.0;
    static constexpr int    commonPositionFrets   = 1;       // "fret proximity"
    static constexpr int    positionJumpFrets     = 5;       // a jump is more than this

    // ---- 4.1 ---------------------------------------------------------------
    static constexpr int    maxInversionSemitones = 4;       // constraint 4
    static constexpr int    minBarreStrings       = 3;       // constraint 5
    static constexpr int    fingers               = 4;       // more fretted notes than this need a barre
};

//==============================================================================
/** Every term of one candidate's score, for the tests and for the UI's
    "why this voicing" readout. Strings are courses on a 12-string. */
struct RubricScore
{
    /** Which 4.1 constraint failed, 1-6, or kNotThisChord when the fingering
        does not voice the request at all (a note that is not a chord tone, a
        pitch used twice in exact mode, nothing sounding, too many strings). */
    static constexpr int kNotThisChord = -1;

    bool valid = false;
    int  failedConstraint = 0;

    int  soundingStrings = 0;
    int  openStrings = 0;
    int  mutedStrings = 0;
    int  frettedStrings = 0;
    int  duplicates = 0;
    int  droppedExtensions = 0;
    int  commonNotes = 0;
    int  commonPositions = 0;
    int  positionJumps = 0;
    bool usesBarre = false;
    int  barreFret = 0;          ///< Hand frets (from the capo); 0 without a barre.
    int  handPosition = 0;       ///< The barre fret, else the lowest fretted; 0 if nothing is fretted.
    int  fretSum = 0;            ///< Sum of fretted positions, for 4.6's second tie-break.

    double openTerm = 0.0;
    double styleTerm = 0.0;
    double transitionTerm = 0.0;
    double handMoveTerm = 0.0;
    double barreTerm = 0.0;
    double muteTerm = 0.0;
    double duplicateTerm = 0.0;
    double droppedTerm = 0.0;

    double total = 0.0;
    int    totalTenths = 0;
};

//==============================================================================
class RubricVoicer
{
public:
    /** The search visits at most this many (string, option) pairs per call, and
        scores at most kMaxScoredCandidates complete fingerings.

        Worst case without the budget: every string can be muted, open, or
        fretted anywhere a chord tone falls, so a 7-note chord on an 8-string
        offers roughly 17 options a string and 17^8 fingerings. Span, the
        constraints and the branch-and-bound cut real chords to a few hundred
        or a few thousand nodes; the budget is what makes the worst case a
        number. At the budget the search keeps the best fingering found so far,
        which is still a valid one, and wasLastSearchTruncated() says it
        stopped early. The order it searches in is fixed, so a truncated
        search is as deterministic as a complete one. */
    static constexpr int kMaxSearchNodes = 32768;
    static constexpr int kMaxScoredCandidates = 4096;

    /** Frets 0-30: more than any neck in the library. */
    static constexpr int kFretTableSize = 31;

    /** Notes of one request, after duplicates are removed. */
    static constexpr int kMaxRequest = 24;

    RubricVoicer() noexcept;

    //==========================================================================
    void prepare (const TuningEngine* tuning, int numStrings) noexcept;
    void setNumStrings (int n) noexcept;

    /** Forgets the previous voicing (4.4) and the hand position, as
        ChordVoicer::reset does, so the first chord after a prepare is voiced the
        same way every time. */
    void reset() noexcept;

    /** 4.1 constraint 2's hand_span_frets: the most frets, highest minus
        lowest, the fretted notes may spread over. rhythm-engine 3: default 5,
        user range 3-7. */
    void setMaxFretSpan (int frets) noexcept { maxFretSpan = juce::jlimit (3, 7, frets); }
    int getMaxFretSpan() const noexcept { return maxFretSpan; }

    /** 4.1 constraint 1's guitar.max_fret. The tuning engine's playable span,
        which a capo shortens, is a second ceiling and the lower one wins. */
    void setMaxFret (int fret) noexcept { maxFret = juce::jlimit (5, kFretTableSize - 1, fret); }

    /** A floor on fretted positions, measured from the capo. Not the capo
        itself: TuningEngine already measures frets from it (4.5). */
    void setMinFret (int fret) noexcept { minFret = juce::jlimit (0, 24, fret); }
    int getMinFret() const noexcept { return minFret; }

    /** 4.2's hand_position_hint, in frets from the capo. Updated to the chosen
        voicing's hand position after each chord, as ChordVoicer does, so a
        caller that never sets it still gets a progression that stays put. */
    void setPreferredPosition (int fret) noexcept { handHint = juce::jlimit (0, 24, fret); }
    int getPreferredPosition() const noexcept { return handHint; }

    void setAllowOpenStrings (bool allow) noexcept { allowOpen = allow; }

    /** Strings already sounding a held note, one bit per string index. Such a
        string is out of play: the search may neither fret it nor count it as
        muted (it is not muted, it is ringing), and the single-note placer
        skips it. MidiInterpreter sets this from what it holds before every
        chord group and clears it after; reset() clears it too. */
    void setOccupiedStrings (uint16_t mask) noexcept { occupiedStrings = mask; }
    uint16_t getOccupiedStrings() const noexcept { return occupiedStrings; }

    //==========================================================================
    void setStyle (RubricStyle s) noexcept;
    RubricStyle getStyle() const noexcept { return style; }

    void setBassPattern (RubricBassPattern p) noexcept { bassPattern = p; }
    RubricBassPattern getBassPattern() const noexcept { return bassPattern; }

    void setPitchMode (RubricPitchMode m) noexcept { pitchMode = m; }
    RubricPitchMode getPitchMode() const noexcept { return pitchMode; }

    /** The chord's root as a pitch class, from the chord detector; -1 takes the
        lowest note given, which is what a caller without a chord symbol means. */
    void setRootPitchClass (int pitchClass) noexcept;

    /** rhythm-engine 3's voicing_density as a string count. Each style has its
        own ceiling as well (triad, shell and power three, drops four, bass two);
        the lower of the two applies. */
    void setMaxSoundingStrings (int n) noexcept { maxSounding = juce::jlimit (1, kMaxStrings, n); }

    /** The voicing 4.4's transition bonus is measured against. voice() records
        its own result here, so this is only needed to seed or override it. */
    void setPreviousVoicing (const ChordVoicing& v) noexcept;
    void clearPreviousVoicing() noexcept { hasPrevious = false; }
    bool hasPreviousVoicing() const noexcept { return hasPrevious; }

    //==========================================================================
    /** Voices a set of MIDI notes, as ChordVoicer::voice does.

        @returns the best fingering by 4.2 - 4.6, or an empty voicing with
                 playable == false when nothing passes 4.1.
    */
    ChordVoicing voice (const int* midiNotes, const double* velocities, int numNotes) noexcept;

    /** Single notes are not chords and the rubric says nothing about them, so
        they are placed exactly as ChordVoicer places them, near the hand. */
    VoicedNote voiceSingleNote (int midiNote, double velocity, int preferStringIndex = -1) noexcept;

    /** Scores one fingering against a request with the current settings, the
        current previous voicing and the current hint, without searching and
        without changing any of them. `fretPerString` has one entry per string,
        string 0 the highest, -1 for muted; frets are measured from the capo.
        On a 12-string only the first string of each course is read. */
    RubricScore evaluate (const int* fretPerString, const int* midiNotes, int numNotes) noexcept;

    //==========================================================================
    RubricOutcome getLastOutcome() const noexcept { return lastOutcome; }
    const RubricScore& getLastScore() const noexcept { return lastScore; }
    int getLastSearchNodes() const noexcept { return nodesVisited; }
    bool wasLastSearchTruncated() const noexcept { return truncated; }

    //==========================================================================
    /** What 4.6 ranks by: score, then string count descending, then fret sum
        ascending. Anything still equal is ordered by the frets themselves,
        string 0 first, lower first, so the winner never depends on the order
        the search happened to meet the candidates in. */
    struct RankKey
    {
        int scoreTenths = 0;
        int strings = 0;
        int fretSum = 0;
        int numSlots = 0;
        std::array<int, kMaxStrings> frets {};
    };

    static bool ranksAbove (const RankKey& a, const RankKey& b) noexcept;

private:
    struct Option
    {
        int relFret = -1;       ///< From the capo; -1 = muted.
        int handFret = 0;       ///< From the capo for every string; 0 = open or muted.
        int pitch = -1;
        int requestIndex = -1;  ///< Exact mode: which requested note this is.
        int gain = 0;           ///< Upper bound on this string's own terms, in tenths.
        int order = 0;
    };

    struct SearchState
    {
        int count = 0;
        int fretted = 0;
        int minHand = 1000;
        int maxHand = -1;
        int lastPitch = -1;
        int gain = 0;
        int dups = 0;
        uint16_t pcMask = 0;
        uint32_t used = 0;
    };

    static constexpr int kMaxOptions = kFretTableSize + 1;

    void prepareSlots() noexcept;
    bool prepareRequest (const int* midiNotes, const double* velocities, int numNotes) noexcept;
    void preparePrevious() noexcept;
    void buildOptions() noexcept;

    int handFretFor (int slot, int relFret) const noexcept;
    bool fretAllowed (int slot, int relFret) const noexcept;
    int findRequest (int pitch) const noexcept;
    int transitionGain (int slot, int relFret, int handFret, int pitch) const noexcept;
    int styleCap() const noexcept;
    int styleMaxTenths() const noexcept;

    /** The one place a fingering is judged: 4.1, then every term of 4.2 - 4.4.
        Returns false when a constraint fails. */
    bool scoreCandidate (const int* relFretPerSlot, RubricScore& out) const noexcept;
    int styleBias (const int* relFretPerSlot, const RubricScore& s, int lowestSlot,
                   int lowestPitch, int highestPitch, uint16_t pcMask, const int* pcCount) const noexcept;
    bool matchesDrop (const int* relFretPerSlot, uint16_t pcMask, int dropFromTop) const noexcept;

    void search (int depth) noexcept;
    void considerLeaf() noexcept;
    int upperBound (int slotsLeft) const noexcept;

    int pairPitch (int stringIndex, int relFret) const noexcept;
    double velocityFor (int pitch) const noexcept;

    //==========================================================================
    const TuningEngine* tuningEngine = nullptr;
    ChordVoicer singleNotes;

    int numStrings = 6;
    int maxFretSpan = 5;
    int maxFret = 22;
    int minFret = 0;
    int handHint = 0;
    bool allowOpen = true;
    int maxSounding = kMaxStrings;
    int rootOverride = -1;
    uint16_t occupiedStrings = 0;

    RubricStyle style = RubricStyle::open;
    RubricBassPattern bassPattern = RubricBassPattern::root;
    RubricPitchMode pitchMode = RubricPitchMode::exact;

    // ---- the strings, per call --------------------------------------------------
    int effectiveStrings = 6;
    int numSlots = 6;
    bool coursed = false;
    int capoFret = 0;
    std::array<int, kMaxStrings> slotString {};
    std::array<int, kMaxStrings> slotCapo {};
    std::array<int, kMaxStrings> slotHighest {};
    std::array<bool, kMaxStrings> slotOccupied {};
    /** occupiedBelow[k]: how many of slots 0..k-1 are occupied, for the bound. */
    std::array<int, kMaxStrings + 1> occupiedBelow {};
    std::array<std::array<int, kFretTableSize>, kMaxStrings> pitchAt {};

    // ---- the request -------------------------------------------------------------
    std::array<int, kMaxRequest> request {};
    std::array<double, kMaxRequest> requestVelocity {};
    int numRequest = 0;
    int unplaceable = 0;
    int rootPc = 0;
    int bassPc = 0;
    int fifthPc = -1;           ///< For Power and Root-Fifth: the 5th, b5 or #5 the chord has.
    int thirdPc = -1;
    int seventhPc = -1;
    uint16_t keptMask = 0;      ///< Pitch classes this style may sound.
    uint16_t countedMask = 0;   ///< Those whose absence costs extension_dropped_penalty.
    int soundingCap = kMaxStrings;

    // ---- the previous voicing (4.4) -----------------------------------------------
    ChordVoicing previous;
    bool hasPrevious = false;
    std::array<int, kMaxStrings> prevHand {};  ///< Per slot: hand fret, 0 open, -1 muted.
    std::array<uint64_t, 2> prevPitches {};
    int prevLowestPitch = -1;

    // ---- the search ---------------------------------------------------------------
    std::array<std::array<Option, kMaxOptions>, kMaxStrings> options {};
    std::array<int, kMaxStrings> numOptions {};
    std::array<int, kMaxStrings + 1> suffixMaxGain {};
    int maxSoundingGain = 0;

    SearchState state;
    std::array<int, kMaxStrings> current {};
    std::array<int, kMaxStrings> bestFrets {};
    RankKey best;
    bool haveBest = false;
    int nodesVisited = 0;
    int leavesScored = 0;
    bool truncated = false;

    RubricOutcome lastOutcome = RubricOutcome::none;
    RubricScore lastScore;
};

} // namespace luthier
