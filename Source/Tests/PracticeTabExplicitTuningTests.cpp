#include "TestFramework.h"
#include "../UI/PracticeTabImporter.h"

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (PracticeTabImporter, reportsResolvedExplicitTuningForAsciiTab)
{
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getNonexistentChildFile ("luthier-explicit-tuning", ".tab", false);
    file.replaceWithText (
        "e|-----|\nB|-----|\nG|-----|\nD|-----|\nA|-----|\nE|--0--|\n"
        "Tuning: Drop D\n");

    PracticeTabImporter importer;
    PerformanceScore score;
    CHECK_MSG (importer.read (file, score), importer.getLastError());
    CHECK (importer.lastAsciiHadResolvedTuning());

    file.deleteFile();
}

LUTHIER_TEST (PracticeTabImporter, noTuningDeclarationDoesNotRequestAutoRetune)
{
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getNonexistentChildFile ("luthier-no-explicit-tuning", ".tab", false);
    file.replaceWithText (
        "e|-----|\nB|-----|\nG|-----|\nD|-----|\nA|-----|\nE|--0--|\n");

    PracticeTabImporter importer;
    PerformanceScore score;
    CHECK_MSG (importer.read (file, score), importer.getLastError());
    CHECK (! importer.lastAsciiHadResolvedTuning());

    file.deleteFile();
}

LUTHIER_TEST (PracticeTabImporter, conflictingTuningDeclarationsDoNotRequestAutoRetune)
{
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getNonexistentChildFile ("luthier-conflicting-tuning", ".tab", false);
    file.replaceWithText (
        "Tuning: Drop D\n"
        "e|-----|\nB|-----|\nG|-----|\nD|-----|\nA|-----|\nE|--0--|\n"
        "Tuning: Eb Standard\n");

    PracticeTabImporter importer;
    PerformanceScore score;
    CHECK_MSG (importer.read (file, score), importer.getLastError());
    CHECK (! importer.lastAsciiHadResolvedTuning());

    file.deleteFile();
}
