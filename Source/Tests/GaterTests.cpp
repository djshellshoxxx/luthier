/*  The Gater pedal: a rhythmic (trance) gate in the mod roster, and the live
    LED the rack drives from its gateOpenness atomic. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../DSP/Effects/PedalsMod.h"
#include "../UI/PedalRack.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    enum { kRate = 0, kSize, kShape, kFrequency };
    constexpr double kFree = 0.0;

    /** A pedal at the given settings, free-running at `hz`. */
    void setUp (GaterPedal& pedal, double hz, double sizePercent, double shape)
    {
        pedal.prepare (kSr, kBlock);
        pedal.setParameterValue (kRate, kFree);
        pedal.setParameterValue (kFrequency, hz);
        pedal.setParameterValue (kSize, sizePercent);
        pedal.setParameterValue (kShape, shape);
        pedal.reset();
    }

    /** Runs a constant (DC) signal through the pedal in blocks, so the output
        is the gate's gain curve itself. */
    std::vector<double> runDc (GaterPedal& pedal, int numSamples, double level = 1.0)
    {
        std::vector<double> l ((size_t) numSamples, level), r ((size_t) numSamples, level);

        for (int at = 0; at < numSamples; at += kBlock)
            pedal.process (l.data() + at, r.data() + at, juce::jmin (kBlock, numSamples - at));

        return l;
    }

    double fractionAbove (const std::vector<double>& v, double threshold)
    {
        int n = 0;

        for (auto x : v)
            if (x > threshold)
                ++n;

        return (double) n / (double) juce::jmax ((size_t) 1, v.size());
    }

    double maxStep (const std::vector<double>& v)
    {
        double m = 0.0;

        for (size_t i = 1; i < v.size(); ++i)
            m = juce::jmax (m, std::abs (v[i] - v[i - 1]));

        return m;
    }

    void setPlain (LuthierAudioProcessor& processor, const juce::String& id, float plain)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }

    juce::Image snapshot (juce::Component& c)
    {
        juce::Image image (juce::Image::ARGB, juce::jmax (1, c.getWidth()), juce::jmax (1, c.getHeight()), true,
                           juce::SoftwareImageType());
        juce::Graphics g (image);
        c.paintEntireComponent (g, true);
        return image;
    }
}

//==============================================================================
LUTHIER_TEST (Gater, isAnAppendedPostAmpModPedal)
{
    CHECK (Pedal::isPostAmpPedal (PedalType::Gater));
    CHECK (! Pedal::isPreAmpPedal (PedalType::Gater));
    CHECK (Parameters::pedalTypeNames().contains ("Gater"));

    // Appended after the Doubler: every pedal saved before it keeps its index.
    CHECK ((int) PedalType::Gater == (int) PedalType::Doubler + 1);
    CHECK ((int) PedalType::Gater == (int) PedalType::NumTypes - 1);

    auto pedal = Pedal::create (PedalType::Gater);
    CHECK (pedal != nullptr && pedal->getType() == PedalType::Gater);

    if (pedal != nullptr)
    {
        CHECK (pedal->getNumParameters() == 4);
        CHECK (juce::String (pedal->getParameterDescriptor (kRate).name) == "Rate");
        CHECK (juce::String (pedal->getParameterDescriptor (kSize).name) == "Size");
        CHECK (juce::String (pedal->getParameterDescriptor (kShape).name) == "Shape");
        CHECK (juce::String (pedal->getParameterDescriptor (kFrequency).name) == "Frequency");

        CHECK (pedal->getParameterDescriptor (kSize).minValue == 1.0 && pedal->getParameterDescriptor (kSize).maxValue == 99.0);
        CHECK (pedal->getParameterDescriptor (kShape).minValue == 0.0 && pedal->getParameterDescriptor (kShape).maxValue == 1.0);
        CHECK (juce::String (pedal->getParameterDescriptor (kFrequency).unit) == "Hz");
    }
}

/*  (a) The output is chopped at the set rate: 4 Hz free-running at 48 kHz is a
    12000-sample cycle, open for the first half and shut for the second, and
    the open windows are louder than the closed ones. */
LUTHIER_TEST (Gater, chopsTheSignalAtTheSetRate)
{
    GaterPedal pedal;
    setUp (pedal, 4.0, 50.0, 0.0);
    CHECK_NEAR (pedal.getEffectiveRateHz(), 4.0, 1.0e-9);

    const auto out = runDc (pedal, (int) kSr);   // one second: four cycles

    // Count the falling edges (the gate starts open, so the first rise is at
    // sample 0 and has no predecessor): one per cycle.
    int falls = 0;

    for (size_t i = 1; i < out.size(); ++i)
        if (out[i - 1] >= 0.5 && out[i] < 0.5)
            ++falls;

    CHECK_MSG (falls == 4, "expected 4 gate cycles in a second at 4 Hz, got " + juce::String (falls));

    // Each cycle: the open half passes the signal, the shut half silences it.
    const int cycle = 12000;

    for (int c = 0; c < 4; ++c)
    {
        double open = 0.0, shut = 0.0;

        for (int i = 0; i < cycle / 2; ++i)
        {
            open += out[(size_t) (c * cycle + i)];
            shut += out[(size_t) (c * cycle + cycle / 2 + i)];
        }

        open /= cycle / 2;
        shut /= cycle / 2;

        CHECK_NEAR (open, 1.0, 1.0e-3);
        CHECK_NEAR (shut, 0.0, 1.0e-3);
        CHECK (open > shut);
    }
}

/*  (b) Size is the duty cycle: the fraction of each cycle the gate is open. */
LUTHIER_TEST (Gater, sizeSetsTheOpenWindowWidth)
{
    for (double sizePercent : { 10.0, 25.0, 75.0, 90.0 })
    {
        GaterPedal pedal;
        setUp (pedal, 4.0, sizePercent, 0.0);

        const auto out = runDc (pedal, (int) kSr);
        const double open = fractionAbove (out, 0.5);

        CHECK_MSG (std::abs (open - sizePercent * 0.01) < 0.005,
                   "Size " + juce::String (sizePercent) + " % opened " + juce::String (open * 100.0, 2) + " % of the cycle");
    }

    // The width is measured at half height, so Shape leaves it alone.
    GaterPedal smooth;
    setUp (smooth, 4.0, 30.0, 1.0);
    const auto out = runDc (smooth, (int) kSr);
    CHECK_NEAR (fractionAbove (out, 0.5), 0.30, 0.005);
}

/*  (c) Shape: 0 is a hard square gate (a full-height step between two samples),
    1 a raised cosine (no step larger than the curve's slope allows). */
LUTHIER_TEST (Gater, shapeGoesFromHardEdgesToRaisedCosine)
{
    GaterPedal hard;
    setUp (hard, 4.0, 50.0, 0.0);
    const auto square = runDc (hard, (int) kSr);
    CHECK_NEAR (maxStep (square), 1.0, 1.0e-9);

    GaterPedal soft;
    setUp (soft, 4.0, 50.0, 1.0);
    const auto cosine = runDc (soft, (int) kSr);

    // A full raised-cosine cycle over 12000 samples: the steepest step is pi / 12000.
    CHECK_MSG (maxStep (cosine) < 0.0005, "Shape 1 still steps by " + juce::String (maxStep (cosine), 6));
    CHECK (maxStep (cosine) < maxStep (square) * 0.001);

    // At Size 50 % and Shape 1 the window is one raised cosine: full at its
    // centre, half height at its edges, shut opposite.
    CHECK_NEAR (soft.gainAtPhase (0.25), 1.0, 1.0e-9);
    CHECK_NEAR (soft.gainAtPhase (0.0),  0.5, 1.0e-9);
    CHECK_NEAR (soft.gainAtPhase (0.5),  0.5, 1.0e-9);
    CHECK_NEAR (soft.gainAtPhase (0.75), 0.0, 1.0e-9);

    // Half way: gradual, but with a flat top and a flat floor.
    GaterPedal mid;
    setUp (mid, 4.0, 50.0, 0.5);
    const auto half = runDc (mid, (int) kSr);
    CHECK (maxStep (half) < 0.001 && maxStep (half) > maxStep (cosine));
    CHECK_NEAR (mid.gainAtPhase (0.25), 1.0, 1.0e-9);
    CHECK_NEAR (mid.gainAtPhase (0.20), 1.0, 1.0e-9);
    CHECK_NEAR (mid.gainAtPhase (0.75), 0.0, 1.0e-9);
}

/*  (d) gateOpenness holds the gain the last block ended on. */
LUTHIER_TEST (Gater, theAtomicTracksTheGate)
{
    GaterPedal pedal;
    setUp (pedal, 4.0, 50.0, 0.0);

    // A 12000-sample cycle: 3000 samples in is the middle of the open window.
    runDc (pedal, 3000);
    CHECK_NEAR (pedal.gateOpenness.load(), 1.0f, 1.0e-6f);

    // 6000 more: the middle of the shut half.
    runDc (pedal, 6000);
    CHECK_NEAR (pedal.gateOpenness.load(), 0.0f, 1.0e-6f);

    // With soft edges it reports the slope, not just the ends.
    GaterPedal soft;
    setUp (soft, 4.0, 50.0, 1.0);
    runDc (soft, 1500);   // phase 0.125: on the rising cosine
    const float rising = soft.gateOpenness.load();
    CHECK (rising > 0.6f && rising < 1.0f);
    CHECK_NEAR (rising, (float) soft.gainAtPhase (1499.0 / 12000.0), 1.0e-3f);   // the last sample the block saw
}

/*  Rate is a note division of the host tempo; Free hands the clock to Frequency. */
LUTHIER_TEST (Gater, rateSyncsToTheTempoAndFreeUsesFrequency)
{
    GaterPedal pedal;
    pedal.prepare (kSr, kBlock);
    pedal.setTempoBpm (120.0);

    pedal.setParameterValue (kRate, 3.0);        // 1/4
    CHECK_NEAR (pedal.getEffectiveRateHz(), 2.0, 1.0e-9);

    pedal.setParameterValue (kRate, 5.0);        // 1/8
    CHECK_NEAR (pedal.getEffectiveRateHz(), 4.0, 1.0e-9);

    pedal.setParameterValue (kRate, 8.0);        // 1/16
    CHECK_NEAR (pedal.getEffectiveRateHz(), 8.0, 1.0e-9);

    // A tempo change reaches the clock on the next block.
    pedal.setTempoBpm (90.0);
    runDc (pedal, kBlock);
    CHECK_NEAR (pedal.getEffectiveRateHz(), 6.0, 1.0e-9);

    // Free: Frequency alone, whatever the tempo.
    pedal.setParameterValue (kRate, kFree);
    pedal.setParameterValue (kFrequency, 7.5);
    CHECK_NEAR (pedal.getEffectiveRateHz(), 7.5, 1.0e-9);
    pedal.setTempoBpm (200.0);
    runDc (pedal, kBlock);
    CHECK_NEAR (pedal.getEffectiveRateHz(), 7.5, 1.0e-9);
}

/*  The rack's LED reads the atomic and glows with it. */
LUTHIER_TEST (Gater, theRackLedFollowsTheGate)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& chain = processor.getEngine().getPostEffects();
    chain.setSlotType (0, PedalType::Gater);
    setPlain (processor, ParamIDs::slotType (true, 0), (float) (int) PedalType::Gater);
    setPlain (processor, ParamIDs::slotBypass (true, 0), 0.0f);

    PedalRack rack (processor, true);
    rack.setSize (PedalSlotComponent::nominalWidth, 100);
    rack.refresh();
    rack.setSize (PedalSlotComponent::nominalWidth, rack.getPreferredHeight());
    rack.resized();

    auto* slot = rack.getSlot (0);
    CHECK (slot != nullptr && slot->getShownType() == PedalType::Gater);

    auto* pedal = dynamic_cast<GaterPedal*> (chain.getPedal (0));
    CHECK (pedal != nullptr);

    if (slot == nullptr || pedal == nullptr)
        return;

    auto& led = slot->getGateLed();
    CHECK_MSG (led.isVisible(), "the Gater's LED is not shown");
    CHECK_MSG (led.getBounds().toFloat().contains (slot->getFaceLayout().led.getCentre()),
               "the LED is not on the face's LED spot");

    // The audio thread's write, as the pedal does it at the end of a block.
    pedal->gateOpenness.store (1.0f);
    led.refresh();
    CHECK_NEAR (led.getShownOpenness(), 1.0f, 1.0e-6f);
    const auto open = snapshot (led);

    pedal->gateOpenness.store (0.0f);
    led.refresh();
    CHECK_NEAR (led.getShownOpenness(), 0.0f, 1.0e-6f);
    const auto shut = snapshot (led);

    pedal->gateOpenness.store (0.5f);
    led.refresh();
    CHECK_NEAR (led.getShownOpenness(), 0.5f, 1.0e-6f);

    const auto centre = led.getLocalBounds().getCentre();
    CHECK_MSG (open.getPixelAt (centre.x, centre.y) != shut.getPixelAt (centre.x, centre.y),
               "the LED looks the same open and shut");

    // Another pedal in the slot: the LED goes away.
    chain.setSlotType (0, PedalType::Tremolo);
    setPlain (processor, ParamIDs::slotType (true, 0), (float) (int) PedalType::Tremolo);
    slot->refresh();
    CHECK (! slot->getGateLed().isVisible());
}

/*  The pedal keeps the block clean and is silent, not NaN, when shut. */
LUTHIER_TEST (Gater, outputIsFiniteAndBypassIsTransparent)
{
    GaterPedal pedal;
    setUp (pedal, 20.0, 1.0, 0.0);

    juce::Random random (0x9a7e);
    std::vector<double> l ((size_t) kSr), r ((size_t) kSr);

    for (size_t i = 0; i < l.size(); ++i)
        l[i] = r[i] = random.nextDouble() * 2.0 - 1.0;

    const auto dry = l;

    for (int at = 0; at < (int) kSr; at += kBlock)
        pedal.processWithBypass (l.data() + at, r.data() + at, juce::jmin (kBlock, (int) kSr - at));

    bool finite = true;

    for (size_t i = 0; i < l.size(); ++i)
        finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]) && std::abs (l[i]) <= std::abs (dry[i]) + 1.0e-12;

    CHECK (finite);
    CHECK (fractionAbove (l, 1.0e-9) < 0.5);   // a 1 % window is mostly silence

    // Bypassed, it passes the signal once the 10 ms crossfade is over.
    pedal.setBypassed (true);
    l = dry; r = dry;

    for (int at = 0; at < (int) kSr; at += kBlock)
        pedal.processWithBypass (l.data() + at, r.data() + at, juce::jmin (kBlock, (int) kSr - at));

    double worst = 0.0;

    for (size_t i = 2400; i < l.size(); ++i)
        worst = juce::jmax (worst, std::abs (l[i] - dry[i]));

    CHECK_NEAR (worst, 0.0, 1.0e-9);
}
