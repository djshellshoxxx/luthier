/*  qa-polish.md 3.14: a corrupt guitar file is refused or loads; never a crash. */

#include "TestFramework.h"

#include "../Model/Workshop/PartLibrary.h"

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (Workshop, everyByteOfAFactoryGuitarFlippedIsRefusedOrLoads)
{
    const auto scratch = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getNonexistentChildFile ("luthier-guitar-flip", {});
    scratch.createDirectory();

    PartLibrary library;
    library.refreshFrom (PartLibrary::getFactoryPartsFolder(), scratch.getChildFile ("user"));

    const auto source = PartLibrary::getFactoryGuitarsFolder().getChildFile ("Electric/Vintage Single-Cut.luthierguitar");
    CHECK_MSG (source.existsAsFile(), "no factory guitar to corrupt");

    juce::MemoryBlock original;
    source.loadFileAsData (original);

    const auto damagedFile = scratch.getChildFile ("damaged.luthierguitar");
    int refused = 0, loaded = 0, nonFinite = 0;

    for (size_t i = 0; i < original.getSize(); ++i)
    {
        juce::MemoryBlock damaged (original);
        static_cast<juce::uint8*> (damaged.getData())[i] ^= 0xFF;
        damagedFile.replaceWithData (damaged.getData(), damaged.getSize());

        WorkshopGuitar guitar;
        PartLibrary::LoadReport report;

        if (! library.loadGuitar (damagedFile, guitar, report))
        {
            ++refused;
            continue;
        }

        ++loaded;

        for (const auto& placement : guitar.placements)
            if (! (std::isfinite (placement.positionMm) && std::isfinite (placement.heightTrebleMm)
                     && std::isfinite (placement.heightBassMm)))
                ++nonFinite;
    }

    scratch.deleteRecursively();

    CHECK ((size_t) (refused + loaded) == original.getSize());
    CHECK_MSG (nonFinite == 0, juce::String (nonFinite) + " loaded guitars had a non-finite placement");
    CHECK_MSG (refused > 0, "no flip was refused");
}

//==============================================================================
/*  qa-polish.md 2.7: every factory guitar, serialised and reloaded, sounds the
    same - a null of the two renders at or below -80 dBFS RMS. */
#include "../PluginProcessor.h"

LUTHIER_TEST (Workshop, everyFactoryGuitarRoundTripsInAudio)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    processor->prepareToPlay (48000.0, 256);

    auto& library = processor->getPartLibrary();
    const auto type = processor->getEngine().getGuitarType();

    auto render = [&processor]
    {
        processor->getEngine().reset();
        juce::AudioBuffer<float> buffer (processor->getTotalNumOutputChannels(), 256);
        std::vector<float> out;

        for (int block = 0; block < 90; ++block)
        {
            juce::MidiBuffer midi;

            if (block == 0)
                for (int note : { 40, 47, 52, 55 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);

            buffer.clear();
            processor->processBlock (buffer, midi);
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 256);
        }

        return out;
    };

    int guitars = 0;

    for (const auto& entry : juce::RangedDirectoryIterator (PartLibrary::getFactoryGuitarsFolder(), true, "*.luthierguitar"))
    {
        WorkshopGuitar original;
        PartLibrary::LoadReport report;

        if (! library.loadGuitar (entry.getFile(), original, report))
        {
            CHECK_MSG (false, entry.getFile().getFileName() + " did not load");
            continue;
        }

        processor->applyGuitar (original, type, report);
        const auto before = render();

        WorkshopGuitar reloaded;
        PartLibrary::LoadReport report2;
        CHECK (library.buildGuitar (juce::JSON::parse (juce::JSON::toString (original.toVar())), reloaded, report2));

        processor->applyGuitar (reloaded, type, report2);
        const auto after = render();

        double diff = 0.0, level = 0.0;

        for (size_t i = 0; i < before.size(); ++i)
        {
            diff += std::pow ((double) before[i] - after[i], 2.0);
            level += (double) before[i] * before[i];
        }

        const double nullDb = 10.0 * std::log10 (diff / (double) before.size() + 1.0e-30);
        CHECK_MSG (level > 1.0e-6, entry.getFile().getFileName() + " rendered silence");
        CHECK_MSG (nullDb <= -80.0, entry.getFile().getFileName() + " nulls only to " + juce::String (nullDb, 1) + " dBFS");
        ++guitars;
    }

    CHECK (guitars >= 15);
}
