#include "TestFramework.h"
#include "../Notation/TabTechniqueCompiler.h"

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (TabTechniqueCompiler, canonicalHammerAndSlideMappings)
{
    TabToken token;
    token.kind = TabTokenKind::technique;

    ScoreTechnique technique;

    token.canonicalMeaning = "hammerOn";
    CHECK (TabTechniqueCompiler::compile (token, technique));
    CHECK (technique.type == ScoreTechnique::Type::hammerOn);

    token.canonicalMeaning = "slideLegato";
    token.numericValue = 9.0;
    CHECK (TabTechniqueCompiler::compile (token, technique));
    CHECK (technique.type == ScoreTechnique::Type::slideLegato);
    CHECK_NEAR (technique.value, 9.0, 0.0001);
}

LUTHIER_TEST (TabTechniqueCompiler, documentLegendMeaningsUseSameScoreVocabulary)
{
    TabToken token;
    token.kind = TabTokenKind::technique;
    token.canonicalMeaning = "naturalHarmonic";

    ScoreTechnique technique;
    CHECK (TabTechniqueCompiler::compile (token, technique));
    CHECK (technique.type == ScoreTechnique::Type::naturalHarmonic);

    token.canonicalMeaning = "tap";
    CHECK (TabTechniqueCompiler::compile (token, technique));
    CHECK (technique.type == ScoreTechnique::Type::tap);
}

LUTHIER_TEST (TabTechniqueCompiler, unknownAndNonTechniqueTokensAreRejected)
{
    TabToken token;
    ScoreTechnique technique;

    token.kind = TabTokenKind::fret;
    token.canonicalMeaning = "hammerOn";
    CHECK (! TabTechniqueCompiler::compile (token, technique));

    token.kind = TabTokenKind::technique;
    token.canonicalMeaning = "siteSpecificUnknownThing";
    CHECK (! TabTechniqueCompiler::compile (token, technique));
}
