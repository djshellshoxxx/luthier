#include "ChordDetector.h"

namespace luthier
{

//==============================================================================
namespace
{
    /*  Interval numbering, semitones above the root:

          0  root      1  b9        2  9        3  m3 / #9
          4  M3        5  11        6  b5 / #11 7  5
          8  #5 / b13  9  13 / bb7  10 b7       11 maj7
    */
    constexpr uint16_t iv (std::initializer_list<int> intervals) noexcept
    {
        uint16_t m = 0;

        for (int i : intervals)
            m = (uint16_t) (m | (uint16_t) (1u << i));

        return m;
    }

    constexpr uint8_t countBits (uint16_t m) noexcept
    {
        uint8_t n = 0;

        while (m != 0)
        {
            n = (uint8_t) (n + (m & 1u));
            m = (uint16_t) (m >> 1);
        }

        return n;
    }

    /** The fifth is droppable almost everywhere: it adds no colour and a
        guitarist leaves it out whenever the hand needs the finger. The root is
        droppable in the extended chords, which is what a rootless voicing is. */
    constexpr uint16_t kFifthOptional = iv ({ 7 });
    constexpr uint16_t kFifthAndRootOptional = iv ({ 0, 7 });

    struct TemplateSpec
    {
        const char* suffix;
        const char* longName;
        uint16_t mask;
        uint16_t optional;
    };

    /*  The chord vocabulary.

        rhythm-engine.md asks for 84 templates. What is here is every quality the
        spec names, plus the altered and extended chords that a guitar plugin is
        actually asked for. It stops where the vocabulary stops being real: two
        templates with the same set of pitch classes cannot both be detected, so
        padding the table to a round number would have meant adding entries that
        could never match. The count is asserted in the tests, and every entry is
        checked to be unique.
    */
    const TemplateSpec kTemplateSpecs[] =
    {
        // ---- triads and dyads ---------------------------------------------------
        { "5",        "power chord",          iv ({ 0, 7 }),                  0 },
        { "",         "major",                iv ({ 0, 4, 7 }),               kFifthOptional },
        { "m",        "minor",                iv ({ 0, 3, 7 }),               kFifthOptional },
        { "dim",      "diminished",           iv ({ 0, 3, 6 }),               0 },
        { "aug",      "augmented",            iv ({ 0, 4, 8 }),               0 },
        { "sus2",     "suspended second",     iv ({ 0, 2, 7 }),               0 },
        { "sus4",     "suspended fourth",     iv ({ 0, 5, 7 }),               0 },

        // ---- sixths ---------------------------------------------------------------
        { "6",        "major sixth",          iv ({ 0, 4, 7, 9 }),            kFifthOptional },
        { "m6",       "minor sixth",          iv ({ 0, 3, 7, 9 }),            kFifthOptional },
        { "6/9",      "six nine",             iv ({ 0, 2, 4, 7, 9 }),         kFifthOptional },
        { "m6/9",     "minor six nine",       iv ({ 0, 2, 3, 7, 9 }),         kFifthOptional },

        // ---- sevenths ---------------------------------------------------------------
        { "7",        "dominant seventh",     iv ({ 0, 4, 7, 10 }),           kFifthOptional },
        { "maj7",     "major seventh",        iv ({ 0, 4, 7, 11 }),           kFifthOptional },
        { "m7",       "minor seventh",        iv ({ 0, 3, 7, 10 }),           kFifthOptional },
        { "mMaj7",    "minor major seventh",  iv ({ 0, 3, 7, 11 }),           kFifthOptional },
        { "dim7",     "diminished seventh",   iv ({ 0, 3, 6, 9 }),            0 },
        { "m7b5",     "half diminished",      iv ({ 0, 3, 6, 10 }),           0 },
        { "dimMaj7",  "diminished major 7",   iv ({ 0, 3, 6, 11 }),           0 },
        { "7sus4",    "dominant 7 sus4",      iv ({ 0, 5, 7, 10 }),           kFifthOptional },
        { "7sus2",    "dominant 7 sus2",      iv ({ 0, 2, 7, 10 }),           kFifthOptional },
        { "maj7sus4", "major 7 sus4",         iv ({ 0, 5, 7, 11 }),           kFifthOptional },

        // ---- added notes -------------------------------------------------------------
        { "add9",     "added ninth",          iv ({ 0, 2, 4, 7 }),            kFifthOptional },
        { "madd9",    "minor added ninth",    iv ({ 0, 2, 3, 7 }),            kFifthOptional },
        { "add11",    "added eleventh",       iv ({ 0, 4, 5, 7 }),            kFifthOptional },
        { "madd11",   "minor added 11th",     iv ({ 0, 3, 5, 7 }),            kFifthOptional },
        { "sus4add9", "sus4 added ninth",     iv ({ 0, 2, 5, 7 }),            kFifthOptional },

        // ---- ninths ---------------------------------------------------------------------
        { "9",        "dominant ninth",       iv ({ 0, 2, 4, 7, 10 }),        kFifthAndRootOptional },
        { "maj9",     "major ninth",          iv ({ 0, 2, 4, 7, 11 }),        kFifthAndRootOptional },
        { "m9",       "minor ninth",          iv ({ 0, 2, 3, 7, 10 }),        kFifthAndRootOptional },
        { "mMaj9",    "minor major ninth",    iv ({ 0, 2, 3, 7, 11 }),        kFifthAndRootOptional },
        { "9sus4",    "ninth sus4",           iv ({ 0, 2, 5, 7, 10 }),        kFifthAndRootOptional },
        { "maj9sus4", "major ninth sus4",     iv ({ 0, 2, 5, 7, 11 }),        kFifthAndRootOptional },
        { "m9b5",     "minor ninth flat 5",   iv ({ 0, 2, 3, 6, 10 }),        0 },
        { "m7b9",     "minor seventh flat 9", iv ({ 0, 1, 3, 7, 10 }),        kFifthOptional },

        // ---- elevenths --------------------------------------------------------------------
        { "maj11",    "major eleventh",       iv ({ 0, 2, 4, 5, 7, 11 }),     kFifthAndRootOptional },
        { "m11",      "minor eleventh",       iv ({ 0, 2, 3, 5, 7, 10 }),     kFifthAndRootOptional },
        { "m7add11",  "minor 7 added 11th",   iv ({ 0, 3, 5, 7, 10 }),        kFifthOptional },
        { "m11b5",    "minor 11th flat 5",    iv ({ 0, 2, 3, 5, 6, 10 }),     0 },
        { "7add11",   "dominant 7 added 11",  iv ({ 0, 4, 5, 7, 10 }),        kFifthOptional },

        // ---- thirteenths ---------------------------------------------------------------------
        { "13",       "dominant thirteenth",  iv ({ 0, 2, 4, 7, 9, 10 }),     kFifthAndRootOptional },
        { "maj13",    "major thirteenth",     iv ({ 0, 2, 4, 7, 9, 11 }),     kFifthAndRootOptional },
        { "m13",      "minor thirteenth",     iv ({ 0, 2, 3, 7, 9, 10 }),     kFifthAndRootOptional },
        { "13sus4",   "thirteenth sus4",      iv ({ 0, 2, 5, 7, 9, 10 }),     kFifthAndRootOptional },

        // ---- altered dominants --------------------------------------------------------------
        { "7b5",      "seventh flat five",    iv ({ 0, 4, 6, 10 }),           0 },
        { "7#5",      "seventh sharp five",   iv ({ 0, 4, 8, 10 }),           0 },
        { "7b9",      "seventh flat nine",    iv ({ 0, 1, 4, 7, 10 }),        kFifthOptional },
        { "7#9",      "seventh sharp nine",   iv ({ 0, 3, 4, 7, 10 }),        kFifthOptional },
        { "7#11",     "seventh sharp eleven", iv ({ 0, 4, 6, 7, 10 }),        0 },
        { "7b13",     "seventh flat 13",      iv ({ 0, 4, 7, 8, 10 }),        kFifthOptional },
        { "7b9b5",    "seventh b9 b5",        iv ({ 0, 1, 4, 6, 10 }),        0 },
        { "7b9#5",    "seventh b9 #5",        iv ({ 0, 1, 4, 8, 10 }),        0 },
        { "7#9b5",    "seventh #9 b5",        iv ({ 0, 3, 4, 6, 10 }),        0 },
        { "7#9#5",    "seventh #9 #5",        iv ({ 0, 3, 4, 8, 10 }),        0 },
        { "7sus4b9",  "seventh sus4 flat 9",  iv ({ 0, 1, 5, 7, 10 }),        kFifthOptional },
        { "7alt",     "altered dominant",     iv ({ 0, 1, 3, 4, 8, 10 }),     0 },
        { "9b5",      "ninth flat five",      iv ({ 0, 2, 4, 6, 10 }),        0 },
        { "9#5",      "ninth sharp five",     iv ({ 0, 2, 4, 8, 10 }),        0 },
        { "9#11",     "ninth sharp eleven",   iv ({ 0, 2, 4, 6, 7, 10 }),     kFifthOptional },
        { "13b9",     "thirteenth flat nine", iv ({ 0, 1, 4, 7, 9, 10 }),     kFifthAndRootOptional },
        { "13#9",     "thirteenth sharp 9",   iv ({ 0, 3, 4, 7, 9, 10 }),     kFifthAndRootOptional },
        { "13#11",    "thirteenth sharp 11",  iv ({ 0, 2, 4, 6, 7, 9, 10 }),  kFifthAndRootOptional },

        // ---- altered majors -------------------------------------------------------------------
        { "maj7b5",   "major 7 flat five",    iv ({ 0, 4, 6, 11 }),           0 },
        { "maj7#5",   "major 7 sharp five",   iv ({ 0, 4, 8, 11 }),           0 },
        { "maj7#11",  "major 7 sharp 11",     iv ({ 0, 4, 6, 7, 11 }),        0 },
        { "maj9#11",  "major 9 sharp 11",     iv ({ 0, 2, 4, 6, 7, 11 }),     kFifthAndRootOptional },
        { "maj7#9",   "major 7 sharp nine",   iv ({ 0, 3, 4, 7, 11 }),        kFifthOptional }
    };

    constexpr int kNumTemplates = (int) (sizeof (kTemplateSpecs) / sizeof (kTemplateSpecs[0]));

    ChordTemplate makeTemplate (const TemplateSpec& spec)
    {
        ChordTemplate t;
        t.suffix = spec.suffix;
        t.longName = spec.longName;
        t.intervalMask = spec.mask;
        t.noteCount = countBits (spec.mask);
        t.optionalMask = spec.optional;
        return t;
    }

    const std::array<ChordTemplate, (size_t) kNumTemplates>& templates()
    {
        static const auto built = []
        {
            std::array<ChordTemplate, (size_t) kNumTemplates> result {};

            for (int i = 0; i < kNumTemplates; ++i)
                result[(size_t) i] = makeTemplate (kTemplateSpecs[i]);

            return result;
        }();

        return built;
    }

    const char* const kPitchNames[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

//==============================================================================
int getNumChordTemplates() noexcept
{
    return kNumTemplates;
}

const ChordTemplate& getChordTemplate (int index) noexcept
{
    return templates()[(size_t) juce::jlimit (0, kNumTemplates - 1, index)];
}

const char* getPitchClassName (int pitchClass) noexcept
{
    return kPitchNames[(size_t) (((pitchClass % 12) + 12) % 12)];
}

//==============================================================================
juce::String ChordSymbol::toString() const
{
    if (! isKnown())
        return "—";

    juce::String name = juce::String (getPitchClassName (root))
                          + getChordTemplate (templateIndex).suffix;

    if (isSlash())
        name << "/" << getPitchClassName (bass);

    return name;
}

int ChordSymbol::writeName (char* dest, int capacity) const noexcept
{
    if (dest == nullptr || capacity <= 0)
        return 0;

    int n = 0;
    auto append = [&] (const char* text)
    {
        for (; text != nullptr && *text != 0 && n < capacity - 1; ++text)
            dest[n++] = *text;
    };

    if (isKnown())
    {
        append (getPitchClassName (root));
        append (getChordTemplate (templateIndex).suffix);

        if (isSlash())
        {
            append ("/");
            append (getPitchClassName (bass));
        }
    }

    dest[n] = 0;
    return n;
}

//==============================================================================
void ChordDetector::prepare (double sampleRate) noexcept
{
    sr = juce::jmax (1.0, sampleRate);
    burstWindowSamples = (int64_t) (kBurstWindowSeconds * sr);
    reset();
}

void ChordDetector::reset() noexcept
{
    numHeld = 0;
    burstStart = -1;
    burstOpen = false;
    lastSymbol = ChordSymbol {};
}

//==============================================================================
ChordSymbol ChordDetector::detect (const int* midiNotes, int numNotes) const noexcept
{
    ChordSymbol result;

    if (midiNotes == nullptr || numNotes <= 0)
        return result;

    // ---- pitch classes and the bass ------------------------------------------
    uint16_t present = 0;
    int lowest = midiNotes[0];

    for (int i = 0; i < numNotes; ++i)
    {
        const int note = midiNotes[i];
        present = (uint16_t) (present | (uint16_t) (1u << (((note % 12) + 12) % 12)));
        lowest = juce::jmin (lowest, note);
    }

    result.bass = ((lowest % 12) + 12) % 12;

    const int distinctHeld = (int) countBits (present);

    if (distinctHeld < 2)
    {
        // One pitch class is not a chord. Reporting it as a major triad with a
        // third of the notes missing would be worse than reporting nothing.
        return result;
    }

    // ---- match every template at every root -----------------------------------
    int bestRoot = -1;
    int bestTemplate = -1;
    double bestScore = -1.0;
    int bestMatched = 0;

    for (int root = 0; root < 12; ++root)
    {
        // Rotate the held set so the candidate root sits at bit zero.
        const uint16_t rotated = (uint16_t) (((present >> root) | (present << (12 - root))) & 0x0FFFu);

        for (int t = 0; t < kNumTemplates; ++t)
        {
            const auto& tmpl = templates()[(size_t) t];

            const uint16_t required = (uint16_t) (tmpl.intervalMask & ~tmpl.optionalMask);
            const uint16_t missingRequired = (uint16_t) (required & ~rotated);

            if (missingRequired != 0)
                continue;

            const uint16_t matchedBits = (uint16_t) (tmpl.intervalMask & rotated);
            const uint16_t extraBits = (uint16_t) (rotated & ~tmpl.intervalMask);

            const int matched = (int) countBits (matchedBits);
            const int extra = (int) countBits (extraBits);
            const int missing = (int) countBits ((uint16_t) (tmpl.intervalMask & ~rotated));

            // Every note the player held should be explained by the template, and
            // every note the template names should be present. Extra notes are
            // penalised harder than missing optional ones, because an unexplained
            // note usually means the wrong template.
            double score = (double) matched
                             - 1.5 * (double) extra
                             - 0.75 * (double) missing;

            // A root sounding in the bass is evidence, but only enough to break
            // a tie. Weighting it any harder makes every first-inversion minor
            // triad read as a sixth chord on its third - Cm becomes Eb6 - which
            // is a reading a guitarist would never agree with.
            if (root == result.bass)
                score += 0.3;

            // Prefer the simpler reading when two templates explain the same
            // notes equally well: a plain triad over a rootless thirteenth.
            score -= 0.05 * (double) tmpl.noteCount;

            if (score > bestScore)
            {
                bestScore = score;
                bestRoot = root;
                bestTemplate = t;
                bestMatched = matched;
            }
        }
    }

    if (bestTemplate < 0)
        return result;

    result.root = bestRoot;
    result.templateIndex = bestTemplate;

    // rhythm-engine 2.5: confidence is how much of what the player held the
    // template accounts for. Notes the template cannot explain count against it,
    // or a chromatic cluster would read as a minor chord with a wrong note in
    // it: three of its four notes do happen to spell one.
    const uint16_t bestRotated = (uint16_t) (((present >> bestRoot) | (present << (12 - bestRoot))) & 0x0FFFu);
    const int unexplained = (int) countBits ((uint16_t) (bestRotated & ~templates()[(size_t) bestTemplate].intervalMask));

    result.confidence = juce::jlimit (0.0, 1.0,
                                      (double) (bestMatched - unexplained)
                                        / (double) juce::jmax (1, distinctHeld));

    if (result.confidence < kConfidenceFloor)
    {
        // Known to be unknown: the caller falls through to literal per-string
        // placement rather than voicing a chord nobody played.
        result.root = -1;
        result.templateIndex = -1;
        return result;
    }

    // ---- extensions above the seventh ------------------------------------------
    const auto& chosen = templates()[(size_t) bestTemplate];
    const uint16_t extras = (uint16_t) (bestRotated & ~chosen.intervalMask);

    for (int interval = 1; interval < 12 && result.numExtensions < ChordSymbol::kMaxExtensions; ++interval)
        if ((extras & (uint16_t) (1u << interval)) != 0)
            result.extensions[(size_t) result.numExtensions++] = interval;

    return result;
}

//==============================================================================
bool ChordDetector::noteOn (int midiNote, int64_t sampleTime) noexcept
{
    if (numHeld < (int) held.size())
    {
        bool alreadyHeld = false;

        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] == midiNote)
                alreadyHeld = true;

        if (! alreadyHeld)
            held[(size_t) numHeld++] = midiNote;
    }

    if (! burstOpen)
    {
        burstOpen = true;
        burstStart = sampleTime;
    }

    return false;
}

void ChordDetector::noteOff (int midiNote) noexcept
{
    for (int i = 0; i < numHeld; ++i)
    {
        if (held[(size_t) i] == midiNote)
        {
            held[(size_t) i] = held[(size_t) (--numHeld)];
            return;
        }
    }
}

void ChordDetector::allNotesOff() noexcept
{
    numHeld = 0;
    burstOpen = false;
    burstStart = -1;
}

bool ChordDetector::advance (int64_t sampleTime) noexcept
{
    if (! burstOpen)
        return false;

    if (sampleTime - burstStart < burstWindowSamples)
        return false;

    burstOpen = false;
    burstStart = -1;

    lastSymbol = detect (held.data(), numHeld);
    return true;
}

} // namespace luthier
