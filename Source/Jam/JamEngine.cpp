#include "JamEngine.h"

namespace luthier
{

const char* getJamStateName (JamState s) noexcept
{
    switch (s)
    {
        case JamState::off:      return "OFF";
        case JamState::armed:    return "ARMED";
        case JamState::counting: return "COUNT";
        case JamState::playing:  return "PLAYING";
        case JamState::ending:   return "ENDING";
    }

    return "";
}

constexpr int JamEngine::kFillEveryBars[];

namespace
{
    constexpr double kTapWindowSeconds = 3.0;
    constexpr int kBassLane = jam::numDrumLanes;   // index 7 in the status lanes

    /** jam-mode 12: drum tokens to sounds and velocities. */
    bool drumFor (int lane, char token, double coin, DrumSound& sound, int& velocity) noexcept
    {
        velocity = 0;

        auto plain = [&] (DrumSound s) -> bool
        {
            switch (token)
            {
                case 'g': velocity = 30;  break;
                case 'x': velocity = 90;  break;
                case 'X': velocity = 118; break;
                case '?': if (coin < 0.5) velocity = 90; break;
                default:  break;
            }

            sound = s;
            return velocity > 0;
        };

        switch (lane)
        {
            case jam::kick:
                return plain (DrumSound::kick);

            case jam::snare:
                if (token == 'r') { sound = DrumSound::snareRim;   velocity = 110; return true; }
                if (token == 'b') { sound = DrumSound::snareBrush; velocity = 90;  return true; }
                if (token == 'g') { sound = DrumSound::snareGhost; velocity = 30;  return true; }
                return plain (DrumSound::snare);

            case jam::hat:
                if (token == 'o') { sound = DrumSound::hatOpen;  velocity = 90;  return true; }
                if (token == 'O') { sound = DrumSound::hatOpen;  velocity = 118; return true; }
                if (token == 'p') { sound = DrumSound::hatPedal; velocity = 70;  return true; }
                return plain (DrumSound::hatClosed);

            case jam::ride:
                if (token == 'b') { sound = DrumSound::rideBell; velocity = 90; return true; }
                return plain (DrumSound::ride);

            case jam::crash:
                return plain (DrumSound::crash);

            case jam::tom:
                switch (token)
                {
                    case '1': sound = DrumSound::tomHigh;  velocity = 90;  return true;
                    case '2': sound = DrumSound::tomMid;   velocity = 90;  return true;
                    case '3': sound = DrumSound::tomFloor; velocity = 90;  return true;
                    case 'H': sound = DrumSound::tomHigh;  velocity = 118; return true;
                    case 'M': sound = DrumSound::tomMid;   velocity = 118; return true;
                    case 'F': sound = DrumSound::tomFloor; velocity = 118; return true;
                    default:  return plain (DrumSound::tomMid);
                }

            case jam::perc:
                if (token == 'r') { sound = DrumSound::rim;    velocity = 90;  return true; }
                if (token == 's') { sound = DrumSound::shaker; velocity = 70;  return true; }
                if (token == 'S') { sound = DrumSound::shaker; velocity = 110; return true; }
                return plain (DrumSound::rim);

            default:
                return false;
        }
    }

    /** jam-mode 4.3: each lane's steady push or pull, in seconds. */
    double lanePush (int lane) noexcept
    {
        switch (lane)
        {
            case jam::hat:   return -0.002;
            case jam::snare: return 0.003;
            default:         return 0.0;
        }
    }
}

//==============================================================================
JamEngine::JamEngine()
{
    for (auto& s : styleSlots)
        s.store (nullptr, std::memory_order_relaxed);

    for (auto& t : tapQueue)
        t.store (0, std::memory_order_relaxed);
}

JamEngine::~JamEngine()
{
    delete chordMapIncoming.exchange (nullptr);
    delete chordMapRetired.exchange (nullptr);
    delete chordMap;
}

void JamEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);

    follower.prepare (sr);
    kit.prepare (sr, maxBlock);
    bass.prepare (sr, maxBlock);

    drumsBuffer.setSize (2, maxBlock, false, true, false);
    bassBuffer.setSize (1, maxBlock, false, true, false);
    drumsOut.setSize (2, maxBlock, false, true, false);
    bassOut.setSize (2, maxBlock, false, true, false);
    drumsOut.clear();
    bassOut.clear();
    midiOut.ensureSize (8192);

    for (auto* s : { &drumsGainL, &drumsGainR, &bassGainL, &bassGainR })
        s->prepare (sr, 0.020);

    for (auto& d : stemDc)
        d.prepare (sr, 5.0);

    reset();
}

void JamEngine::reset() noexcept
{
    clearQueue (false);
    kit.reset();
    bass.reset();
    follower.reset();
    predictor.reset();
    bassLine.reset();

    cursor.valid = false;
    numPendingChords = 0;
    heardChord = bassChord = lastRhythmChord = ChordSymbol {};
    bassMidiNote = -1;
    bassSounding = false;
    lastBassStepMusical = INT64_MIN;
    anticipatedUpTo = -1.0e300;

    clockSource = JamStatus::Clock::own;
    externalWasRunning = false;
    haveLastBlockEnd = false;
    pendingJoinAtBar = false;
    endingPending = cutRequested = false;
    fillActive = fillNowPending = crashNextDownbeat = false;
    fillNowNextBar = -1;
    numTaps = 0;
    hitCount = hitWrite = 0;
    lastSection = -1;
    hintIntensity = 0;
    dynamicsOffset = 0;

    state = JamState::armed;
    stateAtomic.store ((int) (settings.enabled ? JamState::armed : JamState::off), std::memory_order_relaxed);

    for (auto* s : { &drumsGainL, &drumsGainR, &bassGainL, &bassGainR })
        s->snapTo (0.0);

    for (auto& d : stemDc)
        d.reset();
}

//==============================================================================
void JamEngine::setStyleSlot (int index, const JamStyle* style) noexcept
{
    if (juce::isPositiveAndBelow (index, jam::kNumStyleChoices))
        styleSlots[(size_t) index].store (style, std::memory_order_release);
}

const JamStyle* JamEngine::getStyleSlot (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, jam::kNumStyleChoices)
             ? styleSlots[(size_t) index].load (std::memory_order_acquire) : nullptr;
}

void JamEngine::setChordMap (std::unique_ptr<JamChordMap> map)
{
    collectGarbage();

    // A map the audio thread never took is simply replaced.
    delete chordMapIncoming.exchange (map.release(), std::memory_order_acq_rel);
}

void JamEngine::collectGarbage()
{
    delete chordMapRetired.exchange (nullptr, std::memory_order_acq_rel);
}

void JamEngine::tapAtSample (int64_t sample) noexcept
{
    const int slot = tapWrite.load (std::memory_order_relaxed);
    tapQueue[(size_t) (slot % kTapQueue)].store (sample, std::memory_order_relaxed);
    tapWrite.store (slot + 1, std::memory_order_release);
}

bool JamEngine::getBarPosition (int64_t sample, double& quartersIntoBar, double& barQuarters,
                                double& samplesPerQuarter) const noexcept
{
    if (! isBandRunning() || ! cursor.valid)
        return false;

    const double ppq = mapping.ppqOf (sample);
    barQuarters = cursor.barLength;
    const double k = std::floor ((ppq - cursor.barStart) / barQuarters + 1.0e-9);
    quartersIntoBar = juce::jmax (0.0, ppq - (cursor.barStart + k * barQuarters));
    samplesPerQuarter = mapping.samplesPerQuarter;
    return ppq >= 0.0;
}

bool JamEngine::getOwnClock (double& ppq, double& bpm) const noexcept
{
    if (! (state == JamState::playing || state == JamState::ending) || clockSource != JamStatus::Clock::own || ! cursor.valid)
        return false;

    ppq = own.ppqOf (sampleClock);
    bpm = own.bpm (sr);
    return ppq >= 0.0;
}

void JamEngine::addStickClicks (const int* offsets, const bool* downbeats, int count) noexcept
{
    for (int i = 0; i < count; ++i)
    {
        Event e;
        e.type = EventType::drum;
        e.value = (int16_t) DrumSound::sticks;
        e.velocity = downbeats != nullptr && downbeats[i] ? 1.0f : 0.8f;
        e.sample = sampleClock + offsets[i] + latency;
        e.musical = sampleClock + offsets[i];
        queue (e);
    }
}

//==============================================================================
bool JamEngine::queue (const Event& in) noexcept
{
    if (numEvents >= kMaxEvents)
        return false;

    Event e = in;
    e.order = eventOrder++;

    // Sorted by sample, then priority (chords before steps), then arrival.
    int i = numEvents;

    while (i > 0)
    {
        const auto& p = events[(size_t) (i - 1)];

        if (p.sample < e.sample || (p.sample == e.sample && p.priority <= e.priority))
            break;

        events[(size_t) i] = p;
        --i;
    }

    events[(size_t) i] = e;
    ++numEvents;
    return true;
}

void JamEngine::clearQueue (bool keepOffs) noexcept
{
    if (! keepOffs)
    {
        numEvents = 0;
        return;
    }

    int kept = 0;

    for (int i = 0; i < numEvents; ++i)
        if (events[(size_t) i].type == EventType::drumOff)
            events[(size_t) kept++] = events[(size_t) i];

    numEvents = kept;
}

void JamEngine::addChordChange (int64_t musical, double ppq, const ChordSymbol& chord, bool anticipated) noexcept
{
    // A newer decision replaces pending ones at or after it.
    int kept = 0;

    for (int i = 0; i < numPendingChords; ++i)
    {
        const auto& p = pendingChords[(size_t) i];

        if (p.applied || p.musical < musical)
            pendingChords[(size_t) kept++] = p;
    }

    numPendingChords = kept;

    if (numPendingChords >= (int) pendingChords.size())
        return;

    PendingChord p;
    p.musical = musical;
    p.ppq = ppq;
    p.chord = chord;
    p.anticipated = anticipated;
    pendingChords[(size_t) numPendingChords++] = p;
}

void JamEngine::applyChordsUpTo (int64_t musical) noexcept
{
    int kept = 0;

    for (int i = 0; i < numPendingChords; ++i)
    {
        auto p = pendingChords[(size_t) i];

        if (! p.applied && p.musical <= musical)
        {
            bassChord = p.chord;
            continue;
        }

        pendingChords[(size_t) kept++] = p;
    }

    numPendingChords = kept;
}

//==============================================================================
const JamStyle* JamEngine::activeStyle() const noexcept
{
    if (latchedStyle != nullptr)
        return latchedStyle;

    return getStyleSlot (0);
}

const JamMeterSet* JamEngine::activeMeter() const noexcept
{
    if (auto* s = activeStyle())
        return s->findMeter (cursor.numerator, cursor.denominator);

    return nullptr;
}

void JamEngine::buildGenericBar() noexcept
{
    // 3: kick on 1, hat on every beat, snare on the last beat, bass root on 1.
    const int steps = cursor.stepsInBar, spq = cursor.stepsPerQuarter;
    genericBar.clear (steps);
    genericBar.hasBass = true;
    genericBar.drums[jam::kick][0] = 'x';

    for (int s = 0; s < steps; s += spq)
        genericBar.drums[jam::hat][(size_t) s] = 'x';

    const int lastBeat = ((steps - 1) / spq) * spq;
    genericBar.drums[jam::snare][(size_t) lastBeat] = 'x';
    genericBar.bass[0] = 'R';
}

const JamPattern& JamEngine::patternFor (int step) noexcept
{
    juce::ignoreUnused (step);
    const auto* meter = activeMeter();

    if (genericGroove || meter == nullptr)
    {
        if (genericBar.steps != cursor.stepsInBar)
            buildGenericBar();

        return genericBar;
    }

    if (state == JamState::ending && endingBar == cursor.bar)
        return meter->ending;

    // 2.3: below 50 bpm the double-time bar, above 220 the half-time bar.
    if (currentBpm < 50.0)  return meter->doubleTime;
    if (currentBpm > 220.0) return meter->halfTime;

    return meter->grooves[(size_t) juce::jlimit (0, 1, latchedVariation)]
                         [(size_t) juce::jlimit (0, 4, effectiveIntensity - 1)];
}

bool JamEngine::usingTuneSource() const noexcept
{
    if (chordMap == nullptr || chordMap->count == 0)
        return false;

    switch ((ChordSource) settings.chordSource)
    {
        case ChordSource::tune:      return status.followingTune;
        case ChordSource::automatic: return status.followingTune;
        case ChordSource::live:      return false;
    }

    return false;
}

uint64_t JamEngine::laneSeed (int64_t bar, int lane) const noexcept
{
    // 0.5: (preset jam seed, bar index, lane).
    uint64_t x = seedValue.load (std::memory_order_relaxed) * 0x9E3779B97F4A7C15ull;
    x ^= (uint64_t) (bar + 1000000) * 0xBF58476D1CE4E5B9ull;
    x ^= (uint64_t) (lane + 17) * 0x94D049BB133111EBull;
    x ^= x >> 31;
    return x == 0 ? 1 : x;
}

int JamEngine::walkStepsLeft (const JamPattern& pattern, int step) const noexcept
{
    int left = 0;

    for (int s = step; s < pattern.steps; ++s)
    {
        const char t = pattern.bassAt (s);

        if (t == 'W' || t == 'A')
            ++left;
    }

    return left;
}

ChordSymbol JamEngine::nextKnownChord (double afterPpq, double withinPpq, bool& found) const noexcept
{
    found = false;

    // Anticipated changes already decided.
    for (int i = 0; i < numPendingChords; ++i)
    {
        const auto& p = pendingChords[(size_t) i];

        if (! p.applied && p.anticipated && p.ppq > afterPpq + 1.0e-9 && p.ppq < withinPpq + 1.0e-9)
        {
            found = true;
            return p.chord;
        }
    }

    if (status.followingTune && chordMap != nullptr)
    {
        JamChordMap::Entry e;
        double at = 0.0;

        if (chordMap->nextChange (afterPpq + tuneOffset, withinPpq + tuneOffset + 1.0e-6, e, at))
        {
            found = true;
            return e.toSymbol();
        }
    }
    else if (settings.predict && predictor.isPredicting())
    {
        double at = 0.0;
        ChordSymbol c;

        if (predictor.predictNext (afterPpq, withinPpq + 1.0e-6, at, c) && c != bassChord)
        {
            found = true;
            return c;
        }
    }

    return {};
}

//==============================================================================
void JamEngine::applySettingsEdges() noexcept
{
    previousSettings = settings;
    settings = pendingSettings;

    if (! haveSettings)
    {
        previousSettings = settings;
        haveSettings = true;
        return;
    }

    // jam_play: rising starts (from Armed or during an ending), falling stops
    // a counting or playing band. The processor mirrors the state back into
    // the parameter, so an edge that only confirms the state does nothing.
    if (settings.play && ! previousSettings.play)
        if (state == JamState::armed || state == JamState::ending)
            startRequested.store (true, std::memory_order_release);

    if (! settings.play && previousSettings.play)
        if (state == JamState::counting || state == JamState::playing)
            stopRequests.fetch_add (1, std::memory_order_acq_rel);

    if (settings.fillNow && ! previousSettings.fillNow)
        fillRequested.store (true, std::memory_order_release);
}

void JamEngine::handleCommands (const BlockContext& ctx, int64_t blockStart) noexcept
{
    if (panicRequested.exchange (false, std::memory_order_acq_rel))
    {
        // 2.2: a 5 ms choke of every voice, back to Armed.
        cut (0.005);
        toggleRequests.store (0);
        stopRequests.store (0);
        startRequested.store (false);
    }

    const bool hostStart = ctx.hostHasPpq && ctx.hostPlaying;
    const auto mode = (StartMode) settings.startMode;
    const double ownTempo = (ctx.tuneRunning && ! ctx.hostPlaying) ? ctx.tuneBpm : ctx.effectiveTempo;

    auto doStart = [&]
    {
        if (state != JamState::armed && state != JamState::ending)
            return;

        if (state == JamState::ending)
        {
            // START during an ending: the ending is dropped, the band plays on.
            endingPending = false;
            state = JamState::playing;
            return;
        }

        if (mode == StartMode::hostTransport && ctx.hasPlayHead)
            return;   // the DAW owns start and stop

        if (hostStart && (mode == StartMode::automatic || mode == StartMode::hostTransport))
        {
            pendingJoinAtBar = true;   // joins the host grid at the next bar line
            return;
        }

        startBand (blockStart, ownTempo, mode == StartMode::countIn ? settings.countInBars : 0);
    };

    auto doStop = [&]
    {
        if (state == JamState::counting)
            cut (0.020);
        else if (state == JamState::playing)
        {
            if (settings.ending)
                requestEnding (false);
            else
                cut (0.020);
        }
        else if (state == JamState::ending)
            cut (0.020);   // a second STOP during the ending
    };

    for (int n = toggleRequests.exchange (0, std::memory_order_acq_rel); n > 0; --n)
    {
        if (state == JamState::armed)
            doStart();
        else
            doStop();
    }

    for (int n = stopRequests.exchange (0, std::memory_order_acq_rel); n > 0; --n)
        doStop();

    if (startRequested.exchange (false, std::memory_order_acq_rel))
        doStart();

    // Taps, for Tap In (2.1): tempo and phase from four taps.
    const int written = tapWrite.load (std::memory_order_acquire);

    for (; tapRead < written; ++tapRead)
    {
        const auto t = tapQueue[(size_t) (tapRead % kTapQueue)].load (std::memory_order_relaxed);

        if (numTaps > 0 && (double) (t - taps[(size_t) (numTaps - 1)]) > kTapWindowSeconds * sr)
            numTaps = 0;

        if (numTaps == 4)
        {
            for (int i = 0; i < 3; ++i)
                taps[(size_t) i] = taps[(size_t) (i + 1)];

            numTaps = 3;
        }

        taps[(size_t) numTaps++] = t;

        if (numTaps == 4 && state == JamState::armed && mode == StartMode::tapIn)
        {
            const double interval = (double) (taps[3] - taps[0]) / 3.0;
            const double bpm = juce::jlimit (20.0, 300.0, 60.0 * sr / juce::jmax (1.0, interval));
            const double beat = 60.0 * sr / bpm;
            startBand (taps[3] + (int64_t) std::llround (beat), bpm, 0);
            numTaps = 0;
        }
    }

    if (fillRequested.exchange (false, std::memory_order_acq_rel))
        fillNowPending = true;
}

//==============================================================================
void JamEngine::startBand (int64_t beatOneSample, double bpm, int countInBars) noexcept
{
    clearQueue (true);
    numPendingChords = 0;
    predictor.reset();
    bassLine.reset();

    clockSource = JamStatus::Clock::own;
    currentBpm = juce::jlimit (20.0, 400.0, bpm);

    int num = 4, den = 4;

    if (status.meterNumerator > 0)
    {
        num = status.meterNumerator;
        den = status.meterDenominator;
    }

    const double barLength = jamclock::barLength (num, den);
    const double samplesPerQuarter = 60.0 * sr / currentBpm;

    own.refPpq = -countInBars * barLength;
    own.refSample = beatOneSample - (int64_t) std::llround (countInBars * barLength * samplesPerQuarter);
    own.samplesPerQuarter = samplesPerQuarter;
    mapping = own;

    cursor.valid = true;
    cursor.numerator = num;
    cursor.denominator = den;
    cursor.barLength = barLength;
    cursor.bar = -countInBars;
    cursor.barStart = own.refPpq;
    cursor.step = 0;
    cursor.stepsPerQuarter = activeStyle() != nullptr ? activeStyle()->stepsPerQuarter() : 4;
    cursor.stepsInBar = juce::jlimit (1, jam::kMaxSteps, (int) std::round (barLength * cursor.stepsPerQuarter));
    barBegun = false;
    midBarStart = false;

    bandStartSample = beatOneSample;
    firstGrooveBar = 0;
    lastNoteOnSample = beatOneSample;
    anticipatedUpTo = own.refPpq - 1.0e-6;
    endingPending = false;
    fillActive = false;
    fillNowPending = false;
    fillNowNextBar = -1;
    crashNextDownbeat = false;
    lastSection = -1;

    // Counting until beat one (a count-in, or a Tap In's beat after tap 4).
    if (beatOneSample > sampleClock || countInBars > 0)
    {
        state = JamState::counting;

        Event e;
        e.type = EventType::stateChange;
        e.value = (int16_t) JamState::playing;
        e.sample = beatOneSample + latency;
        e.musical = beatOneSample;
        e.priority = 0;
        queue (e);
    }
    else
    {
        state = JamState::playing;
    }

    // The tune's chord is known from the start.
    if (usingTuneSource())
        bassChord = chordMap->chordAt (0.0 + tuneOffset);
    else if (heardChord.isKnown())
        bassChord = heardChord;
}

void JamEngine::startWithExternalClock() noexcept
{
    clearQueue (true);
    numPendingChords = 0;
    predictor.reset();
    bassLine.reset();
    endingPending = false;
    fillActive = fillNowPending = crashNextDownbeat = false;
    fillNowNextBar = -1;
    lastSection = -1;

    locate();
    state = JamState::playing;
    bandStartSample = sampleClock;
    lastNoteOnSample = sampleClock;
    firstGrooveBar = cursor.bar;

    if (usingTuneSource())
        bassChord = chordMap->chordAt (mapping.ppqOf (sampleClock) + tuneOffset);
    else if (heardChord.isKnown())
        bassChord = heardChord;
}

void JamEngine::locate() noexcept
{
    // 11: a cycle jump or locate re-syncs at the new position: the pattern
    // step is recomputed, ringing drums decay naturally, the bass re-plucks.
    clearQueue (true);

    int num = status.meterNumerator > 0 ? status.meterNumerator : 4;
    int den = status.meterDenominator > 0 ? status.meterDenominator : 4;
    const double barLength = jamclock::barLength (num, den);
    const double ppq = mapping.ppqOf (sampleClock);

    double barStart = std::floor (ppq / barLength + 1.0e-9) * barLength;

    if (hostBarStartKnown)
    {
        barStart = hostBarStart;

        // A bar start from the previous bar (some hosts lag it): step on.
        while (barStart + barLength <= ppq + 1.0e-9)
            barStart += barLength;
    }

    cursor.valid = true;
    cursor.numerator = num;
    cursor.denominator = den;
    cursor.barLength = barLength;
    cursor.barStart = barStart;
    cursor.bar = (int64_t) std::floor (barStart / barLength + 0.5);
    cursor.stepsPerQuarter = activeStyle() != nullptr ? activeStyle()->stepsPerQuarter() : 4;
    cursor.stepsInBar = juce::jlimit (1, jam::kMaxSteps, (int) std::round (barLength * cursor.stepsPerQuarter));
    cursor.step = juce::jmax (0, (int) std::ceil ((ppq - barStart) / cursor.stepLength() - 1.0e-6));

    if (pendingJoinAtBar || cursor.step >= cursor.stepsInBar)
    {
        // START with the host playing joins at the next bar line (2.1).
        if (pendingJoinAtBar && cursor.step == 0 && std::abs (ppq - barStart) < 1.0e-6)
        {
            // already on the bar line
        }
        else
        {
            cursor.barStart += barLength;
            cursor.bar += 1;
            cursor.step = 0;
        }
    }

    pendingJoinAtBar = false;
    barBegun = false;
    midBarStart = cursor.step > 0;
    anticipatedUpTo = mapping.ppqOf (sampleClock) - 1.0e-6;
    numPendingChords = 0;

    // Nothing hangs: the bass is released and plucks again at the new place.
    if (bassSounding)
        releaseBass (sampleClock + latency, ppq);

    repluckPending = true;
}

void JamEngine::requestEnding (bool immediate) noexcept
{
    if (! cursor.valid)
    {
        goArmed();
        return;
    }

    if (immediate)
    {
        // The host stopped: the ending plays now, as a bar of its own.
        clearQueue (true);
        const double ppq = mapping.ppqOf (sampleClock);
        cursor.barStart = ppq;
        cursor.bar += 1;
        cursor.step = 0;
        barBegun = false;
        endingBar = cursor.bar;
    }
    else
    {
        // STOP: the ending plays on the next downbeat.
        endingBar = barBegun ? cursor.bar + 1 : cursor.bar;
    }

    endingPending = true;
    state = JamState::ending;
}

void JamEngine::cut (double seconds) noexcept
{
    kit.chokeAll (seconds);
    bass.choke (seconds);

    if (bassMidiNote >= 0)
        emitMidi (0, false, bassChannel.load (std::memory_order_relaxed), bassMidiNote, 0);

    bassMidiNote = -1;
    bassSounding = false;
    clearQueue (true);
    goArmed();
}

void JamEngine::goArmed() noexcept
{
    state = JamState::armed;
    cursor.valid = false;
    clockSource = JamStatus::Clock::own;
    endingPending = false;
    fillActive = false;
    fillNowPending = false;
    pendingJoinAtBar = false;
    numPendingChords = 0;
    lastBassStepMusical = INT64_MIN;
}

//==============================================================================
void JamEngine::updateClock (const BlockContext& ctx, int64_t blockStart, int numSamples) noexcept
{
    juce::ignoreUnused (numSamples);

    auto mode = (StartMode) settings.startMode;
    status.noPlayHead = (mode == StartMode::hostTransport && ! ctx.hasPlayHead);

    const bool hostRunning = ctx.hostHasPpq && ctx.hostPlaying;
    const bool tuneRunning = ctx.tuneRunning && ! ctx.tuneFollowingHost && ! hostRunning;
    const bool externalMode = (mode == StartMode::automatic || mode == StartMode::hostTransport) && ! status.noPlayHead;

    auto external = JamStatus::Clock::own;

    if (externalMode)
    {
        if (hostRunning)
            external = JamStatus::Clock::host;
        else if (tuneRunning && mode == StartMode::automatic)
            external = JamStatus::Clock::tune;
    }

    // Meter: the host's, else the tune's, else 4/4 (2.3).
    if (ctx.hostHasMeter && ctx.hostNumerator > 0 && ctx.hostDenominator > 0)
    {
        status.meterNumerator = ctx.hostNumerator;
        status.meterDenominator = ctx.hostDenominator;
    }
    else if (ctx.tuneRunning && ctx.tuneBeatsPerBar > 0.0)
    {
        status.meterNumerator = juce::jlimit (1, 16, (int) std::round (ctx.tuneBeatsPerBar));
        status.meterDenominator = 4;
    }
    else
    {
        status.meterNumerator = 4;
        status.meterDenominator = 4;
    }

    hostBarStartKnown = external == JamStatus::Clock::host && ctx.hostHasBarStart;
    hostBarStart = ctx.hostBarStartPpq;

    auto externalMapping = [&] (JamStatus::Clock c)
    {
        JamClockMapping m;
        m.refSample = blockStart;
        m.refPpq = c == JamStatus::Clock::host ? ctx.hostPpq : ctx.tunePpq;
        const double bpm = juce::jlimit (1.0, 1000.0, c == JamStatus::Clock::host ? ctx.hostBpm : ctx.tuneBpm);
        m.samplesPerQuarter = 60.0 * sr / bpm;
        return m;
    };

    const bool externalStarted = external != JamStatus::Clock::own && ! externalWasRunning;
    externalWasRunning = external != JamStatus::Clock::own;

    const bool running = state == JamState::counting || state == JamState::playing || state == JamState::ending;

    if (running && clockSource != JamStatus::Clock::own)
    {
        if (external == clockSource)
        {
            mapping = externalMapping (external);

            // A jump: a cycle, a locate, a seek.
            if (haveLastBlockEnd && std::abs (mapping.refPpq - lastBlockEndPpq) > 0.05)
                locate();
        }
        else
        {
            // The external clock stopped: the band carries on its own clock
            // from where it was, for the ending (or stops).
            own.refSample = blockStart;
            own.refPpq = haveLastBlockEnd ? lastBlockEndPpq : mapping.ppqOf (blockStart);
            own.samplesPerQuarter = mapping.samplesPerQuarter;
            mapping = own;
            clockSource = JamStatus::Clock::own;

            if (settings.ending && state != JamState::counting)
                requestEnding (true);
            else
                cut (0.020);
        }
    }
    else if (external != JamStatus::Clock::own && (state == JamState::armed || (running && pendingJoinAtBar)))
    {
        if (externalStarted || pendingJoinAtBar)
        {
            mapping = externalMapping (external);
            clockSource = external;
            startWithExternalClock();
        }
    }
    else if (pendingJoinAtBar && external == JamStatus::Clock::own)
    {
        pendingJoinAtBar = false;   // the host stopped before the bar line
    }

    if (clockSource == JamStatus::Clock::own)
        mapping = own;

    status.clock = clockSource;
    status.followingTune = ctx.tunePlaying || ctx.progressionPlaying;

    if ((ChordSource) settings.chordSource == ChordSource::live)
        status.followingTune = false;

    // The band's ppq to the tune's: equal when the tune is the clock.
    tuneOffset = (ctx.tuneRunning && clockSource != JamStatus::Clock::tune) ? ctx.tunePpq - mapping.ppqOf (blockStart) : 0.0;

    if (clockSource == JamStatus::Clock::host && ctx.tuneFollowingHost)
        tuneOffset = ctx.tunePpq - ctx.hostPpq;

    currentBpm = mapping.bpm (sr);
}

//==============================================================================
void JamEngine::handleMidi (const BlockContext& ctx, const juce::MidiBuffer& notes, int64_t blockStart) noexcept
{
    const auto mode = (StartMode) settings.startMode;
    const bool externalRunning = ctx.hostHasPpq && ctx.hostPlaying;
    const bool noteStarts = mode == StartMode::firstNote
                         || (mode == StartMode::automatic && ! externalRunning && ! ctx.tuneRunning);
    const bool rhythmOwnsChord = ctx.rhythmDriving;
    JamChordFollower::Change change;

    for (const auto metadata : notes)
    {
        const auto m = metadata.getMessage();
        const int64_t t = blockStart + metadata.samplePosition;

        while (follower.poll (t, change))
            if (! rhythmOwnsChord)
                handleLiveChange (change);

        if (m.isNoteOn())
        {
            // 2.1 First Note: velocity 20 and up, beat 1 on the note's sample.
            if (state == JamState::armed && noteStarts && m.getVelocity() >= 20)
            {
                const double tempo = ctx.effectiveTempo;
                startBand (t, tempo, 0);
            }

            follower.noteOn (m.getNoteNumber(), t);

            hits[(size_t) hitWrite] = { t, (int) m.getVelocity() };
            hitWrite = (hitWrite + 1) % (int) hits.size();
            hitCount = juce::jmin (hitCount + 1, (int) hits.size());
            lastNoteOnSample = t;
        }
        else if (m.isNoteOff())
        {
            follower.noteOff (m.getNoteNumber());
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            follower.allNotesOff();
        }
    }

    while (follower.poll (blockEnd - 1, change))
        if (! rhythmOwnsChord)
            handleLiveChange (change);

    // 3.1: while the rhythm engine drives, its chord is Jam's, every block.
    if (rhythmOwnsChord && JamChordFollower::isChord (ctx.rhythmChord))
    {
        if (ctx.rhythmChord != lastRhythmChord)
        {
            lastRhythmChord = ctx.rhythmChord;
            JamChordFollower::Change c;
            c.chord = ctx.rhythmChord;
            c.firstNoteSample = c.detectionSample = blockStart;
            handleLiveChange (c);
        }

        heardChord = ctx.rhythmChord;
    }
}

void JamEngine::handleLiveChange (const JamChordFollower::Change& change) noexcept
{
    if (! JamChordFollower::isChord (change.chord))
        return;   // 3.1: melody and Unknown change nothing

    heardChord = change.chord;

    if (usingTuneSource())
        return;

    const bool running = state == JamState::counting || state == JamState::playing || state == JamState::ending;

    if (! running || ! cursor.valid)
    {
        bassChord = change.chord;
        return;
    }

    const double firstPpq = mapping.ppqOf (change.firstNoteSample);
    const double detPpq = mapping.ppqOf (change.detectionSample);
    const double barLength = cursor.barLength;
    const auto barStartFor = [&] (double ppq)
    {
        return cursor.barStart + std::floor ((ppq - cursor.barStart) / barLength + 1.0e-9) * barLength;
    };

    const double q = jamclock::quantum (settings.follow, barLength);
    const double firstBar = barStartFor (firstPpq);
    const double quantised = jamclock::nearestBoundary (firstPpq, firstBar, q);

    // Prediction (3.3, Live only): history, and contradictions.
    if (settings.predict)
    {
        if (predictor.isPredicting())
        {
            // What the prediction has the band playing at the detection.
            ChordSymbol expected = bassChord;

            for (int i = 0; i < numPendingChords; ++i)
                if (! pendingChords[(size_t) i].applied && pendingChords[(size_t) i].musical <= change.detectionSample)
                    expected = pendingChords[(size_t) i].chord;

            if (expected == change.chord)
            {
                predictor.record (quantised, change.chord);
                return;   // already playing it
            }

            // A contradiction: corrected at the next Q, prediction off until
            // two more clean cycles.
            predictor.contradict (quantised, change.chord);

            int kept = 0;

            for (int i = 0; i < numPendingChords; ++i)
                if (pendingChords[(size_t) i].applied || ! pendingChords[(size_t) i].anticipated
                      || pendingChords[(size_t) i].musical <= change.detectionSample)
                    pendingChords[(size_t) kept++] = pendingChords[(size_t) i];

            numPendingChords = kept;
        }
        else
        {
            predictor.record (quantised, change.chord);
        }
    }

    // 3.2: the grace window - a change whose first note falls just after a
    // boundary lands at once, at the detection sample.
    const double grace = juce::jmin (0.090 * currentBpm / 60.0, 0.25);
    const double last = jamclock::lastBoundary (firstPpq, firstBar, q);
    int64_t effective = 0;
    double effectivePpq = 0.0;

    if (firstPpq - last <= grace + 1.0e-9)
    {
        effective = change.detectionSample;
        effectivePpq = detPpq;
    }
    else
    {
        effectivePpq = jamclock::nextBoundary (detPpq, barStartFor (detPpq), q);
        effective = mapping.sampleOf (effectivePpq);
    }

    addChordChange (effective, effectivePpq, change.chord, false);
}

void JamEngine::scheduleAnticipated (double upToPpq) noexcept
{
    if (! cursor.valid || ! (upToPpq > anticipatedUpTo))
        return;

    double from = anticipatedUpTo;

    if (usingTuneSource())
    {
        JamChordMap::Entry e;
        double at = 0.0;

        for (int guard = 0; guard < 64 && chordMap->nextChange (from + tuneOffset, upToPpq + tuneOffset, e, at); ++guard)
        {
            const double bandPpq = at - tuneOffset;
            addChordChange (mapping.sampleOf (bandPpq), bandPpq, e.toSymbol(), true);
            from = bandPpq;
        }
    }
    else if (settings.predict && predictor.isPredicting())
    {
        double at = 0.0;
        ChordSymbol c;

        for (int guard = 0; guard < 64 && predictor.predictNext (from, upToPpq, at, c); ++guard)
        {
            addChordChange (mapping.sampleOf (at), at, c, true);
            from = at;
        }
    }

    anticipatedUpTo = upToPpq;
}

//==============================================================================
void JamEngine::schedule (const BlockContext& ctx, int64_t horizon) noexcept
{
    juce::ignoreUnused (ctx);

    for (int guard = 0; guard < 4096 && cursor.valid; ++guard)
    {
        if (! barBegun)
        {
            beginBar();

            if (! cursor.valid)
                break;

            scheduleAnticipated (mapping.ppqOf (horizon));
        }

        const auto* style = activeStyle();
        const double swing = style != nullptr && style->grid == 16
                               ? juce::jlimit (0.0, 0.45, style->swing + settings.swing / 200.0) : 0.0;
        const double ppq = cursor.gridPpq (cursor.step) + jamclock::swingOffset (cursor.step, cursor.stepsPerQuarter, swing);

        if (mapping.sampleOf (ppq) >= horizon)
            break;

        stepPpq = ppq;
        processStep();

        if (! cursor.valid)
            break;

        if (++cursor.step >= cursor.stepsInBar)
        {
            cursor.step = 0;
            cursor.barStart += cursor.barLength;
            cursor.bar += 1;
            barBegun = false;
        }
    }
}

void JamEngine::beginBar() noexcept
{
    barBegun = true;

    // The meter can change at a bar line; a mid-bar start keeps its step.
    const int num = status.meterNumerator, den = status.meterDenominator;

    if (num != cursor.numerator || den != cursor.denominator)
    {
        cursor.numerator = num;
        cursor.denominator = den;
        cursor.barLength = jamclock::barLength (num, den);
    }

    // The band's own clock re-tempos at the bar line (2.3: a tap while it
    // plays on its own clock).
    if (clockSource == JamStatus::Clock::own && pendingOwnBpm > 0.0 && std::abs (pendingOwnBpm - own.bpm (sr)) > 1.0e-6
          && cursor.bar >= 0)
    {
        const int64_t barSample = own.sampleOf (cursor.barStart);
        own.refSample = barSample;
        own.refPpq = cursor.barStart;
        own.samplesPerQuarter = 60.0 * sr / pendingOwnBpm;
        mapping = own;
        currentBpm = pendingOwnBpm;
    }

    // 4.4: style, variation, kit and bass voice change at the bar line.
    latchedStyleIndex = juce::jlimit (0, jam::kNumStyleChoices - 1, settings.style);
    latchedStyle = getStyleSlot (latchedStyleIndex);

    if (latchedStyle == nullptr)
        latchedStyle = getStyleSlot (0);

    latchedVariation = juce::jlimit (0, 1, settings.variation);
    const auto* style = latchedStyle;

    if (style != nullptr)
    {
        kit.setKit (settings.kitAuto ? style->kit : settings.kit);
        latchedBassVoice = settings.bassVoice == 0 ? style->bassVoice : settings.bassVoice - 1;
        cursor.stepsPerQuarter = style->stepsPerQuarter();
    }
    else
    {
        kit.setKit (settings.kit);
        latchedBassVoice = juce::jmax (0, settings.bassVoice - 1);
    }

    bass.setVoice ((JamBassVoiceKind) juce::jlimit (0, 3, latchedBassVoice));

    const int newSteps = juce::jlimit (1, jam::kMaxSteps, (int) std::round (cursor.barLength * cursor.stepsPerQuarter));

    if (newSteps != cursor.stepsInBar)
    {
        cursor.step = juce::jmin (cursor.step, newSteps - 1);
        cursor.stepsInBar = newSteps;
    }

    genericGroove = style == nullptr || style->findMeter (cursor.numerator, cursor.denominator) == nullptr;

    if (genericGroove)
        buildGenericBar();

    for (int lane = 0; lane <= jam::numDrumLanes; ++lane)
        laneRandom[(size_t) lane].setSeed (laneSeed (cursor.bar, lane));

    // Humanise draws must not depend on where a mid-bar start began.
    for (int s = 0; s < cursor.step; ++s)
        for (int lane = 0; lane <= jam::numDrumLanes; ++lane)
        {
            laneRandom[(size_t) lane].nextGaussian();
            laneRandom[(size_t) lane].nextGaussian();
            laneRandom[(size_t) lane].nextDouble();
        }

    // The status lanes show the bar now being scheduled.
    for (auto& lane : status.lanes)
        lane.fill (0);

    status.laneHits.fill (0);
    fillActive = false;
    crashThisDownbeat = false;

    if (cursor.bar < 0)
        return;   // a count-in bar: sticks only

    // Intensity for the downbeat (a beat boundary).
    latchIntensity (cursor.gridPpq (0), mapping.sampleOf (cursor.gridPpq (0)));

    const int64_t barSample = mapping.sampleOf (cursor.barStart);

    // 2.2: the ending on this downbeat - asked for by STOP, or by silence.
    bool endingHere = endingPending && cursor.bar >= endingBar;

    if (! endingHere && state == JamState::playing && clockSource == JamStatus::Clock::own
          && settings.stopOnSilence && settings.startMode != (int) StartMode::hostTransport)
    {
        const double silentFrom = cursor.barStart - settings.silenceBars * cursor.barLength;

        if (silentFrom >= -1.0e-9)
        {
            const int64_t from = mapping.sampleOf (silentFrom), to = barSample - lookahead;
            bool heard = false;

            for (int i = 0; i < hitCount && ! heard; ++i)
            {
                const auto& h = hits[(size_t) ((hitWrite - 1 - i + (int) hits.size()) % (int) hits.size())];
                heard = h.sample >= from && h.sample < to;
            }

            if (! heard && lastNoteOnSample < to)
                heard = lastNoteOnSample >= from;

            if (! heard)
            {
                if (settings.ending)
                {
                    endingHere = true;
                    endingBar = cursor.bar;
                    state = JamState::ending;
                }
                else
                {
                    Event e;
                    e.type = EventType::endingDone;
                    e.sample = barSample + latency;
                    e.musical = barSample;
                    queue (e);
                    cursor.valid = false;
                    return;
                }
            }
        }
    }

    if (endingHere)
    {
        endingPending = false;
        endingBar = cursor.bar;

        Event done;
        done.type = EventType::endingDone;
        done.musical = mapping.sampleOf (cursor.barEnd());
        done.sample = done.musical + latency;
        queue (done);
        return;
    }

    // 3.3: prediction checks for a cycle at every bar line (Live only).
    if (settings.predict && ! usingTuneSource())
        predictor.onBarLine (cursor.barStart, cursor.barLength);

    // 11: a tune's section brings its jam hint; a section change is filled into.
    bool fillHere = false;

    if (usingTuneSource())
    {
        const int here = chordMap->findIndexAt (cursor.barStart + tuneOffset);
        const int next = chordMap->findIndexAt (cursor.barEnd() + tuneOffset);
        const int section = here >= 0 ? chordMap->entries[(size_t) here].sectionIndex : -1;
        const int nextSection = next >= 0 ? chordMap->entries[(size_t) next].sectionIndex : -1;

        if (section != lastSection)
        {
            lastSection = section;
            hintIntensity = juce::isPositiveAndBelow (section, JamChordMap::kMaxSections)
                              ? chordMap->hints[(size_t) section].intensity : 0;
        }

        if (nextSection != section && juce::isPositiveAndBelow (nextSection, JamChordMap::kMaxSections)
              && chordMap->hints[(size_t) nextSection].fillInto)
            fillHere = true;
    }
    else
    {
        hintIntensity = 0;
    }

    // 4.3: the fill period, and Fill Now's "next bar".
    int period = kFillEveryBars[juce::jlimit (0, 4, settings.fillEvery)];

    if (period > 0 && effectiveIntensity >= 5)
        period = juce::jmax (1, period / 2);

    const int64_t grooveBar = cursor.bar;

    if (period > 0 && grooveBar >= 0 && ((grooveBar + 1) % period) == 0)
        fillHere = true;

    const auto* meter = activeMeter();
    const int spq = cursor.stepsPerQuarter;
    const int beats = cursor.stepsInBar / spq;

    auto chooseFill = [&] (int wantBeats, int startStep, bool anySize)
    {
        if (meter == nullptr || meter->numFills == 0)
            return;

        RtRandom draw (laneSeed (cursor.bar, 99));
        int candidates[jam::kMaxFills] {};
        int n = 0;

        for (int f = 0; f < meter->numFills; ++f)
            if (meter->fills[(size_t) f].beats == wantBeats || (anySize && meter->fills[(size_t) f].beats >= wantBeats))
                candidates[n++] = f;

        if (n == 0)
            return;

        const auto& fill = meter->fills[(size_t) candidates[draw.nextInt (n)]];
        fillPattern = &fill.pattern;
        fillStartStep = startStep;
        fillPatternOffset = juce::jmax (0, fill.pattern.steps - (cursor.stepsInBar - startStep));
        fillActive = true;
    };

    if (fillNowNextBar == cursor.bar)
    {
        // Fill Now with less than a beat left: the last 2 beats of this bar.
        const int twoBeats = juce::jmin (beats, 2);
        chooseFill (twoBeats, cursor.stepsInBar - twoBeats * spq, false);
        fillNowNextBar = -1;
    }
    else if (fillHere && ! genericGroove)
    {
        // The fill grows with intensity: 1 beat at 1-2, 2 beats at 3, a bar at 4-5.
        const int size = effectiveIntensity <= 2 ? 1 : effectiveIntensity == 3 ? 2 : beats;
        chooseFill (juce::jmin (size, beats), cursor.stepsInBar - juce::jmin (size, beats) * spq, false);
    }

    // A crash on the downbeat after a fill (intensity 3 and up), and every 4
    // bars at intensity 5.
    crashThisDownbeat = crashNextDownbeat || (effectiveIntensity >= 5 && (cursor.bar % 4) == 0)
                     || (cursor.bar == firstGrooveBar && ! midBarStart);   // the band comes in with a crash (JM-14)
    crashNextDownbeat = fillActive && effectiveIntensity >= 3;

    // The chord track: this bar's chord at its start.
    for (size_t i = 0; i + 1 < barChordHistory.size(); ++i)
        barChordHistory[i] = barChordHistory[i + 1];

    barChordHistory.back() = bassChord;

    // A style marker for the Luthier export profile (9).
    if (latchedStyleIndex != markerStyle || latchedVariation != markerVariation
          || effectiveIntensity != markerIntensity || kit.getKit() != markerKit)
    {
        markerStyle = latchedStyleIndex;
        markerVariation = latchedVariation;
        markerIntensity = effectiveIntensity;
        markerKit = kit.getKit();

        JamCaptureEvent marker;
        marker.sample = barSample + latency;
        marker.ppq = cursor.barStart;
        marker.bpm = currentBpm;
        marker.barLengthQuarters = (int) std::round (cursor.barLength);
        marker.part = 2;
        marker.style = (int16_t) markerStyle;
        marker.variation = (int16_t) markerVariation;
        marker.intensity = (int16_t) markerIntensity;
        marker.kit = (int16_t) markerKit;
        capture.add (marker);
    }
}

void JamEngine::latchIntensity (double ppq, int64_t musical) noexcept
{
    const int base = hintIntensity > 0 ? hintIntensity : juce::jlimit (1, 5, settings.intensity);
    baseIntensity = base;

    if (settings.dynamicsFollow)
    {
        // 4.2: the mean note-on velocity over the last 2 bars, with 8 points
        // of hysteresis around 55 and 105.
        const int64_t from = mapping.sampleOf (ppq - 2.0 * cursor.barLength);
        const int64_t to = musical - lookahead;
        double sum = 0.0;
        int n = 0;

        for (int i = 0; i < hitCount; ++i)
        {
            const auto& h = hits[(size_t) i];

            if (h.sample >= from && h.sample < to)
            {
                sum += h.velocity;
                ++n;
            }
        }

        if (n > 0)
        {
            const double mean = sum / n;

            if (dynamicsOffset < 0)       { if (mean > 63.0) dynamicsOffset = mean > 105.0 ? 1 : 0; }
            else if (dynamicsOffset > 0)  { if (mean < 97.0) dynamicsOffset = mean < 55.0 ? -1 : 0; }
            else if (mean < 55.0)         dynamicsOffset = -1;
            else if (mean > 105.0)        dynamicsOffset = 1;
        }
    }
    else
    {
        dynamicsOffset = 0;
    }

    effectiveIntensity = juce::jlimit (1, 5, base + dynamicsOffset);
}

void JamEngine::processStep() noexcept
{
    const int s = cursor.step;
    const double ppq = stepPpq;
    const int64_t musical = mapping.sampleOf (ppq);
    const double h = juce::jlimit (0.0, 1.0, settings.humanise / 100.0);

    status.step = s;

    // 4.4: intensity at the next beat.
    if (cursor.isBeat (s) && s > 0 && cursor.bar >= 0)
        latchIntensity (ppq, musical);

    // Draws for every lane every step, hit or not, so the sequence is fixed.
    std::array<double, jam::numDrumLanes + 1> timeDraw {}, velocityDraw {}, coin {};

    for (int lane = 0; lane <= jam::numDrumLanes; ++lane)
    {
        timeDraw[(size_t) lane] = laneRandom[(size_t) lane].nextGaussian();
        velocityDraw[(size_t) lane] = laneRandom[(size_t) lane].nextGaussian();
        coin[(size_t) lane] = laneRandom[(size_t) lane].nextDouble();
    }

    auto humanTime = [&] (int lane, bool downbeatKick)
    {
        const double sigma = lane == kBassLane ? 0.008 : 0.006;
        double offset = h * (sigma * timeDraw[(size_t) lane] + lanePush (lane));
        offset = juce::jlimit (-0.025, 0.025, offset);

        if (downbeatKick)
            offset = juce::jlimit (-0.002, 0.002, offset);

        return (int64_t) std::llround (offset * sr);
    };

    auto humanVelocity = [&] (int lane, double v)
    {
        return juce::jlimit (0.02, 1.0, v * (1.0 + 0.08 * h * velocityDraw[(size_t) lane]));
    };

    // A count-in bar: the band's sticks on each beat.
    if (cursor.bar < 0)
    {
        if (cursor.isBeat (s))
        {
            Event e;
            e.type = EventType::drum;
            e.value = (int16_t) DrumSound::sticks;
            e.velocity = s == 0 ? 1.0f : 0.8f;
            e.musical = musical;
            e.sample = musical + latency;
            e.ppq = ppq;
            e.priority = 2;
            queue (e);
        }

        return;
    }

    const auto& pattern = patternFor (s);
    const bool inFill = fillActive && s >= fillStartStep && fillPattern != nullptr;

    for (int lane = 0; lane < jam::numDrumLanes; ++lane)
    {
        char token = inFill ? fillPattern->drum (lane, s - fillStartStep + fillPatternOffset) : pattern.drum (lane, s);

        if (lane == jam::crash && s == 0 && crashThisDownbeat && token == '.')
            token = 'X';

        DrumSound sound = DrumSound::kick;
        int velocity = 0;

        if (! drumFor (lane, token, coin[(size_t) lane], sound, velocity))
            continue;

        Event e;
        e.type = EventType::drum;
        e.value = (int16_t) sound;
        e.velocity = (float) humanVelocity (lane, velocity / 127.0);
        e.musical = musical;
        e.sample = musical + humanTime (lane, lane == jam::kick && s == 0) + latency;
        e.ppq = ppq;
        e.priority = 2;
        queue (e);

        if (s < JamStatus::kLaneSteps)
        {
            status.lanes[(size_t) lane][(size_t) s] = (uint8_t) juce::jlimit (1, 127, (int) (e.velocity * 127.0f));
            status.laneHits[(size_t) lane] |= (1u << s);
        }
    }

    // The bass lane: the groove's line even under a fill.
    char token = pattern.bassAt (s);

    if (repluckPending)
    {
        if (token == '-')
            token = 'R';

        repluckPending = false;
    }

    if (token != '-')
    {
        Event e;
        e.type = EventType::bassStep;
        e.value = (int16_t) token;
        e.velocity = (float) humanVelocity (kBassLane, token == 'm' ? 0.4 : 0.8);
        e.musical = musical;
        e.sample = musical + humanTime (kBassLane, false) + latency;
        e.ppq = ppq;
        e.walkLeft = (int16_t) walkStepsLeft (pattern, s);
        e.priority = 1;
        queue (e);

        if (token != '.' && s < JamStatus::kLaneSteps)
        {
            status.lanes[(size_t) kBassLane][(size_t) s] = (uint8_t) juce::jlimit (1, 127, (int) (e.velocity * 127.0f));
            status.laneHits[(size_t) kBassLane] |= (1u << s);
        }
    }
}

//==============================================================================
void JamEngine::emitMidi (int offset, bool noteOn, int channel, int note, int velocity) noexcept
{
    const int position = juce::jmax (0, offset);

    if (noteOn)
        midiOut.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) juce::jlimit (1, 127, velocity)), position);
    else
        midiOut.addEvent (juce::MidiMessage::noteOff (channel, note), position);
}

void JamEngine::noteCapture (int64_t sample, double ppq, int part, int note, int velocity) noexcept
{
    JamCaptureEvent e;
    e.sample = sample;
    e.ppq = ppq;
    e.bpm = currentBpm;
    e.barLengthQuarters = (int) std::round (cursor.barLength);
    e.part = (int8_t) part;
    e.note = (uint8_t) juce::jlimit (0, 127, note);
    e.velocity = (uint8_t) juce::jlimit (0, 127, velocity);
    capture.add (e);
}

void JamEngine::playBassNote (int note, double velocity, bool ghost, int64_t sample, double ppq) noexcept
{
    // 12: the bar's notes, for the lane view's description.
    {
        const int64_t bar = cursor.valid ? cursor.bar : 0;

        if (bar != barBassNotesBar)
        {
            barBassNotesBar = bar;
            numBarBassNotes = 0;
        }

        if (numBarBassNotes < (int) barBassNotes.size())
            barBassNotes[(size_t) numBarBassNotes++] = (int8_t) juce::jlimit (0, 127, note);
    }

    const int channel = bassChannel.load (std::memory_order_relaxed);
    const int offset = (int) (sample - sampleClock);

    if (bassMidiNote >= 0)
    {
        emitMidi (offset, false, channel, bassMidiNote, 0);
        noteCapture (sample, ppq, 1, bassMidiNote, 0);
    }

    bass.noteOn (note, velocity, ghost);
    const int v = juce::jlimit (1, 127, (int) std::round (velocity * 127.0));
    emitMidi (offset, true, channel, note, v);
    noteCapture (sample, ppq, 1, note, v);

    bassMidiNote = note;
    bassSounding = true;
}

void JamEngine::releaseBass (int64_t sample, double ppq) noexcept
{
    bass.noteOff();

    if (bassMidiNote >= 0)
    {
        emitMidi ((int) (sample - sampleClock), false, bassChannel.load (std::memory_order_relaxed), bassMidiNote, 0);
        noteCapture (sample, ppq, 1, bassMidiNote, 0);
    }

    bassMidiNote = -1;
    bassSounding = false;
}

void JamEngine::fire (const Event& e, int64_t now) noexcept
{
    switch (e.type)
    {
        case EventType::drum:
        {
            const auto sound = (DrumSound) e.value;
            kit.trigger (sound, e.velocity);

            const int note = getGmNote (sound);
            const int v = juce::jlimit (1, 127, (int) std::round (e.velocity * 127.0f));
            const int channel = drumChannel.load (std::memory_order_relaxed);
            emitMidi ((int) (now - sampleClock), true, channel, note, v);
            noteCapture (now, e.ppq, 0, note, v);

            Event off;
            off.type = EventType::drumOff;
            off.note = (int16_t) note;
            off.sample = now + (int64_t) (0.030 * sr);
            off.musical = off.sample;
            off.ppq = e.ppq + 0.030 * currentBpm / 60.0;
            queue (off);
            break;
        }

        case EventType::drumOff:
            emitMidi ((int) (now - sampleClock), false, drumChannel.load (std::memory_order_relaxed), e.note, 0);
            noteCapture (now, e.ppq, 0, e.note, 0);
            break;

        case EventType::bassStep:
        {
            const char token = (char) e.value;

            if (token == 'N')
            {
                // The tune's bass line, through the Jam bass (11).
                if (e.velocity > 0.0f)
                    playBassNote (e.note, e.velocity, false, now, e.ppq);
                else if (bassMidiNote == e.note)
                    releaseBass (now, e.ppq);
                break;
            }

            applyChordsUpTo (e.musical);
            lastBassStepMusical = e.musical;

            if (tuneBassPlaying.load (std::memory_order_relaxed) || playerIsBass)
                break;   // the tune's line, or the player, is the bassist

            if (! bassChord.isKnown())
                break;   // 3.1: before the first chord the bass rests

            if (token == '.')
            {
                releaseBass (now, e.ppq);
                break;
            }

            bool found = false;
            const double within = token == 'A' ? e.ppq + 0.5 : (cursor.barStart + cursor.barLength * 2.0);
            const auto next = (token == 'A' || token == 'W') ? nextKnownChord (e.ppq, within, found) : ChordSymbol {};
            const int note = bassLine.resolve (token, bassChord, found ? next : ChordSymbol {}, e.walkLeft);

            if (note >= 0)
                playBassNote (note, e.velocity, token == 'm', now, e.ppq);
            break;
        }

        case EventType::bassOff:
            releaseBass (now, e.ppq);
            break;

        case EventType::stateChange:
            if (state == JamState::counting)
                state = (JamState) e.value;
            break;

        case EventType::endingDone:
            // The ending's cymbals choke, the bass damps, and the band is Armed.
            kit.chokeCymbals (0.08);

            if (bassSounding)
                releaseBass (now, e.ppq);

            clearQueue (true);
            goArmed();
            break;

        case EventType::cymbalChoke:
            kit.chokeCymbals (0.05);
            break;
    }
}

void JamEngine::render (const BlockContext& ctx, const juce::MidiBuffer* tuneBass, int64_t blockStart, int numSamples) noexcept
{
    playerIsBass = ctx.playerIsBass;
    tuneBassPlaying.store (ctx.tuneBassActive, std::memory_order_relaxed);

    // The tune's bass notes, at their samples plus L - from the block the band
    // starts in, so a tune's first note is not lost to the start.
    if (tuneBass != nullptr && ctx.tuneBassActive && ! ctx.playerIsBass
          && state != JamState::armed && state != JamState::off)
    {
        for (const auto metadata : *tuneBass)
        {
            const auto m = metadata.getMessage();

            if (! m.isNoteOnOrOff())
                continue;

            Event e;
            e.type = EventType::bassStep;
            e.value = 'N';
            e.note = (int16_t) m.getNoteNumber();
            e.velocity = m.isNoteOn() ? (float) (m.getVelocity() / 127.0) : 0.0f;
            e.musical = blockStart + metadata.samplePosition;
            e.sample = e.musical + latency;
            e.ppq = mapping.ppqOf (e.musical);
            e.priority = m.isNoteOn() ? 1 : 0;
            queue (e);
        }
    }

    double* dl = drumsBuffer.getWritePointer (0);
    double* dr = drumsBuffer.getWritePointer (1);
    double* bs = bassBuffer.getWritePointer (0);

    const bool running = state != JamState::armed && state != JamState::off;
    const bool idle = ! running && numEvents == 0 && kit.isSilent() && ! bass.isSounding()
                        && bass.getLastPeak() < 1.0e-6;
    renderedSomething = ! idle;

    if (idle)
    {
        // 0.7: armed and stopped, only the conductor runs.
        std::fill (dl, dl + numSamples, 0.0);
        std::fill (dr, dr + numSamples, 0.0);
        std::fill (bs, bs + numSamples, 0.0);
        return;
    }

    int64_t pos = blockStart;
    const int64_t end = blockStart + numSamples;

    while (pos < end)
    {
        // Chord changes due now (their musical sample plus L).
        for (int i = 0; i < numPendingChords; ++i)
        {
            auto& p = pendingChords[(size_t) i];

            if (p.applied || p.musical + latency > pos)
                continue;

            p.applied = true;
            const bool changed = p.chord != bassChord;
            bassChord = p.chord;

            // Between the bass's own steps, the bassist moves to the new chord.
            bool stepHere = false;

            for (int k = 0; k < numEvents && ! stepHere; ++k)
                stepHere = events[(size_t) k].type == EventType::bassStep && events[(size_t) k].musical == p.musical;

            if (changed && bassSounding && lastBassStepMusical < p.musical && ! stepHere
                  && (state == JamState::playing || state == JamState::ending)
                  && ! tuneBassPlaying.load (std::memory_order_relaxed) && ! playerIsBass)
            {
                const int note = bassLine.resolve ('R', bassChord, {}, 0);

                if (note >= 0)
                    playBassNote (note, 0.8, false, pos, p.ppq);
            }
        }

        {
            int kept = 0;

            for (int i = 0; i < numPendingChords; ++i)
                if (! pendingChords[(size_t) i].applied)
                    pendingChords[(size_t) kept++] = pendingChords[(size_t) i];

            numPendingChords = kept;
        }

        // Events due now, in order (one fired may queue another, e.g. an off).
        while (numEvents > 0 && events[0].sample <= pos)
        {
            const Event e = events[0];

            for (int i = 1; i < numEvents; ++i)
                events[(size_t) (i - 1)] = events[(size_t) i];

            --numEvents;
            fire (e, pos);
        }

        // The next thing that happens.
        int64_t next = end;

        if (numEvents > 0)
            next = juce::jmin (next, juce::jmax (pos + 1, events[0].sample));

        for (int i = 0; i < numPendingChords; ++i)
            next = juce::jmin (next, juce::jmax (pos + 1, pendingChords[(size_t) i].musical + latency));

        const int offset = (int) (pos - blockStart);
        const int count = (int) (next - pos);

        kit.render (dl + offset, dr + offset, count);
        bass.render (bs + offset, count);
        pos = next;
    }
}

//==============================================================================
void JamEngine::mix (int numSamples) noexcept
{
    const bool running = isBandRunning() || renderedSomething;
    const double volume = settings.volumeDb <= -59.9 ? 0.0 : dbToGain (settings.volumeDb);

    // 7: an equal-power crossfade, -1 drums only, +1 bass only.
    const double theta = (juce::jlimit (-1.0, 1.0, settings.balance) + 1.0) * constants::kPi * 0.25;
    const double drumsBalance = juce::jmin (1.0, std::sqrt (2.0) * std::cos (theta));
    const double bassBalance = juce::jmin (1.0, std::sqrt (2.0) * std::sin (theta));

    const double dp = juce::jlimit (-1.0, 1.0, settings.drumsPan);
    const double bp = juce::jlimit (-1.0, 1.0, settings.bassPan);
    const double bassAngle = (bp + 1.0) * constants::kPi * 0.25;

    /*  The stems' calibration: at the default -6 dB the kit peaks near -6 dBFS
        (about -20 dBFS RMS) and the bass sits a few dB under it, so the band
        comes in at the level of a guitar through the rig rather than over it.
        Measured across the ten styles at intensity 3: the kit's summed pieces
        peak near +13 dBFS before this trim, the bass voice near -14 dBFS. */
    constexpr double kDrumsTrim = 0.2239;   // -13 dB
    constexpr double kBassTrim = 3.1623;    // +10 dB

    const double drums = settings.drumsMute ? 0.0 : volume * drumsBalance * kDrumsTrim;
    const double bassLevel = (settings.bassMute || playerIsBass) ? 0.0 : volume * bassBalance * kBassTrim;

    drumsGainL.setTarget (drums * (dp > 0.0 ? 1.0 - dp : 1.0));
    drumsGainR.setTarget (drums * (dp < 0.0 ? 1.0 + dp : 1.0));
    bassGainL.setTarget (bassLevel * std::cos (bassAngle));
    bassGainR.setTarget (bassLevel * std::sin (bassAngle));

    // Armed and silent: nothing to mix (0.7).
    if (! renderedSomething && ! isBandRunning())
    {
        for (auto* b : { &drumsOut, &bassOut })
            b->clear (0, numSamples);

        status.drumsPeak = status.bassPeak = 0.0;
        drumsAudible.store (false, std::memory_order_relaxed);
        return;
    }

    const double* dl = drumsBuffer.getReadPointer (0);
    const double* dr = drumsBuffer.getReadPointer (1);
    const double* bs = bassBuffer.getReadPointer (0);
    float* oL = drumsOut.getWritePointer (0);
    float* oR = drumsOut.getWritePointer (1);
    float* bL = bassOut.getWritePointer (0);
    float* bR = bassOut.getWritePointer (1);

    double drumsPeak = 0.0, bassPeak = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        // Each stem's own DC blocker (engine.md 0.3), after everything recursive.
        const double dL = stemDc[0].process (dl[i]), dR = stemDc[1].process (dr[i]), b = stemDc[2].process (bs[i]);
        const double l = dL * drumsGainL.next(), r = dR * drumsGainR.next();
        const double bl = b * bassGainL.next(), br = b * bassGainR.next();
        oL[i] = (float) l;
        oR[i] = (float) r;
        bL[i] = (float) bl;
        bR[i] = (float) br;
        drumsPeak = juce::jmax (drumsPeak, std::abs (l), std::abs (r));
        bassPeak = juce::jmax (bassPeak, std::abs (bl), std::abs (br));
    }

    status.drumsPeak = drumsPeak;
    status.bassPeak = bassPeak;

    drumsAudible.store (running && isBandRunning() && drums > 0.0, std::memory_order_relaxed);
}

void JamEngine::publishStatus (const BlockContext& ctx) noexcept
{
    status.state = state;
    status.bar = cursor.valid ? cursor.bar : 0;
    status.bassNotes = barBassNotes;
    status.numBassNotes = barBassNotesBar == status.bar ? numBarBassNotes : 0;
    status.stepsPerBeat = cursor.stepsPerQuarter;
    status.stepsInBar = cursor.stepsInBar;

    if (cursor.valid)
    {
        // Where the band is now (not where the scheduler is).
        const double now = mapping.ppqOf (sampleClock);
        const double inBar = now - (cursor.barStart - (cursor.step == 0 && ! barBegun ? cursor.barLength : 0.0));
        status.beat = juce::jlimit (0, 15, (int) std::floor (juce::jmax (0.0, inBar)));
    }

    status.baseIntensity = baseIntensity;
    status.effectiveIntensity = effectiveIntensity;
    status.style = latchedStyleIndex;
    status.variation = latchedVariation;
    status.kit = kit.getKit();
    status.bassVoice = latchedBassVoice;
    status.bpm = currentBpm;
    status.currentChord = usingTuneSource() && (bassChord.isKnown()) ? bassChord : (heardChord.isKnown() ? heardChord : bassChord);
    status.waitingForChord = ! bassChord.isKnown() && ! heardChord.isKnown();
    status.predicting = settings.predict && predictor.isPredicting() && ! usingTuneSource();
    status.fillActive = fillActive;
    status.genericGroove = genericGroove;
    status.bassResting = ctx.playerIsBass;
    status.tuneBassPlaying = ctx.tuneBassActive;

    bool found = false;
    const double now = mapping.ppqOf (sampleClock);
    status.nextChord = nextKnownChord (now, now + cursor.barLength * 2.0, found);
    status.nextSource = ! found ? JamStatus::NextSource::none
                      : usingTuneSource() ? JamStatus::NextSource::tune : JamStatus::NextSource::predicted;

    // The chord track: 2 bars back, this bar, 2 ahead.
    for (int i = 0; i < 3; ++i)
    {
        status.chordTrack[(size_t) i] = barChordHistory[barChordHistory.size() - 3 + (size_t) i];
        status.chordTrackSource[(size_t) i] = 0;
    }

    for (int i = 1; i <= 2; ++i)
    {
        bool f = false;
        const double barAt = cursor.barStart + (i - (barBegun ? 0 : 1)) * cursor.barLength;
        const auto c = nextKnownChord (barAt - 1.0e-6, barAt + 1.0e-6, f);
        status.chordTrack[(size_t) (2 + i)] = f ? c : ChordSymbol {};
        status.chordTrackSource[(size_t) (2 + i)] = (uint8_t) (f ? (usingTuneSource() ? 1 : 2) : 0);
    }

    stateAtomic.store ((int) (settings.enabled ? state : JamState::off), std::memory_order_relaxed);
    statusChannel.publish (status);
}

//==============================================================================
void JamEngine::process (const BlockContext& ctx, const juce::MidiBuffer& notes,
                         const juce::MidiBuffer* tuneBass, int numSamples) noexcept
{
    numSamples = juce::jmin (numSamples, maxBlock);
    applySettingsEdges();

    const int64_t blockStart = sampleClock;
    blockEnd = blockStart + numSamples;
    latency = juce::jmax (0, ctx.latency);
    midiOut.clear();

    // A new chord map, by pointer swap; the old one goes back to the message
    // thread to be freed.
    if (auto* incoming = chordMapIncoming.load (std::memory_order_acquire))
    {
        if (chordMapRetired.load (std::memory_order_acquire) == nullptr)
        {
            chordMapIncoming.store (nullptr, std::memory_order_release);
            chordMapRetired.store (chordMap, std::memory_order_release);
            chordMap = incoming;
            anticipatedUpTo = cursor.valid ? mapping.ppqOf (blockStart) - 1.0e-6 : anticipatedUpTo;

            // Anticipated changes from the old map go.
            int kept = 0;

            for (int i = 0; i < numPendingChords; ++i)
                if (! pendingChords[(size_t) i].anticipated)
                    pendingChords[(size_t) kept++] = pendingChords[(size_t) i];

            numPendingChords = kept;
        }
    }

    // Kit and tone settings: only a change costs anything (0.7).
    kit.setReduced (reducedCymbals.load (std::memory_order_relaxed));
    kit.setSeed (seedValue.load (std::memory_order_relaxed));

    if (settings.kitTuning != appliedTuning || settings.kitDamping != appliedDamping)
    {
        appliedTuning = settings.kitTuning;
        appliedDamping = settings.kitDamping;
        kit.setTuning (settings.kitTuning, settings.kitDamping / 100.0);
    }

    kit.setRoom (settings.kitRoom / 100.0);
    kit.setWidth (settings.kitWidth / 100.0);
    kit.setPerspective (settings.perspective == 1);

    if (settings.bassTone != appliedTone)
    {
        appliedTone = settings.bassTone;
        bass.setTone (settings.bassTone);
    }

    lookahead = (int64_t) std::llround (juce::jlimit (0.0, 1.0, settings.humanise / 100.0) * 0.025 * sr) + 1;

    handleCommands (ctx, blockStart);
    updateClock (ctx, blockStart, numSamples);

    // The own clock's tempo source (2.3): the tune's while it plays with the
    // host stopped, else tap / last host / 120.
    pendingOwnBpm = (ctx.tuneRunning && ! ctx.hostPlaying) ? ctx.tuneBpm : ctx.effectiveTempo;

    handleMidi (ctx, notes, blockStart);

    if (cursor.valid && (state == JamState::counting || state == JamState::playing || state == JamState::ending))
    {
        // Fill Now (4.3): from the next beat to the bar line, or the last two
        // beats of the next bar when less than a beat is left.
        if (fillNowPending)
        {
            fillNowPending = false;

            if (cursor.bar >= 0)
            {
                const double now = mapping.ppqOf (blockStart);
                const double barEnd = barBegun ? cursor.barEnd() : cursor.barStart;
                const int spq = cursor.stepsPerQuarter;

                if (barBegun && barEnd - now > 1.0 + 1.0e-9)
                {
                    const int nextBeatStep = ((cursor.step + spq - 1) / spq) * spq;
                    const auto* meter = activeMeter();

                    if (meter != nullptr && nextBeatStep < cursor.stepsInBar)
                    {
                        // The bar fill's tail from the next beat.
                        const int beats = cursor.stepsInBar / spq;

                        for (int f = 0; f < meter->numFills; ++f)
                            if (meter->fills[(size_t) f].beats == beats && ! meter->fills[(size_t) f].big)
                            {
                                fillPattern = &meter->fills[(size_t) f].pattern;
                                fillStartStep = nextBeatStep;
                                fillPatternOffset = juce::jmax (0, fillPattern->steps - (cursor.stepsInBar - nextBeatStep));
                                fillActive = true;
                                crashNextDownbeat = effectiveIntensity >= 3;
                                break;
                            }
                    }
                }
                else
                {
                    fillNowNextBar = barBegun ? cursor.bar + 1 : cursor.bar + 1;
                }
            }
        }

        scheduleAnticipated (mapping.ppqOf (blockEnd + lookahead));
        schedule (ctx, blockEnd + lookahead);
    }
    else
    {
        fillNowPending = false;
    }

    render (ctx, tuneBass, blockStart, numSamples);
    mix (numSamples);
    publishStatus (ctx);

    lastBlockEndPpq = mapping.ppqOf (blockEnd);
    haveLastBlockEnd = clockSource != JamStatus::Clock::own;
    sampleClock = blockEnd;
    sampleClockAtomic.store (sampleClock, std::memory_order_release);
}

} // namespace luthier
