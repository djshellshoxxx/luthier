/*  A preset's pedals come with their settings.

    Found by the preset-morph tests: on a fresh processor, loading a preset
    wrote its pedal types and parameters, and then the structural pass built
    each new pedal and wrote its defaults over the parameters the preset had
    just set - so every factory preset's pedals played at their defaults the
    first time it was loaded. The bridge now keeps a pedal's parameters when
    they were written after its type, and writes the defaults only for a type
    written on its own (a player picking a pedal). */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Presets/FactoryPresets.h"

#include <juce_dsp/juce_dsp.h>
#include <thread>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    /*  Renders `seconds` of the processor as a host would - block by block
        through processBlock, with the parameter bridge in the loop - playing
        one note from the start, released at `noteOffAt` (never when < 0).
        `during` runs once, before the block that crosses `atSecond`: a pedal
        picked while the note sounds. Returns the mono sum of the main outputs. */
    std::vector<double> renderMono (LuthierAudioProcessor& processor, double seconds, double noteOffAt = -1.0,
                                    double atSecond = -1.0, const std::function<void()>& during = {})
    {
        const int total = (int) (kSr * seconds);
        const int channels = juce::jmax (2, processor.getTotalNumInputChannels(), processor.getTotalNumOutputChannels());

        juce::AudioBuffer<float> block (channels, kBlock);
        std::vector<double> out ((size_t) total, 0.0);

        bool onSent = false, offSent = noteOffAt < 0.0, ran = atSecond < 0.0 || ! during;

        for (int position = 0; position < total; position += kBlock)
        {
            const double t = position / kSr;
            juce::MidiBuffer midi;

            if (! onSent)
            {
                midi.addEvent (juce::MidiMessage::noteOn (1, 52, 0.9f), 0);   // E3
                onSent = true;
            }

            if (! offSent && t >= noteOffAt)
            {
                midi.addEvent (juce::MidiMessage::noteOff (1, 52), 0);
                offSent = true;
            }

            if (! ran && t >= atSecond)
            {
                during();
                ran = true;
            }

            block.clear();
            processor.processBlock (block, midi);

            const int count = juce::jmin (kBlock, total - position);

            for (int i = 0; i < count; ++i)
                out[(size_t) (position + i)] = 0.5 * ((double) block.getSample (0, i) + (double) block.getSample (1, i));
        }

        return out;
    }

    double rmsDb (const std::vector<double>& v, double fromSeconds, double toSeconds)
    {
        const int from = juce::jlimit (0, (int) v.size(), (int) (fromSeconds * kSr));
        const int to = juce::jlimit (from, (int) v.size(), (int) (toSeconds * kSr));
        return juce::Decibels::gainToDecibels (rms (v.data() + from, to - from), -120.0);
    }

    /** Energy in dB in four bands (to 300 Hz, to 1.2 kHz, to 4 kHz, above) over a window. */
    std::array<double, 4> bandsDb (const std::vector<double>& v, double fromSeconds, double toSeconds)
    {
        constexpr int order = 14, n = 1 << order;
        const int from = juce::jlimit (0, juce::jmax (0, (int) v.size() - n), (int) (fromSeconds * kSr));
        const int count = juce::jmin (n, (int) v.size() - from, (int) ((toSeconds - fromSeconds) * kSr));

        std::vector<float> data ((size_t) n * 2, 0.0f);

        for (int i = 0; i < count; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / juce::jmax (1, count - 1));
            data[(size_t) i] = (float) (v[(size_t) (from + i)] * w);
        }

        juce::dsp::FFT fft (order);
        fft.performFrequencyOnlyForwardTransform (data.data());

        const double edges[] = { 300.0, 1200.0, 4000.0, kSr * 0.5 };
        std::array<double, 4> energy {};

        for (int bin = 1; bin < n / 2; ++bin)
        {
            const double hz = bin * kSr / n;
            int band = 0;
            while (band < 3 && hz > edges[band]) ++band;
            energy[(size_t) band] += (double) data[(size_t) bin] * (double) data[(size_t) bin];
        }

        std::array<double, 4> db {};
        for (size_t b = 0; b < 4; ++b)
            db[b] = 10.0 * std::log10 (energy[b] + 1.0e-20);
        return db;
    }

    void pick (LuthierAudioProcessor& processor, bool post, int slot, PedalType type)
    {
        if (auto* p = processor.getState().getParameter (ParamIDs::slotType (post, slot)))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) (int) type));
    }

    /** A processor prepared as a host would, with the rack empty. */
    struct PreparedProcessor
    {
        PreparedProcessor()
        {
            processor.setRateAndBufferSizeDetails (kSr, kBlock);
            processor.prepareToPlay (kSr, kBlock);
        }

        LuthierAudioProcessor processor;
    };
    void loadFactory (LuthierAudioProcessor& processor, const juce::String& name)
    {
        for (int i = 0; i < FactoryPresets::getNumPresets(); ++i)
            if (juce::String (FactoryPresets::getPreset (i).name) == name)
                processor.getPresetManager().fromVar (FactoryPresets::toVar (FactoryPresets::getPreset (i), processor));

        processor.getParameterBridge().applyAllNow();
    }

    double plain (LuthierAudioProcessor& processor, const juce::String& id)
    {
        auto* p = processor.getState().getParameter (id);
        return p != nullptr ? (double) p->getValue() : -1.0;
    }
}

//==============================================================================
LUTHIER_TEST (PresetPedals, aFreshLoadKeepsThePresetsPedalSettings)
{
    // Violin Bass Grind: an overdrive in pre slot 0 at 0.22 / 0.50 / 0.55 / 0.50 / 2.0.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    loadFactory (processor, "Violin Bass Grind");

    auto* pedal = processor.getEngine().getPreEffects().getPedal (0);
    CHECK (pedal != nullptr && pedal->getType() == PedalType::Overdrive);

    if (pedal == nullptr)
        return;

    const double expected[] = { 0.22, 0.50, 0.55, 0.50, 2.0 };

    for (int i = 0; i < 5; ++i)
        CHECK_MSG (std::abs (pedal->getParameterValue (i) - expected[i]) < 1.0e-3,
                   "overdrive parameter " + juce::String (i) + " is " + juce::String (pedal->getParameterValue (i), 3)
                     + ", not the preset's " + juce::String (expected[i], 3));
}

LUTHIER_TEST (PresetPedals, pickingAPedalStillStartsItAtItsDefaults)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    // Something odd left in the slot's parameters from before.
    for (int i = 0; i < Pedal::kMaxParams; ++i)
        if (auto* p = processor.getState().getParameter (ParamIDs::slotParam (true, 3, i)))
            p->setValueNotifyingHost (0.93f);

    // The player picks a Delay: the type alone is written.
    if (auto* type = processor.getState().getParameter (ParamIDs::slotType (true, 3)))
        type->setValueNotifyingHost (type->convertTo0to1 ((float) (int) PedalType::Delay));

    processor.getParameterBridge().applyAllNow();

    auto* pedal = processor.getEngine().getPostEffects().getPedal (3);
    CHECK (pedal != nullptr && pedal->getType() == PedalType::Delay);

    if (pedal == nullptr)
        return;

    for (int i = 0; i < pedal->getNumParameters(); ++i)
    {
        const auto& d = pedal->getParameterDescriptor (i);
        CHECK_MSG (std::abs (pedal->getParameterValue (i) - d.defaultValue) < 1.0e-3,
                   juce::String (d.name) + " is " + juce::String (pedal->getParameterValue (i), 3)
                     + ", not its default " + juce::String (d.defaultValue, 3));
        CHECK (std::abs (plain (processor, ParamIDs::slotParam (true, 3, i)) - d.toNormalised (d.defaultValue)) < 1.0e-4);
    }
}

LUTHIER_TEST (PresetPedals, aSnapshotThatChangesAPedalKeepsItsSettings)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    // Snapshot 0: a chorus in post slot 5 with its rate turned right up.
    if (auto* type = processor.getState().getParameter (ParamIDs::slotType (true, 5)))
        type->setValueNotifyingHost (type->convertTo0to1 ((float) (int) PedalType::Chorus));

    processor.getParameterBridge().applyAllNow();

    if (auto* rate = processor.getState().getParameter (ParamIDs::slotParam (true, 5, 0)))
        rate->setValueNotifyingHost (0.97f);

    processor.getSnapshots().capture (0, "Chorus");

    // Then a different pedal there, and back to the snapshot.
    if (auto* type = processor.getState().getParameter (ParamIDs::slotType (true, 5)))
        type->setValueNotifyingHost (type->convertTo0to1 ((float) (int) PedalType::Phaser));

    processor.getParameterBridge().applyAllNow();
    processor.recallSnapshot (0);

    // Past the recall's crossfade, then the structural pass.
    processor.getSnapshots().advance (5.0);
    processor.getParameterBridge().applyAllNow();

    auto* pedal = processor.getEngine().getPostEffects().getPedal (5);
    CHECK (pedal != nullptr && pedal->getType() == PedalType::Chorus);
    CHECK_MSG (std::abs (plain (processor, ParamIDs::slotParam (true, 5, 0)) - 0.97) < 1.0e-3,
               "the recalled chorus's rate is " + juce::String (plain (processor, ParamIDs::slotParam (true, 5, 0)), 3));
}

//==============================================================================
/*  "None of the effects work": a pedal picked in the rack was detected on the
    audio thread and built by an AsyncUpdater pass that also reloaded every IR
    and re-snapped every string - so it arrived late, glitched the note, and
    was skipped altogether whenever anything ahead of it in that pass failed.
    A pick from the message thread now builds its pedal on the spot. */
LUTHIER_TEST (PresetPedals, aPedalPickedFromTheUiBuildsAtOnceAndIsHeard)
{
    PreparedProcessor prepared;
    auto& processor = prepared.processor;
    auto& bridge = processor.getParameterBridge();

    const auto dry = renderMono (processor, 1.0, 0.6);
    processor.getEngine().reset();

    const int instrumentPasses = bridge.getInstrumentStructurePassCount();

    // The rack's combo: the parameter, written on the message thread, nothing else.
    pick (processor, false, 0, PedalType::Overdrive);

    CHECK_MSG (processor.getEngine().getPreEffects().getSlotType (0) == PedalType::Overdrive,
               "the overdrive was not built when it was picked");
    CHECK_MSG (bridge.getInstrumentStructurePassCount() == instrumentPasses,
               "a pedal pick ran the instrument pass");

    const auto wet = renderMono (processor, 1.0, 0.6);
    CHECK_FINITE (wet.data(), (int) wet.size());

    const double rmsChange = rmsDb (wet, 0.05, 0.6) - rmsDb (dry, 0.05, 0.6);
    const auto dryBands = bandsDb (dry, 0.05, 0.4), wetBands = bandsDb (wet, 0.05, 0.4);
    double bandChange = 0.0;

    for (size_t b = 0; b < 4; ++b)
        bandChange = juce::jmax (bandChange, std::abs (wetBands[b] - dryBands[b]));

    CHECK_MSG (std::abs (rmsChange) >= 3.0 || bandChange >= 3.0,
               "the overdrive changed the output by only " + juce::String (rmsChange, 2)
                 + " dB rms and " + juce::String (bandChange, 2) + " dB in its strongest band");
}

LUTHIER_TEST (PresetPedals, aPostAmpDelayLeavesATailAfterTheNote)
{
    PreparedProcessor prepared;
    auto& processor = prepared.processor;

    const auto dry = renderMono (processor, 2.2, 0.4);
    processor.getEngine().reset();

    pick (processor, true, 0, PedalType::Delay);
    CHECK (processor.getEngine().getPostEffects().getSlotType (0) == PedalType::Delay);

    // Its knobs, turned after the pick as a player would: feedback up, mix full.
    // The per-block push carries them to the pedal; nothing else is asked.
    auto turn = [&processor] (int param, double plain)
    {
        auto* pedal = processor.getEngine().getPostEffects().getPedal (0);
        auto* p = processor.getState().getParameter (ParamIDs::slotParam (true, 0, param));

        if (pedal != nullptr && p != nullptr)
            p->setValueNotifyingHost ((float) pedal->getParameterDescriptor (param).toNormalised (plain));
    };

    turn (1, 0.9);   // Feedback
    turn (2, 1.0);   // Mix

    const auto wet = renderMono (processor, 2.2, 0.4);
    CHECK_FINITE (wet.data(), (int) wet.size());

    // Well after the string has been released, the repeats are still coming:
    // the released string alone falls away, the delay keeps feeding it back.
    const double tail = rmsDb (wet, 1.2, 2.0) - rmsDb (dry, 1.2, 2.0);
    CHECK_MSG (tail >= 6.0, "the delay's tail is only " + juce::String (tail, 2) + " dB above the empty rack's");

    // And the repeats are what they are: the tail decays slower than the released string.
    const double drySlope = rmsDb (dry, 1.6, 2.0) - rmsDb (dry, 1.2, 1.6);
    const double wetSlope = rmsDb (wet, 1.6, 2.0) - rmsDb (wet, 1.2, 1.6);
    CHECK_MSG (wetSlope > drySlope, "the delayed tail decays as fast as the dry one ("
                                       + juce::String (wetSlope, 2) + " vs " + juce::String (drySlope, 2) + " dB)");
}

LUTHIER_TEST (PresetPedals, aTypeWrittenOffTheMessageThreadIsBuiltByTheAsyncPass)
{
    // Host automation arrives on the audio thread. That path still goes
    // through the block's detection and the message-thread pass.
    PreparedProcessor prepared;
    auto& processor = prepared.processor;
    auto& bridge = processor.getParameterBridge();

    const int instrumentPasses = bridge.getInstrumentStructurePassCount();
    const int pedalPasses = bridge.getPedalStructurePassCount();

    std::thread automation ([&processor] { pick (processor, false, 2, PedalType::Compressor); });
    automation.join();

    CHECK_MSG (processor.getEngine().getPreEffects().getSlotType (2) == PedalType::None,
               "a write from another thread must not build on that thread");

    // The next block sees it and asks for the pass; nothing until the message thread runs.
    renderMono (processor, 0.05);
    CHECK (processor.getEngine().getPreEffects().getSlotType (2) == PedalType::None);
    CHECK (bridge.isStructuralChangePending());

    // The tests run without a message loop (JUCE_MODAL_LOOPS_PERMITTED is off),
    // so the AsyncUpdater's delivery is run by hand, on the message thread.
    CHECK (juce::MessageManager::existsAndIsCurrentThread());
    bridge.handlePendingStructuralChangeNow();

    CHECK_MSG (processor.getEngine().getPreEffects().getSlotType (2) == PedalType::Compressor,
               "the async pass did not build the automated pedal");
    CHECK_MSG (bridge.getPedalStructurePassCount() == pedalPasses + 1, "the pedal pass did not run once");
    CHECK_MSG (bridge.getInstrumentStructurePassCount() == instrumentPasses,
               "an automated pedal type ran the instrument pass");
}

LUTHIER_TEST (PresetPedals, aPedalPickWhileANoteSoundsDoesNotRebuildTheInstrument)
{
    PreparedProcessor prepared;
    auto& processor = prepared.processor;
    auto& bridge = processor.getParameterBridge();
    auto& engine = processor.getEngine();

    const int irLoads = engine.getIrLoadCount();
    const int instrumentPasses = bridge.getInstrumentStructurePassCount();
    const double pickAt = 0.5;

    // A flat graphic EQ: a pedal whose arrival should be inaudible, so anything
    // heard at the pick is the rebuild it must not do.
    const auto out = renderMono (processor, 1.0, -1.0, pickAt,
                                 [&processor] { pick (processor, true, 7, PedalType::GraphicEQ); });

    CHECK (engine.getPostEffects().getSlotType (7) == PedalType::GraphicEQ);
    CHECK_MSG (engine.getIrLoadCount() == irLoads, "a pedal pick reloaded an impulse response");
    CHECK_MSG (bridge.getInstrumentStructurePassCount() == instrumentPasses,
               "a pedal pick ran the instrument pass (string re-snap, IR reload)");

    // The note keeps ringing across the pick: no fade, no restart.
    const double before = rmsDb (out, pickAt - 0.06, pickAt);
    const double after = rmsDb (out, pickAt, pickAt + 0.06);
    CHECK_MSG (std::abs (after - before) < 3.0,
               "the note moved " + juce::String (after - before, 2) + " dB across the pedal pick");
}

//==============================================================================
/*  A snapshot pairing an acoustic with a pick comes back with the pick. The
    instrument pass used to write use_fingers from the guitar's category on
    every guitar change but the first, so recalling such a snapshot (or a
    setlist entry, or automating guitar_type) turned the fingers on and put
    an extra parameter write on the host mid-crossfade. Only a guitar the
    player picks sets the hand (HeaderBar's gesture; ResetStopTests). */
LUTHIER_TEST (PresetPedals, aSnapshotWithAnAcousticAndAPickKeepsThePick)
{
    PreparedProcessor prepared;
    auto& processor = prepared.processor;
    auto& bridge = processor.getParameterBridge();
    auto& engine = processor.getEngine();

    auto setChoice = [&processor] (const char* id, int index)
    {
        if (auto* p = processor.getState().getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
    };

    auto fingers = [&processor]
    {
        return processor.getState().getRawParameterValue (ParamIDs::useFingers)->load() > 0.5f;
    };

    // An acoustic, loaded through the bridge's pass as any parameter write is.
    setChoice (ParamIDs::guitarType, (int) GuitarType::Dreadnought);
    renderMono (processor, 0.02);
    bridge.handlePendingStructuralChangeNow();
    CHECK_MSG (engine.getGuitarSpec().category == GuitarCategory::Acoustic, "the dreadnought did not load");

    // ...played with a pick, and kept that way in snapshot 1.
    if (auto* p = processor.getState().getParameter (ParamIDs::useFingers))
        p->setValueNotifyingHost (0.0f);

    processor.getSnapshots().setCrossfadeMs (0.0);
    CHECK (processor.captureSnapshot (0, "Dreadnought with a pick"));

    // Then an electric.
    setChoice (ParamIDs::guitarType, (int) GuitarType::Stratocaster);
    renderMono (processor, 0.02);
    bridge.handlePendingStructuralChangeNow();
    CHECK (engine.getGuitarSpec().category != GuitarCategory::Acoustic);

    // The recall brings the acoustic back - with the pick it was saved with.
    CHECK (processor.recallSnapshot (0));
    renderMono (processor, 0.02);
    bridge.handlePendingStructuralChangeNow();

    CHECK_MSG (engine.getGuitarSpec().category == GuitarCategory::Acoustic, "the recall did not bring the acoustic back");
    CHECK_MSG (! fingers(), "the recalled snapshot's pick was overwritten by the guitar's fingers");

    // Automation of guitar_type is not a pick either.
    setChoice (ParamIDs::guitarType, (int) GuitarType::Classical);
    renderMono (processor, 0.02);
    bridge.handlePendingStructuralChangeNow();
    CHECK_MSG (! fingers(), "automating guitar_type wrote use_fingers");
}
