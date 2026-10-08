#pragma once

#include "../Notation/NotationExport.h"
#include "../Notation/TabImportPipeline.h"
#include "../Notation/TabKeyDetector.h"

namespace luthier
{

/** File importer used by the Practice TAB surface.

    ASCII tab goes through the whole-document recovery pipeline. Formats that
    already have structured parsers keep using NotationImporter unchanged. */
class PracticeTabImporter
{
public:
    bool read (const juce::File& file, PerformanceScore& destination)
    {
        lastError.clear();
        lastDiagnostics = {};
        resolvedAsciiTuning = false;

        if (file.existsAsFile() && NotationImporter::detectKind (file) == NotationImporter::FileKind::asciiTab)
        {
            TabImportPipeline pipeline;
            const bool ok = pipeline.read (file, destination, &lastDiagnostics);
            lastError = pipeline.getLastError();

            const auto& metadata = pipeline.getLastDocument().metadata;
            resolvedAsciiTuning = ok
                               && ! metadata.tuningAmbiguous
                               && ! metadata.tuningCandidates.empty()
                               && ! metadata.tuningMidiHighFirst.empty();
            return ok;
        }

        NotationImporter importer;
        const bool ok = importer.read (file, destination);
        lastDiagnostics = importer.getLastDiagnostics();
        lastError = importer.getLastError();
        if (ok)
            TabKeyDetector::apply (destination);
        return ok;
    }

    const TabImportDiagnostics& getLastDiagnostics() const noexcept { return lastDiagnostics; }
    juce::String getLastError() const { return lastError; }
    bool lastAsciiHadResolvedTuning() const noexcept { return resolvedAsciiTuning; }

private:
    TabImportDiagnostics lastDiagnostics;
    juce::String lastError;
    bool resolvedAsciiTuning = false;
};

} // namespace luthier
