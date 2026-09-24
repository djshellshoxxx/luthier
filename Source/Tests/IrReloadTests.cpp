/*  TODO 7's known issue: "the first note after a body/cab IR load renders
    slightly differently (~0.02 peak at the onset)". The response now finishes
    installing - and juce::dsp::Convolution's crossfade from the old response is
    pumped through and cleared - before a load returns (ConvolutionInstaller),
    so the first note after a load is bit-identical to every later one. These
    hold that, through the engine directly and through the plugin. */

#include "TestFramework.h"
#include "../PluginProcessor.h"
#include "../Model/Workshop/PartAcoustics.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    template <typename Process>
    std::vector<float> renderNote (Process&& process, int note)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> out;

        for (int b = 0; b < 12; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

            buffer.clear();
            process (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    out.push_back (buffer.getSample (ch, i));
        }

        return out;
    }

    double largestDifference (const std::vector<float>& a, const std::vector<float>& b)
    {
        double d = 0.0;
        for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i)
            d = juce::jmax (d, (double) std::abs (a[i] - b[i]));
        return d;
    }
}

LUTHIER_TEST (IrReload, theFirstNoteAfterALoadIsEveryNote)
{
    PartLibrary library;
    LuthierEngine engine;
    engine.prepare (48000.0, 512);

    int loads = 0;

    // Every factory guitar in turn: each change of guitar reloads the body and
    // cabinet responses that differ.
    for (const auto& file : library.getGuitarFiles())
    {
        WorkshopGuitar g;
        PartLibrary::LoadReport report;

        if (! library.loadGuitar (file, g, report))
            continue;

        const int before = engine.getIrLoadCount();
        engine.applyWorkshopGuitar (mapSpec (g), engine.getGuitarType());
        loads += engine.getIrLoadCount() - before;

        auto process = [&engine] (juce::AudioBuffer<float>& b, juce::MidiBuffer& m) { engine.processBlock (b, m); };
        const int note = g.family == "bass" ? 40 : 52;

        engine.reset();
        const auto first = renderNote (process, note);
        engine.reset();
        const auto second = renderNote (process, note);

        CHECK_MSG (largestDifference (first, second) == 0.0,
                   file.getFileName() + ": the first note after the load differs by "
                       + juce::String (largestDifference (first, second), 6));
    }

    CHECK_MSG (loads > 3, "only " + juce::String (loads) + " responses loaded; the test did not exercise the reload");
}

LUTHIER_TEST (IrReload, throughThePluginAGuitarChangeLeavesNoOnsetDifference)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto setType = [&processor] (GuitarType type)
    {
        auto* p = processor.getState().getParameter (ParamIDs::guitarType);
        p->setValueNotifyingHost (p->convertTo0to1 ((float) type));
        processor.getParameterBridge().applyAllNow();
    };

    auto process = [&processor] (juce::AudioBuffer<float>& b, juce::MidiBuffer& m) { processor.processBlock (b, m); };

    for (auto type : { GuitarType::Dreadnought, GuitarType::LesPaul, GuitarType::Dreadnought })
    {
        setType (type);

        /*  One silent block first: the plugin applies the guitar's parameters
            at the top of its next block, and those glide over 20 ms as every
            DSP parameter does (engine.md 0.4) - a note inside that glide is
            meant to differ. What must not differ is the response: after that
            block the first note equals the second exactly. */
        {
            juce::AudioBuffer<float> silence (2, 512);
            juce::MidiBuffer none;
            silence.clear();
            processor.processBlock (silence, none);
        }

        processor.getEngine().reset();
        const auto first = renderNote (process, 52);
        processor.getEngine().reset();
        const auto second = renderNote (process, 52);

        CHECK_MSG (largestDifference (first, second) == 0.0,
                   "type " + juce::String ((int) type) + ": the first note differs by "
                       + juce::String (largestDifference (first, second), 6));
    }
}
