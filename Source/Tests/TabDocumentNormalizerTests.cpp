#include "TestFramework.h"
#include "../Notation/TabDocument.h"

using namespace luthier;
using namespace luthier::tests;

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
