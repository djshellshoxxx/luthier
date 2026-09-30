/*  Routing, multi-out, sidechain and MIDI-out tests (routing-io.md section 10).

    These run against the real engine and a real AudioProcessor, because the
    thing being tested is precisely the seam between them: the engine renders
    taps, the processor turns them into host buses, and a test that mocked
    either side would prove nothing about the pair.
*/

#include "TestFramework.h"

#include "../LuthierEngine.h"
#include "../Routing/RoutingMatrix.h"
#include "../Routing/MidiOutRouter.h"
#include "../DSP/Amp/AmpEngine.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    /** A processor that advertises the same buses the plugin does, so the
        distribution code can be exercised without the plugin wrapper, the
        parameter tree or the preset system. */
    class RoutingHarness : public juce::AudioProcessor
    {
    public:
        explicit RoutingHarness (BusLayout layout)
            : AudioProcessor (buildProperties (layout))
        {
        }

        static BusesProperties buildProperties (BusLayout layout)
        {
            auto props = BusesProperties()
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                           .withInput ("Sidechain", juce::AudioChannelSet::stereo(),
                                       true);

            if (RoutingMatrix::layoutHasAux (layout))
                for (int bus = 0; bus < kNumAuxBuses; ++bus)
                    props = props.withOutput (getAuxBusName (bus),
                                              juce::AudioChannelSet::stereo(), true);

            if (RoutingMatrix::layoutHasPerString (layout))
                for (int s = 0; s < kNumPerStringBuses; ++s)
                    props = props.withOutput ("String " + juce::String (s + 1),
                                              juce::AudioChannelSet::mono(), true);

            return props;
        }

        void prepareToPlay (double sampleRate, int blockSize) override
        {
            engine.prepare (sampleRate, blockSize);
            routing.prepare (sampleRate, blockSize);
            sidechainCopy.setSize (2, blockSize, false, true, false);
            sidechainCopy.clear();
        }

        void releaseResources() override { engine.releaseResources(); }

        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
        {
            const int numSamples = buffer.getNumSamples();

            routing.updateWantedTaps (engine.getTapBuffers(), engine.getNumStrings());

            // The input and output buses share channels in the host buffer, so
            // the sidechain must be copied out before the engine writes over it.
            auto sidechainIn = getBusBuffer (buffer, true, 0);
            const int scChannels = juce::jmin (sidechainIn.getNumChannels(),
                                               sidechainCopy.getNumChannels());

            if (scChannels > 0)
            {
                for (int ch = 0; ch < scChannels; ++ch)
                    sidechainCopy.copyFrom (ch, 0, sidechainIn, ch, 0, numSamples);

                engine.setSidechainInput (sidechainCopy.getArrayOfReadPointers(),
                                          scChannels, numSamples);
            }
            else
            {
                engine.setSidechainInput (nullptr, 0, 0);
            }

            {
                auto mainOut = getBusBuffer (buffer, false, 0);
                engine.processBlock (mainOut, midi);
            }

            routing.distribute (*this, buffer, engine.getTapBuffers(), engine.getNumStrings());
        }

        const juce::String getName() const override { return "RoutingHarness"; }
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return true; }
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

        LuthierEngine engine;
        RoutingMatrix routing;
        juce::AudioBuffer<float> sidechainCopy;
    };

    /** Plays one note, so there is something on every tap to look at. */
    void addNote (juce::MidiBuffer& midi, int note, int offset)
    {
        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.85f), offset);
    }

    double peakOf (const float* data, int numSamples)
    {
        double peak = 0.0;

        for (int i = 0; i < numSamples; ++i)
            peak = juce::jmax (peak, std::abs ((double) data[i]));

        return peak;
    }

    double dbOf (double linear)
    {
        return (linear <= 1.0e-12) ? -240.0 : 20.0 * std::log10 (linear);
    }
}

//==============================================================================
/*  Test 1: every advertised layout instantiates cleanly and produces the
    expected sample count, finite, on every output it declares. */
LUTHIER_TEST (Routing, everyLayoutRendersCleanly)
{
    const BusLayout layouts[] = { BusLayout::stereoOnly, BusLayout::studio,
                                  BusLayout::perString, BusLayout::full };

    for (auto layout : layouts)
    {
        RoutingHarness harness (layout);
        harness.setRateAndBufferSizeDetails (kSr, kBlock);
        harness.prepareToPlay (kSr, kBlock);
        harness.routing.setActiveLayout (layout);

        const int totalChannels = juce::jmax (harness.getTotalNumInputChannels(),
                                              harness.getTotalNumOutputChannels());

        juce::AudioBuffer<float> buffer (totalChannels, kBlock);
        buffer.clear();

        juce::MidiBuffer midi;
        addNote (midi, 52, 0);

        // Long enough for the note to be well under way on every tap.
        for (int block = 0; block < 24; ++block)
        {
            harness.processBlock (buffer, midi);
            midi.clear();

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                CHECK_FINITE (buffer.getReadPointer (ch), kBlock);
        }

        // Layouts B, C and D must actually declare the extra buses they promise.
        const int extraBuses = harness.getBusCount (false) - 1;

        if (layout == BusLayout::stereoOnly)
            CHECK (extraBuses == 0);
        else if (layout == BusLayout::studio)
            CHECK (extraBuses == kNumAuxBuses);
        else if (layout == BusLayout::perString)
            CHECK (extraBuses == kNumPerStringBuses);
        else
            CHECK (extraBuses == kNumAuxBuses + kNumPerStringBuses);

        // The main output must work at every layout - rule 2 of section 0.
        auto mainOut = harness.getBusBuffer (buffer, false, 0);
        CHECK (mainOut.getNumChannels() == 2);
        CHECK (mainOut.getNumSamples() == kBlock);
        CHECK_MSG (peakOf (mainOut.getReadPointer (0), kBlock) > 1.0e-6,
                   "main output silent at layout " + juce::String (getBusLayoutName (layout)));
    }
}

//==============================================================================
/*  Test 2: Aux 1 carries the DI. Re-applying the amp module to it externally
    reproduces Aux 2, the pre-cabinet amp output, to better than -60 dBFS.

    This is the null test from section 10. It works because the two taps sit
    either side of exactly one module: if Aux 1 were taken from the wrong point,
    or Aux 2 included something Aux 1 did not, the residual would not null. */
LUTHIER_TEST (Routing, diTapNullsAgainstReappliedAmp)
{
    RoutingHarness harness (BusLayout::studio);
    harness.setRateAndBufferSizeDetails (kSr, kBlock);
    harness.prepareToPlay (kSr, kBlock);
    harness.routing.setActiveLayout (BusLayout::studio);

    // No pedals in front of the amp, so the only thing between the two taps is
    // the amp itself.
    harness.engine.getPreEffects().clear();

    // A second amp in the same configuration, fed the DI by hand.
    AmpEngine reference;
    reference.prepare (kSr, kBlock);
    reference.setModel (harness.engine.getAmpEngine().getModel());
    reference.reset();

    juce::AudioBuffer<float> buffer (harness.getTotalNumOutputChannels(), kBlock);
    buffer.clear();

    juce::MidiBuffer midi;
    addNote (midi, 45, 0);

    double worstResidualDb = -240.0;
    double signalPeak = 0.0;

    for (int block = 0; block < 32; ++block)
    {
        harness.processBlock (buffer, midi);
        midi.clear();

        auto di = harness.getBusBuffer (buffer, false, 1 + (int) AuxBus::di);
        auto preCab = harness.getBusBuffer (buffer, false, 1 + (int) AuxBus::ampPreCab);

        const auto* diData = di.getReadPointer (0);
        const auto* preCabData = preCab.getReadPointer (0);

        // Skip the first blocks: the two amps start from the same reset state,
        // but the engine's amp has already seen the silence of block zero.
        if (block < 2)
        {
            for (int i = 0; i < kBlock; ++i)
                reference.processSample ((double) diData[i]);

            continue;
        }

        for (int i = 0; i < kBlock; ++i)
        {
            const double expected = reference.processSample ((double) diData[i]);
            const double actual = (double) preCabData[i];

            signalPeak = juce::jmax (signalPeak, std::abs (actual));
            worstResidualDb = juce::jmax (worstResidualDb, dbOf (std::abs (actual - expected)));
        }
    }

    CHECK_MSG (signalPeak > 1.0e-4, "pre-cab tap carried no signal to null against");
    CHECK_MSG (worstResidualDb < -60.0,
               "DI re-amp residual " + juce::String (worstResidualDb, 1)
                 + " dBFS, expected below -60");
}

//==============================================================================
/*  Test 3: the per-string outputs sum to the signal entering the body, to
    better than -80 dBFS, and strings the guitar does not have are silent. */
LUTHIER_TEST (Routing, perStringOutputsSumToPreBody)
{
    RoutingHarness harness (BusLayout::perString);
    harness.setRateAndBufferSizeDetails (kSr, kBlock);
    harness.prepareToPlay (kSr, kBlock);
    harness.routing.setActiveLayout (BusLayout::perString);

    const int numStrings = harness.engine.getNumStrings();
    CHECK (numStrings == 6);

    juce::AudioBuffer<float> buffer (harness.getTotalNumOutputChannels(), kBlock);
    buffer.clear();

    juce::MidiBuffer midi;

    // A chord, so several strings are ringing at once - the case where a
    // mis-indexed per-string bus would go unnoticed with a single note.
    for (int note : { 40, 47, 52, 56, 59, 64 })
        addNote (midi, note, 0);

    double worstResidualDb = -240.0;
    double sumPeak = 0.0;

    for (int block = 0; block < 16; ++block)
    {
        harness.processBlock (buffer, midi);
        midi.clear();

        const auto* preBody = harness.engine.getPreBodyBuffer();
        const int n = juce::jmin (kBlock, harness.engine.getLastSubBlockNumSamples());

        // The engine normalises the string sum by 1/sqrt(numStrings) so that a
        // twelve-string is not twice as loud as a six. The per-string taps are
        // the raw string outputs, so the same normalisation applies here.
        const double norm = 1.0 / std::sqrt ((double) juce::jmax (1, numStrings));

        for (int i = 0; i < n; ++i)
        {
            double sum = 0.0;

            for (int s = 0; s < numStrings; ++s)
                sum += (double) harness.getBusBuffer (buffer, false, 1 + s).getReadPointer (0)[i];

            const double expected = preBody[i];
            const double actual = sum * norm;

            sumPeak = juce::jmax (sumPeak, std::abs (expected));
            worstResidualDb = juce::jmax (worstResidualDb, dbOf (std::abs (actual - expected)));
        }

        // Strings the instrument does not have must be silent, not stale.
        for (int s = numStrings; s < kNumPerStringBuses; ++s)
        {
            auto unused = harness.getBusBuffer (buffer, false, 1 + s);
            CHECK_MSG (peakOf (unused.getReadPointer (0), kBlock) == 0.0,
                       "unused string bus " + juce::String (s + 1) + " was not silent");
        }
    }

    CHECK_MSG (sumPeak > 1.0e-4, "pre-body signal was silent, nothing to compare");

    // The residual is not exactly zero: the engine adds whammy spring noise to
    // the instrument bus after summing the strings, which by design is not part
    // of any one string. -80 dBFS is the spec's bar.
    CHECK_MSG (worstResidualDb < -80.0,
               "per-string sum residual " + juce::String (worstResidualDb, 1)
                 + " dBFS, expected below -80");
}

//==============================================================================
/*  Test 4: MIDI-out pass-through is sample-exact over a large random fuzz. */
LUTHIER_TEST (Routing, midiOutPassThroughIsSampleExact)
{
    MidiOutRouter router;
    router.prepare (kSr, kBlock);

    MidiOutConfig cfg;
    cfg.enabled = true;
    cfg.passThrough = true;
    cfg.rhythmEngine = false;
    cfg.stringActivity = false;
    cfg.ccBroadcast = false;

    StringActivityQueue emptyActivity;

    juce::Random rng (0x5EED1234);

    int eventsChecked = 0;
    int mismatches = 0;

    // 10 000 events, as the spec asks, spread over blocks of realistic size.
    while (eventsChecked < 10000)
    {
        juce::MidiBuffer input;

        const int numEvents = 1 + rng.nextInt (24);
        std::vector<std::pair<int, juce::MidiMessage>> expected;

        for (int e = 0; e < numEvents; ++e)
        {
            const int offset = rng.nextInt (kBlock);
            const int channel = 1 + rng.nextInt (16);

            juce::MidiMessage message;

            switch (rng.nextInt (4))
            {
                case 0:  message = juce::MidiMessage::noteOn (channel, rng.nextInt (128),
                                                              (juce::uint8) (1 + rng.nextInt (127))); break;
                case 1:  message = juce::MidiMessage::noteOff (channel, rng.nextInt (128)); break;
                case 2:  message = juce::MidiMessage::controllerEvent (channel, rng.nextInt (128),
                                                                       rng.nextInt (128)); break;
                default: message = juce::MidiMessage::pitchWheel (channel, rng.nextInt (16384)); break;
            }

            input.addEvent (message, offset);
        }

        // MidiBuffer sorts by timestamp, so the expectation is read back from the
        // buffer rather than from the order the events were generated in.
        for (const auto metadata : input)
            expected.emplace_back (metadata.samplePosition, metadata.getMessage());

        router.captureInput (input);

        juce::MidiBuffer output = input;
        router.emit (output, cfg, emptyActivity, kBlock);

        size_t index = 0;

        for (const auto metadata : output)
        {
            if (index >= expected.size())
            {
                ++mismatches;
                break;
            }

            const auto& want = expected[index];

            // Zero samples of drift, and the same bytes.
            if (metadata.samplePosition != want.first
                  || metadata.numBytes != want.second.getRawDataSize()
                  || std::memcmp (metadata.data, want.second.getRawData(),
                                  (size_t) metadata.numBytes) != 0)
            {
                ++mismatches;
            }

            ++index;
            ++eventsChecked;
        }

        if (index != expected.size())
            ++mismatches;
    }

    CHECK (eventsChecked >= 10000);
    CHECK_MSG (mismatches == 0,
               juce::String (mismatches) + " pass-through events differed from the input");
}

//==============================================================================
/*  Test 5: MIDI out emits nothing at all when it is disabled, including the
    host's own events - an instrument that echoed its input by accident would
    double every note in the host's recording. */
LUTHIER_TEST (Routing, midiOutDisabledEmitsNothing)
{
    MidiOutRouter router;
    router.prepare (kSr, kBlock);

    MidiOutConfig cfg;
    cfg.enabled = false;

    StringActivityQueue emptyActivity;

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
    buffer.addEvent (juce::MidiMessage::noteOff (1, 60), 100);

    router.captureInput (buffer);
    router.emit (buffer, cfg, emptyActivity, kBlock);

    CHECK (buffer.isEmpty());
}

//==============================================================================
/*  Test 6: string activity reports the strings that are actually ringing, at
    the sample they started, not stacked on sample zero. */
LUTHIER_TEST (Routing, stringActivityIsSampleAccurate)
{
    LuthierEngine engine;
    engine.prepare (kSr, kBlock);

    // A strum spreads the chord over tens of milliseconds, which is the case
    // that block-quantised timestamps would flatten.
    // 14 ms per string spreads a six-string chord over ~70 ms, which is several
    // blocks: exactly the case that block-quantised timestamps would flatten.
    engine.getMidiInterpreter().setStrumSpeedMs (14.0);

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;

    for (int note : { 40, 47, 52, 56, 59, 64 })
        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

    int totalNoteOns = 0;
    int distinctOffsets = 0;
    juce::SortedSet<int> offsetsSeen;

    // A six-string strum at 14 ms per string runs for about 70 ms, which is
    // thirteen blocks at this size. Running fewer would simply cut the strum
    // off half way and prove nothing about its timing.
    const int strumBlocks = (int) std::ceil ((14.0e-3 * 6.0 * kSr) / (double) kBlock) + 8;

    for (int block = 0; block < strumBlocks; ++block)
    {
        buffer.clear();
        engine.processBlock (buffer, midi);
        midi.clear();

        const auto& activity = engine.getStringActivity();

        for (int i = 0; i < activity.size(); ++i)
        {
            const auto& e = activity[i];

            CHECK (e.sampleOffset >= 0 && e.sampleOffset < kBlock);
            CHECK (e.stringIndex >= 0 && e.stringIndex < engine.getNumStrings());
            CHECK (e.midiNote >= 0 && e.midiNote <= 127);

            if (e.isNoteOn)
            {
                ++totalNoteOns;
                offsetsSeen.add (e.sampleOffset);
            }
        }
    }

    distinctOffsets = offsetsSeen.size();

    juce::StringArray offsetText;

    for (int i = 0; i < offsetsSeen.size(); ++i)
        offsetText.add (juce::String (offsetsSeen[i]));

    CHECK_MSG (totalNoteOns == 6,
               "expected six strings to start ringing, saw " + juce::String (totalNoteOns));

    // The point of the test: a strum is spread across samples, so the six
    // note-ons cannot all carry the same offset. If they did, every event would
    // have been quantised to a block boundary.
    CHECK_MSG (distinctOffsets > 1,
               "a strum produced " + juce::String (distinctOffsets)
                 + " distinct offset(s) [" + offsetText.joinIntoString (", ")
                 + "]; events are being stacked on one sample");
}

//==============================================================================
/*  Test 7: sidechain-to-amp replaces the string engine at the amp input, so the
    DI tap carries the sidechain and not the guitar. */
LUTHIER_TEST (Routing, sidechainToAmpReplacesTheInstrument)
{
    RoutingHarness harness (BusLayout::studio);
    harness.setRateAndBufferSizeDetails (kSr, kBlock);
    harness.prepareToPlay (kSr, kBlock);
    harness.routing.setActiveLayout (BusLayout::studio);
    harness.engine.setSidechainToAmp (true);

    juce::AudioBuffer<float> buffer (harness.getTotalNumOutputChannels(), kBlock);

    juce::MidiBuffer midi;
    addNote (midi, 45, 0);

    double worstDiff = 0.0;
    double sidechainPeak = 0.0;

    for (int block = 0; block < 12; ++block)
    {
        buffer.clear();

        // A sine on the sidechain, standing in for a rendered DI clip.
        auto sidechain = harness.getBusBuffer (buffer, true, 0);

        for (int i = 0; i < kBlock; ++i)
        {
            const double phase = 2.0 * juce::MathConstants<double>::pi * 220.0
                                   * (double) (block * kBlock + i) / kSr;
            const auto value = (float) (0.4 * std::sin (phase));

            for (int ch = 0; ch < sidechain.getNumChannels(); ++ch)
                sidechain.getWritePointer (ch)[i] = value;
        }

        harness.processBlock (buffer, midi);
        midi.clear();

        auto di = harness.getBusBuffer (buffer, false, 1 + (int) AuxBus::di);
        const auto* diData = di.getReadPointer (0);

        if (block < 2)
            continue;

        for (int i = 0; i < kBlock; ++i)
        {
            const double phase = 2.0 * juce::MathConstants<double>::pi * 220.0
                                   * (double) (block * kBlock + i) / kSr;
            const double expected = 0.4 * std::sin (phase);

            sidechainPeak = juce::jmax (sidechainPeak, std::abs (expected));
            worstDiff = juce::jmax (worstDiff, std::abs ((double) diData[i] - expected));
        }
    }

    CHECK (sidechainPeak > 0.3);
    CHECK_MSG (dbOf (worstDiff) < -80.0,
               "DI tap did not carry the sidechain; worst difference "
                 + juce::String (dbOf (worstDiff), 1) + " dBFS");
}

//==============================================================================
/*  Test 8: muting an aux bus silences it and, because a muted tap is never
    rendered, costs nothing. Soloing one bus mutes the rest. */
LUTHIER_TEST (Routing, muteAndSoloResolveTogether)
{
    RoutingHarness harness (BusLayout::studio);
    harness.setRateAndBufferSizeDetails (kSr, kBlock);
    harness.prepareToPlay (kSr, kBlock);
    harness.routing.setActiveLayout (BusLayout::studio);

    auto& routing = harness.routing;

    CHECK (routing.isAuxAudible ((int) AuxBus::di));
    CHECK (! routing.isAnyAuxSoloed());

    routing.setAuxMuted ((int) AuxBus::di, true);
    CHECK (! routing.isAuxAudible ((int) AuxBus::di));
    CHECK (routing.isAuxAudible ((int) AuxBus::roomMic));

    // Solo overrides mute, everywhere: the soloed bus is heard even though the
    // DI is still flagged muted, and every other bus goes quiet.
    routing.setAuxSoloed ((int) AuxBus::cabMic1, true);
    CHECK (routing.isAnyAuxSoloed());
    CHECK (routing.isAuxAudible ((int) AuxBus::cabMic1));
    CHECK (! routing.isAuxAudible ((int) AuxBus::roomMic));
    CHECK (! routing.isAuxAudible ((int) AuxBus::di));

    routing.setAuxSoloed ((int) AuxBus::cabMic1, false);
    routing.setAuxMuted ((int) AuxBus::di, false);

    // A muted bus must not be rendered at all.
    routing.setAuxMuted ((int) AuxBus::wetFx, true);
    routing.updateWantedTaps (harness.engine.getTapBuffers(), harness.engine.getNumStrings());

    CHECK (! harness.engine.getTapBuffers().isAuxWanted ((int) AuxBus::wetFx));
    CHECK (harness.engine.getTapBuffers().isAuxWanted ((int) AuxBus::di));

    // And it must come out silent, not merely unrendered.
    juce::AudioBuffer<float> buffer (harness.getTotalNumOutputChannels(), kBlock);
    juce::MidiBuffer midi;
    addNote (midi, 50, 0);

    for (int block = 0; block < 16; ++block)
    {
        buffer.clear();
        harness.processBlock (buffer, midi);
        midi.clear();
    }

    auto muted = harness.getBusBuffer (buffer, false, 1 + (int) AuxBus::wetFx);
    CHECK_MSG (peakOf (muted.getReadPointer (0), kBlock) == 0.0, "muted aux bus was not silent");
}

//==============================================================================
/*  Test 9: the per-output latency report is internally consistent. Every aux tap
    is a prefix of the main chain, so none of them can be later than the main
    output, and the per-string taps - which sit in front of everything - must be
    the smallest of all. */
LUTHIER_TEST (Routing, perOutputLatencyIsConsistent)
{
    LuthierEngine engine;
    engine.prepare (kSr, kBlock);

    const int main = engine.getLatencySamples();
    const int di = engine.getLatencySamples (AuxBus::di);
    const int preCab = engine.getLatencySamples (AuxBus::ampPreCab);
    const int mic = engine.getLatencySamples (AuxBus::cabMic1);
    const int perString = engine.getPerStringLatencySamples();

    CHECK (main >= 0);
    CHECK (perString >= 0);

    CHECK_MSG (perString <= di, "per-string latency exceeded the DI tap's");
    CHECK_MSG (di <= preCab, "DI latency exceeded the pre-cab tap's");
    CHECK_MSG (preCab <= mic, "pre-cab latency exceeded the cabinet tap's");
    CHECK_MSG (mic <= main, "a tap reported more latency than the main output");

    // The room and wet taps are at the end of the chain but ahead of the
    // master's look-ahead line; the monitor is post-master, so it carries the
    // main output's latency exactly (SPEC-SWEEP: EN-95).
    CHECK (engine.getLatencySamples (AuxBus::roomMic) == main - engine.getMasterBus().getLatencySamples());
    CHECK (engine.getLatencySamples (AuxBus::wetFx) == main - engine.getMasterBus().getLatencySamples());
    CHECK (engine.getLatencySamples (AuxBus::monitor) == main);
}

//==============================================================================
/*  Test 10: routing state survives a save and load, and the bus layout does not
    travel with it - the host owns that. */
LUTHIER_TEST (Routing, stateRoundTrips)
{
    RoutingMatrix saved;
    saved.prepare (kSr, kBlock);

    saved.setAuxMuted ((int) AuxBus::cabMic2, true);
    saved.setAuxSoloed ((int) AuxBus::roomMic, true);
    saved.setAuxGainDb ((int) AuxBus::di, -6.5);
    saved.setPerStringMuted (3, true);
    saved.setPerStringGainDb (1, 4.25);
    saved.setSidechainToAmp (true);
    saved.setActiveLayout (BusLayout::full);

    MidiOutConfig cfg;
    cfg.enabled = true;
    cfg.passThrough = false;
    cfg.stringActivity = true;
    cfg.ccBroadcast = true;
    cfg.channel = 7;
    cfg.macroCc[0] = 21;
    cfg.macroCc[4] = 74;
    saved.setMidiOutConfig (cfg);

    const auto state = saved.toVar();

    RoutingMatrix loaded;
    loaded.prepare (kSr, kBlock);
    loaded.setActiveLayout (BusLayout::stereoOnly);
    loaded.fromVar (state);

    CHECK (loaded.isAuxMuted ((int) AuxBus::cabMic2));
    CHECK (loaded.isAuxSoloed ((int) AuxBus::roomMic));
    CHECK_NEAR (loaded.getAuxGainDb ((int) AuxBus::di), -6.5, 1.0e-4);
    CHECK (loaded.isPerStringMuted (3));
    CHECK_NEAR (loaded.getPerStringGainDb (1), 4.25, 1.0e-4);
    CHECK (loaded.isSidechainToAmp());

    const auto restored = loaded.getMidiOutConfig();
    CHECK (restored.enabled);
    CHECK (! restored.passThrough);
    CHECK (restored.stringActivity);
    CHECK (restored.ccBroadcast);
    CHECK (restored.channel == 7);
    CHECK (restored.macroCc[0] == 21);
    CHECK (restored.macroCc[4] == 74);
    CHECK (restored.macroCc[1] == -1);

    // Layout is the host's, not the preset's.
    CHECK_MSG (loaded.getActiveLayout() == BusLayout::stereoOnly,
               "loading a preset changed the negotiated bus layout");
}

//==============================================================================
/*  Test 11: loading a preset that says nothing about routing returns routing to
    its defaults rather than leaving the previous preset's state behind. */
LUTHIER_TEST (Routing, loadingClearsPreviousState)
{
    RoutingMatrix matrix;
    matrix.prepare (kSr, kBlock);

    matrix.setAuxMuted ((int) AuxBus::di, true);
    matrix.setPerStringGainDb (2, -12.0);

    RoutingMatrix fresh;
    fresh.prepare (kSr, kBlock);
    matrix.fromVar (fresh.toVar());

    CHECK (! matrix.isAuxMuted ((int) AuxBus::di));
    CHECK_NEAR (matrix.getPerStringGainDb (2), 0.0, 1.0e-6);
}
