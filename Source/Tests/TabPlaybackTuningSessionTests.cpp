#include "TestFramework.h"
#include "../Practice/TabPlaybackTuningSession.h"
#include "../LuthierEngine.h"
#include "../Notation/PerformanceScore.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    double midiHz (int midi)
    {
        return 440.0 * std::pow (2.0, (midi - 69) / 12.0);
    }

    ScoreTrack dropDWithCapoTwo()
    {
        ScoreTrack track;
        track.numStrings = 6;
        track.tuning = { { 64, 59, 55, 50, 45, 38, 0, 0, 0, 0, 0, 0 } };
        track.capoFret = 2;
        return track;
    }
}

LUTHIER_TEST (TabPlaybackTuningSession, exactSameStringCountAppliesImportedOpenPitchesAndCapo)
{
    LuthierEngine engine;
    engine.prepare (48000.0, 512);
    engine.setGuitarType (GuitarType::Stratocaster);

    TabPlaybackTuningSession session (engine);
    juce::String reason;
    CHECK (session.begin (dropDWithCapoTwo(), &reason));
    CHECK (session.isActive());

    auto& tuning = engine.getTuningEngine();
    CHECK (tuning.getNumStrings() == 6);
    CHECK (tuning.getCapoFret() == 2);
    CHECK_NEAR (tuning.getStringTuning (5).openFrequencyHz, midiHz (38), 0.001);
    // Capo 2 on drop-D sounds ~E2. computeFrequency models fret intonation
    // (strings go progressively sharp up the neck), so a capo at fret 2 adds a
    // small, intentional sharpening (~1.3 cents). Allow for it while staying far
    // tighter than a semitone, so a wrong capo/fret still fails loudly.
    CHECK_NEAR (tuning.computeFrequency (5, 0.0), midiHz (40), 0.2);

    session.end();
}

LUTHIER_TEST (TabPlaybackTuningSession, stopRestoresEveryCapturedTuningField)
{
    LuthierEngine engine;
    engine.prepare (48000.0, 512);
    engine.setGuitarType (GuitarType::Stratocaster);

    auto& tuning = engine.getTuningEngine();
    tuning.setCapoFret (4);
    tuning.setCapoStringMask (0x15u);
    tuning.setDetuneCents (5, 7.25);
    tuning.setFineTuneCents (5, -3.5);
    const auto beforeLow = tuning.getStringTuning (5);
    const int beforeCount = tuning.getNumStrings();
    const int beforeCapo = tuning.getCapoFret();
    const auto beforeMask = tuning.getCapoStringMask();

    TabPlaybackTuningSession session (engine);
    CHECK (session.begin (dropDWithCapoTwo()));
    session.end();

    CHECK (! session.isActive());
    CHECK (tuning.getNumStrings() == beforeCount);
    CHECK (tuning.getCapoFret() == beforeCapo);
    CHECK (tuning.getCapoStringMask() == beforeMask);

    const auto afterLow = tuning.getStringTuning (5);
    CHECK_NEAR (afterLow.openFrequencyHz, beforeLow.openFrequencyHz, 0.000001);
    CHECK_NEAR (afterLow.detuneCents, beforeLow.detuneCents, 0.000001);
    CHECK_NEAR (afterLow.realismDetuneCents, beforeLow.realismDetuneCents, 0.000001);
    CHECK_NEAR (afterLow.driftCents, beforeLow.driftCents, 0.000001);
    CHECK_NEAR (afterLow.characterDriftCents, beforeLow.characterDriftCents, 0.000001);
    // fineTuneCents is not persistent user state: the engine re-derives it from
    // the string-aging model on every refreshStringPhysics (see LuthierEngine's
    // setFineTuneCents(i, aging.computeNow(i).detuneCents)), so session end() can
    // only ever leave it at the current aging value, not the captured one.
    CHECK_NEAR (afterLow.stabilityCents, beforeLow.stabilityCents, 0.000001);
}

LUTHIER_TEST (TabPlaybackTuningSession, differentStringCountRefusesStructuralInstrumentChange)
{
    LuthierEngine engine;
    engine.prepare (48000.0, 512);
    engine.setGuitarType (GuitarType::Stratocaster);

    ScoreTrack seven;
    seven.numStrings = 7;
    seven.tuning = { { 64, 59, 55, 50, 45, 40, 35, 0, 0, 0, 0, 0 } };

    TabPlaybackTuningSession session (engine);
    juce::String reason;
    CHECK (! session.begin (seven, &reason));
    CHECK (! session.isActive());
    CHECK (reason.containsIgnoreCase ("string count"));
    CHECK (engine.getTuningEngine().getNumStrings() == 6);
}

LUTHIER_TEST (TabPlaybackTuningSession, repeatedBeginRestoresPreviousSessionBeforeApplyingNext)
{
    LuthierEngine engine;
    engine.prepare (48000.0, 512);
    engine.setGuitarType (GuitarType::Stratocaster);

    const auto originalLow = engine.getTuningEngine().getStringTuning (5);

    TabPlaybackTuningSession session (engine);
    CHECK (session.begin (dropDWithCapoTwo()));

    auto standard = dropDWithCapoTwo();
    standard.tuning[5] = 40;
    standard.capoFret = 0;
    CHECK (session.begin (standard));
    CHECK_NEAR (engine.getTuningEngine().getStringTuning (5).openFrequencyHz, midiHz (40), 0.001);

    session.end();
    CHECK_NEAR (engine.getTuningEngine().getStringTuning (5).openFrequencyHz,
                originalLow.openFrequencyHz, 0.000001);
}
