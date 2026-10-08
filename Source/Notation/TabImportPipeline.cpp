#include "TabImportPipeline.h"
#include "TabDirectiveApplier.h"
#include "TabChordChart.h"
#include "TabKeyDetector.h"

namespace luthier
{

bool TabImportPipeline::read (const juce::String& source, PerformanceScore& destination,
                              TabImportDiagnostics* diagnostics)
{
    lastError.clear();
    lastDocument = {};

    if (source.trim().isEmpty())
    {
        destination.clear();
        lastError = "No tablature found: that text is empty.";
        if (diagnostics != nullptr)
            *diagnostics = {};
        return false;
    }

    TabDocumentNormalizer normalizer;
    if (! normalizer.normalize (source, lastDocument))
    {
        destination.clear();
        lastError = lastDocument.diagnostics.drumLinesSkipped > 0
                      ? juce::String ("That looks like drum tablature. Luthier plays guitar and bass tab; "
                                      "no guitar or bass staff was found in it.")
                      : lastDocument.diagnostics.warnings.isEmpty()
                          ? juce::String ("Could not read that as tablature.")
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

    if (ok)
    {
        TabDirectiveApplier::apply (lastDocument, destination, merged);
        TabKeyDetector::apply (destination);     // the page's Key: header, else an estimate
    }

    // Chords over lyrics and nothing else: strum the shapes instead of refusing.
    if (! ok && merged.drumLinesSkipped == 0)
    {
        const auto& meta = lastDocument.metadata;
        const std::vector<int> tuning = (! meta.tuningAmbiguous && meta.tuningMidiHighFirst.size() >= 4
                                         && meta.tuningMidiHighFirst.size() <= 8)
                                          ? meta.tuningMidiHighFirst : std::vector<int>();
        TabImportDiagnostics chart = merged;

        if (TabChordChart::read (lastDocument.normalizedText, destination, &chart, tuning,
                                 meta.capoFret > 0 ? meta.capoFret : 0, meta.tempoBpm))
        {
            merged = chart;
            TabKeyDetector::apply (destination);
            lastError.clear();
            lastDocument.diagnostics = merged;
            if (diagnostics != nullptr)
                *diagnostics = merged;
            return true;
        }
    }

    if (diagnostics != nullptr)
        *diagnostics = merged;
    lastDocument.diagnostics = merged;

    if (! ok)
    {
        lastError = merged.drumLinesSkipped > 0
                      ? juce::String ("That looks like drum tablature. Luthier plays guitar and bass tab; "
                                      "no guitar or bass staff was found in it.")
                      : reader.getLastError();
        return false;
    }

    return true;
}

bool TabImportPipeline::read (const juce::File& file, PerformanceScore& destination,
                              TabImportDiagnostics* diagnostics)
{
    lastError.clear();

    if (! file.existsAsFile())
    {
        lastDocument = {};
        destination.clear();
        lastError = "No such file: " + file.getFullPathName();
        if (diagnostics != nullptr)
            *diagnostics = {};
        return false;
    }

    const auto extension = file.getFileExtension().toLowerCase();
    static const juce::StringArray structured { ".mid", ".midi", ".gp", ".gp3", ".gp4", ".gp5", ".gpx",
                                                ".ptb", ".xml", ".musicxml", ".mxl" };
    if (structured.contains (extension))
    {
        lastDocument = {};
        destination.clear();
        lastError = "The resilient tab pipeline reads .txt and .tab files.";
        if (diagnostics != nullptr)
            *diagnostics = {};
        return false;
    }

    return read (file.loadFileAsString(), destination, diagnostics);
}

} // namespace luthier
