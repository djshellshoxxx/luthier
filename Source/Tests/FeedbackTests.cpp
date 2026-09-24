/*  The physical feedback loop (ambiguity-resolutions.md 1, tests in 1.4). */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../Presets/FactoryPresets.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    void set (LuthierAudioProcessor& processor, const char* id, float plain)
    {
        if (auto* p = processor.getState().getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }

    void load (LuthierAudioProcessor& processor, const juce::String& name)
    {
        for (int i = 0; i < FactoryPresets::getNumPresets(); ++i)
            if (juce::String (FactoryPresets::getPreset (i).name) == name)
                processor.getPresetManager().fromVar (FactoryPresets::toVar (FactoryPresets::getPreset (i), processor));

        set (processor, ParamIDs::macroHumanize, 0.0f);
        set (processor, ParamIDs::feedbackAmount, 0.0f);
    }

    struct Run
    {
        std::vector<float> out;        ///< left channel
        std::vector<double> injection; ///< the loop's feed into the strings
        std::vector<double> di;        ///< the DI, for the circuit's attenuation
        bool everResonant = false;
        double peakActivity = 0.0;
        double lastStringLevel = 0.0;
    };

    /** Enables the aux pairs (layout B), so the DI has a bus to be read from. */
    void enableAux (LuthierAudioProcessor& processor)
    {
        auto layout = processor.getBusesLayout();

        for (int bus = 1; bus <= kNumAuxBuses; ++bus)
            layout.outputBuses.getReference (bus) = juce::AudioChannelSet::stereo();

        processor.setBusesLayout (layout);
    }

    /** Holds one note (never released) for `seconds`. */
    Run hold (LuthierAudioProcessor& processor, int midiNote, double seconds, int stringForLevel = -1,
              int velocity = 110)
    {
        enableAux (processor);
        processor.prepareToPlay (kSr, kBlock);
        processor.getEngine().reset();

        Run run;
        const int blocks = (int) (seconds * kSr / kBlock);
        juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(),
                                                     processor.getTotalNumInputChannels(), 2), kBlock);
        const int diBus = 1 + (int) AuxBus::di;

        for (int b = 0; b < blocks; ++b)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, midiNote, (juce::uint8) velocity), 0);

            processor.processBlock (buffer, midi);

            const auto* injection = processor.getEngine().getFeedbackInjection();
            const auto diOut = processor.getBusBuffer (buffer, false, diBus);
            const auto* di = diOut.getNumChannels() > 0 ? diOut.getReadPointer (0) : nullptr;

            for (int i = 0; i < kBlock; ++i)
            {
                run.out.push_back (buffer.getSample (0, i));
                run.injection.push_back (injection[i]);
                run.di.push_back (di != nullptr ? (double) di[i] : 0.0);
            }

            run.everResonant = run.everResonant || processor.getEngine().getFeedbackLoop().isResonant();
            run.peakActivity = juce::jmax (run.peakActivity, processor.getEngine().getFeedbackLoop().getActivity());
        }

        if (stringForLevel >= 0)
            run.lastStringLevel = processor.getEngine().getStringLevel (stringForLevel);

        return run;
    }

    template <typename T>
    double rmsOf (const std::vector<T>& v, size_t from, size_t to)
    {
        double sum = 0.0;
        to = juce::jmin (to, v.size());

        for (size_t i = from; i < to; ++i)
            sum += (double) v[i] * (double) v[i];

        return std::sqrt (sum / (double) juce::jmax ((size_t) 1, to - from));
    }

    /** The strongest frequency within `cents` of `centre`, by a Hann-windowed
        Goertzel scan at tenth-of-a-cent steps. */
    double peakNear (const std::vector<double>& signal, size_t from, size_t to, double centre, double cents)
    {
        const size_t n = juce::jmin (to, signal.size()) - from;
        double best = centre, bestPower = -1.0;

        for (double c = -cents; c <= cents; c += 0.1)
        {
            const double f = centre * std::pow (2.0, c / 1200.0);
            const double w = juce::MathConstants<double>::twoPi * f / kSr;
            const double coeff = 2.0 * std::cos (w);
            double s1 = 0.0, s2 = 0.0;

            for (size_t i = 0; i < n; ++i)
            {
                const double window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) i / (double) (n - 1));
                const double s0 = signal[from + i] * window + coeff * s1 - s2;
                s2 = s1;
                s1 = s0;
            }

            const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;

            if (power > bestPower)
            {
                bestPower = power;
                best = f;
            }
        }

        return best;
    }

    double centsBetween (double a, double b) { return 1200.0 * std::log2 (a / b); }
}

//==============================================================================
LUTHIER_TEST (Feedback, zeroAmountIsBitIdenticalWhateverTheOtherSettings)
{
    LuthierAudioProcessor a, b;
    load (a, "Shred Lead");
    load (b, "Shred Lead");

    // Everything but the amount set far from a's: at amount 0 none of it may matter.
    set (b, ParamIDs::feedbackDistance, 0.1f);
    set (b, ParamIDs::feedbackAngle, 120.0f);
    set (b, ParamIDs::feedbackFocus, 5.0f);
    set (b, ParamIDs::feedbackOctaveBias, 2.0f);

    const auto ra = hold (a, 64, 1.5);
    const auto rb = hold (b, 64, 1.5);

    CHECK (ra.out.size() == rb.out.size());
    CHECK_MSG (std::memcmp (ra.out.data(), rb.out.data(), ra.out.size() * sizeof (float)) == 0,
               "amount 0 is not a bypass: the other feedback settings changed the output");
    CHECK_MSG (rmsOf (ra.injection, 0, ra.injection.size()) == 0.0, "amount 0 injected something");
}

LUTHIER_TEST (Feedback, aLoudRigTakesOverAndACleanOneDoesNot)
{
    LuthierAudioProcessor processor;

    // Full amount, half a metre, a high-gain lead: the loop takes the note.
    load (processor, "Shred Lead");
    set (processor, ParamIDs::feedbackAmount, 100.0f);
    set (processor, ParamIDs::feedbackDistance, 0.5f);
    const auto loud = hold (processor, 64, 4.0);
    CHECK_MSG (loud.everResonant, "a loud high-gain rig at full amount never fed back (peak activity "
                                    + juce::String (loud.peakActivity, 4) + ", injection rms "
                                    + juce::String (rmsOf (loud.injection, 0, loud.injection.size()), 6) + ")");

    // The same rig with no feedback lets the note die away...
    load (processor, "Shred Lead");
    const auto none = hold (processor, 64, 4.0);
    const size_t tail = none.out.size() - (size_t) kSr;
    CHECK_MSG (rmsOf (loud.out, tail, loud.out.size()) > 2.0 * rmsOf (none.out, tail, none.out.size()),
               "feedback did not sustain the note beyond its natural decay");

    // ...and a clean amp with a little feedback stays clean.
    load (processor, "Clean Double-Cut Funk");
    set (processor, ParamIDs::feedbackAmount, 20.0f);
    const auto clean = hold (processor, 64, 4.0);
    CHECK_MSG (! clean.everResonant, "a clean amp at 20% ran away (peak activity "
                                       + juce::String (clean.peakActivity, 4) + ")");
}

LUTHIER_TEST (Feedback, staysBoundedForAMinuteAtFullTilt)
{
    LuthierAudioProcessor processor;
    load (processor, "Shred Lead");
    set (processor, ParamIDs::feedbackAmount, 100.0f);
    set (processor, ParamIDs::feedbackDistance, 0.5f);
    set (processor, ParamIDs::feedbackFocus, 100.0f);

    const auto run = hold (processor, 64, 60.0);

    float peak = 0.0f;
    bool finite = true;

    for (auto v : run.out)
    {
        finite = finite && std::isfinite (v);
        peak = juce::jmax (peak, std::abs (v));
    }

    CHECK_MSG (finite, "the loop produced a non-finite sample");
    CHECK_MSG (peak <= 1.0f, "the output passed full scale: " + juce::String (peak));

    // Not growing either: the last ten seconds are no louder than the ten before.
    const size_t n = run.out.size();
    const size_t ten = (size_t) (10.0 * kSr);
    CHECK (rmsOf (run.out, n - ten, n) <= 1.05 * rmsOf (run.out, n - 2 * ten, n - ten) + 1.0e-6);
}

LUTHIER_TEST (Feedback, eachStringHearsItsOwnNote)
{
    for (const int bias : { 0, 1 })
    {
        LuthierAudioProcessor processor;
        load (processor, "Shred Lead");
        set (processor, ParamIDs::feedbackAmount, 40.0f);
        set (processor, ParamIDs::feedbackFocus, 100.0f);
        set (processor, ParamIDs::feedbackOctaveBias, (float) bias);

        const auto run = hold (processor, 57, 5.0);

        // The note as the guitar actually plays it, from the DI.
        const size_t from = (size_t) kSr, to = (size_t) (5.0 * kSr);
        const double nominal = 440.0 * std::pow (2.0, (57 - 69) / 12.0);
        const double played = peakNear (run.di, from, to, nominal, 60.0);
        const double target = played * std::pow (2.0, (double) bias);
        const double fed = peakNear (run.injection, from, to, target, 60.0);

        CHECK_MSG (std::abs (centsBetween (fed, target)) <= 5.0,
                   "octave bias " + juce::String (bias) + ": the feedback peaks at " + juce::String (fed, 2)
                     + " Hz, " + juce::String (centsBetween (fed, target), 1) + " cents from "
                     + juce::String (target, 2) + " Hz");
    }
}

LUTHIER_TEST (Feedback, theVolumeKnobLowersTheLoopByTheCircuitsAttenuation)
{
    /*  A clean amp, a small amount and a moderate pluck keep the loop linear,
        so what the knob does to the DI is what it does to the loop (1.4).

        The amp is only linear at a level: at velocity 110 its attack still
        compresses a little, and the loop fell 0.4-0.5 dB less than the
        circuit - inside the tolerance by luck of where the 0.1-0.6 s window
        fell on the attack. Velocity 70 measures 0.08 dB (50: -0.06, 90: 0.25).
        The chord window is zero so the note starts at t = 0 where the window
        assumes it: the default 15 ms delayed it, moved more of the attack into
        the window, and turned that 0.41 dB into 0.51. */
    auto measure = [] (float volume)
    {
        LuthierAudioProcessor processor;
        load (processor, "Clean Double-Cut Funk");
        set (processor, ParamIDs::feedbackAmount, 15.0f);
        set (processor, ParamIDs::guitarVolume, volume);
        set (processor, ParamIDs::chordWindow, 0.0f);

        // Linear between the DI and the speaker: no compressor (this preset
        // has one before the amp), and the amp at its cleanest.
        for (int post = 0; post < 2; ++post)
            for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
                set (processor, ParamIDs::slotBypass (post == 1, slot).toRawUTF8(), 1.0f);

        set (processor, ParamIDs::ampGain, 0.0f);
        const auto run = hold (processor, 64, 1.0, -1, 70);

        const size_t from = (size_t) (0.1 * kSr), to = (size_t) (0.6 * kSr);
        return std::make_pair (rmsOf (run.di, from, to), rmsOf (run.injection, from, to));
    };

    // guitar_volume runs 0..1: "5" on the knob is half way.
    const auto [diFull, loopFull] = measure (1.0f);
    const auto [diHalf, loopHalf] = measure (0.5f);

    CHECK (diFull > 0.0 && loopFull > 0.0 && diHalf > 0.0 && loopHalf > 0.0);

    const double circuitDb = juce::Decibels::gainToDecibels (diHalf / diFull);
    const double loopDb = juce::Decibels::gainToDecibels (loopHalf / loopFull);

    CHECK_MSG (circuitDb < -1.0, "the volume knob at half did not attenuate: " + juce::String (circuitDb, 2) + " dB");
    CHECK_MSG (std::abs (loopDb - circuitDb) <= 0.5,
               "the loop fell by " + juce::String (loopDb, 2) + " dB where the circuit fell by "
                 + juce::String (circuitDb, 2) + " dB");
}

LUTHIER_TEST (Feedback, oldPresetsThatSwitchedItOnGetAnAmount)
{
    LuthierAudioProcessor processor;
    auto state = processor.getPresetManager().toVar();

    if (auto* params = state.getProperty ("parameters", {}).getDynamicObject())
    {
        params->removeProperty (ParamIDs::feedbackAmount);
        params->setProperty (ParamIDs::feedbackOn, 1.0);
    }

    CHECK (processor.getPresetManager().fromVar (state));

    auto* amount = processor.getState().getParameter (ParamIDs::feedbackAmount);
    CHECK (amount != nullptr && std::abs (amount->convertFrom0to1 (amount->getValue()) - 50.0f) < 0.5f);
}
