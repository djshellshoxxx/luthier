#include "TestFramework.h"
#include "../Notation/TabDocumentNormalizer.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    int countBlocks (const NormalizedTabDocument& d, TabBlockKind kind)
    {
        int n = 0;
        for (const auto& b : d.blocks)
            if (b.kind == kind)
                ++n;
        return n;
    }
}

LUTHIER_TEST (TabDocumentRecovery, collapsedLabelledRowsAreSplitIntoAStaff)
{
    const juce::String source =
        "E|--7--7--| A|--------| D|--------| G|--------| B|--------| e|--------|\n";

    TabDocumentNormalizer normalizer;
    NormalizedTabDocument document;
    CHECK (normalizer.normalize (source, document));
    CHECK (countBlocks (document, TabBlockKind::staff) == 1);
    CHECK (document.diagnostics.collapsedRowsSplit >= 5);

    const auto& block = document.blocks.front();
    CHECK (block.lines.size() == 6);
}

LUTHIER_TEST (TabDocumentRecovery, unlabelledContinuationInheritsPriorStringIdentity)
{
    const juce::String source =
        "e|----0----|\n"
        "B|----0----|\n"
        "G|----1----|\n"
        "D|----2----|\n"
        "A|----2----|\n"
        "E|----0----|\n"
        "\n"
        "|----3----|\n"
        "|----3----|\n"
        "|----4----|\n"
        "|----5----|\n"
        "|----5----|\n"
        "|----3----|\n";

    TabDocumentNormalizer normalizer;
    NormalizedTabDocument document;
    CHECK (normalizer.normalize (source, document));
    CHECK (countBlocks (document, TabBlockKind::staff) == 2);
    CHECK (document.diagnostics.inheritedStringLabels >= 6);

    const auto& second = document.blocks.back();
    CHECK (second.inferredStringLabels.size() == 6);
    CHECK (second.confidence == TabConfidence::medium);
}

LUTHIER_TEST (TabDocumentRecovery, embeddedChordDamageIsClassifiedWithoutSplittingOrdinaryWords)
{
    const juce::String source =
        "Standard (EADGBE)\n"
        "AIn my eyes, Cindisposed\n"
        "GIn disguise as no one knF#mows\n"
        "The Asun Gin my disBbgrace\n"
        "And sometimes far too long for snakes\n";

    TabDocumentNormalizer normalizer;
    NormalizedTabDocument document;
    CHECK (normalizer.normalize (source, document));
    CHECK (countBlocks (document, TabBlockKind::chordLyrics) >= 2);
    CHECK (document.diagnostics.chordLyricBlocks >= 2);

    bool ordinaryLineStayedNonChord = false;
    for (const auto& block : document.blocks)
        if (block.lines.joinIntoString (" ").contains ("And sometimes"))
            ordinaryLineStayedNonChord = block.kind != TabBlockKind::chordLyrics;
    CHECK (ordinaryLineStayedNonChord);
}
