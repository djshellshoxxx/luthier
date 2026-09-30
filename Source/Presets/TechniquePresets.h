#pragma once

/*  The technique presets (TECHNIQUES workstream): technique-cascade.md 5's
    combined presets, and the single-technique presets of muting-rhythm.md 6,
    two-hand-tapping.md 8, microtonal-bends.md 7 and
    slide-technique-controls.md 6. FactoryPresets appends them to its bank.

    Each is a list of parameter overrides in real units (choice by index), as
    the rest of the bank is written, plus an optional `techniques` block
    (engine-technique-layer.md 7): a live mute grid, for the presets whose
    groove is a grid.
*/

#include <juce_core/juce_core.h>
#include <vector>

namespace luthier
{

struct TechniquePresetRecipe
{
    juce::String name, category, description, tags;
    std::vector<std::pair<juce::String, double>> values;
    juce::String techniques;   ///< JSON of the preset's techniques block, or empty
};

const std::vector<TechniquePresetRecipe>& getTechniquePresetRecipes();

/** The parameters that arm a technique (gui-techniques-updates.md 7's chip reads these). */
juce::StringArray getTechniqueArmParameterIds();

} // namespace luthier
