/*  Regression tests for bugs found in code review (docs/review/FINDINGS.md).
    Each test names the finding it pins down. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../DSP/Amp/ToneStack.h"
#include "../DSP/Effects/PedalsDrive.h"
#include "../DSP/Effects/PedalsMod.h"
#include "../DSP/Amp/RoomEngine.h"

using namespace luthier;
using namespace luthier::tests;

//==============================================================================
/*  R-001: a host may hand processBlock more samples than it promised in
    prepareToPlay. The engine split such a block, but the processor's own
    scratch buffers (the click, the tune click, the backing track, the monitor)
    were sized from the promise and written for the whole block. */
LUTHIER_TEST (ReviewRegression, aBlockBiggerThanPreparedIsRenderedWhole)
{
    constexpr int kPrepared = 128;
    constexpr int kHostBlock = 1000;

    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, kPrepared);
    processor.setPracticePanelOpen (true);
    processor.getMetronome().setEnabled (true);
    processor.setClickToMain (true);

    juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(),
                                                 processor.getTotalNumInputChannels()), kHostBlock);
    bool heardLate = false;

    for (int block = 0; block < 8; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        // A note after the prepared size must still be played, at its own time.
        if (block == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 110), 700);

        processor.processBlock (buffer, midi);

        CHECK_FINITE (buffer.getReadPointer (0), kHostBlock);

        if (block == 0)
        {
            // Nothing from the note before its own sample: it was not moved earlier.
            heardLate = buffer.getMagnitude (0, kPrepared * 5, kHostBlock - kPrepared * 5) > 0.0f;
        }
    }

    CHECK (heardLate);
}

//==============================================================================
/*  R-006: the tone stack's b3 term had t*C1C2C3R1R2R4 where Yeh's derivation has
    t*l*C1C2C3R1R2R4. A passive network cannot have gain above unity; with the bass
    at zero the typo gave up to +28 dB of treble. */
namespace
{
    double toneStackGainAt (double bass, double mid, double treble, double hz)
    {
        constexpr double sr = 48000.0;
        luthier::ToneStack stack;
        stack.prepare (sr);
        stack.setControls (bass, mid, treble);

        double peak = 0.0;
        const int n = (int) sr;

        for (int i = 0; i < n; ++i)
        {
            const double y = stack.process (std::sin (juce::MathConstants<double>::twoPi * hz * i / sr));

            if (i > n / 2)
                peak = std::max (peak, std::abs (y));
        }

        return peak;
    }
}

LUTHIER_TEST (ReviewRegression, theToneStackIsPassiveAtEverySetting)
{
    // ToneStack::process applies a fixed 6.5x insertion-loss makeup; the
    // network itself must stay at or below unity.
    constexpr double makeup = 6.5;

    for (double bass : { 0.0, 0.5, 1.0 })
        for (double treble : { 0.0, 0.5, 1.0 })
            for (double hz : { 100.0, 1000.0, 5000.0, 10000.0 })
                CHECK_MSG (toneStackGainAt (bass, 0.5, treble, hz) / makeup <= 1.05,
                           "bass " + juce::String (bass) + " treble " + juce::String (treble)
                               + " at " + juce::String (hz) + " Hz");
}

//==============================================================================
/*  R-007: the pitch shifter advanced its read delay by (ratio - 1) per sample,
    which plays at 2 - ratio: a fifth up came out a fourth down, and +12 froze. */
namespace
{
    double shiftedFrequency (double semitones)
    {
        constexpr double sr = 48000.0, inHz = 220.0;
        luthier::PitchShifterPedal pedal;
        pedal.prepare (sr, 512);
        pedal.setParameterValue (0, semitones);
        pedal.setParameterValue (2, 1.0);   // all wet
        pedal.reset();

        const int n = (int) sr;
        std::vector<double> l ((size_t) n), r ((size_t) n);

        for (int i = 0; i < n; ++i)
            l[(size_t) i] = r[(size_t) i] = std::sin (juce::MathConstants<double>::twoPi * inHz * i / sr);

        for (int i = 0; i < n; i += 512)
            pedal.process (l.data() + i, r.data() + i, juce::jmin (512, n - i));

        // Upward zero crossings over the second half.
        int crossings = 0;

        for (int i = n / 2 + 1; i < n; ++i)
            if (l[(size_t) i - 1] < 0.0 && l[(size_t) i] >= 0.0)
                ++crossings;

        return crossings / 0.5;
    }
}

LUTHIER_TEST (ReviewRegression, thePitchShifterShiftsTheWayItSays)
{
    const double up = shiftedFrequency (7.0);
    const double down = shiftedFrequency (-7.0);

    CHECK_MSG (std::abs (up / (220.0 * std::pow (2.0, 7.0 / 12.0)) - 1.0) < 0.08, "up: " + juce::String (up));
    CHECK_MSG (std::abs (down / (220.0 * std::pow (2.0, -7.0 / 12.0)) - 1.0) < 0.08, "down: " + juce::String (down));
}

//==============================================================================
/*  R-008 / R-009: the parameter bridge re-sends every parameter every block. The
    Reverb pedal rebuilt (and zeroed) its delay lines on every Size or Character
    send, and the room engine on every decay-scale send, so with blocks shorter
    than the shortest line no late reverb was ever heard. */
LUTHIER_TEST (ReviewRegression, theReverbPedalTailSurvivesRepeatedParameterSends)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    luthier::ReverbPedal pedal;
    pedal.prepare (sr, block);
    pedal.setParameterValue (5, 1.0);   // mix

    std::vector<double> l ((size_t) block), r ((size_t) block);
    double lateEnergy = 0.0;

    for (int b = 0; b < 40; ++b)
    {
        // What ParameterBridge::applyToEngine does every block.
        for (int p = 0; p < pedal.getNumParameters(); ++p)
            pedal.setParameterNormalised (p, pedal.getParameterNormalised (p));

        std::fill (l.begin(), l.end(), 0.0);
        std::fill (r.begin(), r.end(), 0.0);

        if (b == 0)
            l[0] = r[0] = 1.0;

        pedal.process (l.data(), r.data(), block);

        if (b >= 20)   // well past the input and the pre-delay
            for (auto v : l)
                lateEnergy += v * v;
    }

    CHECK_MSG (lateEnergy > 1.0e-8, "late energy " + juce::String (lateEnergy));
}

LUTHIER_TEST (ReviewRegression, theRoomTailSurvivesRepeatedDecaySends)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    luthier::RoomEngine room;
    room.prepare (sr, block);
    room.setEnabled (true);
    room.setRoomSize (luthier::RoomSize::LiveRoom);
    room.setRoomBlend (1.0);
    room.reset();

    juce::AudioBuffer<float> buffer (2, block);
    double lateEnergy = 0.0;

    for (int b = 0; b < 220; ++b)
    {
        room.setDecayScale (1.0);   // every block, as the bridge does
        buffer.clear();

        if (b == 0)
            buffer.setSample (0, 0, 1.0f), buffer.setSample (1, 0, 1.0f);

        room.processBlock (buffer);

        if (b >= 180)   // past every early reflection (0.68 s at most)
            lateEnergy += buffer.getRMSLevel (0, 0, block);
    }

    CHECK_MSG (lateEnergy > 1.0e-7, "late energy " + juce::String (lateEnergy));
}
