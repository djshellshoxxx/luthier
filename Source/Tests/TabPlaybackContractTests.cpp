#include "TestFramework.h"
#include "../Notation/TabImportPipeline.h"
#include "../Riffs/Riff.h"
#include "../Riffs/RiffCompiler.h"
#include "../Riffs/RiffTransposer.h"

using namespace luthier;
using namespace luthier::tests;

LUTHIER_TEST (TabPlaybackContract, importedTuningAndCapoSurviveRiffConversion)
{
    const juce::String source =
        "e|------|\n"
        "B|------|\n"
        "G|------|\n"
        "D|------|\n"
        "A|------|\n"
        "E|--0---|\n"
        "Tuning: Drop D\n"
        "Capo: 2\n";

    TabImportPipeline importer;
    PerformanceScore score;
    CHECK_MSG (importer.read (source, score), importer.getLastError());

    const auto riff = Riff::fromScore (score);
    CHECK ((int) riff.tuning.size() == 6);
    CHECK (riff.tuning[5] == 38);
    CHECK (riff.capo == 2);
    CHECK (riff.pitchOf (5, 0) == 40);
}

LUTHIER_TEST (TabPlaybackContract, compilingAgainstImportedInstrumentPreservesFretAndPitch)
{
    const juce::String source =
        "e|------|\n"
        "B|------|\n"
        "G|------|\n"
        "D|------|\n"
        "A|------|\n"
        "E|--3---|\n"
        "Tuning: Drop D\n";

    TabImportPipeline importer;
    PerformanceScore score;
    CHECK_MSG (importer.read (source, score), importer.getLastError());

    const auto riff = Riff::fromScore (score);
    const auto compiled = RiffCompiler::compile (riff, RiffPlaySettings{},
                                                  GuitarSpecSummary::forRiff (riff));
    CHECK (compiled != nullptr);
    CHECK (compiled != nullptr && compiled->placement.dropped == 0);
    CHECK (compiled != nullptr && ! compiled->placement.retuned);

    if (compiled != nullptr && ! compiled->placement.notes.empty())
    {
        const auto& note = compiled->placement.notes.front();
        CHECK (note.stringIndex == 5);
        CHECK (note.fret == 3);
        CHECK (note.midiNote == 41);
    }
}
