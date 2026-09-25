#include "TunePlayer.h"

#include <cmath>

namespace luthier
{

namespace
{
    constexpr double kEps = 1.0e-9;
    constexpr double kNoSuppression = -1.0e300;

    /** The controllers TuneTimeline writes for realism (TuneMidi.cpp) that
        change how a note sounds: vibrato, sustain, portamento/slide, palm
        mute, legato, harmonic. A release puts them back to rest. */
    constexpr int kResetControllers[] = { 1, 64, 65, 67, 68, 73 };
}

//==============================================================================
TunePlayer::TunePlayer() = default;
TunePlayer::~TunePlayer() = default;

void TunePlayer::prepare (double newSampleRate, int)
{
    const double now = clockPosition();
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    setClock (now);
}

//==============================================================================
void TunePlayer::setTimeline (std::unique_ptr<TuneTimeline> timeline, double tempoBpm, double beatsPerBar)
{
    std::unique_ptr<TuneTimeline> displaced, displacedPass;

    {
        const juce::SpinLock::ScopedLockType sl (timelineLock);
        displaced = std::move (waiting);
        displacedPass = std::move (nextPass);   // built from the tune before this change
        waiting = std::move (timeline);
        hasWaiting = true;
        nextPassNumber = -1;
        waitingTempo = juce::jlimit (Tune::kMinTempo, Tune::kMaxTempo, tempoBpm);
        waitingBarBeats = juce::jmax (0.25, beatsPerBar);
    }

    // `displaced` and `displacedPass` are destroyed here, on the message thread.
}

void TunePlayer::clearTimeline()
{
    setTimeline (nullptr, 120.0, 4.0);
}

bool TunePlayer::hasTimeline() const
{
    const juce::SpinLock::ScopedLockType sl (timelineLock);
    return hasWaiting ? waiting != nullptr : active != nullptr;
}

int TunePlayer::getPassNeedingTimeline() const
{
    const juce::SpinLock::ScopedLockType sl (timelineLock);
    const auto* newest = hasWaiting ? waiting.get() : active.get();
    const int wanted = pass.load (std::memory_order_relaxed) + 1;

    if (newest == nullptr || ! newest->needsRebuildEachPass())
        return -1;

    // A pass built for somewhere else (before a seek, a stop) is no use.
    if (nextPass != nullptr && nextPassNumber == wanted)
        return -1;

    return wanted;
}

void TunePlayer::setNextPassTimeline (std::unique_ptr<TuneTimeline> timeline, int passNumber)
{
    std::unique_ptr<TuneTimeline> displaced;

    {
        const juce::SpinLock::ScopedLockType sl (timelineLock);
        displaced = std::move (nextPass);
        nextPass = std::move (timeline);
        nextPassNumber = passNumber;
    }
}

void TunePlayer::collectGarbage()
{
    std::array<std::unique_ptr<TuneTimeline>, kRetiredSlots> done;

    {
        const juce::SpinLock::ScopedLockType sl (timelineLock);

        for (size_t i = 0; i < retired.size(); ++i)
            done[i] = std::move (retired[i]);
    }

    // Destroyed here, outside the lock.
}

//==============================================================================
void TunePlayer::play()
{
    wantPlaying.store (true, std::memory_order_relaxed);
}

void TunePlayer::pause()
{
    wantPlaying.store (false, std::memory_order_relaxed);
    hostArmed.store (false, std::memory_order_relaxed);
}

void TunePlayer::stop()
{
    pause();
    pendingRewind.store (true, std::memory_order_relaxed);
}

void TunePlayer::togglePlayPause()
{
    if (isPlaying())
        pause();
    else
        play();
}

void TunePlayer::seek (double ppq)
{
    pendingSeek.store (juce::jmax (0.0, ppq), std::memory_order_relaxed);
}

void TunePlayer::seekToSection (int spanIndex)
{
    double target = -1.0;

    {
        const juce::SpinLock::ScopedLockType sl (timelineLock);
        const auto* newest = hasWaiting ? waiting.get() : active.get();

        if (newest != nullptr && juce::isPositiveAndBelow (spanIndex, (int) newest->getSections().size()))
            target = newest->getSections()[(size_t) spanIndex].ppq;
    }

    if (target >= 0.0)
        seek (target);
}

void TunePlayer::playFromSection (int spanIndex)
{
    seekToSection (juce::jmax (0, spanIndex));
    play();
}

void TunePlayer::skipSection (int delta)
{
    double target = -1.0;

    {
        const juce::SpinLock::ScopedLockType sl (timelineLock);
        const auto* newest = hasWaiting ? waiting.get() : active.get();

        if (newest == nullptr || newest->getSections().empty())
            return;

        const auto& sections = newest->getSections();
        const int count = (int) sections.size();
        const double now = getPositionPpq();
        int span = getPlayingSpan();

        if (! juce::isPositiveAndBelow (span, count))
            span = juce::jlimit (0, count - 1, newest->findSpanAt (now, false));

        if (delta < 0 && now - sections[(size_t) span].ppq > 1.0)
        {
            target = sections[(size_t) span].ppq;
        }
        else
        {
            int to = span + delta;

            // With Loop on, forward from the last section is the first again.
            to = isLooping() ? ((to % count) + count) % count : juce::jlimit (0, count - 1, to);
            target = sections[(size_t) to].ppq;
        }
    }

    seek (target);
}

//==============================================================================
bool TunePlayer::takePendingRhythmChange (TuneRhythmChange& change)
{
    const int counter = rhythmCounter.load (std::memory_order_acquire);

    if (counter == lastTakenRhythmCounter)
        return false;

    lastTakenRhythmCounter = counter;

    const auto key = rhythmKey.load (std::memory_order_relaxed);
    const int serial = (int) (key >> 32);
    const int index = (int) (key & 0xffffffff);

    const juce::SpinLock::ScopedLockType sl (timelineLock);

    // Published for a timeline since replaced: the new one publishes its own.
    if (active == nullptr || serial != activeSerial
          || ! juce::isPositiveAndBelow (index, (int) active->getRhythmChanges().size()))
        return false;

    change = active->getRhythmChanges()[(size_t) index];
    return true;
}

int TunePlayer::popRecordedEvents (RecordedEvent* destination, int maxEvents)
{
    if (destination == nullptr || maxEvents <= 0)
        return 0;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    recordFifo.prepareToRead (maxEvents, start1, size1, start2, size2);

    for (int i = 0; i < size1; ++i)
        destination[i] = recordRing[(size_t) (start1 + i)];

    for (int i = 0; i < size2; ++i)
        destination[size1 + i] = recordRing[(size_t) (start2 + i)];

    recordFifo.finishedRead (size1 + size2);
    return size1 + size2;
}

int TunePlayer::getNumSoundingNotes() const noexcept
{
    int count = 0;

    for (const auto* table : { &heldEngine, &heldOut })
        for (const auto& channel : *table)
            for (auto n : channel)
                count += n;

    return count;
}

//==============================================================================
void TunePlayer::setClock (double ppq) noexcept
{
    clockBase = ppq;
    clockSamples = 0;
    clockSamplesPerQuarter = sampleRate * 60.0 / juce::jmax (1.0, tempo);
}

int TunePlayer::offsetOf (double absolute) const noexcept
{
    return juce::jlimit (0, juce::jmax (0, blockLength - 1),
                         (int) std::llround ((absolute - blockFrom) * blockSamplesPerQuarter));
}

double TunePlayer::localOf (double absolute) const noexcept
{
    const double length = active != nullptr ? active->getLengthPpq() : 0.0;

    if (length <= 0.0 || absolute < 0.0)
        return absolute;

    return juce::jmax (0.0, absolute - std::floor (absolute / length + kEps) * length);
}

void TunePlayer::addClick (int offset, bool downbeat) noexcept
{
    if (clicks.count >= kMaxClicks)
        return;

    clicks.offsets[(size_t) clicks.count] = offset;
    clicks.downbeat[(size_t) clicks.count] = downbeat;
    ++clicks.count;
}

//==============================================================================
void TunePlayer::emit (const TuneEvent& e, int offset, juce::MidiBuffer& toEngine, juce::MidiBuffer& toMidiOut) noexcept
{
    const auto& m = e.message;

    if (m.isMetaEvent())
        return;

    const bool engineToo = e.part != TunePart::bass || bassToEngine.load (std::memory_order_relaxed);
    const int channel = m.getChannel();

    if (channel >= 1 && channel <= 16)
    {
        const auto ch = (size_t) (channel - 1);
        touchedChannels = (uint16_t) (touchedChannels | (1u << ch));

        if (m.isNoteOn())
        {
            const auto note = (size_t) m.getNoteNumber();

            heldOut[ch][note] = (uint8_t) juce::jmin (255, heldOut[ch][note] + 1);
            toMidiOut.addEvent (m, offset);

            if (engineToo)
            {
                heldEngine[ch][note] = (uint8_t) juce::jmin (255, heldEngine[ch][note] + 1);
                toEngine.addEvent (m, offset);
            }

            return;
        }

        if (m.isNoteOff())
        {
            // Only where its note-on went, and only a note this player started.
            const auto note = (size_t) m.getNoteNumber();

            if (heldOut[ch][note] > 0)
            {
                --heldOut[ch][note];
                toMidiOut.addEvent (m, offset);
            }

            if (heldEngine[ch][note] > 0)
            {
                --heldEngine[ch][note];
                toEngine.addEvent (m, offset);
            }

            return;
        }
    }

    toMidiOut.addEvent (m, offset);

    if (engineToo)
        toEngine.addEvent (m, offset);
}

void TunePlayer::chase (double localPpq, int offset, juce::MidiBuffer& toEngine, juce::MidiBuffer& toMidiOut) noexcept
{
    if (active == nullptr || localPpq <= kEps)
        return;

    for (auto& channel : chaseCount)
        channel.fill (0);

    const auto& events = active->getEvents();

    // Which notes started before `localPpq` and have not ended by it. A note
    // starting exactly there plays from the timeline itself.
    for (int i = 0; i < (int) events.size(); ++i)
    {
        const auto& e = events[(size_t) i];

        if (e.ppq >= localPpq + kEps)
            break;

        const auto& m = e.message;
        const int channel = m.getChannel();

        if (channel < 1 || channel > 16)
            continue;

        const auto ch = (size_t) (channel - 1);
        const auto note = (size_t) m.getNoteNumber();

        if (m.isNoteOn())
        {
            if (e.ppq < localPpq - kEps)
            {
                chaseCount[ch][note] = (uint8_t) juce::jmin (255, chaseCount[ch][note] + 1);
                chaseEvent[ch][note] = i;
            }
        }
        else if (m.isNoteOff() && chaseCount[ch][note] > 0)
        {
            --chaseCount[ch][note];
        }
    }

    for (size_t ch = 0; ch < 16; ++ch)
    {
        for (size_t note = 0; note < 128; ++note)
        {
            if (chaseCount[ch][note] == 0)
                continue;

            const int index = chaseEvent[ch][note];

            // The controllers set with the note (palm mute, vibrato, a bend's
            // start) sit just before it at the same position; they come too.
            int first = index;

            while (first > 0)
            {
                const auto& before = events[(size_t) (first - 1)];

                if (before.ppq != events[(size_t) index].ppq || before.message.getChannel() != (int) ch + 1
                      || ! (before.message.isController() || before.message.isPitchWheel()))
                    break;

                --first;
            }

            for (int i = first; i <= index; ++i)
                emit (events[(size_t) i], offset, toEngine, toMidiOut);
        }
    }
}

void TunePlayer::releaseAll (int offset, juce::MidiBuffer& toEngine, juce::MidiBuffer& toMidiOut) noexcept
{
    auto release = [offset] (NoteTable& held, juce::MidiBuffer& buffer)
    {
        for (int ch = 0; ch < 16; ++ch)
        {
            for (int note = 0; note < 128; ++note)
            {
                auto& count = held[(size_t) ch][(size_t) note];

                while (count > 0)
                {
                    buffer.addEvent (juce::MidiMessage::noteOff (ch + 1, note), offset);
                    --count;
                }
            }
        }
    };

    release (heldOut, toMidiOut);
    release (heldEngine, toEngine);

    for (int ch = 0; ch < 16; ++ch)
    {
        if ((touchedChannels & (1u << ch)) == 0)
            continue;

        for (int cc : kResetControllers)
        {
            const auto reset = juce::MidiMessage::controllerEvent (ch + 1, cc, 0);
            toMidiOut.addEvent (reset, offset);
            toEngine.addEvent (reset, offset);
        }

        const auto centre = juce::MidiMessage::pitchWheel (ch + 1, 8192);
        toMidiOut.addEvent (centre, offset);
        toEngine.addEvent (centre, offset);
    }

    touchedChannels = 0;
}

//==============================================================================
void TunePlayer::publishRhythm (int index) noexcept
{
    const auto key = ((juce::int64) activeSerial << 32) | (juce::int64) (uint32_t) index;
    rhythmKey.store (key, std::memory_order_relaxed);
    rhythmCounter.fetch_add (1, std::memory_order_release);
}

void TunePlayer::publishRhythmAt (double localPpq) noexcept
{
    if (active == nullptr)
        return;

    const auto& changes = active->getRhythmChanges();
    int found = -1;

    for (int i = 0; i < (int) changes.size(); ++i)
        if (changes[(size_t) i].ppq <= juce::jmax (0.0, localPpq) + kEps)
            found = i;

    if (found >= 0)
        publishRhythm (found);
}

void TunePlayer::publishState() noexcept
{
    const double now = clockPosition();
    const double local = localOf (juce::jmax (0.0, now));
    const int span = active != nullptr ? active->findSpanAt (local, false) : -1;

    position.store (local, std::memory_order_relaxed);
    playingSpan.store (span, std::memory_order_relaxed);
    playingSection.store (span >= 0 ? active->getSections()[(size_t) span].sectionIndex : -1, std::memory_order_relaxed);
    pass.store (currentPass, std::memory_order_relaxed);
    countingIn.store (running && countInBeats > 0.0 && now < suppressBefore - kEps, std::memory_order_relaxed);
}

//==============================================================================
bool TunePlayer::swapIn (std::unique_ptr<TuneTimeline>& incoming, double atAbsolute, int offset, double& shift,
                         juce::MidiBuffer& toEngine, juce::MidiBuffer& toMidiOut) noexcept
{
    shift = 0.0;
    const double oldLength = active != nullptr ? active->getLengthPpq() : 0.0;

    if (active != nullptr)
    {
        std::unique_ptr<TuneTimeline>* slot = nullptr;

        for (auto& r : retired)
        {
            if (r == nullptr)
            {
                slot = &r;
                break;
            }
        }

        // The message thread has not collected: keep playing this one for now.
        if (slot == nullptr)
            return false;

        *slot = std::move (active);
    }

    releaseAll (offset, toEngine, toMidiOut);
    active = std::move (incoming);
    ++activeSerial;

    const double newLength = active != nullptr ? active->getLengthPpq() : 0.0;

    if (newLength <= 0.0)
        return true;

    // Keep the place in the tune. On the internal clock that is the same
    // point of the same pass (or the next pass's start, if the tune got
    // shorter than where it was); the host's clock is wherever the host is.
    double target = atAbsolute;

    if (! wasFollowingHost)
    {
        const double local = oldLength > 0.0 ? atAbsolute - (double) currentPass * oldLength : atAbsolute;

        target = local < newLength - kEps ? (double) currentPass * newLength + local
                                          : (double) (currentPass + 1) * newLength;
        shift = target - atAbsolute;

        clockBase += shift;
        blockFrom += shift;
        lastTo += shift;
        chaseAt += shift;

        if (suppressBefore > kNoSuppression)
            suppressBefore += shift;
    }

    if (running)
    {
        // What the new timeline holds across this point starts here.
        if (! chasePending || chaseAt < target)
            chaseAt = target;

        chasePending = true;
        publishRhythmAt (localOf (chaseAt));
    }

    return true;
}

bool TunePlayer::swapInWaiting (double atAbsolute, int offset, double& shift,
                                juce::MidiBuffer& toEngine, juce::MidiBuffer& toMidiOut) noexcept
{
    if (! swapIn (waiting, atAbsolute, offset, shift, toEngine, toMidiOut))
        return false;

    hasWaiting = false;
    tempo = waitingTempo;
    barBeats = waitingBarBeats;
    return true;
}

void TunePlayer::finishAtEnd (int offset, juce::MidiBuffer& toEngine, juce::MidiBuffer& toMidiOut) noexcept
{
    // Played to the end without Loop: stop, and let the next Play start over.
    releaseAll (offset, toEngine, toMidiOut);
    running = false;
    wantPlaying.store (false, std::memory_order_relaxed);
    countInBeats = 0.0;
    currentPass = 0;
    setClock (active != nullptr ? active->getLengthPpq() : 0.0);
}

//==============================================================================
void TunePlayer::renderBlock (int numSamples, const HostInfo& host,
                              juce::MidiBuffer& toEngine, juce::MidiBuffer& toMidiOut) noexcept
{
    clicks.count = 0;
    stateBoundaryThisBlock = false;
    blockRecordable = false;
    blockTransport.running = false;
    blockTransport.followingHost = false;

    if (numSamples <= 0)
        return;

    // Offline rendering: the message-thread service, in step with the render.
    if (offlineService != nullptr)
        offlineService();

    blockLength = numSamples;
    blockFrom = clockPosition();
    blockSamplesPerQuarter = clockSamplesPerQuarter;

    // Held for the block, so the message thread cannot swap anything under it.
    // A miss only means timelines are not exchanged this block.
    const juce::SpinLock::ScopedTryLockType sl (timelineLock);
    const bool canSwap = sl.isLocked();

    // --- commands --------------------------------------------------------------------
    if (pendingRewind.exchange (false, std::memory_order_relaxed))
    {
        releaseAll (0, toEngine, toMidiOut);
        currentPass = 0;
        setClock (0.0);
        suppressBefore = kNoSuppression;
        countInBeats = 0.0;
        chasePending = false;
    }

    bool want = wantPlaying.load (std::memory_order_relaxed);
    const bool hostPlaying = host.hasPosition && host.isPlaying;

    // The host stopped under a tune that was following it: the tune pauses,
    // and follows the host again when it next plays.
    if (want && wasFollowingHost && ! hostPlaying)
    {
        want = false;
        wantPlaying.store (false, std::memory_order_relaxed);
        hostArmed.store (true, std::memory_order_relaxed);
    }
    else if (! want && hostPlaying && hostArmed.load (std::memory_order_relaxed))
    {
        want = true;
        wantPlaying.store (true, std::memory_order_relaxed);
    }

    const bool hostDriving = want && hostPlaying;
    const double seekTo = pendingSeek.exchange (-1.0, std::memory_order_relaxed);
    const bool seeking = seekTo >= 0.0 && ! hostDriving;

    // A new timeline goes in now when nothing is playing, or at a seek, which
    // is a jump anyway. Playing, it waits for the next bar line (below).
    if (canSwap && hasWaiting && (! running || ! want || seeking || active == nullptr))
    {
        double shift = 0.0;

        if (swapInWaiting (clockPosition(), 0, shift, toEngine, toMidiOut))
            setClock (clockPosition());   // the new tempo, from here
    }

    if (active == nullptr || active->getLengthPpq() <= 0.0)
    {
        if (running)
        {
            releaseAll (0, toEngine, toMidiOut);
            running = false;
        }

        wasFollowingHost = false;
        followingHost.store (false, std::memory_order_relaxed);
        publishState();
        return;
    }

    const double length = active->getLengthPpq();

    // --- seek ------------------------------------------------------------------------
    if (seeking)
    {
        releaseAll (0, toEngine, toMidiOut);

        const double target = juce::jlimit (0.0, length, seekTo);
        setClock ((double) currentPass * length + target);
        suppressBefore = clockPosition();
        countInBeats = 0.0;
        chasePending = true;
        chaseAt = suppressBefore;
        publishRhythmAt (target);
    }

    // --- start and stop ------------------------------------------------------------------
    if (! want)
    {
        if (running)
        {
            releaseAll (0, toEngine, toMidiOut);
            running = false;
        }

        countInBeats = 0.0;
        wasFollowingHost = false;
        followingHost.store (false, std::memory_order_relaxed);
        publishState();
        return;
    }

    bool justStarted = false;

    if (! running)
    {
        running = true;
        justStarted = true;

        if (! hostDriving)
        {
            // From where it was, in the first pass's terms, so a count-in
            // never reaches back into another pass.
            double startAt = localOf (juce::jmax (0.0, clockPosition()));

            if (clockPosition() >= (double) (currentPass + 1) * length - kEps)
                startAt = 0.0;   // played to the end last time: Play starts again

            currentPass = 0;
            setClock (startAt);
            suppressBefore = startAt;
            chasePending = true;
            chaseAt = startAt;
            publishRhythmAt (startAt);

            const int bars = countInBars.load (std::memory_order_relaxed);

            if (bars > 0)
            {
                countInBeats = bars * barBeats;
                setClock (startAt - countInBeats);
            }
        }
    }

    // --- the block's range ---------------------------------------------------------------
    double from = 0.0, to = 0.0;

    if (hostDriving)
    {
        const double samplesPerQuarter = sampleRate * 60.0 / juce::jlimit (1.0, 1000.0, host.bpm);

        from = host.ppqPosition;
        to = host.ppqPosition + (double) numSamples / samplesPerQuarter;

        // Continuing from where the last block ended, the last block's end is
        // used exactly, so nothing on the boundary plays twice or not at all.
        // Anything else is the host jumping (a locate, its own loop) or the
        // host taking over: what sounds ends, and what the tune holds at the
        // new place starts.
        const double tolerance = 2.0 * (double) numSamples / samplesPerQuarter;

        if (wasFollowingHost && ! justStarted && std::abs (from - lastTo) <= tolerance)
        {
            from = lastTo;
        }
        else
        {
            releaseAll (0, toEngine, toMidiOut);
            suppressBefore = kNoSuppression;
            countInBeats = 0.0;
            chasePending = true;
            chaseAt = from;
            publishRhythmAt (localOf (from));
        }

        to = juce::jmax (to, from);
        blockSamplesPerQuarter = samplesPerQuarter;
    }
    else
    {
        from = clockPosition();
        to = clockBase + (double) (clockSamples + numSamples) / clockSamplesPerQuarter;
        blockSamplesPerQuarter = clockSamplesPerQuarter;
    }

    blockFrom = from;
    wasFollowingHost = hostDriving;
    followingHost.store (hostDriving, std::memory_order_relaxed);

    blockTransport.followingHost = hostDriving;
    blockTransport.bpm = hostDriving ? host.bpm : tempo;
    blockTransport.ppq = from;
    blockTransport.running = from >= suppressBefore - kEps;
    blockRecordable = blockTransport.running;

    renderSegments (from, to, canSwap, hostDriving, toEngine, toMidiOut);

    if (running)
    {
        if (hostDriving)
        {
            lastTo = to;
            setClock (to);
        }
        else
        {
            // The clock moves by samples; a new tempo re-anchors it here.
            clockSamples += numSamples;

            if (std::abs (sampleRate * 60.0 / juce::jmax (1.0, tempo) - clockSamplesPerQuarter) > 1.0e-9)
                setClock (clockPosition());
        }

        if (countInBeats > 0.0 && clockPosition() >= suppressBefore - kEps)
            countInBeats = 0.0;
    }

    publishState();
}

void TunePlayer::renderSegments (double from, double to, bool canSwap, bool hostDriving,
                                 juce::MidiBuffer& toEngine, juce::MidiBuffer& toMidiOut) noexcept
{
    const bool looping = loop.load (std::memory_order_relaxed);
    const bool clicking = metronome.load (std::memory_order_relaxed);
    double segFrom = from;

    // Bounded, so a tiny tune in a huge block cannot spin.
    for (int guard = 0; segFrom < to && guard < 64; ++guard)
    {
        if (active == nullptr || active->getLengthPpq() <= 0.0)
        {
            // The timeline was cleared at a bar line.
            releaseAll (offsetOf (segFrom), toEngine, toMidiOut);
            running = false;
            return;
        }

        const double length = active->getLengthPpq();
        const int passNumber = juce::jmax (0, (int) std::floor (segFrom / length + kEps));
        const double passStart = (double) passNumber * length;
        const double passEnd = passStart + length;

        // Past the end without Loop. The internal clock stops there; a host
        // playing on past it hears nothing more.
        if (passNumber > 0 && ! looping)
        {
            if (! hostDriving)
                finishAtEnd (offsetOf (segFrom), toEngine, toMidiOut);

            return;
        }

        // A new pass: an improvised tune swaps in the timeline built for it.
        if (passNumber != currentPass)
        {
            currentPass = passNumber;

            if (canSwap && ! hasWaiting && nextPass != nullptr && nextPassNumber == passNumber)
            {
                double shift = 0.0;

                if (swapIn (nextPass, segFrom, offsetOf (segFrom), shift, toEngine, toMidiOut))
                {
                    nextPassNumber = -1;
                    segFrom += shift;
                    to += shift;
                    continue;
                }
            }
        }

        double segEnd = juce::jmin (to, passEnd);
        bool swapAtEnd = false;

        // An edit waiting for the next bar line (2: "changes take effect on
        // the next bar boundary").
        if (canSwap && hasWaiting)
        {
            const double local = segFrom - passStart;
            const double barLine = passStart + std::ceil (local / barBeats - kEps) * barBeats;

            if (barLine <= segFrom + kEps)
            {
                double shift = 0.0;

                if (swapInWaiting (segFrom, offsetOf (segFrom), shift, toEngine, toMidiOut))
                {
                    segFrom += shift;
                    to += shift;
                    continue;
                }
            }
            else if (barLine < segEnd)
            {
                segEnd = barLine;
                swapAtEnd = true;
            }
        }

        const bool reachesEnd = segEnd >= passEnd - kEps;
        const double localFrom = segFrom - passStart;

        // The pass's closing note-offs sit exactly at its length; reaching the
        // end takes them with it, ahead of the next pass's first notes.
        const double localTo = reachesEnd ? length + kEps : segEnd - passStart;
        const auto* timeline = active.get();

        // Notes held across where playback (re)started.
        if (chasePending && chaseAt < segEnd)
        {
            const double at = juce::jmax (chaseAt, segFrom);
            chase (at - passStart, offsetOf (at), toEngine, toMidiOut);
            chasePending = false;
        }

        timeline->forEachEventInRange (localFrom, localTo, false, [&] (const TuneEvent& e, double at)
        {
            const double absolute = passStart + at;

            if (absolute >= suppressBefore - kEps)
                emit (e, offsetOf (absolute), toEngine, toMidiOut);
        });

        // Rhythm settings that come into force in this segment.
        const auto& changes = timeline->getRhythmChanges();

        for (int i = 0; i < (int) changes.size(); ++i)
        {
            const double at = changes[(size_t) i].ppq;

            if (at >= localFrom && at < localTo && passStart + at >= suppressBefore - kEps)
            {
                publishRhythm (i);

                if (changes[(size_t) i].stateBoundary)
                    stateBoundaryThisBlock = true;
            }
        }

        // The count-in's clicks, on the beats before the start.
        if (countInBeats > 0.0)
        {
            for (int k = (int) std::ceil (countInBeats - kEps); k >= 1; --k)
            {
                const double at = suppressBefore - (double) k;

                if (at >= segFrom && at < segEnd)
                    addClick (offsetOf (at), std::abs (std::fmod ((double) k, barBeats)) < 1.0e-6);
            }
        }

        // The metronome, on the tune's beats.
        if (clicking)
        {
            const double upTo = juce::jmin (localTo, length);

            for (double beat = std::ceil (juce::jmax (0.0, localFrom)); beat < upTo; beat += 1.0)
                if (passStart + beat >= suppressBefore - kEps)
                    addClick (offsetOf (passStart + beat), std::abs (std::fmod (beat, barBeats)) < 1.0e-6);
        }

        if (swapAtEnd)
        {
            double shift = 0.0;

            // No retired slot free: the old timeline plays on to the next bar line.
            if (swapInWaiting (segEnd, offsetOf (segEnd), shift, toEngine, toMidiOut))
                to += shift;

            segFrom = segEnd + shift;
            continue;
        }

        if (! reachesEnd)
            break;

        if (! looping && ! hostDriving)
        {
            finishAtEnd (offsetOf (passEnd), toEngine, toMidiOut);
            return;
        }

        segFrom = passEnd;
    }
}

//==============================================================================
void TunePlayer::captureInput (const juce::MidiBuffer& incoming) noexcept
{
    if (! recordArmed.load (std::memory_order_relaxed) || ! blockRecordable || active == nullptr)
        return;

    const double length = active->getLengthPpq();

    if (length <= 0.0)
        return;

    for (const auto metadata : incoming)
    {
        // Read from the bytes: making a MidiMessage of a long SysEx would allocate.
        if (metadata.numBytes < 3)
            continue;

        const int status = metadata.data[0] & 0xf0;
        const int velocity = metadata.data[2];
        const bool on = status == 0x90 && velocity > 0;
        const bool off = status == 0x80 || (status == 0x90 && velocity == 0);

        if (! on && ! off)
            continue;

        const double absolute = blockFrom + (double) metadata.samplePosition / blockSamplesPerQuarter;

        if (absolute < suppressBefore - kEps || absolute < 0.0)
            continue;

        RecordedEvent event;
        event.ppq = localOf (absolute);
        event.span = active->findSpanAt (event.ppq, false);
        event.note = metadata.data[1] & 0x7f;
        event.velocity = velocity;
        event.isNoteOn = on;

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        recordFifo.prepareToWrite (1, start1, size1, start2, size2);

        if (size1 > 0)
        {
            recordRing[(size_t) start1] = event;
            recordFifo.finishedWrite (1);
        }
    }
}

} // namespace luthier
