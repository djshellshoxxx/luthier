#include "TestFramework.h"
#include "../Notation/AsciiTabReader.h"
#include "../Notation/TabDocumentNormalizer.h"
#include "../Notation/TabSemanticAdapter.h"

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

    const juce::String lowOpenStaff =
        "e|------|\n"
        "B|------|\n"
        "G|------|\n"
        "D|------|\n"
        "A|------|\n"
        "E|--0---|\n";
}

LUTHIER_TEST (TabSemanticAdapter, footerMetadataIsPromotedBeforeSemanticParsing)
{
    NormalizedTabDocument document;
    TabDocumentNormalizer normalizer;
    CHECK (normalizer.normalize (lowOpenStaff + "Tuning: Drop D\nCapo: 2\n", document));

    const auto prepared = TabSemanticAdapter::buildLegacyReaderText (document);
    CHECK (prepared.startsWithIgnoreCase ("Tuning: Drop D"));
    CHECK (prepared.containsIgnoreCase ("Capo: 2"));

    AsciiTabReader reader;
    PerformanceScore score;
    TabImportDiagnostics semantic;
    CHECK_MSG (reader.read (prepared, score, &semantic), reader.getLastError());
    CHECK (score.getTrack (0).tuning[5] == 38);
    CHECK (score.getTrack (0).capoFret == 2);

    const auto* note = firstNoteOnString (score, 5);
    CHECK (note != nullptr);
    if (note != nullptr)
        CHECK (note->midiNote == 40);
}

LUTHIER_TEST (TabSemanticAdapter, conflictingTuningIsNotPromoted)
{
    NormalizedTabDocument document;
    TabDocumentNormalizer normalizer;
    CHECK (normalizer.normalize ("Tuning: Drop D\n" + lowOpenStaff + "Tuning: DADGAD\n", document));
    CHECK (document.metadata.tuningAmbiguous);

    const auto prepared = TabSemanticAdapter::buildLegacyReaderText (document);
    CHECK (! prepared.startsWithIgnoreCase ("Tuning:"));
    CHECK (! prepared.containsIgnoreCase ("Tuning: Drop D"));
    CHECK (! prepared.containsIgnoreCase ("Tuning: DADGAD"));

    AsciiTabReader reader;
    PerformanceScore score;
    TabImportDiagnostics semantic;
    CHECK_MSG (reader.read (prepared, score, &semantic), reader.getLastError());
    CHECK (score.getTrack (0).tuning[5] == 40);
}

LUTHIER_TEST (TabSemanticAdapter, recoveryDiagnosticsSurviveSemanticMerge)
{
    TabImportDiagnostics recovery;
    recovery.collapsedRowsSplit = 5;
    recovery.entitiesDecoded = 2;
    recovery.metadataConflicts = 1;
    recovery.warnings.add ("recovery warning");

    TabImportDiagnostics semantic;
    semantic.notes = 17;
    semantic.measures = 3;
    semantic.numStrings = 6;
    semantic.warnings.add ("semantic warning");

    const auto merged = TabSemanticAdapter::mergeDiagnostics (recovery, semantic);
    CHECK (merged.collapsedRowsSplit == 5);
    CHECK (merged.entitiesDecoded == 2);
    CHECK (merged.metadataConflicts == 1);
    CHECK (merged.notes == 17);
    CHECK (merged.measures == 3);
    CHECK (merged.warnings.contains ("recovery warning"));
    CHECK (merged.warnings.contains ("semantic warning"));
}
