#include "RhythmEngine.h"
#include "MutedThump.h"   // REALISM-B: string-interaction.md 6

namespace luthier
{

//==============================================================================
const char* getVoicingStyleName (VoicingStyle style) noexcept
{
    switch (style)
    {
        case VoicingStyle::open:     return "Open";
        case VoicingStyle::barre:    return "Barre";
        case VoicingStyle::triad:    return "Triad";
        case VoicingStyle::shell:    return "Shell";
        case VoicingStyle::drop2:    return "Drop 2";
        case VoicingStyle::drop3:    return "Drop 3";
        case VoicingStyle::power:    return "Power";
        case VoicingStyle::rootless: return "Rootless";
        case VoicingStyle::wide:     return "Wide";
        case VoicingStyle::bass:     return "Bass";
        case VoicingStyle::numStyles:
        default:                     return "Open";
    }
}


//==============================================================================
RhythmEngine::RhythmEngine()
{
    // A pattern that does nothing is the honest default: the engine is off until
    // the user chooses something for it to play.
    patternBuffer.resetAll (writtenPattern);
    bassGridBuffer.resetAll (writtenBassGrid);
    humaniseBuffer.resetAll (writtenHumanise);
}

void RhythmEngine::prepare (double sampleRate, int maxBlockSize,
                            TuningEngine* tuningEngine, RubricVoicer* chordVoicer) noexcept
{
    sr = juce::jmax (1.0, sampleRate);
    maxBlock = juce::jmax (1, maxBlockSize);
    tuning = tuningEngine;
    voicer = chordVoicer;

    detector.prepare (sr);
    reset();
}

void RhythmEngine::reset() noexcept
{
    detector.reset();
    currentChord = ChordSymbol {};
    currentVoicing = ChordVoicing {};
    voicingValid = false;

    freeRunPpq = 0.0;
    lastPpq = -1.0;
    wasPlaying = false;
    pendingRelease = false;
    soundingMask = 0;
    driving = false;

    lastStepPlayed.store (-1, std::memory_order_relaxed);
    nextStrumType.store ((int) StrumType::rest, std::memory_order_relaxed);

    rng.setSeed (seed);
    gesture.setSeed (seed);
    strumCount = 0;
}

void RhythmEngine::setSeed (uint64_t newSeed) noexcept
{
    seed = newSeed;
    rng.setSeed (seed);
    gesture.setSeed (seed);
}

//==============================================================================
void RhythmEngine::setCapoFret (int fret) noexcept
{
    // rhythm-engine 8.2's capo up/down, moving the one capo TuningEngine owns.
    if (tuning != nullptr)
        tuning->setCapoFret (juce::jlimit (0, 12, fret));
}

int RhythmEngine::getCapoFret() const noexcept
{
    return tuning != nullptr ? tuning->getCapoFret() : 0;
}

//==============================================================================
void RhythmEngine::setEnabled (bool shouldBeEnabled) noexcept
{
    const bool was = enabled.exchange (shouldBeEnabled, std::memory_order_relaxed);

    // rhythm-engine 0.6: bypassing must revert to raw MIDI within one block and
    // without artefacts, which means releasing whatever the engine was holding
    // rather than abandoning it to ring for ever.
    if (was && ! shouldBeEnabled)
        pendingRelease = true;
}

//==============================================================================
void RhythmEngine::setPattern (const RhythmPattern& pattern)
{
    const juce::ScopedLock sl (patternLock);
    writtenPattern = pattern;
    patternBuffer.write (pattern);   // SPEC-SWEEP RE-2
}

RhythmPattern RhythmEngine::getPattern() const
{
    const juce::ScopedLock sl (patternLock);
    return writtenPattern;
}

void RhythmEngine::setHumanise (const RhythmHumanise& h) noexcept
{
    const juce::ScopedLock sl (humaniseLock);
    writtenHumanise = h;
    humaniseBuffer.write (h);   // SPEC-SWEEP RE-2: the audio thread never takes this lock
}

RhythmHumanise RhythmEngine::getHumanise() const noexcept
{
    const juce::ScopedLock sl (humaniseLock);
    return writtenHumanise;
}

//==============================================================================
double RhythmEngine::resolveCrossingSps (const StrumStep& step, const RhythmPattern& pattern,
                                         CrossingSource* source) const noexcept
{
    auto answer = [source] (CrossingSource from, double sps)
    {
        if (source != nullptr)
            *source = from;

        return sps;
    };

    if (step.crossingSps > 0.0)
        return answer (CrossingSource::step, step.crossingSps);

    if (pattern.getCrossingSps() > 0.0)
        return answer (CrossingSource::pattern, pattern.getCrossingSps());

    const double kitMs = strumDurationMs.load (std::memory_order_relaxed);

    if (kitMs > 0.0)
        return answer (CrossingSource::kit, (double) (kKitReferenceStrings - 1) / (kitMs * 0.001));

    return answer (CrossingSource::global, strumSettings.crossingSps);
}

RhythmEngine::CrossingSource RhythmEngine::getCrossingSource (double& sps) const
{
    auto source = CrossingSource::global;
    sps = resolveCrossingSps (StrumStep {}, getPattern(), &source);
    return source;
}

//==============================================================================
void RhythmEngine::handleMidi (const juce::MidiBuffer& midi, int64_t blockStartSample) noexcept
{
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const int64_t when = blockStartSample + metadata.samplePosition;

        if (message.isNoteOn())
            detector.noteOn (message.getNoteNumber(), when);
        else if (message.isNoteOff())
            detector.noteOff (message.getNoteNumber());
        else if (message.isAllNotesOff() || message.isAllSoundOff())
            detector.allNotesOff();
    }
}

//==============================================================================
// bass-techniques 9 (MODEL-GAPS): the bass step grid.
void RhythmEngine::setBassGrid (const BassStepGrid& grid)
{
    const juce::ScopedLock sl (patternLock);
    writtenBassGrid = grid;
    bassGridHasSteps.store (! grid.isEmpty(), std::memory_order_release);
    bassGridBuffer.write (grid);   // SPEC-SWEEP RE-2
}

BassStepGrid RhythmEngine::getBassGrid() const
{
    const juce::ScopedLock sl (patternLock);
    return writtenBassGrid;
}

bool RhythmEngine::isBassGridActive() const noexcept
{
    return isBassFamily() && bassGridHasSteps.load (std::memory_order_acquire);
}

int RhythmEngine::processBassGrid (const BassStepGrid& grid, double startBeats, double endBeats,
                                   double beatsPerSample, int numSamples, PlayEventQueue& out) noexcept
{
    const double stepBeats = 1.0 / juce::jmax (1.0e-9, subdivisionsPerBeat (grid.getSubdivision()));
    const int length = juce::jmax (1, grid.getLength());
    const int stepSamples = (int) std::round (stepBeats / juce::jmax (1.0e-12, beatsPerSample));

    // A note ends before the next step starts, whatever the gate says, so its
    // note-off can never land on the next note.
    const int noteSamples = juce::jmax (1, (int) std::floor (stepSamples * juce::jmin (grid.getGate(), 0.97)));

    const int firstIndex = (int) std::ceil (startBeats / stepBeats) - 1;
    const int lastIndex = (int) std::floor (endBeats / stepBeats) + 1;
    int emitted = 0;

    for (int index = juce::jmax (0, firstIndex); index <= lastIndex; ++index)
    {
        const double beatPosition = (double) index * stepBeats;

        if (beatPosition < startBeats || beatPosition >= endBeats)
            continue;

        const int stepInGrid = index % length;
        const int sampleOffset = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                               (int) std::round ((beatPosition - startBeats) / juce::jmax (1.0e-12, beatsPerSample)));

        const auto& step = grid.getStep (stepInGrid);
        lastStepPlayed.store (stepInGrid, std::memory_order_relaxed);

        if (step.isRest())
            continue;

        const int before = out.getNumNoteOns();
        emitBassStep (step, sampleOffset, noteSamples, out);
        emitted += out.getNumNoteOns() - before;
    }

    lastPpq = endBeats;
    return emitted;
}

void RhythmEngine::emitBassStep (const BassStep& step, int sampleOffset, int lengthSamples, PlayEventQueue& out) noexcept
{
    // The root is the lowest voiced note: the Bass style voices it (4.3).
    const VoicedNote* root = nullptr;

    for (int i = 0; i < currentVoicing.numNotes; ++i)
    {
        const auto& note = currentVoicing.notes[(size_t) i];

        if (note.valid && (root == nullptr || note.midiNote < root->midiNote))
            root = &note;
    }

    if (root == nullptr)
        return;

    int stringIndex = root->stringIndex;
    int midiNote = root->midiNote;
    double fret = root->fretPosition;

    // The fifth or the octave: the string that reaches it nearest the root's fret.
    if (step.note != BassStepNote::root && tuning != nullptr)
    {
        const int target = root->midiNote + (step.note == BassStepNote::fifth ? 7 : 12);
        const double hz = 440.0 * std::pow (2.0, (target - 69) / 12.0);
        double best = 1.0e9;

        for (int s = 0; s < numStrings; ++s)
        {
            if (! tuning->canPlay (s, hz))
                continue;

            const double f = std::round (tuning->frequencyToFretPosition (s, hz));

            if (f < 0.0 || f > (double) tuning->getHighestPlayableFret (s))
                continue;

            const double distance = std::abs (f - root->fretPosition) + 0.25 * std::abs (s - root->stringIndex);

            if (distance < best)
            {
                best = distance;
                stringIndex = s;
                midiNote = target;
                fret = f;
            }
        }
    }

    NoteOnEvent on;
    on.stringIndex = stringIndex;
    on.midiNote = midiNote;
    on.midiChannel = 1;
    on.fretPosition = fret;
    on.velocity = juce::jlimit (0.02, 1.0, step.level);
    on.technique = step.type == BassStepType::dead ? Technique::MutedPick : Technique::Pluck;
    on.sampleOffset = juce::jmax (0, sampleOffset);
    on.bassTechnique = (int) step.type;

    if (tuning != nullptr)
        on.pitchHz = tuning->computeFrequency (stringIndex, fret, 0.0);

    out.addNoteOn (on);
    soundingMask = (uint16_t) (soundingMask | (uint16_t) (1u << stringIndex));

    NoteOffEvent off;
    off.stringIndex = stringIndex;
    off.midiNote = midiNote;
    off.sampleOffset = on.sampleOffset + lengthSamples;
    out.addNoteOff (off);
}

//==============================================================================
void RhythmEngine::revoice() noexcept
{
    if (voicer == nullptr)
    {
        voicingValid = false;
        return;
    }

    currentChord = detector.detect (detector.getHeldNotes(), detector.getNumHeldNotes());

    if (detector.getNumHeldNotes() <= 0)
    {
        voicingValid = false;
        currentVoicing = ChordVoicing {};
        return;
    }

    const auto style = getVoicingStyle();

    /*  ambiguity-resolutions 4: the rubric voicer places any chord tone in any
        octave (chordTones), scores the style's bias itself and caps the notes
        it sounds; density sets how many strings that may be. */
    voicer->setPitchMode (RubricPitchMode::chordTones);
    voicer->setStyle ((RubricStyle) (int) style);
    voicer->setBassPattern (getBassPattern());   // MODEL-GAPS
    voicer->setRootPitchClass (currentChord.root);
    voicer->setMaxSoundingStrings (juce::jmax (1, (int) std::round ((double) numStrings * getVoicingDensity() / 100.0)));
    voicer->setAllowOpenStrings (style != VoicingStyle::barre);
    voicer->setPreferredPosition (getHandPositionHint());
    voicer->setMaxFretSpan (getHandSpan() + (style == VoicingStyle::wide ? 1 : 0));   // SPEC-SWEEP RE-12
    /*  Zero, not the capo. TuningEngine measures fret positions from the capo now
        (ambiguity-resolutions 4.5), so frequencyToFretPosition already hands the
        voicer capo-relative frets, and filtering below the capo a second time here
        would take those frets away twice. minFret is back to meaning what its name
        says - a floor on where to voice - and nothing currently sets one. */
    voicer->setMinFret (0);

    currentVoicing = voicer->voice (detector.getHeldNotes(), nullptr, detector.getNumHeldNotes());
    voicingValid = currentVoicing.numNotes > 0;

    // rhythm-engine 3: the hint follows the last chord, so a progression stays in
    // one region of the neck instead of jumping.
    if (voicingValid)
    {
        int sum = 0, n = 0;

        for (int i = 0; i < currentVoicing.numNotes; ++i)
        {
            if (currentVoicing.notes[(size_t) i].valid)
            {
                sum += (int) currentVoicing.notes[(size_t) i].fretPosition;
                ++n;
            }
        }

        if (n > 0)
            handPositionHint.store (juce::jlimit (0, 22, sum / n), std::memory_order_relaxed);
    }
}

//==============================================================================
void RhythmEngine::emitNote (int stringIndex, double velocity, bool muted, double chuck,
                             int strikerMaterial, int sampleOffset, PlayEventQueue& out, int finger) noexcept
{
    if (! juce::isPositiveAndBelow (stringIndex, numStrings))
        return;

    // Find the voiced note on this string.
    const VoicedNote* found = nullptr;

    for (int i = 0; i < currentVoicing.numNotes; ++i)
    {
        const auto& note = currentVoicing.notes[(size_t) i];

        if (note.valid && note.stringIndex == stringIndex)
        {
            found = &note;
            break;
        }
    }

    if (found == nullptr)
        return;

    // A string already ringing is stopped first, the way the same string being
    // struck twice actually behaves.
    if ((soundingMask & (uint16_t) (1u << stringIndex)) != 0)
    {
        NoteOffEvent off;
        off.stringIndex = stringIndex;
        off.midiNote = found->midiNote;
        off.sampleOffset = sampleOffset;
        off.letRing = false;
        out.addNoteOff (off);
    }

    NoteOnEvent on;
    on.stringIndex = stringIndex;
    on.midiNote = found->midiNote;
    on.midiChannel = 1;
    on.fretPosition = found->fretPosition;
    on.velocity = juce::jlimit (0.02, 1.0, velocity);
    on.technique = muted ? Technique::PalmMute : Technique::Pluck;
    on.sampleOffset = juce::jmax (0, sampleOffset);
    on.slideFromFret = -1.0;
    on.slideSeconds = 0.0;
    on.chuck = chuck;
    on.strikerMaterial = strikerMaterial;
    on.finger = finger;   // REALISM-B: fingerstyle-attack.md 3, the pattern's finger reaches the string

    if (tuning != nullptr)
        on.pitchHz = tuning->computeFrequency (stringIndex, found->fretPosition, 0.0);

    out.addNoteOn (on);

    soundingMask = (uint16_t) (soundingMask | (uint16_t) (1u << stringIndex));
}

void RhythmEngine::releaseAll (int sampleOffset, PlayEventQueue& out) noexcept
{
    for (int s = 0; s < numStrings; ++s)
    {
        if ((soundingMask & (uint16_t) (1u << s)) == 0)
            continue;

        NoteOffEvent off;
        off.stringIndex = s;
        off.sampleOffset = juce::jmax (0, sampleOffset);
        off.letRing = false;
        out.addNoteOff (off);
    }

    soundingMask = 0;
}

//==============================================================================
void RhythmEngine::scheduleStrum (const StrumStep& step, double sourceSps, int sampleOffset,
                                  PlayEventQueue& out) noexcept
{
    if (step.isRest() || ! voicingValid)
        return;

    const auto& h = humaniseBuffer.current();   // SPEC-SWEEP RE-2: acquired at the top of processBlock

    // rhythm-engine 4: a scheduled stroke can simply not happen.
    if (h.missPercent > 0.0 && rng.nextDouble() * 100.0 < h.missPercent * h.amount)
        return;

    // Which strings take part: in the voicing, and allowed by the step's mask.
    std::array<int, kMaxStrings> participating {};
    int numParticipating = 0;

    for (int s = 0; s < numStrings; ++s)
    {
        if (((step.stringMask >> s) & 1u) == 0)
            continue;

        for (int i = 0; i < currentVoicing.numNotes; ++i)
        {
            if (currentVoicing.notes[(size_t) i].valid
                  && currentVoicing.notes[(size_t) i].stringIndex == s)
            {
                participating[(size_t) numParticipating++] = s;
                break;
            }
        }
    }

    if (numParticipating == 0)
        return;

    // A down stroke crosses the low strings first. String index 0 is the high E,
    // so the low strings carry the highest indices and must sound first.
    const bool downward = isDownStroke (step.type);

    if (downward)
        std::sort (participating.begin(), participating.begin() + numParticipating,
                   [] (int a, int b) { return a > b; });
    else
        std::sort (participating.begin(), participating.begin() + numParticipating);

    const bool muted = isMutedStrum (step.type);
    const bool chuck = step.type == StrumType::chuck;

    // strum-dynamics 6.3: Feel scales whichever crossing source is in charge,
    // and the evenness, by the same map; at 0.5 it changes nothing.
    const double feel = getStrumFeel();

    auto settings = strumSettings;
    settings.evenness = juce::jlimit (0.0, 1.0, strumEvenness.load (std::memory_order_relaxed)
                                                  * StrumFeel::evennessScaleFor (feel));

    double sps = sourceSps * StrumFeel::crossingScaleFor (feel);

    // A rake is slower and deliberately drags; a rasgueado is a burst of four.
    if (step.type == StrumType::rake)
        sps /= 1.6;

    const int strikerMaterial = getStrikerMaterial (downward ? settings.strikerDown : settings.strikerUp);
    const double chuckAmount = StrumGesture::chuckFor (settings, chuck);

    // 6.1: the fretting hand lands flat before the strum crosses, so a string
    // still ringing that this strum does not strike is stopped too. (Struck
    // ones are stopped by emitNote, as any re-strike is.)
    if (chuck)
    {
        for (int s = 0; s < numStrings; ++s)
        {
            const auto bit = (uint16_t) (1u << s);
            const auto end = participating.begin() + numParticipating;

            if ((soundingMask & bit) == 0 || std::find (participating.begin(), end, s) != end)
                continue;

            NoteOffEvent off;
            off.stringIndex = s;
            off.sampleOffset = juce::jmax (0, sampleOffset);
            off.letRing = false;
            out.addNoteOff (off);

            soundingMask = (uint16_t) (soundingMask & ~bit);
        }
    }

    // Ground rule 1: timing and velocity humanisation belong to the gesture,
    // not each string - per string they would undo the acceleration profile.
    double gestureOffset = (double) sampleOffset;

    if (h.timingMs > 0.0)
        gestureOffset += rng.nextGaussian() * h.timingMs * 0.001 * sr * h.amount * 0.5;

    double baseVelocity = step.dynamic;

    if (h.velocityPercent > 0.0)
        baseVelocity *= 1.0 + rng.nextGaussian() * (h.velocityPercent * 0.01 * h.amount) * 0.5;

    const int strokes = (step.type == StrumType::rasgueado) ? 4 : 1;
    std::array<StrumStrike, kMaxStrings> strikes {};

    for (int stroke = 0; stroke < strokes; ++stroke)
    {
        // Flamenco: four sequential up-strums, 10-20 ms apart.
        const double strokeOffsetMs = (step.type == StrumType::rasgueado)
                                        ? (double) stroke * (10.0 + rng.nextDouble() * 10.0)
                                        : 0.0;

        StrumRequest request;
        request.strings = participating.data();
        request.numStrings = numParticipating;
        request.down = downward;
        request.sourceSps = sps;
        request.strumIndex = strumCount++;
        request.missScale = h.amount;   // humanise off is a hand that never misses

        const int planned = gesture.plan (settings, request, strikes.data(), (int) strikes.size());

        // SPEC-SWEEP (RE-18, rhythm-engine 2): a rake drags muted across the
        // strings and lands on its target - the last string it strikes - open
        // and at the step's full dynamic.
        int rakeTarget = -1;

        if (step.type == StrumType::rake)
            for (int i = planned; --i >= 0;)
                if (! strikes[(size_t) i].missed)
                {
                    rakeTarget = i;
                    break;
                }

        for (int i = 0; i < planned; ++i)
        {
            const auto& strike = strikes[(size_t) i];

            // 3.1: the hand crossed this string without striking it.
            if (strike.missed)
                continue;

            const double offset = gestureOffset + strokeOffsetMs * 0.001 * sr + strike.timeSeconds * sr;
            const bool isRakeTarget = (i == rakeTarget);

            emitNote (strike.stringIndex,
                      isRakeTarget ? baseVelocity : baseVelocity * strike.force,
                      muted && ! isRakeTarget, chuckAmount,
                      strikerMaterial, (int) std::round (juce::jmax (0.0, offset)), out);
        }

        // REALISM-B, string-interaction.md 6: muted strings inside the STRUM
        // mask and the strum's span are struck too. Skipped entirely at 0.
        if (mutedThumpLevel > 0.0)
        {
            juce::uint32 candidates = 0;

            for (int s = 0; s < numStrings; ++s)
            {
                if (((step.stringMask >> s) & 1u) == 0)
                    continue;

                bool voiced = false;

                for (int i = 0; i < currentVoicing.numNotes; ++i)
                    voiced = voiced || (currentVoicing.notes[(size_t) i].valid
                                          && currentVoicing.notes[(size_t) i].stringIndex == s);

                if (! voiced)
                    candidates |= (juce::uint32) 1u << (juce::uint32) s;
            }

            std::array<MutedThump, kMaxStrings> thumps {};
            const int numThumps = planMutedThumps (strikes.data(), planned, candidates, thumps.data(), (int) thumps.size(), false);
            const int hand = handPositionHint.load (std::memory_order_relaxed);

            for (int k = 0; k < numThumps; ++k)
            {
                const auto& t = thumps[(size_t) k];
                const double offset = gestureOffset + strokeOffsetMs * 0.001 * sr + t.timeSeconds * sr;
                const double hz = tuning != nullptr ? tuning->computeFrequency (t.stringIndex, (double) hand, 0.0) : 110.0;

                out.addNoteOn (makeThumpEvent (t.stringIndex, baseVelocity * t.force, mutedThumpLevel, hz,
                                               (int) std::round (juce::jmax (0.0, offset)), strikerMaterial));
            }
        }
    }

    nextStrumType.store ((int) step.type, std::memory_order_relaxed);
    strokesScheduled.fetch_add (1, std::memory_order_relaxed);   // SPEC-SWEEP GD-10
}

void RhythmEngine::scheduleFingerpick (const FingerpickStep& step, int sampleOffset,
                                       PlayEventQueue& out) noexcept
{
    if (! step.active || ! voicingValid)
        return;

    const auto& h = humaniseBuffer.current();   // SPEC-SWEEP RE-2

    if (h.missPercent > 0.0 && rng.nextDouble() * 100.0 < h.missPercent * h.amount)
        return;

    const auto& pattern = patternBuffer.current();   // SPEC-SWEEP RE-2: no copy on the audio thread
    const int stringIndex = pattern.getStringForFinger (step.finger);

    double velocity = step.dynamic;

    if (h.velocityPercent > 0.0)
        velocity *= 1.0 + rng.nextGaussian() * (h.velocityPercent * 0.01 * h.amount) * 0.5;

    double offset = (double) sampleOffset;

    if (h.timingMs > 0.0)
        offset += rng.nextGaussian() * h.timingMs * 0.001 * sr * h.amount * 0.5;

    emitNote (stringIndex, velocity, false, 0.0, -1, (int) juce::jmax (0.0, offset), out, (int) step.finger);
}

//==============================================================================
int RhythmEngine::processBlock (int numSamples, const RhythmTransport& transport,
                                PlayEventQueue& out) noexcept
{
    driving = false;

    // SPEC-SWEEP (RE-2): pick up whatever the message thread published since the
    // last block; everything below reads these slots by reference.
    patternBuffer.acquire();
    bassGridBuffer.acquire();
    humaniseBuffer.acquire();

    // ---- bypass ---------------------------------------------------------------
    if (pendingRelease.exchange (false))
    {
        releaseAll (0, out);
    }

    if (! isEnabled())
    {
        lastStepPlayed.store (-1, std::memory_order_relaxed);
        return 0;
    }

    // ---- chord ----------------------------------------------------------------
    // The burst window groups a spread chord into one, so a strummed input does
    // not re-voice six times.
    if (detector.advance (0))
    {
        // advance() uses absolute sample time; the caller drives it through
        // handleMidi, and a closed burst means the held set is settled.
    }

    const int heldNow = detector.getNumHeldNotes();

    if (heldNow == 0)
    {
        if (soundingMask != 0)
            releaseAll (0, out);

        voicingValid = false;
        lastStepPlayed.store (-1, std::memory_order_relaxed);
        return 0;
    }

    revoice();

    if (! voicingValid)
        return 0;

    // ---- transport -------------------------------------------------------------
    const double bpm = juce::jlimit (20.0, 300.0, transport.bpm);
    const double beatsPerSample = bpm / (60.0 * sr);
    const double blockBeats = beatsPerSample * (double) numSamples;

    double startBeats = 0.0;

    if (transport.isPlaying)
    {
        startBeats = transport.ppqPosition;

        // Keep free-run in step with the host, so switching between them does not
        // jump the pattern.
        freeRunPpq = startBeats;
    }
    else if (isFreeRunning())
    {
        startBeats = freeRunPpq;
        freeRunPpq += blockBeats;
    }
    else
    {
        // rhythm-engine 0.4: silent when the host is stopped.
        if (wasPlaying && soundingMask != 0)
            releaseAll (0, out);

        wasPlaying = false;
        lastStepPlayed.store (-1, std::memory_order_relaxed);
        return 0;
    }

    wasPlaying = transport.isPlaying;
    driving = true;
    drivenBlocks.fetch_add (1, std::memory_order_relaxed);   // SPEC-SWEEP GD-10

    // bass-techniques 9 (MODEL-GAPS): on a bass, a grid with steps in it plays.
    if (isBassGridActive())
    {
        const auto& grid = bassGridBuffer.current();
        return processBassGrid (grid, startBeats, startBeats + blockBeats, beatsPerSample, numSamples, out);
    }

    // ---- walk the pattern's grid over this block ----------------------------------
    const auto& pattern = patternBuffer.current();   // SPEC-SWEEP RE-2: no copy on the audio thread

    if (pattern.isEmpty() || pattern.getLength() <= 0)
        return 0;

    const double perBeat = subdivisionsPerBeat (pattern.getSubdivision());
    const double stepBeats = 1.0 / juce::jmax (1.0e-9, perBeat);
    const double patternBeats = stepBeats * (double) pattern.getLength();
    const double swing = pattern.getSwing();

    const double endBeats = startBeats + blockBeats;

    // The first grid index at or after the start of this block.
    const int firstIndex = (int) std::ceil (startBeats / stepBeats) - 1;
    const int lastIndex = (int) std::floor (endBeats / stepBeats) + 1;

    int emitted = 0;

    for (int index = firstIndex; index <= lastIndex; ++index)
    {
        if (index < 0)
            continue;

        double beatPosition = (double) index * stepBeats;

        // Swing pushes the odd steps later. The pair still spans the same total
        // time, so the bar does not stretch. SPEC-SWEEP (RE-35): swing is the
        // share of the pair the on-beat step takes - 0.5 straight, 0.66 a
        // triplet feel, 0.75 a dotted feel - so the offbeat lands at swing x the
        // pair; it used to move only half as far.
        if ((index % 2) != 0 && swing > 0.5)
            beatPosition += (swing - 0.5) * 2.0 * stepBeats;

        if (beatPosition < startBeats || beatPosition >= endBeats)
            continue;

        const int stepInPattern = (int) std::floor (
            std::fmod ((double) index * stepBeats, patternBeats) / stepBeats + 0.5) % pattern.getLength();

        const int sampleOffset = juce::jlimit (
            0, juce::jmax (0, numSamples - 1),
            (int) std::round ((beatPosition - startBeats) / juce::jmax (1.0e-12, beatsPerSample)));

        const int before = out.getNumNoteOns();

        if (pattern.getKind() == RhythmPattern::Kind::strum)
        {
            const auto step = pattern.getStrumStep (stepInPattern);

            // ambiguity-resolutions 6: step, then pattern, then kit, then the STRUM group.
            const double crossingSps = resolveCrossingSps (step, pattern);

            // A ghost stroke is an extra muted brush just before the hit, which
            // is most of what makes a strummed part sound played rather than
            // programmed.
            const auto& h = humaniseBuffer.current();

            if (! step.isRest() && h.ghostPercent > 0.0
                  && rng.nextDouble() * 100.0 < h.ghostPercent * h.amount)
            {
                StrumStep ghost = step;
                ghost.type = isDownStroke (step.type) ? StrumType::upMute : StrumType::downMute;
                ghost.dynamic = step.dynamic * 0.35;

                const int ghostOffset = juce::jmax (0, sampleOffset - (int) (0.035 * sr));
                scheduleStrum (ghost, crossingSps, ghostOffset, out);
            }

            scheduleStrum (step, crossingSps, sampleOffset, out);
        }
        else
        {
            scheduleFingerpick (pattern.getFingerpickStep (stepInPattern), sampleOffset, out);
        }

        emitted += out.getNumNoteOns() - before;
        lastStepPlayed.store (stepInPattern, std::memory_order_relaxed);
    }

    lastPpq = endBeats;
    return emitted;
}

//==============================================================================
juce::var RhythmEngine::toVar() const
{
    auto* root = new juce::DynamicObject();

    root->setProperty ("enabled", isEnabled());
    root->setProperty ("freeRun", isFreeRunning());
    root->setProperty ("voicingStyle", (int) getVoicingStyle());
    root->setProperty ("bassPattern", (int) getBassPattern());
    root->setProperty ("voicingDensity", getVoicingDensity());
    root->setProperty ("handSpan", getHandSpan());   // SPEC-SWEEP RE-12
    root->setProperty ("handPosition", getHandPositionHint());

    /*  No "capoFret" here any more. The capo is a parameter now (ParamIDs::capoFret),
        so it is saved with every other parameter in the same preset, and writing
        it twice would give a preset two capos that a later edit could disagree
        about. fromVar still reads an old one - see below. */
    root->setProperty ("strumEvenness", strumEvenness.load (std::memory_order_relaxed));
    root->setProperty ("strumDurationMs", getStrumDurationMs());
    root->setProperty ("strumFeel", getStrumFeel());
    root->setProperty ("pattern", getPattern().toVar());
    root->setProperty ("bassGrid", getBassGrid().toVar());   // MODEL-GAPS

    const auto h = getHumanise();
    auto* humaniseObject = new juce::DynamicObject();
    humaniseObject->setProperty ("timingMs", h.timingMs);
    humaniseObject->setProperty ("velocityPercent", h.velocityPercent);
    humaniseObject->setProperty ("missPercent", h.missPercent);
    humaniseObject->setProperty ("ghostPercent", h.ghostPercent);
    humaniseObject->setProperty ("amount", h.amount);
    root->setProperty ("humanise", juce::var (humaniseObject));

    return juce::var (root);
}

void RhythmEngine::fromVar (const juce::var& state)
{
    auto* root = state.getDynamicObject();

    if (root == nullptr)
        return;

    setEnabled ((bool) root->getProperty ("enabled"));
    setFreeRun ((bool) root->getProperty ("freeRun"));
    setVoicingStyle ((VoicingStyle) juce::jlimit (0, (int) VoicingStyle::numStyles - 1,
                                                  (int) root->getProperty ("voicingStyle")));
    setBassPattern ((RubricBassPattern) juce::jlimit (0, (int) RubricBassPattern::walking,
                                                      (int) (root->hasProperty ("bassPattern") ? root->getProperty ("bassPattern") : juce::var (0))));
    setVoicingDensity ((double) root->getProperty ("voicingDensity"));
    setHandSpan (root->hasProperty ("handSpan") ? (int) root->getProperty ("handSpan") : 5);   // SPEC-SWEEP RE-12
    setHandPositionHint ((int) root->getProperty ("handPosition"));

    /*  A capo saved by a build that kept one here. It is applied so an old
        session does not silently lose it, and it is not written back: the
        parameter owns it from now on, and re-saving this preset moves it across.

        A preset whose parameters also carry a capo will have that one applied
        afterwards by applyStructural, which is the right way round - the newer
        field wins. */
    if (root->hasProperty ("capoFret"))
        setCapoFret ((int) root->getProperty ("capoFret"));

    if (root->hasProperty ("strumEvenness"))
        setStrumEvenness ((double) root->getProperty ("strumEvenness"));

    if (root->hasProperty ("strumDurationMs"))
        setStrumDurationMs ((double) root->getProperty ("strumDurationMs"));

    if (root->hasProperty ("strumFeel"))
        setStrumFeel ((double) root->getProperty ("strumFeel"));

    if (root->hasProperty ("pattern"))
        setPattern (RhythmPattern::fromVar (root->getProperty ("pattern")));

    // MODEL-GAPS: the bass step grid; absent in older state, which is an empty grid.
    setBassGrid (BassStepGrid::fromVar (root->getProperty ("bassGrid")));

    if (auto* humaniseObject = root->getProperty ("humanise").getDynamicObject())
    {
        RhythmHumanise h;
        h.timingMs = (double) humaniseObject->getProperty ("timingMs");
        h.velocityPercent = (double) humaniseObject->getProperty ("velocityPercent");
        h.missPercent = (double) humaniseObject->getProperty ("missPercent");
        h.ghostPercent = (double) humaniseObject->getProperty ("ghostPercent");
        h.amount = humaniseObject->hasProperty ("amount")
                     ? (double) humaniseObject->getProperty ("amount") : 1.0;
        setHumanise (h);
    }
}

} // namespace luthier
