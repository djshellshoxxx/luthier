/*  Jam mode, engine and timing (jam-mode.md 17, JM-01 to JM-23). The band on
    a bench (JamTestHarness.h): a fixture host, a note list, the capture ring
    as the record of what played and when. */

#include "JamTestHarness.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr int kL = 37;   // a latency the band must add (0.6)

    ChordSymbol detectChord (std::initializer_list<int> notes)
    {
        std::vector<int> v (notes);
        ChordDetector d;
        d.prepare (48000.0);
        return d.detect (v.data(), (int) v.size());
    }

    const std::initializer_list<int> kAm { 45, 48, 52 };
    const std::initializer_list<int> kF  { 41, 45, 48 };
    const std::initializer_list<int> kC  { 48, 52, 55 };
    const std::initializer_list<int> kG  { 43, 47, 50 };
    const std::initializer_list<int> kDm { 50, 53, 57 };

    int64_t at (double ppq) { return (int64_t) std::llround (ppq * 24000.0); }

    void hostBand (JamBench& b)
    {
        b.host = true;
        b.hostPlaying = true;
        b.context.latency = kL;
    }
}

//==============================================================================
LUTHIER_TEST (Jam, JM01_everyFactoryStyleParsesAndIsComplete)
{
    JamStyleLibrary library;

    for (int i = 0; i < jam::kNumFactoryStyles; ++i)
    {
        const auto* style = library.getFactoryStyle (i);
        CHECK (style != nullptr);

        if (style == nullptr)
            continue;

        juce::String why;
        CHECK_MSG (style->isComplete (&why), style->name + ": " + why);

        // Through the file format and back: it still parses and is complete,
        // and it writes the same file again.
        const auto text = juce::JSON::toString (style->toVar());
        JamStyle parsed;
        juce::String error;
        CHECK_MSG (JamStyle::fromVar (juce::JSON::parse (text), parsed, error), style->name + ": " + error);
        CHECK_MSG (parsed.isComplete (&why), style->name + " after a round trip: " + why);
        CHECK (juce::JSON::toString (parsed.toVar()) == text);

        // Every lane is as long as its grid.
        for (const auto& m : style->meters)
        {
            if (! m.present)
                continue;

            const int bar = m.barSteps (style->stepsPerQuarter());

            for (const auto& variation : m.grooves)
                for (const auto& groove : variation)
                    for (int lane = 0; lane < jam::numDrumLanes; ++lane)
                        CHECK (groove.laneString (lane).length() == bar && groove.bassString().length() == bar);
        }
    }

    CHECK (library.getFactoryStyle (4)->findMeter (3, 4) != nullptr);   // Country 3/4
    CHECK (library.getFactoryStyle (8)->findMeter (3, 4) != nullptr);   // Ballad 3/4
    CHECK (library.getFactoryStyle (8)->findMeter (6, 8) != nullptr);   // Ballad 6/8
    CHECK (library.getFactoryStyle (0)->findMeter (3, 4) == nullptr);   // Rock is 4/4 only
    CHECK (library.getFactoryStyle (3)->grid == 12 && library.getFactoryStyle (7)->grid == 12);
}

//==============================================================================
LUTHIER_TEST (Jam, JM02_rockKicksLandOnTheHostGrid)
{
    JamBench b;
    hostBand (b);
    b.chord (0, kAm);
    b.runSeconds (128.0);   // 64 bars at 120

    const auto kicks = b.noteOns (0, 36);
    CHECK_MSG (kicks.size() >= 64 * 3, "kicks: " + juce::String ((int) kicks.size()));

    int worst = 0;

    for (const auto& k : kicks)
        worst = juce::jmax (worst, (int) std::abs (k.sample - (at (k.ppq) + kL)));

    CHECK_MSG (worst <= 1, "worst kick is " + juce::String (worst) + " samples off grid + L");
}

LUTHIER_TEST (Jam, JM03_tempoAutomationDoesNotDrift)
{
    JamBench b;
    hostBand (b);

    // 90 to 140 bpm over 8 bars, then 140.
    b.hostBpmAt = [&b] (int64_t) { return 90.0 + 50.0 * juce::jmin (1.0, b.hostPpq / 32.0); };

    struct Seg { int64_t start; double ppq, bpm; };
    std::vector<Seg> segments;
    b.beforeBlock = [&segments] (JamBench& bench)
    {
        segments.push_back ({ bench.position, bench.hostPpq, bench.bpmAt (bench.position) });
    };

    b.chord (0, kAm);
    b.runSeconds (40.0);

    auto gridSample = [&segments] (double beat) -> int64_t
    {
        for (size_t i = 0; i < segments.size(); ++i)
        {
            const double end = i + 1 < segments.size() ? segments[i + 1].ppq : 1.0e300;

            if (beat >= segments[i].ppq - 1.0e-12 && beat < end)
                return segments[i].start + (int64_t) std::floor ((beat - segments[i].ppq) * 60.0 * 48000.0 / segments[i].bpm + 0.5);
        }

        return -1;
    };

    int beats = 0, worst = 0;

    for (const auto& e : b.noteOns (0))
    {
        if (std::abs (e.ppq - std::round (e.ppq)) > 1.0e-9)
            continue;

        const auto grid = gridSample (e.ppq);
        worst = juce::jmax (worst, (int) std::abs (e.sample - kL - grid));
        ++beats;
    }

    CHECK (beats > 100);
    CHECK_MSG (worst <= 1, "worst beat drift " + juce::String (worst) + " samples");
}

//==============================================================================
namespace
{
    void chordEveryBar (JamBench& b, int bars)
    {
        const std::initializer_list<int>* cycle[] = { &kAm, &kF, &kC, &kG };

        for (int bar = 0; bar < bars; ++bar)
        {
            const auto t = at (bar * 4.0) + 300 + (bar * 977) % 3000;

            if (bar > 0)
                b.release (t - 10, *cycle[(bar - 1) % 4]);

            b.chord (t, *cycle[bar % 4], 70 + (bar * 13) % 50);
        }
    }

    JamBench& fixture30s (JamBench& b, uint64_t seed)
    {
        hostBand (b);
        b.settings.humanise = 50.0;
        b.settings.fillEvery = 2;
        b.settings.dynamicsFollow = true;
        b.engine.setSeed (seed);
        b.keepAudio = true;
        chordEveryBar (b, 16);
        return b;
    }
}

LUTHIER_TEST (Jam, JM04_rendersAreDeterministicPerSeed)
{
    JamBench a, b, c;
    fixture30s (a, 1234).runSeconds (30.0);
    fixture30s (b, 1234).runSeconds (30.0);
    fixture30s (c, 98765).runSeconds (30.0);

    CHECK (a.left.size() == b.left.size());
    CHECK_MSG (a.left == b.left && a.right == b.right, "two renders of one seed differ");
    CHECK_MSG (a.left != c.left, "a different seed gave the same render");
}

LUTHIER_TEST (Jam, JM05_blockSizesNull)
{
    JamBench reference (48000.0, 32);
    fixture30s (reference, 777).runSeconds (20.0);

    for (int size : { 128, 512, 2048 })
    {
        JamBench other (48000.0, size, size == 2048 ? 512 : size);
        fixture30s (other, 777).runSeconds (20.0);

        CHECK (other.left.size() == reference.left.size());
        double worst = 0.0;

        for (size_t i = 0; i < juce::jmin (other.left.size(), reference.left.size()); ++i)
            worst = juce::jmax (worst, (double) std::abs (other.left[i] - reference.left[i]),
                                (double) std::abs (other.right[i] - reference.right[i]));

        CHECK_MSG (worst <= 1.0e-6, "block " + juce::String (size) + " differs by " + juce::String (gainToDb (worst), 1) + " dBFS");
    }
}

//==============================================================================
LUTHIER_TEST (Jam, JM06_naturalFollowChangesOnTheNextBeat)
{
    JamBench b;
    hostBand (b);
    b.chord (0, kAm);
    const auto t = at (1.0) + 9600;   // 200 ms after beat 2
    b.release (t - 1, kAm);
    b.chord (t, kF);
    b.runSeconds (3.0);

    std::vector<JamCaptureEvent> store;
    const auto* f = b.firstBass (5, 0, store);
    CHECK (f != nullptr);

    if (f != nullptr)
        CHECK_MSG (f->sample == at (2.0) + kL, "F came at " + juce::String (f->sample) + ", beat 3 is " + juce::String (at (2.0) + kL));
}

LUTHIER_TEST (Jam, JM07_graceWindowChangesAtDetection)
{
    JamBench b;
    hostBand (b);
    b.chord (0, kAm);
    const auto t = at (1.0) + 1920;   // 40 ms after beat 2
    b.release (t - 1, kAm);
    b.chord (t, kF);
    b.runSeconds (3.0);

    std::vector<JamCaptureEvent> store;
    const auto* f = b.firstBass (5, 0, store);
    CHECK (f != nullptr);

    if (f != nullptr)
    {
        const auto after = f->sample - kL - t;
        CHECK_MSG (after >= 1440 && after <= 1440 + b.block, "F came " + juce::String (after) + " samples after the note");
    }
}

LUTHIER_TEST (Jam, JM08_tightRelaxedAndBarQuantise)
{
    juce::Random random (42);
    const int modes[] = { 0, 2, 3 };

    for (int mode : modes)
    {
        int wrong = 0;

        for (int trial = 0; trial < 100; ++trial)
        {
            JamBench b;
            hostBand (b);
            b.settings.follow = mode;
            b.settings.predict = false;

            const double q = jamclock::quantum (mode, 4.0);
            const double grace = 0.18;

            // A first note outside the grace window, somewhere in bar 1.
            const double boundary = 4.0 + q * random.nextInt ((int) (4.0 / q));
            const double firstPpq = boundary + grace + 0.01 + random.nextDouble() * (q - grace - 0.02);
            const auto t = at (firstPpq);

            b.chord (0, kAm);
            b.release (t - 1, kAm);
            b.chord (t, kF);
            b.runSeconds (7.0);

            const double detPpq = firstPpq + 0.06;
            const double expectedPpq = jamclock::nextBoundary (detPpq, std::floor (detPpq / 4.0) * 4.0, q);

            std::vector<JamCaptureEvent> store;
            const auto* f = b.firstBass (5, 0, store);

            if (f == nullptr || f->sample != at (expectedPpq) + kL)
                ++wrong;
        }

        CHECK_MSG (wrong == 0, "follow " + juce::String (mode) + ": " + juce::String (wrong) + " of 100 changed at the wrong time");
    }
}

LUTHIER_TEST (Jam, JM09_singleNotesAndUnknownsNeverChangeTheChord)
{
    JamBench b;
    hostBand (b);
    b.chord (0, kAm);
    b.release (at (0.5), kAm);

    juce::Random random (9);
    ChordDetector detector;
    detector.prepare (48000.0);
    int phrases = 0;
    int64_t t = at (1.0);

    while (phrases < 1000)
    {
        if (random.nextBool())
        {
            const int n = 40 + random.nextInt (40);
            b.chord (t, { n }, 30 + random.nextInt (90));
            b.release (t + 1800, { n });
            ++phrases;
        }
        else
        {
            const int r = 40 + random.nextInt (30);
            int cluster[] = { r, r + 1, r + 2 };

            if (detector.detect (cluster, 3).isKnown())
                continue;

            b.notes.push_back ({ t, juce::MidiMessage::noteOn (1, r, (juce::uint8) 90) });
            b.notes.push_back ({ t, juce::MidiMessage::noteOn (1, r + 1, (juce::uint8) 90) });
            b.notes.push_back ({ t, juce::MidiMessage::noteOn (1, r + 2, (juce::uint8) 90) });
            b.release (t + 1800, { r, r + 1, r + 2 });
            ++phrases;
        }

        t += 2400;   // 50 ms apart: each its own burst
    }

    b.run (t + 48000);

    CHECK (b.engine.getBassChord() == detectChord (kAm));

    for (const auto& e : b.noteOns (1))
        CHECK_MSG (e.note % 12 == 9, "a bass note left A: " + juce::String (e.note));
}

LUTHIER_TEST (Jam, JM10_slashChordRootTokensPlayTheBassNote)
{
    JamBench b;
    hostBand (b);
    b.chord (0, { 43, 48, 52, 55 });   // C/G
    b.runSeconds (2.0);

    const auto bassNotes = b.noteOns (1);
    CHECK (! bassNotes.empty());

    for (const auto& e : bassNotes)
        if (std::abs (e.ppq - std::round (e.ppq)) < 1.0e-9)
            CHECK_MSG (e.note % 12 == 7, "R played " + juce::String (e.note));
}

//==============================================================================
namespace
{
    std::unique_ptr<JamChordMap> fourChordMap()
    {
        auto map = std::make_unique<JamChordMap>();
        const std::initializer_list<int>* chords[] = { &kAm, &kF, &kC, &kG };

        for (int i = 0; i < 4; ++i)
        {
            const auto s = detectChord (*chords[i]);
            auto& e = map->entries[(size_t) i];
            e.ppq = i * 4.0;
            e.root = s.root;
            e.bass = s.bass;
            e.templateIndex = s.templateIndex;
        }

        map->count = 4;
        map->lengthPpq = 16.0;
        map->loop = true;
        return map;
    }

    void followTune (JamBench& b)
    {
        b.settings.chordSource = 2;
        b.beforeBlock = [] (JamBench& bench)
        {
            bench.context.tunePlaying = bench.context.tuneRunning = true;
            bench.context.tuneFollowingHost = true;
            bench.context.tunePpq = bench.hostPpq;
            bench.context.tuneBpm = 120.0;
        };
    }
}

LUTHIER_TEST (Jam, JM11_tuneChangesLandExactlyWithApproaches)
{
    JamBench b;
    hostBand (b);
    followTune (b);
    b.engine.setChordMap (fourChordMap());
    b.runSeconds (32.0);

    const int roots[] = { 9, 5, 0, 7 };
    const auto bassNotes = b.noteOns (1);

    for (int change = 1; change < 8; ++change)
    {
        const double p = change * 4.0;
        const int root = roots[change % 4];
        const JamCaptureEvent* onChange = nullptr;
        const JamCaptureEvent* approach = nullptr;

        for (const auto& e : bassNotes)
        {
            if (e.sample == at (p) + kL)
                onChange = &e;

            if (e.sample == at (p - 0.5) + kL)
                approach = &e;
        }

        CHECK_MSG (onChange != nullptr && onChange->note % 12 == root, "change at " + juce::String (p) + " missed");

        if (onChange != nullptr && approach != nullptr)
        {
            const int step = onChange->note - approach->note;
            CHECK_MSG (step == 1 || step == -2, "the A token at " + juce::String (p - 0.5) + " stepped " + juce::String (step));
        }
        else
        {
            CHECK_MSG (approach != nullptr, "no approach note before " + juce::String (p));
        }
    }
}

LUTHIER_TEST (Jam, JM12_predictionFromTheThirdCycle)
{
    JamBench b;
    hostBand (b);

    const std::initializer_list<int>* first[] = { &kAm, &kF, &kC, &kG };
    const std::initializer_list<int>* second[] = { &kAm, &kDm, &kC, &kG };

    for (int bar = 0; bar < 28; ++bar)
    {
        const auto& chords = bar < 12 ? first : second;
        const auto t = at (bar * 4.0) + 4800;   // 100 ms late, outside the grace window

        if (bar > 0)
        {
            const auto& prev = (bar - 1) < 12 ? first : second;
            b.release (t - 1, *prev[(bar - 1) % 4]);
        }

        b.chord (t, *chords[bar % 4]);
    }

    b.runSeconds (28 * 2.0 + 1.0);

    std::vector<JamCaptureEvent> store;
    auto firstIn = [&] (int pc, double fromPpq) -> double
    {
        const auto* e = b.firstBass (pc, at (fromPpq) + kL, store);
        return e == nullptr ? -1.0 : (double) (e->sample - kL) / 24000.0;
    };

    CHECK_MSG (std::abs (firstIn (5, 20.0) - 21.0) < 1.0e-6, "cycle 2 is followed, not predicted: " + juce::String (firstIn (5, 20.0)));
    CHECK_MSG (std::abs (firstIn (5, 36.0) - 36.0) < 1.0e-6, "cycle 3 predicts F on the downbeat: " + juce::String (firstIn (5, 36.0)));
    CHECK_MSG (std::abs (firstIn (2, 52.0) - 53.0) < 1.0e-6, "the deviation is corrected at the next Q: " + juce::String (firstIn (2, 52.0)));
    CHECK_MSG (std::abs (firstIn (0, 56.0) - 57.0) < 1.0e-6, "prediction is off after it: " + juce::String (firstIn (0, 56.0)));
    CHECK_MSG (std::abs (firstIn (2, 84.0) - 84.0) < 1.0e-6, "two clean cycles later it predicts again: " + juce::String (firstIn (2, 84.0)));
}

LUTHIER_TEST (Jam, JM13_rhythmEngineChordIsJamsChord)
{
    JamBench b;
    hostBand (b);
    const ChordSymbol chords[] = { detectChord (kAm), detectChord (kF), detectChord (kC), detectChord (kG) };
    int block = 0, mismatches = 0;

    b.beforeBlock = [&] (JamBench& bench)
    {
        bench.context.rhythmDriving = true;
        bench.context.rhythmChord = chords[(block++ / 37) % 4];
    };

    for (int i = 0; i < 400; ++i)
    {
        b.run (b.block);

        if (b.engine.getHeardChord() != b.context.rhythmChord)
            ++mismatches;
    }

    CHECK_MSG (mismatches == 0, juce::String (mismatches) + " blocks disagreed");
}

//==============================================================================
LUTHIER_TEST (Jam, JM14_firstNoteStartsOnItsSample)
{
    JamBench b;
    b.context.latency = kL;
    b.settings.startMode = 2;
    const int64_t t = 10007;
    b.chord (t, kAm);
    b.runSeconds (2.0);

    const auto kicks = b.noteOns (0, 36);
    const auto crashes = b.noteOns (0, 49);
    CHECK (! kicks.empty() && ! crashes.empty());

    if (! kicks.empty() && ! crashes.empty())
    {
        CHECK_MSG (kicks.front().sample == t + kL, "kick at " + juce::String (kicks.front().sample));
        CHECK_MSG (crashes.front().sample == t + kL, "crash at " + juce::String (crashes.front().sample));
    }
}

LUTHIER_TEST (Jam, JM15_tapInSetsTempoAndPhase)
{
    JamBench b;
    b.context.latency = kL;
    b.settings.startMode = 4;
    const int64_t taps[] = { 1000, 25000, 49000, 73000 };
    int next = 0;

    b.beforeBlock = [&] (JamBench& bench)
    {
        while (next < 4 && taps[next] < bench.position + bench.block)
            bench.engine.tapAtSample (taps[next++]);
    };

    b.runSeconds (4.0);

    CHECK_NEAR (b.engine.getTempo(), 120.0, 0.01);
    const auto kicks = b.noteOns (0, 36);
    CHECK (! kicks.empty());

    if (! kicks.empty())
        CHECK_NEAR ((double) (kicks.front().sample - kL - (73000 + 24000)), 0.0, 48.0);
}

LUTHIER_TEST (Jam, JM16_countInSticks)
{
    for (int bars : { 1, 0 })
    {
        JamBench b;
        b.context.latency = kL;
        b.settings.startMode = 3;
        b.settings.countInBars = bars;
        b.engine.requestStart();
        b.runSeconds (5.0);

        const auto kicks = b.noteOns (0, 36);
        CHECK (! kicks.empty());

        int sticks = 0;

        for (const auto& e : b.noteOns (0, 37))
            if (! kicks.empty() && e.sample < kicks.front().sample)
                ++sticks;

        CHECK_MSG (sticks == bars * 4, juce::String (bars) + " count-in bar(s) gave " + juce::String (sticks) + " sticks");
    }
}

LUTHIER_TEST (Jam, JM17_stopWhenIStopPlaying)
{
    {
        JamBench b;
        b.context.latency = kL;
        b.settings.startMode = 2;
        b.settings.stopOnSilence = true;
        b.settings.silenceBars = 2;
        b.chord (0, kAm);              // held for ever: a held chord is not playing
        b.chord (at (5.0), { 69 });    // the last note-on, in bar 1
        b.runSeconds (12.0);

        const auto crashes = b.noteOns (0, 49);
        bool endingAtBar4 = false;

        for (const auto& e : crashes)
            endingAtBar4 = endingAtBar4 || e.sample == at (16.0) + kL;

        CHECK_MSG (endingAtBar4, "no ending on the downbeat after two silent bars");
        CHECK (b.engine.getState() == JamState::armed);

        for (const auto& e : b.noteOns (0, 36))
            CHECK (e.sample <= at (16.0) + kL);
    }

    {
        // Under host transport the DAW owns start and stop.
        JamBench b;
        hostBand (b);
        b.settings.stopOnSilence = true;
        b.settings.silenceBars = 1;
        b.chord (0, kAm);
        b.runSeconds (20.0);
        CHECK (b.engine.getState() == JamState::playing);
    }
}

LUTHIER_TEST (Jam, JM18_hostStopEndsOrCutsAndPanicChokes)
{
    auto peakAfter = [] (const JamBench& b, int64_t from)
    {
        double peak = 0.0;

        for (size_t i = (size_t) juce::jmax ((int64_t) 0, from); i < b.left.size(); ++i)
            peak = juce::jmax (peak, (double) std::abs (b.left[i]), (double) std::abs (b.right[i]));

        return peak;
    };

    {
        JamBench b;
        hostBand (b);
        b.chord (0, kAm);
        b.runSeconds (4.0);
        const auto stopAt = b.position;
        b.hostPlaying = false;
        b.runSeconds (3.0);

        bool crash = false;

        for (const auto& e : b.noteOns (0, 49))
            crash = crash || (e.sample >= stopAt && e.sample <= stopAt + b.block + kL);

        CHECK_MSG (crash, "the host stopped with the ending on, and no ending played");
        CHECK (b.engine.getState() == JamState::armed);
    }

    {
        JamBench b;
        hostBand (b);
        b.keepAudio = true;
        b.settings.ending = false;
        b.chord (0, kAm);
        b.runSeconds (4.0);
        const auto stopAt = b.position;
        b.hostPlaying = false;
        b.runSeconds (1.0);

        CHECK_MSG (peakAfter (b, stopAt + (int64_t) (0.020 * 48000) + kL) < 1.0e-3, "the band did not cut within 20 ms");
    }

    {
        JamBench b;
        hostBand (b);
        b.keepAudio = true;
        b.settings.intensity = 5;
        b.chord (0, kAm);
        b.runSeconds (4.0);
        const auto panicAt = b.position;
        b.engine.requestPanic();
        b.runSeconds (0.5);

        const double peak = peakAfter (b, panicAt + (int64_t) (0.010 * 48000));
        CHECK_MSG (peak < std::pow (10.0, -90.0 / 20.0), "after panic the band is at " + juce::String (gainToDb (peak), 1) + " dBFS");
        CHECK (b.engine.getState() == JamState::armed);
    }
}

//==============================================================================
LUTHIER_TEST (Jam, JM19_changesLandOnBeatsAndBars)
{
    JamBench b;
    hostBand (b);
    b.settings.kitAuto = false;
    b.settings.fillEvery = 1;   // 2 bars: a fill in bar 1
    b.chord (0, kAm);

    b.beforeBlock = [] (JamBench& bench)
    {
        if (bench.hostPpq >= 1.3 && bench.hostPpq < 1.4)
        {
            bench.settings.intensity = 5;
            bench.settings.kit = 2;
        }

        if (bench.hostPpq >= 5.5 && bench.hostPpq < 5.6)
            bench.settings.style = 2;
    };

    b.runSeconds (6.0);

    const auto rides = b.noteOns (0, 51);
    CHECK (! rides.empty());

    if (! rides.empty())
        CHECK_MSG (std::abs (rides.front().ppq - 2.0) < 1.0e-9, "intensity 5 began at " + juce::String (rides.front().ppq));

    // Markers: the kit at bar 1, the style at bar 2 (after bar 1's fill).
    int kitAt = -1, styleAt = -1;

    for (const auto& e : b.events())
    {
        if (e.part != 2)
            continue;

        if (e.kit == 2 && kitAt < 0)     kitAt = (int) std::round (e.ppq);
        if (e.style == 2 && styleAt < 0) styleAt = (int) std::round (e.ppq);
    }

    CHECK_MSG (kitAt == 4, "the kit changed at " + juce::String (kitAt));
    CHECK_MSG (styleAt == 8, "the style changed at " + juce::String (styleAt));

    bool fillInBar1 = false;

    for (int tom : { 45, 47, 50 })
        for (const auto& e : b.noteOns (0, tom))
            fillInBar1 = fillInBar1 || (e.ppq >= 4.0 && e.ppq < 8.0);

    CHECK_MSG (fillInBar1, "bar 1's fill did not play (it must finish before the style changes)");
}

LUTHIER_TEST (Jam, JM20_fillNow)
{
    auto toms = [] (const JamBench& b, double from, double to)
    {
        int n = 0;

        for (int tom : { 45, 47, 50 })
            for (const auto& e : b.noteOns (0, tom))
                if (e.ppq >= from - 1.0e-9 && e.ppq < to - 1.0e-9)
                    ++n;

        return n;
    };

    {
        JamBench b;
        hostBand (b);
        b.chord (0, kAm);
        bool asked = false;
        b.beforeBlock = [&asked] (JamBench& bench)
        {
            if (! asked && bench.hostPpq >= 0.3) { bench.engine.requestFill(); asked = true; }
        };
        b.runSeconds (6.0);

        CHECK_MSG (toms (b, 2.0, 4.0) > 0, "no fill up to the bar line");
        CHECK (toms (b, 0.0, 1.0) == 0);
        CHECK (toms (b, 4.0, 12.0) == 0);

        bool fillSnare = false;

        for (const auto& e : b.noteOns (0, 38))
            fillSnare = fillSnare || std::abs (e.ppq - 1.5) < 1.0e-9;

        CHECK_MSG (fillSnare, "the fill did not start on the next beat");
    }

    {
        JamBench b;
        hostBand (b);
        b.chord (0, kAm);
        bool asked = false;
        b.beforeBlock = [&asked] (JamBench& bench)
        {
            if (! asked && bench.hostPpq >= 11.5) { bench.engine.requestFill(); asked = true; }
        };
        b.runSeconds (9.0);

        CHECK_MSG (toms (b, 14.0, 16.0) > 0, "no fill in the last 2 beats of the next bar");
        CHECK (toms (b, 8.0, 14.0) == 0);
    }
}

LUTHIER_TEST (Jam, JM21_dynamicsFollow)
{
    auto settle = [] (int base, int velocity, int jitter)
    {
        JamBench b;
        hostBand (b);
        b.settings.intensity = base;
        b.settings.dynamicsFollow = true;
        juce::Random random (5);

        for (int beat = 0; beat < 64; ++beat)
        {
            const int v = velocity + (jitter > 0 ? random.nextInt (2 * jitter + 1) - jitter : 0);
            b.chord (at (beat) + 500, { 57 }, v);
            b.release (at (beat) + 12000, { 57 });
        }

        int changes = 0, last = -1;
        b.beforeBlock = [&] (JamBench& bench)
        {
            const int now = bench.engine.getEffectiveIntensity();

            if (bench.hostPpq > 8.0 && last >= 0 && now != last)
                ++changes;

            last = now;
        };

        b.runSeconds (32.0);
        return std::make_pair (b.engine.getEffectiveIntensity(), changes);
    };

    CHECK (settle (3, 40, 0).first == 2);
    CHECK (settle (3, 120, 0).first == 4);
    CHECK (settle (1, 40, 0).first == 1);
    CHECK (settle (5, 120, 0).first == 5);

    const auto flap = settle (3, 55, 4);
    CHECK_MSG (flap.second == 0, "intensity flapped " + juce::String (flap.second) + " times at 55 +- 4");
}

LUTHIER_TEST (Jam, JM22_unsupportedMeterPlaysTheGenericBar)
{
    {
        JamBench b;
        hostBand (b);
        b.numerator = 7;
        b.denominator = 8;
        b.chord (0, kAm);
        b.runSeconds (4.0);

        JamStatus s;
        CHECK (b.engine.getStatusChannel().read (s));
        CHECK (s.genericGroove);
        CHECK (s.stepsInBar == 14);
    }

    {
        JamBench b;
        hostBand (b);
        b.numerator = 3;
        b.settings.style = 8;
        b.chord (0, kAm);
        b.runSeconds (4.0);

        JamStatus s;
        CHECK (b.engine.getStatusChannel().read (s));
        CHECK (! s.genericGroove);
        CHECK (s.stepsInBar == 12);
    }
}

LUTHIER_TEST (Jam, JM23_cycleJumpResyncs)
{
    JamBench b;
    hostBand (b);
    b.hostPpq = 32.0;   // bar 9
    b.chord (0, kAm);
    b.runSeconds (1.3);

    const auto jumpAt = b.position;
    b.hostPpq = 0.0;    // back to bar 1
    b.runSeconds (2.0);

    const JamCaptureEvent* first = nullptr;

    for (const auto& e : b.noteOns (0))
        if (e.sample >= jumpAt + kL && first == nullptr)
            first = &e;

    auto all = b.noteOns (0);
    first = nullptr;

    for (const auto& e : all)
        if (e.sample >= jumpAt + kL) { first = &e; break; }

    CHECK (first != nullptr);

    if (first != nullptr)
        CHECK_MSG (first->ppq < 1.0, "after the jump the band played ppq " + juce::String (first->ppq));

    for (const auto& e : all)
        if (e.sample >= jumpAt + kL + b.block)
            CHECK_MSG (e.ppq < 32.0, "a step from before the jump played after it");

    // Every bass note-on is ended by a note-off (nothing hangs).
    int on = 0, off = 0;

    for (const auto& e : b.events())
        if (e.part == 1)
            (e.velocity > 0 ? on : off)++;

    CHECK (on - off <= 1);
}


//==============================================================================
/*  7: the band at its default volume sits at the level of a guitar through the
    rig - under full scale, drums and bass within reach of each other. */
LUTHIER_TEST (Jam, JM07_defaultLevelSitsWithTheGuitar)
{
    for (int style = 0; style < jam::kNumFactoryStyles; ++style)
    {
        double peak[2] {}, rms[2] {};

        for (int part = 0; part < 2; ++part)
        {
            JamBench b (48000.0, 512);
            b.keepAudio = true;
            b.settings.style = style;
            b.settings.startMode = 2;
            b.settings.bassMute = part == 0;
            b.settings.drumsMute = part == 1;

            for (int i = 0; i < 8; ++i)
                b.chord (i * 48000, { 45, 48, 52 });

            b.runSeconds (10.0);

            double sum = 0.0;

            for (size_t i = 0; i < b.left.size(); ++i)
            {
                peak[part] = juce::jmax (peak[part], (double) std::abs (b.left[i]), (double) std::abs (b.right[i]));
                sum += (double) b.left[i] * b.left[i];
            }

            rms[part] = std::sqrt (sum / (double) juce::jmax ((size_t) 1, b.left.size()));
        }

        const auto name = juce::String (JamStyleLibrary::getFactoryStyleName (style));
        const double drumsPeakDb = juce::Decibels::gainToDecibels (peak[0]);
        const double gapDb = juce::Decibels::gainToDecibels (rms[0]) - juce::Decibels::gainToDecibels (rms[1]);

        CHECK_MSG (drumsPeakDb < -1.0 && drumsPeakDb > -14.0, name + ": drums peak at " + juce::String (drumsPeakDb, 1) + " dBFS");
        CHECK_MSG (peak[1] < juce::Decibels::decibelsToGain (-3.0), name + ": bass peaks at " + juce::String (juce::Decibels::gainToDecibels (peak[1]), 1) + " dBFS");
        CHECK_MSG (std::abs (gapDb) < 18.0, name + ": drums and bass are " + juce::String (gapDb, 1) + " dB apart");
    }
}
