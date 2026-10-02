#include "TestFramework.h"
#include "../Notation/TabDocument.h"
#include "../Notation/TabDocumentNormalizer.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    const juce::String simpleStaff =
        "e|--0--|\n"
        "B|--0--|\n"
        "G|--1--|\n"
        "D|--2--|\n"
        "A|--2--|\n"
        "E|--0--|\n";
}

LUTHIER_TEST (TabDocument, sharedModelDefaultsAreSafe)
{
    TabSourceSpan span;
    CHECK (span.sourceLineStart == -1);
    CHECK (span.sourceLineEnd == -1);
    CHECK (span.sourceColumnStart == -1);
    CHECK (span.sourceColumnEnd == -1);

    NormalizedTabBlock block;
    CHECK (block.kind == TabBlockKind::unknown);
    CHECK (block.confidence == TabConfidence::low);
    CHECK (block.partIndex == 0);
    CHECK (block.inferredStringLabels.isEmpty());

    TabDocumentMetadata metadata;
    CHECK (metadata.capoFret == -1);
    CHECK (metadata.numStrings == 0);
    CHECK (metadata.tuningMidiHighFirst.empty());
    CHECK (! metadata.tuningAmbiguous);

    NormalizedTabDocument document;
    CHECK (document.blocks.empty());
    CHECK (document.metadata.capoFret == -1);
}

LUTHIER_TEST (TabDocument, notationAndDirectivesRetainProvenance)
{
    TabNotationDefinition definition;
    definition.pattern = "<fret>B";
    definition.canonicalMeaning = "bend";
    definition.rawDefinition = "7B = bend";
    definition.source = { 11, 11, 0, 9 };
    definition.confidence = TabConfidence::exact;

    CHECK (definition.pattern == "<fret>B");
    CHECK (definition.source.sourceLineStart == 11);
    CHECK (definition.confidence == TabConfidence::exact);

    TabDirective directive;
    directive.canonicalName = "naturalHarmonic";
    directive.rawText = "all notes harmonics on intro";
    directive.scope = DirectiveScope::section;
    directive.sectionName = "Intro";
    directive.source = { 3, 3, 0, 28 };
    directive.confidence = TabConfidence::high;

    CHECK (directive.scope == DirectiveScope::section);
    CHECK (directive.sectionName == "Intro");
    CHECK (directive.source.sourceLineStart == 3);
}

LUTHIER_TEST (TabDocumentNormalizer, tuningAtBeginningAndEndHaveEqualAuthority)
{
    TabDocumentNormalizer normalizer;
    NormalizedTabDocument header;
    NormalizedTabDocument footer;

    CHECK (normalizer.normalize ("Tuning: Drop D\n" + simpleStaff, header));
    CHECK (normalizer.normalize (simpleStaff + "Tuning: Drop D\n", footer));

    CHECK (header.metadata.tuningMidiHighFirst == footer.metadata.tuningMidiHighFirst);
    CHECK (header.metadata.tuningMidiHighFirst.size() == 6);
    if (header.metadata.tuningMidiHighFirst.size() == 6)
        CHECK (header.metadata.tuningMidiHighFirst[5] == 38);
}

LUTHIER_TEST (TabDocumentNormalizer, repeatedIdenticalTuningIsNotAConflict)
{
    TabDocumentNormalizer normalizer;
    NormalizedTabDocument document;
    CHECK (normalizer.normalize ("Standard (EADGBE)\n" + simpleStaff + "Play in normal tuning.\n", document));
    CHECK (! document.metadata.tuningAmbiguous);
    CHECK (document.diagnostics.metadataConflicts == 0);
    CHECK (document.metadata.tuningCandidates.size() >= 2);
}

LUTHIER_TEST (TabDocumentNormalizer, incompatibleExplicitTuningsAreReported)
{
    TabDocumentNormalizer normalizer;
    NormalizedTabDocument document;
    CHECK (normalizer.normalize ("Tuning: Drop D\n" + simpleStaff + "Tuning: DADGAD\n", document));
    CHECK (document.metadata.tuningAmbiguous);
    CHECK (document.diagnostics.metadataConflicts >= 1);
    CHECK (! document.diagnostics.warnings.isEmpty());
}

LUTHIER_TEST (TabDocumentNormalizer, capoFormsAreFoundAnywhere)
{
    TabDocumentNormalizer normalizer;
    NormalizedTabDocument document;
    CHECK (normalizer.normalize (simpleStaff + "Capo on 3rd fret\n", document));
    CHECK (document.metadata.capoFret == 3);
}

LUTHIER_TEST (TabDocumentNormalizer, htmlAndMarkdownTransportDamageIsCleaned)
{
    TabDocumentNormalizer normalizer;
    NormalizedTabDocument document;
    CHECK (normalizer.normalize ("**Standard (EADGBE)**\r\n&#x20;Intro&#xA0;\r\n" + simpleStaff, document));
    CHECK (document.normalizedText.contains ("Standard (EADGBE)"));
    CHECK (! document.normalizedText.contains ("&#x20;"));
    CHECK (! document.normalizedText.contains ("&#xA0;"));
    CHECK (document.diagnostics.entitiesDecoded >= 2);
    CHECK (document.diagnostics.markdownWrappersRemoved >= 1);
}

LUTHIER_TEST (TabDocumentNormalizer, originalSourceIsNeverDestroyed)
{
    const juce::String source = "**Tuning: Drop D**\r\n" + simpleStaff;
    TabDocumentNormalizer normalizer;
    NormalizedTabDocument document;
    CHECK (normalizer.normalize (source, document));
    CHECK (document.originalText == source);
}
