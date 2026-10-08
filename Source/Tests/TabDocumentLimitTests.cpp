#include "TestFramework.h"
#include "../Notation/TabDocumentNormalizer.h"

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (TabDocumentLimits, rejectsStaffRowsWiderThanConfiguredColumnLimit)
{
    TabDocumentNormalizer normalizer;
    TabDocumentNormalizer::Options options;
    options.maxColumnsPerSystem = 12;

    const juce::String source =
        "e|----------------0---|\n"
        "B|----------------0---|\n"
        "G|----------------1---|\n"
        "D|----------------2---|\n"
        "A|----------------2---|\n"
        "E|----------------0---|\n";

    NormalizedTabDocument document;
    CHECK (! normalizer.normalize (source, document, options));
    CHECK (document.diagnostics.warnings.joinIntoString (" ").containsIgnoreCase ("column"));
}

LUTHIER_TEST (TabDocumentLimits, rejectsMoreRecoveredSystemsThanConfiguredLimit)
{
    TabDocumentNormalizer normalizer;
    TabDocumentNormalizer::Options options;
    options.maxRecoveredSystems = 1;

    const juce::String source =
        "e|--0--|\n"
        "B|--0--|\n"
        "G|--1--|\n"
        "D|--2--|\n"
        "A|--2--|\n"
        "E|--0--|\n"
        "\n"
        "e|--3--|\n"
        "B|--3--|\n"
        "G|--4--|\n"
        "D|--5--|\n"
        "A|--5--|\n"
        "E|--3--|\n";

    NormalizedTabDocument document;
    CHECK (! normalizer.normalize (source, document, options));
    CHECK (document.diagnostics.warnings.joinIntoString (" ").containsIgnoreCase ("system"));
}
