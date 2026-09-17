#pragma once

/*  Finds the shipped impulse responses and resolves a configuration to a file.

    Two things make this more than a path join:

    1. The Resources folder sits in a different place depending on how the plugin
       was built and installed - inside a VST3 bundle, beside a standalone binary,
       or in the user's Documents folder after a hand install. Rather than guessing
       once, the folder is searched for and the answer cached.

    2. The library is curated, not exhaustive: there is no IR for every combination
       of eighteen body shapes, sixteen woods, three sizes and three ages. So the
       lookup degrades - exact match, then the same shape at a different size or
       age, then anything for that shape - and finally reports failure so the
       caller can fall back to modal synthesis rather than dropping the body
       entirely.
*/

#include <juce_core/juce_core.h>
#include "../Model/Guitar/BodyModels.h"
#include "../DSP/Amp/CabinetEngine.h"

namespace luthier
{

class IrLibrary
{
public:
    /** The Resources folder, or an invalid File if none was found. Cached. */
    static juce::File getResourcesFolder();

    /** Forces the next call to search again. Used after a hand install. */
    static void forgetResourcesFolder();

    static juce::File getBodyIrFolder();
    static juce::File getCabIrFolder();

    /** Best available body IR for a configuration, or an invalid File. */
    static juce::File findBodyIr (const BodyConfig& config);

    /** Best available cabinet IR for a configuration, or an invalid File. */
    static juce::File findCabIr (const CabinetConfig& config);

    static int countBodyIrs();
    static int countCabIrs();

    static bool isAvailable();

    /** A one-line description for the diagnostics panel. */
    static juce::String describe();

private:
    static juce::File searchForResources();
};

} // namespace luthier
