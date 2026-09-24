/*  MidiInterpreter's Performance Assist hooks (auto-articulation.md 4.2).

    Kept out of MidiInterpreter.cpp so the interpreter's own logic reads as it
    did: each hook there is one marked line that calls in here, and every one
    of them returns straight away unless isAssistEffective(). With Assist off
    nothing below runs, nothing draws from the interpreter's RtRandom, and the
    events are the ones a build without this feature makes (0.1).
*/

#include "MidiInterpreter.h"

#include <algorithm>

namespace luthier
{

//==============================================================================
void MidiInterpreter::setAssistContext (const AssistExplicitContext& c, const AssistTransport& t,
                                        int64_t blockStartSample) noexcept
{
    auto context = c;
    context.slideMode = technique != nullptr && technique->isSlideMode();
    autoArt.setExplicitContext (context);
    autoArt.setTransport (t, blockStartSample);
}

bool MidiInterpreter::isAssistEffective() const noexcept
{
    return autoArt.isEnabledBySettings()
        && mode != PlayingMode::GuitarController
        && ! mpeEnabled
        && ! autoArt.getExplicitContext().preArticulated
        && ! autoArt.getExplicitContext().rhythmDriving;
}

AssistBypass MidiInterpreter::getAssistBypass() const noexcept
{
    if (! autoArt.isEnabledBySettings())
        return AssistBypass::off;

    if (mode == PlayingMode::GuitarController || mpeEnabled)
        return AssistBypass::guitarControllerOrMpe;

    return AssistBypass::none;
}

bool MidiInterpreter::assistNoteIsExplicit (double velocity) const noexcept
{
    const auto& c = autoArt.getExplicitContext();

    return c.slapHeld || c.tapArmed
        || (c.slapVelocityZone && (int) std::lround (velocity * 127.0) >= c.slapZoneVelocity);
}

//==============================================================================
void MidiInterpreter::assistBeginBlock() noexcept
{
    const bool effective = isAssistEffective();
    autoArt.setEffectiveNow (effective, blockStart);

    // 4.2: TechniqueEngine's own legato inference steps aside only while
    // Assist owns legato; every other case gets today's inference back.
    if (technique != nullptr)
        technique->setLegatoInferenceEnabled (! (effective && (autoArt.rule (AssistRule::legato)
                                                                 || autoArt.rule (AssistRule::slide))));
}

void MidiInterpreter::assistBeforeEvent (int64_t timestamp, PlayEventQueue& out) noexcept
{
    if (pendingLegato.active && pendingLegato.plan.deferUntil <= timestamp)
        assistResolvePending (pendingLegato.plan.deferUntil, false, out);
}

void MidiInterpreter::assistEndBlock (int numSamples, PlayEventQueue& out) noexcept
{
    autoArt.noteControllerVibrato (vibratoDepth);

    if (pendingLegato.active && pendingLegato.plan.deferUntil < blockStart + numSamples)
        assistResolvePending (pendingLegato.plan.deferUntil, false, out);
}

//==============================================================================
void MidiInterpreter::assistEmit (const VoicedNote& note, const AssistPlan& plan, int64_t arrival,
                                  int64_t timestamp, int blockOffset, int extraDelay, PlayEventQueue& out) noexcept
{
    currentPlan = &plan;
    currentArrival = arrival;
    emitVoicedNote (note, timestamp, blockOffset, extraDelay, out);
    currentPlan = nullptr;
}

void MidiInterpreter::assistDecorate (NoteOnEvent& e, Technique decided, bool explicitTech,
                                      int64_t soundSample) noexcept
{
    if (currentPlan == nullptr)
        return;

    autoArt.decorate (e, decided, explicitTech, currentArrival, soundSample, *currentPlan);

    // The legato rules chose a slide: its time is TechniqueEngine's (3.3).
    if (e.slideFromFret >= 0.0 && e.slideSeconds <= 0.0 && technique != nullptr)
        e.slideSeconds = technique->slideDurationFor (e.fretPosition - e.slideFromFret);
}

void MidiInterpreter::assistResolvePending (int64_t atSample, bool sourceReleased, PlayEventQueue& out) noexcept
{
    auto p = pendingLegato;
    pendingLegato.active = false;

    autoArt.resolveLegato (p.plan, sourceReleased);

    const int s = juce::jlimit (0, numStrings - 1, p.plan.note.stringIndex);

    if (mode == PlayingMode::Mono)
        slots[(size_t) s].channel = p.channel;

    const int offset = (int) juce::jlimit ((int64_t) 0, (int64_t) juce::jmax (0, blockLength - 1), atSample - blockStart);
    assistEmit (p.plan.note, p.plan.plan, p.arrival, atSample, offset, 0, out);
}

//==============================================================================
bool MidiInterpreter::assistMonoNoteOn (int midiNote, int channel, double velocity, int64_t timestamp,
                                        int blockOffset, PlayEventQueue& out) noexcept
{
    if (! isAssistEffective())
        return false;

    // A waiting legato note is decided before the next one: the player moved on.
    if (pendingLegato.active)
        assistResolvePending (timestamp, true, out);

    autoArt.noteControllerVibrato (vibratoDepth);
    auto context = autoArt.getExplicitContext();
    context.slideMode = technique->isSlideMode();
    autoArt.setExplicitContext (context);

    AutoArticulator::SinglePlan sp;

    if (! assistNoteIsExplicit (velocity))
        sp = autoArt.planSingle (midiNote, velocity, timestamp, false, 0);

    if (! sp.note.valid)
    {
        // 13: not playable anywhere (or explicit): the voicer, as today, recorded
        // in the history so held state stays whole.
        const auto v = voicer->voiceSingleNote (midiNote, velocity, lastMonoString);

        if (! v.valid)
            return true;

        lastMonoString = v.stringIndex;
        slots[(size_t) v.stringIndex].channel = channel;

        const AssistPlan none;
        assistEmit (v, none, timestamp, timestamp, blockOffset, 0, out);
        return true;
    }

    lastMonoString = sp.note.stringIndex;

    if (sp.deferUntil > timestamp)
    {
        pendingLegato.active = true;
        pendingLegato.plan = sp;
        pendingLegato.channel = channel;
        pendingLegato.arrival = timestamp;
        return true;
    }

    if (sp.deferUntil >= 0)
        autoArt.resolveLegato (sp, false);

    slots[(size_t) sp.note.stringIndex].channel = channel;
    assistEmit (sp.note, sp.plan, timestamp, timestamp, blockOffset, 0, out);
    return true;
}

bool MidiInterpreter::assistFlushSingle (int midiNote, double velocity, int64_t arrival, int64_t releasedAt,
                                         int64_t groupTimestamp, int blockOffset, PlayEventQueue& out) noexcept
{
    const int64_t flushAt = groupTimestamp + chordWindowSamples;

    if (pendingLegato.active)
        assistResolvePending (juce::jmax (flushAt, pendingLegato.arrival + chordWindowSamples), true, out);

    autoArt.noteControllerVibrato (vibratoDepth);
    auto context = autoArt.getExplicitContext();
    context.slideMode = technique != nullptr && technique->isSlideMode();
    autoArt.setExplicitContext (context);

    if (assistNoteIsExplicit (velocity))
        return false;

    // 3.1 late join: the player's roll into a chord that is still held.
    const auto& st = autoArt.style();
    bool heldGroup = false;

    for (int s = 0; s < numStrings; ++s)
        if (((lastGroupMask >> s) & 1u) != 0 && autoArt.isStringHeldAt (s, arrival))
            heldGroup = true;

    const bool lateJoin = lastGroupSize >= 2 && heldGroup
                          && arrival > lastGroupArrival + chordWindowSamples
                          && (double) (arrival - lastGroupArrival) * 1000.0 / sr <= st.lateJoinMs;

    auto sp = autoArt.planSingle (midiNote, velocity, arrival, lateJoin, lateJoin ? lastGroupMask : 0u);

    if (! sp.note.valid)
        return false;

    if (sp.deferUntil >= 0)
    {
        if (releasedAt >= 0)
        {
            autoArt.resolveLegato (sp, true);      // the note itself was already let go
        }
        else if (sp.deferUntil > flushAt)
        {
            pendingLegato.active = true;
            pendingLegato.plan = sp;
            pendingLegato.channel = 1;
            pendingLegato.arrival = arrival;
            return true;
        }
        else
        {
            autoArt.resolveLegato (sp, false);
        }
    }

    assistEmit (sp.note, sp.plan, arrival, groupTimestamp, blockOffset, 0, out);

    // Released before its window closed: it sounds for as long as it was held
    // (flushChordGroup's rule for chord notes).
    if (releasedAt >= 0)
    {
        const int s = juce::jlimit (0, numStrings - 1, sp.note.stringIndex);
        auto& slot = slots[(size_t) s];
        const int64_t heldFor = juce::jmax (releasedAt - arrival, (int64_t) (0.010 * sr));

        slot.releaseDueAt = flushAt + heldFor;
        slot.releaseWasLetRing = sustainDown || slot.sostenutoHeld;
        autoArt.onNoteOff (midiNote, s, releasedAt, true);
    }

    return true;
}

//==============================================================================
void MidiInterpreter::assistPlanChordNotes (const ChordVoicing& voicing, int64_t groupTimestamp,
                                            bool playedSpread, AssistPlan& chordPlan) noexcept
{
    juce::ignoreUnused (playedSpread);

    chordPlan = AssistPlan {};
    chordPlan.assisted = true;
    chordPlan.chordMember = voicing.numNotes > 1;
    chordPlan.chordSize = voicing.numNotes;

    const auto& st = autoArt.style();
    bool inRegister = st.palmMuteRegister >= 0;
    juce::uint32 mask = 0;

    for (int i = 0; i < voicing.numNotes; ++i)
    {
        const auto& n = voicing.notes[(size_t) i];

        if (! n.valid)
            continue;

        mask |= 1u << juce::jlimit (0, 31, n.stringIndex);

        if (autoArt.semitonesAboveLowestOpen (n.midiNote) > st.palmMuteRegister)
            inRegister = false;
    }

    chordPlan.chordInRegister = inRegister;

    if (voicing.numNotes >= 2)
    {
        lastGroupArrival = groupTimestamp;
        lastGroupMask = mask;
        lastGroupSize = voicing.numNotes;
    }
}

bool MidiInterpreter::assistPlanStrum (StrumRequest& request, int* order, int numOrdered,
                                       const ChordVoicing& voicing, int64_t groupTimestamp,
                                       double speedVariation, AssistPlan& chordPlan,
                                       std::array<double, kMaxStrings>& delays) noexcept
{
    // 3.7's style exceptions: Fingerstyle's pinch roll, Bass's together.
    std::array<int, kMaxStrings> strs {}, mids {}, index {};
    std::array<double, kMaxStrings> d {};
    int n = 0;
    double peak = 0.0;

    for (int i = 0; i < voicing.numNotes && n < kMaxStrings; ++i)
    {
        const auto& v = voicing.notes[(size_t) i];

        if (! v.valid)
            continue;

        strs[(size_t) n] = v.stringIndex;
        mids[(size_t) n] = v.midiNote;
        index[(size_t) n] = i;
        peak = juce::jmax (peak, v.velocity);
        ++n;
    }

    if (autoArt.planRollOrTogether (strs.data(), mids.data(), n, d.data()))
    {
        for (int k = 0; k < n; ++k)
            delays[(size_t) index[(size_t) k]] = d[(size_t) k];

        return true;
    }

    if (! autoArt.planStrum (request, groupTimestamp, peak, speedVariation, chordPlan))
        return false;

    // The direction may differ from the interpreter's: re-order the crossing.
    if (request.down)
        std::sort (order, order + numOrdered, [] (int a, int b) { return a > b; });
    else
        std::sort (order, order + numOrdered);

    juce::uint16 mask = 0;

    for (int k = 0; k < numOrdered; ++k)
        mask = (juce::uint16) (mask | (1u << juce::jlimit (0, 15, order[k])));

    chordPlan.strumMask = mask;
    return false;
}

void MidiInterpreter::assistShapeStrikes (StrumStrike* strikes, int count, const AssistPlan& chordPlan) const noexcept
{
    if (! chordPlan.strummed || ! chordPlan.strumUp || count <= 1)
        return;

    // 3.7: an up-stroke favours the high strings. Lowness is 0 on the highest
    // struck string (lowest index) and 1 on the lowest.
    int lo = kMaxStrings, hi = -1;

    for (int k = 0; k < count; ++k)
    {
        lo = juce::jmin (lo, strikes[k].stringIndex);
        hi = juce::jmax (hi, strikes[k].stringIndex);
    }

    if (hi <= lo)
        return;

    for (int k = 0; k < count; ++k)
    {
        const double lowness = (double) (strikes[k].stringIndex - lo) / (double) (hi - lo);
        strikes[k].force *= 1.0 - 0.35 * lowness;
    }
}

void MidiInterpreter::assistEmitChordNote (const VoicedNote& note, const AssistPlan& chordPlan, int voicingIndex,
                                           const int64_t* arrivals, const int* notesIn, int count,
                                           int64_t groupTimestamp, int blockOffset, int delaySamples,
                                           PlayEventQueue& out) noexcept
{
    auto plan = chordPlan;
    int64_t arrival = groupTimestamp;

    for (int k = 0; k < count; ++k)
        if (notesIn[k] == note.midiNote)
        {
            arrival = arrivals[k];
            break;
        }

    if (assistNoteIsExplicit (note.velocity))
        plan.assisted = false;

    // The strum's arrow is drawn once, from the chord's first note.
    if (voicingIndex != 0)
        plan.strumMask = 0;

    assistEmit (note, plan, arrival, groupTimestamp, blockOffset, delaySamples, out);
}

//==============================================================================
bool MidiInterpreter::assistNoteOff (int midiNote, int channel, int blockOffset, PlayEventQueue& out) noexcept
{
    juce::ignoreUnused (blockOffset);

    if (! isAssistEffective())
        return false;

    const int64_t t = currentTimestamp;

    if (pendingLegato.active)
    {
        const int src = juce::jlimit (0, numStrings - 1, pendingLegato.plan.sourceString);
        const auto& srcSlot = slots[(size_t) src];

        // 3.2 hand-over: the source let go first - the legato note takes its
        // string now, and no release is heard between the two.
        if (midiNote == pendingLegato.plan.sourceMidi && srcSlot.held && srcSlot.midiNote == midiNote)
        {
            autoArt.onNoteOff (midiNote, src, t, true);
            assistResolvePending (t, true, out);
            return true;
        }

        // The legato note itself let go first: it sounds, briefly, and ends.
        if (midiNote == pendingLegato.plan.note.midiNote)
        {
            const int s = juce::jlimit (0, numStrings - 1, pendingLegato.plan.note.stringIndex);
            assistResolvePending (t, true, out);
            autoArt.onNoteOff (midiNote, s, t, true);

            auto& slot = slots[(size_t) s];
            slot.releaseDueAt = t + (int64_t) (0.010 * sr);
            slot.releaseWasLetRing = sustainDown || slot.sostenutoHeld;
            return true;
        }
    }

    int s = -1;

    for (int i = 0; i < numStrings && s < 0; ++i)
        if (slots[(size_t) i].held && slots[(size_t) i].midiNote == midiNote && slots[(size_t) i].channel == channel)
            s = i;

    for (int i = 0; i < numStrings && s < 0; ++i)
        if (slots[(size_t) i].held && slots[(size_t) i].midiNote == midiNote)
            s = i;

    if (s < 0)
        return false;

    auto& slot = slots[(size_t) s];

    // 3.8 fall: the release waits while the pitch glides down.
    const int fall = autoArt.onNoteOff (midiNote, s, t, offSharesNoteOn || numPending > 0);

    if (fall > 0)
    {
        slot.releaseDueAt = t + fall;
        slot.releaseWasLetRing = sustainDown || slot.sostenutoHeld;
        return true;
    }

    // 3.2 hand-over in Poly: a note waiting in the chord window may take this
    // string when it is flushed; the release waits for that sample.
    if (mode == PlayingMode::Poly && numPending > 0 && t >= pending[0].timestamp)
    {
        slot.releaseDueAt = pending[0].timestamp + chordWindowSamples;
        slot.releaseWasLetRing = sustainDown || slot.sostenutoHeld;
        return true;
    }

    return false;
}

} // namespace luthier
