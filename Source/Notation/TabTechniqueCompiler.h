#pragma once

#include "PerformanceScore.h"
#include "TabDialectLexer.h"

namespace luthier
{

/** Converts lexer-level canonical technique names into score techniques.
    Keeping this mapping explicit prevents source glyph policy from leaking into
    the performance-score model. */
class TabTechniqueCompiler
{
public:
    static bool compile (const TabToken& token, ScoreTechnique& out) noexcept;
};

} // namespace luthier
