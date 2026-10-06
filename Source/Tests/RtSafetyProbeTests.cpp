/*  docs/review/CODEX_RTSAFETY.md: the triggers the forty-second playback
    fixture in AudioThreadSafetyTests.cpp does not reach, each run with both
    probes armed - the per-thread allocation counter (CircuitTests.cpp) and
    the blocking-lock trap (ThreadProbe, with pthread_mutex_lock interposed
    on Linux by AudioThreadSafetyTests.cpp).

    P0  a live part swap taken at a block boundary;
    P0  active rhythm strumming while the editor changes humanisation;
    P1  the hidden effect switched on mid-session, oversized host blocks with
        dense MIDI and SysEx, and the bounded MIDI copy's overflow policy;
    P2  NaN / infinite / non-positive host tempo, position and sample rate;
    beta  ReverbPedal Size / Character changes on the audio thread. */

#include "TestFramework.h"

#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../Model/Workshop/PartAcoustics.h"
#include "../Rhythm/RhythmEngine.h"
#include "../Rhythm/Patterns.h"
#include "../DSP/Effects/PedalsMod.h"
#include "../Support/BoundedMidi.h"
#include "../Support/HostClockGuard.h"
#include "../Support/ThreadProbe.h"

#include <thread>

namespace luthier::tests
{
    long allocationsOnThisThread() noexcept;
}

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;

    struct AudioThreadScope
    {
        AudioThreadScope()  { ThreadProbe::markAsAudioThread (true); }
        ~AudioThreadScope() { ThreadProbe::markAsAudioThread (false); }
    };

    /** Allocations and blocking locks seen on this thread across @p fn. */
    struct Probe
    {
        long allocations = 0;
        int locks = 0;

        template <typename Fn>
        void run (Fn&& fn)
        {
            const auto before = allocationsOnThisThread();
            const int locksBefore = ThreadProbe::audioThreadLocks.load();

            {
                AudioThreadScope scope;
                fn();
            }

            allocations += allocationsOnThisThread() - before;
            locks += ThreadProbe::audioThreadLocks.load() - locksBefore;
        }
    };

    WorkshopGuitar factoryGuitar (PartLibrary& library, GuitarType type)
    {
        WorkshopGuitar g;
        PartLibrary::LoadReport report;
        library.loadGuitar (PartLibrary::getFactoryGuitarsFolder()
                              .getChildFile (LuthierAudioProcessor::getFactoryGuitarPath (type)), g, report);
        return g;
    }

    bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i)))
                    return false;

        return true;
    }

    /** A play head whose every clock field can be made invalid. */
    struct BadPlayHead : juce::AudioPlayHead
    {
        double bpm = 120.0, ppq = 0.0, seconds = 0.0, barStart = 0.0;
        bool playing = true;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setBpm (bpm);
            p.setPpqPosition (ppq);
            p.setTimeInSeconds (seconds);
            p.setPpqPositionOfLastBarStart (barStart);
            p.setIsPlaying (playing);
            p.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
            return p;
        }
    };
}

//==============================================================================
/*  P0: applyPartSwapLive used to call BodyEngine::setBodyConfig, which took
    rebuildLock and built and sorted the modal bank on the audio thread. The
    bank is now built with the swap on the message thread. */
LUTHIER_TEST (RtSafety, aLivePartSwapNeitherAllocatesNorLocksOnTheAudioThread)
{
    constexpr int kBlock = 256;

    PartLibrary library;
    library.refresh();

    const auto before = factoryGuitar (library, GuitarType::LesPaul);
    auto after = before;

    for (const auto& part : library.getParts (PartType::pickup))
        if (part->name != before.get (GuitarSlot::pickupBridge)->name)
            { after.parts[(size_t) GuitarSlot::pickupBridge] = part; break; }

    const auto derivedAfter = mapSpec (after);

    LuthierEngine engine;
    engine.prepare (kSr, kBlock);
    engine.applyWorkshopGuitar (mapSpec (before), GuitarType::LesPaul);
    CHECK (engine.partSwapKeepsStructure (derivedAfter));

    std::atomic<int> blocks { 0 };
    std::atomic<bool> stop { false };
    Probe probe;
    bool finite = true;

    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;
        midi.ensureSize (4096);

        for (int b = 0; ! stop.load() && b < 4000; ++b)
        {
            midi.clear();

            if (b == 0)
                for (int note : { 45, 52, 57 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);

            if (b < 4)   // warm-up: the first blocks may size things once
                engine.processBlock (buffer, midi);
            else
                probe.run ([&] { engine.processBlock (buffer, midi); });

            finite = finite && allFinite (buffer);
            ++blocks;
            std::this_thread::yield();
        }
    });

    while (blocks.load() < 40)
        std::this_thread::yield();

    const bool taken = engine.swapPartsAtBlockBoundary (derivedAfter, GuitarType::LesPaul);

    const int swappedBy = blocks.load();

    while (blocks.load() < swappedBy + 20)
        std::this_thread::yield();

    stop = true;
    audio.join();

    CHECK_MSG (taken, "the swap was not taken at a block boundary");
    CHECK (engine.getLivePartSwapCount() == 1);
    CHECK_NEAR (engine.getPartsPickup (0).position, derivedAfter.pickups[0].spec.position, 1.0e-12);
    CHECK (finite);
    CHECK_MSG (probe.allocations == 0, juce::String (probe.allocations) + " allocations on the audio thread around a live swap");
    CHECK_MSG (probe.locks == 0, juce::String (probe.locks) + " blocking locks on the audio thread around a live swap");
}

/*  The prebuilt bank is the bank setBodyConfig builds: installing it on the
    audio thread renders the same body as the message-thread path. */
LUTHIER_TEST (RtSafety, aPrebuiltBodyBankRendersAsTheStagedOne)
{
    BodyConfig cfg;
    cfg.scaleWidth = 1.12;   // a body other than the default
    cfg.age = 0.7;

    BodyEngine staged, prebuilt;

    for (auto* b : { &staged, &prebuilt })
    {
        b->prepare (kSr, 256);
        b->setMode (BodyEngine::Mode::Modal);
        b->setAmount (1.0);
    }

    staged.setBodyConfig (cfg);
    staged.reset();   // installs the staged bank

    // Built on the message thread, adopted at a block boundary (CODEX-RTSAFETY P0).
    prebuilt.stageBodyConfig (cfg);
    prebuilt.commitStagedConfig (cfg);
    prebuilt.reset();

    std::vector<double> a (256), b (256);
    double diff = 0.0, energy = 0.0;

    for (int block = 0; block < 8; ++block)
    {
        std::fill (a.begin(), a.end(), 0.0);
        std::fill (b.begin(), b.end(), 0.0);

        if (block == 0)
            a[3] = b[3] = 1.0;

        staged.processMono (a.data(), 256);
        prebuilt.processMono (b.data(), 256);

        for (int i = 0; i < 256; ++i)
        {
            diff += std::abs (a[(size_t) i] - b[(size_t) i]);
            energy += std::abs (a[(size_t) i]);
        }
    }

    CHECK_MSG (energy > 1.0e-6, "the body produced no response to compare");
    CHECK_MSG (diff == 0.0, "the prebuilt bank differs by " + juce::String (diff, 9));
}

//==============================================================================
/*  P0: active strumming with humanise and ghost strokes, while another thread
    keeps changing the humanisation as the editor's slider would. */
LUTHIER_TEST (RtSafety, activeRhythmWithConcurrentHumaniseNeitherAllocatesNorLocks)
{
    constexpr int kBlock = 256;

    TuningEngine tuning;
    tuning.prepare (kSr);
    tuning.setNumStrings (6);
    tuning.setTuningPreset (TuningPreset::Standard);

    RubricVoicer voicer;
    voicer.prepare (&tuning, 6);

    RhythmEngine rhythm;
    rhythm.prepare (kSr, kBlock, &tuning, &voicer);
    rhythm.setNumStrings (6);
    rhythm.setEnabled (true);
    rhythm.setSeed (0x5EEDull);

    RhythmPattern pattern;
    pattern.setName ("RT Eighths");
    pattern.setKind (RhythmPattern::Kind::strum);
    pattern.setSubdivision (Subdivision::sixteenth);
    pattern.setLength (16);
    pattern.setSwing (0.5);

    for (int i = 0; i < 16; ++i)
    {
        StrumStep step;
        step.type = (i % 2 == 0) ? (i % 4 == 0 ? StrumType::down : StrumType::up) : StrumType::rest;
        step.dynamic = 0.9;
        step.stringMask = 0x0FFF;
        pattern.setStrumStep (i, step);
    }

    rhythm.setPattern (pattern);

    {
        juce::MidiBuffer chord;

        for (int note : { 40, 47, 52, 56, 59, 64 })
            chord.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

        rhythm.handleMidi (chord, 0);
    }

    std::atomic<bool> stop { false };
    std::atomic<int> writes { 0 };

    std::thread editor ([&]
    {
        juce::Random rng (7);

        while (! stop.load())
        {
            RhythmHumanise h;
            h.timingMs = rng.nextDouble() * 20.0;
            h.velocityPercent = rng.nextDouble() * 30.0;
            h.missPercent = rng.nextDouble() * 10.0;
            h.ghostPercent = 20.0 + rng.nextDouble() * 60.0;
            h.amount = 0.5 + rng.nextDouble();
            rhythm.setHumanise (h);
            ++writes;
            std::this_thread::yield();
        }
    });

    Probe probe;
    int noteOns = 0;
    const double perBeat = 60.0 / 120.0 * kSr;

    for (int block = 0; block < 2000; ++block)
    {
        PlayEventQueue out;
        out.clear();

        RhythmTransport transport;
        transport.bpm = 120.0;
        transport.isPlaying = true;
        transport.ppqPosition = (double) (block * kBlock) / perBeat;

        probe.run ([&] { rhythm.processBlock (kBlock, transport, out); });
        noteOns += out.getNumNoteOns();

        if (block % 50 == 0)
            std::this_thread::yield();   // let the editor thread in
    }

    stop = true;
    editor.join();

    CHECK_MSG (noteOns > 100, "the rhythm engine strummed only " + juce::String (noteOns) + " notes");
    CHECK_MSG (writes.load() > 10, "the editor thread barely ran");
    CHECK_MSG (probe.allocations == 0, juce::String (probe.allocations) + " allocations while strumming");
    CHECK_MSG (probe.locks == 0, juce::String (probe.locks) + " blocking locks while strumming");
}

//==============================================================================
/*  P1: the hidden effect's scratch pair was a static thread_local that grew
    the first time the effect was switched on; and the pedal pair grew on each
    new host thread. Both are engine members sized in prepare now. */
LUTHIER_TEST (RtSafety, theHiddenEffectSwitchedOnMidSessionDoesNotAllocate)
{
    constexpr int kBlock = 128;

    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);

    juce::AudioBuffer<float> buffer (processor->getTotalNumOutputChannels(), kBlock);
    juce::MidiBuffer midi;
    midi.ensureSize (8192);

    buffer.clear();
    processor->processBlock (buffer, midi);   // warm-up

    Probe probe;
    bool finite = true;

    // A fresh host thread: a thread_local scratch would allocate here again.
    std::thread host ([&]
    {
        for (int b = 0; b < 400; ++b)
        {
            midi.clear();

            if (b % 40 == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 40 + (b / 40) % 12, (juce::uint8) 100), 3);

            if (b == 100)
                processor->getEngine().getSecretEffect().setEnabled (true);

            buffer.clear();
            probe.run ([&] { processor->processBlock (buffer, midi); });
            finite = finite && allFinite (buffer);
        }
    });

    host.join();

    CHECK (finite);
    CHECK_MSG (probe.allocations == 0, juce::String (probe.allocations) + " allocations (hidden effect / new host thread)");
    CHECK_MSG (probe.locks == 0, juce::String (probe.locks) + " blocking locks (hidden effect / new host thread)");
}

/*  P1: a host block bigger than prepareToPlay promised, carrying dense MIDI and
    SysEx, is sliced without creating or growing a MidiBuffer. */
LUTHIER_TEST (RtSafety, oversizedBlocksWithDenseMidiDoNotAllocate)
{
    constexpr int kPrepared = 128;
    constexpr int kHostBlock = 1000;

    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kPrepared);

    juce::AudioBuffer<float> buffer (processor->getTotalNumOutputChannels(), kHostBlock);
    juce::MidiBuffer midi;
    midi.ensureSize (256 * 1024);

    {
        juce::AudioBuffer<float> warm (processor->getTotalNumOutputChannels(), kPrepared);
        juce::MidiBuffer none;
        warm.clear();
        processor->processBlock (warm, none);
    }

    const juce::uint8 sysex[] = { 0xf0, 0x7d, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0xf7 };

    Probe probe;
    bool finite = true;

    for (int b = 0; b < 60; ++b)
    {
        midi.clear();

        for (int i = 0; i < 600; ++i)
        {
            const int pos = (i * 7 + b) % kHostBlock;
            const int note = 40 + (i % 30);

            if (i % 3 == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 90), pos);
            else if (i % 3 == 1)
                midi.addEvent (juce::MidiMessage::noteOff (1, note), pos);
            else
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, i % 128), pos);

            if (i % 50 == 0)
                midi.addEvent (sysex, (int) sizeof (sysex), pos);
        }

        buffer.clear();
        probe.run ([&] { processor->processBlock (buffer, midi); });
        finite = finite && allFinite (buffer);
    }

    CHECK (finite);
    CHECK_MSG (probe.allocations == 0, juce::String (probe.allocations) + " allocations for oversized dense blocks");
    CHECK_MSG (probe.locks == 0, juce::String (probe.locks) + " blocking locks for oversized dense blocks");
    CHECK_MSG (processor->getMidiOverflowDropCount() == 0,
               juce::String (processor->getMidiOverflowDropCount()) + " events trimmed at a realistic density");
}

/*  The overflow policy itself: a burst beyond the reserve never grows the
    buffer, and the note-offs survive when the other events are cut. */
LUTHIER_TEST (RtSafety, boundedMidiCopyKeepsNoteOffsAndNeverGrows)
{
    constexpr int kReserved = 4096;
    juce::MidiBuffer dest;
    dest.ensureSize (kReserved);

    juce::MidiBuffer burst;

    for (int i = 0; i < 3000; ++i)
    {
        burst.addEvent (juce::MidiMessage::controllerEvent (1, 1, i % 128), i);
        if (i % 10 == 0)
            burst.addEvent (juce::MidiMessage::noteOff (1, 40 + i % 40), i);
    }

    int offsIn = 0;

    for (const auto m : burst)
        if (BoundedMidi::isRelease (m.data, m.numBytes))
            ++offsIn;

    const auto before = allocationsOnThisThread();
    const int dropped = BoundedMidi::addEvents (dest, kReserved, burst, 0, -1, 0);
    CHECK (allocationsOnThisThread() == before);

    int offsOut = 0;

    for (const auto m : dest)
        if (BoundedMidi::isRelease (m.data, m.numBytes))
            ++offsOut;

    CHECK (BoundedMidi::bytesUsed (dest) <= kReserved);
    CHECK (dropped > 0);
    CHECK_MSG (offsOut == offsIn, juce::String (offsOut) + " of " + juce::String (offsIn) + " note-offs kept");

    // A long SysEx is inspected without the heap, and is not a channel message.
    const juce::uint8 sysex[] = { 0xf0, 0x7d, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0xf7 };
    juce::MidiBuffer s;
    s.addEvent (sysex, (int) sizeof (sysex), 0);

    for (const auto m : s)
    {
        const auto a = allocationsOnThisThread();
        const auto inspected = BoundedMidi::inspect (m);
        CHECK (allocationsOnThisThread() == a);
        CHECK (! inspected.isController() && ! inspected.isNoteOnOrOff() && ! inspected.isProgramChange());
    }

    CHECK (BoundedMidi::isRelease (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 0).getRawData(), 3));
    CHECK (BoundedMidi::isRelease (juce::MidiMessage::allNotesOff (1).getRawData(), 3));
    CHECK (BoundedMidi::isRelease (juce::MidiMessage::controllerEvent (1, 64, 0).getRawData(), 3));
    CHECK (! BoundedMidi::isRelease (juce::MidiMessage::controllerEvent (1, 64, 127).getRawData(), 3));
    CHECK (! BoundedMidi::isRelease (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 1).getRawData(), 3));
}

//==============================================================================
/*  P2: invalid host clocks are rejected at the processor boundary, the last
    valid value stands in, and rhythm scheduling stays finite. */
LUTHIER_TEST (RtSafety, invalidHostClockIsRejectedAndOutputStaysFinite)
{
    constexpr int kBlock = 256;

    auto processor = std::make_unique<LuthierAudioProcessor>();
    BadPlayHead head;
    processor->setPlayHead (&head);
    processor->prepareToPlay (kSr, kBlock);
    processor->getEngine().getRhythmEngine().setEnabled (true);

    juce::AudioBuffer<float> buffer (processor->getTotalNumOutputChannels(), kBlock);
    juce::MidiBuffer midi;
    midi.ensureSize (8192);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    Probe probe;
    bool finite = true;
    bool tempoValid = true;

    for (int b = 0; b < 400; ++b)
    {
        midi.clear();

        if (b == 2)
            for (int note : { 40, 47, 52, 55, 59, 64 })
                midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

        const double validPpq = (double) (b * kBlock) / (kSr * 0.5);

        switch ((b / 20) % 5)
        {
            case 0:  head.bpm = 120.0; head.ppq = validPpq; head.seconds = b * kBlock / kSr; head.barStart = 0.0; break;
            case 1:  head.bpm = nan;   head.ppq = nan;      head.seconds = nan;  head.barStart = nan; break;
            case 2:  head.bpm = inf;   head.ppq = -inf;     head.seconds = inf;  head.barStart = inf; break;
            case 3:  head.bpm = 0.0;   head.ppq = validPpq; head.seconds = 0.0;  head.barStart = 0.0; break;
            default: head.bpm = -90.0; head.ppq = nan;      head.seconds = -inf; head.barStart = nan; break;
        }

        buffer.clear();

        if (b < 2)
            processor->processBlock (buffer, midi);
        else
            probe.run ([&] { processor->processBlock (buffer, midi); });

        finite = finite && allFinite (buffer);

        const double tempo = processor->getEngine().getTempoBpm();
        tempoValid = tempoValid && std::isfinite (tempo) && tempo > 0.0;
        tempoValid = tempoValid && std::isfinite (processor->getEngine().getTransportPpq());
    }

    CHECK (finite);
    CHECK_MSG (tempoValid, "an invalid host tempo or position reached the engine");
    CHECK (processor->getRejectedHostClockCount() > 0);
    CHECK_MSG (probe.allocations == 0, juce::String (probe.allocations) + " allocations with an invalid host clock");
    CHECK_MSG (probe.locks == 0, juce::String (probe.locks) + " blocking locks with an invalid host clock");

    processor->setPlayHead (nullptr);
}

LUTHIER_TEST (RtSafety, hostClockGuardKeepsTheLastValidValue)
{
    HostClockGuard guard;
    juce::AudioPlayHead::PositionInfo p;

    // Nothing valid yet: an invalid field is left out.
    p.setBpm (std::numeric_limits<double>::quiet_NaN());
    CHECK (! guard.sanitise (p).getBpm().hasValue());

    p.setBpm (97.0);
    p.setPpqPosition (12.5);
    CHECK_NEAR (*guard.sanitise (p).getBpm(), 97.0, 1.0e-12);

    p.setBpm (-1.0);
    p.setPpqPosition (std::numeric_limits<double>::infinity());
    const auto s = guard.sanitise (p);
    CHECK_NEAR (*s.getBpm(), 97.0, 1.0e-12);
    CHECK_NEAR (*s.getPpqPosition(), 12.5, 1.0e-12);
    CHECK (guard.getRejectedCount() == 3);

    // Negative PPQ is a valid pre-roll, not an error.
    p.setPpqPosition (-2.0);
    CHECK_NEAR (*guard.sanitise (p).getPpqPosition(), -2.0, 1.0e-12);

    CHECK_NEAR (HostClockGuard::validSampleRate (std::numeric_limits<double>::quiet_NaN(), 48000.0), 48000.0, 1.0e-9);
    CHECK_NEAR (HostClockGuard::validSampleRate (0.0, 96000.0), 96000.0, 1.0e-9);
    CHECK_NEAR (HostClockGuard::validSampleRate (-44100.0, std::numeric_limits<double>::infinity()), 44100.0, 1.0e-9);
    CHECK_NEAR (HostClockGuard::validSampleRate (88200.0, 48000.0), 88200.0, 1.0e-9);
}

LUTHIER_TEST (RtSafety, anInvalidHostSampleRateKeepsTheLastValidOne)
{
    constexpr int kBlock = 128;

    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (kSr, kBlock);
    processor->prepareToPlay (std::numeric_limits<double>::quiet_NaN(), kBlock);

    juce::AudioBuffer<float> buffer (processor->getTotalNumOutputChannels(), kBlock);
    juce::MidiBuffer midi;
    bool finite = true;

    for (int b = 0; b < 200; ++b)
    {
        midi.clear();

        if (b % 50 == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 110), 0);

        buffer.clear();
        processor->processBlock (buffer, midi);
        finite = finite && allFinite (buffer);
    }

    CHECK (finite);

    processor->prepareToPlay (-1.0, 0);   // non-positive rate and block
    buffer.clear();
    processor->processBlock (buffer, midi);
    CHECK (allFinite (buffer));
}

//==============================================================================
/*  docs/audit/BETA_TEST_REPORT.md: ReverbPedal::rebuildLines grew its lines on
    the audio thread when Size changed. The lines are sized for the longest
    length in prepare; a Size or Character sweep only refills them. */
LUTHIER_TEST (RtSafety, reverbSizeAndCharacterChangesDoNotAllocate)
{
    constexpr int kBlock = 128;

    ReverbPedal reverb;
    reverb.prepare (kSr, kBlock);

    std::vector<double> l (kBlock, 0.0), r (kBlock, 0.0);
    Probe probe;
    bool finite = true;

    for (int b = 0; b < 400; ++b)
    {
        for (int i = 0; i < kBlock; ++i)
            l[(size_t) i] = r[(size_t) i] = (i == 0 && b % 20 == 0) ? 0.5 : 0.0;

        probe.run ([&]
        {
            reverb.setParameterValue (0, (double) (b % 11) / 10.0);   // Size, 0..1
            reverb.setParameterValue (4, (double) ((b / 7) % 4));      // Character: hall is the longest
            reverb.process (l.data(), r.data(), kBlock);
        });

        for (int i = 0; i < kBlock; ++i)
            finite = finite && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]);
    }

    CHECK (finite);
    CHECK_MSG (probe.allocations == 0, juce::String (probe.allocations) + " allocations on Size / Character changes");
    CHECK_MSG (probe.locks == 0, juce::String (probe.locks) + " blocking locks on Size / Character changes");
}
