/*  Setup geometry and sensed fret buzz (fret-buzz.md 9).

    The engine tests play real notes through the string model, because what is
    being tested is the claim that buzz follows the string's actual amplitude:
    a hard note on a bad setup rattles and then cleans up as it rings.
*/

#include "TestFramework.h"

#include "../DSP/Noise/FretBuzz.h"
#include "../LuthierEngine.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    SetupGeometry withStyle (int styleIndex)
    {
        SetupGeometry g;
        const auto& style = getSetupStyle (styleIndex);
        g.actionTreble = style.actionTreble;
        g.actionBass = style.actionBass;
        g.relief = style.relief;
        return g;
    }

    constexpr int kFactoryLow = 0, kClean = 2, kNeedsATech = 5;

    /*  Plays the lowest string at `fret` and returns, per 256-sample block,
        the fret it was buzzing on (-1 for none). */
    std::vector<int> playLowString (const SetupGeometry& setup, double fret, double velocity, double seconds)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.setGuitarType (GuitarType::Stratocaster);
        engine.setSetupGeometry (setup);

        const int low = engine.getNumStrings() - 1;

        NoteOnEvent e;
        e.stringIndex = low;
        e.fretPosition = fret;
        e.velocity = velocity;
        e.pitchHz = 82.41 * std::pow (2.0, fret / 12.0);
        engine.triggerNoteNow (e);

        juce::AudioBuffer<float> block (2, 256);
        std::vector<int> frets;

        for (int b = 0; b < (int) (seconds * 48000.0 / 256.0); ++b)
        {
            block.clear();
            juce::MidiBuffer none;
            engine.processBlock (block, none);
            frets.push_back (engine.getFretBuzz().getBuzzingFret (low));
        }

        return frets;
    }

    bool anyBuzz (const std::vector<int>& frets, size_t from, size_t to)
    {
        for (size_t i = from; i < juce::jmin (to, frets.size()); ++i)
            if (frets[i] >= 0)
                return true;

        return false;
    }

    constexpr size_t blocksFor (double seconds) { return (size_t) (seconds * 48000.0 / 256.0); }
}

//==============================================================================
LUTHIER_TEST (Buzz, lowActionBuzzesAndHighActionDoesNot)
{
    const auto bad = playLowString (withStyle (kNeedsATech), 0.0, 100.0 / 127.0, 1.0);
    CHECK_MSG (anyBuzz (bad, 0, bad.size()), "'Needs a tech' at velocity 100 did not buzz on the low E");

    const auto clean = playLowString (withStyle (kClean), 0.0, 1.0, 1.0);
    CHECK_MSG (! anyBuzz (clean, 0, clean.size()), "'Clean / high' at velocity 127 buzzed");
}

LUTHIER_TEST (Buzz, buzzStopsAsTheNoteDecays)
{
    const auto frets = playLowString (withStyle (kFactoryLow), 0.0, 1.0, 4.0);

    CHECK_MSG (anyBuzz (frets, 0, blocksFor (0.3)), "Factory low did not buzz in the first 300 ms");
    CHECK_MSG (! anyBuzz (frets, blocksFor (2.0), frets.size()), "still buzzing after 2 s");
}

LUTHIER_TEST (Buzz, onlyFretsAheadOfTheFingerBuzz)
{
    const auto frets = playLowString (withStyle (kNeedsATech), 7.0, 1.0, 1.0);

    for (auto f : frets)
        if (f >= 0 && f <= 7)
        {
            ctx.fail ("fretted at 7, buzzed on fret " + juce::String (f));
            break;
        }

    // And directly: nothing behind the finger is ever a candidate.
    const auto setup = withStyle (kNeedsATech);

    for (int f = 1; f <= 7; ++f)
        CHECK (setup.clearanceMm (5, 7.0, f) > 1.0e8);
}

LUTHIER_TEST (Buzz, reliefMovesWhereItBuzzes)
{
    /*  A high nut protects the low frets, which is when relief decides where a
        flat neck and a bowed one rattle: the flat neck around fret 5, the bowed
        one up where the bow ends. With a low nut both buzz on the first few
        frets and relief barely moves it - which is also true of a guitar. */
    auto worstFret = [] (double relief)
    {
        auto g = withStyle (kNeedsATech);
        g.relief = relief;
        g.nutDepth.fill (1.0);

        FretBuzz buzz;
        buzz.setGeometry (g);
        return buzz.sense (5, 0.0, 0.5, 0.16).fret;
    };

    const int flat = worstFret (0.0), bowed = worstFret (0.35);

    CHECK_MSG (std::abs (flat - bowed) >= 3,
               "relief 0.0 buzzes at fret " + juce::String (flat) + " and 0.35 at " + juce::String (bowed));
}

LUTHIER_TEST (Buzz, fretHeightChangesLevelNotPosition)
{
    auto g = withStyle (kNeedsATech);
    FretBuzz normal, tall;

    normal.setGeometry (g);
    g.fretHeight *= 2.0;
    tall.setGeometry (g);

    CHECK (normal.sense (5, 0.0, 0.5, 0.16).fret == tall.sense (5, 0.0, 0.5, 0.16).fret);
    CHECK (gainToDb (tall.levelFor (0.1) / normal.levelFor (0.1)) >= 3.0);
}

LUTHIER_TEST (Buzz, theThresholdIsATrimNotAMute)
{
    auto g = withStyle (kNeedsATech);
    g.buzzThreshold = 0.0;

    const auto frets = playLowString (g, 0.0, 1.0, 1.0);
    CHECK_MSG (anyBuzz (frets, 0, frets.size()), "threshold 0 silenced a bad setup");
}

LUTHIER_TEST (Buzz, sitarModeIsContinuous)
{
    auto g = withStyle (kClean);
    g.sitarMode = true;

    const auto frets = playLowString (g, 0.0, 0.8, 4.0);

    // Continuous through the decay, not attack-only: buzzing in every block of
    // the first three seconds.
    int silentBlocks = 0;

    // From 50 ms: the string has no level until its first block has run.
    for (size_t i = blocksFor (0.05); i < blocksFor (3.0); ++i)
        if (frets[i] < 0)
            ++silentBlocks;

    CHECK_MSG (silentBlocks == 0, juce::String (silentBlocks) + " silent blocks in sitar mode");
}

LUTHIER_TEST (Buzz, theHeatmapAgreesWithTheGenerator)
{
    FretBuzz buzz;
    buzz.setGeometry (withStyle (kNeedsATech));

    NoiseEngine pool;
    pool.prepare (48000.0);

    juce::Random random (42);
    int agreements = 0, checks = 0;

    for (int block = 0; block < 400; ++block)
    {
        std::array<double, 6> levels {}, fretted {}, fundamentals {};

        for (int s = 0; s < 6; ++s)
        {
            levels[(size_t) s] = random.nextDouble() * 0.6;
            fretted[(size_t) s] = (double) random.nextInt (10);
            fundamentals[(size_t) s] = 110.0;
        }

        buzz.process (pool, levels.data(), fretted.data(), fundamentals.data(), 6, 0.16);

        for (int s = 0; s < 6; ++s)
        {
            const int reported = buzz.getBuzzingFret (s);

            if (reported < 0)
                continue;

            int hottest = -1;
            float best = -1.0e9f;

            for (int f = 1; f <= SetupGeometry::kMaxFrets; ++f)
                if (buzz.getHeat (s, f) > best)
                {
                    best = buzz.getHeat (s, f);
                    hottest = f;
                }

            ++checks;

            if (hottest == reported)
                ++agreements;
        }
    }

    CHECK (checks > 0);
    CHECK_MSG (agreements == checks,
               juce::String (checks - agreements) + " of " + juce::String (checks)
                 + " blocks where the heatmap's hottest fret was not the buzzing one");
}


LUTHIER_TEST (Buzz, playerFriendlyBuzzesOnlyWhenAttackedHard)
{
    const auto gentle = playLowString (withStyle (1), 0.0, 100.0 / 127.0, 1.0);
    const auto hard = playLowString (withStyle (1), 0.0, 1.0, 1.0);

    CHECK_MSG (! anyBuzz (gentle, 0, gentle.size()), "Player-friendly buzzed at velocity 100");
    CHECK_MSG (anyBuzz (hard, 0, hard.size()), "Player-friendly never buzzed, even at 127");
}
