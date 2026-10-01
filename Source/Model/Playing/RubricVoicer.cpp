#include "RubricVoicer.h"

#include <cmath>

namespace luthier
{

namespace
{
    //==========================================================================
    // The weights in tenths. Every weight in 4.2 - 4.4 is a whole number of
    // tenths, so scoring in integers makes equal scores exactly equal, which
    // 4.6's tie-break needs: two doubles that should tie can differ in the last
    // bit depending on the order the terms were added in.
    constexpr int tenths (double weight) noexcept
    {
        return (int) (weight * 10.0 + (weight < 0.0 ? -0.5 : 0.5));
    }

    using W = RubricWeights;

    constexpr int kOpenBonus         = tenths (W::openBonus);
    constexpr int kHandMovePenalty   = tenths (W::handMovePenalty);
    constexpr int kBarrePenalty      = tenths (W::barrePenalty);
    constexpr int kMutePenalty       = tenths (W::mutePenalty);
    constexpr int kDupPenalty        = tenths (W::dupNotePenalty);
    constexpr int kDroppedPenalty    = tenths (W::extensionDroppedPenalty);

    constexpr int kOpenStyleOpen     = tenths (W::openStyleOpenString);
    constexpr int kOpenStyleRootLow  = tenths (W::openStyleRootLow);
    constexpr int kOpenStyleBarre    = tenths (W::openStyleBarre);
    constexpr int kBarreStyleBarre   = tenths (W::barreStyleBarre);
    constexpr int kBarreStyleRoot    = tenths (W::barreStyleRootLowest);
    constexpr int kTriadMatch        = tenths (W::triadMatch);
    constexpr int kTriadMiss         = tenths (W::triadMiss);
    constexpr int kShellMatch        = tenths (W::shellMatch);
    constexpr int kShellMiss         = tenths (W::shellMiss);
    constexpr int kDropMatch         = tenths (W::dropMatch);
    constexpr int kPowerMatch        = tenths (W::powerMatch);
    constexpr int kPowerMiss         = tenths (W::powerMiss);
    constexpr int kRootlessMatch     = tenths (W::rootlessMatch);
    constexpr int kRootlessMiss      = tenths (W::rootlessMiss);
    constexpr int kWideMatch         = tenths (W::wideMatch);
    constexpr int kBassMatch         = tenths (W::bassMatch);
    constexpr int kBassWalking       = tenths (W::bassWalkingApproach);
    constexpr int kBassWide          = tenths (W::bassWiderThanOctave);

    constexpr int kCommonNote        = tenths (W::commonNote);
    constexpr int kCommonPosition    = tenths (W::commonPosition);
    constexpr int kPositionJump      = tenths (W::positionJump);

    /** The slop ChordVoicer allows between a pitch and a fret: an alternate
        tuning rarely lands exactly on a fret, and rejecting those would make
        whole tunings unplayable. */
    constexpr double kFretSlop = 0.08;

    int pitchClass (int midi) noexcept          { return ((midi % 12) + 12) % 12; }
    uint16_t pcBit (int pc) noexcept            { return (uint16_t) (1u << pc); }
    bool hasPc (uint16_t mask, int pc) noexcept { return pc >= 0 && ((mask >> pc) & 1u) != 0; }

    int countBits (uint16_t m) noexcept
    {
        int n = 0;

        while (m != 0)
        {
            n += (int) (m & 1u);
            m = (uint16_t) (m >> 1);
        }

        return n;
    }

    bool hasPitch (const std::array<uint64_t, 2>& set, int pitch) noexcept
    {
        return pitch >= 0 && pitch < 128 && ((set[(size_t) (pitch >> 6)] >> (pitch & 63)) & 1u) != 0;
    }

    void addPitch (std::array<uint64_t, 2>& set, int pitch) noexcept
    {
        if (pitch >= 0 && pitch < 128)
            set[(size_t) (pitch >> 6)] |= (uint64_t) 1 << (pitch & 63);
    }
}

//==============================================================================
RubricVoicer::RubricVoicer() noexcept
{
    current.fill (-1);
    bestFrets.fill (-1);
    prevHand.fill (-1);
}

void RubricVoicer::prepare (const TuningEngine* tuning, int strings) noexcept
{
    tuningEngine = tuning;
    numStrings = juce::jlimit (1, kMaxStrings, strings);
    singleNotes.prepare (tuning, numStrings);
}

void RubricVoicer::setNumStrings (int n) noexcept
{
    numStrings = juce::jlimit (1, kMaxStrings, n);
    singleNotes.setNumStrings (numStrings);
}

void RubricVoicer::reset() noexcept
{
    handHint = 0;
    hasPrevious = false;
    previous = ChordVoicing {};
    singleNotes.reset();

    lastOutcome = RubricOutcome::none;
    lastScore = RubricScore {};
}

void RubricVoicer::setStyle (RubricStyle s) noexcept
{
    style = (s == RubricStyle::numStyles) ? RubricStyle::open : s;
}

void RubricVoicer::setRootPitchClass (int pc) noexcept
{
    rootOverride = (pc < 0) ? -1 : pitchClass (pc);
}

void RubricVoicer::setPreviousVoicing (const ChordVoicing& v) noexcept
{
    previous = v;
    hasPrevious = v.numNotes > 0;
}

//==============================================================================
int RubricVoicer::handFretFor (int slot, int relFret) const noexcept
{
    /*  Where the fretting hand is, measured from the capo on every string. With a
        full capo, or none, this is the fret position itself. Under a partial capo
        the strings it leaves open are measured from the nut (4.5), so they are
        brought onto the same scale here, or a span and a hand position would mix
        two rulers. */
    return relFret + slotCapo[(size_t) slot] - capoFret;
}

bool RubricVoicer::fretAllowed (int slot, int relFret) const noexcept
{
    // 4.1 constraint 1, and 4.5: nothing is fretted at or behind the capo, on
    // any string - including those a partial capo leaves open to the nut.
    if (relFret < 0 || relFret > slotHighest[(size_t) slot])
        return false;

    if (relFret == 0)
        return allowOpen;

    return handFretFor (slot, relFret) >= juce::jmax (1, minFret);
}

int RubricVoicer::findRequest (int pitch) const noexcept
{
    for (int r = 0; r < numRequest; ++r)
        if (request[(size_t) r] == pitch)
            return r;

    return -1;
}

int RubricVoicer::styleCap() const noexcept
{
    // The note budgets RhythmEngine gives these styles, so the mute penalty
    // compares like with like: a triad is three strings, and four muted strings
    // cost a triad nothing more than they cost any other triad.
    switch (style)
    {
        case RubricStyle::triad:
        case RubricStyle::shell:
        case RubricStyle::power:     return 3;
        case RubricStyle::drop2:
        case RubricStyle::drop3:     return 4;
        case RubricStyle::bass:      return 2;
        case RubricStyle::open:
        case RubricStyle::barre:
        case RubricStyle::rootless:
        case RubricStyle::wide:
        case RubricStyle::numStyles:
        default:                     return kMaxStrings;
    }
}

int RubricVoicer::styleMaxTenths() const noexcept
{
    // The most a style can add, for the search's bound.
    switch (style)
    {
        case RubricStyle::open:      return kOpenStyleOpen + kOpenStyleRootLow;
        case RubricStyle::barre:     return kBarreStyleBarre + kBarreStyleRoot;
        case RubricStyle::triad:     return kTriadMatch;
        case RubricStyle::shell:     return kShellMatch;
        case RubricStyle::drop2:
        case RubricStyle::drop3:     return kDropMatch;
        case RubricStyle::power:     return kPowerMatch;
        case RubricStyle::rootless:  return kRootlessMatch;
        case RubricStyle::wide:      return kWideMatch;
        case RubricStyle::bass:      return kBassMatch + kBassWalking;
        case RubricStyle::numStyles:
        default:                     return 0;
    }
}

//==============================================================================
void RubricVoicer::prepareSlots() noexcept
{
    /*  The tuning engine is the authority on how many strings there are; the
        voicer's own count can only lower it. A 12-string is six courses, and a
        finger stops both strings of a course, so it is voiced as six: each course
        is searched on its first string and the second follows it at the same fret
        (LuthierEngine::applyTwelveStringTuning is the only 12-string layout the
        engine builds). */
    effectiveStrings = juce::jmax (1, juce::jmin (numStrings, tuningEngine->getNumStrings()));
    coursed = (effectiveStrings == 12);
    numSlots = coursed ? 6 : effectiveStrings;
    capoFret = tuningEngine->getCapoFret();

    const double a4 = tuningEngine->getConcertA();

    for (int slot = 0; slot < numSlots; ++slot)
    {
        const int s = coursed ? slot * 2 : slot;

        slotString[(size_t) slot] = s;
        slotCapo[(size_t) slot] = tuningEngine->getCapoFretFor (s);

        // Two ceilings and the lower wins: maxFret is how far up the neck to
        // reach, and the tuning engine's playable span is the neck itself, which
        // a capo shortens (4.5).
        const int highest = juce::jlimit (0, kFretTableSize - 1,
                                          juce::jmin (maxFret, tuningEngine->getHighestPlayableFret (s)));
        slotHighest[(size_t) slot] = highest;

        // What each fret sounds, checked against the tuning engine the same way
        // ChordVoicer checks it, so a detuned string that sits between semitones
        // offers no notes rather than wrong ones.
        const int openPitch = (int) std::round (hzToMidi (tuningEngine->computeFrequency (s, 0.0), a4));
        auto& row = pitchAt[(size_t) slot];

        for (int f = 0; f < kFretTableSize; ++f)
        {
            row[(size_t) f] = -1;

            const int m = openPitch + f;

            if (f > highest || m < 0 || m > 127)
                continue;

            const double position = tuningEngine->frequencyToFretPosition (s, midiToHz ((double) m, a4));

            if (std::abs (position - (double) f) <= kFretSlop + tuningEngine->getMicroOffsetFrets (s))
                row[(size_t) f] = m;
        }
    }
}

//==============================================================================
bool RubricVoicer::prepareRequest (const int* midiNotes, const double* velocities, int count) noexcept
{
    numRequest = 0;
    unplaceable = 0;

    // Distinct pitches, low to high, velocities kept with their notes.
    for (int i = 0; i < count; ++i)
    {
        const int note = midiNotes[i];

        if (note < 0 || note > 127 || findRequest (note) >= 0)
            continue;

        if (numRequest == kMaxRequest)
            break;

        int j = numRequest;

        while (j > 0 && request[(size_t) (j - 1)] > note)
        {
            request[(size_t) j] = request[(size_t) (j - 1)];
            requestVelocity[(size_t) j] = requestVelocity[(size_t) (j - 1)];
            --j;
        }

        request[(size_t) j] = note;
        requestVelocity[(size_t) j] = (velocities != nullptr) ? velocities[i] : 0.8;
        ++numRequest;
    }

    if (numRequest == 0)
        return false;

    /*  Exact mode: a pitch no string can sound at all is not the instrument's to
        play, so it leaves the request here and counts as dropped. Leaving it in
        would make it the bass that constraint 3 insists on, and a chord with one
        note below the guitar's range would become unplayable in its entirety
        rather than playable without that note, which is what ChordVoicer did and
        what a player expects. */
    if (pitchMode == RubricPitchMode::exact)
    {
        int kept = 0;

        for (int r = 0; r < numRequest; ++r)
        {
            bool soundable = false;

            for (int slot = 0; slot < numSlots && ! soundable; ++slot)
                for (int f = 0; f <= slotHighest[(size_t) slot] && ! soundable; ++f)
                    soundable = pitchAt[(size_t) slot][(size_t) f] == request[(size_t) r] && fretAllowed (slot, f);

            if (soundable)
            {
                request[(size_t) kept] = request[(size_t) r];
                requestVelocity[(size_t) kept] = requestVelocity[(size_t) r];
                ++kept;
            }
            else
            {
                ++unplaceable;
            }
        }

        numRequest = kept;

        if (numRequest == 0)
        {
            keptMask = 0;
            return true;
        }
    }

    // ---- the chord's degrees -----------------------------------------------------
    uint16_t chordMask = 0;

    for (int r = 0; r < numRequest; ++r)
        chordMask = (uint16_t) (chordMask | pcBit (pitchClass (request[(size_t) r])));

    rootPc = (rootOverride >= 0) ? rootOverride : pitchClass (request[0]);

    // A chord symbol's root is a chord tone whether or not the player held it.
    if (pitchMode == RubricPitchMode::chordTones)
        chordMask = (uint16_t) (chordMask | pcBit (rootPc));

    auto degree = [this] (int semitones) { return (rootPc + semitones) % 12; };
    auto inChord = [&] (int semitones) { return hasPc (chordMask, degree (semitones)); };

    fifthPc   = inChord (7) ? degree (7) : inChord (6) ? degree (6) : inChord (8) ? degree (8) : -1;
    thirdPc   = inChord (4) ? degree (4) : inChord (3) ? degree (3) : -1;
    seventhPc = inChord (10) ? degree (10) : inChord (11) ? degree (11)
              : (inChord (9) && inChord (6)) ? degree (9) : -1;   // dim7's bb7

    /*  4.3's styles that are defined by what they leave out. Leaving those notes
        out is the style, not a loss, so they are removed from the chord here and
        never cost extension_dropped_penalty: Power would otherwise pay 3 for
        every third it correctly omits, which is most of its own +8. */
    const uint16_t rootBit = pcBit (rootPc);

    switch (style)
    {
        case RubricStyle::power:
            keptMask = (uint16_t) (rootBit | (fifthPc >= 0 ? pcBit (fifthPc) : 0));
            break;

        case RubricStyle::shell:
            keptMask = (uint16_t) (rootBit | (thirdPc >= 0 ? pcBit (thirdPc) : 0)
                                           | (seventhPc >= 0 ? pcBit (seventhPc) : 0));
            break;

        case RubricStyle::rootless:
            keptMask = (uint16_t) (chordMask & ~rootBit);

            if (keptMask == 0)
                keptMask = chordMask;   // a lone root has nothing else to play

            break;

        case RubricStyle::bass:
            keptMask = (uint16_t) (rootBit | (bassPattern == RubricBassPattern::rootFifth && fifthPc >= 0
                                                ? pcBit (fifthPc) : 0));
            break;

        case RubricStyle::open:
        case RubricStyle::barre:
        case RubricStyle::triad:
        case RubricStyle::drop2:
        case RubricStyle::drop3:
        case RubricStyle::wide:
        case RubricStyle::numStyles:
        default:
            keptMask = chordMask;
            break;
    }

    keptMask = (uint16_t) (keptMask & chordMask);

    /*  4.2's dropped_extensions: the chord tones the voicing leaves out. The
        perfect fifth is exempt, as it is from ChordDetector's templates: it adds
        no colour, and a guitarist drops it whenever the hand needs the finger. */
    countedMask = (uint16_t) (keptMask & ~pcBit (degree (7)));

    // Exact mode places only the notes that survived the style.
    if (pitchMode == RubricPitchMode::exact)
    {
        int kept = 0;

        for (int r = 0; r < numRequest; ++r)
        {
            if (hasPc (keptMask, pitchClass (request[(size_t) r])))
            {
                request[(size_t) kept] = request[(size_t) r];
                requestVelocity[(size_t) kept] = requestVelocity[(size_t) r];
                ++kept;
            }
        }

        numRequest = kept;
    }

    // Constraint 3's "bass": the lowest note given that the style still plays.
    bassPc = rootPc;

    for (int r = 0; r < numRequest; ++r)
    {
        if (hasPc (keptMask, pitchClass (request[(size_t) r])))
        {
            bassPc = pitchClass (request[(size_t) r]);
            break;
        }
    }

    soundingCap = juce::jmin (styleCap(), maxSounding, numSlots);
    return true;
}

//==============================================================================
void RubricVoicer::preparePrevious() noexcept
{
    prevHand.fill (-1);
    prevPitches = { 0, 0 };
    prevLowestPitch = -1;

    if (! hasPrevious)
        return;

    for (int i = 0; i < previous.numNotes; ++i)
    {
        const auto& note = previous.notes[(size_t) i];

        if (! note.valid)
            continue;

        addPitch (prevPitches, note.midiNote);

        if (prevLowestPitch < 0 || note.midiNote < prevLowestPitch)
            prevLowestPitch = note.midiNote;

        const int s = note.stringIndex;
        const int slot = coursed ? ((s % 2 == 0) ? s / 2 : -1) : s;

        if (slot < 0 || slot >= numSlots)
            continue;

        const int rel = (int) std::round (note.fretPosition);
        prevHand[(size_t) slot] = (rel > 0) ? juce::jmax (1, handFretFor (slot, rel)) : 0;
    }
}

int RubricVoicer::transitionGain (int slot, int relFret, int handFret, int pitch) const noexcept
{
    /*  4.4, per string. "Common finger position (approximated by fret
        proximity)" is read as the same string fretted within a fret of where it
        was, and "a position jump > 5 frets on any finger" as the same string
        fretted more than five frets away. A string that was open or muted had no
        finger on it, so it can neither keep nor jump one. */
    if (! hasPrevious || relFret < 0)
        return 0;

    int gain = hasPitch (prevPitches, pitch) ? kCommonNote : 0;

    if (relFret > 0 && prevHand[(size_t) slot] > 0)
    {
        const int distance = std::abs (handFret - prevHand[(size_t) slot]);

        if (distance <= W::commonPositionFrets)
            gain += kCommonPosition;
        else if (distance > W::positionJumpFrets)
            gain += kPositionJump;
    }

    return gain;
}

//==============================================================================
void RubricVoicer::buildOptions() noexcept
{
    maxSoundingGain = -kMutePenalty;
    suffixMaxGain[0] = 0;

    for (int slot = 0; slot < numSlots; ++slot)
    {
        auto& row = options[(size_t) slot];
        int n = 0;
        int maxGain = -kMutePenalty;

        for (int f = 0; f <= slotHighest[(size_t) slot]; ++f)
        {
            const int pitch = pitchAt[(size_t) slot][(size_t) f];

            if (pitch < 0 || ! fretAllowed (slot, f))
                continue;

            int requestIndex = -1;

            if (pitchMode == RubricPitchMode::chordTones)
            {
                if (! hasPc (keptMask, pitchClass (pitch)))
                    continue;
            }
            else
            {
                requestIndex = findRequest (pitch);

                if (requestIndex < 0)
                    continue;
            }

            Option o;
            o.relFret = f;
            o.handFret = (f > 0) ? handFretFor (slot, f) : 0;
            o.pitch = pitch;
            o.requestIndex = requestIndex;
            o.gain = (f == 0 ? kOpenBonus : 0) + transitionGain (slot, f, o.handFret, pitch);

            // Search order: open strings, then frets nearest the hand. Good
            // fingerings found early let the bound cut more of the rest.
            o.order = (f == 0) ? 0 : 1 + std::abs (o.handFret - handHint) * 64 + o.handFret;

            int j = n;

            while (j > 0 && row[(size_t) (j - 1)].order > o.order)
            {
                row[(size_t) j] = row[(size_t) (j - 1)];
                --j;
            }

            row[(size_t) j] = o;
            ++n;

            maxGain = juce::jmax (maxGain, o.gain);
            maxSoundingGain = juce::jmax (maxSoundingGain, o.gain);
        }

        // Muting is always possible, and is tried last.
        Option mute;
        mute.gain = -kMutePenalty;
        row[(size_t) n++] = mute;

        numOptions[(size_t) slot] = n;
        suffixMaxGain[(size_t) slot + 1] = suffixMaxGain[(size_t) slot] + maxGain;
    }
}

//==============================================================================
bool RubricVoicer::matchesDrop (const int* fret, uint16_t pcMask, int dropFromTop) const noexcept
{
    /*  4.3's "canonical drop pattern": four different notes, one per string in
        pitch order, that become a close voicing - all four inside an octave - when
        the lowest is raised an octave, and where the raised note lands second
        (drop 2) or third (drop 3) from the top. The strings are the canonical
        ones too: drop 2 on four adjacent strings, drop 3 with the bass a string
        below three adjacent ones. */
    if (countBits (pcMask) != 4)
        return false;

    int slots[4] = {};
    int pitches[4] = {};
    int n = 0;

    for (int slot = numSlots - 1; slot >= 0; --slot)
    {
        if (fret[slot] < 0)
            continue;

        if (n == 4)
            return false;

        slots[n] = slot;
        pitches[n] = pitchAt[(size_t) slot][(size_t) fret[slot]];
        ++n;
    }

    if (n != 4)
        return false;

    for (int i = 1; i < 4; ++i)
        if (pitches[i] <= pitches[i - 1])
            return false;

    const int raised = pitches[0] + 12;

    if (pitches[3] - pitches[1] >= 12)
        return false;

    if (dropFromTop == 2)
        return slots[0] - slots[3] == 3 && pitches[2] < raised && raised < pitches[3];

    return slots[0] - slots[1] == 2 && slots[1] - slots[3] == 2
        && pitches[1] < raised && raised < pitches[2];
}

int RubricVoicer::styleBias (const int* fret, const RubricScore& s, int lowestSlot, int lowestPitch,
                             int highestPitch, uint16_t pcMask, const int* pcCount) const noexcept
{
    const int bassStringPc = pitchClass (pitchAt[(size_t) lowestSlot][(size_t) fret[lowestSlot]]);
    const bool rootOnBassString = (bassStringPc == rootPc);
    const uint16_t rootBit = pcBit (rootPc);

    switch (style)
    {
        case RubricStyle::open:
            // "Root on string 5 / 6": the two lowest strings, whatever the count.
            return (s.openStrings > 0 ? kOpenStyleOpen : 0)
                 + (rootOnBassString && lowestSlot >= numSlots - 2 ? kOpenStyleRootLow : 0)
                 + (s.usesBarre ? kOpenStyleBarre : 0);

        case RubricStyle::barre:
            // "Root on string 6": the lowest string.
            return (s.usesBarre ? kBarreStyleBarre : 0)
                 + (rootOnBassString && lowestSlot == numSlots - 1 ? kBarreStyleRoot : 0);

        case RubricStyle::triad:
        {
            const bool topThree = s.soundingStrings == 3 && numSlots >= 3
                               && fret[0] >= 0 && fret[1] >= 0 && fret[2] >= 0;
            return topThree ? kTriadMatch : kTriadMiss;
        }

        case RubricStyle::shell:
        {
            const bool shell = thirdPc >= 0 && seventhPc >= 0
                            && pcMask == (uint16_t) (rootBit | pcBit (thirdPc) | pcBit (seventhPc));
            return shell ? kShellMatch : kShellMiss;
        }

        case RubricStyle::drop2:
            return matchesDrop (fret, pcMask, 2) ? kDropMatch : 0;

        case RubricStyle::drop3:
            return matchesDrop (fret, pcMask, 3) ? kDropMatch : 0;

        case RubricStyle::power:
        {
            // Root and fifth, the root perhaps doubled at the octave, nothing else.
            const bool power = fifthPc >= 0
                            && pcMask == (uint16_t) (rootBit | pcBit (fifthPc))
                            && pcCount[fifthPc] == 1 && pcCount[rootPc] <= 2;
            return power ? kPowerMatch : kPowerMiss;
        }

        case RubricStyle::rootless:
            return hasPc (pcMask, rootPc) ? kRootlessMiss : kRootlessMatch;

        case RubricStyle::wide:
            return s.mutedStrings == 0 ? kWideMatch : 0;

        case RubricStyle::bass:
        {
            int bias = 0;

            // "Root only" is one note: the root doubled at the octave is two, and
            // would otherwise beat the single root on the mute penalty alone.
            const bool rootOnly = s.soundingStrings == 1 && pcMask == rootBit;
            const bool rootFifth = s.soundingStrings == 2 && fifthPc >= 0
                                && pcMask == (uint16_t) (rootBit | pcBit (fifthPc));

            if (bassPattern == RubricBassPattern::rootFifth ? rootFifth : rootOnly)
                bias += kBassMatch;

            // A walking bass arrives by step: the lowest note one or two
            // semitones from the last chord's lowest note.
            if (hasPrevious && prevLowestPitch >= 0)
            {
                const int step = std::abs (lowestPitch - prevLowestPitch);

                if (step >= 1 && step <= 2)
                    bias += kBassWalking;
            }

            if (highestPitch - lowestPitch > 12)
                bias += kBassWide;

            return bias;
        }

        case RubricStyle::numStyles:
        default:
            return 0;
    }
}

//==============================================================================
bool RubricVoicer::scoreCandidate (const int* fret, RubricScore& s) const noexcept
{
    s = RubricScore {};

    auto fail = [&s] (int which)
    {
        s.valid = false;
        s.failedConstraint = which;
        return false;
    };

    int pcCount[12] = {};
    uint16_t pcMask = 0;
    uint32_t used = 0;
    int minHand = 1000, maxHand = -1;
    int lowestSlot = -1, lowestPitch = 1000, highestPitch = -1;
    std::array<uint64_t, 2> commonCounted { 0, 0 };

    // ---- 4.1 constraint 1, and whether this fingering voices the request at all
    for (int slot = 0; slot < numSlots; ++slot)
    {
        const int f = fret[slot];

        if (f < 0)
        {
            ++s.mutedStrings;
            continue;
        }

        if (! fretAllowed (slot, f))
            return fail (1);

        const int pitch = pitchAt[(size_t) slot][(size_t) f];

        if (pitch < 0)
            return fail (RubricScore::kNotThisChord);

        const int pc = pitchClass (pitch);

        if (pitchMode == RubricPitchMode::chordTones)
        {
            if (! hasPc (keptMask, pc))
                return fail (RubricScore::kNotThisChord);
        }
        else
        {
            const int r = findRequest (pitch);

            if (r < 0 || ((used >> r) & 1u) != 0)
                return fail (RubricScore::kNotThisChord);

            used |= (1u << r);
        }

        ++s.soundingStrings;
        ++pcCount[pc];
        pcMask = (uint16_t) (pcMask | pcBit (pc));

        lowestSlot = slot;   // slots rise in index as the strings fall in pitch
        lowestPitch = juce::jmin (lowestPitch, pitch);
        highestPitch = juce::jmax (highestPitch, pitch);

        if (f == 0)
        {
            ++s.openStrings;
        }
        else
        {
            const int hand = handFretFor (slot, f);

            ++s.frettedStrings;
            s.fretSum += hand;
            minHand = juce::jmin (minHand, hand);
            maxHand = juce::jmax (maxHand, hand);

            if (hasPrevious && prevHand[(size_t) slot] > 0)
            {
                const int distance = std::abs (hand - prevHand[(size_t) slot]);

                if (distance <= W::commonPositionFrets)
                    ++s.commonPositions;
                else if (distance > W::positionJumpFrets)
                    ++s.positionJumps;
            }
        }

        // "+2 per common note": a pitch the last chord also sounded, counted once
        // however many strings sound it now.
        if (hasPrevious && hasPitch (prevPitches, pitch) && ! hasPitch (commonCounted, pitch))
        {
            ++s.commonNotes;
            addPitch (commonCounted, pitch);
        }
    }

    if (s.soundingStrings == 0 || s.soundingStrings > soundingCap)
        return fail (RubricScore::kNotThisChord);

    // ---- 4.1 constraint 2: the hand's span, open strings excepted -----------------
    if (s.frettedStrings > 0 && maxHand - minHand > maxFretSpan)
        return fail (2);

    // ---- 4.1 constraint 3: root or bass on the lowest sounding string -------------
    {
        const int pc = pitchClass (pitchAt[(size_t) lowestSlot][(size_t) fret[lowestSlot]]);

        if (pc != rootPc && pc != bassPc)
            return fail (3);
    }

    // ---- 4.1 constraint 4: no adjacent sounding strings inverted by more than 4 ----
    {
        int below = -1;

        for (int slot = numSlots - 1; slot >= 0; --slot)
        {
            if (fret[slot] < 0)
                continue;

            const int pitch = pitchAt[(size_t) slot][(size_t) fret[slot]];

            if (below >= 0 && pitch < below - W::maxInversionSemitones)
                return fail (4);

            below = pitch;
        }
    }

    // ---- 4.1 constraints 5 and 6: the barre -----------------------------------------
    /*  A barre is used when there are more fretted notes than fingers. It lies at
        the lowest fretted fret, across every string from the lowest to the
        highest one sounding that fret. Constraint 5 wants at least three strings
        under it. Constraint 6, that everything is reachable from it, is read as:
        every string under the barre is stopped at or above it (the barre would
        fret an open string, and a muted one would ring), and the notes above the
        barre fit the three fingers left. */
    if (s.frettedStrings > W::fingers)
    {
        const int barreFret = minHand;
        int lo = numSlots, hi = -1;

        for (int slot = 0; slot < numSlots; ++slot)
        {
            if (fret[slot] > 0 && handFretFor (slot, fret[slot]) == barreFret)
            {
                lo = juce::jmin (lo, slot);
                hi = juce::jmax (hi, slot);
            }
        }

        if (hi - lo + 1 < W::minBarreStrings)
            return fail (5);

        int above = 0;

        for (int slot = 0; slot < numSlots; ++slot)
        {
            if (slot >= lo && slot <= hi && fret[slot] <= 0)
                return fail (6);

            if (fret[slot] > 0 && handFretFor (slot, fret[slot]) > barreFret)
                ++above;
        }

        if (above > W::fingers - 1)
            return fail (6);

        s.usesBarre = true;
        s.barreFret = barreFret;
    }

    // ---- 4.2 --------------------------------------------------------------------------
    s.handPosition = (s.frettedStrings > 0) ? minHand : 0;
    s.duplicates = s.soundingStrings - countBits (pcMask);
    s.droppedExtensions = countBits ((uint16_t) (countedMask & ~pcMask));

    const int openT = kOpenBonus * s.openStrings;
    const int styleT = styleBias (fret, s, lowestSlot, lowestPitch, highestPitch, pcMask, pcCount);
    const int transitionT = kCommonNote * s.commonNotes + kCommonPosition * s.commonPositions
                          + kPositionJump * s.positionJumps;

    // The hint is where the hand is; with nothing fretted it need not move.
    const int handT = (s.frettedStrings > 0) ? -kHandMovePenalty * std::abs (s.handPosition - handHint) : 0;
    const int barreT = s.usesBarre ? -kBarrePenalty : 0;
    const int muteT = -kMutePenalty * s.mutedStrings;
    const int dupT = -kDupPenalty * s.duplicates;
    const int droppedT = -kDroppedPenalty * s.droppedExtensions;

    s.totalTenths = openT + styleT + transitionT + handT + barreT + muteT + dupT + droppedT;

    s.openTerm       = openT / 10.0;
    s.styleTerm      = styleT / 10.0;
    s.transitionTerm = transitionT / 10.0;
    s.handMoveTerm   = handT / 10.0;
    s.barreTerm      = barreT / 10.0;
    s.muteTerm       = muteT / 10.0;
    s.duplicateTerm  = dupT / 10.0;
    s.droppedTerm    = droppedT / 10.0;
    s.total          = s.totalTenths / 10.0;

    s.valid = true;
    return true;
}

//==============================================================================
bool RubricVoicer::ranksAbove (const RankKey& a, const RankKey& b) noexcept
{
    // 4.6: score, then more strings, then the lower fret sum.
    if (a.scoreTenths != b.scoreTenths)
        return a.scoreTenths > b.scoreTenths;

    if (a.strings != b.strings)
        return a.strings > b.strings;

    if (a.fretSum != b.fretSum)
        return a.fretSum < b.fretSum;

    // Beyond the spec, so the result never depends on search order.
    const int n = juce::jmin (a.numSlots, b.numSlots);

    for (int i = 0; i < n; ++i)
        if (a.frets[(size_t) i] != b.frets[(size_t) i])
            return a.frets[(size_t) i] < b.frets[(size_t) i];

    return false;
}

//==============================================================================
int RubricVoicer::upperBound (int slotsLeft) const noexcept
{
    /*  The most any completion of the current partial fingering could score. It
        must never be too low, or the search would cut the best answer; it may be
        too high, which only costs time. Each remaining string is credited with
        its best option, the style with its best case, and only the penalties
        that can no longer be avoided are charged. */
    int bound = state.gain + styleMaxTenths();

    int rest = suffixMaxGain[(size_t) slotsLeft];
    const int soundLeft = juce::jmax (0, soundingCap - state.count);

    // Strings past the sounding cap have to be muted.
    if (soundLeft < slotsLeft)
        rest = juce::jmin (rest, soundLeft * maxSoundingGain - (slotsLeft - soundLeft) * kMutePenalty);

    bound += rest;
    bound -= kDupPenalty * state.dups;

    // Chord tones still missing beyond what the remaining strings could add.
    const int unsounded = countBits ((uint16_t) (countedMask & ~state.pcMask));
    bound -= kDroppedPenalty * juce::jmax (0, unsounded - juce::jmin (slotsLeft, soundLeft));

    // The hand can only move down from the lowest fret so far, and no further
    // than the span allows below the highest.
    if (state.fretted > 0)
    {
        int distance = 0;

        if (handHint > state.minHand)
            distance = handHint - state.minHand;
        else if (state.maxHand - maxFretSpan > handHint)
            distance = state.maxHand - maxFretSpan - handHint;

        bound -= kHandMovePenalty * distance;
    }

    return bound;
}

void RubricVoicer::considerLeaf() noexcept
{
    ++leavesScored;

    RubricScore s;

    if (! scoreCandidate (current.data(), s))
        return;

    RankKey key;
    key.scoreTenths = s.totalTenths;
    key.strings = s.soundingStrings;
    key.fretSum = s.fretSum;
    key.numSlots = numSlots;

    for (int slot = 0; slot < numSlots; ++slot)
        key.frets[(size_t) slot] = current[(size_t) slot];

    if (! haveBest || ranksAbove (key, best))
    {
        best = key;
        bestFrets = current;
        haveBest = true;
    }
}

void RubricVoicer::search (int depth) noexcept
{
    if (depth == numSlots)
    {
        considerLeaf();
        return;
    }

    // The lowest string first, so constraint 3 (what sounds lowest) and
    // constraint 4 (each string against the one below) are settled as it goes.
    const int slot = numSlots - 1 - depth;
    const auto& row = options[(size_t) slot];

    for (int o = 0; o < numOptions[(size_t) slot]; ++o)
    {
        if (nodesVisited >= kMaxSearchNodes || leavesScored >= kMaxScoredCandidates)
        {
            truncated = true;
            break;
        }

        ++nodesVisited;

        const auto& opt = row[(size_t) o];
        const SearchState saved = state;

        if (opt.relFret >= 0)
        {
            if (state.count >= soundingCap)
                continue;

            if (opt.requestIndex >= 0 && ((state.used >> opt.requestIndex) & 1u) != 0)
                continue;

            const int pc = pitchClass (opt.pitch);

            if (state.count == 0)
            {
                if (pc != rootPc && pc != bassPc)                                  // 4.1.3
                    continue;
            }
            else if (opt.pitch < state.lastPitch - W::maxInversionSemitones)       // 4.1.4
            {
                continue;
            }

            if (opt.relFret > 0)
            {
                const int lo = juce::jmin (state.minHand, opt.handFret);
                const int hi = juce::jmax (state.maxHand, opt.handFret);

                if (hi - lo > maxFretSpan)                                         // 4.1.2
                    continue;

                state.minHand = lo;
                state.maxHand = hi;
                ++state.fretted;
            }

            if (hasPc (state.pcMask, pc))
                ++state.dups;

            state.pcMask = (uint16_t) (state.pcMask | pcBit (pc));

            if (opt.requestIndex >= 0)
                state.used |= (1u << opt.requestIndex);

            state.lastPitch = opt.pitch;
            ++state.count;
        }

        state.gain += opt.gain;
        current[(size_t) slot] = opt.relFret;

        if (! haveBest || upperBound (slot) >= best.scoreTenths)
            search (depth + 1);

        state = saved;
    }

    current[(size_t) slot] = -1;
}

//==============================================================================
int RubricVoicer::pairPitch (int stringIndex, int relFret) const noexcept
{
    // The second string of a 12-string course, stopped by the same finger.
    if (relFret > juce::jmin (maxFret, tuningEngine->getHighestPlayableFret (stringIndex)))
        return -1;

    const double a4 = tuningEngine->getConcertA();
    const int m = (int) std::round (hzToMidi (tuningEngine->computeFrequency (stringIndex, 0.0), a4)) + relFret;

    if (m < 0 || m > 127)
        return -1;

    const double position = tuningEngine->frequencyToFretPosition (stringIndex, midiToHz ((double) m, a4));
    return std::abs (position - (double) relFret) <= kFretSlop + tuningEngine->getMicroOffsetFrets (stringIndex) ? m : -1;
}

double RubricVoicer::velocityFor (int pitch) const noexcept
{
    const int exact = findRequest (pitch);

    if (exact >= 0)
        return requestVelocity[(size_t) exact];

    // A chord tone voiced in another octave takes the velocity it was held at.
    for (int r = 0; r < numRequest; ++r)
        if (pitchClass (request[(size_t) r]) == pitchClass (pitch))
            return requestVelocity[(size_t) r];

    return 0.8;
}

//==============================================================================
ChordVoicing RubricVoicer::voice (const int* midiNotes, const double* velocities, int numNotes) noexcept
{
    ChordVoicing result;

    lastScore = RubricScore {};
    lastOutcome = RubricOutcome::none;
    nodesVisited = 0;
    leavesScored = 0;
    truncated = false;

    if (tuningEngine == nullptr || midiNotes == nullptr || numNotes <= 0)
        return result;

    /*  A lone note played is not a chord, and 4 is the chord-fingering rubric:
        scored by it, a single note goes wherever the style bias and the
        tie-breaks send it (5 muted strings cost every candidate the same),
        not where the hand is. It is placed as it always was, by the single-note
        voicer, and still becomes the voicing the next chord moves from. */
    if (numNotes == 1 && pitchMode == RubricPitchMode::exact)
    {
        singleNotes.setMaxFret (maxFret);
        singleNotes.setMinFret (minFret);
        singleNotes.setAllowOpenStrings (allowOpen);
        singleNotes.setPreferredPosition (handHint);

        result = singleNotes.voice (midiNotes, velocities, 1);
        lastOutcome = result.numNotes > 0 ? RubricOutcome::voiced : RubricOutcome::unplayable;

        if (result.numNotes > 0)
        {
            previous = result;
            hasPrevious = true;

            if (result.notes[0].fretPosition > 0.0)
                handHint = juce::jlimit (0, 24, (int) std::lround (result.notes[0].fretPosition));
        }

        return result;
    }

    prepareSlots();

    if (! prepareRequest (midiNotes, velocities, numNotes))
        return result;

    // From here, notes were asked for: the answer is a voicing or 4.7's explicit
    // "unplayable", never an empty result that could be mistaken for silence.
    lastOutcome = RubricOutcome::unplayable;

    if (numRequest == 0 || keptMask == 0)
        return result;

    preparePrevious();
    buildOptions();

    state = SearchState {};
    current.fill (-1);
    bestFrets.fill (-1);
    haveBest = false;

    search (0);

    if (! haveBest)
        return result;

    scoreCandidate (bestFrets.data(), lastScore);
    lastOutcome = RubricOutcome::voiced;

    // ---- the voicing, lowest string first ----------------------------------------------
    auto add = [this, &result] (int pitch, int stringIndex, int fret)
    {
        if (result.numNotes >= ChordVoicing::kMaxNotes)
            return;

        auto& note = result.notes[(size_t) result.numNotes++];
        note.midiNote = pitch;
        note.stringIndex = stringIndex;
        note.fretPosition = (double) fret;
        note.velocity = velocityFor (pitch);
        note.valid = true;
    };

    uint16_t sounded = 0;
    int placed = 0;
    int lowest = 99, highest = -1;

    for (int slot = numSlots - 1; slot >= 0; --slot)
    {
        const int f = bestFrets[(size_t) slot];

        if (f < 0)
            continue;

        const int s = slotString[(size_t) slot];
        const int pitch = pitchAt[(size_t) slot][(size_t) f];

        add (pitch, s, f);
        sounded = (uint16_t) (sounded | pcBit (pitchClass (pitch)));
        ++placed;

        if (f > 0)
        {
            lowest = juce::jmin (lowest, f);
            highest = juce::jmax (highest, f);
        }

        if (coursed && s + 1 < effectiveStrings)
        {
            const int pair = pairPitch (s + 1, f);

            if (pair >= 0)
                add (pair, s + 1, f);
        }
    }

    if (highest >= 0)
    {
        result.lowestFret = lowest;
        result.highestFret = highest;
        result.fretSpan = highest - lowest;
    }

    result.requiresBarre = lastScore.usesBarre;
    result.playable = true;
    result.droppedNotes = (pitchMode == RubricPitchMode::exact)
                            ? (numRequest - placed) + unplaceable
                            : countBits ((uint16_t) (keptMask & ~sounded));

    // 4.4 measures the next chord against this one, and the hand stays here.
    previous = result;
    hasPrevious = true;

    if (lastScore.frettedStrings > 0)
        handHint = juce::jlimit (0, 24, lastScore.handPosition);

    return result;
}

//==============================================================================
VoicedNote RubricVoicer::voiceSingleNote (int midiNote, double velocity, int preferStringIndex) noexcept
{
    singleNotes.setMaxFret (maxFret);
    singleNotes.setMinFret (minFret);
    singleNotes.setAllowOpenStrings (allowOpen);
    singleNotes.setPreferredPosition (handHint);

    return singleNotes.voiceSingleNote (midiNote, velocity, preferStringIndex);
}

//==============================================================================
RubricScore RubricVoicer::evaluate (const int* fretPerString, const int* midiNotes, int numNotes) noexcept
{
    RubricScore s;
    s.failedConstraint = RubricScore::kNotThisChord;

    if (tuningEngine == nullptr || fretPerString == nullptr || midiNotes == nullptr || numNotes <= 0)
        return s;

    prepareSlots();

    if (! prepareRequest (midiNotes, nullptr, numNotes) || numRequest == 0 || keptMask == 0)
        return s;

    preparePrevious();

    int frets[kMaxStrings];

    for (int slot = 0; slot < numSlots; ++slot)
        frets[slot] = fretPerString[slotString[(size_t) slot]];

    scoreCandidate (frets, s);
    return s;
}

} // namespace luthier
