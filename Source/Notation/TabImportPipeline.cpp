#include "TabImportPipeline.h"

namespace luthier
{

bool TabImportPipeline::read (const juce::String& source, PerformanceScore& destination,
                              TabImportDiagnostics* diagnostics)
{
    lastError.clear();
    lastDocument = {};

    TabDocumentNormalizer normalizer;
    if (! normalizer.normalize (source, lastDocument))
    {
        destination.clear();
        lastError = lastDocument.diagnostics.warnings.isEmpty()
                      ? juce::String ("Could not normalize that tablature.")
                      : lastDocument.diagnostics.warnings.joinIntoString ("; ");
        if (diagnostics != nullptr)
            *diagnostics = lastDocument.diagnostics;
        return false;
    }

    const auto prepared = TabSemanticAdapter::buildLegacyReaderText (lastDocument);

    AsciiTabReader reader;
    TabImportDiagnostics semantic;
    const bool ok = reader.read (prepared, destination, &semantic);

    auto merged = TabSemanticAdapter::mergeDiagnostics (lastDocument.diagnostics, semantic);
    if (diagnostics != nullptr)
        *diagnostics = merged;
    lastDocument.diagnostics = merged;

    if (! ok)
    {
        lastError = reader.getLastError();
        return false;
    }

    return true;
}

} // namespace luthier
