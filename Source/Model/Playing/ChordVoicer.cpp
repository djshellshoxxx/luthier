#include "ChordVoicer.h"

namespace luthier
{

namespace
{
    //==========================================================================
    // A library of the shapes a guitarist actually plays. String 0 = high E.
    // -1 = muted, 0 = open.
    const ChordVoicer::LibraryChord kLibrary[] =
    {
        { "C",       {  0,  1,  0,  2,  3, -1 }, {  0,  1,  0,  2,  3, -1 } },
        { "C7",      {  0,  1,  3,  2,  3, -1 }, {  0,  1,  4,  2,  3, -1 } },
        { "Cmaj7",   {  0,  0,  0,  2,  3, -1 }, {  0,  0,  0,  2,  3, -1 } },
        { "Cm",      {  3,  4,  5,  5,  3, -1 }, {  1,  2,  4,  4,  1, -1 } },
        { "C5",      { -1, -1,  5,  5,  3, -1 }, { -1, -1,  4,  3,  1, -1 } },

        { "D",       {  2,  3,  2,  0, -1, -1 }, {  2,  3,  1,  0, -1, -1 } },
        { "D7",      {  2,  1,  2,  0, -1, -1 }, {  3,  1,  2,  0, -1, -1 } },
        { "Dm",      {  1,  3,  2,  0, -1, -1 }, {  1,  3,  2,  0, -1, -1 } },
        { "Dmaj7",   {  2,  2,  2,  0, -1, -1 }, {  3,  2,  1,  0, -1, -1 } },
        { "Dsus4",   {  3,  3,  2,  0, -1, -1 }, {  4,  3,  1,  0, -1, -1 } },

        { "E",       {  0,  0,  1,  2,  2,  0 }, {  0,  0,  1,  3,  2,  0 } },
        { "E7",      {  0,  0,  1,  0,  2,  0 }, {  0,  0,  1,  0,  2,  0 } },
        { "Em",      {  0,  0,  0,  2,  2,  0 }, {  0,  0,  0,  3,  2,  0 } },
        { "Em7",     {  0,  0,  0,  0,  2,  0 }, {  0,  0,  0,  0,  2,  0 } },
        { "E5",      { -1, -1, -1,  2,  2,  0 }, { -1, -1, -1,  3,  2,  0 } },

        { "F",       {  1,  1,  2,  3,  3,  1 }, {  1,  1,  2,  4,  3,  1 } },
        { "Fmaj7",   {  0,  1,  2,  3, -1, -1 }, {  0,  1,  2,  3, -1, -1 } },
        { "Fm",      {  1,  1,  1,  3,  3,  1 }, {  1,  1,  1,  4,  3,  1 } },

        { "G",       {  3,  0,  0,  0,  2,  3 }, {  4,  0,  0,  0,  1,  3 } },
        { "G7",      {  1,  0,  0,  0,  2,  3 }, {  1,  0,  0,  0,  2,  3 } },
        { "Gm",      {  3,  3,  3,  5,  5,  3 }, {  1,  1,  1,  4,  3,  1 } },
        { "Gmaj7",   {  2,  0,  0,  0,  2,  3 }, {  2,  0,  0,  0,  1,  3 } },

        { "A",       {  0,  2,  2,  2,  0, -1 }, {  0,  3,  2,  1,  0, -1 } },
        { "A7",      {  0,  2,  0,  2,  0, -1 }, {  0,  3,  0,  2,  0, -1 } },
        { "Am",      {  0,  1,  2,  2,  0, -1 }, {  0,  1,  3,  2,  0, -1 } },
        { "Am7",     {  0,  1,  0,  2,  0, -1 }, {  0,  1,  0,  2,  0, -1 } },
        { "Amaj7",   {  0,  2,  1,  2,  0, -1 }, {  0,  3,  1,  2,  0, -1 } },
        { "A5",      { -1, -1, -1,  2,  0, -1 }, { -1, -1, -1,  2,  0, -1 } },

        { "B",       {  2,  4,  4,  4,  2, -1 }, {  1,  3,  3,  3,  1, -1 } },
        { "B7",      {  2,  0,  2,  1,  2, -1 }, {  4,  0,  3,  1,  2, -1 } },
        { "Bm",      {  2,  3,  4,  4,  2, -1 }, {  1,  2,  4,  3,  1, -1 } },
        { "Bm7",     {  2,  3,  2,  4,  2, -1 }, {  1,  2,  1,  3,  1, -1 } },

        { "Dsus2",   {  0,  3,  2,  0, -1, -1 }, {  0,  3,  2,  0, -1, -1 } },
        { "Asus2",   {  0,  0,  2,  2,  0, -1 }, {  0,  0,  3,  2,  0, -1 } },
        { "Asus4",   {  0,  3,  2,  2,  0, -1 }, {  0,  4,  3,  2,  0, -1 } },
        { "Esus4",   {  0,  0,  2,  2,  2,  0 }, {  0,  0,  2,  4,  3,  0 } },
        { "Cadd9",   {  3,  3,  0,  2,  3, -1 }, {  4,  3,  0,  1,  2, -1 } },
        { "Gadd9",   {  3,  0,  0,  2,  0,  3 }, {  3,  0,  0,  2,  0,  4 } },

        { "Cm7",     {  3,  4,  3,  5,  3, -1 }, {  1,  2,  1,  4,  1, -1 } },
        { "Fm7",     {  1,  1,  1,  3,  3,  1 }, {  1,  1,  1,  4,  3,  1 } },
        { "D5",      { -1, -1,  7,  7,  5, -1 }, { -1, -1,  4,  3,  1, -1 } },
        { "G5",      { -1, -1, -1,  5,  5,  3 }, { -1, -1, -1,  4,  3,  1 } },
        { "F5",      { -1, -1, -1,  3,  3,  1 }, { -1, -1, -1,  4,  3,  1 } },
        { "Bb",      {  1,  3,  3,  3,  1, -1 }, {  1,  3,  3,  3,  1, -1 } },
        { "Eb",      {  3,  4,  3,  1, -1, -1 }, {  4,  4,  3,  1, -1, -1 } },
        { "Ab",      {  4,  4,  5,  6,  6,  4 }, {  1,  1,  2,  4,  3,  1 } },
        { "E9",      {  2,  0,  1,  0,  2,  0 }, {  3,  0,  1,  0,  2,  0 } },
        { "A9",      {  0,  0,  0,  2,  0, -1 }, {  0,  0,  0,  2,  0, -1 } },
        { "Cdim",    { -1,  1,  2,  1, -1, -1 }, { -1,  1,  3,  2, -1, -1 } },
        { "Caug",    { -1,  1,  1,  2,  3, -1 }, { -1,  1,  1,  2,  4, -1 } },
        { "D7sus4",  {  3,  1,  2,  0, -1, -1 }, {  4,  1,  2,  0, -1, -1 } },
        { "Am9",     {  0,  1,  0,  2,  0, -1 }, {  0,  1,  0,  2,  0, -1 } },
        { "Em9",     {  2,  0,  0,  0,  2,  0 }, {  2,  0,  0,  0,  1,  0 } },
        { "G6",      {  0,  0,  0,  0,  2,  3 }, {  0,  0,  0,  0,  1,  3 } },
        { "C6",      {  0,  1,  2,  2,  3, -1 }, {  0,  1,  2,  3,  4, -1 } }
    };

    constexpr int kLibrarySize = (int) (sizeof (kLibrary) / sizeof (kLibrary[0]));

    const char* const kPitchClassNames[12] =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    struct ChordFormula
    {
        const char* suffix;
        int intervals[6];
        int count;
    };

    // Ordered most-specific first, so "maj7" is matched before "maj".
    const ChordFormula kFormulas[] =
    {
        { "maj9",  { 0, 4, 7, 11, 14, 0 }, 5 },
        { "m9",    { 0, 3, 7, 10, 14, 0 }, 5 },
        { "9",     { 0, 4, 7, 10, 14, 0 }, 5 },
        { "m7b5",  { 0, 3, 6, 10,  0, 0 }, 4 },
        { "maj7",  { 0, 4, 7, 11,  0, 0 }, 4 },
        { "m7",    { 0, 3, 7, 10,  0, 0 }, 4 },
        { "7",     { 0, 4, 7, 10,  0, 0 }, 4 },
        { "6",     { 0, 4, 7,  9,  0, 0 }, 4 },
        { "m6",    { 0, 3, 7,  9,  0, 0 }, 4 },
        { "add9",  { 0, 4, 7, 14,  0, 0 }, 4 },
        { "dim7",  { 0, 3, 6,  9,  0, 0 }, 4 },
        { "sus2",  { 0, 2, 7,  0,  0, 0 }, 3 },
        { "sus4",  { 0, 5, 7,  0,  0, 0 }, 3 },
        { "dim",   { 0, 3, 6,  0,  0, 0 }, 3 },
        { "aug",   { 0, 4, 8,  0,  0, 0 }, 3 },
        { "m",     { 0, 3, 7,  0,  0, 0 }, 3 },
        { "",      { 0, 4, 7,  0,  0, 0 }, 3 },
        { "5",     { 0, 7, 0,  0,  0, 0 }, 2 }
    };

    constexpr int kNumFormulas = (int) (sizeof (kFormulas) / sizeof (kFormulas[0]));
}

//==============================================================================
void ChordVoicer::prepare (const TuningEngine* tuning, int strings) noexcept
{
    tuningEngine = tuning;
    numStrings = juce::jlimit (1, kMaxStrings, strings);
}

//==============================================================================
int ChordVoicer::findCandidates (int midiNote, Candidate* dest) const noexcept
{
    if (tuningEngine == nullptr || dest == nullptr)
        return 0;

    const double targetHz = midiToHz ((double) midiNote, tuningEngine->getConcertA());

    int count = 0;

    for (int s = 0; s < numStrings && count < kMaxCandidates; ++s)
    {
        const double fret = tuningEngine->frequencyToFretPosition (s, targetHz);
        const int rounded = (int) std::round (fret);

        // Allow a couple of cents of slop: an alternate tuning rarely lands on an
        // exact integer fret, and rejecting those would make whole tunings unplayable.
        if (std::abs (fret - (double) rounded) > 0.08)
            continue;

        if (rounded < 0 || rounded > maxFret)
            continue;

        if (rounded == 0 && ! allowOpen)
            continue;

        dest[count].stringIndex = s;
        dest[count].fret = rounded;
        ++count;
    }

    return count;
}

//==============================================================================
double ChordVoicer::scoreAssignment (const ChordVoicing& v) const noexcept
{
    if (v.numNotes == 0)
        return 1.0e9;

    // Fretted notes define the hand position; open strings cost the hand nothing.
    int lowest = 99, highest = -1;
    int frettedCount = 0;

    for (int i = 0; i < v.numNotes; ++i)
    {
        if (! v.notes[(size_t) i].valid)
            continue;

        const int f = (int) v.notes[(size_t) i].fretPosition;

        if (f <= 0)
            continue;

        lowest = juce::jmin (lowest, f);
        highest = juce::jmax (highest, f);
        ++frettedCount;
    }

    const int span = (frettedCount > 0) ? (highest - lowest) : 0;

    // Unreachable: reject outright.
    if (span > maxFretSpan)
        return 1.0e9;

    double score = 0.0;

    // A wide stretch is playable but tiring, so it is penalised, not forbidden.
    score += span * span * 3.0;

    // Prefer positions near where the hand already is.
    if (frettedCount > 0)
        score += std::abs (lowest - preferredPosition) * 1.2;

    // Prefer lower positions all else being equal: that is where the guitar
    // sounds fullest and where a player's hand rests.
    if (frettedCount > 0)
        score += lowest * 0.35;

    // Strongly prefer pitch order to follow string order. A voicing that puts a
    // low note on a high string and a high note on a low string is technically
    // possible and almost never what a guitarist plays.
    for (int i = 0; i < v.numNotes; ++i)
    {
        for (int j = i + 1; j < v.numNotes; ++j)
        {
            const auto& a = v.notes[(size_t) i];
            const auto& b = v.notes[(size_t) j];

            if (! a.valid || ! b.valid)
                continue;

            const bool aLowerString = a.stringIndex > b.stringIndex;
            const bool aLowerPitch = a.midiNote < b.midiNote;

            if (aLowerString != aLowerPitch)
                score += 22.0;
        }
    }

    // Penalise gaps: a muted string in the middle of a chord has to be damped by
    // the fretting hand, which is possible but awkward.
    int minString = 99, maxString = -1;

    for (int i = 0; i < v.numNotes; ++i)
        if (v.notes[(size_t) i].valid)
        {
            minString = juce::jmin (minString, v.notes[(size_t) i].stringIndex);
            maxString = juce::jmax (maxString, v.notes[(size_t) i].stringIndex);
        }

    if (maxString >= minString)
    {
        const int spanStrings = maxString - minString + 1;
        const int gaps = spanStrings - v.numNotes;
        score += juce::jmax (0, gaps) * 9.0;
    }

    // Every dropped note is a serious failure of the voicing.
    score += v.droppedNotes * 400.0;

    return score;
}

//==============================================================================
bool ChordVoicer::search (int noteIndex,
                          const int* sortedNotes,
                          const double* sortedVelocities,
                          int numNotes,
                          bool* stringUsed,
                          ChordVoicing& current,
                          ChordVoicing& best,
                          double& bestScore) noexcept
{
    if (noteIndex >= numNotes)
    {
        const double score = scoreAssignment (current);

        if (score < bestScore)
        {
            bestScore = score;
            best = current;
            best.playable = (score < 1.0e8);
        }

        return true;
    }

    Candidate candidates[kMaxCandidates];
    const int numCandidates = findCandidates (sortedNotes[noteIndex], candidates);

    bool placedAny = false;

    for (int c = 0; c < numCandidates; ++c)
    {
        const int s = candidates[c].stringIndex;

        if (stringUsed[s])
            continue;

        stringUsed[s] = true;

        auto& slot = current.notes[(size_t) current.numNotes];
        slot.midiNote = sortedNotes[noteIndex];
        slot.stringIndex = s;
        slot.fretPosition = (double) candidates[c].fret;
        slot.velocity = sortedVelocities[noteIndex];
        slot.valid = true;
        ++current.numNotes;

        // Prune: if the partial assignment already exceeds the hand's reach there
        // is no point completing it.
        if (scoreAssignment (current) < 1.0e8)
            placedAny |= search (noteIndex + 1, sortedNotes, sortedVelocities, numNotes,
                                 stringUsed, current, best, bestScore);

        --current.numNotes;
        current.notes[(size_t) current.numNotes].valid = false;
        stringUsed[s] = false;
    }

    if (! placedAny)
    {
        // This note cannot be placed anywhere. Drop it and carry on, so a chord
        // that is *nearly* playable still produces its playable part rather than
        // producing nothing.
        ++current.droppedNotes;
        placedAny = search (noteIndex + 1, sortedNotes, sortedVelocities, numNotes,
                            stringUsed, current, best, bestScore);
        --current.droppedNotes;
    }

    return placedAny;
}

//==============================================================================
ChordVoicing ChordVoicer::voice (const int* midiNotes, const double* velocities, int numNotes) noexcept
{
    ChordVoicing best;

    if (midiNotes == nullptr || numNotes <= 0 || tuningEngine == nullptr)
        return best;

    // Sort low to high and keep at most one note per string.
    int sorted[kMaxStrings];
    double sortedVel[kMaxStrings];

    const int n = juce::jmin (numNotes, numStrings);

    // Insertion sort by pitch, keeping the velocities with their notes. When there
    // are more notes than strings, the lowest and highest matter most musically,
    // so the inner voices are the ones dropped.
    int temp[kMaxStrings * 2];
    double tempVel[kMaxStrings * 2];
    const int copyCount = juce::jmin (numNotes, kMaxStrings * 2);

    for (int i = 0; i < copyCount; ++i)
    {
        temp[i] = midiNotes[i];
        tempVel[i] = (velocities != nullptr) ? velocities[i] : 0.8;
    }

    for (int i = 1; i < copyCount; ++i)
    {
        const int keyNote = temp[i];
        const double keyVel = tempVel[i];
        int j = i - 1;

        while (j >= 0 && temp[j] > keyNote)
        {
            temp[j + 1] = temp[j];
            tempVel[j + 1] = tempVel[j];
            --j;
        }

        temp[j + 1] = keyNote;
        tempVel[j + 1] = keyVel;
    }

    if (copyCount <= n)
    {
        for (int i = 0; i < copyCount; ++i)
        {
            sorted[i] = temp[i];
            sortedVel[i] = tempVel[i];
        }
    }
    else
    {
        // Keep the bass and the top, thin from the middle.
        int written = 0;
        sorted[written] = temp[0];
        sortedVel[written] = tempVel[0];
        ++written;

        const int inner = n - 2;
        for (int i = 0; i < inner; ++i)
        {
            const int src = 1 + (int) ((double) i * (double) (copyCount - 2) / (double) juce::jmax (1, inner));
            sorted[written] = temp[juce::jlimit (1, copyCount - 2, src)];
            sortedVel[written] = tempVel[juce::jlimit (1, copyCount - 2, src)];
            ++written;
        }

        sorted[written] = temp[copyCount - 1];
        sortedVel[written] = tempVel[copyCount - 1];
    }

    bool stringUsed[kMaxStrings] = {};
    ChordVoicing current;
    double bestScore = 1.0e18;

    search (0, sorted, sortedVel, n, stringUsed, current, best, bestScore);

    // Fill in the summary fields.
    int lowest = 99, highest = -1;

    for (int i = 0; i < best.numNotes; ++i)
    {
        if (! best.notes[(size_t) i].valid)
            continue;

        const int f = (int) best.notes[(size_t) i].fretPosition;

        if (f > 0)
        {
            lowest = juce::jmin (lowest, f);
            highest = juce::jmax (highest, f);
        }
    }

    if (highest >= 0)
    {
        best.lowestFret = lowest;
        best.highestFret = highest;
        best.fretSpan = highest - lowest;

        // A barre is needed when three or more notes sit on the lowest fret in use.
        int atLowest = 0;

        for (int i = 0; i < best.numNotes; ++i)
            if (best.notes[(size_t) i].valid && (int) best.notes[(size_t) i].fretPosition == lowest)
                ++atLowest;

        best.requiresBarre = (atLowest >= 3);

        // Remember where the hand ended up, so the next chord voices nearby.
        preferredPosition = lowest;
    }

    return best;
}

//==============================================================================
VoicedNote ChordVoicer::voiceSingleNote (int midiNote, double velocity, int preferStringIndex) noexcept
{
    VoicedNote result;
    result.midiNote = midiNote;
    result.velocity = velocity;

    if (tuningEngine == nullptr)
        return result;

    Candidate candidates[kMaxCandidates];
    const int count = findCandidates (midiNote, candidates);

    if (count == 0)
        return result;

    int bestIndex = 0;
    double bestCost = 1.0e9;

    for (int i = 0; i < count; ++i)
    {
        double cost = 0.0;

        // Stay near the hand's current position.
        cost += std::abs (candidates[i].fret - preferredPosition) * 1.0;

        // In mono mode, prefer the string the player was already on.
        if (preferStringIndex >= 0)
            cost += std::abs (candidates[i].stringIndex - preferStringIndex) * 2.5;

        // A note played high on a low string sounds thicker than the same note
        // open on a high string; guitarists lean slightly toward the lower string
        // for lead lines, so a small bias goes that way.
        cost -= candidates[i].stringIndex * 0.25;

        if (cost < bestCost)
        {
            bestCost = cost;
            bestIndex = i;
        }
    }

    result.stringIndex = candidates[bestIndex].stringIndex;
    result.fretPosition = (double) candidates[bestIndex].fret;
    result.valid = true;

    return result;
}

//==============================================================================
juce::String ChordVoicer::identifyChord (const int* midiNotes, int numNotes)
{
    if (midiNotes == nullptr || numNotes < 2)
        return {};

    // Collect the distinct pitch classes present.
    bool present[12] = {};
    int lowest = 128;

    for (int i = 0; i < numNotes; ++i)
    {
        present[((midiNotes[i] % 12) + 12) % 12] = true;
        lowest = juce::jmin (lowest, midiNotes[i]);
    }

    const int bassClass = ((lowest % 12) + 12) % 12;

    // Try every root, most-specific formula first.
    for (int f = 0; f < kNumFormulas; ++f)
    {
        const auto& formula = kFormulas[f];

        for (int root = 0; root < 12; ++root)
        {
            bool needed[12] = {};
            int neededCount = 0;

            for (int i = 0; i < formula.count; ++i)
            {
                const int pc = ((root + formula.intervals[i]) % 12 + 12) % 12;

                if (! needed[pc])
                {
                    needed[pc] = true;
                    ++neededCount;
                }
            }

            bool matches = true;
            int presentCount = 0;

            for (int pc = 0; pc < 12; ++pc)
            {
                if (present[pc])
                    ++presentCount;

                if (present[pc] != needed[pc])
                {
                    matches = false;
                    break;
                }
            }

            if (matches && presentCount == neededCount)
            {
                juce::String name = juce::String (kPitchClassNames[root]) + formula.suffix;

                // Name the inversion when the bass is not the root.
                if (bassClass != root)
                    name += "/" + juce::String (kPitchClassNames[bassClass]);

                return name;
            }
        }
    }

    return {};
}

int ChordVoicer::getNumLibraryChords() noexcept
{
    return kLibrarySize;
}

const ChordVoicer::LibraryChord& ChordVoicer::getLibraryChord (int index) noexcept
{
    return kLibrary[(size_t) juce::jlimit (0, kLibrarySize - 1, index)];
}

int ChordVoicer::searchLibrary (const juce::String& query, int* dest, int maxResults)
{
    if (dest == nullptr || maxResults <= 0)
        return 0;

    const auto lower = query.trim().toLowerCase();
    int found = 0;

    for (int i = 0; i < kLibrarySize && found < maxResults; ++i)
    {
        if (lower.isEmpty() || juce::String (kLibrary[i].name).toLowerCase().contains (lower))
            dest[found++] = i;
    }

    return found;
}

//==============================================================================
const char* getTechniqueName (Technique t) noexcept
{
    switch (t)
    {
        case Technique::Pluck:              return "Pluck";
        case Technique::HammerOn:           return "Hammer-On";
        case Technique::PullOff:            return "Pull-Off";
        case Technique::Slide:              return "Slide";
        case Technique::Bend:               return "Bend";
        case Technique::Vibrato:            return "Vibrato";
        case Technique::PalmMute:           return "Palm Mute";
        case Technique::MutedPick:          return "Muted Pick";
        case Technique::NaturalHarmonic:    return "Natural Harmonic";
        case Technique::PinchHarmonic:      return "Pinch Harmonic";
        case Technique::ArtificialHarmonic: return "Artificial Harmonic";
        case Technique::Tap:                return "Tap";
        case Technique::SlideGuitar:        return "Slide Guitar";
        case Technique::Strum:              return "Strum";
        case Technique::NumTechniques:
        default:                            return "Pluck";
    }
}

} // namespace luthier
