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
