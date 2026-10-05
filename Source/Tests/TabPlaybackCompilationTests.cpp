#include "TestFramework.h"
#include "../Riffs/Riff.h"
#include "../Riffs/RiffCompiler.h"
#include "../Riffs/RiffTransposer.h"
#include "../Notation/PerformanceScore.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    PerformanceScore dropDOpenLowString()
    {
        PerformanceScore score;
        score.clear();
        auto& track = score.getTrack (0);
        track.name = "Guitar";
        track.numStrings = 6;
        track.tuning = { { 64, 59, 55, 50, 45, 38, 0, 0, 0, 0, 0, 0 } };
        track.capoFret = 0;

        ScoreMeasure measure;
        measure.voices.resize (1);
        ScoreNote note;
        note.stringIndex = 5;
        note.fret = 0;
        note.midiNote = 38;
        note.startBeat = 0.0;
        note.durationBeats = 1.0;
        measure.voices[0].notes.push_back (note);
        track.measures.push_back (measure);
        return score;
    }
}

LUTHIER_TEST (TabPlaybackCompilation, importedTuningPreservesAuthorStringAndFret)
{
    const auto riff = Riff::fromScore (dropDOpenLowString());
    const auto tabInstrument = GuitarSpecSummary::forRiff (riff);
    const auto compiled = RiffCompiler::compile (riff, RiffPlaySettings{}, tabInstrument);

    CHECK (compiled != nullptr);
    CHECK (compiled->placement.dropped == 0);
    CHECK (compiled->placement.notes.size() == 1);
    CHECK (compiled->placement.notes[0].stringIndex == 5);
    CHECK (compiled->placement.notes[0].fret == 0);
    CHECK (compiled->placement.notes[0].midiNote == 38);
    CHECK (! compiled->placement.retuned);
}

LUTHIER_TEST (TabPlaybackCompilation, standardGuitarWouldOtherwiseRefretOrDropDropDOpenNote)
{
    const auto riff = Riff::fromScore (dropDOpenLowString());
    auto standard = GuitarSpecSummary::forRiff (riff);
    standard.tuning[5] = 40;

    const auto placement = RiffTransposer::place (riff, RiffPlaySettings{}, standard);
    const bool changed = placement.dropped != 0
                      || placement.notes.empty()
                      || placement.notes[0].stringIndex != 5
                      || placement.notes[0].fret != 0;
    CHECK (changed);
}
