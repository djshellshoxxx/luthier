#include "TestFramework.h"
#include "../Notation/AsciiTabReader.h"

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

LUTHIER_TEST (TabSemanticIntegration, footerDropDTunesTheImportedScoreBeforeNotesAreEmitted)
{
    AsciiTabReader reader;
    PerformanceScore score;
    TabImportDiagnostics diagnostics;

    CHECK_MSG (reader.read (lowOpenStaff + "Tuning: Drop D\n", score, &diagnostics),
               reader.getLastError());
    CHECK (score.getTrack (0).tuning[5] == 38);
    CHECK (score.getMeta().tuningName.containsIgnoreCase ("drop d"));

    const auto* note = firstNoteOnString (score, 5);
    CHECK (note != nullptr);
    if (note != nullptr)
        CHECK (note->midiNote == 38);
}

LUTHIER_TEST (TabSemanticIntegration, normalizedCollapsedStaffReachesExistingSemanticReader)
{
    const juce::String source =
        "Standard (EADGBE)\n"
        "e|------| B|------| G|--1---| D|------| A|------| E|------|\n";

    AsciiTabReader reader;
    PerformanceScore score;
    TabImportDiagnostics diagnostics;
    CHECK_MSG (reader.read (source, score, &diagnostics), reader.getLastError());
    CHECK (diagnostics.collapsedRowsSplit >= 5);
    CHECK (firstNoteOnString (score, 2) != nullptr);
}

LUTHIER_TEST (TabSemanticIntegration, conflictingExplicitTuningsDoNotSilentlyRetune)
{
    const juce::String source =
        "Tuning: Drop D\n" + lowOpenStaff + "Tuning: DADGAD\n";

    AsciiTabReader reader;
    PerformanceScore score;
    TabImportDiagnostics diagnostics;
    CHECK_MSG (reader.read (source, score, &diagnostics), reader.getLastError());
    CHECK (diagnostics.metadataConflicts >= 1);
    CHECK (! diagnostics.warnings.isEmpty());

    // Conflicting declarations are deliberately not allowed to choose an
    // arbitrary alternate tuning. The labelled staff remains the safe source.
    CHECK (score.getTrack (0).tuning[5] == 40);
}
