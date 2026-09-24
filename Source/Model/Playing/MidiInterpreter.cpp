#include "MidiInterpreter.h"

#include <algorithm>

namespace luthier
{

//==============================================================================
const char* getMidiTargetName (MidiTarget t) noexcept
{
    switch (t)
    {
        case MidiTarget::None:              return "None";
        case MidiTarget::VibratoDepth:      return "Vibrato Depth";
        case MidiTarget::VibratoRate:       return "Vibrato Rate";
        case MidiTarget::WhammyBar:         return "Whammy Bar";
        case MidiTarget::Expression:        return "Expression";
        case MidiTarget::MasterLevel:       return "Master Level";
        case MidiTarget::PalmMute:          return "Palm Mute";
        case MidiTarget::MutedPick:         return "Muted Pick";
        case MidiTarget::PickPosition:      return "Pick Position";
        case MidiTarget::SlideToggle:       return "Slide Mode";
        case MidiTarget::SlideGuitarToggle: return "Slide Guitar";
        case MidiTarget::PinchHarmonic:     return "Pinch Harmonic";
        case MidiTarget::NaturalHarmonic:   return "Natural Harmonic";
        case MidiTarget::Tap:               return "Tap";
        case MidiTarget::StrumSpeed:        return "Strum Speed";
        case MidiTarget::StrumDirection:    return "Strum Direction";
        case MidiTarget::Humanize:          return "Humanize";
        case MidiTarget::Drive:             return "Drive";
        case MidiTarget::Tone:              return "Tone";
        case MidiTarget::Space:             return "Space";
        case MidiTarget::Body:              return "Body";
        case MidiTarget::Attack:            return "Attack";
        case MidiTarget::NumTargets:
        default:                            return "None";
    }
}

//==============================================================================
void MidiInterpreter::prepare (double sampleRate, int strings)
{
    sr = sampleRate;
    numStrings = juce::jlimit (1, kMaxStrings, strings);

    stringBendRange.fill (2.0);
    resetChannelMap();
    autoArt.prepare (sampleRate, numStrings);   // FEAT-ASSIST
    setChordWindowMs (chordWindowMs);
    reset();
}

void MidiInterpreter::reset() noexcept
{
    for (auto& s : slots)
    {
        s.midiNote = -1;
        s.channel = -1;
        s.held = false;
        s.sostenutoHeld = false;
        s.bendCents = 0.0;
        s.pressure = 0.0;
        s.timbre = 0.0;
        s.startedAt = 0;
        s.releaseDueAt = -1;
        s.releaseWasLetRing = false;
    }

    numPending = 0;
    currentTimestamp = 0;
    sustainDown = false;
    sostenutoDown = false;
    vibratoDepth = 0.0;
    whammyPosition = 0.0;
    // Controller values go back to their defaults like the pedals above: left
    // as they were, the first render after a preset whose CC routing differs
    // starts from the old preset's last value and does not repeat.
    expressionValue = 0.5;
    pickPosition = 0.5;
    globalBendCents = 0.0;
    activeNoteCount = 0;
    lastMonoString = -1;
    activity = false;
    nextStrumIsUp = false;
    {
        const juce::SpinLock::ScopedLockType sl (lastChordLock);
        lastChordCount = 0;
    }

    // Deterministic humanisation after a reset, for reproducible renders.
    rng.setSeed (0x4D1D1ull);
    strumCount = 0;

    // FEAT-ASSIST (auto-articulation.md 4.1): history and hand position go too.
    autoArt.reset();
    pendingLegato.active = false;
    lastGroupArrival = -1000000000;
    lastGroupMask = 0;
    lastGroupSize = 0;
}

void MidiInterpreter::setNumStrings (int n) noexcept
{
    numStrings = juce::jlimit (1, kMaxStrings, n);
    autoArt.setNumStrings (numStrings);   // FEAT-ASSIST
}

void MidiInterpreter::setEngines (TuningEngine* t, TechniqueEngine* te, RubricVoicer* cv) noexcept
{
    tuning = t;
    technique = te;
    voicer = cv;
    autoArt.setTuning (t);   // FEAT-ASSIST
}

void MidiInterpreter::setPlayingMode (PlayingMode m) noexcept
{
    if (m == mode)
        return;

    mode = m;
    numPending = 0;
    pendingLegato.active = false;   // FEAT-ASSIST
}

void MidiInterpreter::setPitchBendRange (double semitones) noexcept
{
    bendRangeSemitones = juce::jlimit (0.5, 96.0, semitones);
}

void MidiInterpreter::setStringBendRange (int stringIndex, double semitones) noexcept
{
    if (juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        stringBendRange[(size_t) stringIndex] = juce::jlimit (0.5, 96.0, semitones);
}

//==============================================================================
void MidiInterpreter::resetChannelMap() noexcept
{
    // The default convention: channel 1 is the high E, and the strings run
    // upward from there.
    for (int s = 0; s < kMaxStrings; ++s)
        channelMap[(size_t) s] = s + 1;
}

void MidiInterpreter::setChannelForString (int stringIndex, int channel) noexcept
{
    if (juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        channelMap[(size_t) stringIndex] = juce::jlimit (0, 16, channel);
}

int MidiInterpreter::getChannelForString (int stringIndex) const noexcept
{
    return juce::isPositiveAndBelow (stringIndex, kMaxStrings)
             ? channelMap[(size_t) stringIndex] : 0;
}

void MidiInterpreter::setPitchDeadZoneCents (double cents) noexcept
{
    pitchDeadZoneCents = juce::jlimit (0.0, 100.0, cents);
}

void MidiInterpreter::setMinimumNoteDurationMs (double ms) noexcept
{
    minNoteDurationMs = juce::jlimit (0.0, 1000.0, ms);
}

void MidiInterpreter::setChordWindowMs (double ms) noexcept
{
    chordWindowMs = juce::jlimit (0.0, 50.0, ms);
    chordWindowSamples = juce::jmax (0, (int) (chordWindowMs * 0.001 * sr));
}

void MidiInterpreter::setStrumSpeedMs (double ms) noexcept
{
    strumSpeedMs = juce::jlimit (0.0, 60.0, ms);
}

int MidiInterpreter::getLatencySamples() const noexcept
{
    return (mode == PlayingMode::Poly) ? chordWindowSamples : 0;
}

//==============================================================================
void MidiInterpreter::resetCcMapToDefaults() noexcept
{
    ccMap.fill (MidiTarget::None);

    // Engine spec 2, "CC assignments".
    ccMap[1]  = MidiTarget::VibratoDepth;      // mod wheel
    ccMap[2]  = MidiTarget::WhammyBar;         // breath
    ccMap[4]  = MidiTarget::Expression;        // foot
    ccMap[11] = MidiTarget::MasterLevel;       // expression
    ccMap[65] = MidiTarget::SlideToggle;       // portamento
    ccMap[67] = MidiTarget::PalmMute;          // soft pedal

    // The user-mappable block, given sensible defaults rather than left blank.
    ccMap[70] = MidiTarget::PickPosition;
    ccMap[71] = MidiTarget::MutedPick;
    ccMap[72] = MidiTarget::PinchHarmonic;
    ccMap[73] = MidiTarget::NaturalHarmonic;
    ccMap[74] = MidiTarget::Tap;               // also MPE timbre; see handleController
    ccMap[75] = MidiTarget::SlideGuitarToggle;
    ccMap[76] = MidiTarget::StrumSpeed;
    ccMap[77] = MidiTarget::StrumDirection;
    ccMap[78] = MidiTarget::VibratoRate;
    ccMap[79] = MidiTarget::Humanize;
}

void MidiInterpreter::setCcTarget (int ccNumber, MidiTarget target) noexcept
{
    if (juce::isPositiveAndBelow (ccNumber, 128))
        ccMap[(size_t) ccNumber] = target;
}

MidiTarget MidiInterpreter::getCcTarget (int ccNumber) const noexcept
{
    if (! juce::isPositiveAndBelow (ccNumber, 128))
        return MidiTarget::None;

    return ccMap[(size_t) ccNumber];
}

//==============================================================================
int MidiInterpreter::stringForChannel (int channel) const noexcept
{
    /*  The map rather than arithmetic (controllers.md 4).

        The default map is the old convention - channel 1 is the high E - so
        nothing changes for a controller that follows it. A Roland GK, which puts
        the high E on channel 11, is now expressible without a special case. */
    for (int s = 0; s < numStrings; ++s)
        if (channelMap[(size_t) s] == channel)
            return s;

    return -1;
}

int MidiInterpreter::getStringMidiNote (int stringIndex) const noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return -1;

    return slots[(size_t) stringIndex].midiNote;
}

double MidiInterpreter::getStringBendCents (int stringIndex) const noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return globalBendCents;

    return globalBendCents + slots[(size_t) stringIndex].bendCents;
}

//==============================================================================
void MidiInterpreter::processBlock (const juce::MidiBuffer& midi,
                                    int numSamples,
                                    int64_t blockStartSample,
                                    PlayEventQueue& out) noexcept
{
    out.clear();
    blockStart = blockStartSample;
    blockLength = numSamples;
    assistBeginBlock();   // FEAT-ASSIST

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1), metadata.samplePosition);
        const int64_t timestamp = blockStartSample + offset;
        const int channel = message.getChannel();

        // FEAT-ASSIST (auto-articulation.md 3.3, 3.8, 5): a waiting legato note
        // whose time has come, and what the explicit rows need from this event.
        if (pendingLegato.active)
            assistBeforeEvent (timestamp, out);

        if (isAssistEffective())
        {
            offSharesNoteOn = false;

            if (message.isNoteOff())
                for (const auto other : midi)
                    if (other.samplePosition == metadata.samplePosition && other.getMessage().isNoteOn())
                        offSharesNoteOn = true;

            if (message.isPitchWheel())
                autoArt.noteBend ((mpeEnabled || mode == PlayingMode::GuitarController) ? -2 : -1, timestamp);

            if (message.isController())
            {
                const auto target = getCcTarget (message.getControllerNumber());

                if (target == MidiTarget::StrumSpeed || target == MidiTarget::StrumDirection)
                    autoArt.noteStrumController (timestamp);
            }
        }

        if (message.isNoteOn())
        {
            activity = true;
            handleNoteOn (message.getNoteNumber(), channel,
                          (double) message.getVelocity() / 127.0,
                          timestamp, offset, out);
        }
        else if (message.isNoteOff())
        {
            currentTimestamp = timestamp;
            handleNoteOff (message.getNoteNumber(), channel, offset, out);
        }
        else if (message.isPitchWheel())
        {
            const double normalised = ((double) message.getPitchWheelValue() - 8192.0) / 8192.0;

            if ((mpeEnabled || mode == PlayingMode::GuitarController) && channel > 0)
            {
                // Per-note / per-string bend.
                int target = -1;

                if (mode == PlayingMode::GuitarController)
                {
                    target = stringForChannel (channel);
                }
                else
                {
                    for (int s = 0; s < numStrings; ++s)
                        if (slots[(size_t) s].channel == channel && slots[(size_t) s].held)
                            target = s;
                }

                if (target >= 0)
                {
                    const double range = stringBendRange[(size_t) target];
                    const double cents = normalised * range * 100.0;

                    /*  controllers.md 5: some hex pickups never stop hunting for
                        the pitch of a sustained note, and emit a steady dribble
                        of small bends around it. Passing those through would
                        modulate the string engine's delay line continuously for
                        a note the player is holding still.

                        A bend inside the dead zone is ignored - but only while
                        the current bend is also inside it, so that a real bend
                        is never truncated on its way back to pitch. */
                    const bool bothInsideDeadZone =
                        pitchDeadZoneCents > 0.0
                          && std::abs (cents) < pitchDeadZoneCents
                          && std::abs (slots[(size_t) target].bendCents) < pitchDeadZoneCents;

                    if (! bothInsideDeadZone)
                    {
                        slots[(size_t) target].bendCents = cents;

                        BendEvent e;
                        e.stringIndex = target;
                        e.cents = cents;
                        e.sampleOffset = offset;
                        out.addBend (e);
                    }
                }
            }
            else
            {
                globalBendCents = normalised * bendRangeSemitones * 100.0;

                BendEvent e;
                e.stringIndex = -1;
                e.cents = globalBendCents;
                e.sampleOffset = offset;
                out.addBend (e);
            }
        }
        else if (message.isAftertouch() || message.isChannelPressure())
        {
            const double value = (double) (message.isAftertouch()
                                             ? message.getAfterTouchValue()
                                             : message.getChannelPressureValue()) / 127.0;

            int target = -1;

            if (mpeEnabled || mode == PlayingMode::GuitarController)
            {
                if (mode == PlayingMode::GuitarController)
                    target = stringForChannel (channel);
                else
                    for (int s = 0; s < numStrings; ++s)
                        if (slots[(size_t) s].channel == channel && slots[(size_t) s].held)
                            target = s;
            }

            if (target >= 0)
                slots[(size_t) target].pressure = value;

            PressureEvent e;
            e.stringIndex = target;
            e.value = value;
            e.sampleOffset = offset;
            out.addPressure (e);

            applyTarget (aftertouchTarget, value, offset, out);
        }
        else if (message.isController())
        {
            handleController (message.getControllerNumber(),
                              message.getControllerValue(),
                              channel, offset, out);
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            allNotesOff (out);
        }
    }

    // Close any chord group whose window has expired.
    flushChordGroup (blockStartSample + numSamples - chordWindowSamples, 0, numSamples, out);

    assistEndBlock (numSamples, out);   // FEAT-ASSIST: deadlines and controller state

    // And carry out any note-off that was held back for being too early.
    flushDeferredReleases (blockStartSample, numSamples, out);

    activeNoteCount = 0;

    for (int s = 0; s < numStrings; ++s)
        if (slots[(size_t) s].held)
            ++activeNoteCount;
}

//==============================================================================
void MidiInterpreter::handleNoteOn (int midiNote, int channel, double velocity,
                                    int64_t timestamp, int blockOffset, PlayEventQueue& out) noexcept
{
    if (tuning == nullptr || voicer == nullptr || technique == nullptr)
        return;

    currentTimestamp = timestamp;

    // Humanised velocity: no two strokes of a real hand are the same.
    const double velJitter = humanise.velocityVariation * humanise.amount;
    velocity = juce::jlimit (0.02, 1.0, velocity * (1.0 + rng.nextGaussian() * velJitter * 0.5));

    if (mode == PlayingMode::GuitarController)
    {
        int stringIndex = stringForChannel (channel);

        if (stringIndex < 0)
        {
            // Not a per-string channel: fall back to picking a string by pitch.
            const auto v = voicer->voiceSingleNote (midiNote, velocity, lastMonoString);

            if (! v.valid)
                return;

            stringIndex = v.stringIndex;
        }

        const double targetHz = midiToHz ((double) midiNote, tuning->getConcertA());
        double fret = tuning->frequencyToFretPosition (stringIndex, targetHz);

        /*  Identity rule 2: a note that cannot be played on this string is clipped
            into range rather than producing a nonsense pitch. The top of the range
            is the capo'd one (ambiguity-resolutions 4.5) - a capo at 5 makes the
            neck five frets shorter, and clamping to the raw fret count would put a
            note off the end of it. */
        fret = juce::jlimit (0.0, (double) tuning->getHighestPlayableFret (stringIndex), fret);

        VoicedNote v;
        v.midiNote = midiNote;
        v.stringIndex = stringIndex;
        v.fretPosition = fret;
        v.velocity = velocity;
        v.valid = true;

        slots[(size_t) stringIndex].channel = channel;
        emitVoicedNote (v, timestamp, blockOffset, 0, out);
        return;
    }

    if (mode == PlayingMode::Mono)
    {
        // FEAT-ASSIST (auto-articulation.md 4.2): planSingle instead of the voicer.
        if (assistMonoNoteOn (midiNote, channel, velocity, timestamp, blockOffset, out))
            return;

        const auto v = voicer->voiceSingleNote (midiNote, velocity, lastMonoString);

        if (! v.valid)
            return;

        lastMonoString = v.stringIndex;
        slots[(size_t) v.stringIndex].channel = channel;
        emitVoicedNote (v, timestamp, blockOffset, 0, out);
        return;
    }

    // ---- Poly mode: collect into a chord group -------------------------------
    // A note after the group's window has closed starts a group of its own.
    if (numPending > 0 && timestamp - pending[0].timestamp > chordWindowSamples)
        flushChordGroup (timestamp, blockOffset, blockOffset + 1, out);

    if (numPending >= kMaxPending)
        flushChordGroup (timestamp, blockOffset, blockOffset + 1, out);

    if (numPending < kMaxPending)
    {
        auto& p = pending[(size_t) numPending++];
        p.midiNote = midiNote;
        p.channel = channel;
        p.velocity = velocity;
        p.timestamp = timestamp;
        p.used = false;
        p.releasedAt = -1;
    }

    if (chordWindowSamples == 0)
        flushChordGroup (timestamp, blockOffset, blockOffset + 1, out);
}

//==============================================================================
void MidiInterpreter::flushChordGroup (int64_t upToSample, int blockOffset, int numSamples,
                                       PlayEventQueue& out) noexcept
{
    if (numPending == 0 || voicer == nullptr || tuning == nullptr)
        return;

    // Only flush notes whose window has closed.
    if (pending[0].timestamp > upToSample && chordWindowSamples > 0)
        return;

    int notes[kMaxPending];
    double velocities[kMaxPending];
    int64_t arrivals[kMaxPending];
    int64_t releases[kMaxPending];
    const int count = numPending;

    for (int i = 0; i < count; ++i)
    {
        notes[i] = pending[(size_t) i].midiNote;
        velocities[i] = pending[(size_t) i].velocity;
        arrivals[i] = pending[(size_t) i].timestamp;
        releases[i] = pending[(size_t) i].releasedAt;
    }

    const int64_t groupTimestamp = pending[0].timestamp;
    numPending = 0;

    /*  The group sounds when its window closes: its first note's sample plus
        the window, which is the latency getLatencySamples() reports, so after
        the host's compensation it lands where it was played. That sample is
        always inside the block doing the flush. (It used the flush's own
        offset, which at the end-of-block flush is 0, so every chord started
        at its block's first sample, up to a block early - found by
        midi-export 6's live PICK event test.) */
    blockOffset = (int) juce::jlimit ((int64_t) 0, (int64_t) juce::jmax (0, blockLength - 1),
                                      groupTimestamp + chordWindowSamples - blockStart);
    juce::ignoreUnused (numSamples);

    {
        // Try-locked: a UI reading the previous chord just keeps it one block.
        const juce::SpinLock::ScopedTryLockType sl (lastChordLock);

        if (sl.isLocked())
        {
            lastChordCount = juce::jmin (count, (int) lastChordNotes.size());
            std::copy (notes, notes + lastChordCount, lastChordNotes.begin());
        }
    }

    // FEAT-ASSIST (auto-articulation.md 3.1, 4.2): a single note is placed by
    // planSingle; a chord's voicer starts from the hand position.
    const bool assisted = isAssistEffective();

    if (assisted && count == 1
        && assistFlushSingle (notes[0], velocities[0], arrivals[0], releases[0], groupTimestamp, blockOffset, out))
        return;

    if (assisted && autoArt.rule (AssistRule::position))
        voicer->setPreferredPosition (autoArt.getHandPosition());

    const auto voicing = voicer->voice (notes, velocities, count);

    if (assisted)
        autoArt.setHandPositionFromVoicing (voicer->getLastScore().handPosition);

    if (voicing.numNotes == 0)
        return;

    // ---- strum ---------------------------------------------------------------
    // A single note is not strummed; two or more are.
    const bool isChord = voicing.numNotes > 1;

    bool strumUp = (strumDirection == StrumDirection::Up);

    if (strumDirection == StrumDirection::Alternate)
    {
        strumUp = nextStrumIsUp;
        nextStrumIsUp = ! nextStrumIsUp;
    }

    /*  strum-dynamics 1.1: MPE / hex routing (source 1) and a chord the player
        rolled (source 2) carry their own timing: each note sounds at its own
        arrival, one window late like every chord, and nothing is synthesised -
        "a player who rolls a chord gets their roll". Only a chord that arrived
        all at once is strummed, as one gesture at the global crossing (source 4). */
    int64_t lastArrival = groupTimestamp;

    for (int i = 0; i < count; ++i)
        lastArrival = juce::jmax (lastArrival, arrivals[i]);

    const bool playedSpread = mpeEnabled || lastArrival > groupTimestamp;

    std::array<StrumStrike, kMaxStrings> strikes {};
    int planned = 0;

    // FEAT-ASSIST (auto-articulation.md 3.7): the chord's plan and its own timing.
    AssistPlan chordPlan;
    std::array<double, kMaxStrings> assistDelays {};
    bool assistTimed = false;

    if (assisted)
        assistPlanChordNotes (voicing, groupTimestamp, playedSpread, chordPlan);

    if (isChord && ! playedSpread && (strumSpeedMs > 0.0 || assisted))
    {
        std::array<int, kMaxStrings> order {};
        int numOrdered = 0;

        for (int i = 0; i < voicing.numNotes && numOrdered < kMaxStrings; ++i)
            if (voicing.notes[(size_t) i].valid)
                order[(size_t) numOrdered++] = voicing.notes[(size_t) i].stringIndex;

        // A downstroke crosses the low strings first: string index counts down
        // from the high E, so the low strings have the HIGHEST index.
        if (strumUp)
            std::sort (order.begin(), order.begin() + numOrdered);
        else
            std::sort (order.begin(), order.begin() + numOrdered, [] (int a, int b) { return a > b; });

        // Human strums vary in speed; a machine-even strum is instantly recognisable.
        // FEAT-ASSIST: no draw where today makes none (0.1), so Humanize's sequence is the same.
        const double speedVar = strumSpeedMs > 0.0 ? 1.0 + rng.nextGaussian() * humanise.strumSpeedVariation
                                                               * humanise.amount * 0.5
                                                   : 1.0;

        // Striker and chuck are the rhythm engine's; a live chord is the player's pick.
        auto live = strumSettings;
        live.strikerDown = live.strikerUp = Striker::pick;

        StrumRequest request;
        request.strings = order.data();
        request.numStrings = numOrdered;
        request.down = ! strumUp;
        request.sourceSps = 1000.0 / (strumSpeedMs * juce::jmax (0.05, speedVar));
        request.strumIndex = strumCount++;
        request.missScale = 0.0;   // a key the player pressed always sounds

        if (assisted)
            assistTimed = assistPlanStrum (request, order.data(), numOrdered, voicing, groupTimestamp,
                                           speedVar, chordPlan, assistDelays);

        planned = assistTimed ? 0 : strumGesture.plan (live, request, strikes.data(), (int) strikes.size());

        if (assisted && planned > 0)
            assistShapeStrikes (strikes.data(), planned, chordPlan);
    }

    for (int i = 0; i < voicing.numNotes; ++i)
    {
        const auto& note = voicing.notes[(size_t) i];

        if (! note.valid)
            continue;

        int delaySamples = 0;
        double velocityScale = 1.0;

        if (planned > 0)
        {
            for (int k = 0; k < planned; ++k)
            {
                if (strikes[(size_t) k].stringIndex == note.stringIndex)
                {
                    delaySamples = (int) std::round (strikes[(size_t) k].timeSeconds * sr);
                    velocityScale = strikes[(size_t) k].force;
                    break;
                }
            }
        }
        else if (assistTimed)
        {
            delaySamples = (int) std::round (assistDelays[(size_t) i] * sr);   // FEAT-ASSIST
        }
        else if (playedSpread)
        {
            for (int k = 0; k < count; ++k)
            {
                if (notes[k] == note.midiNote)
                {
                    delaySamples = (int) (arrivals[k] - groupTimestamp);
                    break;
                }
            }
        }

        auto humanised = note;
        humanised.velocity = juce::jlimit (0.02, 1.0, note.velocity * velocityScale);

        if (assisted)
            assistEmitChordNote (humanised, chordPlan, i, arrivals, notes, count, groupTimestamp,
                                 blockOffset, delaySamples, out);   // FEAT-ASSIST
        else
            emitVoicedNote (humanised, groupTimestamp, blockOffset, delaySamples, out);

        // Released before its window closed: it still sounds, for as long as it
        // was held, and flushDeferredReleases ends it. The floor keeps the
        // note-off after the note-on whatever strum delay or jitter moved it.
        for (int k = 0; k < count; ++k)
        {
            if (notes[k] != note.midiNote || releases[k] < 0)
                continue;

            auto& slot = slots[(size_t) juce::jlimit (0, numStrings - 1, note.stringIndex)];
            const int64_t heldFor = juce::jmax (releases[k] - arrivals[k], (int64_t) (0.010 * sr));

            slot.releaseDueAt = groupTimestamp + chordWindowSamples + delaySamples + heldFor;
            slot.releaseWasLetRing = sustainDown || slot.sostenutoHeld;
            releases[k] = -1;
            break;
        }
    }

    juce::ignoreUnused (numSamples);
}

//==============================================================================
void MidiInterpreter::emitVoicedNote (const VoicedNote& note, int64_t timestamp,
                                      int blockOffset, int extraDelaySamples,
                                      PlayEventQueue& out) noexcept
{
    if (! note.valid || tuning == nullptr || technique == nullptr)
        return;

    const int s = juce::jlimit (0, numStrings - 1, note.stringIndex);

    // Timing jitter: a real player is never exactly on the grid.
    const double jitterMs = humanise.timingJitterMs * humanise.amount;
    const int jitterSamples = (int) (rng.nextGaussian() * jitterMs * 0.001 * sr * 0.5);

    const int offset = juce::jmax (0, blockOffset + extraDelaySamples + jitterSamples);

    // If this string is already sounding, that note ends here.
    if (slots[(size_t) s].held)
        technique->noteEnded (s, timestamp);

    int harmonicPartial = 0;
    double slideFromFret = -1.0;

    // FEAT-ASSIST: an assisted note also learns whether a controller chose it.
    bool explicitTech = false;
    const auto tech = currentPlan != nullptr
                        ? technique->decide (s, note.fretPosition, note.velocity, timestamp + extraDelaySamples,
                                             harmonicPartial, slideFromFret, explicitTech)
                        : technique->decide (s, note.fretPosition, note.velocity,
                                             timestamp + extraDelaySamples,
                                             harmonicPartial, slideFromFret);

    // Micro-detune, refreshed per note: identity rule for realism, and the thing
    // that stops repeated notes sounding like a sampler.
    const double detune = rng.nextGaussian() * humanise.microDetuneCents * humanise.amount * 0.5;

    NoteOnEvent e;
    e.stringIndex = s;
    e.midiNote = note.midiNote;
    e.midiChannel = slots[(size_t) s].channel;
    e.fretPosition = note.fretPosition;
    e.velocity = note.velocity;
    e.technique = tech;
    e.harmonicPartial = harmonicPartial;
    e.sampleOffset = offset;
    e.slideFromFret = slideFromFret;
    e.slideSeconds = (slideFromFret >= 0.0)
                       ? technique->slideDurationFor (note.fretPosition - slideFromFret)
                       : 0.0;

    e.pitchHz = tuning->computeFrequency (s, note.fretPosition,
                                          getStringBendCents (s) + detune);

    if (currentPlan != nullptr)
        assistDecorate (e, tech, explicitTech, blockStart + offset);   // FEAT-ASSIST

    out.addNoteOn (e);

    slots[(size_t) s].midiNote = note.midiNote;
    slots[(size_t) s].held = true;
    slots[(size_t) s].startedAt = timestamp;
    slots[(size_t) s].releaseDueAt = -1;
}

//==============================================================================
juce::String MidiInterpreter::getLastChordName() const
{
    std::array<int, 16> chord {};
    int count = 0;

    {
        const juce::SpinLock::ScopedLockType sl (lastChordLock);
        chord = lastChordNotes;
        count = lastChordCount;
    }

    return count > 0 ? ChordVoicer::identifyChord (chord.data(), count) : juce::String();
}

void MidiInterpreter::handleNoteOff (int midiNote, int channel, int blockOffset,
                                     PlayEventQueue& out) noexcept
{
    // FEAT-ASSIST (auto-articulation.md 3.2 hand-over, 3.3, 3.8 fall).
    if (assistNoteOff (midiNote, channel, blockOffset, out))
        return;

    // The string that played this note on this channel first: a hex pickup
    // playing a unison on two strings sends the same note on two channels, and
    // releasing one must not stop the other.
    for (int s = 0; s < numStrings; ++s)
    {
        const auto& slot = slots[(size_t) s];

        if (slot.held && slot.midiNote == midiNote && slot.channel == channel)
        {
            releaseString (s, blockOffset, out);
            return;
        }
    }

    // A note still waiting in the chord window that is released before the window
    // closes was a mistake or a very short stab; let it through anyway so it sounds,
    // then release it.
    for (int s = 0; s < numStrings; ++s)
    {
        if (slots[(size_t) s].held && slots[(size_t) s].midiNote == midiNote)
        {
            releaseString (s, blockOffset, out);
            return;
        }
    }

    // Still waiting in the chord window: remembered, and ended once voiced
    // (flushChordGroup). Dropping it left the note sounding for ever.
    for (int i = 0; i < numPending; ++i)
    {
        auto& p = pending[(size_t) i];

        if (p.midiNote == midiNote && p.releasedAt < 0)
        {
            p.releasedAt = currentTimestamp;
            return;
        }
    }
}

void MidiInterpreter::flushDeferredReleases (int64_t blockStartSample, int numSamples,
                                             PlayEventQueue& out) noexcept
{
    for (int s = 0; s < numStrings; ++s)
    {
        auto& slot = slots[(size_t) s];

        if (slot.releaseDueAt < 0 || ! slot.held)
            continue;

        if (slot.releaseDueAt >= blockStartSample + numSamples)
            continue;

        const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                         (int) (slot.releaseDueAt - blockStartSample));

        NoteOffEvent e;
        e.stringIndex = s;
        e.midiNote = slot.midiNote;
        e.sampleOffset = offset;
        e.letRing = slot.releaseWasLetRing;
        out.addNoteOff (e);

        slot.held = false;
        slot.midiNote = -1;
        slot.bendCents = 0.0;
        slot.pressure = 0.0;
        slot.releaseDueAt = -1;

        if (technique != nullptr && ! slot.releaseWasLetRing)
            technique->noteEnded (s, 0);
    }
}

void MidiInterpreter::releaseString (int stringIndex, int blockOffset, PlayEventQueue& out) noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, kMaxStrings))
        return;

    auto& slot = slots[(size_t) stringIndex];

    if (! slot.held)
        return;

    // The sustain pedal lets every string ring; sostenuto holds only the notes
    // that were down when the pedal was pressed.
    const bool letRing = sustainDown || slot.sostenutoHeld;

    /*  controllers.md 5: a controller with lazy note-offs sends one far too
        early. Rather than dropping it - which would leave the note hanging if no
        second one ever came - it is deferred to the earliest moment the note is
        allowed to end, and flushDeferredReleases carries it out then. */
    if (minNoteDurationMs > 0.0)
    {
        const int64_t minimumSamples = (int64_t) (minNoteDurationMs * 0.001 * sr);
        const int64_t dueAt = slot.startedAt + minimumSamples;

        if (currentTimestamp < dueAt)
        {
            slot.releaseDueAt = dueAt;
            slot.releaseWasLetRing = letRing;
            return;
        }
    }

    NoteOffEvent e;
    e.stringIndex = stringIndex;
    e.midiNote = slot.midiNote;
    e.sampleOffset = blockOffset;
    e.letRing = letRing;
    out.addNoteOff (e);

    slot.held = false;
    slot.midiNote = -1;
    slot.bendCents = 0.0;
    slot.pressure = 0.0;
    slot.releaseDueAt = -1;

    if (technique != nullptr && ! letRing)
        technique->noteEnded (stringIndex, 0);
}

//==============================================================================
void MidiInterpreter::handleController (int cc, int value, int channel,
                                        int blockOffset, PlayEventQueue& out) noexcept
{
    const double v = (double) value / 127.0;

    ControlEvent ce;
    ce.ccNumber = cc;
    ce.value = v;
    ce.sampleOffset = blockOffset;
    out.addControl (ce);

    // ---- pedals, which are not remappable -----------------------------------
    if (cc == 64)
    {
        const bool down = value >= 64;

        if (sustainDown && ! down)
        {
            // Releasing the pedal drops every string whose key is already up.
            for (int s = 0; s < numStrings; ++s)
                if (! slots[(size_t) s].held && ! slots[(size_t) s].sostenutoHeld)
                    continue;
        }

        sustainDown = down;
        return;
    }

    if (cc == 66)
    {
        const bool down = value >= 64;

        if (down && ! sostenutoDown)
        {
            for (int s = 0; s < numStrings; ++s)
                slots[(size_t) s].sostenutoHeld = slots[(size_t) s].held;
        }
        else if (! down)
        {
            for (int s = 0; s < numStrings; ++s)
                slots[(size_t) s].sostenutoHeld = false;
        }

        sostenutoDown = down;
        return;
    }

    if (cc == 123 || cc == 120)
    {
        allNotesOff (out);
        return;
    }

    // ---- MPE timbre ----------------------------------------------------------
    // CC 74 is the MPE Y-axis. When MPE is on it belongs to the note, not to the
    // global technique map.
    if (cc == 74 && mpeEnabled)
    {
        for (int s = 0; s < numStrings; ++s)
            if (slots[(size_t) s].channel == channel && slots[(size_t) s].held)
                slots[(size_t) s].timbre = v;

        return;
    }

    applyTarget (getCcTarget (cc), v, blockOffset, out);
}

//==============================================================================
void MidiInterpreter::applyTarget (MidiTarget target, double value, int blockOffset,
                                   PlayEventQueue& out) noexcept
{
    juce::ignoreUnused (blockOffset, out);

    if (technique == nullptr)
        return;

    switch (target)
    {
        case MidiTarget::VibratoDepth:      vibratoDepth = value; break;
        case MidiTarget::VibratoRate:       vibratoRate = juce::jmap (value, 3.0, 8.0); break;
        case MidiTarget::WhammyBar:         whammyPosition = value * 2.0 - 1.0; break;
        case MidiTarget::Expression:        expressionValue = value; break;
        case MidiTarget::PickPosition:      pickPosition = value; break;
        case MidiTarget::PalmMute:          technique->setPalmMuteAmount (value); break;
        case MidiTarget::MutedPick:         technique->setMutedPickAmount (value); break;
        case MidiTarget::SlideToggle:       technique->setSlideMode (value >= 0.5); break;
        case MidiTarget::SlideGuitarToggle: technique->setSlideGuitarMode (value >= 0.5); break;
        case MidiTarget::PinchHarmonic:     technique->setPinchHarmonicTrigger (value >= 0.5); break;
        case MidiTarget::NaturalHarmonic:   technique->setNaturalHarmonicTrigger (value >= 0.5); break;
        case MidiTarget::Tap:               technique->setTapTrigger (value >= 0.5); break;
        case MidiTarget::StrumSpeed:        setStrumSpeedMs (juce::jmap (value, 0.0, 30.0)); break;

        case MidiTarget::StrumDirection:
            strumDirection = (value < 0.33) ? StrumDirection::Down
                           : (value < 0.66) ? StrumDirection::Up
                                            : StrumDirection::Alternate;
            break;

        case MidiTarget::Humanize:          humanise.amount = value; break;

        // These land on plugin parameters rather than on the interpreter; the
        // processor reads them from the control events in the queue.
        case MidiTarget::MasterLevel:
        case MidiTarget::Drive:
        case MidiTarget::Tone:
        case MidiTarget::Space:
        case MidiTarget::Body:
        case MidiTarget::Attack:
        case MidiTarget::None:
        case MidiTarget::NumTargets:
        default:
            break;
    }
}

//==============================================================================
void MidiInterpreter::allNotesOff (PlayEventQueue& out) noexcept
{
    numPending = 0;
    pendingLegato.active = false;   // FEAT-ASSIST

    for (int s = 0; s < numStrings; ++s)
    {
        slots[(size_t) s].sostenutoHeld = false;

        if (slots[(size_t) s].held)
        {
            NoteOffEvent e;
            e.stringIndex = s;
            e.midiNote = slots[(size_t) s].midiNote;
            e.sampleOffset = 0;
            e.letRing = false;
            out.addNoteOff (e);
        }

        slots[(size_t) s].held = false;
        slots[(size_t) s].midiNote = -1;
        slots[(size_t) s].bendCents = 0.0;
        slots[(size_t) s].pressure = 0.0;

        if (technique != nullptr)
            technique->noteEnded (s, 0);
    }

    sustainDown = false;
    sostenutoDown = false;
    activeNoteCount = 0;
}

} // namespace luthier
