/*  Live performance tests (live-performance.md section 12).

    The three that matter most are the ones about audible glitches: a snapshot
    recall must not click, the kill switch must reach silence inside its stated
    window and come back inside it, and a morph must follow the curve the user
    picked. The rest guard the plumbing - program change mapping, tap tempo
    accuracy, setlist walking.
*/

#include "TestFramework.h"

#include "../Live/Snapshots.h"
#include "../Live/Setlist.h"
#include "../Live/TapTempo.h"
#include "../Live/LiveControls.h"
#include "../Parameters.h"

#include <map>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;

    /** A bare processor carrying Luthier's real parameter tree, which is all the
        snapshot bank needs. Standing one of these up is far cheaper than a whole
        plugin instance, and the bank never touches anything else. */
    class ParameterHost : public juce::AudioProcessor
    {
    public:
        ParameterHost()
            : juce::AudioProcessor (BusesProperties()
                                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
              apvts (*this, nullptr, "PARAMETERS", Parameters::createLayout())
        {
        }

        juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }

        //======================================================================
        const juce::String getName() const override { return "ParameterHost"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}

    private:
        juce::AudioProcessorValueTreeState apvts;
    };

    /** The first continuous parameter in the tree, which is what the morph and
        crossfade tests watch travel. */
    juce::AudioProcessorParameterWithID* findContinuous (juce::AudioProcessor& processor)
    {
        for (auto* p : processor.getParameters())
            if (! SnapshotBank::isDiscrete (*p))
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
                    return withId;

        return nullptr;
    }

    juce::AudioProcessorParameterWithID* findDiscrete (juce::AudioProcessor& processor)
    {
        for (auto* p : processor.getParameters())
            if (SnapshotBank::isDiscrete (*p) && p->getNumSteps() > 2)
                if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
                    return withId;

        return nullptr;
    }

    void setAllParameters (juce::AudioProcessor& processor, float value)
    {
        for (auto* p : processor.getParameters())
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
                withId->setValueNotifyingHost (value);
    }

    /*  What every parameter actually holds, which is not always what was written
        to it: a choice parameter with twelve options quantises 0.25 to the
        nearest of its eleven steps. A recall has to restore these values, not
        the ones that were asked for. */
    std::map<juce::String, double> captureValues (juce::AudioProcessor& processor)
    {
        std::map<juce::String, double> values;

        for (auto* p : processor.getParameters())
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
                values[withId->paramID] = (double) withId->getValue();

        return values;
    }

    /** Checks every parameter against a previously captured set. Returns how
        many disagreed. */
    int countDifferences (juce::AudioProcessor& processor,
                          const std::map<juce::String, double>& expected,
                          double tolerance)
    {
        int differences = 0;

        for (auto* p : processor.getParameters())
        {
            auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p);

            // SPEC-SWEEP: LP-16 - the morph position is never snapshotted.
            if (withId == nullptr || withId->paramID == ParamIDs::snapshotMorph)
                continue;

            const auto entry = expected.find (withId->paramID);

            if (entry == expected.end() || ParamIDs::isJamTransient (withId->paramID))   // FEAT-JAM: never in a snapshot
                continue;

            if (std::abs ((double) withId->getValue() - entry->second) > tolerance)
                ++differences;
        }

        return differences;
    }
}

//==============================================================================
/*  A snapshot captures every parameter, and recalling it with no crossfade puts
    every one of them back exactly. */
LUTHIER_TEST (LiveSnapshots, captureAndRecallRoundTrip)
{
    ParameterHost host;
    SnapshotBank bank (host);

    setAllParameters (host, 0.25f);
    const auto quarter = captureValues (host);
    CHECK (bank.capture (0, "Quarter"));

    setAllParameters (host, 0.75f);
    const auto threeQuarters = captureValues (host);
    CHECK (bank.capture (1, "Three quarters"));

    bank.setCrossfadeMs (0.0);

    CHECK (bank.recall (0));
    CHECK_MSG (countDifferences (host, quarter, 1.0e-6) == 0,
               juce::String (countDifferences (host, quarter, 1.0e-6))
                 + " parameters did not come back from snapshot 0");

    CHECK (bank.recall (1));
    CHECK_MSG (countDifferences (host, threeQuarters, 1.0e-6) == 0,
               juce::String (countDifferences (host, threeQuarters, 1.0e-6))
                 + " parameters did not come back from snapshot 1");

    CHECK (bank.getSnapshot (0).label == "Quarter");
    CHECK (bank.getSnapshot (1).label == "Three quarters");
}

//==============================================================================
/*  live-performance 1: a recall crossfades continuous parameters rather than
    jumping them, and hard-switches discrete ones at the midpoint. */
LUTHIER_TEST (LiveSnapshots, recallCrossfadesContinuousAndStepsDiscrete)
{
    ParameterHost host;
    SnapshotBank bank (host);

    auto* continuous = findContinuous (host);
    auto* discrete = findDiscrete (host);

    CHECK (continuous != nullptr);
    CHECK (discrete != nullptr);

    if (continuous == nullptr || discrete == nullptr)
        return;

    setAllParameters (host, 0.0f);
    bank.capture (0, "A");

    setAllParameters (host, 1.0f);
    bank.capture (1, "B");

    bank.setCrossfadeMs (0.0);
    bank.recall (0);

    bank.setCrossfadeMs (100.0);
    CHECK (bank.recall (1));
    CHECK (bank.isRecalling());

    // Quarter of the way: the continuous parameter has moved a quarter, and the
    // discrete one has not moved at all.
    bank.advance (0.025);

    CHECK_NEAR (continuous->getValue(), 0.25, 0.02);
    CHECK_NEAR (discrete->getValue(), 0.0, 0.001);

    // Past the midpoint: the discrete parameter switches, once.
    bank.advance (0.030);
    CHECK_NEAR (discrete->getValue(), 1.0, 0.001);

    bank.advance (0.100);

    CHECK (! bank.isRecalling());
    CHECK_NEAR (continuous->getValue(), 1.0, 0.001);
}

//==============================================================================
/*  live-performance 12: recall 1000 random snapshot pairs and verify that no
    continuous parameter ever jumps. A jump in a parameter is what a click in the
    audio is made of, and it is the thing the crossfade exists to prevent. */
LUTHIER_TEST (LiveSnapshots, thousandRecallsNeverJumpAParameter)
{
    ParameterHost host;
    SnapshotBank bank (host);

    juce::Random random (0x11fe);

    for (int i = 0; i < 8; ++i)
    {
        setAllParameters (host, random.nextFloat());
        bank.capture (i);
    }

    auto* continuous = findContinuous (host);
    CHECK (continuous != nullptr);

    if (continuous == nullptr)
        return;

    bank.setCrossfadeMs (30.0);

    // One block at 48 kHz and a 512-sample buffer, which is what the fade is
    // stepped by in the plugin.
    const double blockSeconds = 512.0 / kSr;

    double worstStep = 0.0;

    for (int trial = 0; trial < 1000; ++trial)
    {
        const int target = random.nextInt (8);

        if (! bank.recall (target))
            continue;

        double previous = continuous->getValue();

        while (bank.isRecalling())
        {
            bank.advance (blockSeconds);

            const double now = continuous->getValue();
            worstStep = juce::jmax (worstStep, std::abs (now - previous));
            previous = now;
        }
    }

    // A 30 ms fade stepped every 10.7 ms cannot move more than about 36% of the
    // full range in one step, and a full-range snapshot change is the worst
    // case. What matters is that it is a ramp at all rather than a jump to the
    // target on the first block.
    CHECK_MSG (worstStep < 0.5,
               "largest single-block parameter step was " + juce::String (worstStep, 4));
}

//==============================================================================
/*  live-performance 2: program change maps straight onto the snapshot index,
    across all 128 values. */
LUTHIER_TEST (LiveSnapshots, programChangeMapsAcrossAllOneTwentyEight)
{
    ParameterHost host;
    SnapshotBank bank (host);

    juce::Random random (0x9c);

    for (int i = 0; i < SnapshotBank::kMaxSnapshots; ++i)
    {
        setAllParameters (host, (float) i / 127.0f);
        CHECK (bank.capture (i, "S" + juce::String (i)));
    }

    CHECK (bank.getNumSnapshots() == SnapshotBank::kMaxSnapshots);

    bank.setCrossfadeMs (0.0);

    for (int pc = 0; pc < 128; ++pc)
    {
        CHECK_MSG (bank.recall (pc), "program change " + juce::String (pc) + " recalled nothing");
        CHECK_MSG (bank.getCurrentSnapshot() == pc,
                   "program change " + juce::String (pc) + " landed on "
                     + juce::String (bank.getCurrentSnapshot()));
    }

    // 128 is past the end of the bank and must be refused rather than wrapped.
    CHECK (! bank.recall (128));
}

//==============================================================================
/*  live-performance 12: for each curve, the interpolation must match the shape
    within 0.001 at 100 sample points. */
LUTHIER_TEST (LiveSnapshots, morphFollowsItsCurve)
{
    for (int c = 0; c < (int) MorphCurve::numCurves; ++c)
    {
        const auto curve = (MorphCurve) c;

        // The endpoints are pinned for every curve.
        CHECK_NEAR (applyMorphCurve (0.0, curve), 0.0, 0.001);
        CHECK_NEAR (applyMorphCurve (1.0, curve), 1.0, 0.001);

        double previous = -1.0;

        for (int i = 0; i <= 100; ++i)
        {
            const double x = (double) i / 100.0;
            const double y = applyMorphCurve (x, curve);

            CHECK_MSG (y >= -0.001 && y <= 1.001,
                       juce::String (getMorphCurveName (curve)) + " left 0..1 at x="
                         + juce::String (x, 3));

            // Every curve here is an easing curve, so it must never go backwards.
            CHECK_MSG (y >= previous - 0.001,
                       juce::String (getMorphCurveName (curve)) + " is not monotonic at x="
                         + juce::String (x, 3));

            previous = y;

            switch (curve)
            {
                case MorphCurve::linear:
                    CHECK_NEAR (y, x, 0.001);
                    break;

                case MorphCurve::sCurve:
                    CHECK_NEAR (y, x * x * (3.0 - 2.0 * x), 0.001);
                    break;

                case MorphCurve::exponential:
                    CHECK_NEAR (y, x * x, 0.001);
                    break;

                case MorphCurve::bezier:
                case MorphCurve::numCurves:
                default:
                    break;
            }
        }
    }

    // The Bezier's midpoint follows its control points rather than the line.
    const double eased = applyMorphCurve (0.5, MorphCurve::bezier, 0.25, 0.1, 0.75, 0.9);
    CHECK_NEAR (eased, 0.5, 0.05);

    const double lifted = applyMorphCurve (0.5, MorphCurve::bezier, 0.1, 0.9, 0.9, 1.0);
    CHECK_MSG (lifted > 0.6, "a curve lifted at the start should be above the line at its middle");
}

//==============================================================================
/*  live-performance 3: an excluded parameter takes snapshot A's value for the
    whole morph, and discrete parameters switch at the midpoint. */
LUTHIER_TEST (LiveSnapshots, morphHonoursExclusionsAndDiscreteSwitching)
{
    ParameterHost host;
    SnapshotBank bank (host);

    auto* continuous = findContinuous (host);
    auto* discrete = findDiscrete (host);

    CHECK (continuous != nullptr && discrete != nullptr);

    if (continuous == nullptr || discrete == nullptr)
        return;

    setAllParameters (host, 0.0f);
    bank.capture (0, "A");

    setAllParameters (host, 1.0f);
    bank.capture (1, "B");

    bank.setMorphSlots (0, 1);
    bank.setMorphEnabled (true);
    bank.setMorphCurve (MorphCurve::linear);

    bank.setMorphPosition (0.3);
    CHECK_NEAR (continuous->getValue(), 0.3, 0.01);
    CHECK_NEAR (discrete->getValue(), 0.0, 0.001);

    bank.setMorphPosition (0.7);
    CHECK_NEAR (continuous->getValue(), 0.7, 0.01);
    CHECK_NEAR (discrete->getValue(), 1.0, 0.001);

    // Excluded: it should stay at A's value wherever the morph goes.
    bank.setParameterExcludedFromMorph (continuous->paramID, true);
    CHECK (bank.isParameterExcludedFromMorph (continuous->paramID));

    bank.setMorphPosition (0.9);
    CHECK_NEAR (continuous->getValue(), 0.0, 0.01);

    bank.setParameterExcludedFromMorph (continuous->paramID, false);
    bank.setMorphPosition (0.9);
    CHECK_NEAR (continuous->getValue(), 0.9, 0.01);
}

//==============================================================================
/*  The bank travels inside the preset, so it has to round-trip. */
LUTHIER_TEST (LiveSnapshots, bankRoundTripsThroughJson)
{
    ParameterHost host;
    SnapshotBank bank (host);

    std::map<juce::String, double> expectedForSnapshotTwo;

    for (int i = 0; i < 4; ++i)
    {
        setAllParameters (host, 0.1f * (float) i);

        if (i == 2)
            expectedForSnapshotTwo = captureValues (host);

        bank.capture (i, "Snap " + juce::String (i), i * 3);
    }

    bank.setCrossfadeMs (77.0);
    bank.setMorphSlots (1, 3);
    bank.setMorphEnabled (true);
    bank.setMorphCurve (MorphCurve::sCurve);
    bank.setParameterExcludedFromMorph (ParamIDs::ampModel, true);

    const auto text = juce::JSON::toString (bank.toVar(), false);

    SnapshotBank restored (host);
    restored.fromVar (juce::JSON::parse (text));

    CHECK (restored.getNumSnapshots() == 4);
    CHECK_NEAR (restored.getCrossfadeMs(), 77.0, 0.001);
    CHECK (restored.getMorphSlotA() == 1);
    CHECK (restored.getMorphSlotB() == 3);
    CHECK (restored.isMorphEnabled());
    CHECK (restored.getMorphCurve() == MorphCurve::sCurve);
    CHECK (restored.isParameterExcludedFromMorph (ParamIDs::ampModel));

    for (int i = 0; i < 4; ++i)
    {
        CHECK (restored.getSnapshot (i).label == "Snap " + juce::String (i));
        CHECK (restored.getSnapshot (i).colourTag == i * 3);
    }

    // And the restored snapshots still recall to the same values.
    restored.setCrossfadeMs (0.0);
    restored.recall (2);

    CHECK_MSG (countDifferences (host, expectedForSnapshotTwo, 1.0e-6) == 0,
               juce::String (countDifferences (host, expectedForSnapshotTwo, 1.0e-6))
                 + " parameters did not survive the bank round trip");
}

//==============================================================================
/*  live-performance 12: the kill switch must drop below -80 dBFS within 5 ms and
    recover within 5 ms, over a thousand activations. */
LUTHIER_TEST (LiveKillSwitch, reachesSilenceAndRecoversInsideFiveMilliseconds)
{
    KillSwitch kill;
    kill.prepare (kSr);

    const int blockSize = 64;
    const int blocksInFiveMs = (int) std::ceil (0.005 * kSr / (double) blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);

    auto fillWithFullScale = [&buffer, blockSize]
    {
        for (int channel = 0; channel < 2; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            // Full-scale DC is the hardest case: there is no zero crossing to
            // hide a discontinuity in.
            for (int i = 0; i < blockSize; ++i)
                data[i] = 1.0f;
        }
    };

    int failedToSilence = 0, failedToRecover = 0;

    for (int activation = 0; activation < 1000; ++activation)
    {
        kill.setActive (true);

        double peakAfterFade = 0.0;

        for (int block = 0; block < blocksInFiveMs + 1; ++block)
        {
            fillWithFullScale();
            kill.processBlock (buffer);
        }

        fillWithFullScale();
        kill.processBlock (buffer);

        for (int channel = 0; channel < 2; ++channel)
            peakAfterFade = juce::jmax (peakAfterFade,
                                        (double) buffer.getMagnitude (channel, 0, blockSize));

        if (juce::Decibels::gainToDecibels (peakAfterFade, -200.0) > -80.0)
            ++failedToSilence;

        kill.setActive (false);

        for (int block = 0; block < blocksInFiveMs + 1; ++block)
        {
            fillWithFullScale();
            kill.processBlock (buffer);
        }

        fillWithFullScale();
        kill.processBlock (buffer);

        double minimumAfterRecovery = 1.0;

        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < blockSize; ++i)
                minimumAfterRecovery = juce::jmin (minimumAfterRecovery,
                                                   (double) buffer.getReadPointer (channel)[i]);

        if (minimumAfterRecovery < 0.999)
            ++failedToRecover;
    }

    CHECK_MSG (failedToSilence == 0,
               juce::String (failedToSilence) + " activations did not reach silence in 5 ms");

    CHECK_MSG (failedToRecover == 0,
               juce::String (failedToRecover) + " releases did not recover in 5 ms");
}

//==============================================================================
/*  The fade has to be a fade. A mute that jumps to zero would pass the timing
    test above and still click, so check the step size directly. */
LUTHIER_TEST (LiveKillSwitch, fadesRatherThanJumping)
{
    KillSwitch kill;
    kill.prepare (kSr);

    juce::AudioBuffer<float> buffer (1, 512);

    for (int i = 0; i < 512; ++i)
        buffer.getWritePointer (0)[i] = 1.0f;

    kill.setActive (true);
    kill.processBlock (buffer);

    const auto* data = buffer.getReadPointer (0);

    double worstStep = 0.0;

    for (int i = 1; i < 512; ++i)
        worstStep = juce::jmax (worstStep, std::abs ((double) data[i] - (double) data[i - 1]));

    // Three milliseconds at 48 kHz is 144 samples, so no single step may be
    // larger than roughly 1/144 of full scale.
    CHECK_MSG (worstStep < 0.02,
               "kill switch stepped by " + juce::String (worstStep, 5) + " in one sample");

    CHECK (data[0] < 1.0);
    CHECK (data[511] < 0.01);
}

//==============================================================================
/*  live-performance 6: the kill switch must not disturb the DSP underneath. The
    signal that comes back after a release is the signal that would have been
    there, not the one that was there when the switch went down. */
LUTHIER_TEST (LiveKillSwitch, doesNotDisturbTheSignalUnderneath)
{
    const int total = 4096;

    // A sine that keeps running whether or not anyone is listening to it.
    auto makeSine = [] (juce::AudioBuffer<float>& buffer, int offset, int count)
    {
        for (int i = 0; i < count; ++i)
            buffer.getWritePointer (0)[i] =
                (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0
                                    * (double) (offset + i) / kSr);
    };

    KillSwitch kill;
    kill.prepare (kSr);

    juce::AudioBuffer<float> block (1, 256);
    std::vector<float> withKill (total, 0.0f);

    for (int offset = 0; offset < total; offset += 256)
    {
        makeSine (block, offset, 256);

        // Held down through the middle of the render.
        kill.setActive (offset >= 1024 && offset < 2048);
        kill.processBlock (block);

        std::copy (block.getReadPointer (0), block.getReadPointer (0) + 256,
                   withKill.begin() + offset);
    }

    // Well after the release, the output must match the undisturbed sine.
    for (int i = 3072; i < total; ++i)
    {
        const double expected = std::sin (2.0 * juce::MathConstants<double>::pi * 220.0
                                            * (double) i / kSr);

        CHECK_NEAR (withKill[(size_t) i], expected, 1.0e-5);
    }
}

//==============================================================================
/*  live-performance 12: ten tap sequences with known intervals plus noise, each
    detected within half a bpm. */
LUTHIER_TEST (LiveTapTempo, detectsTempoWithinHalfABpm)
{
    juce::Random random (0x7a9);

    const double tempos[] = { 60.0, 72.0, 90.0, 100.0, 110.0, 120.0, 132.0, 144.0, 168.0, 180.0 };

    for (double bpm : tempos)
    {
        TapTempo tap;

        const double interval = 60.0 / bpm;

        /*  Eight taps against a steady beat, each landing up to 6 ms either side
            of where it should.

            The jitter is applied to each tap's own position rather than added to
            the gap before it. Adding it to the gap would make it accumulate -
            every tap would inherit the error of all the taps before it - and
            that is a player drifting off tempo, not a player tapping an
            imprecise but steady beat. A tempo detector is supposed to follow the
            first and reject the second, so testing it against the wrong one
            proves nothing. */
        for (int i = 0; i < 8; ++i)
            tap.tap ((double) i * interval + (random.nextDouble() - 0.5) * 0.012);

        CHECK (tap.hasTempo());

        CHECK_MSG (std::abs (tap.getTappedBpm() - bpm) <= 0.5,
                   "tapped " + juce::String (bpm) + " bpm, detected "
                     + juce::String (tap.getTappedBpm(), 3));
    }
}

//==============================================================================
/*  The other side of the previous test: a player who genuinely speeds up must be
    followed, not averaged against where they started. This is what separates a
    tempo change from jitter, and the estimator has to tell them apart. */
LUTHIER_TEST (LiveTapTempo, followsAGenuineTempoChange)
{
    TapTempo tap;

    double now = 0.0;

    // Settle at 100 bpm.
    for (int i = 0; i < 8; ++i)
    {
        tap.tap (now);
        now += 60.0 / 100.0;
    }

    CHECK_NEAR (tap.getTappedBpm(), 100.0, 0.5);

    // Then tap steadily at 140 for long enough to fill the window.
    for (int i = 0; i < 10; ++i)
    {
        tap.tap (now);
        now += 60.0 / 140.0;
    }

    CHECK_MSG (std::abs (tap.getTappedBpm() - 140.0) <= 0.5,
               "after changing to 140 bpm the estimate was "
                 + juce::String (tap.getTappedBpm(), 3));
}

//==============================================================================
/*  One badly late tap must not move the estimate, which is the whole reason the
    average is taken around a median. */
LUTHIER_TEST (LiveTapTempo, oneOutlierDoesNotMoveTheEstimate)
{
    TapTempo tap;

    const double interval = 0.5;    // 120 bpm
    double now = 0.0;

    for (int i = 0; i < 4; ++i)
    {
        tap.tap (now);
        now += interval;
    }

    const double before = tap.getTappedBpm();
    CHECK_NEAR (before, 120.0, 0.5);

    // One tap 40% late, then back on the beat.
    tap.tap (now);
    now += interval * 1.4;

    for (int i = 0; i < 3; ++i)
    {
        tap.tap (now);
        now += interval;
    }

    CHECK_MSG (std::abs (tap.getTappedBpm() - 120.0) <= 1.0,
               "one late tap moved the estimate to " + juce::String (tap.getTappedBpm(), 3));
}

//==============================================================================
/*  Range, snapping, sequence expiry and the host-wins rule. */
LUTHIER_TEST (LiveTapTempo, respectsRangeSnapAndHostPriority)
{
    TapTempo tap;

    // Taps far enough apart to be a new sequence each time never produce a tempo.
    tap.tap (0.0);
    tap.tap (10.0);
    tap.tap (20.0);
    CHECK (! tap.hasTempo());

    // A tempo above the range is rejected rather than clamped into nonsense.
    TapTempo fast;
    double now = 0.0;

    for (int i = 0; i < 6; ++i)
    {
        fast.tap (now);
        now += 60.0 / 400.0;       // 400 bpm, outside the 20-300 range
    }

    CHECK (! fast.hasTempo());

    // Snapping: taps a hair off 120 land exactly on 120.
    TapTempo snapper;
    now = 0.0;

    for (int i = 0; i < 8; ++i)
    {
        snapper.tap (now);
        now += 60.0 / 120.2;
    }

    CHECK_NEAR (snapper.getTappedBpm(), 120.0, 0.001);

    // live-performance 5: the host wins while it is rolling, unless forced.
    CHECK_NEAR (snapper.getEffectiveBpm (90.0, true), 90.0, 0.001);
    CHECK_NEAR (snapper.getEffectiveBpm (90.0, false), 120.0, 0.001);

    snapper.setForceInternal (true);
    CHECK_NEAR (snapper.getEffectiveBpm (90.0, true), 120.0, 0.001);
}

//==============================================================================
/*  live-performance 12: walk a fifty-entry setlist forwards and backwards and
    verify nothing grows per pass. */
LUTHIER_TEST (LiveSetlist, walksForwardsAndBackwardsWithoutGrowing)
{
    Setlist setlist;
    setlist.setName ("Friday");
    setlist.setDefaultBpm (128.0);

    for (int i = 0; i < 50; ++i)
    {
        SetlistEntry entry;
        entry.presetPath = "Song " + juce::String (i) + ".luthierpreset";
        entry.snapshotIndex = i % 8;
        entry.notes = "capo 2";
        setlist.addEntry (entry);
    }

    CHECK (setlist.getNumEntries() == 50);

    SetlistPlayer player;
    player.setSetlist (setlist);

    for (int pass = 0; pass < 20; ++pass)
    {
        while (player.next()) {}

        CHECK (player.getPosition() == 49);

        while (player.previous()) {}

        CHECK (player.getPosition() == 0);
    }

    // The player holds the current entry and at most one pre-loaded next entry,
    // regardless of how far it has been walked.
    CHECK (player.getCurrentEntry() != nullptr);
    CHECK (player.getPreviousEntry() == nullptr);
    CHECK (player.getNextEntry() != nullptr);
    CHECK (player.getSetlist().getNumEntries() == 50);
}

//==============================================================================
/*  The triptych the header shows: previous, current and next. */
LUTHIER_TEST (LiveSetlist, reportsPreviousCurrentAndNext)
{
    Setlist setlist;

    for (int i = 0; i < 3; ++i)
    {
        SetlistEntry entry;
        entry.presetPath = "S" + juce::String (i);
        setlist.addEntry (entry);
    }

    SetlistPlayer player;
    player.setSetlist (setlist);

    CHECK (player.getPreviousEntry() == nullptr);
    CHECK (player.getCurrentEntry() != nullptr && player.getCurrentEntry()->presetPath == "S0");
    CHECK (player.getNextEntry() != nullptr && player.getNextEntry()->presetPath == "S1");

    CHECK (player.next());

    CHECK (player.getPreviousEntry() != nullptr && player.getPreviousEntry()->presetPath == "S0");
    CHECK (player.getCurrentEntry() != nullptr && player.getCurrentEntry()->presetPath == "S1");
    CHECK (player.getNextEntry() != nullptr && player.getNextEntry()->presetPath == "S2");

    CHECK (player.next());
    CHECK (player.getNextEntry() == nullptr);
    CHECK (! player.next());
}

//==============================================================================
/*  A setlist is a file, so it has to survive being written and read. */
LUTHIER_TEST (LiveSetlist, roundTripsThroughJson)
{
    Setlist setlist;
    setlist.setName ("Saturday Late");
    setlist.setNotes ("two guitars, no bass");
    setlist.setDefaultBpm (96.0);

    for (int i = 0; i < 6; ++i)
    {
        SetlistEntry entry;
        entry.presetPath = "C:/Songs/Track " + juce::String (i) + ".luthierpreset";
        entry.snapshotIndex = i;
        entry.notes = "verse clean, chorus dirty";
        setlist.addEntry (entry);
    }

    const auto text = juce::JSON::toString (setlist.toVar(), false);

    Setlist restored;
    restored.fromVar (juce::JSON::parse (text));

    CHECK (restored.getName() == "Saturday Late");
    CHECK (restored.getNotes() == "two guitars, no bass");
    CHECK_NEAR (restored.getDefaultBpm(), 96.0, 0.001);
    CHECK (restored.getNumEntries() == 6);

    for (int i = 0; i < 6; ++i)
    {
        CHECK (restored.getEntry (i).snapshotIndex == i);
        CHECK (restored.getEntry (i).presetPath.contains ("Track " + juce::String (i)));
    }
}

//==============================================================================
/*  Reordering and removal, which is what the setlist editor does. */
LUTHIER_TEST (LiveSetlist, reordersAndRemoves)
{
    Setlist setlist;

    for (int i = 0; i < 4; ++i)
    {
        SetlistEntry entry;
        entry.presetPath = juce::String (i);
        setlist.addEntry (entry);
    }

    CHECK (setlist.moveEntry (0, 3));
    CHECK (setlist.getEntry (3).presetPath == "0");
    CHECK (setlist.getEntry (0).presetPath == "1");

    CHECK (setlist.removeEntry (0));
    CHECK (setlist.getNumEntries() == 3);
    CHECK (setlist.getEntry (0).presetPath == "2");

    CHECK (! setlist.removeEntry (99));
    CHECK (! setlist.moveEntry (0, 99));
}

//==============================================================================
/*  live-performance 8: a calibration maps a pedal's real travel onto a clean
    0 to 1, with the dead zones clamped at each end. */
LUTHIER_TEST (LiveExpression, calibrationMapsRealTravelOntoFullRange)
{
    ExpressionCalibration calibration;
    calibration.ccNumber = 11;
    calibration.rawMinimum = 12;
    calibration.rawMaximum = 118;
    calibration.heelDeadZone = 0.03;
    calibration.toeDeadZone = 0.05;
    calibration.curve = ExpressionCalibration::Curve::linear;

    // Anywhere inside the heel dead zone reads as fully back.
    CHECK_NEAR (calibration.map (12), 0.0, 0.001);
    CHECK_NEAR (calibration.map (0), 0.0, 0.001);

    // Anywhere inside the toe dead zone reads as fully forward.
    CHECK_NEAR (calibration.map (118), 1.0, 0.001);
    CHECK_NEAR (calibration.map (127), 1.0, 0.001);

    // And the travel in between is monotonic and covers the whole range.
    double previous = -1.0;

    for (int raw = 0; raw <= 127; ++raw)
    {
        const double value = calibration.map (raw);

        CHECK (value >= -0.001 && value <= 1.001);
        CHECK_MSG (value >= previous - 0.001,
                   "expression map went backwards at raw " + juce::String (raw));

        previous = value;
    }

    // An uncalibrated pedal passes straight through.
    ExpressionCalibration raw;
    raw.rawMinimum = 0;
    raw.rawMaximum = 0;
    CHECK_NEAR (raw.map (64), 64.0 / 127.0, 0.001);
}

//==============================================================================
/*  Each curve keeps the endpoints and stays monotonic. */
LUTHIER_TEST (LiveExpression, everyCurveIsMonotonicAndPinnedAtBothEnds)
{
    for (int c = 0; c < (int) ExpressionCalibration::Curve::numCurves; ++c)
    {
        ExpressionCalibration calibration;
        calibration.ccNumber = 11;
        calibration.rawMinimum = 0;
        calibration.rawMaximum = 127;
        calibration.heelDeadZone = 0.0;
        calibration.toeDeadZone = 0.0;
        calibration.curve = (ExpressionCalibration::Curve) c;

        CHECK_NEAR (calibration.map (0), 0.0, 0.001);
        CHECK_NEAR (calibration.map (127), 1.0, 0.001);

        double previous = -1.0;

        for (int value = 0; value <= 127; ++value)
        {
            const double mapped = calibration.map (value);

            CHECK_MSG (mapped >= previous - 0.001,
                       juce::String (getExpressionCurveName (calibration.curve))
                         + " went backwards at " + juce::String (value));

            previous = mapped;
        }
    }
}

//==============================================================================
/*  The calibration wizard: heel, toe, commit. */
LUTHIER_TEST (LiveExpression, wizardCapturesHeelAndToe)
{
    ExpressionCalibrationSet set;

    CHECK (! set.has (11));

    set.beginCalibration (11);
    CHECK (set.getWizardStage() == ExpressionCalibrationSet::WizardStage::heel);

    // The player rocks the pedal back; it settles around 9.
    for (int value : { 40, 22, 14, 9, 10, 9 })
        set.observe (11, value);

    CHECK (set.confirmStage() == ExpressionCalibrationSet::WizardStage::toe);

    // And forward; it settles around 119.
    for (int value : { 30, 70, 104, 119, 118, 119 })
        set.observe (11, value);

    CHECK (set.confirmStage() == ExpressionCalibrationSet::WizardStage::done);

    CHECK (set.has (11));

    const auto calibration = set.get (11);
    CHECK (calibration.rawMinimum == 9);
    CHECK (calibration.rawMaximum == 119);

    CHECK_NEAR (set.map (11, 9), 0.0, 0.001);
    CHECK_NEAR (set.map (11, 119), 1.0, 0.001);

    // A CC that was never calibrated still passes straight through.
    CHECK_NEAR (set.map (7, 127), 1.0, 0.001);

    // A pedal wired backwards is ordered rather than rejected.
    ExpressionCalibrationSet reversed;
    reversed.beginCalibration (4);

    for (int value : { 120, 127 })
        reversed.observe (4, value);

    reversed.confirmStage();

    for (int value : { 8, 2 })
        reversed.observe (4, value);

    reversed.confirmStage();

    CHECK (reversed.has (4));
    CHECK (reversed.get (4).rawMinimum < reversed.get (4).rawMaximum);
}

//==============================================================================
/*  live-performance 11: calibrations are user-global, so they round-trip through
    their own config file rather than through a preset. */
LUTHIER_TEST (LiveExpression, calibrationSetRoundTrips)
{
    ExpressionCalibrationSet set;

    for (int cc : { 4, 11, 64 })
    {
        ExpressionCalibration calibration;
        calibration.ccNumber = cc;
        calibration.rawMinimum = cc / 2;
        calibration.rawMaximum = 127 - cc / 4;
        calibration.heelDeadZone = 0.02 * (double) (cc % 5);
        calibration.toeDeadZone = 0.01 * (double) (cc % 7);
        calibration.curve = (ExpressionCalibration::Curve) (cc % 4);
        set.set (calibration);
    }

    const auto text = juce::JSON::toString (set.toVar(), false);

    ExpressionCalibrationSet restored;
    restored.fromVar (juce::JSON::parse (text));

    CHECK (restored.getCalibratedCcNumbers().size() == 3);

    for (int cc : { 4, 11, 64 })
    {
        CHECK (restored.has (cc));

        const auto a = set.get (cc);
        const auto b = restored.get (cc);

        CHECK (a.rawMinimum == b.rawMinimum);
        CHECK (a.rawMaximum == b.rawMaximum);
        CHECK (a.curve == b.curve);
        CHECK_NEAR (a.heelDeadZone, b.heelDeadZone, 0.0001);
        CHECK_NEAR (a.toeDeadZone, b.toeDeadZone, 0.0001);
    }

    restored.remove (11);
    CHECK (! restored.has (11));
    CHECK (restored.getCalibratedCcNumbers().size() == 2);
}

//==============================================================================
/*  live-performance 7: the monitor path costs nothing when there is nothing to
    monitor, and sums what there is when there is. */
LUTHIER_TEST (LiveMonitor, idleWhenNothingToMonitorAndSumsWhenThereIs)
{
    MonitorMix monitor;
    monitor.prepare (kSr, 512);

    // Level at -inf: nothing to do, whatever is plugged in.
    monitor.setLevelDb (-100.0);
    CHECK (! monitor.isActive (true, true));

    monitor.setLevelDb (0.0);
    monitor.setMainLevelDb (-100.0);

    // Level up but nothing feeding it: still nothing to do.
    CHECK (! monitor.isActive (false, false));
    CHECK (monitor.isActive (true, false));
    CHECK (monitor.isActive (false, true));

    monitor.setMainLevelDb (0.0);
    CHECK (monitor.isActive (false, false));

    // ---- and it actually sums -----------------------------------------------------
    const int numSamples = 512;

    juce::AudioBuffer<float> main (2, numSamples), sidechain (2, numSamples), out (2, numSamples);

    main.clear();
    sidechain.clear();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto phase = 2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / kSr;

        main.getWritePointer (0)[i] = main.getWritePointer (1)[i] = (float) (0.25 * std::sin (phase));
        sidechain.getWritePointer (0)[i] = sidechain.getWritePointer (1)[i] = (float) (0.25 * std::cos (phase));
    }

    monitor.setSidechainLevelDb (0.0);
    monitor.setPan (0.0);
    monitor.setEqLowDb (0.0);
    monitor.setEqMidDb (0.0);
    monitor.setEqHighDb (0.0);
    monitor.reset();

    monitor.processBlock (out, main, &sidechain, nullptr, numSamples);

    CHECK_FINITE (out.getReadPointer (0), numSamples);
    CHECK_FINITE (out.getReadPointer (1), numSamples);

    CHECK_MSG (out.getMagnitude (0, 0, numSamples) > 0.05,
               "monitor produced nothing with both sources up");

    // With the main source muted, only the sidechain should be left.
    monitor.setMainLevelDb (-100.0);
    monitor.reset();
    monitor.processBlock (out, main, &sidechain, nullptr, numSamples);

    CHECK_FINITE (out.getReadPointer (0), numSamples);
    CHECK (out.getMagnitude (0, 0, numSamples) > 0.05);

    // And with everything down, silence.
    monitor.setSidechainLevelDb (-100.0);
    monitor.setLevelDb (-100.0);
    monitor.reset();
    monitor.processBlock (out, main, &sidechain, nullptr, numSamples);

    CHECK_NEAR (out.getMagnitude (0, 0, numSamples), 0.0, 1.0e-6);
}
