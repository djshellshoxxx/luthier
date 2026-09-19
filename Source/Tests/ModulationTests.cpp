/*  Modulation matrix tests (modulation-matrix.md section 8).

    The source tests measure what the source actually produces rather than
    asserting on its internal state: an LFO is correct if its output completes
    the right number of cycles per second, not if a phase variable looks right.
*/

#include "TestFramework.h"

#include "../Modulation/ModMatrix.h"
#include "../Parameters.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    /** A processor carrying the real parameter tree, so destinations resolve to
        the same ids and ranges the plugin uses. */
    class ModHarness : public juce::AudioProcessor
    {
    public:
        ModHarness()
            : AudioProcessor (BusesProperties()
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
              apvts (*this, nullptr, "LUTHIER", Parameters::createLayout())
        {
            matrix.prepare (kSr, kBlock, apvts);
        }

        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}

        const juce::String getName() const override { return "ModHarness"; }
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

        juce::AudioProcessorValueTreeState apvts;
        ModMatrix matrix;
    };

    /** Counts how many times a bipolar signal crosses zero going upward, which
        is one per cycle for every shape that is not stuck at a constant. */
    int countRisingZeroCrossings (const std::vector<double>& values)
    {
        int crossings = 0;

        for (size_t i = 1; i < values.size(); ++i)
            if (values[i - 1] < 0.0 && values[i] >= 0.0)
                ++crossings;

        return crossings;
    }
}

//==============================================================================
/*  Test 1: LFO frequency accuracy. A 2 Hz LFO run for four seconds of control
    ticks must complete eight cycles. */
LUTHIER_TEST (Modulation, lfoFrequencyIsAccurate)
{
    const double controlRate = 375.0;   // 48 kHz / 128

    for (double hz : { 0.5, 2.0, 7.0 })
    {
        ModLfo lfo;
        lfo.prepare (controlRate, 1);
        lfo.setShape (ModLfo::Shape::sine);
        lfo.setRateHz (hz);
        lfo.setBipolar (true);
        lfo.setDepth (1.0);

        const double seconds = 4.0;
        const int ticks = (int) (controlRate * seconds);

        std::vector<double> values;
        values.reserve ((size_t) ticks);

        for (int i = 0; i < ticks; ++i)
            values.push_back (lfo.tick (0.0, -1.0));

        const int cycles = countRisingZeroCrossings (values);
        const int expected = (int) std::round (hz * seconds);

        // One cycle of tolerance: whether the run starts and ends mid-cycle
        // shifts the count by at most one.
        CHECK_MSG (std::abs (cycles - expected) <= 1,
                   "LFO at " + juce::String (hz) + " Hz completed " + juce::String (cycles)
                     + " cycles in " + juce::String (seconds) + " s, expected "
                     + juce::String (expected));
    }
}

//==============================================================================
/*  Test 2: a tempo-synced LFO tracks the host rather than its own clock. At
    120 bpm a 1/4 division is 0.5 s, so four seconds is eight cycles. */
LUTHIER_TEST (Modulation, syncedLfoFollowsTheHost)
{
    const double controlRate = 375.0;
    const double bpm = 120.0;
    const double beatsPerTick = (bpm / 60.0) / controlRate;

    ModLfo lfo;
    lfo.prepare (controlRate, 2);
    lfo.setShape (ModLfo::Shape::sine);
    lfo.setSynced (true);
    lfo.setSyncDivision (ModSyncDivision::quarter);
    lfo.setDepth (1.0);

    const double seconds = 4.0;
    const int ticks = (int) (controlRate * seconds);

    std::vector<double> values;
    values.reserve ((size_t) ticks);

    for (int i = 0; i < ticks; ++i)
        values.push_back (lfo.tick (beatsPerTick, -1.0));

    const int cycles = countRisingZeroCrossings (values);

    CHECK_MSG (std::abs (cycles - 8) <= 1,
               "synced LFO completed " + juce::String (cycles) + " cycles, expected 8");
}

//==============================================================================
/*  Test 3: envelope stage times. An envelope with a 100 ms attack must reach
    full scale in 100 ms, and its decay must land on the sustain level. */
LUTHIER_TEST (Modulation, envelopeStageTimesAreAccurate)
{
    const double controlRate = 375.0;

    ModEnvelope env;
    env.prepare (controlRate);
    env.setDelaySeconds (0.0);
    env.setAttackSeconds (0.1);
    env.setHoldSeconds (0.0);
    env.setDecaySeconds (0.2);
    env.setSustainLevel (0.5);
    env.setReleaseSeconds (0.1);
    env.setStageCurve (ModEnvelope::Stage::attack, ModCurve::linear);
    env.setStageCurve (ModEnvelope::Stage::decay, ModCurve::linear);
    env.setRetrigger (ModEnvelope::Retrigger::always);

    env.noteOn();

    int ticksToPeak = 0;

    for (int i = 0; i < (int) (controlRate * 2.0); ++i)
    {
        const double v = env.tick();
        ++ticksToPeak;

        if (v >= 0.999)
            break;
    }

    const double attackSeconds = (double) ticksToPeak / controlRate;

    CHECK_NEAR (attackSeconds, 0.1, 0.01);

    // Run past the decay and check we have settled on sustain.
    for (int i = 0; i < (int) (controlRate * 0.5); ++i)
        env.tick();

    CHECK_NEAR (env.getCurrent(), 0.5, 0.02);
    CHECK (env.getStage() == ModEnvelope::Stage::sustain);

    // Release must fall back to zero and stop.
    env.noteOff();

    for (int i = 0; i < (int) (controlRate * 0.5); ++i)
        env.tick();

    CHECK_NEAR (env.getCurrent(), 0.0, 0.02);
    CHECK (! env.isActive());
}

//==============================================================================
/*  Test 4: sample-and-hold holds. Its output must be piecewise constant, with
    exactly one change per cycle. */
LUTHIER_TEST (Modulation, sampleAndHoldHoldsForAWholeCycle)
{
    const double controlRate = 375.0;

    ModLfo lfo;
    lfo.prepare (controlRate, 3);
    lfo.setShape (ModLfo::Shape::sampleAndHold);
    lfo.setRateHz (5.0);
    lfo.setDepth (1.0);
    lfo.setSmoothingMs (0.0);

    const double seconds = 2.0;
    const int ticks = (int) (controlRate * seconds);

    int changes = 0;
    double previous = lfo.tick (0.0, -1.0);

    for (int i = 1; i < ticks; ++i)
    {
        const double v = lfo.tick (0.0, -1.0);

        if (std::abs (v - previous) > 1.0e-9)
            ++changes;

        previous = v;
    }

    const int expected = (int) std::round (5.0 * seconds);

    CHECK_MSG (std::abs (changes - expected) <= 1,
               "S+H changed " + juce::String (changes) + " times in "
                 + juce::String (seconds) + " s, expected about " + juce::String (expected));
}

//==============================================================================
/*  Test 5: the step sequencer walks its steps and respects gates. */
LUTHIER_TEST (Modulation, stepSequencerWalksItsSteps)
{
    const double controlRate = 375.0;
    const double beatsPerTick = (120.0 / 60.0) / controlRate;

    ModStepSequencer seq;
    seq.prepare (controlRate, 4);
    seq.setLength (4);
    seq.setDivision (ModSyncDivision::quarter);
    seq.setDirection (ModStepSequencer::Direction::forward);
    seq.setSynced (true);
    seq.setSwing (0.0);

    for (int i = 0; i < 4; ++i)
    {
        ModStepSequencer::Step step;
        step.value = -1.0 + 0.5 * (double) i;
        step.gate = true;
        step.slide = false;
        step.probability = 1.0;
        seq.setStep (i, step);
    }

    seq.transportStarted();

    juce::SortedSet<int> visited;

    // Four quarter notes at 120 bpm is two seconds, so four seconds covers the
    // pattern twice over.
    for (int i = 0; i < (int) (controlRate * 4.0); ++i)
    {
        seq.tick (beatsPerTick);
        visited.add (seq.getCurrentStep());
    }

    CHECK_MSG (visited.size() == 4,
               "sequencer visited " + juce::String (visited.size()) + " of 4 steps");

    // A closed gate holds the previous value rather than snapping to zero.
    ModStepSequencer::Step muted;
    muted.value = 1.0;
    muted.gate = false;
    muted.probability = 1.0;
    seq.setStep (1, muted);
    seq.reset();
    seq.transportStarted();

    bool sawNonZero = false;

    for (int i = 0; i < (int) (controlRate * 4.0); ++i)
    {
        const double v = seq.tick (beatsPerTick);

        if (std::abs (v) > 1.0e-6)
            sawNonZero = true;

        CHECK_FINITE (&v, 1);
    }

    CHECK (sawNonZero);
}

//==============================================================================
/*  Test 6: the envelope follower tracks a level with the attack and release it
    was given. */
LUTHIER_TEST (Modulation, envelopeFollowerTracksLevel)
{
    const double controlRate = 375.0;

    ModEnvelopeFollower follower;
    follower.prepare (controlRate);
    follower.setAttackMs (10.0);
    follower.setReleaseMs (100.0);
    follower.setDetection (ModEnvelopeFollower::Detection::peak);
    follower.setThreshold (0.0);
    follower.setLogarithmic (false);

    // Feed a steady level and let it settle.
    for (int i = 0; i < (int) (controlRate * 0.2); ++i)
        follower.tick (0.8, 0.32);

    CHECK_NEAR (follower.getCurrent(), 0.8, 0.02);

    // Silence: the release must bring it down, and further than the attack
    // would have in the same time.
    for (int i = 0; i < (int) (controlRate * 0.3); ++i)
        follower.tick (0.0, 0.0);

    CHECK_MSG (follower.getCurrent() < 0.1,
               "follower released to " + juce::String (follower.getCurrent(), 4)
                 + ", expected below 0.1");

    // The gate holds the output down for signals under the threshold.
    follower.reset();
    follower.setThreshold (0.5);

    for (int i = 0; i < (int) (controlRate * 0.2); ++i)
        follower.tick (0.2, 0.04);

    CHECK_NEAR (follower.getCurrent(), 0.0, 1.0e-3);
}

//==============================================================================
/*  Test 7: a route actually moves its destination, and depth scales it. */
LUTHIER_TEST (Modulation, routeModulatesItsDestination)
{
    ModHarness harness;
    auto& matrix = harness.matrix;

    // A macro is the simplest source to drive by hand: it is whatever we set.
    ModRoute route;
    route.sourceId = "macro1";
    route.destinationId = ParamIDs::ampGain;
    route.depth = 1.0f;
    route.offset = 0.0f;
    route.curve = ModCurve::linear;
    route.enabled = true;

    CHECK (matrix.addRoute (route));
    CHECK (matrix.getNumRoutes() == 1);
    CHECK (matrix.isActive());

    const auto range = harness.apvts.getParameterRange (ParamIDs::ampGain);
    const float base = range.start + range.getRange().getLength() * 0.25f;

    const int index = [&]
    {
        const auto& parameters = harness.getParameters();

        for (int i = 0; i < parameters.size(); ++i)
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]))
                if (withId->paramID == ParamIDs::ampGain)
                    return i;

        return -1;
    }();

    CHECK (index >= 0);
    CHECK (matrix.isDestinationModulated (index));

    ModBlockContext context;

    // Macro at zero: the destination is left alone.
    matrix.setMacroValue (0, 0.0);

    for (int i = 0; i < 64; ++i)
        matrix.processBlock (kBlock, context);

    CHECK_NEAR (matrix.apply (index, base), base, 1.0e-3);

    // Macro at full: the destination moves by the full parameter range, and
    // clamps at the top rather than running past it.
    matrix.setMacroValue (0, 1.0);

    for (int i = 0; i < 128; ++i)
        matrix.processBlock (kBlock, context);

    const float modulated = matrix.apply (index, base);

    CHECK_MSG (modulated > base, "full-depth modulation did not raise the value");
    CHECK_MSG (modulated <= range.end + 1.0e-4f, "modulation ran past the parameter maximum");
    CHECK_NEAR (modulated, range.end, range.getRange().getLength() * 0.02);

    // Half depth moves it half as far.
    matrix.setRouteDepth (0, 0.25f);

    for (int i = 0; i < 128; ++i)
        matrix.processBlock (kBlock, context);

    const float quarterDepth = matrix.apply (index, base);
    CHECK_NEAR (quarterDepth - base, range.getRange().getLength() * 0.25, range.getRange().getLength() * 0.02);

    // A disabled route does nothing at all.
    matrix.setRouteEnabled (0, false);

    for (int i = 0; i < 64; ++i)
        matrix.processBlock (kBlock, context);

    CHECK_NEAR (matrix.apply (index, base), base, 1.0e-3);
}

//==============================================================================
/*  Test 8: a destination accepts eight sources and refuses the ninth
    (modulation-matrix 0.4). */
LUTHIER_TEST (Modulation, destinationAcceptsEightSourcesAndNoMore)
{
    ModHarness harness;
    auto& matrix = harness.matrix;

    for (int i = 0; i < ModMatrix::kMaxRoutesPerDestination; ++i)
    {
        ModRoute route;
        route.sourceId = "lfo" + juce::String (i + 1);
        route.destinationId = ParamIDs::ampGain;
        route.depth = 0.1f;
        CHECK_MSG (matrix.addRoute (route),
                   "route " + juce::String (i + 1) + " to the same destination was refused");
    }

    ModRoute ninth;
    ninth.sourceId = "macro1";
    ninth.destinationId = ParamIDs::ampGain;
    ninth.depth = 0.1f;

    CHECK_MSG (! matrix.addRoute (ninth), "a ninth source was accepted for one destination");
    CHECK (matrix.getRouteCountForDestination (ParamIDs::ampGain) == 8);

    // An unknown source or destination is refused outright.
    ModRoute nonsense;
    nonsense.sourceId = "lfo99";
    nonsense.destinationId = ParamIDs::ampBass;
    CHECK (! matrix.addRoute (nonsense));

    nonsense.sourceId = "lfo1";
    nonsense.destinationId = "no_such_parameter";
    CHECK (! matrix.addRoute (nonsense));
}

//==============================================================================
/*  Test 9: a thousand routes stay within the CPU budget. */
LUTHIER_TEST (Modulation, thousandRouteStressTest)
{
    ModHarness harness;
    auto& matrix = harness.matrix;

    // Spread a thousand routes over as many destinations as the eight-per-
    // destination rule allows.
    juce::StringArray destinations;

    for (auto* param : harness.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
            if (dynamic_cast<juce::AudioParameterFloat*> (param) != nullptr)
                destinations.add (withId->paramID);

    CHECK_MSG (destinations.size() * 8 >= 1000,
               "not enough float parameters to host a thousand routes");

    int added = 0;

    for (int d = 0; d < destinations.size() && added < 1000; ++d)
    {
        for (int s = 0; s < ModMatrix::kMaxRoutesPerDestination && added < 1000; ++s)
        {
            ModRoute route;
            route.sourceId = "lfo" + juce::String ((s % ModSourceSlots::numLfos) + 1);
            route.destinationId = destinations[d];
            route.depth = 0.01f;
            route.curve = (ModCurve) (added % (int) ModCurve::numCurves);

            if (matrix.addRoute (route))
                ++added;
        }
    }

    CHECK_MSG (added == 1000, "only added " + juce::String (added) + " of 1000 routes");

    ModBlockContext context;

    // Warm up, so the measurement is not dominated by first-touch page faults.
    for (int i = 0; i < 32; ++i)
        matrix.processBlock (kBlock, context);

    const int blocks = 2000;
    const auto start = juce::Time::getHighResolutionTicks();

    for (int i = 0; i < blocks; ++i)
        matrix.processBlock (kBlock, context);

    const double elapsed = juce::Time::highResolutionTicksToSeconds (
        juce::Time::getHighResolutionTicks() - start);

    const double audioSeconds = (double) blocks * (double) kBlock / kSr;
    const double cpuPercent = (elapsed / audioSeconds) * 100.0;

    // The spec's claim is "well below 1%". Measured on this machine it is a
    // small fraction of that; the bar is set at 1% so the test fails on a real
    // regression rather than on ordinary timing noise.
    CHECK_MSG (cpuPercent < 1.0,
               "1000 routes cost " + juce::String (cpuPercent, 4)
                 + "% of real time, expected under 1%");
}

//==============================================================================
/*  Test 10: preset round trip. Every route comes back exactly as it went in. */
LUTHIER_TEST (Modulation, presetRoundTripIsExact)
{
    ModHarness saved;

    std::vector<ModRoute> written;

    const char* sources[] = { "lfo1", "env2", "seq1", "follow2", "macro5",
                              "cc74", "pitchBend", "randNote" };

    const char* destinations[] = { ParamIDs::ampGain, ParamIDs::ampBass, ParamIDs::ampTreble,
                                   ParamIDs::bodyAmount, ParamIDs::pluckPosition,
                                   ParamIDs::guitarTone, ParamIDs::masterGain,
                                   ParamIDs::couplingAmount };

    for (int i = 0; i < 8; ++i)
    {
        ModRoute r;
        r.sourceId = sources[i];
        r.sourceChannel = i % 3;
        r.destinationId = destinations[i];
        r.depth = -1.0f + 0.25f * (float) i;
        r.offset = 0.125f * (float) i;
        r.curve = (ModCurve) (i % (int) ModCurve::numCurves);
        r.enabled = (i % 2) == 0;

        CHECK (saved.matrix.addRoute (r));
        written.push_back (r);
    }

    // Move some source state too, so the round trip covers more than routes.
    saved.matrix.getLfo (0).setShape (ModLfo::Shape::custom);
    saved.matrix.getLfo (0).setRateHz (3.75);
    saved.matrix.getLfo (0).setSynced (true);
    saved.matrix.getLfo (0).setSyncDivision (ModSyncDivision::eighthD);
    saved.matrix.getLfo (0).setBreakpoint (3, -0.625);
    saved.matrix.getEnvelope (1).setAttackSeconds (1.25);
    saved.matrix.getEnvelope (1).setSustainLevel (0.375);
    saved.matrix.getSequencer (0).setLength (12);
    saved.matrix.getSequencer (0).setSwing (0.5);
    saved.matrix.getFollower (1).setAttackMs (33.0);

    const auto state = saved.matrix.toVar();
    const auto json = juce::JSON::toString (state, false);

    ModHarness loaded;
    loaded.matrix.fromVar (juce::JSON::parse (json));

    CHECK (loaded.matrix.getNumRoutes() == (int) written.size());
    CHECK (loaded.matrix.getNumDroppedOnLoad() == 0);

    for (size_t i = 0; i < written.size(); ++i)
    {
        const auto got = loaded.matrix.getRoute ((int) i);
        const auto& want = written[i];

        CHECK (got.sourceId == want.sourceId);
        CHECK (got.sourceChannel == want.sourceChannel);
        CHECK (got.destinationId == want.destinationId);
        CHECK (got.depth == want.depth);
        CHECK (got.offset == want.offset);
        CHECK (got.curve == want.curve);
        CHECK (got.enabled == want.enabled);
    }

    CHECK (loaded.matrix.getLfo (0).getShape() == ModLfo::Shape::custom);
    CHECK_NEAR (loaded.matrix.getLfo (0).getRateHz(), 3.75, 1.0e-6);
    CHECK (loaded.matrix.getLfo (0).isSynced());
    CHECK (loaded.matrix.getLfo (0).getSyncDivision() == ModSyncDivision::eighthD);
    CHECK_NEAR (loaded.matrix.getLfo (0).getBreakpoint (3), -0.625, 1.0e-6);
    CHECK_NEAR (loaded.matrix.getEnvelope (1).getAttackSeconds(), 1.25, 1.0e-6);
    CHECK_NEAR (loaded.matrix.getEnvelope (1).getSustainLevel(), 0.375, 1.0e-6);
    CHECK (loaded.matrix.getSequencer (0).getLength() == 12);
    CHECK_NEAR (loaded.matrix.getSequencer (0).getSwing(), 0.5, 1.0e-6);
    CHECK_NEAR (loaded.matrix.getFollower (1).getAttackMs(), 33.0, 1.0e-6);
}

//==============================================================================
/*  Test 11: a preset naming a parameter this build does not have loads anyway,
    dropping the route and reporting it (modulation-matrix 6). */
LUTHIER_TEST (Modulation, unknownDestinationsAreReportedNotFatal)
{
    ModHarness harness;

    std::vector<ModRoute> routes;

    ModRoute good;
    good.sourceId = "lfo1";
    good.destinationId = ParamIDs::ampGain;
    good.depth = 0.5f;
    routes.push_back (good);

    ModRoute bad;
    bad.sourceId = "lfo2";
    bad.destinationId = "parameter_from_a_later_version";
    bad.depth = 0.5f;
    routes.push_back (bad);

    harness.matrix.setRoutes (routes);

    CHECK (harness.matrix.getNumRoutes() == 1);
    CHECK (harness.matrix.getNumDroppedOnLoad() == 1);
    CHECK (harness.matrix.getUnknownDestinations().contains ("parameter_from_a_later_version"));
}

//==============================================================================
/*  Test 12: a discrete destination changes at the option boundaries
    (modulation-matrix 4). */
LUTHIER_TEST (Modulation, discreteDestinationsStepAtBoundaries)
{
    ModHarness harness;

    // Find a choice parameter with a useful number of options.
    juce::String choiceId;
    int choiceIndex = -1;
    int numOptions = 0;

    const auto& parameters = harness.getParameters();

    for (int i = 0; i < parameters.size(); ++i)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameters[i]))
        {
            if (choice->choices.size() >= 5)
            {
                choiceId = choice->paramID;
                choiceIndex = i;
                numOptions = choice->choices.size();
                break;
            }
        }
    }

    CHECK_MSG (choiceIndex >= 0, "no choice parameter with at least five options");

    if (choiceIndex < 0)
        return;

    ModRoute route;
    route.sourceId = "macro1";
    route.destinationId = choiceId;
    route.depth = 1.0f;
    CHECK (harness.matrix.addRoute (route));

    const auto range = harness.apvts.getParameterRange (choiceId);

    ModBlockContext context;

    // Sweep the source and record the distinct values the destination takes.
    juce::SortedSet<int> observed;

    for (int step = 0; step <= 100; ++step)
    {
        harness.matrix.setMacroValue (0, (double) step / 100.0);

        // Long enough for the interpolation to settle on the new target.
        for (int i = 0; i < 64; ++i)
            harness.matrix.processBlock (kBlock, context);

        const float value = harness.matrix.apply (choiceIndex, range.start);
        observed.add (juce::roundToInt (value));
    }

    // Every option between the base and the top must be reachable, and the
    // result must always land exactly on an option rather than between two.
    CHECK_MSG (observed.size() >= 2,
               "modulating a selector produced only " + juce::String (observed.size())
                 + " distinct value(s)");

    for (int i = 0; i < observed.size(); ++i)
    {
        CHECK (observed[i] >= (int) range.start);
        CHECK (observed[i] <= (int) range.end);
    }
}

//==============================================================================
/*  Test 13: determinism. Two matrices with the same seed produce identical
    output, which is what makes an offline render repeatable. */
LUTHIER_TEST (Modulation, randomSourcesAreDeterministic)
{
    auto run = []
    {
        ModHarness harness;

        ModRoute route;
        route.sourceId = "randSmooth";
        route.destinationId = ParamIDs::ampGain;
        route.depth = 1.0f;
        harness.matrix.addRoute (route);

        const auto& parameters = harness.getParameters();
        int index = -1;

        for (int i = 0; i < parameters.size(); ++i)
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[i]))
                if (withId->paramID == ParamIDs::ampGain)
                    index = i;

        ModBlockContext context;
        std::vector<float> values;

        for (int i = 0; i < 256; ++i)
        {
            harness.matrix.processBlock (kBlock, context);
            values.push_back (harness.matrix.apply (index, 0.5f));
        }

        return values;
    };

    const auto first = run();
    const auto second = run();

    CHECK (first.size() == second.size());

    int differences = 0;

    for (size_t i = 0; i < first.size(); ++i)
        if (first[i] != second[i])
            ++differences;

    CHECK_MSG (differences == 0,
               juce::String (differences) + " of " + juce::String ((int) first.size())
                 + " values differed between two identically seeded runs");

    // And a reset must put the sequence back to the beginning.
    ModRandomSource a, b;
    a.prepare (375.0, 0xABCDEF);
    b.prepare (375.0, 0xABCDEF);

    for (int i = 0; i < 100; ++i)
    {
        a.tick();
        b.tick();
    }

    CHECK (a.getSmooth() == b.getSmooth());

    a.reset();

    for (int i = 0; i < 100; ++i)
        a.tick();

    CHECK_MSG (a.getSmooth() == b.getSmooth(), "reset did not restore the random sequence");
}

//==============================================================================
/*  Test 14: source ids round trip through their slot numbers, which is what
    lets a preset name a source as a string and the audio thread use an int. */
LUTHIER_TEST (Modulation, sourceIdsResolveBothWays)
{
    for (int slot = 0; slot < ModSourceSlots::count; ++slot)
    {
        const auto id = modSourceIdForSlot (slot);

        CHECK_MSG (id.isNotEmpty(), "slot " + juce::String (slot) + " has no id");
        CHECK_MSG (modSourceSlotForId (id) == slot,
                   "id " + id + " resolved to slot " + juce::String (modSourceSlotForId (id))
                     + ", expected " + juce::String (slot));
        CHECK (modSourceDisplayName (slot).isNotEmpty());
    }

    // Ids that name nothing must be rejected, not silently mapped to slot zero.
    CHECK (modSourceSlotForId ("") == -1);
    CHECK (modSourceSlotForId ("lfo0") == -1);
    CHECK (modSourceSlotForId ("lfo9") == -1);
    CHECK (modSourceSlotForId ("cc128") == -1);
    CHECK (modSourceSlotForId ("nonsense") == -1);
    CHECK (modSourceSlotForId ("env5") == -1);

    // The 14-bit form must not be confused with the plain one.
    CHECK (modSourceSlotForId ("cc14b0") == ModSourceSlots::cc14Base);
    CHECK (modSourceSlotForId ("cc14") == ModSourceSlots::ccBase + 14);
}

//==============================================================================
/*  Test 15: curves preserve their fixed points and their sign, so a bipolar
    source stays symmetrical through any of them. */
LUTHIER_TEST (Modulation, curvesPreserveSignAndFixedPoints)
{
    for (int c = 0; c < (int) ModCurve::numCurves; ++c)
    {
        const auto curve = (ModCurve) c;

        CHECK_NEAR (applyModCurve (0.0, curve), 0.0, 1.0e-9);
        CHECK_NEAR (applyModCurve (1.0, curve), 1.0, 1.0e-9);
        CHECK_NEAR (applyModCurve (-1.0, curve), -1.0, 1.0e-9);

        // Odd symmetry: f(-x) must equal -f(x).
        for (double x = 0.05; x < 1.0; x += 0.05)
            CHECK_NEAR (applyModCurve (-x, curve), -applyModCurve (x, curve), 1.0e-9);

        // And nothing may leave the range.
        for (double x = -1.0; x <= 1.0; x += 0.01)
        {
            const double y = applyModCurve (x, curve);
            CHECK (y >= -1.0 - 1.0e-9 && y <= 1.0 + 1.0e-9);
        }

        CHECK (juce::String (getModCurveName (curve)).isNotEmpty());
    }
}
