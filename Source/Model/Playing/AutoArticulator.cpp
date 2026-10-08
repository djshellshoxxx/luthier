#include "AutoArticulator.h"
#include "TuningEngine.h"

#include <cmath>

namespace luthier
{

//==============================================================================
const char* getAssistLabelGlyph (AssistLabel l) noexcept
{
    switch (l)
    {
        case AssistLabel::hammerOn:  return "H";
        case AssistLabel::pullOff:   return "P";
        case AssistLabel::slideUp:   return "/";
        case AssistLabel::slideDown: return "\\";
        case AssistLabel::vibrato:   return "~";
        case AssistLabel::palmMute:  return "PM";
        case AssistLabel::upStroke:  return "\xe2\x86\x91";          // up arrow
        case AssistLabel::accent:    return ">";
        case AssistLabel::bendHalf:  return "b\xc2\xbd";             // b1/2
        case AssistLabel::bendWhole: return "b1";
        case AssistLabel::fall:      return "\xe2\x86\x98";          // south-east arrow
        case AssistLabel::strumDown: return "\xe2\x86\x93";
        case AssistLabel::strumUp:   return "\xe2\x86\x91";
        case AssistLabel::slideIn:   return "/";
        case AssistLabel::muteLift:  return "";
        case AssistLabel::none:
        case AssistLabel::numLabels:
        default:                     return "";
    }
}

const char* getAssistLabelName (AssistLabel l) noexcept
{
    switch (l)
    {
        case AssistLabel::hammerOn:  return "hammer-on";
        case AssistLabel::pullOff:   return "pull-off";
        case AssistLabel::slideUp:   return "slide up";
        case AssistLabel::slideDown: return "slide down";
        case AssistLabel::vibrato:   return "vibrato";
        case AssistLabel::palmMute:  return "palm mute";
        case AssistLabel::upStroke:  return "up-stroke";
        case AssistLabel::accent:    return "accent";
        case AssistLabel::bendHalf:  return "bend half";
        case AssistLabel::bendWhole: return "bend whole";
        case AssistLabel::fall:      return "fall";
        case AssistLabel::strumDown: return "strum down";
        case AssistLabel::strumUp:   return "strum up";
        case AssistLabel::slideIn:   return "slide in";
        case AssistLabel::muteLift:  return "mute lift";
        case AssistLabel::none:
        case AssistLabel::numLabels:
        default:                     return "";
    }
}

//==============================================================================
AutoArticulator::AutoArticulator() noexcept
{
    reset();
}

void AutoArticulator::prepare (double sampleRate, int strings) noexcept
{
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    setNumStrings (strings);
    reset();
}

void AutoArticulator::reset() noexcept
{
    for (auto& r : log)
        r = NoteRecord {};

    logCount = 0;
    logHead = 0;
    current.fill (-1);
    lastSingle = -1;
    lastStruck = -1;
    hand = 0;

    lastPickedArrival = -1000000000;
    lastPickedUp = false;
    lastStrumSample = -1000000000;
    lastStrumUp = false;
    lastStrumCcSample = -1000000000;
    lastBendSample.fill (-1000000000);
    lastGlobalBendSample = -1000000000;
    controllerVibrato = 0.0;

    for (auto& p : pitch)   p = PitchCurve {};
    for (auto& v : vibrato) v = Vibrato {};
    pendingEvents.fill (0);

    offSince = -1;
    noteOrdinal = 0;
}

void AutoArticulator::setNumStrings (int n) noexcept
{
    numStrings = juce::jlimit (1, kMaxStrings, n);
}

void AutoArticulator::setSettings (const AutoArticulationSettings& s) noexcept
{
    settings = s;
    settings.style = juce::jlimit (0, AutoArticulationStyles::kNumStyles - 1, s.style);
    settings.amount = juce::jlimit (0.0, 1.0, std::isfinite (s.amount) ? s.amount : 0.0);
    settings.rules = s.rules & AssistRule::all;
}

//==============================================================================
double AutoArticulator::hash01 (juce::uint64 a, juce::uint64 b) noexcept
{
    // splitmix64 over the pair: a counter-based hash, so the draw for a note
    // depends only on which note it is, never on how many draws came before.
    juce::uint64 z = a * 0x9E3779B97F4A7C15ull + (b + 0x632BE59BD9B4E019ull) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return (double) (z >> 11) * (1.0 / 9007199254740992.0);
}

double AutoArticulator::probability (double styleProbability) const noexcept
{
    if (probabilityOverride >= 0.0)
        return styleProbability > 0.0 ? probabilityOverride : 0.0;

    return styleProbability * probabilityScale();
}

//==============================================================================
int AutoArticulator::addRecord (const NoteRecord& r) noexcept
{
    const int index = logHead;

    // The slot being reused may still be some string's sounding note: that
    // string forgets it (32 notes ago; nothing reads that far back).
    for (auto& c : current)
        if (c == index)
            c = -1;

    if (lastSingle == index) lastSingle = -1;
    if (lastStruck == index) lastStruck = -1;

    for (auto& v : vibrato)
        if (v.logIndex == index)
            v = Vibrato {};

    log[(size_t) index] = r;
    logHead = (logHead + 1) % kLogSize;
    logCount = juce::jmin (kLogSize, logCount + 1);
    return index;
}

const AutoArticulator::NoteRecord* AutoArticulator::record (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, kLogSize) && log[(size_t) index].midi >= 0 ? &log[(size_t) index] : nullptr;
}

AutoArticulator::NoteRecord* AutoArticulator::record (int index) noexcept
{
    return juce::isPositiveAndBelow (index, kLogSize) && log[(size_t) index].midi >= 0 ? &log[(size_t) index] : nullptr;
}

bool AutoArticulator::heldAt (const NoteRecord& r, juce::int64 sample) const noexcept
{
    return r.midi >= 0 && r.arrival <= sample && (r.off < 0 || r.off > sample);
}

bool AutoArticulator::isStringHeldAt (int s, juce::int64 sample) const noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return false;

    const auto* r = record (current[(size_t) s]);
    return r != nullptr && heldAt (*r, sample);
}

int AutoArticulator::numStringsHeldAt (juce::int64 sample) const noexcept
{
    int n = 0;

    for (int s = 0; s < numStrings; ++s)
        if (isStringHeldAt (s, sample))
            ++n;

    return n;
}

bool AutoArticulator::anyHeldAt (juce::int64 sample) const noexcept
{
    return numStringsHeldAt (sample) > 0;
}

bool AutoArticulator::nothingHeldFor (juce::int64 sample, double ms) const noexcept
{
    // Silence for `ms` before `sample`: no key held now, and the last release
    // at least that long ago (3.7, 3.8).
    const auto window = (juce::int64) (ms * 0.001 * sr);

    for (int i = 0; i < logCount; ++i)
    {
        const auto& r = log[(size_t) i];

        if (r.midi < 0 || r.arrival > sample)
            continue;

        if (r.off < 0 || r.off > sample - window)
            return false;
    }

    return true;
}

bool AutoArticulator::isLeadAt (int s, juce::int64 sample) const noexcept
{
    // 3.4: no more than two strings held, and this the highest held note.
    const auto* mine = record (current[(size_t) s]);

    if (mine == nullptr || ! heldAt (*mine, sample))
        return false;

    int held = 0;

    for (int o = 0; o < numStrings; ++o)
    {
        const auto* r = record (current[(size_t) o]);

        if (r == nullptr || ! heldAt (*r, sample))
            continue;

        ++held;

        if (o != s && r->midi > mine->midi)
            return false;
    }

    return held <= 2;
}

double AutoArticulator::ppqAt (juce::int64 sample) const noexcept
{
    return transport.ppqAtBlockStart
         + (double) (sample - transportBlockStart) / sr * juce::jmax (1.0, transport.bpm) / 60.0;
}

int AutoArticulator::semitonesAboveLowestOpen (int midiNote) const noexcept
{
    if (tuning == nullptr)
        return 0;

    double lowestHz = 1.0e9;

    for (int s = 0; s < numStrings; ++s)
        lowestHz = juce::jmin (lowestHz, tuning->computeFrequency (s, 0.0, 0.0));

    const double lowestMidi = 69.0 + 12.0 * std::log2 (juce::jmax (1.0, lowestHz) / juce::jmax (1.0, tuning->getConcertA()));
    return (int) std::lround ((double) midiNote - lowestMidi);
}

double AutoArticulator::openBias (int s) const noexcept
{
    const auto& st = style();

    if (st.metalOpenBias)
        return (s == numStrings - 1) ? 2.0 : -1.0;

    return st.openStringBias;
}

bool AutoArticulator::explicitOnString (int s, int midiNote, double velocity) const noexcept
{
    juce::ignoreUnused (midiNote);

    // 5: slap held or its velocity zone hit, a scrape on the string, Tap armed.
    if (context.slapHeld || context.tapArmed)
        return true;

    if (context.slapVelocityZone && (int) std::lround (velocity * 127.0) >= context.slapZoneVelocity)
        return true;

    return juce::isPositiveAndBelow (s, 32) && ((context.scrapeMask >> s) & 1u) != 0;
}

void AutoArticulator::noteBend (int s, juce::int64 sample) noexcept
{
    if (s < 0)
        lastGlobalBendSample = sample;
    else if (s < kMaxStrings)
        lastBendSample[(size_t) s] = sample;
}

bool AutoArticulator::strumControllerRecent (juce::int64 sample) const noexcept
{
    return sample - lastStrumCcSample <= (juce::int64) (2.0 * sr);
}

//==============================================================================
bool AutoArticulator::legatoEligible (const NoteRecord& p, int midiNote, double velocity,
                                      juce::int64 arrival) const noexcept
{
    // 3.2, all but the interval, which hammer-ons and slides judge differently.
    if (p.midi < 0 || p.string < 0 || p.midi == midiNote || p.chord)
        return false;

    const auto grace = (juce::int64) (0.015 * sr);
    const bool heldOrJust = p.off < 0 || p.off > arrival || arrival - p.off <= grace;

    if (! heldOrJust)
        return false;

    const double ioiMs = (double) (arrival - p.arrival) * 1000.0 / sr;

    if (ioiMs < 0.0 || ioiMs > style().legatoMaxIoiMs * windowScale())
        return false;

    if (velocity > p.velocity + 12.0 / 127.0 + 1.0e-9)
        return false;

    return p.chain < style().legatoChainCap;
}

AutoArticulator::SinglePlan AutoArticulator::planSingle (int midiNote, double velocity, juce::int64 arrival,
                                                         bool lateJoin, juce::uint32 lateJoinGroupMask) noexcept
{
    SinglePlan out;
    out.note.midiNote = midiNote;
    out.note.velocity = velocity;

    if (tuning == nullptr)
        return out;

    const auto& st = style();
    const double hz = midiToHz ((double) midiNote, tuning->getConcertA());

    const auto* p = record (lastSingle);
    const int interval = p != nullptr ? std::abs (midiNote - p->midi) : 0;
    const bool legatoRules = (rule (AssistRule::legato) || rule (AssistRule::slide)) && ! context.slideMode;
    const bool eligible = ! lateJoin && legatoRules && p != nullptr && legatoEligible (*p, midiNote, velocity, arrival);
    const bool hammerFits = eligible && rule (AssistRule::legato) && interval >= 1 && interval <= st.legatoMaxInterval;
    const bool slideFits = eligible && rule (AssistRule::slide)
                           && (interval <= st.slideMaxInterval || (interval >= 4 && interval <= 7));
    const int legatoString = (hammerFits || slideFits) ? p->string : -1;

    const auto* previous = record (lastStruck);
    const int previousString = previous != nullptr ? previous->string : -1;

    // ---- 3.1: the candidate strings -----------------------------------------
    int bestString = -1, bestFret = 0;
    double bestCost = 1.0e9;

    for (int s = 0; s < numStrings; ++s)
    {
        const double exact = tuning->frequencyToFretPosition (s, hz);
        const int fret = (int) std::round (exact);

        if (std::abs (exact - (double) fret) > 0.08 || fret < 0 || fret > tuning->getHighestPlayableFret (s))
            continue;

        double cost = 0.0;

        if (rule (AssistRule::position))
        {
            if (fret > 0)
            {
                if (fret < hand)          cost += 3.0 * (hand - fret);
                else if (fret > hand + 4) cost += 3.0 * (fret - hand - 4);
            }
            else
            {
                cost -= openBias (s);
            }

            if (previousString >= 0)
                cost += juce::jmax (0, std::abs (s - previousString) - 1);

            if (s != legatoString && isStringHeldAt (s, arrival))
                cost += 4.0;

            if (fret > 12)
                cost += 1.0;

            if (lateJoin && lateJoinGroupMask != 0)
            {
                const bool adjacent = (s > 0 && ((lateJoinGroupMask >> (s - 1)) & 1u) != 0)
                                   || (s + 1 < 32 && ((lateJoinGroupMask >> (s + 1)) & 1u) != 0);
                if (adjacent)
                    cost -= 1.0;
            }
        }
        else
        {
            // Position rule off: today's single-note placement, near the hand
            // and the last string (ChordVoicer::voiceSingleNote's terms).
            cost += std::abs (fret - hand) + (previousString >= 0 ? std::abs (s - previousString) * 2.5 : 0.0)
                  - s * 0.25;
        }

        if (s == legatoString)
            cost -= 2.0;

        const bool better = cost < bestCost - 1.0e-9
                         || (std::abs (cost - bestCost) <= 1.0e-9
                               && (fret < bestFret || (fret == bestFret && s > bestString)));

        if (bestString < 0 || better)
        {
            bestCost = cost;
            bestString = s;
            bestFret = fret;
        }
    }

    if (bestString < 0)
        return out;

    out.note.stringIndex = bestString;
    out.note.fretPosition = (double) bestFret;
    out.note.valid = true;
    out.plan.assisted = true;
    out.plan.lateJoin = lateJoin;

    if (rule (AssistRule::position) && bestFret > 0 && (bestFret < hand || bestFret > hand + 4))
        hand = juce::jlimit (0, 24, bestFret - 1);

    ++noteOrdinal;

    // ---- 3.2 / 3.3: legato on the source's string ---------------------------
    if (bestString == legatoString)
    {
        out.plan.chainCount = p->chain + 1;
        out.sourceString = p->string;
        out.sourceMidi = p->midi;

        const bool heldAtArrival = p->off < 0 || p->off > arrival;
        const auto minOverlap = (juce::int64) std::llround (st.slideMinOverlapMs * 0.001 * sr);

        if (slideFits && heldAtArrival)
        {
            if (p->off >= 0)
            {
                // Released already (inside the chord window): the overlap is known.
                if (p->off - arrival >= minOverlap)
                    out.plan.legato = Technique::Slide;
                else if (hammerFits)
                    out.plan.legato = midiNote > p->midi ? Technique::HammerOn : Technique::PullOff;
            }
            else
            {
                // Still held: the overlap decides, so wait for the release or
                // the minimum overlap, whichever comes first (3.3).
                out.deferUntil = arrival + minOverlap;
            }
        }
        else if (hammerFits)
        {
            out.plan.legato = midiNote > p->midi ? Technique::HammerOn : Technique::PullOff;
        }

        if (out.plan.legato == Technique::Slide)
            out.plan.slideFromFret = p->fret;

        if (out.plan.legato == Technique::Pluck && out.deferUntil < 0)
            out.plan.chainCount = 0;
    }

    // ---- 3.8: ornaments on phrase starts -------------------------------------
    const bool lead = st.ornament != AssistOrnament::none && ! lateJoin && ! context.slideMode;
    const bool pitchGesture = controllerVibrato > 0.02
                              || arrival - lastBendSample[(size_t) bestString] < (juce::int64) (0.1 * sr)
                              || arrival - lastGlobalBendSample < (juce::int64) (0.1 * sr);

    if (rule (AssistRule::ornament) && lead && ! pitchGesture
        && out.plan.legato == Technique::Pluck && out.deferUntil < 0
        && velocity * 127.0 >= 80.0 - 1.0e-6)
    {
        const int approach = previous != nullptr ? midiNote - previous->midi : 0;
        bool phraseStart = nothingHeldFor (arrival, 250.0);

        if (! phraseStart && transport.playing && approach > 0 && approach <= 2)
        {
            const double ppq = ppqAt (arrival);
            phraseStart = std::abs (ppq - std::round (ppq)) < 0.0625;
        }

        if (phraseStart && hash01 (noteOrdinal, (juce::uint64) midiNote) < probability (st.ornamentProbability))
        {
            if (st.ornament == AssistOrnament::bendInto)
            {
                int k = (st.bendWholeStep || approach >= 2) ? 2 : 1;

                // Bends past a half step only on a guitar's three highest strings.
                if (k > 1 && (context.bassFamily || bestString > 2))
                    k = 1;

                if (bestFret >= k + 1)
                {
                    out.plan.ornament = AssistOrnament::bendInto;
                    out.plan.bendSemitones = k;
                    out.note.fretPosition = (double) (bestFret - k);
                }
            }
            else if (st.ornament == AssistOrnament::slideIn && bestFret >= st.slideInFrets + 1)
            {
                out.plan.ornament = AssistOrnament::slideIn;
                out.plan.slideInFrets = st.slideInFrets;
            }
        }
    }

    return out;
}

void AutoArticulator::resolveLegato (SinglePlan& sp, bool sourceReleased) noexcept
{
    const auto* p = record (lastSingle);
    const auto& st = style();

    sp.deferUntil = -1;

    if (sourceReleased)
    {
        const int interval = std::abs (sp.note.midiNote - sp.sourceMidi);

        if (rule (AssistRule::legato) && interval >= 1 && interval <= st.legatoMaxInterval)
            sp.plan.legato = sp.note.midiNote > sp.sourceMidi ? Technique::HammerOn : Technique::PullOff;
        else
            sp.plan.legato = Technique::Pluck;
    }
    else
    {
        sp.plan.legato = Technique::Slide;
        sp.plan.slideFromFret = p != nullptr && p->string == sp.sourceString ? p->fret : sp.note.fretPosition;
    }

    if (sp.plan.legato == Technique::Pluck)
        sp.plan.chainCount = 0;
}

void AutoArticulator::setHandPositionFromVoicing (int handFret) noexcept
{
    if (rule (AssistRule::position))
        hand = juce::jlimit (0, 24, handFret);
}

//==============================================================================
bool AutoArticulator::planStrum (StrumRequest& request, juce::int64 sample, double peakVelocity,
                                 double speedVariation, AssistPlan& chordPlan) noexcept
{
    if (! rule (AssistRule::strum) || strumControllerRecent (sample))
        return false;

    const auto& st = style();
    bool up = false;

    if (st.upStrokes)
    {
        if (transport.playing)
        {
            // On the beat is down, off the beat up: straight or swung 8ths.
            const double ppq = ppqAt (sample);
            const double f = ppq - std::floor (ppq);
            up = st.swing ? (f >= 1.0 / 3.0 && f < 5.0 / 6.0) : (f >= 0.25 && f < 0.75);
        }
        else
        {
            up = nothingHeldFor (sample, 450.0) ? false : ! lastStrumUp;
        }
    }

    lastStrumUp = up;
    lastStrumSample = sample;

    request.down = ! up;
    request.sourceSps = st.strumCrossingSps * (0.75 + 0.5 * juce::jlimit (0.0, 1.0, peakVelocity))
                        / juce::jmax (0.05, speedVariation);
    request.missScale = 0.0;

    chordPlan.strummed = true;
    chordPlan.strumUp = up;
    return true;
}

bool AutoArticulator::planRollOrTogether (const int* strings, const int* midiNotes, int count,
                                          double* delaySeconds) const noexcept
{
    if (! rule (AssistRule::strum) || count <= 0)
        return false;

    const auto& st = style();

    if (st.strumMode == AssistStrumMode::together || context.bassFamily)
    {
        for (int i = 0; i < count; ++i)
            delaySeconds[i] = 0.0;

        return true;
    }

    if (st.strumMode == AssistStrumMode::pinchRoll)
    {
        // Thumb first, then the fingers ascending across the roll.
        for (int i = 0; i < count; ++i)
        {
            int rank = 0;

            for (int j = 0; j < count; ++j)
                if (midiNotes[j] < midiNotes[i] || (midiNotes[j] == midiNotes[i] && strings[j] > strings[i]))
                    ++rank;

            delaySeconds[i] = count > 1 ? st.rollMs * 0.001 * (double) rank / (double) (count - 1) : 0.0;
        }

        return true;
    }

    return false;
}

//==============================================================================
void AutoArticulator::decorate (NoteOnEvent& e, Technique decided, bool explicitTech,
                                juce::int64 arrival, juce::int64 soundSample, const AssistPlan& plan) noexcept
{
    const int s = juce::jlimit (0, numStrings - 1, e.stringIndex);
    e.arrivalSample = arrival;

    const bool harmonicOrTap = decided == Technique::NaturalHarmonic || decided == Technique::PinchHarmonic
                               || decided == Technique::ArtificialHarmonic || decided == Technique::Tap
                               || decided == Technique::SlideGuitar || decided == Technique::MutedPick;

    if (! plan.assisted || e.explicitArticulation || explicitOnString (s, e.midiNote, e.velocity)
        || (explicitTech && harmonicOrTap))
    {
        recordUnassisted (e, arrival, soundSample);
        return;
    }

    const auto& st = style();
    const double k = depthScale();
    juce::uint16 bits = 0;
    Technique tech = decided;

    pitch[(size_t) s] = PitchCurve {};

    NoteRecord r;
    r.midi = e.midiNote;
    r.string = s;
    r.fret = e.fretPosition;
    r.velocity = e.velocity;
    r.arrival = arrival;
    r.sound = soundSample;
    r.assisted = true;
    r.chord = plan.chordMember;

    // The previous note in the palm-mute register, and the previous note at all.
    const NoteRecord* prevReg = nullptr;
    const NoteRecord* prevAny = nullptr;

    for (int i = 1; i <= logCount; ++i)
    {
        const auto& c = log[(size_t) ((logHead - i + kLogSize) % kLogSize)];

        if (c.midi < 0 || c.arrival >= arrival)
            continue;

        if (prevAny == nullptr)
            prevAny = &c;

        if (prevReg == nullptr && st.palmMuteRegister >= 0
            && semitonesAboveLowestOpen (c.midi) <= st.palmMuteRegister)
            prevReg = &c;

        if (prevAny != nullptr && prevReg != nullptr)
            break;
    }

    if (! explicitTech)
    {
        // ---- 3.2 / 3.3 -----------------------------------------------------------
        if (plan.legato != Technique::Pluck && ! context.slideMode)
        {
            tech = plan.legato;
            r.chain = plan.chainCount;

            if (tech == Technique::Slide)
            {
                e.slideFromFret = plan.slideFromFret;
                bits |= AssistRule::slide;
            }
            else
            {
                // Fretless keeps today's rule: legato becomes a slide (3.2).
                if (context.fretless)
                {
                    tech = Technique::Slide;

                    if (const auto* src = record (current[(size_t) s]))
                        e.slideFromFret = src->fret;
                }

                bits |= AssistRule::legato;
            }
        }

        // ---- 3.8: ornaments --------------------------------------------------------
        if (plan.ornament == AssistOrnament::bendInto && tech == Technique::Pluck)
        {
            auto& c = pitch[(size_t) s];
            c.active = true;
            c.start = soundSample;
            c.length = juce::jmax ((juce::int64) 1, (juce::int64) std::llround (st.bendRiseMs * 0.001 * sr));
            c.from = 0.0;
            c.to = c.hold = 100.0 * plan.bendSemitones;
            e.autoOrnament = plan.bendSemitones >= 2 ? 2 : 1;
            bits |= AssistRule::ornament;
        }
        else if (plan.ornament == AssistOrnament::slideIn && tech == Technique::Pluck)
        {
            e.slideFromFret = juce::jmax (0.0, e.fretPosition - plan.slideInFrets);
            e.slideSeconds = st.slideInMs * 0.001;
            e.autoOrnament = 3;
            bits |= AssistRule::ornament;
        }

        // ---- 3.6: palm mute ----------------------------------------------------------
        if (rule (AssistRule::palmMute) && st.palmMuteRegister >= 0 && ! context.muteGridActive
            && tech == Technique::Pluck && e.slideFromFret < 0.0
            && (! plan.chordMember || (plan.chordSize <= 3 && plan.chordInRegister))
            && semitonesAboveLowestOpen (e.midiNote) <= st.palmMuteRegister)
        {
            double ioiRef = 0.0;
            bool mute = false;
            const double chugIoiMs = st.chugIoiMs * windowScale();

            if (prevReg != nullptr)
            {
                const double ioiMs = (double) (arrival - prevReg->arrival) * 1000.0 / sr;
                const double lastedMs = prevReg->off >= 0 ? (double) (prevReg->off - prevReg->arrival) * 1000.0 / sr : 1.0e9;

                if (ioiMs > 0.0 && ioiMs <= chugIoiMs && lastedMs <= 0.65 * ioiMs)
                {
                    mute = true;
                    ioiRef = ioiMs;
                }
            }

            if (! mute && prevAny != nullptr && prevAny->midi == e.midiNote)
            {
                const double ioiMs = (double) (arrival - prevAny->arrival) * 1000.0 / sr;

                if (ioiMs > 0.0 && ioiMs <= 250.0)
                {
                    mute = true;
                    ioiRef = ioiMs;
                }
            }

            if (! mute && st.metalFirstNoteMute && e.velocity * 127.0 >= 70.0 - 1.0e-6
                && semitonesAboveLowestOpen (e.midiNote) <= 5)
                mute = true;

            if (mute)
            {
                tech = Technique::PalmMute;
                e.palmMuteAmount = juce::jlimit (0.0, 1.0, st.palmMuteDepth * k);
                e.muteLiftSamples = (int) std::llround (juce::jmax (1.5 * ioiRef, 220.0) * 0.001 * sr);
                r.autoMuted = true;
                bits |= AssistRule::palmMute;
            }
        }
    }

    // ---- 3.5: attack ---------------------------------------------------------------
    if (rule (AssistRule::attack))
    {
        const double v127 = e.velocity * 127.0;

        if (v127 >= (double) st.accentVelocity - 1.0e-6)
        {
            e.attackBrightnessScale *= 1.0 + 0.25 * k;
            e.attackNoiseScale *= 1.0 + 0.8 * k;
            e.autoAccent = true;
            bits |= AssistRule::attack;
        }
        else if (v127 < 40.0)
        {
            e.attackBrightnessScale *= 0.85;
            e.attackNoiseScale *= 0.5;
            bits |= AssistRule::attack;
        }
    }

    // ---- 3.7: alternate picking (single notes) and strums ------------------------------
    const bool picked = tech == Technique::Pluck || tech == Technique::PalmMute;

    if (plan.strummed)
    {
        e.upStroke = plan.strumUp;
        e.autoStrumMask = plan.strumMask;
        bits |= AssistRule::strum;
    }
    else if (rule (AssistRule::alternate) && picked && ! plan.chordMember
             && ! (context.bassFamily && context.bassFingers))
    {
        const double ioiMs = (double) (arrival - lastPickedArrival) * 1000.0 / sr;
        bool up = false;

        if (ioiMs <= st.alternateIoiMs)
        {
            if (transport.playing)
                up = ((juce::int64) std::floor (ppqAt (arrival) * 4.0 + 0.5) & 1) != 0;
            else
                up = ! lastPickedUp;

            bits |= AssistRule::alternate;
        }

        if (up)
        {
            e.velocity = juce::jlimit (0.02, 1.0, e.velocity * (1.0 - 0.08 * k));
            e.attackBrightnessScale *= 0.9;
            e.attackNoiseScale *= 0.8;
            e.upStroke = true;
        }

        lastPickedArrival = arrival;
        lastPickedUp = up;
    }
    else if (picked && ! plan.chordMember)
    {
        lastPickedArrival = arrival;
        lastPickedUp = false;
    }

    r.technique = tech;
    r.picked = picked;
    r.upStroke = e.upStroke;
    r.ordinal = noteOrdinal;
    e.technique = tech;
    e.autoRules = bits;

    const int index = addRecord (r);
    current[(size_t) s] = index;
    lastStruck = index;

    if (! plan.chordMember)
        lastSingle = index;

    // ---- 3.4: a vibrato candidate --------------------------------------------------------
    vibrato[(size_t) s] = Vibrato {};

    if (rule (AssistRule::vibrato) && tech != Technique::PalmMute)
        startVibratoCandidate (s, index, soundSample, e.midiNote);
}

void AutoArticulator::recordUnassisted (const NoteOnEvent& e, juce::int64 arrival, juce::int64 soundSample) noexcept
{
    const int s = juce::jlimit (0, numStrings - 1, e.stringIndex);

    pitch[(size_t) s] = PitchCurve {};
    vibrato[(size_t) s] = Vibrato {};

    NoteRecord r;
    r.midi = e.midiNote;
    r.string = s;
    r.fret = e.fretPosition;
    r.velocity = e.velocity;
    r.arrival = arrival;
    r.sound = soundSample;
    r.technique = e.technique;
    r.picked = e.technique == Technique::Pluck || e.technique == Technique::PalmMute;

    const int index = addRecord (r);
    current[(size_t) s] = index;
    lastStruck = index;
    lastSingle = index;
}

void AutoArticulator::startVibratoCandidate (int s, int logIndex, juce::int64 sound, int midiNote) noexcept
{
    const auto& st = style();
    auto& v = vibrato[(size_t) s];

    v.candidate = true;
    v.started = false;
    v.logIndex = logIndex;
    v.startAt = sound + (juce::int64) std::llround (st.vibratoDelayMs * 0.001 * sr);

    // ±4 % per note, from the hash (3.4).
    v.rate = st.vibratoRateHz * (1.0 + 0.04 * (2.0 * hash01 (noteOrdinal, (juce::uint64) midiNote + 7777u) - 1.0));
    v.depth = st.vibratoDepthCents * depthScale() * (context.bassFamily ? 0.5 : 1.0);
}

//==============================================================================
int AutoArticulator::onNoteOff (int midiNote, int stringIndex, juce::int64 sample, bool sharedNoteOn) noexcept
{
    NoteRecord* r = nullptr;
    int index = -1;

    if (juce::isPositiveAndBelow (stringIndex, kMaxStrings))
    {
        index = current[(size_t) stringIndex];
        r = record (index);

        if (r != nullptr && (r->midi != midiNote || r->off >= 0))
            r = nullptr;
    }

    if (r == nullptr)
    {
        for (int i = 1; i <= logCount; ++i)
        {
            const int j = (logHead - i + kLogSize) % kLogSize;
            auto& c = log[(size_t) j];

            if (c.midi == midiNote && c.off < 0)
            {
                r = &c;
                index = j;
                break;
            }
        }
    }

    if (r == nullptr)
        return 0;

    r->off = sample;

    // ---- 3.8: fall --------------------------------------------------------------------
    const auto& st = style();
    const int s = r->string;

    if (! r->assisted || r->chord || st.fallProbability <= 0.0 || ! rule (AssistRule::ornament)
        || sharedNoteOn || context.slideMode || ! juce::isPositiveAndBelow (s, kMaxStrings)
        || current[(size_t) s] != index)
        return 0;

    if (sample - r->sound < (juce::int64) (0.5 * sr) || anyHeldAt (sample))
        return 0;

    if (controllerVibrato > 0.02 || sample - lastBendSample[(size_t) s] < (juce::int64) (0.1 * sr)
        || sample - lastGlobalBendSample < (juce::int64) (0.1 * sr))
        return 0;

    if (hash01 (r->ordinal, (juce::uint64) r->midi + 0xFA11u) >= probability (st.fallProbability))
        return 0;

    const int delay = (int) std::llround (0.120 * sr);
    const double from = autoPitchCents (s, sample);

    auto& c = pitch[(size_t) s];
    c.active = true;
    c.start = sample;
    c.length = delay;
    c.from = from;
    c.to = c.hold = from - 100.0 * st.fallSemitones;
    c.logIndex = index;

    pendingEvents[(size_t) s] |= fallStarted;
    return delay;
}

//==============================================================================
double AutoArticulator::autoPitchCents (int s, juce::int64 sample) const noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return 0.0;

    const auto& c = pitch[(size_t) s];

    if (! c.active || sample < c.start)
        return c.active ? c.from : 0.0;

    const double x = juce::jlimit (0.0, 1.0, (double) (sample - c.start) / (double) c.length);

    if (x >= 1.0)
        return c.hold;

    // A finger pushes a string in a smooth S, not a ramp.
    const double shape = x * x * (3.0 - 2.0 * x);
    return c.from + (c.to - c.from) * shape;
}

double AutoArticulator::autoVibratoDepth (int s, juce::int64 sample) const noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return 0.0;

    const auto& v = vibrato[(size_t) s];

    if (! v.candidate || current[(size_t) s] != v.logIndex || sample < v.startAt)
        return 0.0;

    const auto* r = record (v.logIndex);

    if (r == nullptr || (r->off >= 0 && r->off <= sample))
        return 0.0;

    // 5: a controller or a bend on the string owns the pitch.
    if (controllerVibrato > 0.02 || sample - lastBendSample[(size_t) s] < (juce::int64) (0.1 * sr)
        || sample - lastGlobalBendSample < (juce::int64) (0.1 * sr))
        return 0.0;

    // Lead when the delay ran out (decided from stamps), and still lead.
    if (! isLeadAt (s, v.startAt) || ! isLeadAt (s, sample))
        return 0.0;

    const double ramp = juce::jlimit (0.0, 1.0, (double) (sample - v.startAt) / (0.300 * sr));
    double depth = v.depth * ramp;

    if (offSince >= 0)
        depth *= juce::jlimit (0.0, 1.0, 1.0 - (double) (sample - offSince) / (0.150 * sr));

    return depth;
}

double AutoArticulator::autoVibratoCents (int s, juce::int64 sample) noexcept
{
    const double depth = autoVibratoDepth (s, sample);

    if (depth <= 0.0)
        return 0.0;

    auto& v = vibrato[(size_t) s];

    if (! v.started)
    {
        v.started = true;
        pendingEvents[(size_t) s] |= vibratoStarted;
    }

    const double t = (double) (sample - v.startAt) / sr;
    return depth * std::sin (juce::MathConstants<double>::twoPi * v.rate * t);
}

void AutoArticulator::setEffectiveNow (bool effective, juce::int64 sample) noexcept
{
    if (effectiveNow && ! effective)
        offSince = sample;
    else if (effective)
        offSince = -1;

    effectiveNow = effective;
}

int AutoArticulator::takePendingEvents (int s) noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return 0;

    const int e = pendingEvents[(size_t) s];
    pendingEvents[(size_t) s] = 0;
    return e;
}

double AutoArticulator::getVibratoRate (int s) const noexcept
{
    return juce::isPositiveAndBelow (s, kMaxStrings) ? vibrato[(size_t) s].rate : 0.0;
}

double AutoArticulator::getVibratoDepthCents (int s) const noexcept
{
    return juce::isPositiveAndBelow (s, kMaxStrings) ? vibrato[(size_t) s].depth : 0.0;
}

juce::int64 AutoArticulator::getVibratoStartSample (int s) const noexcept
{
    return juce::isPositiveAndBelow (s, kMaxStrings) ? vibrato[(size_t) s].startAt : 0;
}

double AutoArticulator::getSoundingFret (int s) const noexcept
{
    if (! juce::isPositiveAndBelow (s, kMaxStrings))
        return -1.0;

    const auto* r = record (current[(size_t) s]);
    return r != nullptr ? r->fret : -1.0;
}

void AutoArticulator::pushFeed (juce::int64 sample, int s, double fret, AssistLabel label, int ruleBit,
                                juce::uint16 mask) noexcept
{
    AutoArticulationFeedEntry e;
    e.sample = sample;
    e.string = (juce::int8) s;
    e.fret = (float) fret;
    e.label = (juce::uint8) label;
    e.rule = (juce::uint8) ruleBit;
    e.stringMask = mask;
    feed.push (e);
}

} // namespace luthier
