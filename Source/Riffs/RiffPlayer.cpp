#include "RiffPlayer.h"

#include <algorithm>
#include <cmath>

namespace luthier
{

RiffPlayer::RiffPlayer()
{
    activeSegment.fill (-1);
    lastCents.fill (0.0);
    lateUntil.fill (0);
}

RiffPlayer::~RiffPlayer() = default;

void RiffPlayer::prepare (double newSampleRate)
{
    sampleRate = (std::isfinite (newSampleRate) && newSampleRate > 0.0) ? newSampleRate : 48000.0;
}

//==============================================================================
void RiffPlayer::setCompiled (std::shared_ptr<const CompiledRiff> compiled, bool immediate)
{
    std::shared_ptr<const CompiledRiff> displaced, previousNewest;

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        displaced = std::move (waiting);
        previousNewest = std::move (newest);
        waiting = compiled;
        newest = std::move (compiled);
        waitingImmediate = waitingImmediate || immediate;
        hasWaiting = true;
        pendingSwap.store (true, std::memory_order_relaxed);
    }

    // `displaced` and `previousNewest` are released here, on the message thread.
}

void RiffPlayer::collectGarbage()
{
    std::array<std::shared_ptr<const CompiledRiff>, kRetiredSlots> done;

    {
        const juce::SpinLock::ScopedLockType sl (lock);

        for (size_t i = 0; i < retired.size(); ++i)
            done[i] = std::move (retired[i]);
    }

    // Freed here, outside the lock.
}

std::shared_ptr<const CompiledRiff> RiffPlayer::getCompiled() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return newest;
}

void RiffPlayer::play() noexcept
{
    stopRequested.store (false, std::memory_order_relaxed);
    playRequested.store (true, std::memory_order_relaxed);
}

void RiffPlayer::stop() noexcept
{
    playRequested.store (false, std::memory_order_relaxed);
    stopRequested.store (true, std::memory_order_relaxed);
}

void RiffPlayer::setTempoFactor (double factor) noexcept
{
    tempoFactor.store (std::isfinite (factor) ? juce::jlimit (0.25, 2.0, factor) : 1.0, std::memory_order_relaxed);
}

void RiffPlayer::setAbsoluteBpm (double bpm) noexcept
{
    absoluteBpm.store (std::isfinite (bpm) && bpm > 0.0 ? juce::jlimit (20.0, 400.0, bpm) : 0.0,
                       std::memory_order_relaxed);
}

double RiffPlayer::ownBpm() const noexcept
{
    const double absolute = absoluteBpm.load (std::memory_order_relaxed);

    if (absolute > 0.0)
        return absolute;

    const double riffTempo = active != nullptr ? active->tempoBpm : 120.0;
    return juce::jlimit (7.5, 600.0, riffTempo * tempoFactor.load (std::memory_order_relaxed));
}

//==============================================================================
bool RiffPlayer::swapInWaiting (Output& out, int offset) noexcept
{
    // Called with `lock` held.
    if (active != nullptr)
    {
        std::shared_ptr<const CompiledRiff>* slot = nullptr;

        for (auto& r : retired)
        {
            if (r == nullptr)
            {
                slot = &r;
                break;
            }
        }

        // The message thread has not collected yet: keep playing this one.
        if (slot == nullptr)
            return false;

        releaseAll (out, offset);
        *slot = std::move (active);
    }

    active = std::move (waiting);
    hasWaiting = false;
    waitingImmediate = false;
    pendingSwap.store (false, std::memory_order_relaxed);

    if (active == nullptr)
    {
        playingNow = armed = false;
        return true;
    }

    // Same place in the new riff, wrapped into it.
    const double length = looping.load (std::memory_order_relaxed) ? active->loopBeats : active->lengthBeats;

    if (playingNow && length > 0.0)
    {
        if (position >= length)
            position = std::fmod (position, length);

        relocate (position);
    }

    return true;
}

void RiffPlayer::releaseAll (Output& out, int offset) noexcept
{
    for (int s = 0; s < kMaxStrings; ++s)
    {
        const int bit = 1 << s;

        if ((sounding & bit) != 0 || lateUntil[(size_t) s] > 0)
        {
            NoteOffEvent off;
            off.stringIndex = s;
            off.sampleOffset = juce::jmax (offset, lateUntil[(size_t) s] + 1);
            out.queue.addNoteOff (off);
            ++slotsUsed;
        }

        if (activeSegment[(size_t) s] >= 0 || ! juce::exactlyEqual (lastCents[(size_t) s], 0.0))
            addBend (s, 0.0, offset, out, true);

        activeSegment[(size_t) s] = -1;
        lateUntil[(size_t) s] = 0;
    }

    sounding = 0;
}

void RiffPlayer::relocate (double beat) noexcept
{
    if (active == nullptr)
    {
        eventIndex = 0;
        return;
    }

    const auto& events = active->events;
    const auto it = std::lower_bound (events.begin(), events.end(), beat - 1.0e-9,
                                      [] (const RiffEvent& e, double b) { return e.beat < b; });
    eventIndex = (size_t) std::distance (events.begin(), it);
    activeSegment.fill (-1);
}

//==============================================================================
bool RiffPlayer::addBend (int stringIndex, double cents, int offset, Output& out, bool force) noexcept
{
    if (! force && slotsUsed >= kMaxSlotsPerSubBlock)
    {
        overflowCount.fetch_add (1, std::memory_order_relaxed);
        return false;
    }

    BendEvent b;
    b.stringIndex = stringIndex;
    b.cents = std::isfinite (cents) ? cents : 0.0;
    b.sampleOffset = juce::jmax (0, offset);

    if (! out.queue.addBend (b))
    {
        overflowCount.fetch_add (1, std::memory_order_relaxed);
        return false;
    }

    ++slotsUsed;
    lastCents[(size_t) juce::jlimit (0, kMaxStrings - 1, stringIndex)] = b.cents;
    return true;
}

double RiffPlayer::segmentCentsAt (const RiffBendSegment& seg, double beat, double samplesPerBeat) const noexcept
{
    const auto& points = active->bendPoints;
    double cents = 0.0;

    if (seg.numPoints > 0)
    {
        const int first = seg.firstPoint;
        const int last = seg.firstPoint + seg.numPoints - 1;

        if (beat <= points[(size_t) first].first)
        {
            cents = points[(size_t) first].second;
        }
        else if (beat >= points[(size_t) last].first)
        {
            cents = points[(size_t) last].second;
        }
        else
        {
            for (int k = first + 1; k <= last; ++k)
            {
                const auto& a = points[(size_t) k - 1];
                const auto& b = points[(size_t) k];

                if (beat <= b.first)
                {
                    const double span = b.first - a.first;
                    cents = span > 0.0 ? a.second + (b.second - a.second) * (beat - a.first) / span : b.second;
                    break;
                }
            }
        }
    }

    if (seg.vibratoRateHz > 0.0 && seg.vibratoDepthCents > 0.0)
    {
        const double from = seg.startBeat + seg.vibratoDelayBeats;

        if (beat > from)
        {
            const double seconds = (beat - from) * samplesPerBeat / sampleRate;
            cents += seg.vibratoDepthCents * std::sin (juce::MathConstants<double>::twoPi * seg.vibratoRateHz * seconds);
        }
    }

    return cents;
}

void RiffPlayer::emit (const RiffEvent& e, double offset, double samplesPerBeat, int numSamples, Output& out) noexcept
{
    const int s = juce::jlimit (0, kMaxStrings - 1, e.stringIndex);
    const int at = (int) std::llround (juce::jmax (0.0, offset) + e.delaySeconds * sampleRate);

    if (e.kind == RiffEvent::Kind::noteOn)
    {
        NoteOnEvent n;
        n.stringIndex = s;
        n.midiNote = e.midiNote;
        n.midiChannel = s + 1;
        n.fretPosition = e.fret;
        n.velocity = e.velocity;
        n.technique = e.technique;
        n.harmonicPartial = e.harmonicPartial;
        n.touchFret = e.touchFret;
        n.sampleOffset = at;
        n.slideFromFret = e.slideFromFret;
        n.slideSeconds = e.slideBeats * samplesPerBeat / sampleRate;
        n.palmMuteDepth = e.palmMuteDepth;
        n.bassTechnique = e.bassTechnique;
        n.explicitArticulation = true;

        // The pitch the note starts at (a prebend), and the string's bend state.
        double startCents = 0.0;

        if (e.bendSegment >= 0 && e.bendSegment < (int) active->bends.size())
        {
            startCents = segmentCentsAt (active->bends[(size_t) e.bendSegment], e.beat, samplesPerBeat);
            activeSegment[(size_t) s] = e.bendSegment;
        }
        else
        {
            activeSegment[(size_t) s] = -1;
        }

        if (! juce::exactlyEqual (startCents, lastCents[(size_t) s]))
            addBend (s, startCents, at, out, true);

        const int index = out.queue.getNumNoteOns();

        if (out.queue.addNoteOn (n))
            out.startCents[(size_t) index] = startCents;
        else
            overflowCount.fetch_add (1, std::memory_order_relaxed);

        ++slotsUsed;
        sounding |= 1 << s;

        if (at >= numSamples)
            lateUntil[(size_t) s] = juce::jmax (lateUntil[(size_t) s], at - numSamples + 1);
    }
    else
    {
        NoteOffEvent off;
        off.stringIndex = s;
        off.midiNote = e.midiNote;
        off.sampleOffset = at;
        off.letRing = e.letRing;

        if (! out.queue.addNoteOff (off))
            overflowCount.fetch_add (1, std::memory_order_relaxed);

        ++slotsUsed;
        sounding &= ~(1 << s);

        if (activeSegment[(size_t) s] >= 0 || ! juce::exactlyEqual (lastCents[(size_t) s], 0.0))
            addBend (s, 0.0, at, out, true);

        activeSegment[(size_t) s] = -1;
    }
}

void RiffPlayer::emitBends (double beat, int offset, double samplesPerBeat, Output& out) noexcept
{
    for (int s = 0; s < kMaxStrings; ++s)
    {
        const int index = activeSegment[(size_t) s];

        if (index < 0)
            continue;

        const auto& seg = active->bends[(size_t) index];

        if (beat >= seg.endBeat)
        {
            addBend (s, 0.0, offset, out, true);
            activeSegment[(size_t) s] = -1;
            continue;
        }

        addBend (s, segmentCentsAt (seg, beat, samplesPerBeat), offset, out, false);
    }
}

void RiffPlayer::playSpan (Output& out, double beatFrom, double beatTo, double sampleFrom,
                           double samplesPerBeat, int numSamples) noexcept
{
    const auto& events = active->events;
    double beat = beatFrom;

    while (beat < beatTo - 1.0e-12)
    {
        // Bend points sit on the block's 64-sample grid.
        const double sample = sampleFrom + (beat - beatFrom) * samplesPerBeat;
        const double grid = (std::floor (sample / kBendIntervalSamples + 1.0e-9) + 1.0) * kBendIntervalSamples;
        const double chunkEnd = juce::jmin (beatTo, beatFrom + (grid - sampleFrom) / samplesPerBeat);

        while (eventIndex < events.size() && events[eventIndex].beat < chunkEnd - 1.0e-12)
        {
            const auto& e = events[eventIndex++];
            emit (e, sampleFrom + (juce::jmax (beatFrom, e.beat) - beatFrom) * samplesPerBeat,
                  samplesPerBeat, numSamples, out);
        }

        const int at = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                                     (int) std::floor (sampleFrom + (chunkEnd - beatFrom) * samplesPerBeat) - 1);
        emitBends (chunkEnd, at, samplesPerBeat, out);

        if (chunkEnd <= beat)
            break;   // no progress; cannot happen with a positive tempo

        beat = chunkEnd;
    }
}

void RiffPlayer::advance (Output& out, double sampleFrom, double samplesPerBeat, int numSamples) noexcept
{
    double sample = sampleFrom;

    while (playingNow && active != nullptr && sample < numSamples - 1.0e-9)
    {
        const bool loop = looping.load (std::memory_order_relaxed);
        const double length = juce::jmax (1.0e-3, loop ? active->loopBeats : active->lengthBeats);

        double end = position + (numSamples - sample) / samplesPerBeat;
        bool atLength = false, atSwap = false;

        if (end >= length)
        {
            end = length;
            atLength = true;
        }

        // A waiting riff comes in on the next beat line.
        if (pendingSwap.load (std::memory_order_relaxed))
        {
            const double nextBeat = std::ceil (position - 1.0e-9);

            if (nextBeat <= end)
            {
                end = nextBeat;
                atSwap = true;
                atLength = atLength && juce::exactlyEqual (end, length);
            }
        }

        playSpan (out, position, end, sample, samplesPerBeat, numSamples);
        sample += (end - position) * samplesPerBeat;
        position = end;

        const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1), (int) std::llround (sample));

        if (atSwap)
        {
            const juce::SpinLock::ScopedTryLockType sl (lock);

            if (sl.isLocked() && hasWaiting)
                swapInWaiting (out, offset);
            else
                pendingSwap.store (hasWaiting, std::memory_order_relaxed);   // retry next block

            if (! sl.isLocked() || hasWaiting)
                break;   // could not swap now: play on from here next block

            continue;
        }

        if (atLength)
        {
            if (loop)
            {
                position = 0.0;
                relocate (0.0);
                loopCount.fetch_add (1, std::memory_order_relaxed);
            }
            else
            {
                releaseAll (out, offset);
                playingNow = false;
            }
        }
    }
}

//==============================================================================
void RiffPlayer::renderSubBlock (int numSamples, double hostPpq, bool hostPlaying, double hostBpm, Output& out) noexcept
{
    slotsUsed = 0;

    for (auto& late : lateUntil)
        late = juce::jmax (0, late - lastBlockSamples);

    lastBlockSamples = juce::jmax (0, numSamples);

    if (numSamples <= 0)
        return;

    if (resetPending.exchange (false, std::memory_order_relaxed))
    {
        // The engine's reset silenced the strings and zeroed their bends.
        sounding = 0;
        activeSegment.fill (-1);
        lastCents.fill (0.0);
        lateUntil.fill (0);
    }

    // ---- commands -------------------------------------------------------------
    if (stopRequested.exchange (false, std::memory_order_relaxed))
    {
        releaseAll (out, 0);
        playingNow = armed = false;
    }

    {
        const juce::SpinLock::ScopedTryLockType sl (lock);

        if (sl.isLocked() && hasWaiting && ((! playingNow && ! armed) || waitingImmediate))
            swapInWaiting (out, 0);
    }

    const bool hostUsable = hostPlaying && std::isfinite (hostBpm) && hostBpm > 0.0 && std::isfinite (hostPpq);

    if (playRequested.exchange (false, std::memory_order_relaxed) && active != nullptr)
    {
        if (playingNow || armed)
            releaseAll (out, 0);

        hostLocked = (ClockMode) clockMode.load (std::memory_order_relaxed) == ClockMode::automatic && hostUsable;
        position = 0.0;
        loopCount.store (0, std::memory_order_relaxed);
        relocate (0.0);

        if (hostLocked)
        {
            const auto q = (StartQuantise) startQuantise.load (std::memory_order_relaxed);
            const double unit = q == StartQuantise::nextBar ? juce::jmax (0.25, active->beatsPerBar) : 1.0;

            anchorPpq = q == StartQuantise::immediate ? hostPpq : std::ceil (hostPpq / unit - 1.0e-9) * unit;
            armed = true;
            playingNow = false;
            expectedPpq = -1.0;
        }
        else
        {
            armed = false;
            playingNow = true;
        }
    }

    if (active == nullptr)
        playingNow = armed = false;

    // ---- play -------------------------------------------------------------------
    if (playingNow || armed)
    {
        if (hostLocked)
        {
            if (! hostUsable)
            {
                // riff-library 5.3: a host stop ends the riff.
                releaseAll (out, 0);
                playingNow = armed = false;
            }
            else
            {
                const double spb = sampleRate * 60.0 / hostBpm;
                const double blockEndPpq = hostPpq + numSamples / spb;
                double sampleFrom = 0.0;
                bool run = true;

                if (armed)
                {
                    if (anchorPpq >= blockEndPpq - 1.0e-12)
                    {
                        run = false;
                    }
                    else
                    {
                        sampleFrom = juce::jmax (0.0, (anchorPpq - hostPpq) * spb);
                        position = juce::jmax (0.0, hostPpq - anchorPpq);
                        relocate (position);
                        armed = false;
                        playingNow = true;
                    }
                }
                else if (expectedPpq >= 0.0 && std::abs (hostPpq - expectedPpq) > 1.0e-3)
                {
                    // A host jump: end every note and re-locate by ppq.
                    releaseAll (out, 0);
                    const bool loop = looping.load (std::memory_order_relaxed);
                    double riffBeat = hostPpq - anchorPpq;

                    if (riffBeat < 0.0)
                    {
                        armed = true;
                        playingNow = false;
                        run = false;
                    }
                    else if (loop && active->loopBeats > 0.0)
                    {
                        position = std::fmod (riffBeat, active->loopBeats);
                        relocate (position);
                    }
                    else if (riffBeat >= active->lengthBeats)
                    {
                        playingNow = false;
                        run = false;
                    }
                    else
                    {
                        position = riffBeat;
                        relocate (position);
                    }
                }

                if (run)
                    advance (out, sampleFrom, spb, numSamples);

                expectedPpq = blockEndPpq;
                playingBpm.store (hostBpm, std::memory_order_relaxed);
            }
        }
        else
        {
            const double bpm = ownBpm();
            advance (out, 0.0, sampleRate * 60.0 / bpm, numSamples);
            playingBpm.store (bpm, std::memory_order_relaxed);
        }
    }

    // ---- publish ----------------------------------------------------------------
    playingFlag.store (playingNow, std::memory_order_relaxed);
    waitingFlag.store (armed, std::memory_order_relaxed);
    followingHostFlag.store (hostLocked && (playingNow || armed), std::memory_order_relaxed);
    beatPosition.store (playingNow ? position : -1.0, std::memory_order_relaxed);
    soundingFlag.store (sounding, std::memory_order_relaxed);

    if (playingNow)
        positionStamp.fetch_add (1, std::memory_order_relaxed);
}

} // namespace luthier
