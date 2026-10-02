#include "TestFramework.h"
#include "../Notation/TabImportPipeline.h"

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

LUTHIER_TEST (TabImportPipeline, footerTuningAffectsPitchBeforeFirstNote)
{
    const juce::String source =
        "e|------|\n"
        "B|------|\n"
        "G|------|\n"
        "D|------|\n"
        "A|------|\n"
        "E|--0---|\n"
        "Tuning: Drop D\n";

    TabImportPipeline pipeline;
    PerformanceScore score;
    TabImportDiagnostics diagnostics;
    CHECK_MSG (pipeline.read (source, score, &diagnostics), pipeline.getLastError());
    CHECK (score.getTrack (0).tuning[5] == 38);
    const auto* note = firstNoteOnString (score, 5);
    CHECK (note != nullptr);
    if (note != nullptr)
        CHECK (note->midiNote == 38);
}

LUTHIER_TEST (TabImportPipeline, collapsedPhysicalStaffBecomesPlayableScore)
{
    const juce::String source =
        "Standard (EADGBE)\n"
        "e|------| B|------| G|--1---| D|------| A|------| E|------|\n";

    TabImportPipeline pipeline;
    PerformanceScore score;
    TabImportDiagnostics diagnostics;
    CHECK_MSG (pipeline.read (source, score, &diagnostics), pipeline.getLastError());
    CHECK (diagnostics.collapsedRowsSplit >= 5);
    CHECK (firstNoteOnString (score, 2) != nullptr);
}

LUTHIER_TEST (TabImportPipeline, conflictingTuningsRemainConservative)
{
    const juce::String source =
        "Tuning: Drop D\n"
        "e|------|\n"
        "B|------|\n"
        "G|------|\n"
        "D|------|\n"
        "A|------|\n"
        "E|--0---|\n"
        "Tuning: DADGAD\n";

    TabImportPipeline pipeline;
    PerformanceScore score;
    TabImportDiagnostics diagnostics;
    CHECK_MSG (pipeline.read (source, score, &diagnostics), pipeline.getLastError());
    CHECK (diagnostics.metadataConflicts >= 1);
    CHECK (! diagnostics.warnings.isEmpty());
    CHECK (score.getTrack (0).tuning[5] == 40);
}
