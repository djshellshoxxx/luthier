/*  REALISM-B: the engine's side of harmonic-realism.md, string-interaction.md
    and fingerstyle-attack.md.

    Kept out of LuthierEngine.cpp so the hub file carries only the calls into
    it. Everything here runs on the audio thread at note-on or block rate and
    allocates nothing.
*/

#include "LuthierEngine.h"

namespace luthier
{

//==============================================================================
void LuthierEngine::setStringInteraction (const StringInteractionSettings& s) noexcept
{
    interaction = s;
    refreshAirCoupling();
    midi.setMutedThumpLevel (s.mutedThumpLevel);
    rhythm.setMutedThumpLevel (s.mutedThumpLevel);
}

void LuthierEngine::refreshAirCoupling() noexcept
{
    // string-interaction.md 1: a_air = coupling_air_amount x a_cat.
    const bool acoustic = spec.category == GuitarCategory::Acoustic;
    const bool bass = spec.category == GuitarCategory::Bass;
    const bool semiHollow = guitarType == GuitarType::ES335;

    coupling.setAirCoefficient (juce::jlimit (0.0, 1.0, interaction.airAmount)
                                * CouplingMatrix::airCategoryCoefficient (acoustic, semiHollow, bass));
}

void LuthierEngine::resetRealismB() noexcept
{
    for (auto& b : borrowed)
        b = BorrowedDamping {};

    palmCentre = -1.0;
    palmGroupStart = -1000000;
    palmGroupSum = 0.0;
    palmGroupCount = 0;
    struckThisBlock = 0;
    alternationPhase = 0;
    lastFingerNoteSample = -1000000;
    lastNoteOnSample = -1000000;
    lastNoteOnString = -1;
    crosstalkWasBent = true;   // so the next block writes the unbent gains

    for (int s = 0; s < kMaxStrings; ++s)
    {
        contactDisplay[(size_t) s].fret.store (-1.0f, std::memory_order_relaxed);
        contactDisplay[(size_t) s].life.store (0.0f, std::memory_order_relaxed);
        contactDisplaySamples[(size_t) s] = 0;
        palmWeightDisplay[(size_t) s].store (0.0f, std::memory_order_relaxed);
        lastTool[(size_t) s] = RhTool::global;
        lastRest[(size_t) s] = false;
        strings[(size_t) s].setCouplingSendScale (1.0);
    }
}

//==============================================================================
// Borrowed damping: another string's hand lying on this one.

void LuthierEngine::borrowDamping (int s, StringEngine::Damping d, double amount) noexcept
{
    auto& b = borrowed[(size_t) s];
    auto& str = strings[(size_t) s];

    if (! b.held)
    {
        b.held = true;
        b.prior = str.getDamping();
        b.priorAmount = str.getDampingAmount();
    }

    // Two hands on one string: the firmer one is what the string feels.
    if (str.getDamping() == d && str.getDampingAmount() >= amount)
        return;

    str.setDamping (d, amount);
}

void LuthierEngine::releaseBorrowIfFree (int s) noexcept
{
    auto& b = borrowed[(size_t) s];

    if (! b.held || b.adjacentSources != 0 || b.restFrom >= 0 || b.palm)
        return;

    strings[(size_t) s].setDamping (b.prior, b.priorAmount);
    b = BorrowedDamping {};
}

void LuthierEngine::clearBorrowed (int s) noexcept
{
    // The string's own note sets its damping; nothing is restored over it.
    borrowed[(size_t) s] = BorrowedDamping {};
    palmWeightDisplay[(size_t) s].store (0.0f, std::memory_order_relaxed);
}

void LuthierEngine::liftMutesFrom (int s) noexcept
{
    // string-interaction.md 3: the source's note-off lifts every mute it caused.
    const auto bit = (juce::uint32) 1u << (juce::uint32) s;

    for (int n = 0; n < numStrings; ++n)
    {
        auto& b = borrowed[(size_t) n];

        if ((b.adjacentSources & bit) != 0)
        {
            b.adjacentSources &= ~bit;
            releaseBorrowIfFree (n);
        }
    }
}

void LuthierEngine::onRealismBNoteOn (int s) noexcept
{
    clearBorrowed (s);

    // fingerstyle-attack.md 2: a rest stroke's neighbour stays damped until
    // the next note-on on either string.
    for (int n = 0; n < numStrings; ++n)
    {
        if (borrowed[(size_t) n].restFrom == s)
        {
            borrowed[(size_t) n].restFrom = -1;
            releaseBorrowIfFree (n);
        }
    }

    // A new note on s replaces whatever its old finger was lying on.
    liftMutesFrom (s);

    struckThisBlock |= (juce::uint32) 1u << (juce::uint32) s;
}

//==============================================================================
// string-interaction.md 3: adjacent finger damping.

void LuthierEngine::applyAdjacentMute (const NoteOnEvent& e, int s, double fret) noexcept
{
    const double amount = interaction.adjacentMute * interaction.frettingStyle;

    if (amount <= 0.0 || fret <= 0.0 || e.deadStrike || e.touchFret >= 0.0)
        return;

    switch (e.technique)
    {
        case Technique::NaturalHarmonic:
        case Technique::ArtificialHarmonic:
        case Technique::PinchHarmonic:
        case Technique::Tap:
        case Technique::SlideGuitar:
            return;
        default:
            break;
    }

    if (slide.isUnderBar (s))
        return;

    // The underside of the finger on the thinner string (s - 1), the tip on
    // the thicker one (s + 1).
    const int neighbours[2] = { s - 1, s + 1 };
    const double weights[2] = { 1.0, 0.5 };

    for (int k = 0; k < 2; ++k)
    {
        const int n = neighbours[k];

        if (n < 0 || n >= numStrings || stringMidiNote[(size_t) n] >= 0)
            continue;

        const double a = juce::jlimit (0.0, 1.0, amount * weights[k]);

        if (a <= 0.0)
            continue;

        borrowDamping (n, StringEngine::Damping::Chuck, a);
        borrowed[(size_t) n].adjacentSources |= (juce::uint32) 1u << (juce::uint32) s;
    }
}

//==============================================================================
// string-interaction.md 2: palm spread.

void LuthierEngine::notePalmStrike (int s) noexcept
{
    const int64_t now = blockStartSample + activeSampleOffset;

    // Strikes within 30 ms are one group; its centre is their mean string.
    if (now - palmGroupStart > (int64_t) (0.030 * sr))
    {
        palmGroupStart = now;
        palmGroupSum = 0.0;
        palmGroupCount = 0;
    }

    palmGroupSum += (double) s;
    ++palmGroupCount;
    palmCentre = palmGroupSum / (double) palmGroupCount;
}

void LuthierEngine::updatePalmSpread() noexcept
{
    const double P = technique.getPalmMuteAmount();
    const bool acoustic = spec.category == GuitarCategory::Acoustic;
    const bool classical = guitarType == GuitarType::Classical || guitarType == GuitarType::Flamenco;
    const bool bass = spec.category == GuitarCategory::Bass;
    const double sb = bridgeStringSpacingMm (acoustic, classical, bass);
    const double W = juce::jmax (0.0, interaction.palmSpreadMm) / sb;

    for (int s = 0; s < numStrings; ++s)
    {
        auto& b = borrowed[(size_t) s];
        const bool struck = (struckThisBlock & ((juce::uint32) 1u << (juce::uint32) s)) != 0;
        const double w = (P > 0.05 && palmCentre >= 0.0) ? palmWeight ((double) s - palmCentre, W) : 0.0;

        palmWeightDisplay[(size_t) s].store ((float) (P > 0.05 && palmCentre >= 0.0
                                                          ? juce::jmax (w, stringMidiNote[(size_t) s] >= 0
                                                                              && strings[(size_t) s].getDamping() == StringEngine::Damping::PalmMute ? 1.0 : 0.0)
                                                          : 0.0),
                                             std::memory_order_relaxed);

        if (P > 0.05 && w > 0.0 && ! struck)
        {
            const auto d = strings[(size_t) s].getDamping();

            // Only a string ringing free or released is covered; a choke, a
            // silence or a chuck is already stronger than a palm.
            if (b.palm || d == StringEngine::Damping::Open || d == StringEngine::Damping::Released)
            {
                if (! b.palm || std::abs (strings[(size_t) s].getDampingAmount() - P * w) > 1.0e-6)
                {
                    if (! b.held)
                    {
                        b.held = true;
                        b.prior = d;
                        b.priorAmount = strings[(size_t) s].getDampingAmount();
                    }

                    strings[(size_t) s].setDamping (StringEngine::Damping::PalmMute, P * w);
                }

                b.palm = true;
            }
        }
        else if (b.palm && (P <= 0.05 || w <= 0.0))
        {
            // The palm lifts: back to the state it had (5).
            b.palm = false;
            releaseBorrowIfFree (s);
        }
    }

    struckThisBlock = 0;
}

//==============================================================================
// string-interaction.md 4: release stagger. Returns how many note-offs it
// staggered; `due` holds every note-off's absolute sample.

int LuthierEngine::stageNoteOffs (const PlayEventQueue& queue, std::array<int64_t, PlayEventQueue::kCapacity>& due) noexcept
{
    const int count = queue.getNumNoteOffs();

    for (int i = 0; i < count; ++i)
        due[(size_t) i] = samplePosition + queue.getNoteOff (i).sampleOffset;

    if (interaction.releaseStaggerMs <= 0.0 || count < 2)
        return 0;

    const int64_t window = (int64_t) std::ceil (0.003 * sr);
    std::array<bool, PlayEventQueue::kCapacity> grouped {};
    int staggered = 0;

    for (int i = 0; i < count; ++i)
    {
        if (grouped[(size_t) i] || queue.getNoteOff (i).letRing)
            continue;

        // A group: note-offs on distinct strings within 3 ms of the first.
        std::array<int, kMaxStrings> members {};
        int numMembers = 0;
        juce::uint32 seen = 0;

        for (int j = i; j < count && numMembers < kMaxStrings; ++j)
        {
            const auto& off = queue.getNoteOff (j);
            const auto bit = (juce::uint32) 1u << (juce::uint32) juce::jlimit (0, kMaxStrings - 1, off.stringIndex);

            if (grouped[(size_t) j] || off.letRing || (seen & bit) != 0
                || std::abs (due[(size_t) j] - due[(size_t) i]) > window)
                continue;

            seen |= bit;
            members[(size_t) numMembers++] = j;
        }

        if (numMembers < 2)
            continue;

        // Seeded from the character seed and the group's sample: a render
        // repeats bit for bit (0.3).
        const int64_t groupSample = due[(size_t) i];
        staggerRng.setSeed ((character.getSeed() ^ (uint64_t) groupSample) | 1ull);

        int lowest = kMaxStrings, highest = -1;

        for (int m = 0; m < numMembers; ++m)
        {
            const int st = queue.getNoteOff (members[(size_t) m]).stringIndex;
            lowest = juce::jmin (lowest, st);
            highest = juce::jmax (highest, st);
        }

        for (int m = 0; m < numMembers; ++m)
        {
            const int j = members[(size_t) m];
            const int st = queue.getNoteOff (j).stringIndex;
            const double rank = highest > lowest ? (double) (st - lowest) / (double) (highest - lowest) : 0.0;
            const double delayMs = staggerDelayMs (interaction.releaseStaggerMs, interaction.releaseStaggerBias,
                                                   staggerRng.nextDouble(), rank);

            due[(size_t) j] += (int64_t) std::round (delayMs * 0.001 * sr);
            grouped[(size_t) j] = true;
            ++staggered;
        }
    }

    return staggered;
}

//==============================================================================
// string-interaction.md 5: crosstalk from lateral displacement.

void LuthierEngine::updateCrosstalk() noexcept
{
    std::array<double, kMaxStrings> mm {}, stop {};
    std::array<bool, kMaxStrings> bassward {};
    bool bent = false;

    for (int s = 0; s < numStrings; ++s)
    {
        const double cents = midi.getStringBendCents (s);

        if (std::abs (cents) > 0.5 && ! slide.isUnderBar (s))
        {
            mm[(size_t) s] = lateralMmForBend (cents);
            bent = true;
        }

        const double absoluteFret = (double) tuning.getCapoFretFor (s) + currentFret[(size_t) s];
        stop[(size_t) s] = harmonics::vibratingLengthMm (spec.scaleLengthMm, absoluteFret);

        // Strings 0-3 are pushed toward the bass side, 4 and up pulled toward the treble.
        bassward[(size_t) s] = s <= 3;
    }

    if (! bent && ! crosstalkWasBent)
        return;

    const bool acoustic = spec.category == GuitarCategory::Acoustic;
    const bool classical = guitarType == GuitarType::Classical || guitarType == GuitarType::Flamenco;
    const bool bass = spec.category == GuitarCategory::Bass;

    pickups.setStringLateralOffsets (mm.data(), stop.data(), bassward.data(), numStrings, spec.scaleLengthMm,
                                     bridgeStringSpacingMm (acoustic, classical, bass), interaction.apertureScale);
    crosstalkWasBent = bent;
}

//==============================================================================
// harmonic-realism.md 2-3: the contact a note's technique puts on the string.

void LuthierEngine::applyHarmonicContact (const NoteOnEvent& e, int s, StringEngine& str, double fret,
                                          Excitation::Params& p) noexcept
{
    const bool tapped = e.technique == Technique::Tap && e.touchFret >= 0.0;
    const bool natural = e.technique == Technique::NaturalHarmonic || e.technique == Technique::ArtificialHarmonic;
    const bool pinch = e.technique == Technique::PinchHarmonic;

    // A new note lifts the old touch; a tapped harmonic converts what is ringing.
    if (! tapped)
        str.clearAllContacts();

    if (! tapped && ! natural && ! pinch)
        return;

    p.exactPluckComb = ! pinch;   // 3: plucking (or tapping) a node kills the harmonic

    const double capo = (double) tuning.getCapoFretFor (s);
    const double length = harmonics::vibratingLengthMm (spec.scaleLengthMm, capo + fret);
    const auto& t = harmonicTouch;

    StringEngine::Contact c;
    c.vibratingLengthMm = length;
    c.widthMm = t.fingerWidthMm;

    double touchFretFromNut = -1.0;

    if (pinch)
    {
        // The pick releases at the pluck position and the thumb lands
        // pinch_thumb_offset_mm toward the neck; CC 70 moves the pick.
        const double pickFraction = midi.hasPickPositionController()
                                      ? juce::jmap (midi.getPickPosition(), 0.02, 0.5)
                                      : p.pluckPosition;
        p.pluckPosition = juce::jlimit (0.02, 0.5, pickFraction);
        c.positionFromBridge = juce::jlimit (0.0, 1.0, p.pluckPosition + t.thumbOffsetMm / juce::jmax (1.0, length));

        /*  DECISION (REALISM-B): a graze is half the deliberate touch's
            strength. At full strength the 8 ms graze took 18-21 dB off the B
            string's fundamental, where ground rule 4 and HR-10 keep a clean
            pinch 10-25 dB under it. */
        c.strength = 0.5 * t.pressure;
        c.seconds = t.briefSeconds;
    }
    else
    {
        const double touch = e.touchFret >= 0.0 ? e.touchFret
                           : (e.harmonicPartial > 1 ? fret + 12.0 * std::log2 ((double) e.harmonicPartial
                                                                                / ((double) e.harmonicPartial - 1.0))
                                                    : fret + 12.0);
        c.positionFromBridge = harmonics::touchFractionFromBridge (touch, fret);
        touchFretFromNut = capo + touch;

        if (tapped)
        {
            /*  DECISION (REALISM-B): a tap presses the string onto the fret
                wire under it, an ideal damper (g = 1), and stays for half a
                harmonic touch (35 ms by default) or 1.5 grazes, whichever is
                longer. Measured, the node comb takes a partial down about
                2 dB per ms at g = 1 (the comb's taps are themselves in the
                loop, so a round trip does not cancel it outright); the
                spec's 12 ms left the A string's fundamental 23 dB down,
                short of HR-11's conversion. */
            c.strength = 1.0;
            c.seconds = juce::jmax (1.5 * t.briefSeconds, 0.5 * t.touchSeconds);

            // 3: the tap's impulse lands at the node.
            p.pluckPosition = juce::jlimit (0.02, 0.5, juce::jmin (c.positionFromBridge, 1.0 - c.positionFromBridge));
        }
        else
        {
            c.strength = t.pressure;
            c.seconds = t.touchSeconds;
        }
    }

    if (touchFretFromNut < 0.0)
        touchFretFromNut = capo + fret - 12.0 * std::log2 (juce::jmax (1.0e-6, c.positionFromBridge));

    const auto node = harmonics::findNode (c.positionFromBridge, c.vibratingLengthMm, c.widthMm);

    if (str.canRealiseContact (c))
    {
        str.addContact (c);
    }
    else
    {
        // 2's fallback: band isolation for this note, counted, never silent.
        p.isolateHarmonic = true;
        p.harmonicNumber = node.partial;
        validator.reportHarmonicFallback();
    }

    // The illustration's ring.
    auto& d = contactDisplay[(size_t) s];
    d.fret.store ((float) touchFretFromNut, std::memory_order_relaxed);
    d.missed.store (node.partial == 0, std::memory_order_relaxed);
    d.life.store (1.0f, std::memory_order_relaxed);
    contactDisplayTotal[(size_t) s] = contactDisplaySamples[(size_t) s]
        = juce::jmax (1, (int) (juce::jmax (0.05, c.seconds) * sr));
}

//==============================================================================
// fingerstyle-attack.md 3: which tool plays a note.

LuthierEngine::HandResolution LuthierEngine::resolveRightHand (const NoteOnEvent& e, int s) noexcept
{
    HandResolution h;

    // strum-dynamics 5: a strum's striker is what hit the string.
    if (e.strikerMaterial >= 0 || e.deadStrike)
        return h;

    const RhTool stringTool = rightHand.toolForString (s, spec.twelveString, GuitarLibrary::courseForString (s));
    const int live = midi.getRightHandToolOverride();

    if (live > 0)
        h.tool = (RhTool) juce::jlimit (1, (int) RhTool::numTools - 1, live);
    else if (e.finger >= 0)
        h.tool = e.finger == 0 ? (isThumbClass (stringTool) ? stringTool : RhTool::thumb)
                               : (isFingerClass (stringTool) ? stringTool : RhTool::finger);
    else
        h.tool = stringTool;

    if (h.tool == RhTool::slap)
        h.slapType = 0;
    else if (h.tool == RhTool::pop)
        h.slapType = 1;

    // 2: the stroke, for the flesh tools.
    const bool restable = h.tool == RhTool::finger || h.tool == RhTool::thumb || h.tool == RhTool::thumbpick;

    if (restable)
    {
        bool rest = midi.isRestStrokeForced() || rightHand.stroke == RhStroke::rest;

        // bass-techniques.md 6: the bass's rest_stroke forces it for fingers.
        if (rightHand.bassRestStroke && spec.category == GuitarCategory::Bass && h.tool == RhTool::finger)
            rest = true;

        if (! rest && rightHand.stroke == RhStroke::automatic && e.velocity >= 0.7)
        {
            // The classical rule: a melody note (no other note-on within
            // 30 ms, before or after) is a rest stroke.
            const int64_t now = blockStartSample + activeSampleOffset;
            const int64_t window = (int64_t) (0.030 * sr);
            bool alone = ! (lastNoteOnString != s && lastNoteOnString >= 0 && now - lastNoteOnSample <= window);

            for (int i = 0; i < numScheduled && alone; ++i)
            {
                const auto& ev = scheduled[(size_t) i];

                if (ev.isNoteOn && ev.noteOn.stringIndex != s && ! ev.noteOn.deadStrike
                    && ev.absoluteSample >= now && ev.absoluteSample - now <= window)
                    alone = false;
            }

            rest = alone;
        }

        h.rest = rest;
    }

    return h;
}

SlapStrike LuthierEngine::makeToolStrike (const NoteOnEvent& e, int slapType) const noexcept
{
    // 4: the Slap and Pop tools are the intent; strength and position come
    // from SlapSettings, as a slap armed by its trigger would take them.
    const auto& ss = slap.getSettings();
    SlapStrike st;
    st.stringIndex = juce::jlimit (0, numStrings - 1, e.stringIndex);
    st.strike = true;
    st.type = slapType == 1 ? SlapType::pop : SlapType::thumb;
    st.force = (st.type == SlapType::thumb ? ss.slapStrength : ss.popStrength)
               * (0.7 + 0.3 * juce::jlimit (0.0, 1.0, e.velocity));
    st.contactMm = st.type == SlapType::thumb ? ss.slapPositionMm : ss.popPositionMm;
    st.velocity = st.force;
    return st;
}

void LuthierEngine::applyRightHand (const HandResolution& hand, const NoteOnEvent& e, int s, StringEngine& str,
                                    Excitation::Params& p, bool& fingersForNoise) noexcept
{
    str.setCouplingSendScale (1.0);
    lastTool[(size_t) s] = hand.tool;
    lastRest[(size_t) s] = false;

    if (hand.tool == RhTool::global || hand.slapType >= 0)
        return;

    const double b = juce::jlimit (0.0, 1.0, nailVsFlesh);
    const double flesh = juce::jmax (0.001, rightHand.fleshReleaseMs);
    const double nail = juce::jmax (0.001, rightHand.nailReleaseMs);
    const double thumbOffset = rightHand.thumbPositionOffset;
    const double v = juce::jlimit (0.0, 1.0, rightHand.alternationVariation);

    switch (hand.tool)
    {
        case RhTool::pick:
        {
            const bool fingerMaterial = ! PlayingNoise::getPickMaterial (chosenPickMaterial).isPick;
            p.material = fingerMaterial ? Excitation::Material::PickCelluloid : chosenPickMaterial;
            fingersForNoise = false;
            break;
        }

        case RhTool::finger:
        {
            p.material = Excitation::Material::Fingertip;
            p.nailVsFlesh = b;
            double tau = flesh + (nail - flesh) * b;

            // 3: alternation. The pattern's letters when it has them, i and m
            // in turn otherwise; the m stroke is slightly slower, nearer the
            // neck and later. Deterministic.
            const bool middle = e.finger >= 0 ? e.finger == 2 : (alternationPhase++ & 1) != 0;

            if (middle && v > 0.0)
            {
                tau *= 1.0 + 0.15 * v;
                p.pluckPosition = juce::jlimit (0.02, 0.5, p.pluckPosition + 0.01 * v);
                p.startDelaySamples = (int) std::round (1.5 * v * 0.001 * sr);
            }

            p.releaseSeconds = tau * 0.001;
            fingersForNoise = true;

            // 1: a nail meeting the string clicks, 0.35 b of a pick's click.
            if (b > 0.0)
            {
                auto nailPick = playingNoise.getPick();
                nailPick.material = Excitation::Material::PickCelluloid;   // keratin clicks like celluloid
                nailPick.fingers = false;
                nailPick.clickAmount *= 0.35 * b;
                nailPick.pluckPosition = p.pluckPosition;
                playingNoise.getPool().trigger (PlayingNoise::makeClick (nailPick, s, e.velocity));
            }
            break;
        }

        case RhTool::thumb:
            p.material = Excitation::Material::Thumb;
            p.nailVsFlesh = b;
            p.releaseSeconds = (2.0 * flesh + (nail - 2.0 * flesh) * 0.5 * b) * 0.001;
            p.pluckPosition = juce::jlimit (0.02, 0.5, p.pluckPosition + thumbOffset);
            fingersForNoise = true;
            break;

        case RhTool::thumbpick:
            p.material = Excitation::Material::Thumbpick;
            p.pluckPosition = juce::jlimit (0.02, 0.5, p.pluckPosition + thumbOffset);
            fingersForNoise = false;
            break;

        case RhTool::global:
        case RhTool::slap:
        case RhTool::pop:
        case RhTool::numTools:
        default:
            break;
    }

    // 4, Travis: the heel of the hand on the alternating bass.
    if (isThumbClass (hand.tool) && rightHand.thumbPalmMute > 0.0)
        str.setDamping (StringEngine::Damping::PalmMute, juce::jlimit (0.0, 1.0, rightHand.thumbPalmMute));

    // 4, Hybrid: chicken-picking's snap is a small pop, through the slap's
    // collision path, on a guitar with any pick string.
    if (isFingerClass (hand.tool) && rightHand.hybridSnap > 0.0 && ! fretless)
    {
        bool anyPick = false;

        for (const auto t : rightHand.stringTool)
            anyPick = anyPick || t == RhTool::pick;

        if (anyPick)
        {
            SlapStrike snap;
            snap.stringIndex = s;
            snap.strike = true;
            snap.type = SlapType::pop;
            snap.force = 0.3 * rightHand.hybridSnap * juce::jlimit (0.0, 1.0, e.velocity);
            snap.velocity = snap.force;
            snap.contactMm = slap.getSettings().popPositionMm;

            const auto clack = slap.makeContactBuzz (snap, stringSpecs[(size_t) s].wound,
                                                     str.getCurrentFrequency(), fretBuzzModel);

            if (clack.level > 0.0)
                playingNoise.getPool().trigger (clack);
        }
    }

    // 2: the rest stroke.
    if (hand.rest)
    {
        lastRest[(size_t) s] = true;
        p.levelScale = 1.26;
        p.contactScale = 1.15;
        p.brightnessScale = 0.90;
        str.setCouplingSendScale (1.30);
        noteSustainScale[(size_t) s] = juce::jlimit (0.05, 4.0, noteSustainScale[(size_t) s] * 0.85);
        str.setSustainScale (noteSustainScale[(size_t) s]);

        // The finger comes to rest on the next lower-pitched string (s + 1);
        // the thumb moves toward the treble and lands on s - 1.
        const int n = isThumbClass (hand.tool) ? s - 1 : s + 1;

        if (n >= 0 && n < numStrings && rightHand.restStrokeDamping > 0.0)
        {
            borrowDamping (n, StringEngine::Damping::Chuck, juce::jlimit (0.0, 1.0, rightHand.restStrokeDamping));
            borrowed[(size_t) n].restFrom = s;
        }
    }
}

} // namespace luthier
