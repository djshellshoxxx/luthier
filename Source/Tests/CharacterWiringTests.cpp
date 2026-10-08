/*  SPEC-SWEEP: character-wear values that used to be computed and never heard.

    CharacterEngine derived the aged pot taper, the tone cap's drift, the
    intermittent jack, the saddle and pole balances, the saddle-height
    intonation and the worn-fret buzz multiplier from its seed, and nothing
    read them. These check that each one now reaches the sound, and that with
    character off (or all fresh) the engine is bit-for-bit what it was.
*/

#include "TestFramework.h"

#include "../Character/CharacterEngine.h"
#include "../DSP/Circuit/GuitarCircuit.h"
#include "../DSP/Noise/FretBuzz.h"
#include "../DSP/Noise/NoiseEngine.h"
#include "../DSP/Pickup/PickupEngine.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../UI/CharacterPanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    void quietRig (LuthierEngine& engine)
    {
        engine.getCabinetEngine().setEnabled (false);
        engine.getRoomEngine().setEnabled (false);
        engine.getAmpEngine().setGain (0.0);
    }

    /** Renders `blocks` blocks of one note, left channel, optionally calling
        `between` before each block. */
    std::vector<float> render (LuthierEngine& engine, int note, int blocks,
                               const std::function<void (int)>& between = {})
    {
        std::vector<float> out;
        out.reserve ((size_t) (blocks * kBlock));

        juce::AudioBuffer<float> buffer (2, kBlock);

        for (int b = 0; b < blocks; ++b)
        {
            if (between)
                between (b);

            juce::MidiBuffer midi;

            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.9f), 0);

            buffer.clear();
            engine.processBlock (buffer, midi);

            for (int i = 0; i < kBlock; ++i)
                out.push_back (buffer.getSample (0, i));
        }

        return out;
    }

    double rmsOf (const std::vector<float>& x, size_t from, size_t count)
    {
        double sum = 0.0;
        count = juce::jmin (count, x.size() - juce::jmin (from, x.size()));

        for (size_t i = from; i < from + count; ++i)
            sum += (double) x[i] * x[i];

        return count > 0 ? std::sqrt (sum / (double) count) : 0.0;
    }
}

//==============================================================================
/*  CW-16 / CW-17: the aged volume taper and the drifted tone cap are what the
    circuit gets, and the circuit's response moves with them. */
LUTHIER_TEST (CharacterWiring, agedPotAndCapReachTheCircuit)
{
    LuthierEngine engine;
    engine.prepare (kSr, kBlock);
    engine.setGuitarType (GuitarType::Stratocaster);

    auto& character = engine.getCharacterEngine();
    character.setEnabled (true);
    character.setAmount (1.0);
    character.setPotLinearityAmount (1.0);
    character.setCapacitorDriftRange (0.2);

    CircuitComponents controls;
    controls.volume = 0.6;
    controls.tone = 0.4;
    engine.setCircuitControls (controls);

    const auto live = engine.getLiveCircuitComponents();

    CHECK_NEAR (live.volume, character.applyPotTaper (0.6), 1.0e-12);
    CHECK_MSG (live.volume < 0.6, "an aged pot sags in the upper middle: " + juce::String (live.volume, 4));
    CHECK_NEAR (live.toneCap, controls.toneCap * character.getCapacitorDrift(), 1.0e-15);
    CHECK_MSG (std::abs (character.getCapacitorDrift() - 1.0) > 1.0e-4, "the seed should drift the cap");

    // The ends of the pot stay pinned.
    controls.volume = 1.0;
    engine.setCircuitControls (controls);
    CHECK_NEAR (engine.getLiveCircuitComponents().volume, 1.0, 1.0e-12);
    controls.volume = 0.0;
    engine.setCircuitControls (controls);
    CHECK_NEAR (engine.getLiveCircuitComponents().volume, 0.0, 1.0e-12);

    // The tone control's corner moves with the cap: the response at 2 kHz
    // with the tone half down differs from the nominal cap's.
    controls.volume = 1.0;
    engine.setCircuitControls (controls);
    auto aged = engine.getLiveCircuitComponents();
    auto nominal = aged;
    nominal.toneCap = controls.toneCap;
    CHECK_MSG (std::abs (GuitarCircuit::magnitudeDb (aged, 2000.0) - GuitarCircuit::magnitudeDb (nominal, 2000.0)) > 0.01,
               "a drifted tone cap should move the tone control's response");

    // Character off: the knobs are what the circuit sees.
    character.setEnabled (false);
    controls.volume = 0.6;
    engine.setCircuitControls (controls);
    CHECK (engine.getLiveCircuitComponents().volume == 0.6);
    CHECK (engine.getLiveCircuitComponents().toneCap == controls.toneCap);
}

//==============================================================================
/*  CW-18: with the jack enabled, a dropout takes the output down for 20-100 ms
    and it comes back; until one happens, and with the jack off, nothing
    differs from a guitar without the feature. */
LUTHIER_TEST (CharacterWiring, theIntermittentJackDropsOutAndRecovers)
{
    auto make = [] (bool jack)
    {
        auto engine = std::make_unique<LuthierEngine>();
        engine->prepare (kSr, kBlock);
        engine->setGuitarType (GuitarType::Stratocaster);
        quietRig (*engine);
        engine->getCharacterEngine().setSeed (0x1234ull);
        engine->getCharacterEngine().setJackIntermittentEnabled (jack);
        engine->reset();
        return engine;
    };

    auto withJack = make (true);
    auto without = make (false);

    const int forceAt = 20;    // blocks: about 107 ms in
    auto force = [] (LuthierEngine& e) { return [&e] (int b) { if (b == forceAt) e.getCharacterEngine().advance (500.0); }; };

    const auto a = render (*withJack, 45, 80, force (*withJack));
    const auto b = render (*without, 45, 80, force (*without));

    // Identical up to the dropout.
    bool same = true;
    for (size_t i = 0; i < (size_t) (forceAt * kBlock); ++i)
        same = same && a[i] == b[i];
    CHECK_MSG (same, "before any dropout the jack must change nothing");

    // The dropout: the first 15 ms after it starts are well down on the reference.
    const size_t start = (size_t) (forceAt * kBlock) + (size_t) (0.006 * kSr);
    const double down = rmsOf (a, start, (size_t) (0.012 * kSr)) / juce::jmax (1.0e-9, rmsOf (b, start, (size_t) (0.012 * kSr)));
    CHECK_MSG (down < 0.4, "the jack should drop the signal out, ratio " + juce::String (down, 3));

    // And back within 100 ms plus the ramp: 150 ms later the level matches.
    const size_t after = (size_t) (forceAt * kBlock) + (size_t) (0.15 * kSr);
    const double back = rmsOf (a, after, (size_t) (0.02 * kSr)) / juce::jmax (1.0e-9, rmsOf (b, after, (size_t) (0.02 * kSr)));
    CHECK_MSG (back > 0.7 && back < 1.3, "the signal should come back after the dropout, ratio " + juce::String (back, 3));
}

//==============================================================================
/*  CW-20: one string's balance in one pickup scales that string only. */
LUTHIER_TEST (CharacterWiring, poleBalanceScalesOneStringInOnePickup)
{
    auto level = [] (double balance, int stringDriven)
    {
        PickupEngine p;
        p.prepare (kSr, 2);
        p.setNumPickups (1);

        auto spec = PickupSpec::makeDefault (PickupType::SingleCoil, 0.2);
        spec.inductanceHenries = 0.0;
        spec.capacitancePf = 0.0;
        p.setPickupSpec (0, spec);
        p.setSelector (PickupSelector::Bridge);
        p.setStringBalance (0, 0, balance);
        p.reset();

        std::vector<double> out (8192);

        for (int i = 0; i < 8192; ++i)
        {
            const double x = std::sin (2.0 * constants::kPi * 330.0 * i / kSr);
            const double ins[2] = { stringDriven == 0 ? x : 0.0, stringDriven == 1 ? x : 0.0 };
            const double delays[2] = { 300.0, 300.0 };
            out[(size_t) i] = p.processStrings (ins, delays, 2);
        }

        return rms (out.data() + 4096, 4096);
    };

    CHECK_NEAR (level (dbToGain (1.5), 0) / level (1.0, 0), dbToGain (1.5), 1.0e-3);
    CHECK_NEAR (level (dbToGain (1.5), 1) / level (1.0, 1), 1.0, 1.0e-9);
}

//==============================================================================
/*  CW-19 / CW-20 / CW-35: with character on, an electric and an acoustic
    render differ from character off; all fresh at amount 0 is bit-identical
    to off. */
LUTHIER_TEST (CharacterWiring, freshIsBitIdenticalAndWornIsNot)
{
    for (auto type : { GuitarType::Stratocaster, GuitarType::Dreadnought })
    {
        auto run = [type] (int mode)   // 0 off, 1 all fresh, 2 all old
        {
            LuthierEngine engine;
            engine.prepare (kSr, kBlock);
            engine.setGuitarType (type);
            quietRig (engine);

            auto& c = engine.getCharacterEngine();
            c.setSeed (0xABCDull);

            if (mode == 0) c.setEnabled (false);
            if (mode == 1) c.setAllFresh();
            if (mode == 2) c.setAllOld();

            engine.reset();
            return render (engine, 52, 40);
        };

        const auto off = run (0);
        const auto fresh = run (1);
        const auto old = run (2);

        CHECK_MSG (off == fresh, "all fresh must be bit-identical to character off");
        CHECK_MSG (off != old, "a worn instrument must sound different");
    }
}

//==============================================================================
/*  CW-22: the saddle height moves fretted intonation, grows with the fret,
    and is nothing open or when fresh. */
LUTHIER_TEST (CharacterWiring, saddleHeightMovesFrettedIntonation)
{
    CharacterEngine c;
    c.prepare (kSr, 6);
    c.setSeed (0x77ull);
    c.setAmount (1.0);

    int moved = 0;

    for (int s = 0; s < 6; ++s)
    {
        const double mm = c.getSaddleHeightOffsetMm (s);
        CHECK (c.getSaddleIntonationCents (s, 0.0) == 0.0);
        CHECK_NEAR (c.getSaddleIntonationCents (s, 12.0), mm * 1.5, 1.0e-9);
        CHECK (std::abs (c.getSaddleIntonationCents (s, 5.0)) <= std::abs (c.getSaddleIntonationCents (s, 12.0)));

        if (std::abs (mm) > 1.0e-3)
            ++moved;
    }

    CHECK (moved > 0);

    c.setAllFresh();
    for (int s = 0; s < 6; ++s)
        CHECK (c.getSaddleIntonationCents (s, 12.0) == 0.0);
}

//==============================================================================
/*  CW-12: a worn fret under the finger buzzes where a fresh one at the same
    action does not. */
LUTHIER_TEST (CharacterWiring, aWornFretBuzzesSooner)
{
    FretBuzz buzz;
    SetupGeometry g;
    g.actionTreble = 1.4;
    g.actionBass = 1.6;
    buzz.setGeometry (g);

    // Find a level just clear of the frets: 0.05 mm short of contact.
    const int s = 0;
    const double fretted = 5.0;
    double lo = 0.0, hi = 4.0;

    for (int i = 0; i < 60; ++i)
    {
        const double mid = 0.5 * (lo + hi);
        (buzz.sense (s, fretted, mid, 0.16).excessMm < -0.05 ? lo : hi) = mid;
    }

    CHECK_MSG (buzz.sense (s, fretted, lo, 0.16).excessMm < 0.0, "the search should land short of contact");

    auto buzzes = [&] (double wear)
    {
        FretBuzz b;
        b.setGeometry (g);
        NoiseEngine pool;
        pool.prepare (kSr);

        std::array<double, 6> levels {}, frets {}, hz {}, wearArr {};
        levels.fill (0.0);
        levels[(size_t) s] = lo;
        frets.fill (fretted);
        hz.fill (110.0);
        wearArr.fill (wear);

        b.process (pool, levels.data(), frets.data(), hz.data(), 6, 0.16, wearArr.data());
        return b.getBuzzingFret (s) >= 0;
    };

    CHECK_MSG (! buzzes (1.0), "a fresh fret at this level should be clear");
    CHECK_MSG (buzzes (2.0), "a worn fret (multiplier 2) should buzz at the same level");

    // And the multiplier is what CharacterEngine reports for a worn fret.
    CharacterEngine c;
    c.prepare (kSr, 6);
    c.setAmount (1.0);
    c.setFretWear (5, 1.0);
    CHECK_NEAR (c.getFretBuzzMultiplier (5.0), 2.5, 1.0e-9);
}

//==============================================================================
/*  CW-3: character ships on, at low intensity (character-wear 0.3). */
LUTHIER_TEST (Character, shipsOnAtLowIntensity)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, 512);

    const auto& c = processor.getEngine().getCharacterEngine();
    CHECK (c.isEnabled());
    CHECK_NEAR (c.getAmount(), 0.25, 1.0e-6);
    CHECK_NEAR (c.getTunerLooseness(), 15.0, 1.0e-9);
    CHECK (! c.isJackIntermittentEnabled());
}

/*  CW-6: "New character" is a new instrument, and it is the one that is saved. */
LUTHIER_TEST (Character, rerollChangesTheSeedAndTheInstrument)
{
    CharacterEngine c;
    c.prepare (kSr, 6);
    c.setSeed (0x1111ull);

    const auto before = c.toVar();
    juce::Array<int> spotsBefore;
    for (int s = 0; s < 6; ++s)
        for (int i = 0; i < c.getNumDeadSpots (s); ++i)
            spotsBefore.add (c.getDeadSpot (s, i).fret * 100 + (int) (c.getDeadSpot (s, i).depth * 90));

    c.reroll();

    CHECK (c.getSeed() != 0x1111ull);
    CHECK (c.toVar()["seed"].toString() != before["seed"].toString());

    juce::Array<int> spotsAfter;
    for (int s = 0; s < 6; ++s)
        for (int i = 0; i < c.getNumDeadSpots (s); ++i)
            spotsAfter.add (c.getDeadSpot (s, i).fret * 100 + (int) (c.getDeadSpot (s, i).depth * 90));

    CHECK_MSG (spotsAfter != spotsBefore, "a reroll should move the dead spots");

    CharacterEngine restored;
    restored.prepare (kSr, 6);
    restored.fromVar (c.toVar());
    CHECK (restored.getSeed() == c.getSeed());
    CHECK (restored.getSaddleBalanceDb (2) == c.getSaddleBalanceDb (2));
}

/*  CW-9: a dead spot costs more near the body's air resonance. */
LUTHIER_TEST (Character, deadSpotsBiteHarderNearTheBodyResonance)
{
    CharacterEngine c;
    c.prepare (kSr, 6);
    c.setAmount (1.0);
    c.setDeadSpot (0, 0, DeadSpot { 7, 0.5, 3.0 });

    const double near = c.getSustainMultiplier (0, 7.0, 110.0, 110.0);
    const double far  = c.getSustainMultiplier (0, 7.0, 880.0, 110.0);

    CHECK_MSG (near < far, "near " + juce::String (near, 4) + " should lose more than far " + juce::String (far, 4));
}

//==============================================================================
/*  CW-24: body break-in reaches the body engine - at age 100 and full
    intensity the air resonance is 3-8 % lower than new. */
LUTHIER_TEST (CharacterWiring, bodyBreakInLowersTheAirMode)
{
    auto airAt = [] (double age)
    {
        LuthierEngine engine;
        engine.prepare (kSr, kBlock);
        engine.setGuitarType (GuitarType::Dreadnought);
        auto& c = engine.getCharacterEngine();
        c.setAmount (1.0);
        c.setBodyAge (age);

        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer none;
        engine.processBlock (buffer, none);
        return engine.getBodyEngine().getAirResonanceHz();
    };

    const double drop = 1.0 - airAt (100.0) / airAt (0.0);
    CHECK_MSG (drop >= 0.03 && drop <= 0.08, "air mode dropped " + juce::String (drop * 100.0, 2) + " %");
}

//==============================================================================
/*  CW-21: nut slot wear shortens an open string and leaves a fretted note
    alone (character-wear 7). */
LUTHIER_TEST (CharacterWiring, nutWearShortensOnlyTheOpenString)
{
    // character-wear 7: a worn nut slot damps the open string and nothing else.
    // Read as the sustain multiplier each note starts with, so the rest of the
    // wear (pickups, saddles, body) cannot stand in for it.
    auto scaleFor = [] (double amount, double fret, int& stringUsed, double& nutDamping)
    {
        LuthierEngine engine;
        engine.prepare (kSr, kBlock);
        engine.setGuitarType (GuitarType::Stratocaster);
        quietRig (engine);

        auto& c = engine.getCharacterEngine();
        c.setSeed (0x2468ull);
        c.setAmount (amount);
        c.setTunerLooseness (0.0);
        c.refret();

        for (int s = 0; s < 6; ++s)
            for (int i = 0; i < CharacterEngine::kMaxDeadSpotsPerString; ++i)
                c.setDeadSpot (s, i, DeadSpot { 7, 0.0, 3.0 });

        int best = 0;
        for (int s = 1; s < 6; ++s)
            if (c.getNutDamping (s) > c.getNutDamping (best))
                best = s;
        stringUsed = best;
        nutDamping = c.getNutDamping (best);

        engine.reset();

        NoteOnEvent e;
        e.stringIndex = best;
        e.velocity = 0.9;
        e.fretPosition = fret;
        e.pitchHz = 110.0 * std::pow (2.0, (best * 5.0 + fret) / 12.0);
        engine.triggerNoteNow (e);

        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer none;
        engine.processBlock (buffer, none);

        return engine.getNoteSustainScale (best);
    };

    int s = 0;
    double wornNut = 0.0, freshNut = 0.0;
    const double wornOpen = scaleFor (1.0, 0.0, s, wornNut);
    const double wornFretted = scaleFor (1.0, 5.0, s, wornNut);
    const double freshOpen = scaleFor (0.0, 0.0, s, freshNut);
    const double freshFretted = scaleFor (0.0, 5.0, s, freshNut);

    CHECK_MSG (wornNut > 0.01, "the seed should wear a nut slot: " + juce::String (wornNut, 4));
    CHECK_MSG (freshNut == 0.0, "a fresh nut is not worn");

    // The open note loses exactly the nut's share; the fretted note loses nothing to it.
    const double wornRatio = wornOpen / juce::jmax (1.0e-12, wornFretted);
    const double freshRatio = freshOpen / juce::jmax (1.0e-12, freshFretted);
    CHECK_MSG (std::abs (wornRatio / freshRatio - (1.0 - wornNut)) < 0.01,
               "open/fretted " + juce::String (wornRatio, 4) + " vs fresh " + juce::String (freshRatio, 4)
               + ", nut damping " + juce::String (wornNut, 4));
}


//==============================================================================
/*  CW-32: character-wear 12 - a dead spot of depth >= 0.5 shortens the note's
    rendered decay (the string's level to -60 dB) by at least 10 % against the
    same note with the spot gone. */
LUTHIER_TEST (Character, deadSpotShortensTheRenderedT60)
{
    auto t60 = [] (double depth)
    {
        LuthierEngine engine;
        engine.prepare (kSr, kBlock);
        engine.setGuitarType (GuitarType::Stratocaster);
        quietRig (engine);

        auto& c = engine.getCharacterEngine();
        c.setSeed (0x1357ull);
        c.setAmount (1.0);
        c.setTunerLooseness (0.0);
        c.refret();

        for (int s = 0; s < 6; ++s)
            for (int i = 0; i < CharacterEngine::kMaxDeadSpotsPerString; ++i)
                c.setDeadSpot (s, i, DeadSpot { 7, 0.0, 3.0 });

        c.setDeadSpot (2, 0, DeadSpot { 7, depth, 2.0 });
        engine.reset();

        NoteOnEvent e;
        e.stringIndex = 2;
        e.velocity = 0.9;
        e.fretPosition = 7.0;
        e.pitchHz = 196.0 * std::pow (2.0, 7.0 / 12.0);
        engine.triggerNoteNow (e);

        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer none;
        double peak = 0.0;

        for (int b = 0; b < (int) (20.0 * kSr / kBlock); ++b)
        {
            buffer.clear();
            engine.processBlock (buffer, none);

            const double level = engine.getStringLevel (2);
            peak = juce::jmax (peak, level);

            if (b > 20 && level < peak * 0.001)
                return b * (double) kBlock / kSr;
        }

        return 20.0;
    };

    const double clean = t60 (0.0);
    const double dead = t60 (0.6);

    CHECK_MSG (dead <= clean * 0.9,
               "a 0.6 dead spot left T60 at " + juce::String (dead, 2) + " s against " + juce::String (clean, 2) + " s");
}

//==============================================================================
/*  CW-2: character-wear 0.2 - the tuner drift moves at block rate and slowly:
    no block-to-block step bigger than a fiftieth of a cent at full looseness
    and intensity, so nothing in it can zipper. */
LUTHIER_TEST (Character, driftChangesOnlyPerBlockAndSlowly)
{
    CharacterEngine c;
    c.prepare (kSr, 6);
    c.setSeed (0x9999ull);
    c.setAmount (1.0);
    c.setTunerLooseness (100.0);

    double worst = 0.0, range = 0.0;
    std::array<double, 6> last {};
    for (int s = 0; s < 6; ++s) last[(size_t) s] = c.getTunerDriftCents (s);

    for (int b = 0; b < (int) (120.0 * kSr / 512.0); ++b)   // two minutes of 512-sample blocks
    {
        c.advance (512.0 / kSr);

        for (int s = 0; s < 6; ++s)
        {
            const double d = c.getTunerDriftCents (s);
            worst = juce::jmax (worst, std::abs (d - last[(size_t) s]));
            range = juce::jmax (range, std::abs (d));
            last[(size_t) s] = d;
        }
    }

    CHECK_MSG (range > 0.5, "the drift did not move: " + juce::String (range, 3) + " cents");
    CHECK_MSG (worst < 0.02, "a block stepped the drift by " + juce::String (worst, 4) + " cents");
}

//==============================================================================
/*  CW-29: the dead-spot map edits a spot's width as well as its depth. */
LUTHIER_TEST (CharacterUi, theWheelSetsADeadSpotsWidth)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, 512);

    auto& c = processor.getEngine().getCharacterEngine();
    c.setDeadSpot (1, 0, DeadSpot { 7, 0.4, 3.0 });

    DeadSpotMap map (processor);
    map.setSize (24 + 22 * 20, DeadSpotMap::preferredHeight);

    const float x = 20.0f + (float) (map.getWidth() - 24) * 7.0f / 22.0f;
    const float y = 1.5f * DeadSpotMap::rowHeight;

    const juce::MouseEvent e (juce::Desktop::getInstance().getMainMouseSource(), { x, y },
                              juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &map, &map,
                              juce::Time::getCurrentTime(), { x, y }, juce::Time::getCurrentTime(), 1, false);

    juce::MouseWheelDetails up {};
    up.deltaY = 0.5f;
    map.mouseWheelMove (e, up);
    map.mouseWheelMove (e, up);
    CHECK_NEAR (c.getDeadSpot (1, 0).width, 3.5, 1.0e-9);

    juce::MouseWheelDetails down {};
    down.deltaY = -0.5f;
    for (int i = 0; i < 10; ++i)
        map.mouseWheelMove (e, down);
    CHECK_NEAR (c.getDeadSpot (1, 0).width, 2.0, 1.0e-9);   // held at the spec's 2-fret floor
    CHECK_NEAR (c.getDeadSpot (1, 0).depth, 0.4, 1.0e-9);   // depth untouched
}
