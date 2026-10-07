#include "TestFramework.h"
#include "Fixtures/AsciiDialectCorpus/Corpus.h"
#include "../Notation/TabDocumentNormalizer.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
int countBlocks (const NormalizedTabDocument& document, TabBlockKind kind)
{
    int count = 0;
    for (const auto& block : document.blocks)
        if (block.kind == kind)
            ++count;
    return count;
}
}

LUTHIER_TEST (AsciiDialectCorpus, coversAllTwentyFourArchitectureClasses)
{
    CHECK (ascii_corpus::fixtures.size() == 24);

    TabDocumentNormalizer normalizer;

    for (const auto& fixture : ascii_corpus::fixtures)
    {
        NormalizedTabDocument document;
        CHECK_MSG (normalizer.normalize (fixture.text, document), fixture.name);

        const int staffBlocks = countBlocks (document, TabBlockKind::staff);
        if (fixture.expectStaff)
            CHECK_MSG (staffBlocks > 0, fixture.name);
        else
            CHECK_MSG (staffBlocks == 0, fixture.name);

        if (fixture.expectedTuning[0] != '\0')
            CHECK_MSG (document.metadata.tuningName == fixture.expectedTuning, fixture.name);

        CHECK_MSG (document.metadata.tuningAmbiguous == fixture.expectAmbiguousTuning, fixture.name);
        CHECK_MSG (document.diagnostics.legendEntries >= fixture.minLegendEntries, fixture.name);
        CHECK_MSG (document.diagnostics.chordLyricBlocks >= fixture.minChordLyricBlocks, fixture.name);
        CHECK_MSG (document.diagnostics.multiPartBlocks >= fixture.minMultiPartBlocks, fixture.name);
        CHECK_MSG ((int) document.metadata.directives.size() >= fixture.minDirectives, fixture.name);
    }
}

LUTHIER_TEST (AsciiDialectCorpus, ambiguousProseDoesNotInventAStaff)
{
    const auto& fixture = ascii_corpus::fixtures.back();
    NormalizedTabDocument document;
    TabDocumentNormalizer normalizer;

    CHECK (normalizer.normalize (fixture.text, document));
    CHECK (countBlocks (document, TabBlockKind::staff) == 0);
    CHECK (document.diagnostics.staffLines == 0);
}
