/*  performance-budget.md 1, 3, 5, 6, 7 and 10: boot time, load and swap
    timings, memory, the oversampling downgrade, and (under LUTHIER_PERF=1) the
    CPU-unit budgets and scaling curves.

    Every timing here is machine-dependent: the budgets are the spec's for a
    "mid CPU", and a failure message says what was measured so a slow runner
    can be told apart from a regression. The CPU-unit tests are gated because a
    unit is a percentage of whatever core runs them (see the QA report).
*/

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Presets/FactoryPresets.h"
#include "../Support/IrLibrary.h"
#include "../Support/MemoryProbe.h"
#include "../Tune/TuneFile.h"
#include "../Workshop/SpectrumDelta.h"
#include "../DSP/Amp/AmpEngine.h"
#include "../DSP/Amp/CabinetEngine.h"
#include "../DSP/Amp/RoomEngine.h"
#include "../DSP/Body/BodyEngine.h"
#include "../DSP/Circuit/GuitarCircuit.h"
#include "../DSP/Coupling/CouplingMatrix.h"
#include "../DSP/Master/MasterBus.h"
#include "../DSP/Effects/EffectsChain.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;

    double msSince (double startMs) { return juce::Time::getMillisecondCounterHiRes() - startMs; }

    double percentile (std::vector<double> v, double p)
    {
        if (v.empty())
            return 0.0;

        std::sort (v.begin(), v.end());
        return v[(size_t) juce::jlimit (0, (int) v.size() - 1, (int) std::ceil (p * (double) v.size()) - 1)];
    }

    int presetIndex (const char* name)
    {
        for (int i = 0; i < FactoryPresets::getNumPresets(); ++i)
            if (juce::String (FactoryPresets::getPreset (i).name) == name)
                return i;

        return -1;
    }

    /** Held notes, retriggered each second: `voices` strings of an open chord. */
    void renderNotes (LuthierAudioProcessor& p, int voices, double seconds, int block = 128)
    {
        static constexpr int chord[] = { 40, 45, 50, 55, 59, 64, 43, 47, 52, 57, 62, 67 };
        juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), block);
        juce::MidiBuffer midi;
        const int blocks = (int) (seconds * kSr / block);
        const int perSecond = (int) (kSr / block);

        for (int b = 0; b < blocks; ++b)
        {
            midi.clear();

            if (b % perSecond == 0)
                for (int v = 0; v < voices; ++v)
                    midi.addEvent (juce::MidiMessage::noteOn (1 + v % 12, chord[v % 12], (juce::uint8) 100), v);

            buffer.clear();
            p.processBlock (buffer, midi);
        }
    }

    bool loadFactory (LuthierAudioProcessor& p, const char* name)
    {
        const int index = presetIndex (name);

        if (index < 0)
            return false;

        FactoryPresets::setProcessorForRanges (&p);
        const bool ok = p.getPresetManager().fromVar (FactoryPresets::toVar (FactoryPresets::getPreset (index), p));
        p.getParameterBridge().applyAllNow();
        return ok;
    }

    void setParam (LuthierAudioProcessor& p, const char* id, float plain)
    {
        if (auto* param = p.getState().getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (plain));
    }
}

//==============================================================================
/*  PB-5.1 / QA-2.4: construction + prepareToPlay + the first block. Cold is
    the first in the process with the resource search forgotten; warm is the
    next. Best of three for each (DECISIONS: timing tests take the best, as
    noise only adds). */
LUTHIER_TEST (Boot, coldAndWarmInstantiationStayInBudget)
{
    auto instantiate = []
    {
        const double start = juce::Time::getMillisecondCounterHiRes();
        auto p = std::make_unique<LuthierAudioProcessor>();
        p->prepareToPlay (kSr, 128);

        juce::AudioBuffer<float> buffer (p->getTotalNumOutputChannels(), 128);
        juce::MidiBuffer midi;
        buffer.clear();
        p->processBlock (buffer, midi);
        return msSince (start);
    };

    double cold = 1.0e9, warm = 1.0e9;

    for (int i = 0; i < 3; ++i)
    {
        IrLibrary::forgetResourcesFolder();
        cold = juce::jmin (cold, instantiate());
        warm = juce::jmin (warm, instantiate());
    }

    /*  The spec's numbers are for a "mid CPU". Under LUTHIER_PERF=1 (a quiet
        machine) they are asserted as written; the default run, on a shared
        CI runner, holds 1.5x of them as a regression bar. Measured on the
        4-vCPU development container: warm 225-255 ms, of which the factory
        guitar's parts load is ~80 ms and the two cabinet IR installs ~50 ms
        (QA report, Decisions). */
    const double slack = perfRunRequested() ? 1.0 : 1.5;
    const auto said = " (cold " + juce::String (cold, 1) + " ms, warm " + juce::String (warm, 1) + " ms)";

    CHECK_MSG (cold <= 400.0 * slack, "cold instantiation over budget: 400 ms x " + juce::String (slack, 1) + said);
    CHECK_MSG (warm <= 200.0 * slack, "warm instantiation over budget: 200 ms x " + juce::String (slack, 1) + said);
}

//==============================================================================
/*  PB-5.3 - 5.5, PB-10.9 - 10.11: guitar load, tune load, part swap, audition
    and spectrum delta, each over many events. The 95th percentile carries the
    budget (a shared runner's scheduler owns the worst case); the worst is
    reported in the message. */
namespace
{
    struct WorkshopRig
    {
        std::unique_ptr<LuthierAudioProcessor> processor = std::make_unique<LuthierAudioProcessor>();

        WorkshopRig()
        {
            processor->prepareToPlay (kSr, 512);
            setParam (*processor, ParamIDs::guitarType, (float) GuitarType::LesPaul);
            processor->getParameterBridge().applyAllNow();
        }
    };

    juce::String describe (const std::vector<double>& t)
    {
        return "p95 " + juce::String (percentile (t, 0.95), 1) + " ms, worst "
                 + juce::String (percentile (t, 1.0), 1) + " ms over " + juce::String ((int) t.size());
    }
}

LUTHIER_TEST (Workshop, hundredRandomPartSwapsStayUnder50ms)
{
    WorkshopRig rig;
    auto& bench = rig.processor->getBench();
    auto& library = rig.processor->getPartLibrary();
    juce::Random rng (0x5A4B);
    std::vector<double> times;

    static constexpr GuitarSlot slots[] = { GuitarSlot::bridge, GuitarSlot::pickupNeck, GuitarSlot::pickupBridge,
                                            GuitarSlot::neck, GuitarSlot::body };

    for (int i = 0; i < 100; ++i)
    {
        const auto slot = slots[rng.nextInt ((int) std::size (slots))];
        const auto parts = library.getParts (getSlotPartType (slot));

        if (parts.isEmpty())
            continue;

        const auto part = parts[rng.nextInt (parts.size())];
        const double start = juce::Time::getMillisecondCounterHiRes();
        bench.fit (slot, part);
        times.push_back (msSince (start));
    }

    CHECK (times.size() >= 50);
    CHECK_MSG (percentile (times, 0.95) <= 50.0, "part swap " + describe (times) + " (budget 50 ms)");
}

LUTHIER_TEST (WorkshopBench, hundredAuditionsStayInBudget)
{
    WorkshopRig rig;
    auto& bench = rig.processor->getBench();
    const auto parts = rig.processor->getPartLibrary().getParts (PartType::bridge);
    std::vector<double> times;

    for (int i = 0; i < 100 && ! parts.isEmpty(); ++i)
    {
        const double start = juce::Time::getMillisecondCounterHiRes();
        bench.beginAudition (GuitarSlot::bridge, parts[i % parts.size()]);
        times.push_back (msSince (start));
        bench.endAudition();
    }

    CHECK (times.size() == 100);
    CHECK_MSG (percentile (times, 0.95) <= 50.0, "Alt-hover audition " + describe (times) + " (budget 50 ms, the swap's)");
}

LUTHIER_TEST (Workshop, guitarLoadStaysUnder300ms)
{
    WorkshopRig rig;
    std::vector<double> times;

    for (int round = 0; round < 2; ++round)
        for (auto type : { GuitarType::Stratocaster, GuitarType::LesPaul, GuitarType::Telecaster,
                           GuitarType::JazzBass, GuitarType::Dreadnought })
        {
            // The parameter route a user's guitar pick takes (0.6: a type is a
            // factory guitar file), timed through the structural apply.
            setParam (*rig.processor, ParamIDs::guitarType, (float) type);
            const double start = juce::Time::getMillisecondCounterHiRes();
            rig.processor->getParameterBridge().applyAllNow();
            times.push_back (msSince (start));
        }

    CHECK_MSG (percentile (times, 0.95) <= 300.0, "guitar load " + describe (times) + " (budget 300 ms)");
}

LUTHIER_TEST (Tune, loadStaysUnder100ms)
{
    const auto folder = IrLibrary::getResourcesFolder().getChildFile ("Tunes");
    std::vector<double> times;

    for (int round = 0; round < 3; ++round)
        for (const auto& entry : juce::RangedDirectoryIterator (folder, true, "*.luthiertune"))
        {
            Tune tune;
            const double start = juce::Time::getMillisecondCounterHiRes();
            const auto result = TuneFile::load (entry.getFile(), tune);
            times.push_back (msSince (start));
            CHECK_MSG (result.error == TuneLoadError::none, entry.getFile().getFileName() + ": " + result.message);
        }

    CHECK (! times.empty());
    CHECK_MSG (percentile (times, 0.95) <= 100.0, "tune load " + describe (times) + " (budget 100 ms)");
}

LUTHIER_TEST (WorkshopSpectrum, hundredShadowRendersStayUnder40ms)
{
    WorkshopRig rig;
    SpectrumDelta delta;
    const auto committed = rig.processor->getCurrentGuitar();
    const auto parts = rig.processor->getPartLibrary().getParts (PartType::bridge);
    std::vector<double> times;

    for (int i = 0; i < 100 && ! parts.isEmpty(); ++i)
    {
        const auto id = delta.request (committed, rig.processor->getBench().withPart (GuitarSlot::bridge, parts[i % parts.size()]),
                                       GuitarType::LesPaul);
        SpectrumDelta::Result r;
        const auto until = juce::Time::getMillisecondCounter() + 10000;

        while (juce::Time::getMillisecondCounter() < until)
        {
            if (delta.takeResult (r) && r.requestId == id)
                break;

            juce::Thread::sleep (1);
        }

        if (r.requestId == id)
            times.push_back (r.renderMs);
    }

    CHECK_MSG (times.size() == 100, juce::String ((int) times.size()) + " of 100 renders came back");
    CHECK_MSG (percentile (times, 0.95) < 40.0, "spectrum delta " + describe (times) + " (budget 40 ms)");
}

//==============================================================================
/*  PB-7.2: above 96 kHz the oversampled modules downgrade (4x -> 2x -> 1x). */
LUTHIER_TEST (Engine, oversamplingDowngradesAbove96k)
{
    struct Case { double rate; int expected; };

    for (const auto& c : { Case { 44100.0, 4 }, Case { 48000.0, 4 }, Case { 96000.0, 4 },
                           Case { 176400.0, 2 }, Case { 192000.0, 1 } })
    {
        LuthierEngine engine;
        engine.setOversamplingFactor (4);
        engine.prepare (c.rate, 256);

        CHECK_MSG (engine.getEffectiveOversamplingFactor() == c.expected,
                   juce::String (c.rate) + " Hz runs at " + juce::String (engine.getEffectiveOversamplingFactor()) + "x");
        CHECK_NEAR (engine.getAmpEngine().getOversampledRate(), c.rate * c.expected, 1.0);
        CHECK (engine.getOversamplingFactor() == 4);   // the user's choice is kept

        // Changing the factor at a high rate keeps the downgrade.
        engine.setOversamplingFactor (8);
        CHECK (engine.getEffectiveOversamplingFactor() == LuthierEngine::effectiveOversamplingFactor (8, c.rate));
    }

    CHECK (LuthierEngine::effectiveOversamplingFactor (1, 192000.0) == 1);
    CHECK (LuthierEngine::effectiveOversamplingFactor (8, 192000.0) == 2);
}

//==============================================================================
/*  PB-3.1 / QA-2.5: a baseline instance - one processor, prepared, one block -
    adds no more than 350 MB of resident memory. Measured as the growth, since
    the test runner itself (and every test before this one) is resident too;
    the absolute RSS is in the message. */
LUTHIER_TEST (Memory, baselineInstanceUnder350MB)
{
    const auto before = MemoryProbe::residentBytes();

    auto p = std::make_unique<LuthierAudioProcessor>();
    p->prepareToPlay (kSr, 128);
    juce::AudioBuffer<float> buffer (p->getTotalNumOutputChannels(), 128);
    juce::MidiBuffer midi;
    buffer.clear();
    p->processBlock (buffer, midi);

    const auto after = MemoryProbe::residentBytes();

   #if JUCE_LINUX || JUCE_WINDOWS || JUCE_MAC
    CHECK (after > 0);
   #endif

    const double grownMB = MemoryProbe::toMB (after > before ? after - before : 0);
    CHECK_MSG (grownMB <= 350.0, "one instance added " + juce::String (grownMB, 1) + " MB (RSS now "
                                   + juce::String (MemoryProbe::toMB (after), 1) + " MB; budget 350)");
}

/*  PB-10.3: sixty minutes, sampled every ten, no monotonic growth (under 8 MB
    across the session) and a peak under the 900 MB cap. An hour of audio: it
    runs under LUTHIER_PERF=1 only. */
LUTHIER_TEST (Memory, sixtyMinuteSessionNoMonotonicGrowth)
{
    if (! perfRunRequested())
        return;

    auto p = std::make_unique<LuthierAudioProcessor>();
    p->prepareToPlay (kSr, 256);
    loadFactory (*p, "Single-Cut Crunch");
    renderNotes (*p, 4, 60.0, 256);                 // settle

    std::vector<double> samples;
    samples.push_back (MemoryProbe::toMB (MemoryProbe::residentBytes()));

    for (int tenMinutes = 0; tenMinutes < 6; ++tenMinutes)
    {
        renderNotes (*p, 4 + tenMinutes % 3, 600.0, 256);
        samples.push_back (MemoryProbe::toMB (MemoryProbe::residentBytes()));
    }

    bool monotonic = true;

    for (size_t i = 1; i < samples.size(); ++i)
        monotonic = monotonic && samples[i] > samples[i - 1];

    CHECK_MSG (! (monotonic && samples.back() - samples.front() >= 8.0),
               "RSS grew every ten minutes, " + juce::String (samples.front(), 1) + " -> " + juce::String (samples.back(), 1) + " MB");
    CHECK_MSG (samples.back() - samples.front() < 8.0,
               "RSS grew " + juce::String (samples.back() - samples.front(), 1) + " MB across the hour");
    CHECK (MemoryProbe::toMB (MemoryProbe::peakResidentBytes()) <= 900.0);
}

//==============================================================================
/*  PB-1.1 / PB-10.1: per-module cost at 48 kHz / 128, each module alone, held
    to budget x 1.10. LUTHIER_PERF=1 only: units are machine-relative. */
LUTHIER_TEST (PerfBudget, everyModuleWithinBudget)
{
    if (! perfRunRequested())
        return;

    constexpr int kBlock = 128;
    constexpr double kSeconds = 10.0;
    const int blocks = (int) (kSeconds * kSr / kBlock);
    juce::Random rng (7);
    std::vector<double> input ((size_t) kBlock);

    for (auto& x : input)
        x = rng.nextDouble() * 0.2 - 0.1;

    auto check = [&ctx] (const char* name, double units, double budget)
    {
        std::printf ("    %-24s %6.3f units (budget %.2f)\n", name, units, budget);
        CHECK_MSG (units <= budget * 1.10, juce::String (name) + ": " + juce::String (units, 3) + " units, budget "
                                             + juce::String (budget, 2));
    };

    {
        CouplingMatrix coupling;
        coupling.prepare (kSr, 12);
        double in[12] {}, out[12] {};
        check ("CouplingMatrix", cpuUnits ([&] { for (int i = 0; i < blocks * kBlock; ++i) { in[i % 12] = input[(size_t) (i % kBlock)]; coupling.process (in, out); } }, kSeconds), 0.4);
    }
    {
        GuitarCircuit circuit;
        circuit.prepare (kSr);
        double sink = 0.0;
        check ("GuitarCircuit", cpuUnits ([&] { for (int i = 0; i < blocks * kBlock; ++i) sink += circuit.process (input[(size_t) (i % kBlock)]); }, kSeconds), 0.05);
        juce::ignoreUnused (sink);
    }
    {
        AmpEngine amp;
        amp.prepare (kSr, kBlock);
        double sink = 0.0;
        check ("AmpEngine", cpuUnits ([&] { for (int i = 0; i < blocks * kBlock; ++i) sink += amp.processSample (input[(size_t) (i % kBlock)]); }, kSeconds), 1.5);
        juce::ignoreUnused (sink);
    }

    juce::AudioBuffer<float> stereo (2, kBlock);
    auto fill = [&] { for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < kBlock; ++i) stereo.setSample (ch, i, (float) input[(size_t) i]); };

    {
        BodyEngine body;
        body.prepare (kSr, kBlock);
        std::vector<double> mono ((size_t) kBlock);
        check ("BodyEngine", cpuUnits ([&] { for (int b = 0; b < blocks; ++b) { std::copy (input.begin(), input.end(), mono.begin()); body.processMono (mono.data(), kBlock); } }, kSeconds), 0.6);
    }
    {
        CabinetEngine cab;
        cab.prepare (kSr, kBlock);
        check ("CabinetEngine", cpuUnits ([&] { for (int b = 0; b < blocks; ++b) { fill(); cab.processBlock (stereo); } }, kSeconds), 0.4);
    }
    {
        RoomEngine room;
        room.prepare (kSr, kBlock);
        check ("RoomEngine", cpuUnits ([&] { for (int b = 0; b < blocks; ++b) { fill(); room.processBlock (stereo); } }, kSeconds), 0.3);
    }
    {
        MasterBus master;
        master.prepare (kSr, kBlock);
        check ("MasterBus", cpuUnits ([&] { for (int b = 0; b < blocks; ++b) { fill(); master.processBlock (stereo); } }, kSeconds), 0.15);
    }
    {
        // Eight slots at the chain's average: section 1 budgets the chain.
        EffectsChain chain;
        chain.prepare (kSr, kBlock);
        const PedalType types[] = { PedalType::Compressor, PedalType::Wah, PedalType::Overdrive, PedalType::Distortion,
                                    PedalType::Boost, PedalType::NoiseGate, PedalType::VolumePedal, PedalType::Fuzz };

        for (int s = 0; s < EffectsChain::kNumSlots; ++s)
            chain.setSlotType (s, types[s]);

        std::vector<double> l ((size_t) kBlock), r ((size_t) kBlock);
        check ("PreEffectsChain (8)", cpuUnits ([&] { for (int b = 0; b < blocks; ++b) { std::copy (input.begin(), input.end(), l.begin()); std::copy (input.begin(), input.end(), r.begin()); chain.processStereo (l.data(), r.data(), kBlock); } }, kSeconds), 1.0);
    }
}

//==============================================================================
/*  PB-1.2 / PB-10.2 / QA-7.1: the six scenario totals, through the whole
    processor at 48 kHz / 128. LUTHIER_PERF=1 only. */
LUTHIER_TEST (PerfBudget, scenarioTotals)
{
    if (! perfRunRequested())
        return;

    struct Scenario
    {
        const char* label;
        const char* preset;
        int voices;
        double budget;
        std::function<void (LuthierAudioProcessor&)> setUp;
    };

    const Scenario scenarios[] = {
        { "idle",     "Init",               0, 1.5,  {} },
        { "steady",   "Single-Cut Crunch",  4, 8.0,  {} },
        { "realism",  "Fingerstyle Folk",   4, 12.0, [] (LuthierAudioProcessor& p)
                                                     {
                                                         setParam (p, ParamIDs::squeakAmount, 1.0f);
                                                         setParam (p, ParamIDs::pickChirpAmount, 1.0f);
                                                         setParam (p, ParamIDs::setupBuzzThreshold, 0.0f);
                                                     } },
        { "slide",    "Blues Slide",        4, 18.0, [] (LuthierAudioProcessor& p) { setParam (p, ParamIDs::feedbackAmount, 30.0f); } },
        { "bass",     "Rockabilly Slap",    4, 15.0, [] (LuthierAudioProcessor& p) { setParam (p, ParamIDs::slapArmed, 1.0f); } },
        { "heavy",    "Modern Metal Chug",  6, 22.0, [] (LuthierAudioProcessor& p)
                                                     {
                                                         const PedalType pre[] = { PedalType::Compressor, PedalType::NoiseGate, PedalType::Overdrive, PedalType::Distortion };
                                                         const PedalType post[] = { PedalType::Chorus, PedalType::Delay, PedalType::Reverb, PedalType::ParametricEQ };

                                                         for (int s = 0; s < 4; ++s)
                                                         {
                                                             setParam (p, ParamIDs::slotType (false, s).toRawUTF8(), (float) pre[s]);
                                                             setParam (p, ParamIDs::slotType (true, s).toRawUTF8(), (float) post[s]);
                                                         }
                                                     } },
    };

    for (const auto& s : scenarios)
    {
        auto p = std::make_unique<LuthierAudioProcessor>();
        p->prepareToPlay (kSr, 128);
        CHECK_MSG (loadFactory (*p, s.preset), juce::String ("no factory preset ") + s.preset);

        if (s.setUp)
        {
            s.setUp (*p);
            p->getParameterBridge().applyAllNow();
        }

        renderNotes (*p, s.voices, 1.0);          // warm
        const double units = cpuUnits ([&] { renderNotes (*p, s.voices, 10.0); }, 10.0);
        std::printf ("    %-10s %6.2f units (budget %.1f)\n", s.label, units, s.budget);
        CHECK_MSG (units <= s.budget, juce::String (s.label) + ": " + juce::String (units, 2) + " units, budget " + juce::String (s.budget, 1));
    }
}

//==============================================================================
/*  PB-6.1 / PB-7.1 / PB-10.7: the voice-count and sample-rate curves, as ratios
    within 10% of sections 6 and 7 (the spec's "~" values). LUTHIER_PERF=1 only. */
LUTHIER_TEST (PerfBudget, voiceCountScaling)
{
    if (! perfRunRequested())
        return;

    auto p = std::make_unique<LuthierAudioProcessor>();
    p->prepareToPlay (kSr, 128);
    loadFactory (*p, "Single-Cut Crunch");

    auto unitsFor = [&] (int voices) { renderNotes (*p, voices, 1.0); return cpuUnits ([&] { renderNotes (*p, voices, 8.0); }, 8.0); };

    const double four = unitsFor (4);

    struct Point { int voices; double ratio; };

    for (const auto& pt : { Point { 1, 0.30 }, Point { 8, 1.80 }, Point { 12, 2.50 } })
    {
        const double ratio = unitsFor (pt.voices) / four;
        std::printf ("    %2d voices: %.2fx of 4 (spec ~%.2fx)\n", pt.voices, ratio, pt.ratio);
        CHECK_MSG (std::abs (ratio - pt.ratio) <= pt.ratio * 0.10,
                   juce::String (pt.voices) + " voices cost " + juce::String (ratio, 2) + "x of 4 (spec ~" + juce::String (pt.ratio, 2) + "x)");
    }
}

LUTHIER_TEST (PerfBudget, sampleRateScaling)
{
    if (! perfRunRequested())
        return;

    struct Point { double rate; double ratio; };
    const Point curve[] = { { 44100.0, 1.0 }, { 48000.0, 1.09 }, { 88200.0, 2.0 }, { 96000.0, 2.2 },
                            { 176400.0, 4.0 }, { 192000.0, 4.4 } };

    auto unitsAt = [] (double rate)
    {
        auto p = std::make_unique<LuthierAudioProcessor>();
        p->prepareToPlay (rate, 128);
        loadFactory (*p, "Single-Cut Crunch");

        const int blocks = (int) (8.0 * rate / 128);
        juce::AudioBuffer<float> buffer (p->getTotalNumOutputChannels(), 128);

        return cpuUnits ([&]
        {
            juce::MidiBuffer midi;

            for (int b = 0; b < blocks; ++b)
            {
                midi.clear();

                if (b % (int) (rate / 128) == 0)
                    for (int note : { 40, 47, 52, 55 })
                        midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

                buffer.clear();
                p->processBlock (buffer, midi);
            }
        }, 8.0);
    };

    const double base = unitsAt (44100.0);

    for (const auto& pt : curve)
    {
        const double ratio = unitsAt (pt.rate) / base;
        std::printf ("    %6.0f Hz: %.2fx (spec %.2fx)\n", pt.rate, ratio, pt.ratio);
        CHECK_MSG (std::abs (ratio - pt.ratio) <= pt.ratio * 0.10,
                   juce::String (pt.rate) + " Hz costs " + juce::String (ratio, 2) + "x (spec " + juce::String (pt.ratio, 2) + "x)");
    }
}
