/*  string-interaction.md 11 (REALISM-B): six strings on one guitar are not six
    instruments.

    Measured where each effect lives: the coupling matrix on its own for the
    air path, the strings' loop gains for the palm (a T60 is a loop gain), the
    per-string taps for the neighbour mute and the thump, the damping states
    for the release stagger, the pickup's aperture gains for crosstalk.
*/

#include "TestFramework.h"

#include "../Rhythm/MutedThump.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../PhysicalRange.h"

using namespace luthier;
using namespace luthier::tests;

long luthierAllocationsOnThisThread() noexcept;   // CircuitTests.cpp

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;
    const double kOpen[6] = { 329.63, 246.94, 196.0, 146.83, 110.0, 82.41 };

    std::unique_ptr<LuthierEngine> makeEngine (GuitarType type = GuitarType::Stratocaster, int block = kBlock)
    {
        auto e = std::make_unique<LuthierEngine>();
        e->prepare (kSr, block);
        e->setGuitarType (type);
        e->getCharacterEngine().setEnabled (false);
        e->getCharacterEngine().setSeed (0x5711C0DEull);
        e->getTapBuffers().setPerStringWanted (true);
        e->reset();
        return e;
    }

    NoteOnEvent note (LuthierEngine& e, int s, double fret, Technique t = Technique::Pluck, double velocity = 0.8)
    {
        NoteOnEvent n;
        n.stringIndex = s;
        n.midiNote = 40 + s;
        n.fretPosition = fret;
        n.technique = t;
        n.velocity = velocity;
        n.pitchHz = e.getTuningEngine().computeFrequency (s, fret, 0.0);
        return n;
    }

    void runBlocks (LuthierEngine& e, int blocks, std::vector<std::vector<double>>* record = nullptr)
    {
        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;

        for (int b = 0; b < blocks; ++b)
        {
            buffer.clear();
            e.processBlock (buffer, midi);

            if (record != nullptr)
                for (int s = 0; s < e.getNumStrings(); ++s)
                    for (int i = 0; i < kBlock; ++i)
                        (*record)[(size_t) s].push_back (e.getTapBuffers().stringRead (s)[i]);
        }
    }

    double t60Of (LuthierEngine& e, int s)
    {
        const auto& str = e.getString (s);
        const double g = juce::jlimit (1.0e-9, 0.999999999, str.getLoopGain());
        return -6.907755 * str.getCurrentDelaySamples() / (kSr * std::log (g));
    }

    double rmsDb (const std::vector<double>& x, double t0, double t1)
    {
        double sum = 0.0;
        const auto a = (size_t) (t0 * kSr), b = juce::jmin (x.size(), (size_t) (t1 * kSr));

        for (size_t i = a; i < b; ++i)
            sum += x[i] * x[i];

        return juce::Decibels::gainToDecibels (std::sqrt (sum / (double) juce::jmax ((size_t) 1, b - a)), -300.0);
    }

    /** The matrix alone, noise into `source`, output on `dest`; air and bridge separately. */
    double couplingRms (double bridgeAmount, double air, int source, int dest, std::vector<double>* trace = nullptr, bool impulse = false)
    {
        CouplingMatrix m;
        m.prepare (kSr, 6);
        m.setAmount (bridgeAmount);
        m.setAirCoefficient (air);

        for (int s = 0; s < 6; ++s)
            m.setStringFrequency (s, kOpen[s]);

        RtRandom r { 0xA17ull };
        double in[kMaxStrings] {}, out[kMaxStrings] {}, sum = 0.0;

        for (int i = 0; i < (int) kSr; ++i)
        {
            for (auto& v : in) v = 0.0;
            in[source] = impulse ? (i == 0 ? 1.0 : 0.0) : r.nextBipolar() * 0.1;
            m.process (in, out);
            sum += out[dest] * out[dest];

            if (trace != nullptr)
                trace->push_back (out[dest]);
        }

        return std::sqrt (sum / kSr);
    }
}

//==============================================================================
LUTHIER_TEST (StringInteraction, SI01_airMagnitudes)
{
    const double acousticAir = CouplingMatrix::airCategoryCoefficient (true, false, false);
    const double solidAir = CouplingMatrix::airCategoryCoefficient (false, false, false);

    // "Energy" is read as RMS level (the draft's 0.4-0.8 is the 0.6x of
    // section 1's coefficients; as power it would be 0.36x).
    const double outerBridge = couplingRms (1.0, 0.0, 5, 0);
    const double outerAir = couplingRms (0.0, acousticAir, 5, 0);
    const double adjacentBridge = couplingRms (1.0, 0.0, 5, 4);
    const double adjacentAir = couplingRms (0.0, acousticAir, 5, 4);
    const double solid = couplingRms (0.0, solidAir, 5, 0);

    CHECK_MSG (outerAir / outerBridge >= 0.4 && outerAir / outerBridge <= 0.8,
               "acoustic outer pair: air " + juce::String (outerAir / outerBridge, 3) + "x the bridge");
    CHECK_MSG (solid / outerBridge <= 0.05, "solid body: air " + juce::String (solid / outerBridge, 3) + "x the bridge");
    CHECK_MSG (adjacentAir / adjacentBridge <= 0.25, "adjacent pair: air " + juce::String (adjacentAir / adjacentBridge, 3) + "x");
}

LUTHIER_TEST (StringInteraction, SI02_SI03_airOffIsBitIdenticalAndDelayed)
{
    std::vector<double> plain, off;
    {
        CouplingMatrix m;
        m.prepare (kSr, 6);
        for (int s = 0; s < 6; ++s) m.setStringFrequency (s, kOpen[s]);
        RtRandom r { 3 };
        double in[kMaxStrings] {}, out[kMaxStrings] {};
        for (int i = 0; i < 20000; ++i) { in[5] = r.nextBipolar(); m.process (in, out); plain.push_back (out[0]); }
    }
    {
        CouplingMatrix m;
        m.prepare (kSr, 6);
        m.setAirCoefficient (0.003);
        m.setAirCoefficient (0.0);
        for (int s = 0; s < 6; ++s) m.setStringFrequency (s, kOpen[s]);
        RtRandom r { 3 };
        double in[kMaxStrings] {}, out[kMaxStrings] {};
        for (int i = 0; i < 20000; ++i) { in[5] = r.nextBipolar(); m.process (in, out); off.push_back (out[0]); }
    }

    CHECK_MSG (plain == off, "coupling_air_amount 0 changed the matrix's output");

    // SI-03: the air term arrives 0.29 ms after the bridge's.
    std::vector<double> bridge, air;
    couplingRms (1.0, 0.0, 5, 0, &bridge, true);
    couplingRms (0.0, 0.003, 5, 0, &air, true);

    auto firstNonZero = [] (const std::vector<double>& x)
    {
        for (size_t i = 0; i < x.size(); ++i)
            if (std::abs (x[i]) > 1.0e-15)
                return (int) i;
        return -1;
    };

    const int lag = firstNonZero (air) - firstNonZero (bridge);
    CHECK_MSG (std::abs (lag - 0.00029 * kSr) <= 1.0, "the air arrived " + juce::String (lag) + " samples after the bridge");
}

LUTHIER_TEST (StringInteraction, SI04_SI05_palmSpreadCoversAndLifts)
{
    auto t60sAfterPalm = [] (double spreadMm, std::array<double, 6>& before, std::array<double, 6>& after,
                             std::array<double, 6>& lifted, std::array<double, 6>& gainBefore, std::array<double, 6>& gainLifted)
    {
        auto e = makeEngine();
        StringInteractionSettings si;
        si.palmSpreadMm = spreadMm;
        e->setStringInteraction (si);

        for (int s = 0; s < 6; ++s)
            e->triggerNoteNow (note (*e, s, 0.0));

        runBlocks (*e, 4);

        for (int s = 0; s < 6; ++s)
        {
            before[(size_t) s] = t60Of (*e, s);
            gainBefore[(size_t) s] = e->getString (s).getLoopGain();
        }

        // CC 67 = 127 and a palm-muted strike on strings 3-5.
        e->getTechniqueEngine().setPalmMuteAmount (1.0);

        for (int s = 3; s <= 5; ++s)
            e->triggerNoteNow (note (*e, s, 0.0, Technique::PalmMute));

        runBlocks (*e, 2);

        for (int s = 0; s < 6; ++s)
            after[(size_t) s] = t60Of (*e, s);

        // CC 67 = 0: within one block.
        e->getTechniqueEngine().setPalmMuteAmount (0.0);
        runBlocks (*e, 1);

        for (int s = 0; s < 6; ++s)
        {
            lifted[(size_t) s] = t60Of (*e, s);
            gainLifted[(size_t) s] = e->getString (s).getLoopGain();
        }
    };

    std::array<double, 6> before {}, after {}, lifted {}, gainBefore {}, gainLifted {};
    t60sAfterPalm (35.0, before, after, lifted, gainBefore, gainLifted);

    CHECK_MSG (before[2] / after[2] >= 2.5, "string 2's T60 fell only " + juce::String (before[2] / after[2], 2) + "x");

    for (int s : { 0, 1 })
        CHECK_MSG (std::abs (after[(size_t) s] / before[(size_t) s] - 1.0) <= 0.10,
                   "string " + juce::String (s) + " changed " + juce::String (after[(size_t) s] / before[(size_t) s], 3) + "x");

    // SI-05: string 2 is back where it was.
    CHECK_MSG (std::abs (gainLifted[2] - gainBefore[2]) <= 1.0e-9,
               "string 2's loop gain after the lift is off by " + juce::String (gainLifted[2] - gainBefore[2], 12));

    // At 20 mm string 2 is not under the palm.
    t60sAfterPalm (20.0, before, after, lifted, gainBefore, gainLifted);
    CHECK_MSG (std::abs (after[2] / before[2] - 1.0) <= 0.10, "at 20 mm string 2 changed " + juce::String (after[2] / before[2], 3) + "x");
}

LUTHIER_TEST (StringInteraction, SI06_SI07_neighbourMute)
{
    // Open B and high E ringing; at 0.3 s the G is fretted at 5.
    auto render = [] (bool frettedG, double amount, double style)
    {
        auto e = makeEngine();
        StringInteractionSettings si;
        si.adjacentMute = amount;
        si.frettingStyle = style;
        e->setStringInteraction (si);

        std::vector<std::vector<double>> rec (6);
        e->triggerNoteNow (note (*e, 1, 0.0));
        e->triggerNoteNow (note (*e, 0, 0.0));

        // Ringing, not held: let go under the sustain pedal (3: only a
        // string with no held note is muted by its neighbour's finger).
        for (int s : { 0, 1 })
        {
            NoteOffEvent off;
            off.stringIndex = s;
            off.letRing = true;
            e->releaseNoteNow (off);
        }

        runBlocks (*e, (int) (0.3 * kSr / kBlock), &rec);

        if (frettedG)
            e->triggerNoteNow (note (*e, 2, 5.0));

        runBlocks (*e, (int) (0.3 * kSr / kBlock), &rec);
        return rec;
    };

    const double at = (double) ((int) (0.3 * kSr / kBlock) * kBlock) / kSr;
    const auto quiet = render (false, 0.6, 1.0);
    const auto rock = render (true, 0.6, 1.0);
    const auto classical = render (true, 0.6, 0.1);

    const double bDrop = rmsDb (quiet[1], at + 0.13, at + 0.15) - rmsDb (rock[1], at + 0.13, at + 0.15);
    const double eChange = std::abs (rmsDb (quiet[0], at + 0.13, at + 0.15) - rmsDb (rock[0], at + 0.13, at + 0.15));
    const double classicalDrop = rmsDb (quiet[1], at + 0.13, at + 0.15) - rmsDb (classical[1], at + 0.13, at + 0.15);

    CHECK_MSG (bDrop >= 10.0, "rock: the B dropped only " + juce::String (bDrop, 1) + " dB");
    CHECK_MSG (eChange < 0.5, "rock: the high E changed " + juce::String (eChange, 2) + " dB");
    CHECK_MSG (classicalDrop <= 2.0, "classical: the B dropped " + juce::String (classicalDrop, 1) + " dB");

    // A chord including the B: its own note overrides the mute.
    auto chord = [] (double amount)
    {
        auto e = makeEngine();
        StringInteractionSettings si;
        si.adjacentMute = amount;
        e->setStringInteraction (si);

        std::vector<std::vector<double>> rec (6);
        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;

        // D fret 2, G fret 2, B fret 1: struck bass to treble, 6 ms apart.
        e->triggerNoteNow (note (*e, 3, 2.0));
        runBlocks (*e, 1, &rec);
        e->triggerNoteNow (note (*e, 2, 2.0));
        runBlocks (*e, 1, &rec);
        e->triggerNoteNow (note (*e, 1, 1.0));
        runBlocks (*e, 60, &rec);
        return rmsDb (rec[1], 0.1, 0.3);
    };

    const double withMute = chord (0.6), without = chord (0.0);
    CHECK_MSG (std::abs (withMute - without) <= 0.5, "the chord's B moved " + juce::String (withMute - without, 2) + " dB");
}

LUTHIER_TEST (StringInteraction, SI08_SI09_releaseStagger)
{
    /*  Six notes held, then six note-offs on one sample; single-sample blocks
        so each string's release is seen on the sample it lands. */
    auto onsets = [] (double staggerMs, double bias)
    {
        auto e = makeEngine (GuitarType::Stratocaster, kBlock);
        StringInteractionSettings si;
        si.releaseStaggerMs = staggerMs;
        si.releaseStaggerBias = bias;
        e->setStringInteraction (si);

        e->getMidiInterpreter().setPlayingMode (PlayingMode::GuitarController);

        juce::AudioBuffer<float> one (2, 1);
        auto play = [&] (const juce::MidiBuffer& m) { one.clear(); auto mm = m; e->processBlock (one, mm); };

        juce::MidiBuffer on;
        const int keys[6] = { 64, 59, 55, 50, 45, 40 };

        for (int s = 0; s < 6; ++s)
            on.addEvent (juce::MidiMessage::noteOn (s + 1, keys[s], (juce::uint8) 100), 0);

        play (on);

        for (int i = 0; i < 4800; ++i)
            play ({});

        juce::MidiBuffer off;

        for (int s = 0; s < 6; ++s)
            off.addEvent (juce::MidiMessage::noteOff (s + 1, keys[s]), 0);

        std::array<int, 6> onset {};
        onset.fill (-1);
        play (off);

        for (int i = 0; i < 2400; ++i)
        {
            for (int s = 0; s < 6; ++s)
                if (onset[(size_t) s] < 0 && e->getString (s).getDamping() == StringEngine::Damping::Released)
                    onset[(size_t) s] = i;

            play ({});
        }

        return onset;
    };

    const auto a = onsets (12.0, 0.0), b = onsets (12.0, 0.0), none = onsets (0.0, 0.0);

    int lo = 1 << 30, hi = -1;

    for (int s = 0; s < 6; ++s)
    {
        CHECK_MSG (a[(size_t) s] >= 0, "string " + juce::String (s) + " never released");
        lo = juce::jmin (lo, a[(size_t) s]);
        hi = juce::jmax (hi, a[(size_t) s]);
        CHECK_MSG (none[(size_t) s] == 0, "at 0 ms string " + juce::String (s) + " released at " + juce::String (none[(size_t) s]));
    }

    const double spreadMs = (hi - lo) * 1000.0 / kSr;
    CHECK_MSG (spreadMs >= 4.0 && spreadMs <= 12.0, "the release spread " + juce::String (spreadMs, 2) + " ms");
    CHECK_MSG (lo >= 0, "a release preceded its note-off");
    CHECK_MSG (a == b, "two renders released differently");

    // SI-09.
    const auto treble = onsets (12.0, 1.0), bass = onsets (12.0, -1.0);

    for (int s = 1; s < 6; ++s)
    {
        CHECK_MSG (treble[(size_t) s] > treble[(size_t) s - 1], "bias +1 is not strictly treble first at string " + juce::String (s));
        CHECK_MSG (bass[(size_t) s] < bass[(size_t) s - 1], "bias -1 is not strictly bass first at string " + juce::String (s));
    }
}

LUTHIER_TEST (StringInteraction, SI10_crosstalk)
{
    auto gainDb = [] (PickupType type, double positionMm, double bendCents)
    {
        PickupEngine p;
        p.prepare (kSr, 6);
        p.setNumPickups (1);
        p.setPickupSpec (0, PickupSpec::makeDefault (type, positionMm / 648.0));

        double mm[kMaxStrings] {}, stop[kMaxStrings] {};
        bool bassward[kMaxStrings] {};

        for (int s = 0; s < 6; ++s)
        {
            stop[s] = 648.0;
            bassward[s] = s <= 3;
        }

        mm[2] = lateralMmForBend (bendCents);
        stop[2] = 648.0 * std::pow (2.0, -7.0 / 12.0);
        p.setStringLateralOffsets (mm, stop, bassward, 6, 648.0, 10.5, 1.0);
        return juce::Decibels::gainToDecibels (p.getApertureGain (0, 2));
    };

    // Unbent: exactly 1.
    {
        PickupEngine p;
        p.prepare (kSr, 6);
        double mm[kMaxStrings] {}, stop[kMaxStrings] {};
        bool bassward[kMaxStrings] {};
        for (auto& v : stop) v = 648.0;
        p.setStringLateralOffsets (mm, stop, bassward, 6, 648.0, 10.5, 1.0);

        for (int s = 0; s < 6; ++s)
            CHECK (p.getApertureGain (0, s) == 1.0);
    }

    const double neckSingle = -gainDb (PickupType::SingleCoil, 160.0, 200.0);
    const double bridgeSingle = -gainDb (PickupType::SingleCoil, 40.0, 200.0);
    const double neckHumbucker = -gainDb (PickupType::Humbucker, 160.0, 200.0);

    CHECK_MSG (neckSingle >= 0.5 && neckSingle <= 2.0, "neck single coil drops " + juce::String (neckSingle, 2) + " dB");
    CHECK_MSG (bridgeSingle < 0.4, "bridge single coil drops " + juce::String (bridgeSingle, 2) + " dB");
    CHECK_MSG (neckHumbucker < neckSingle, "a humbucker's wider aperture should forgive more");
}

LUTHIER_TEST (StringInteraction, SI11_mutedStringThump)
{
    // x32010 strummed down: the low E is in the STRUM mask but muted.
    std::array<StrumStrike, kMaxStrings> strikes {};
    std::array<int, 5> order { 4, 3, 2, 1, 0 };
    StrumRequest request;
    request.strings = order.data();
    request.numStrings = 5;
    request.down = true;
    request.sourceSps = 200.0;
    request.missScale = 0.0;
    StrumGesture gesture;
    const int planned = gesture.plan (StrumSettings::guitarDefaults(), request, strikes.data(), (int) strikes.size());

    std::array<MutedThump, kMaxStrings> thumps {};
    CHECK (planMutedThumps (strikes.data(), planned, 1u << 5, thumps.data(), kMaxStrings, true) == 0);   // live: outside the span
    const int numThumps = planMutedThumps (strikes.data(), planned, 1u << 5, thumps.data(), kMaxStrings, false);
    CHECK (numThumps == 1 && thumps[0].stringIndex == 5);

    auto render = [&] (double level, bool& reported)
    {
        auto e = makeEngine();
        std::vector<std::vector<double>> rec (6);
        const double frets[6] = { 0, 1, 0, 2, 3, 0 };
        juce::AudioBuffer<float> one (2, 1);
        juce::MidiBuffer midi;
        reported = false;

        for (int i = 0; i < (int) (0.3 * kSr); ++i)
        {
            for (int k = 0; k < planned; ++k)
                if ((int) std::round (strikes[(size_t) k].timeSeconds * kSr) == i)
                    e->triggerNoteNow (note (*e, strikes[(size_t) k].stringIndex, frets[strikes[(size_t) k].stringIndex]));

            if (level > 0.0 && numThumps > 0 && (int) std::round (thumps[0].timeSeconds * kSr) == i)
                e->triggerNoteNow (makeThumpEvent (5, 0.8 * thumps[0].force, level,
                                                   e->getTuningEngine().computeFrequency (5, 3.0, 0.0), 0, -1));

            one.clear();
            e->processBlock (one, midi);

            for (int k = 0; k < e->getStringActivity().size(); ++k)
                reported = reported || e->getStringActivity()[k].stringIndex == 5;

            for (int s = 0; s < 6; ++s)
                rec[(size_t) s].push_back (e->getTapBuffers().stringRead (s)[0]);
        }

        return rec;
    };

    bool reported = true, reportedOff = true;
    const auto with = render (0.5, reported);
    const auto without = render (0.0, reportedOff);

    double loudest = 0.0, thumpPeak = 0.0;

    for (int s = 0; s < 5; ++s)
        for (double v : with[(size_t) s])
            loudest = juce::jmax (loudest, std::abs (v));

    for (double v : with[5])
        thumpPeak = juce::jmax (thumpPeak, std::abs (v));

    const double under = juce::Decibels::gainToDecibels (loudest / juce::jmax (1.0e-12, thumpPeak));
    CHECK_MSG (under >= 8.0 && under <= 20.0, "the thump is " + juce::String (under, 1) + " dB under the loudest string");
    CHECK_MSG (! reported, "the thump was reported as a note");

    // Pitchless: no autocorrelation peak above 0.3 at lags 2-25 ms.
    {
        const auto& x = with[5];
        size_t start = 0;
        while (start < x.size() && std::abs (x[start]) < thumpPeak * 0.01) ++start;
        const size_t n = (size_t) (0.06 * kSr);
        double r0 = 0.0;
        for (size_t i = start; i < start + n && i < x.size(); ++i) r0 += x[i] * x[i];
        double worst = 0.0;

        for (int lag = (int) (0.002 * kSr); lag <= (int) (0.025 * kSr); ++lag)
        {
            double r = 0.0;
            for (size_t i = start; i + (size_t) lag < start + n && i + (size_t) lag < x.size(); ++i) r += x[i] * x[i + (size_t) lag];
            worst = juce::jmax (worst, r / juce::jmax (1.0e-30, r0));
        }

        CHECK_MSG (worst <= 0.3, "the thump's autocorrelation reaches " + juce::String (worst, 3));

        // 40 dB down within 60 ms.
        double tail = 0.0;
        for (size_t i = start + (size_t) (0.055 * kSr); i < start + (size_t) (0.06 * kSr) && i < x.size(); ++i)
            tail = juce::jmax (tail, std::abs (x[i]));
        CHECK_MSG (juce::Decibels::gainToDecibels (tail / thumpPeak) <= -40.0,
                   "60 ms on the thump is at " + juce::String (juce::Decibels::gainToDecibels (tail / thumpPeak), 1) + " dB");
    }

    // Level 0: the interpreter and the rhythm engine emit nothing extra.
    {
        auto e = makeEngine();
        e->getMidiInterpreter().setMutedThumpLevel (0.0);
        juce::MidiBuffer midi;
        for (int k : { 48, 52, 55, 60, 64 })
            midi.addEvent (juce::MidiMessage::noteOn (1, k, (juce::uint8) 100), 0);
        PlayEventQueue q;
        e->getMidiInterpreter().processBlock (midi, kBlock, 0, q);
        juce::MidiBuffer later;
        e->getMidiInterpreter().processBlock (later, kBlock, kBlock, q);

        for (int i = 0; i < q.getNumNoteOns(); ++i)
            CHECK (! q.getNoteOn (i).deadStrike);
    }
}

LUTHIER_TEST (StringInteraction, SI13_realtime)
{
    auto e = makeEngine (GuitarType::TwelveString);
    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;
    e->processBlock (buffer, midi);

    const long before = luthierAllocationsOnThisThread();

    for (int b = 0; b < 200; ++b)
    {
        if (b % 20 == 0)
        {
            for (int s = 0; s < 12; ++s)
                e->triggerNoteNow (note (*e, s, (double) (s % 5), s % 3 == 0 ? Technique::PalmMute : Technique::Pluck));

            e->getTechniqueEngine().setPalmMuteAmount (b % 40 == 0 ? 1.0 : 0.0);
        }

        buffer.clear();
        e->processBlock (buffer, midi);
    }

    CHECK_MSG (luthierAllocationsOnThisThread() == before, "string interaction allocated on the audio thread");

    // Cost at 12 strings: the air path is the only per-sample part.
    auto seconds = [] (double air)
    {
        CouplingMatrix m;
        m.prepare (kSr, 12);
        m.setAirCoefficient (air);
        double in[kMaxStrings] {}, out[kMaxStrings] {};
        double best = 1.0e9;

        for (int run = 0; run < 5; ++run)
        {
            const auto start = juce::Time::getHighResolutionTicks();
            for (int i = 0; i < (int) kSr; ++i) { in[i % 12] = 0.001; m.process (in, out); }
            best = juce::jmin (best, juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start));
        }

        return best;
    };

    /*  DECISION (REALISM-B): 0.2 units at 12 strings (0.1 at six). The air
        term needs each string's own 120 Hz high-pass to subtract itself
        exactly; filtering only the sum would feed a low E 0.3 % of itself
        back through the loop, which is a gain change, not a coupling.
        string-interaction.md 10 records the measured figure. */
    const double units = 100.0 * juce::jmax (0.0, seconds (0.003) - seconds (0.0));
    CHECK_MSG (units <= 0.2, "the air path costs " + juce::String (units, 3) + " units at 12 strings");
}

LUTHIER_TEST (StringInteraction, SI12_SI14_rangesAndPresets)
{
    juce::ignoreUnused (Parameters::createLayout());

    CHECK (RangeRegistry::find (ParamIDs::palmMuteSpread) != nullptr && RangeRegistry::find (ParamIDs::palmMuteSpread)->family == RangeFamily::pick);
    CHECK (RangeRegistry::find (ParamIDs::adjacentMuteAmount) != nullptr && RangeRegistry::find (ParamIDs::adjacentMuteAmount)->family == RangeFamily::squeak);
    CHECK (RangeRegistry::find (ParamIDs::pickupApertureScale) != nullptr && RangeRegistry::find (ParamIDs::pickupApertureScale)->family == RangeFamily::circuit);

    for (const char* id : { ParamIDs::palmMuteSpread, ParamIDs::adjacentMuteAmount, ParamIDs::pickupApertureScale })
        CHECK (RangeRegistry::find (id)->isValid());

    // Gesture timings and amounts are not physical.
    for (const char* id : { ParamIDs::couplingAirAmount, ParamIDs::releaseStaggerMs, ParamIDs::releaseStaggerBias, ParamIDs::mutedThumpLevel })
        CHECK (RangeRegistry::find (id) == nullptr);

    CHECK (RangeRegistry::findDeclarationMismatches().isEmpty());

    const char* ids[] = { ParamIDs::couplingAirAmount, ParamIDs::palmMuteSpread, ParamIDs::adjacentMuteAmount,
                          ParamIDs::releaseStaggerMs, ParamIDs::releaseStaggerBias, ParamIDs::pickupApertureScale,
                          ParamIDs::mutedThumpLevel };

    auto from = std::make_unique<LuthierAudioProcessor>();
    auto to = std::make_unique<LuthierAudioProcessor>();
    std::vector<float> written;

    for (auto* id : ids)
    {
        auto* p = from->getState().getParameter (id);
        CHECK_MSG (p != nullptr, juce::String ("no parameter ") + id);
        const float target = p != nullptr ? p->convertTo0to1 (p->convertFrom0to1 (p->getDefaultValue() < 0.5f ? 0.8f : 0.2f)) : 0.0f;
        if (p != nullptr) p->setValueNotifyingHost (target);
        written.push_back (p != nullptr ? p->getValue() : 0.0f);
    }

    to->getPresetManager().fromVar (from->getPresetManager().toVar ("interaction round trip"));

    for (size_t i = 0; i < std::size (ids); ++i)
        if (auto* p = to->getState().getParameter (ids[i]))
            CHECK_MSG (std::abs (p->getValue() - written[i]) < 1.0e-4f, juce::String (ids[i]) + " did not round-trip");
}
