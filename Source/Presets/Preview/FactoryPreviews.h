#pragma once

/*  preset-browser-previews.md 2 ("Build time") and 5.3: renders every factory
    preset's preview and writes what ships beside the bank:

        <outDir>/Previews/<file>.ogg          one per preset
        <outDir>/Previews/previews.json       the manifest (5.3)
        <outDir>/descriptor-calibration.json  the factory percentiles (6.3)

    Shared by `luthier-render --render-previews` and the tests, so the files CI
    ships are the ones the tests check.
*/

#include "PreviewRenderer.h"
#include "../Search/ToneDescriptors.h"

namespace luthier
{

class FactoryPreviews
{
public:
    struct Rendered
    {
        juce::String name, uid, fileName;
        juce::var json;
        PreviewResult result;
    };

    /** Renders the whole bank through `renderer`. `progress` gets (done, total, name). */
    static std::vector<Rendered> renderBank (PreviewRenderer& renderer,
                                             std::function<void (int, int, const juce::String&)> progress = {});

    /** The calibration the bank implies. */
    static DescriptorCalibration calibrationFor (const std::vector<Rendered>&);

    /** Writes the Previews folder, the manifest and the calibration file.
        Returns false if anything could not be written. */
    static bool write (const std::vector<Rendered>&, const juce::File& outDir, juce::String& error);

    /** The Ogg file name for a uid ("factory:Init" is not a legal file name). */
    static juce::String fileNameFor (const juce::String& uid);
};

} // namespace luthier
