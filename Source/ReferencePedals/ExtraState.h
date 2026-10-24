// OWNER: WP2
#pragma once
#include "BoardState.h"
namespace luthier::reference
{
// NamedValueSet::set uses approximate double equality. Existing opaque values
// must be replaced by assignment or adjacent/tiny changes silently disappear.
inline void setExactExtraProperty (juce::DynamicObject& object, const juce::Identifier& name, const juce::var& value)
{
    auto copy = value.clone();
    if (auto* existing = object.getProperties().getVarPointer (name)) *existing = std::move (copy);
    else object.setProperty (name, copy);
}
// Message-thread identity comparison. Default JUCE JSON formatting rounds
// opaque doubles, so it must never decide whether runtime state is reusable.
inline bool sameExtraState (const juce::var& a, const juce::var& b)
{
    juce::String first, second;
    return BoardState::canonicalJson (a, first) && BoardState::canonicalJson (b, second) && first == second;
}
}
