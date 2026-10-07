/*  fx_saturation (FEAT-SAT): one knob, one smoothed soft-clip stage at the end
    of the pre-amp effects. Off is an exact bypass; on adds harmonics at about
    the same loudness; the parameter saves and loads; the knob is on screen. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../Parameters.h"
#include "../DSP/Effects/Saturation.h"
#include "../UI/EasyPanel.h"

#include <cstring>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr double kTwoPi = 2.0 * juce::MathConstants<double>::pi;
    constexpr int kN = 48000;

    std::vector<double> sine (double amplitude)
    {
        std::vector<double> x ((size_t) kN);
        for (int i = 0; i < kN; ++i)
            x[(size_t) i] = amplitude * std::sin (kTwoPi * 440.0 * i / kSr);
        return x;
    }

    void run (Saturation& s, std::vector<double>& l, std::vector<double>& r)
    {
        for (int o = 0; o < kN; o += 512)
            s.process (l.data() + o, r.data() + o, juce::jmin (512, kN - o));
    }

    /** Magnitude of the bin at `freq` over the last half second (whole cycles). */
    double bin (const std::vector<double>& x, double freq)
    {
        double re = 0.0, im = 0.0;
        const int start = kN / 2, len = 24000;   // 220 cycles of 440 Hz
        for (int i = 0; i < len; ++i)
        {
            const double ph = kTwoPi * freq * (start + i) / kSr;
            re += x[(size_t) (start + i)] * std::cos (ph);
            im += x[(size_t) (start + i)] * std::sin (ph);
        }
        return std::sqrt (re * re + im * im) / len * 2.0;
    }

    double thd (const std::vector<double>& x)
    {
        double h = 0.0;
        for (int k = 2; k <= 9; ++k)
        {
            const double b = bin (x, 440.0 * k);
            h += b * b;
        }
        return std::sqrt (h) / bin (x, 440.0);
    }

    double rms (const std::vector<double>& x)
    {
        double s = 0.0;
        for (int i = kN / 2; i < kN; ++i)
            s += x[(size_t) i] * x[(size_t) i];
        return std::sqrt (s / (kN / 2));
    }

    template <typename T>
    void collect (juce::Component& root, juce::Array<T*>& found)
    {
        for (auto* child : root.getChildren())
        {
            if (auto* match = dynamic_cast<T*> (child))
                found.add (match);
            collect<T> (*child, found);
        }
    }
}

LUTHIER_TEST (Saturation, zeroPercentIsABitIdenticalBypass)
{
    Saturation s;
    s.prepare (kSr);
    s.setAmount (0.0);

    auto l = sine (0.4), r = sine (0.2);
    const auto l0 = l, r0 = r;
    run (s, l, r);

    CHECK (! s.isActive());
    CHECK (std::memcmp (l.data(), l0.data(), l.size() * sizeof (double)) == 0);
    CHECK (std::memcmp (r.data(), r0.data(), r.size() * sizeof (double)) == 0);
}

LUTHIER_TEST (Saturation, fullAddsHarmonicsAtRoughlyTheSameLoudness)
{
    Saturation s;
    s.prepare (kSr);
    s.setAmount (1.0);
    s.reset();

    auto l = sine (0.25), r = l;
    const auto dry = l;
    run (s, l, r);

    CHECK_MSG (thd (l) > thd (dry) + 0.1, "THD " + juce::String (thd (l)) + " vs " + juce::String (thd (dry)));

    const double db = 20.0 * std::log10 (rms (l) / rms (dry));
    CHECK_MSG (std::abs (db) <= 3.0, "RMS changed by " + juce::String (db) + " dB");

    // Turned back down to zero it settles to an exact bypass again.
    s.setAmount (0.0);
    auto l2 = sine (0.25), r2 = l2;
    run (s, l2, r2);
    CHECK (! s.isActive());
}

LUTHIER_TEST (Saturation, parameterRoundTripsThroughState)
{
    LuthierAudioProcessor a;
    a.prepareToPlay (kSr, 512);

    auto* p = a.getState().getParameter (ParamIDs::fxSaturation);
    CHECK (p != nullptr);
    if (p == nullptr)
        return;

    CHECK_NEAR (a.getState().getRawParameterValue (ParamIDs::fxSaturation)->load(), 0.0f, 1.0e-6f);   // off by default
    p->setValueNotifyingHost (p->convertTo0to1 (65.0f));

    juce::MemoryBlock block;
    a.getStateInformation (block);

    LuthierAudioProcessor b;
    b.setStateInformation (block.getData(), (int) block.getSize());
    CHECK_NEAR (b.getState().getRawParameterValue (ParamIDs::fxSaturation)->load(), 65.0f, 0.01f);
}

LUTHIER_TEST (Saturation, theKnobExistsAndIsAttached)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setVisible (true);
    editor->setSize (LuthierAudioProcessorEditor::defaultWidth, LuthierAudioProcessorEditor::defaultHeight);

    juce::Array<EasyPanel*> easy;
    collect (*editor, easy);
    CHECK (easy.size() == 1);

    int inEasy = 0, inAll = 0;
    juce::Array<LuthierKnob*> knobs;
    collect (*editor, knobs);
    for (auto* k : knobs)
        if (k->getParameterId() == ParamIDs::fxSaturation)
            ++inAll;

    if (easy.size() == 1)
    {
        juce::Array<LuthierKnob*> easyKnobs;
        collect (*easy[0], easyKnobs);
        for (auto* k : easyKnobs)
            if (k->getParameterId() == ParamIDs::fxSaturation)
                ++inEasy;
    }

    CHECK_MSG (inEasy == 1, "Easy tone strip has " + juce::String (inEasy) + " saturation knobs");
    CHECK_MSG (inAll >= 2, "found " + juce::String (inAll) + " saturation knobs (Easy + Advanced)");
}
