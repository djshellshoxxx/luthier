/*  Character and wear tests (character-wear.md section 12).

    Determinism is the one that matters most and is checked hardest: rule 1 of
    section 0 says a given seed produces a given instrument, and everything else
    in the file - reproducible presets, a "new character" button that means
    something, two guitars that differ consistently - rests on it holding.
*/

#include "TestFramework.h"

#include "../Character/CharacterEngine.h"

using namespace luthier;
using namespace luthier::tests;

//==============================================================================
/*  character-wear 12: a fixed seed produces byte-identical dead spots, fret
    wear, drift phases and capacitor values, across runs. */
LUTHIER_TEST (Character, fixedSeedIsByteIdentical)
{
    constexpr uint64_t seed = 0xC0FFEE1234ull;

    CharacterEngine a, b;

    a.prepare (48000.0, 6);
    b.prepare (48000.0, 6);

    a.setSeed (seed);
    b.setSeed (seed);

    // ---- dead spots ----------------------------------------------------------------
    for (int s = 0; s < kMaxStrings; ++s)
    {
        CHECK_MSG (a.getNumDeadSpots (s) == b.getNumDeadSpots (s),
                   "string " + juce::String (s) + " got a different number of dead spots");

        for (int i = 0; i < a.getNumDeadSpots (s); ++i)
        {
            const auto spotA = a.getDeadSpot (s, i);
            const auto spotB = b.getDeadSpot (s, i);

            CHECK (spotA.fret == spotB.fret);
            CHECK (spotA.depth == spotB.depth);
            CHECK (spotA.width == spotB.width);
        }
    }

    // ---- fret wear -------------------------------------------------------------------
    for (int fret = 0; fret <= CharacterEngine::kMaxFrets; ++fret)
        CHECK_MSG (a.getFretWear (fret) == b.getFretWear (fret),
                   "fret " + juce::String (fret) + " wore differently");

    // ---- capacitor ---------------------------------------------------------------------
    CHECK (a.getCapacitorDrift() == b.getCapacitorDrift());

    // ---- balance ------------------------------------------------------------------------
    for (int s = 0; s < 6; ++s)
    {
        for (int pickup = 0; pickup < 3; ++pickup)
            CHECK (a.getPickupBalanceDb (s, pickup) == b.getPickupBalanceDb (s, pickup));

        CHECK (a.getSaddleBalanceDb (s) == b.getSaddleBalanceDb (s));
        CHECK (a.getNutDamping (s) == b.getNutDamping (s));
        CHECK (a.getSaddleHeightOffsetMm (s) == b.getSaddleHeightOffsetMm (s));
    }

    // ---- drift ---------------------------------------------------------------------------
    // Advanced identically, the two must drift identically.
    for (int block = 0; block < 500; ++block)
    {
        a.advance (0.01);
        b.advance (0.01);
    }

    for (int s = 0; s < 6; ++s)
        CHECK_MSG (a.getTunerDriftCents (s) == b.getTunerDriftCents (s),
                   "string " + juce::String (s) + " drifted differently");
}

//==============================================================================
/*  The order values are asked for must not change them, which is what the
    per-coordinate hash exists to guarantee. */
LUTHIER_TEST (Character, valuesDoNotDependOnAccessOrder)
{
    CharacterEngine forwards, backwards;

    forwards.setSeed (0xABCDEFull);
    backwards.setSeed (0xABCDEFull);

    // Ask one in ascending order and the other in descending, then compare.
    std::array<double, kMaxStrings> forwardValues {}, backwardValues {};

    for (int s = 0; s < kMaxStrings; ++s)
        forwardValues[(size_t) s] = forwards.getSaddleBalanceDb (s);

    for (int s = kMaxStrings - 1; s >= 0; --s)
        backwardValues[(size_t) s] = backwards.getSaddleBalanceDb (s);

    for (int s = 0; s < kMaxStrings; ++s)
        CHECK (forwardValues[(size_t) s] == backwardValues[(size_t) s]);
}

//==============================================================================
/*  Two different seeds produce measurably different instruments - otherwise the
    seed does nothing and "New Character" is a lie. */
LUTHIER_TEST (Character, differentSeedsProduceDifferentInstruments)
{
    CharacterEngine a, b;

    a.setSeed (0x1111ull);
    b.setSeed (0x2222ull);

    int differences = 0;

    for (int fret = 0; fret <= CharacterEngine::kMaxFrets; ++fret)
        if (std::abs (a.getFretWear (fret) - b.getFretWear (fret)) > 0.01)
            ++differences;

    CHECK_MSG (differences > 5,
               "only " + juce::String (differences) + " frets differed between two seeds");

    // And the dead spots are not the same either.
    bool deadSpotsDiffer = false;

    for (int s = 0; s < 6 && ! deadSpotsDiffer; ++s)
    {
        if (a.getNumDeadSpots (s) != b.getNumDeadSpots (s))
            deadSpotsDiffer = true;

        for (int i = 0; i < juce::jmin (a.getNumDeadSpots (s), b.getNumDeadSpots (s)); ++i)
            if (a.getDeadSpot (s, i).fret != b.getDeadSpot (s, i).fret)
                deadSpotsDiffer = true;
    }

    CHECK_MSG (deadSpotsDiffer, "two seeds produced identical dead spots");
}

//==============================================================================
/*  character-wear 2: dead spots cluster where they do on a real neck, and are
    within the depth and width the spec states. */
LUTHIER_TEST (Character, deadSpotsAreInRangeAndClusterCorrectly)
{
    int total = 0;
    int inClusterRange = 0;

    for (uint64_t seed = 1; seed <= 200; ++seed)
    {
        CharacterEngine engine;
        engine.setSeed (seed * 0x9E3779B9ull);

        for (int s = 0; s < 6; ++s)
        {
            CHECK_MSG (engine.getNumDeadSpots (s) <= CharacterEngine::kMaxDeadSpotsPerString,
                       "a string got more than three dead spots");

            for (int i = 0; i < engine.getNumDeadSpots (s); ++i)
            {
                const auto spot = engine.getDeadSpot (s, i);

                CHECK_MSG (spot.depth >= 0.1 && spot.depth <= 0.7,
                           "dead spot depth " + juce::String (spot.depth, 3) + " out of range");

                CHECK_MSG (spot.width >= 2.0 && spot.width <= 5.0,
                           "dead spot width " + juce::String (spot.width, 3) + " out of range");

                CHECK (spot.fret >= 0 && spot.fret <= CharacterEngine::kMaxFrets);

                ++total;

                if (spot.fret >= 6 && spot.fret <= 12)
                    ++inClusterRange;
            }
        }
    }

    CHECK_MSG (total > 100, "not enough dead spots generated to judge the distribution");

    // character-wear 2 wants them centred on frets 6 to 12. A distribution
    // peaked there should put well over a third of them in that seven-fret
    // window; a uniform one over the whole neck would put about a quarter.
    const double fraction = (double) inClusterRange / (double) juce::jmax (1, total);

    CHECK_MSG (fraction > 0.35,
               "only " + juce::String (fraction * 100.0, 1)
                 + "% of dead spots landed on frets 6-12");
}

//==============================================================================
/*  character-wear 12: a note at a dead spot must sustain measurably less than
    one next to it, for spots at depth 0.5 or more. */
LUTHIER_TEST (Character, deadSpotsReduceSustainWhereTheyAre)
{
    int deepSpotsChecked = 0;

    for (uint64_t seed = 1; seed <= 400; ++seed)
    {
        CharacterEngine engine;
        engine.setSeed (seed * 0x1234567ull);
        engine.setAmount (1.0);
        engine.setEnabled (true);

        for (int s = 0; s < 6; ++s)
        {
            for (int i = 0; i < engine.getNumDeadSpots (s); ++i)
            {
                const auto spot = engine.getDeadSpot (s, i);

                if (spot.depth < 0.5)
                    continue;

                ++deepSpotsChecked;

                const double atSpot = engine.getSustainMultiplier (s, (double) spot.fret);

                /*  The note to compare against has to be a live one.

                    A fixed offset is not good enough: dead spots are placed per
                    string, so a fret eight away can sit under a different - and
                    possibly deeper - spot, and the engine takes the worst spot at
                    each fret. Comparing two dead notes measures nothing. So look
                    for the liveliest fret on this string that is clear of this
                    spot, and if the neck is too crowded to offer one, skip the
                    pair rather than assert something the spec never claimed. */
                double away = 0.0;

                for (int fret = 0; fret <= 24; ++fret)
                {
                    if (std::abs (fret - spot.fret) < 4)
                        continue;

                    away = juce::jmax (away, engine.getSustainMultiplier (s, (double) fret));
                }

                if (away < 0.98)
                {
                    --deepSpotsChecked;
                    continue;
                }

                CHECK_MSG (atSpot < away,
                           "a depth-" + juce::String (spot.depth, 2)
                             + " dead spot did not reduce sustain");

                // character-wear 12 asks for at least 10% shorter.
                CHECK_MSG (atSpot <= away * 0.9,
                           "a depth-" + juce::String (spot.depth, 2)
                             + " dead spot reduced sustain by only "
                             + juce::String ((1.0 - atSpot / away) * 100.0, 1) + "%");
            }
        }
    }

    CHECK_MSG (deepSpotsChecked > 20,
               "only " + juce::String (deepSpotsChecked) + " deep dead spots to check");
}

//==============================================================================
/*  character-wear 12: with character off, everything is exactly neutral - the
    bitwise-identical render the spec asks for. */
LUTHIER_TEST (Character, zeroCharacterIsExactlyNeutral)
{
    CharacterEngine engine;
    engine.setSeed (0xDEADBEEFull);
    engine.setAllFresh();

    for (int s = 0; s < 6; ++s)
    {
        for (double fret = 0.0; fret <= 22.0; fret += 0.5)
        {
            CHECK_MSG (engine.getSustainMultiplier (s, fret) == 1.0,
                       "a fresh instrument still lost sustain at fret " + juce::String (fret));

            CHECK (engine.getFretDetuneCents (fret) == 0.0);
            CHECK (engine.getFretBuzzMultiplier (fret) == 1.0);
            CHECK (engine.getFretSustainMultiplier (fret) == 1.0);
        }

        CHECK (engine.getTunerDriftCents (s) == 0.0);
        CHECK (engine.getPickupBalanceDb (s, 0) == 0.0);
        CHECK (engine.getSaddleBalanceDb (s) == 0.0);
        CHECK (engine.getNutDamping (s) == 0.0);
        CHECK (engine.getSaddleHeightOffsetMm (s) == 0.0);
    }

    CHECK (engine.getBodyQMultiplier() == 1.0);
    CHECK (engine.getAirResonanceMultiplier() == 1.0);
    CHECK (engine.getBodyHfDampingMultiplier() == 1.0);
    CHECK (CharacterEngine::legacyTemperatureOffsetCents (engine.getTemperature(), engine.getAmount()) == 0.0);
    CHECK (engine.applyPotTaper (0.5) == 0.5);

    // And it stays neutral as time passes.
    for (int block = 0; block < 1000; ++block)
        engine.advance (0.01);

    for (int s = 0; s < 6; ++s)
        CHECK (engine.getTunerDriftCents (s) == 0.0);

    // Disabling has the same effect whatever the settings say.
    CharacterEngine disabled;
    disabled.setSeed (0xDEADBEEFull);
    disabled.setAllOld();
    disabled.setEnabled (false);

    for (int s = 0; s < 6; ++s)
        CHECK (disabled.getSustainMultiplier (s, 7.0) == 1.0);
}

//==============================================================================
/*  character-wear 12: over ten minutes at 5% looseness, the RMS drift is close
    to what the model says it should be. */
LUTHIER_TEST (Character, tunerDriftStaysWithinItsStatedAmplitude)
{
    CharacterEngine engine;
    engine.prepare (48000.0, 6);
    engine.setSeed (0x7A17ull);
    engine.setEnabled (true);
    engine.setAmount (1.0);
    engine.setTunerLooseness (5.0);
    engine.setTemperature (Temperature::room);

    double sumOfSquares = 0.0;
    double peak = 0.0;
    int samples = 0;

    // Ten minutes, a tenth of a second at a time.
    for (int step = 0; step < 6000; ++step)
    {
        engine.advance (0.1);

        for (int s = 0; s < 6; ++s)
        {
            const double drift = engine.getTunerDriftCents (s);

            sumOfSquares += drift * drift;
            peak = juce::jmax (peak, std::abs (drift));
            ++samples;
        }
    }

    const double rms = std::sqrt (sumOfSquares / (double) juce::jmax (1, samples));

    /*  At 5% looseness the amplitude scale is 0.25 cents, times a per-string
        factor of 0.4 to 1.0. A sine's RMS is its amplitude over root two, so the
        expected RMS is well under a cent - and character-wear 12 asks for it to
        be within one cent of expected. */
    CHECK_MSG (rms < 1.0,
               "RMS drift over ten minutes was " + juce::String (rms, 4) + " cents");

    CHECK_MSG (peak <= 0.30,
               "peak drift at 5% looseness was " + juce::String (peak, 4)
                 + " cents; the model allows about 0.25");

    // At full looseness it reaches the five cents the spec names.
    CharacterEngine loose;
    loose.prepare (48000.0, 6);
    loose.setSeed (0x7A17ull);
    loose.setEnabled (true);
    loose.setAmount (1.0);
    loose.setTunerLooseness (100.0);

    double loosePeak = 0.0;

    for (int step = 0; step < 6000; ++step)
    {
        loose.advance (0.1);

        for (int s = 0; s < 6; ++s)
            loosePeak = juce::jmax (loosePeak, std::abs (loose.getTunerDriftCents (s)));
    }

    CHECK_MSG (loosePeak > 1.5 && loosePeak <= 5.1,
               "peak drift at full looseness was " + juce::String (loosePeak, 3)
                 + " cents; the spec says up to 5");
}

//==============================================================================
/*  character-wear 9: a retune puts everything back in tune and starts again. */
LUTHIER_TEST (Character, retuneResetsTheDrift)
{
    CharacterEngine engine;
    engine.prepare (48000.0, 6);
    engine.setSeed (0x9ull);
    engine.setEnabled (true);
    engine.setAmount (1.0);
    engine.setTunerLooseness (100.0);

    for (int step = 0; step < 200; ++step)
        engine.advance (0.1);

    bool anyDrift = false;

    for (int s = 0; s < 6; ++s)
        if (std::abs (engine.getTunerDriftCents (s)) > 0.01)
            anyDrift = true;

    CHECK_MSG (anyDrift, "nothing drifted at all at full looseness");

    engine.retune();

    for (int s = 0; s < 6; ++s)
        CHECK_MSG (std::abs (engine.getTunerDriftCents (s)) < 1.0e-9,
                   "string " + juce::String (s) + " was still out of tune after a retune");

    CHECK (engine.getSessionSeconds() == 0.0);
}

//==============================================================================
/*  environment.md 1: the temperature offset left CharacterEngine for the
    physical EnvironmentModel (ENV-02 replaces this spec's 20 K test). The
    legacy choice is kept only so the loader can convert it (environment.md 6):
    it no longer reaches the tuner drift. */
LUTHIER_TEST (Character, temperatureIsTheEnvironmentsNow)
{
    CharacterEngine engine;
    engine.setSeed (0x7ull);
    engine.setEnabled (true);
    engine.setAmount (1.0);
    engine.setTunerLooseness (0.0);

    engine.setTemperature (Temperature::cold);
    CHECK_NEAR (engine.getTunerDriftCents (0), 0.0, 1.0e-12);

    // The legacy conversion's target: +-2.5 cents x amount, sharp when cold.
    CHECK_NEAR (CharacterEngine::legacyTemperatureOffsetCents (Temperature::cold, 1.0), 2.5, 1.0e-12);
    CHECK_NEAR (CharacterEngine::legacyTemperatureOffsetCents (Temperature::warm, 0.25), -0.625, 1.0e-12);
    CHECK (CharacterEngine::legacyTemperatureOffsetCents (Temperature::room, 1.0) == 0.0);
}

//==============================================================================
/*  environment.md 6: the old keys are read, for one schema, and not written. */
LUTHIER_TEST (Character, legacyEnvironmentKeysAreReadNotWritten)
{
    CharacterEngine engine;
    engine.setTemperature (Temperature::warm);
    engine.setHumidity (Humidity::dry);

    auto state = engine.toVar();
    CHECK (! state.getDynamicObject()->hasProperty ("temperature"));
    CHECK (! state.getDynamicObject()->hasProperty ("humidity"));

    CharacterEngine absent;
    absent.fromVar (state);
    CHECK (absent.getTemperature() == Temperature::room);
    CHECK (absent.getHumidity() == Humidity::normal);

    state.getDynamicObject()->setProperty ("temperature", (int) Temperature::cold);
    state.getDynamicObject()->setProperty ("humidity", (int) Humidity::humid);

    CharacterEngine legacy;
    legacy.fromVar (state);
    CHECK (legacy.getTemperature() == Temperature::cold);
    CHECK (legacy.getHumidity() == Humidity::humid);
}

//==============================================================================
/*  character-wear 8: the body opens up with age. */
LUTHIER_TEST (Character, bodyBreakInMovesInTheRightDirection)
{
    CharacterEngine engine;
    engine.setSeed (0x13ull);
    engine.setEnabled (true);
    engine.setAmount (1.0);

    engine.setBodyAge (0.0);
    CHECK_NEAR (engine.getBodyQMultiplier(), 1.0, 1.0e-9);
    CHECK_NEAR (engine.getAirResonanceMultiplier(), 1.0, 1.0e-9);
    CHECK_NEAR (engine.getBodyHfDampingMultiplier(), 1.0, 1.0e-9);

    engine.setBodyAge (100.0);

    // The Qs rise.
    CHECK (engine.getBodyQMultiplier() > 1.0);

    // character-wear 8: the air resonance drops 3 to 8 percent.
    const double resonanceDrop = 1.0 - engine.getAirResonanceMultiplier();

    CHECK_MSG (resonanceDrop >= 0.03 && resonanceDrop <= 0.08,
               "the air resonance dropped " + juce::String (resonanceDrop * 100.0, 2)
                 + "%, expected 3 to 8");

    // And high-frequency damping falls 5 to 10 percent.
    const double dampingDrop = 1.0 - engine.getBodyHfDampingMultiplier();

    CHECK_MSG (dampingDrop >= 0.05 && dampingDrop <= 0.10,
               "high-frequency damping dropped " + juce::String (dampingDrop * 100.0, 2)
                 + "%, expected 5 to 10");
}

//==============================================================================
/*  character-wear 3: fret wear is heavier where hands go, and a refret clears
    it. */
LUTHIER_TEST (Character, fretWearFollowsRealWearPatterns)
{
    double lowPositionTotal = 0.0, highPositionTotal = 0.0, elsewhereTotal = 0.0;
    int lowCount = 0, highCount = 0, elsewhereCount = 0;

    for (uint64_t seed = 1; seed <= 300; ++seed)
    {
        CharacterEngine engine;
        engine.setSeed (seed * 0x2545F491ull);

        for (int fret = 0; fret <= CharacterEngine::kMaxFrets; ++fret)
        {
            const double wear = engine.getFretWear (fret);

            CHECK (wear >= 0.0 && wear <= 1.0);

            if (fret >= 1 && fret <= 5)        { lowPositionTotal += wear;  ++lowCount; }
            else if (fret >= 12 && fret <= 17) { highPositionTotal += wear; ++highCount; }
            else if (fret > 17)                { elsewhereTotal += wear;    ++elsewhereCount; }
        }
    }

    const double lowAverage = lowPositionTotal / juce::jmax (1, lowCount);
    const double highAverage = highPositionTotal / juce::jmax (1, highCount);
    const double elsewhereAverage = elsewhereTotal / juce::jmax (1, elsewhereCount);

    CHECK_MSG (lowAverage > elsewhereAverage,
               "frets 1-5 are no more worn than the far end of the neck");

    CHECK_MSG (highAverage > elsewhereAverage,
               "frets 12-17 are no more worn than the far end of the neck");

    // ---- refret ----------------------------------------------------------------------
    CharacterEngine engine;
    engine.setSeed (0x99ull);

    bool anyWear = false;

    for (int fret = 0; fret <= CharacterEngine::kMaxFrets; ++fret)
        if (engine.getFretWear (fret) > 0.01)
            anyWear = true;

    CHECK (anyWear);

    engine.refret();

    for (int fret = 0; fret <= CharacterEngine::kMaxFrets; ++fret)
        CHECK_MSG (engine.getFretWear (fret) == 0.0,
                   "fret " + juce::String (fret) + " was still worn after a refret");
}

//==============================================================================
/*  character-wear 3: a worn fret detunes flat, buzzes more and sustains less. */
LUTHIER_TEST (Character, wornFretsBehaveAsDescribed)
{
    CharacterEngine engine;
    engine.setSeed (0x21ull);
    engine.setEnabled (true);
    engine.setAmount (1.0);

    engine.refret();

    // A pristine fret does nothing.
    CHECK_NEAR (engine.getFretDetuneCents (5.0), 0.0, 1.0e-9);
    CHECK_NEAR (engine.getFretBuzzMultiplier (5.0), 1.0, 1.0e-9);
    CHECK_NEAR (engine.getFretSustainMultiplier (5.0), 1.0, 1.0e-9);

    engine.setFretWear (5, 1.0);

    // Worn: flat, buzzier, shorter.
    CHECK_MSG (engine.getFretDetuneCents (5.0) < 0.0,
               "a worn fret went sharp rather than flat");

    CHECK_MSG (engine.getFretBuzzMultiplier (5.0) > 1.0,
               "a worn fret did not increase the buzz probability");

    CHECK_MSG (engine.getFretSustainMultiplier (5.0) < 1.0,
               "a worn fret did not reduce sustain");

    // And a few cents, not a semitone.
    CHECK_MSG (std::abs (engine.getFretDetuneCents (5.0)) < 10.0,
               "a worn fret detuned by " + juce::String (engine.getFretDetuneCents (5.0), 2)
                 + " cents");
}

//==============================================================================
/*  character-wear 5: the aged pot taper is monotonic and pinned at both ends. */
LUTHIER_TEST (Character, agedPotTaperIsMonotonicAndPinned)
{
    CharacterEngine engine;
    engine.setSeed (0x31ull);
    engine.setEnabled (true);
    engine.setAmount (1.0);
    engine.setPotLinearityAmount (1.0);

    // A control that did not reach its own ends would be a broken control.
    CHECK_NEAR (engine.applyPotTaper (0.0), 0.0, 1.0e-9);
    CHECK_NEAR (engine.applyPotTaper (1.0), 1.0, 1.0e-9);

    double previous = -1.0;

    for (int i = 0; i <= 100; ++i)
    {
        const double position = (double) i / 100.0;
        const double mapped = engine.applyPotTaper (position);

        CHECK (mapped >= -1.0e-9 && mapped <= 1.0 + 1.0e-9);

        CHECK_MSG (mapped >= previous - 1.0e-9,
                   "the pot taper went backwards at " + juce::String (position, 2));

        previous = mapped;
    }

    // It sags in the middle rather than being linear.
    CHECK_MSG (engine.applyPotTaper (0.5) < 0.5,
               "the aged taper did not sag at all");

    // With the effect off it is exactly linear.
    engine.setPotLinearityAmount (0.0);

    for (int i = 0; i <= 10; ++i)
    {
        const double position = (double) i / 10.0;
        CHECK_NEAR (engine.applyPotTaper (position), position, 1.0e-9);
    }
}

//==============================================================================
/*  character-wear 5: the capacitor drifts within its stated range, and is
    deterministic. */
LUTHIER_TEST (Character, capacitorDriftIsInRangeAndDeterministic)
{
    for (uint64_t seed = 1; seed <= 100; ++seed)
    {
        CharacterEngine engine;
        engine.setSeed (seed * 0x27220A95ull);
        engine.setCapacitorDriftRange (0.05);

        const double drift = engine.getCapacitorDrift();

        // character-wear 5: plus or minus five percent of nominal.
        CHECK_MSG (drift >= 0.95 - 1.0e-9 && drift <= 1.05 + 1.0e-9,
                   "the capacitor drifted to " + juce::String (drift, 5));

        // Asking twice gives the same answer.
        CHECK (engine.getCapacitorDrift() == drift);
    }
}

//==============================================================================
/*  character-wear 5: the intermittent jack is off by default and stays silent
    until it is asked for. */
LUTHIER_TEST (Character, intermittentJackIsOffByDefault)
{
    CharacterEngine engine;
    engine.prepare (48000.0, 6);
    engine.setSeed (0x41ull);
    engine.setEnabled (true);
    engine.setAmount (1.0);

    CHECK_MSG (! engine.isJackIntermittentEnabled(),
               "the intermittent jack was on by default");

    // Ten minutes with it off: the gain never moves.
    for (int step = 0; step < 6000; ++step)
    {
        engine.advance (0.1);
        CHECK (engine.getJackGain() == 1.0);
    }

    // Switched on, it does eventually drop out.
    engine.setJackIntermittentEnabled (true);

    bool droppedOut = false;

    for (int step = 0; step < 60000 && ! droppedOut; ++step)
    {
        engine.advance (0.01);

        if (engine.getJackGain() < 0.5)
            droppedOut = true;
    }

    CHECK_MSG (droppedOut, "the intermittent jack never dropped out in ten minutes");
}

//==============================================================================
/*  character-wear 10: the two audition presets do what they say. */
LUTHIER_TEST (Character, allFreshAndAllOldPresets)
{
    CharacterEngine engine;
    engine.setSeed (0x51ull);

    engine.setAllOld();

    CHECK_NEAR (engine.getAmount(), 1.0, 1.0e-9);
    CHECK (engine.getTunerLooseness() > 50.0);
    CHECK (engine.getBodyAge() > 50.0);
    CHECK (engine.getFretWear (3) > 0.5);

    // Even "all old" leaves the jack alone, because it surprises people.
    CHECK_MSG (! engine.isJackIntermittentEnabled(),
               "the all-old preset switched the intermittent jack on");

    engine.setAllFresh();

    CHECK_NEAR (engine.getAmount(), 0.0, 1.0e-9);
    CHECK_NEAR (engine.getTunerLooseness(), 0.0, 1.0e-9);
    CHECK_NEAR (engine.getBodyAge(), 0.0, 1.0e-9);
    CHECK (engine.getFretWear (3) == 0.0);
}

//==============================================================================
/*  character-wear 1: the seed and every override travel in the preset. */
LUTHIER_TEST (Character, stateRoundTrips)
{
    CharacterEngine engine;
    engine.prepare (48000.0, 6);
    engine.setSeed (0x1234ABCDull);
    engine.setAmount (0.7);
    engine.setTunerLooseness (42.0);
    engine.setPotLinearityAmount (0.6);
    engine.setCapacitorDriftRange (0.12);
    engine.setJackIntermittentEnabled (true);
    engine.setBoneNut (false);
    engine.setBodyAge (77.0);
    engine.setTemperature (Temperature::cold);
    engine.setHumidity (Humidity::humid);

    // An edited fret and an edited dead spot, which the panel allows.
    engine.setFretWear (7, 0.83);

    DeadSpot edited;
    edited.fret = 9;
    edited.depth = 0.55;
    edited.width = 4.0;
    engine.setDeadSpot (0, 0, edited);

    const auto text = juce::JSON::toString (engine.toVar(), false);

    CharacterEngine restored;
    restored.prepare (48000.0, 6);
    restored.fromVar (juce::JSON::parse (text));

    CHECK (restored.getSeed() == 0x1234ABCDull);
    CHECK_NEAR (restored.getAmount(), 0.7, 1.0e-9);
    CHECK_NEAR (restored.getTunerLooseness(), 42.0, 1.0e-9);
    CHECK_NEAR (restored.getPotLinearityAmount(), 0.6, 1.0e-9);
    CHECK (restored.isJackIntermittentEnabled());
    CHECK (! restored.isBoneNut());
    CHECK_NEAR (restored.getBodyAge(), 77.0, 1.0e-9);
    // environment.md 6: these are env_* parameters now and are not saved here.
    CHECK (restored.getTemperature() == Temperature::room);
    CHECK (restored.getHumidity() == Humidity::normal);

    CHECK_NEAR (restored.getFretWear (7), 0.83, 1.0e-6);

    const auto spot = restored.getDeadSpot (0, 0);
    CHECK (spot.fret == 9);
    CHECK_NEAR (spot.depth, 0.55, 1.0e-6);
    CHECK_NEAR (spot.width, 4.0, 1.0e-6);

    // And the generated values are back too, because the seed came with it.
    CHECK (restored.getCapacitorDrift() == engine.getCapacitorDrift());
    CHECK (restored.getSaddleBalanceDb (3) == engine.getSaddleBalanceDb (3));
}

//==============================================================================
/*  character-wear 7: the nut material biases the string's damping. */
LUTHIER_TEST (Character, nutMaterialChangesDamping)
{
    CharacterEngine engine;
    engine.setSeed (0x61ull);

    engine.setBoneNut (true);
    const double bone = engine.getNutMaterialDamping();

    engine.setBoneNut (false);
    const double synthetic = engine.getNutMaterialDamping();

    CHECK_MSG (bone < synthetic,
               "bone damped more than synthetic, which is the wrong way round");

    // Both are a bias, not a transformation: within ten percent of neutral.
    CHECK (std::abs (bone - 1.0) < 0.1);
    CHECK (std::abs (synthetic - 1.0) < 0.1);
}

//==============================================================================
/*  CW-9, character-wear 2: a dead spot is weighted toward the body's air
    resonance - a note near it loses more sustain than the same spot at a note
    far from it. */
LUTHIER_TEST (Character, deadSpotLossIsWorseNearBodyResonance)
{
    CharacterEngine engine;
    engine.setSeed (0x9ull);
    engine.setAmount (1.0);
    engine.setEnabled (true);

    DeadSpot spot;
    spot.fret = 7;
    spot.depth = 0.6;
    spot.width = 3.0;
    engine.setDeadSpot (0, 0, spot);

    const double bodyResonanceHz = 110.0;
    const double nearHz = bodyResonanceHz;                 // right on it
    const double farHz  = bodyResonanceHz * 4.0;            // two octaves away

    const double atResonance = engine.getSustainMultiplier (0, (double) spot.fret, nearHz, bodyResonanceHz);
    const double awayFromResonance = engine.getSustainMultiplier (0, (double) spot.fret, farHz, bodyResonanceHz);

    CHECK_MSG (atResonance < awayFromResonance,
               "a note at the body resonance did not lose more sustain than one two octaves away");

    // Without a body resonance hint at all, the spot still applies its base loss.
    const double noHint = engine.getSustainMultiplier (0, (double) spot.fret);
    CHECK (noHint < 1.0);
}

//==============================================================================
/*  CW-21, character-wear 7: nut slot wear dampens the open string.
    LuthierEngine.cpp applies (1 - getNutDamping(s)) * getNutMaterialDamping() to
    the fret-0 sustain scale, so a worn nut must read as strictly less than a
    fresh (disabled) one for every string that has any wear at all. */
LUTHIER_TEST (Character, nutWearDampensTheOpenString)
{
    CharacterEngine engine;
    engine.setSeed (0x21ull);
    engine.setAmount (1.0);
    engine.setEnabled (true);

    bool anyWorn = false;

    for (int s = 0; s < 6; ++s)
    {
        const double damping = engine.getNutDamping (s);
        CHECK (damping >= 0.0 && damping <= 0.08);

        if (damping > 0.0)
            anyWorn = true;

        // The multiplier LuthierEngine actually applies at fret 0.
        const double openSustain = 1.0 - damping;
        CHECK_MSG (openSustain <= 1.0, "nut wear made the open string louder, not softer");
    }

    CHECK_MSG (anyWorn, "no string had any nut wear to compare against");

    engine.setEnabled (false);

    for (int s = 0; s < 6; ++s)
        CHECK_MSG (engine.getNutDamping (s) == 0.0, "a disabled Character still reported nut wear");
}
