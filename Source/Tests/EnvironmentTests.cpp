/*  Environment (environment.md 9, ENV-01 to ENV-15; ENV-13 is in
    BodyCouplingTests, beside BC-05, whose measurement it shares).

    The model is tested alone, fed the strains of a real string set from
    StringMaterials, and in the engine for the wiring: the room's offset in the
    tuning, the body's scaling, the buzz geometry, and the legacy loader.
*/

#include "TestFramework.h"

#include "../Character/EnvironmentModel.h"
#include "../Model/Guitar/StringMaterials.h"
#include "../DSP/Noise/FretBuzz.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../PhysicalRange.h"

using namespace luthier;
using namespace luthier::tests;

long luthierAllocationCount() noexcept;   // CircuitTests.cpp

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;
    constexpr double kScale = 648.0;
    constexpr double kOpenHz[6] = { 329.63, 246.94, 196.00, 146.83, 110.00, 82.41 };

    struct Set
    {
        std::array<double, 6> strain {}, diameter {};
    };

    /** A .010 nickel-plated set on a 648 mm scale, as refreshStringPhysics sees it. */
    Set regularSet()
    {
        Set set;

        for (int i = 0; i < 6; ++i)
        {
            const auto s = StringMaterials::computeSpec (StringMaterial::NickelPlatedSteel, StringGauge::Regular,
                                                         StringAge::Fresh, i, kOpenHz[i], kScale);
            const double r = s.coreDiameterMm * 0.0005;
            set.strain[(size_t) i] = s.tensionNewtons / (2.0e11 * constants::kPi * r * r);
            set.diameter[(size_t) i] = s.diameterMm;
        }

        return set;
    }

    void load (EnvironmentModel& env, const Set& set, int n = 6)
    {
        env.prepare (kSr);
        env.setNumStrings (n);

        for (int i = 0; i < n; ++i)
            env.setStringMaterial (i, set.strain[(size_t) (i % 6)], EnvironmentModel::kAlphaSteel, set.diameter[(size_t) (i % 6)]);
    }

    EnvironmentModel::Inputs make (double ambient, double tunedAt = 22.0, double rh = 45.0,
                                   EnvProfile profile = EnvProfile::staticRoom,
                                   EnvClock clock = EnvClock::freeRunning)
    {
        EnvironmentModel::Inputs in;
        in.temperatureC = ambient;
        in.tunedAtC = tunedAt;
        in.humidityPct = rh;
        in.profile = profile;
        in.clock = clock;
        return in;
    }

    /** Advances `seconds` in steps of `step`. */
    void run (EnvironmentModel& env, double seconds, double step = 1.0)
    {
        for (double t = 0.0; t < seconds - 1.0e-9; t += step)
            env.advance (juce::jmin (step, seconds - t), -1.0, false);
    }

    double steady (const Set& set, int i, double dT)
    {
        return 865.6 * (-(EnvironmentModel::kAlphaSteel - EnvironmentModel::kAlphaNeck) * dT) / set.strain[(size_t) i];
    }

    std::unique_ptr<LuthierEngine> makeEngine (GuitarType type = GuitarType::Stratocaster)
    {
        auto e = std::make_unique<LuthierEngine>();
        e->prepare (kSr, kBlock);
        e->setGuitarType (type);
        e->reset();
        return e;
    }

    void renderBlocks (LuthierEngine& e, int blocks, std::vector<double>* out = nullptr)
    {
        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;

        for (int b = 0; b < blocks; ++b)
        {
            buffer.clear();
            e.processBlock (buffer, midi);

            if (out != nullptr)
                for (int i = 0; i < kBlock; ++i)
                    out->push_back (buffer.getSample (0, i));
        }
    }

    void chord (LuthierEngine& e)
    {
        for (int s = 0; s < e.getNumStrings(); ++s)
        {
            NoteOnEvent n;
            n.stringIndex = s;
            n.fretPosition = (s == 0 || s == 5) ? 0.0 : 2.0;
            n.velocity = 0.8;
            n.midiNote = 40 + s;
            n.pitchHz = e.getTuningEngine().computeFrequency (s, n.fretPosition);
            e.triggerNoteNow (n);
        }
    }
}

//==============================================================================
LUTHIER_TEST (Environment, ENV01_theRoomIsANoOp)
{
    // Every delta the model publishes is exactly neutral at the room defaults,
    // block after block, so nothing it touches changes by a single bit: the
    // tuning drift is the character's alone, the body's multipliers are 1 and
    // the buzz geometry is the one asked for.
    auto engine = makeEngine (GuitarType::Dreadnought);
    engine->getCharacterEngine().setEnabled (false);
    chord (*engine);

    std::vector<double> out;

    for (int b = 0; b < (int) (4.0 * kSr / kBlock); ++b)
    {
        renderBlocks (*engine, 1, &out);
        const auto& st = engine->getEnvironment().getState();
        bool neutral = st.plateFreqMul == 1.0 && st.airFreqMul == 1.0 && st.plateQMul == 1.0
                       && st.reliefDeltaMm == 0.0 && st.actionDeltaMm == 0.0 && st.corrosionRate == 1.0;

        for (int s = 0; s < 6; ++s)
            neutral = neutral && st.openCents[(size_t) s] == 0.0
                      && engine->getEnvironment().fretCents (s, 7.3) == 0.0
                      && engine->getTuningEngine().getStringTuning (s).characterDriftCents == 0.0;

        neutral = neutral && engine->getBodyEngine().getRuntimePlateScale() == 1.0
                          && engine->getBodyEngine().getRuntimeAirScale() == 1.0;

        CHECK_MSG (neutral, "block " + juce::String (b) + " is not neutral");

        if (! neutral)
            break;
    }

    // The same render on the other clock is the same audio.
    auto other = makeEngine (GuitarType::Dreadnought);
    other->getCharacterEngine().setEnabled (false);
    auto in = make (22.0);
    in.clock = EnvClock::hostTimeline;
    other->getEnvironment().setInputs (in);
    other->setHostTimeSeconds (100.0, true);
    chord (*other);

    std::vector<double> again;
    renderBlocks (*other, (int) (4.0 * kSr / kBlock), &again);
    CHECK (out == again);
}

LUTHIER_TEST (Environment, ENV02_steadySlopeFromTheStringsOwnNumbers)
{
    const auto set = regularSet();
    EnvironmentModel env;
    load (env, set);
    env.setInputs (make (32.0, 22.0));
    env.reset();
    run (env, 2.0 * 3600.0, 10.0);

    for (int i = 0; i < 6; ++i)
    {
        const double expected = steady (set, i, 10.0);
        CHECK_NEAR (env.getState().openCents[(size_t) i] / expected, 1.0, 1.0e-6);
    }

    const double highE = env.getState().openCents[0], lowE = env.getState().openCents[5];
    CHECK_MSG (highE >= -13.0 && highE <= -7.0, "high E " + juce::String (highE, 2) + " cents");
    CHECK_MSG (lowE >= -48.0 && lowE <= -32.0, "low E " + juce::String (lowE, 2) + " cents");
}

LUTHIER_TEST (Environment, ENV03_coldCaseOvershootsThenSettles)
{
    const auto set = regularSet();
    EnvironmentModel env;
    load (env, set);
    env.setInputs (make (22.0, 22.0, 45.0, EnvProfile::coldCase));
    env.reset();

    CHECK_NEAR (env.getState().openCents[5], 0.0, 1.0e-9);   // tuned on arrival

    double minimum = 0.0, minimumAt = 0.0;

    for (int t = 1; t <= 3600; ++t)
    {
        env.advance (1.0, -1.0, false);
        const double c = env.getState().openCents[5];

        if (c < minimum)
        {
            minimum = c;
            minimumAt = t;
        }
    }

    const double final = steady (set, 5, EnvironmentModel::kColdCaseKelvin);
    CHECK_MSG (minimumAt >= 40.0 && minimumAt <= 300.0, "minimum at " + juce::String (minimumAt) + " s");
    CHECK_MSG (std::abs (minimum) >= 1.3 * std::abs (final),
               "minimum " + juce::String (minimum, 2) + " vs final " + juce::String (final, 2));
    CHECK_NEAR (env.getState().openCents[5] / final, 1.0, 0.05);
}

LUTHIER_TEST (Environment, ENV04_theAirModeFollowsTheSpeedOfSound)
{
    EnvironmentModel env;
    load (env, regularSet());
    env.setChambering (Chambering::acoustic);
    env.setInputs (make (22.0));
    env.reset();
    env.setInputs (make (42.0));
    env.advance (0.01, -1.0, false);

    CHECK_NEAR (env.getState().airFreqMul, 1.033, 0.002);
    CHECK_NEAR (env.getState().plateFreqMul, 1.0, 0.005);   // the body has not warmed yet

    // And the body engine's air resonance - which character dead spots read - follows.
    auto engine = makeEngine (GuitarType::Dreadnought);
    const double before = engine->getBodyEngine().getAirResonanceHz();
    engine->getEnvironment().setInputs (make (42.0));
    renderBlocks (*engine, 1);
    CHECK_NEAR (engine->getBodyEngine().getAirResonanceHz() / before, 1.033, 0.002);
}

LUTHIER_TEST (Environment, ENV05_humidityMovesTheGeometryAndThePlate)
{
    EnvironmentModel env;
    load (env, regularSet());
    env.setChambering (Chambering::acoustic);
    env.setInputs (make (22.0, 22.0, 75.0));
    env.reset();

    const auto& st = env.getState();
    CHECK_NEAR (st.plateFreqMul, 0.952, 0.002);
    CHECK_NEAR (st.reliefDeltaMm, 0.090, 0.001);
    CHECK_NEAR (st.actionDeltaMm, 0.30, 0.01);
    CHECK_NEAR (st.plateQMul, 1.0 / (1.0 + 0.04 * 6.0), 1.0e-9);

    env.setChambering (Chambering::solid);
    env.advance (0.01, -1.0, false);
    CHECK (env.getState().actionDeltaMm == 0.0);
    CHECK_NEAR (env.getState().reliefDeltaMm, 0.090, 0.001);

    // The sorption isotherm's own points.
    CHECK (EnvironmentModel::emc (45.0) == 8.5);
    CHECK_NEAR (EnvironmentModel::emc (50.0), 9.25, 1.0e-12);
}

LUTHIER_TEST (Environment, ENV06_woodIsSlow)
{
    EnvironmentModel env;
    load (env, regularSet());
    env.setInputs (make (22.0, 22.0, 45.0, EnvProfile::humidClub));
    env.reset();
    run (env, 90.0 * 60.0, 5.0);

    const double moved = env.getState().rhAcclimatised - 45.0;
    CHECK_MSG (moved > 0.0 && moved < 1.75, "acclimatised RH moved " + juce::String (moved, 3) + " %");
    CHECK_NEAR (env.getState().ambientRh, 70.0, 0.1);
}

LUTHIER_TEST (Environment, ENV07_frettingStretchFromTheActionDelta)
{
    const auto set = regularSet();
    EnvironmentModel env;
    load (env, set);
    env.setChambering (Chambering::acoustic);

    // dMC = 7 is +0.35 mm of action on an acoustic: EMC 15.5 % sits between 75 and 85 % RH.
    env.setInputs (make (22.0, 22.0, 75.0 + 10.0 / 3.5));
    env.reset();
    CHECK_NEAR (env.getState().actionDeltaMm, 0.35, 1.0e-9);

    SetupGeometry requested, effective;
    requested.numStrings = 6;
    requested.scaleLengthMm = kScale;
    CHECK (env.updateGeometry (requested, effective, true));
    CHECK_NEAR (effective.actionBass - requested.actionBass, 0.35, 1.0e-9);

    const double cents = env.fretCents (5, 12.0);
    const double formula = EnvironmentModel::stretchCents (requested.actionBass, requested.actionBass + 0.35,
                                                           requested.fretPositionMm (12.0), kScale, set.strain[5]);

    CHECK_NEAR (cents, 3.6, 0.6);
    CHECK_NEAR (cents / formula, 1.0, 0.02);
    CHECK (env.fretCents (5, 0.0) == 0.0);

    // In the engine, fret 12 minus the open string carries it.
    auto engine = makeEngine (GuitarType::Dreadnought);
    engine->getEnvironment().setInputs (make (22.0, 22.0, 75.0 + 10.0 / 3.5));
    engine->getEnvironment().reset();
    renderBlocks (*engine, 2);
    CHECK (engine->getEnvironment().fretCents (5, 12.0) > 2.0);
}

LUTHIER_TEST (Environment, ENV08_aDryNeckBuzzesMore)
{
    auto excessAt = [] (double rh)
    {
        EnvironmentModel env;
        load (env, regularSet());
        env.setChambering (Chambering::solid);
        env.setInputs (make (22.0, 22.0, rh));
        env.reset();

        SetupGeometry requested, effective;
        const auto& style = getSetupStyle (1);   // player-friendly
        requested.actionTreble = style.actionTreble;
        requested.actionBass = style.actionBass;
        requested.relief = style.relief;
        requested.numStrings = 6;
        requested.scaleLengthMm = kScale;
        env.updateGeometry (requested, effective, true);

        FretBuzz buzz;
        buzz.setGeometry (effective);

        // A velocity-110 pluck's level, in the string's level-follower units.
        const double level = 1.75 * 110.0 / 127.0;
        return buzz.sense (5, 3.0, level, 0.16).excessMm;
    };

    const double normal = excessAt (45.0), dry = excessAt (25.0);
    // environment.md 9 (amended): the relief loses 0.049 mm at 25 % RH, and
    // fret-buzz.md's parabolic board passes about a quarter of that to the
    // frets just above fret 3.
    CHECK_MSG (dry - normal >= 0.01, "dry neck raised the excess by " + juce::String (dry - normal, 4) + " mm");
}

LUTHIER_TEST (Environment, ENV09_seekingIsDeterministic)
{
    const auto set = regularSet();
    const double barSeconds = 2.0;   // 4/4 at 120 bpm
    const double dt = kBlock / kSr;

    auto blocksFrom = [&] (double startSeconds)
    {
        EnvironmentModel env;
        load (env, set);
        env.setInputs (make (22.0, 22.0, 45.0, EnvProfile::stageLights, EnvClock::hostTimeline));
        env.reset();

        std::vector<std::array<double, 6>> offsets;

        for (double t = startSeconds; t < 17.0 * barSeconds; t += dt)
        {
            env.advance (dt, t, true);

            if (t >= 8.0 * barSeconds)
            {
                std::array<double, 6> o {};

                for (int s = 0; s < 6; ++s)
                    o[(size_t) s] = env.getState().openCents[(size_t) s];

                offsets.push_back (o);
            }
        }

        return offsets;
    };

    // The loops step t identically, so the two sample the same instants.
    const auto whole = blocksFrom (0.0);
    auto seeked = blocksFrom (0.0);
    seeked.clear();

    {
        EnvironmentModel env;
        load (env, set);
        env.setInputs (make (22.0, 22.0, 45.0, EnvProfile::stageLights, EnvClock::hostTimeline));
        env.reset();
        env.advance (dt, 3.0, true);   // somewhere else first

        for (double t = 0.0; t < 17.0 * barSeconds; t += dt)
        {
            if (t < 8.0 * barSeconds)
                continue;

            env.advance (dt, t, true);
            std::array<double, 6> o {};

            for (int s = 0; s < 6; ++s)
                o[(size_t) s] = env.getState().openCents[(size_t) s];

            seeked.push_back (o);
        }
    }

    CHECK (whole.size() == seeked.size() && ! whole.empty());
    double worst = 0.0;

    for (size_t i = 0; i < juce::jmin (whole.size(), seeked.size()); ++i)
        for (int s = 0; s < 6; ++s)
            worst = juce::jmax (worst, std::abs (whole[i][(size_t) s] - seeked[i][(size_t) s]));

    CHECK_MSG (worst <= 1.0e-9, "seek differs by " + juce::String (worst, 12) + " cents");
    CHECK (std::abs (whole.back()[5]) > 0.1);   // the profile really is moving
}

LUTHIER_TEST (Environment, ENV10_retuneZeroesThenDriftsAgain)
{
    EnvironmentModel env;
    load (env, regularSet());
    env.setInputs (make (22.0, 22.0, 45.0, EnvProfile::stageLights));
    env.reset();
    run (env, 600.0);

    CHECK (std::abs (env.getState().openCents[5]) > 1.0);

    env.requestRetune (22.0);
    env.advance (0.01, -1.0, false);

    for (int s = 0; s < 6; ++s)
        CHECK_NEAR (env.getState().openCents[(size_t) s], 0.0, 0.01);

    run (env, 300.0);
    CHECK (std::abs (env.getState().openCents[5]) > 0.1);
}

LUTHIER_TEST (Environment, ENV11_legacyTemperatureAndHumidityMigrate)
{
    auto stateWith = [] (int temperature, int humidity)
    {
        LuthierAudioProcessor processor;
        processor.prepareToPlay (kSr, kBlock);

        juce::MemoryBlock block;
        processor.getStateInformation (block);
        auto root = juce::JSON::parse (block.toString());

        // SPEC-SWEEP (SM-1): the session keeps character inside its preset block;
        // a session saved before that had it at the top level, which is the
        // legacy shape this test builds.
        auto* rootObject = root.getDynamicObject();
        auto characterVar = rootObject->getProperty ("character");

        if (auto* preset = rootObject->getProperty ("preset").getDynamicObject();
            preset != nullptr && preset->hasProperty ("character"))
        {
            characterVar = preset->getProperty ("character");
            preset->removeProperty ("character");
            rootObject->setProperty ("character", characterVar);
        }

        auto* character = characterVar.getDynamicObject();

        character->setProperty ("enabled", true);
        character->setProperty ("amount", 0.25);
        character->setProperty ("temperature", temperature);
        character->setProperty ("humidity", humidity);
        character->removeProperty ("environment");

        return juce::JSON::toString (root);
    };

    auto loadAndRun = [] (const juce::String& json, LuthierAudioProcessor& processor)
    {
        processor.prepareToPlay (kSr, kBlock);
        processor.setStateInformation (json.toRawUTF8(), (int) json.getNumBytesAsUTF8());

        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;

        for (int b = 0; b < 4; ++b)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
    };

    {
        LuthierAudioProcessor processor;
        loadAndRun (stateWith ((int) Temperature::cold, (int) Humidity::normal), processor);

        const auto& env = processor.getEngine().getEnvironment();
        double mean = 0.0;
        const int n = processor.getEngine().getNumStrings();

        for (int s = 0; s < n; ++s)
            mean += env.getState().openCents[(size_t) s] / n;

        CHECK_NEAR (mean, 0.625, 0.05);
    }

    // Humid loads as 45 % and renders as normal does.
    auto render = [&] (int humidity)
    {
        LuthierAudioProcessor processor;
        loadAndRun (stateWith ((int) Temperature::room, humidity), processor);

        auto* rh = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (ParamIDs::envHumidityPct));
        CHECK (rh != nullptr && std::abs (rh->convertFrom0to1 (rh->getValue()) - 45.0f) < 1.0e-3f);

        juce::AudioBuffer<float> buffer (2, kBlock);
        std::vector<float> out;

        for (int b = 0; b < 40; ++b)
        {
            juce::MidiBuffer midi;

            if (b == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 0);

            buffer.clear();
            processor.processBlock (buffer, midi);

            for (int i = 0; i < kBlock; ++i)
                out.push_back (buffer.getSample (0, i));
        }

        return out;
    };

    CHECK (render ((int) Humidity::humid) == render ((int) Humidity::normal));
}

LUTHIER_TEST (Environment, ENV12_aTemperatureStepIsASlew)
{
    // A step reaches the string only through its wire's time constant. The
    // fastest string is the plain high E (5 s): 18 K at most moves it about
    // 5.1 cents a second, 0.055 cents in a 512-sample block at 48 kHz - so the
    // spec's bound is 0.1 cents per block (environment.md 9, amended).
    EnvironmentModel env;
    load (env, regularSet());
    env.setInputs (make (22.0));
    env.reset();
    env.advance (kBlock / kSr, -1.0, false);

    std::array<double, 6> last {};

    for (int s = 0; s < 6; ++s)
        last[(size_t) s] = env.getState().openCents[(size_t) s];

    env.setInputs (make (40.0));
    double worst = 0.0;

    for (int b = 0; b < (int) (60.0 * kSr / kBlock); ++b)
    {
        env.advance (kBlock / kSr, -1.0, false);

        for (int s = 0; s < 6; ++s)
        {
            worst = juce::jmax (worst, std::abs (env.getState().openCents[(size_t) s] - last[(size_t) s]));
            last[(size_t) s] = env.getState().openCents[(size_t) s];
        }
    }

    CHECK_MSG (worst <= 0.1, "a block moved a string " + juce::String (worst, 4) + " cents");
    CHECK (worst > 0.0);
}

LUTHIER_TEST (Environment, ENV14_independentOfCharacter)
{
    auto engine = makeEngine();
    engine->getCharacterEngine().setAmount (0.0);
    engine->getCharacterEngine().setEnabled (false);
    engine->getEnvironment().setInputs (make (32.0, 22.0));
    engine->getEnvironment().reset();
    renderBlocks (*engine, 2);

    const auto& env = engine->getEnvironment().getState();

    for (int s = 0; s < 6; ++s)
    {
        const double inv = env.invStrain[(size_t) s];
        const double expected = 865.6 * (-(EnvironmentModel::kAlphaSteel - EnvironmentModel::kAlphaNeck) * 10.0) * inv;
        CHECK_NEAR (env.openCents[(size_t) s] / expected, 1.0, 1.0e-6);
        CHECK_NEAR (engine->getTuningEngine().getStringTuning (s).characterDriftCents, env.openCents[(size_t) s], 1.0e-12);
    }
}

LUTHIER_TEST (Environment, ENV15_budgetSafetyAndCorners)
{
    const auto set = regularSet();
    EnvironmentModel env;
    load (env, set, 12);
    env.setChambering (Chambering::acoustic);
    SetupGeometry requested, effective;
    requested.numStrings = 12;

    constexpr int blocks = 20000;
    double best = 1.0e9;

    for (int run = 0; run < 3; ++run)
    {
        const long allocations = luthierAllocationCount();
        const auto start = juce::Time::getHighResolutionTicks();

        for (int b = 0; b < blocks; ++b)
        {
            const double lfo = std::sin (constants::kTwoPi * b * kBlock / kSr);
            env.setInputs (make (22.0 + 10.0 * lfo, 22.0 + 5.0 * lfo, 45.0 + 20.0 * lfo,
                                 EnvProfile::stageLights, EnvClock::freeRunning));
            env.advance (kBlock / kSr, -1.0, false);
            env.updateGeometry (requested, effective, false);
        }

        best = juce::jmin (best, juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start));
        CHECK (luthierAllocationCount() == allocations);
    }

    const double units = 100.0 * best / (blocks * kBlock / kSr);
    CHECK_MSG (units <= 0.02, "EnvironmentModel costs " + juce::String (units, 4) + " units (budget 0.02)");

    // 1000 random corners of the advanced ranges, 5 s each: finite everywhere.
    juce::Random random (0xE7u);
    bool finite = true;

    for (int corner = 0; corner < 1000 && finite; ++corner)
    {
        auto pick = [&random] (double lo, double hi) { return random.nextBool() ? lo : hi; };
        env.setInputs (make (pick (-30.0, 70.0), pick (-30.0, 70.0), pick (5.0, 100.0),
                             (EnvProfile) random.nextInt ((int) EnvProfile::numProfiles), EnvClock::freeRunning));
        env.reset();
        run (env, 5.0, kBlock / kSr * 20.0);
        env.updateGeometry (requested, effective, true);

        const auto& st = env.getState();

        for (int s = 0; s < 12; ++s)
            finite = finite && std::isfinite (st.openCents[(size_t) s]) && std::abs (st.openCents[(size_t) s]) <= 300.0
                     && std::isfinite (env.fretCents (s, 11.5));

        finite = finite && std::isfinite (st.plateFreqMul) && std::isfinite (st.airFreqMul)
                        && std::isfinite (st.plateQMul) && st.plateFreqMul >= 0.7 && st.plateFreqMul <= 1.3
                        && st.plateQMul >= 0.5 && st.plateQMul <= 2.0;
    }

    CHECK (finite);

    // The family.
    for (const char* id : { ParamIDs::envTemperatureC, ParamIDs::envTunedAtC, ParamIDs::envHumidityPct })
    {
        const auto* r = RangeRegistry::find (id);
        CHECK (r != nullptr && r->isValid() && r->family == RangeFamily::environment);
    }
}
