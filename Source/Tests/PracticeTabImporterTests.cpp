#include "TestFramework.h"
#include "../UI/PracticeTabImporter.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    const ScoreNote* firstNoteOnString (const PerformanceScore& score, int stringIndex)
    {
        for (const auto& measure : score.getTrack (0).measures)
            for (const auto& voice : measure.voices)
                for (const auto& note : voice.notes)
                    if (note.stringIndex == stringIndex)
                        return &note;
        return nullptr;
    }
}

LUTHIER_TEST (PracticeTabImporter, asciiFileUsesWholeDocumentRecovery)
{
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getNonexistentChildFile ("luthier-practice-resilient", ".tab", false);
    file.replaceWithText (
        "e|------| B|------| G|------| D|------| A|------| E|--0---|\n"
        "Tuning: Drop D\n");

    PracticeTabImporter importer;
    PerformanceScore score;
    CHECK_MSG (importer.read (file, score), importer.getLastError());
    CHECK (importer.getLastDiagnostics().collapsedRowsSplit >= 5);
    CHECK (score.getTrack (0).tuning[5] == 38);

    const auto* note = firstNoteOnString (score, 5);
    CHECK (note != nullptr);
    if (note != nullptr)
        CHECK (note->midiNote == 38);

    file.deleteFile();
}

LUTHIER_TEST (PracticeTabImporter, missingAsciiFileReportsPipelineError)
{
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getNonexistentChildFile ("luthier-missing-tab", ".tab", false);

    PracticeTabImporter importer;
    PerformanceScore score;
    CHECK (! importer.read (file, score));
    CHECK (importer.getLastError().containsIgnoreCase ("No such file"));
}
