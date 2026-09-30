#pragma once

/*  cpu-quality-modes.md 9: every CPU-quality string, in the catalogue.

    Kept in their own file so the feature's strings do not churn the main
    catalogue; Localisation::getBuiltInEnglish() merges these in.
*/

#include <juce_core/juce_core.h>
#include <map>

namespace luthier::QualityStrings
{
    /** Adds the CPU-quality English strings to `catalog` and returns it. */
    std::map<juce::String, juce::String> mergeInto (std::map<juce::String, juce::String> catalog);
}
