#include "TestFramework.h"
#include "../Notation/TabDialectLexer.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    NormalizedTabDocument oneRow (const juce::String& row)
    {
        NormalizedTabDocument d;
        NormalizedTabBlock b;
        b.kind = TabBlockKind::staff;
        b.confidence = TabConfidence::high;
        b.lines.add (row);
        d.blocks.push_back (b);
        return d;
    }

    int countKind (const TabTokenDocument& d, TabTokenKind kind)
    {
        int n = 0;
        for (const auto& system : d.systems)
            for (const auto& row : system.rows)
                for (const auto& token : row)
                    if (token.kind == kind)
                        ++n;
        return n;
    }

    bool hasMeaning (const TabTokenDocument& d, const juce::String& meaning)
    {
        for (const auto& system : d.systems)
            for (const auto& row : system.rows)
                for (const auto& token : row)
                    if (token.canonicalMeaning == meaning)
                        return true;
        return false;
    }
}

LUTHIER_TEST (TabDialectLexer, multiDigitFretsAndColumnsArePreserved)
{
    TabDialectLexer lexer;
    TabTokenDocument out;
    CHECK (lexer.lex (oneRow ("E|--7---12---24--|"), out));
    CHECK (countKind (out, TabTokenKind::fret) == 3);

    const auto& row = out.systems.front().rows.front();
    CHECK (row[0].integerValue == 7);
    CHECK (row[1].integerValue == 12);
    CHECK (row[2].integerValue == 24);
    CHECK (row[0].columnStart < row[1].columnStart);
    CHECK (row[1].columnStart < row[2].columnStart);
}

LUTHIER_TEST (TabDialectLexer, fractionalPositionsNeverBecomeOrdinaryFrets)
{
    TabDialectLexer lexer;
    TabTokenDocument out;
    CHECK (lexer.lex (oneRow ("E|--2.6---7--|"), out));
    CHECK (countKind (out, TabTokenKind::fractionalPosition) == 1);
    CHECK (countKind (out, TabTokenKind::fret) == 1);

    bool found = false;
    for (const auto& token : out.systems.front().rows.front())
        if (token.kind == TabTokenKind::fractionalPosition)
        {
            found = true;
            CHECK_NEAR (token.numericValue, 2.6, 1.0e-9);
            CHECK (token.rawText == "2.6");
        }
    CHECK (found);
}

LUTHIER_TEST (TabDialectLexer, commonLegatoAndSlidesProduceTechniqueTokens)
{
    TabDialectLexer lexer;
    TabTokenDocument out;
    CHECK (lexer.lex (oneRow ("G|--7h9p7--5/7--7\\5--|"), out));
    CHECK (hasMeaning (out, "hammerOn"));
    CHECK (hasMeaning (out, "pullOff"));
    CHECK (hasMeaning (out, "slideUp"));
    CHECK (hasMeaning (out, "slideDown"));
}

LUTHIER_TEST (TabDialectLexer, documentLegendOverridesAmbiguousHAndB)
{
    auto d = oneRow ("E|--7H--9B--|"
    );

    TabNotationDefinition harmonic;
    harmonic.pattern = "<fret>H";
    harmonic.canonicalMeaning = "naturalHarmonic";
    harmonic.confidence = TabConfidence::exact;
    d.notation.push_back (harmonic);

    TabNotationDefinition bend;
    bend.pattern = "<fret>B";
    bend.canonicalMeaning = "bend";
    bend.confidence = TabConfidence::exact;
    d.notation.push_back (bend);

    TabDialectLexer lexer;
    TabTokenDocument out;
    CHECK (lexer.lex (d, out));
    CHECK (hasMeaning (out, "naturalHarmonic"));
    CHECK (hasMeaning (out, "bend"));
}

LUTHIER_TEST (TabDialectLexer, caretDirectionDefaultsToHammerOrPull)
{
    TabDialectLexer lexer;
    TabTokenDocument out;
    CHECK (lexer.lex (oneRow ("G|--7^9---9^7--|"), out));
    CHECK (hasMeaning (out, "hammerOn"));
    CHECK (hasMeaning (out, "pullOff"));
}
