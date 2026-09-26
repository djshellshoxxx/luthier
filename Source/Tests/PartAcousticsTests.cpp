/*  From parts to engine numbers (part-acoustics.md 11). */

#include "TestFramework.h"

#include "../Model/Workshop/PartAcoustics.h"
#include "../LuthierEngine.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    PartLibrary& library()
    {
        static PartLibrary lib = []
        {
            PartLibrary l;
            l.refreshFrom (PartLibrary::getFactoryPartsFolder(),
                           juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-no-user-parts"));
            return l;
        }();

        return lib;
    }

    WorkshopGuitar factory (const juce::String& relativePath)
    {
        WorkshopGuitar g;
        PartLibrary::LoadReport report;
        library().loadGuitar (PartLibrary::getFactoryGuitarsFolder().getChildFile (relativePath), g, report);
        return g;
    }

    /** A copy of a part with one field replaced. */
    PartPtr withField (const PartPtr& part, const char* field, const juce::var& value)
    {
        auto copy = std::make_shared<Part> (*part);
        auto fields = juce::JSON::parse (juce::JSON::toString (part->fields));
        fields.getDynamicObject()->setProperty (field, value);
        copy->fields = fields;
        return copy;
    }

    double renderDecaySeconds (const DerivedAcoustics& d)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.applyWorkshopGuitar (d);

        NoteOnEvent e;
        e.stringIndex = 1;
        e.velocity = 0.9;
        e.pitchHz = 246.9;
        engine.triggerNoteNow (e);

        juce::AudioBuffer<float> block (2, 256);
        double peak = 0.0;

        for (int b = 0; b < (int) (10.0 * 48000.0 / 256.0); ++b)
        {
            block.clear();
            juce::MidiBuffer none;
            engine.processBlock (block, none);

            const double level = engine.getStringLevel (1);
            peak = juce::jmax (peak, level);

            if (b > 10 && level < peak * 0.001)
                return b * 256.0 / 48000.0;
        }

        return 10.0;
    }
}

//==============================================================================
LUTHIER_TEST (PartAcoustics, scaleLengthSetsTension)
{
    // 11: 628 mm against 648 mm, same pitch and gauge: (648/628)^2 within 1%.
    auto les = factory ("Electric/Vintage Double-Cut.luthierguitar");
    auto shorter = les;
    shorter.parts[(size_t) GuitarSlot::neck] = withField (les.get (GuitarSlot::neck), "scale_length_mm", 628.0);

    const auto a = mapSpec (les), b = mapSpec (shorter);
    const double expected = std::pow (648.0 / 628.0, 2.0);

    for (int s = 0; s < 6; ++s)
        CHECK_MSG (std::abs (a.tensionNewtons[(size_t) s] / b.tensionNewtons[(size_t) s] - expected) / expected < 0.01,
                   "string " + juce::String (s + 1) + ": tension ratio "
                     + juce::String (a.tensionNewtons[(size_t) s] / b.tensionNewtons[(size_t) s], 4));
}

LUTHIER_TEST (PartAcoustics, magnetPullShortensSustainAndPullsFlat)
{
    auto base = factory ("Electric/Vintage Double-Cut.luthierguitar");

    auto withPickups = [&base] (const char* magnet, double heightMm)
    {
        auto g = base;

        for (int i = 0; i < 3; ++i)
        {
            const auto slot = WorkshopGuitar::pickupSlot (i);
            g.parts[(size_t) slot] = withField (base.get (slot), "magnet", magnet);
            g.placements[(size_t) i].heightTrebleMm = g.placements[(size_t) i].heightBassMm = heightMm;
        }

        return mapSpec (g);
    };

    const auto strong = withPickups ("ceramic", 1.5);
    const auto weak = withPickups ("alnico3", 3.5);

    const double strongDecay = renderDecaySeconds (strong), weakDecay = renderDecaySeconds (weak);

    CHECK_MSG (strongDecay <= weakDecay * 0.85,
               "ceramic at 1.5 mm decays in " + juce::String (strongDecay, 2) + " s, alnico 3 at 3.5 mm in "
                 + juce::String (weakDecay, 2) + " s - not 15% shorter");

    LuthierEngine engine;
    engine.prepare (48000.0, 256);
    engine.applyWorkshopGuitar (strong);
    CHECK_MSG (engine.getMagnetDetuneCents() < -0.5, "no measurable flat pull from a close ceramic magnet");
}

LUTHIER_TEST (PartAcoustics, aCoverCostsTopEnd)
{
    auto levelAt4k = [] (double coverLoss)
    {
        PickupEngine p;
        p.prepare (48000.0, 1);
        p.setNumPickups (1);

        auto spec = PickupSpec::makeDefault (PickupType::Humbucker, 0.1);
        spec.coverLossDbAt4k = coverLoss;
        p.setPickupSpec (0, spec);
        p.setSelector (PickupSelector::Bridge);

        double power = 0.0;
        const double delays[1] = { 200.0 };

        for (int i = 0; i < 48000; ++i)
        {
            const double in[1] = { std::sin (constants::kTwoPi * 4000.0 * i / 48000.0) };
            const double out = p.processStrings (in, delays, 1);

            if (i > 24000)
                power += out * out;
        }

        return power;
    };

    const double lossDb = 10.0 * std::log10 (levelAt4k (-0.8) / levelAt4k (0.0));
    CHECK_MSG (std::abs (lossDb + 0.8) <= 0.2, "a nickel cover costs " + juce::String (lossDb, 2) + " dB at 4 kHz");
}

LUTHIER_TEST (PartAcoustics, chamberingPutsTheAirModeInItsRange)
{
    auto base = factory ("Electric/Vintage Double-Cut.luthierguitar");

    struct Row { const char* chambering; double lo, hi; };

    for (const auto& row : { Row { "chambered", 180, 240 }, Row { "semi_hollow", 140, 190 },
                             Row { "hollow", 90, 140 }, Row { "acoustic", 90, 110 } })
    {
        auto g = base;
        g.parts[(size_t) GuitarSlot::body] = withField (base.get (GuitarSlot::body), "chambering", row.chambering);

        const double hz = mapSpec (g).airResonanceHz;
        CHECK_MSG (hz >= row.lo && hz <= row.hi,
                   juce::String (row.chambering) + ": air mode at " + juce::String (hz, 1) + " Hz");
    }

    CHECK (mapSpec (base).airResonanceHz == 0.0);   // solid
}

LUTHIER_TEST (PartAcoustics, hardwareColourIsSilent)
{
    auto a = factory ("Electric/Vintage Single-Cut.luthierguitar");
    auto b = a;
    b.hardwareColour = "gold";

    CHECK_MSG (mapSpec (a) == mapSpec (b), "hardware colour changed a derived acoustic value");
}

LUTHIER_TEST (PartAcoustics, couplingsMultiply)
{
    auto base = factory ("Electric/Classic T-Style.luthierguitar");
    const double full = mapSpec (base).couplingFraction;

    auto halfBridge = base;
    halfBridge.parts[(size_t) GuitarSlot::bridge] =
        withField (base.get (GuitarSlot::bridge), "coupling", base.get (GuitarSlot::bridge)->number ("coupling", 0.7) * 0.5);

    const double half = mapSpec (halfBridge).couplingFraction;
    CHECK (std::abs (half / full - 0.5) < 1.0e-9);

    // A through-neck couples 0.95 where a bolt-on couples 0.55: the result is
    // the product of both parts, never either one alone.
    auto doubleHalf = halfBridge;
    doubleHalf.parts[(size_t) GuitarSlot::neck] = withField (base.get (GuitarSlot::neck), "joint", "through");
    CHECK (std::abs (mapSpec (doubleHalf).couplingFraction - 0.95 * base.get (GuitarSlot::bridge)->number ("coupling", 0.7) * 0.5) < 1.0e-9);
}

LUTHIER_TEST (PartAcoustics, theMappingIsMonotonic)
{
    auto base = factory ("Electric/Vintage Double-Cut.luthierguitar");

    auto sweep = [&base] (GuitarSlot slot, const char* field, double from, double to,
                          std::function<double (const DerivedAcoustics&)> read, bool rising)
    {
        double previous = rising ? -1.0e300 : 1.0e300;
        bool ok = true;

        for (int step = 0; step <= 10; ++step)
        {
            auto g = base;
            g.parts[(size_t) slot] = withField (base.get (slot), field, from + (to - from) * step / 10.0);

            const double now = read (mapSpec (g));
            ok = ok && (rising ? now >= previous : now <= previous);
            previous = now;
        }

        return ok;
    };

    CHECK_MSG (sweep (GuitarSlot::pickupBridge, "inductance_h", 1.0, 8.0,
                      [] (const DerivedAcoustics& d) { return d.pickups[0].spec.inductanceHenries; }, true),
               "inductance");
    CHECK_MSG (sweep (GuitarSlot::bridge, "mass_g", 30.0, 480.0,
                      [] (const DerivedAcoustics& d) { return d.sustainScale; }, true),
               "bridge mass");
    CHECK_MSG (sweep (GuitarSlot::body, "thickness_mm", 30.0, 60.0,
                      [] (const DerivedAcoustics& d) { return d.body.scaleDepth; }, true),
               "body thickness");
    CHECK_MSG (sweep (GuitarSlot::body, "density_kg_m3", 350.0, 800.0,
                      [] (const DerivedAcoustics& d) { return d.body.resonanceTrim; }, false),
               "body density");

    // Pickup position: placement, not a part field.
    double previous = -1.0;
    bool ok = true;

    for (double mm = 30.0; mm <= 170.0; mm += 14.0)
    {
        auto g = base;
        g.placements[2].positionMm = mm;
        const double now = mapSpec (g).pickups[0].spec.position;
        ok = ok && now >= previous;
        previous = now;
    }

    CHECK_MSG (ok, "pickup position");
}

LUTHIER_TEST (PartAcoustics, pickupPositionSetsTheComb)
{
    // 6.1: the first null for a pickup at 38 mm, from v / (2 x position).
    auto g = factory ("Electric/Vintage Double-Cut.luthierguitar");
    g.placements[2].positionMm = 38.0;

    const auto d = mapSpec (g);
    const double open = 82.41;
    const double expected = (2.0 * 0.648 * open) / (2.0 * 0.038);

    CHECK (std::abs (firstCombNullHz (38.0, 648.0, open) - expected) / expected < 0.05);

    // And the engine's pickup is placed there: the comb it computes from its
    // position fraction nulls at the same frequency.
    const double fromFraction = open / d.pickups[0].spec.position;
    CHECK_MSG (std::abs (fromFraction - expected) / expected < 0.05,
               "the engine's pickup nulls at " + juce::String (fromFraction, 0) + " Hz, not "
                 + juce::String (expected, 0));
}

LUTHIER_TEST (PartAcoustics, theMappingIsDeterministic)
{
    for (const auto& path : { "Electric/Vintage Single-Cut.luthierguitar", "Acoustic/Dreadnought.luthierguitar",
                              "Bass/J-Style Bass.luthierguitar" })
    {
        const auto g = factory (path);
        CHECK_MSG (mapSpec (g) == mapSpec (g), juce::String (path) + " mapped differently twice");
    }
}

LUTHIER_TEST (PartAcoustics, everyMappedFieldMovesSomething)
{
    // 11, for the fields this build maps. Fields a later spec consumes
    // (tuners, nut friction, fretboard radius: tuning-stability.md and the
    // board-radius clearance) are listed in DECISIONS.md rather than faked.
    struct Probe { GuitarSlot slot; const char* field; double scale; };

    const Probe probes[] =
    {
        { GuitarSlot::body, "density_kg_m3", 1.1 }, { GuitarSlot::body, "thickness_mm", 1.1 },
        { GuitarSlot::body, "area_cm2", 1.1 },
        { GuitarSlot::neck, "scale_length_mm", 1.1 },
        { GuitarSlot::frets, "height_mm", 1.1 }, { GuitarSlot::frets, "width_mm", 1.1 },
        { GuitarSlot::bridge, "mass_g", 1.1 }, { GuitarSlot::bridge, "coupling", 1.1 },
        { GuitarSlot::pickupBridge, "inductance_h", 1.1 }, { GuitarSlot::pickupBridge, "dc_resistance_k", 1.1 },
        { GuitarSlot::pickupBridge, "capacitance_pf", 1.1 }, { GuitarSlot::pickupBridge, "output_dbfs_reference", 1.1 },
        { GuitarSlot::wiring, "volume_pot_ohm", 1.1 }, { GuitarSlot::wiring, "tone_pot_ohm", 1.1 },
        { GuitarSlot::wiring, "tone_cap_f", 1.1 },
        { GuitarSlot::pickguard, "mass_g", 1.1 },
    };

    auto base = factory ("Electric/Vintage Double-Cut.luthierguitar");
    const auto reference = mapSpec (base);

    for (const auto& probe : probes)
    {
        auto g = base;
        const auto& part = base.get (probe.slot);

        if (part == nullptr)
            continue;

        g.parts[(size_t) probe.slot] = withField (part, probe.field, part->number (probe.field, 1.0) * probe.scale);

        CHECK_MSG (! (mapSpec (g) == reference),
                   juce::String (getPartTypeId (part->type)) + "." + probe.field + " moved nothing");
    }
}

LUTHIER_TEST (PartAcoustics, aReferenceGuitarSoundsLikeTheEngineDefault)
{
    // The mapping's neutral points are the reference parts: a factory guitar
    // built from nickel-silver frets and a bone nut leaves the termination
    // brightness where a compiled guitar has it.
    const auto d = mapSpec (factory ("Electric/Vintage Double-Cut.luthierguitar"));
    CHECK (std::abs (d.nutBrightness - 0.75) < 1.0e-9);
    CHECK (std::abs (d.fretBrightness / 0.70 - 1.0) < 0.1);
}

//==============================================================================
/*  SPEC-SWEEP: PA-14 / PA-15 / PA-T7 - chambering's air mode, its Q and its
    gain used to be computed and dropped. The body engine now builds its air
    mode where the mapping put it, and the electric chamberings' modes are
    louder than solid by the table's gain. */
LUTHIER_TEST (PartAcoustics, chamberingReachesTheBodyEngine)
{
    auto base = factory ("Electric/Vintage Double-Cut.luthierguitar");

    auto modesFor = [] (const DerivedAcoustics& d)
    {
        std::vector<BodyMode> modes;
        BodyModels::buildModes (d.body, modes);
        return modes;
    };

    double gainSum[2] = { 0.0, 0.0 };

    for (const char* chambering : { "chambered", "semi_hollow", "hollow", "acoustic" })
    {
        auto g = base;
        g.parts[(size_t) GuitarSlot::body] = withField (base.get (GuitarSlot::body), "chambering", chambering);
        const auto d = mapSpec (g);

        // The rendered body's air mode sits where the mapping put it.
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.applyWorkshopGuitar (d);
        engine.getCharacterEngine().setEnabled (false);   // body break-in (CW-24) would move it

        juce::AudioBuffer<float> block (2, 256);
        juce::MidiBuffer none;
        engine.processBlock (block, none);

        CHECK_NEAR (engine.getBodyEngine().getAirResonanceHz(), d.airResonanceHz, 1.0e-6);

        const auto modes = modesFor (d);
        bool found = false;

        for (const auto& m : modes)
            if (std::abs (m.frequencyHz - d.airResonanceHz) < 0.01 && std::abs (m.q - d.airResonanceQ * (1.0 + d.body.age * 0.55)) < 0.01)
                found = true;

        CHECK_MSG (found, juce::String (chambering) + ": no body mode at the mapped air resonance "
                            + juce::String (d.airResonanceHz, 1) + " Hz, Q " + juce::String (d.airResonanceQ, 1));

        if (juce::String (chambering) == "semi_hollow")
            for (const auto& m : modes)
                gainSum[1] += m.gain;
    }

    for (const auto& m : modesFor (mapSpec (base)))
        gainSum[0] += m.gain;

    // 2.1: semi-hollow's modes are +6 dB against solid.
    CHECK_NEAR (gainToDb (gainSum[1] / gainSum[0]), 6.0, 0.01);
}

/*  SPEC-SWEEP: PA-56 - a full gloss on an acoustic top costs its modes about
    half a dB and lowers their Q by 8 %; a half gloss costs nothing. */
LUTHIER_TEST (PartAcoustics, aThickFinishDampsTheTop)
{
    auto base = factory ("Acoustic/Dreadnought.luthierguitar");

    auto topPeak = [&base] (double gloss)
    {
        auto g = base;
        g.finish.gloss = gloss;
        const auto d = mapSpec (g);

        std::vector<BodyMode> modes;
        BodyModels::buildModes (d.body, modes);

        // The top's fundamental: the largest mode above the air pair.
        const double top = BodyModels::computeTopFundamental (d.body);
        BodyMode best;
        double nearest = 1.0e9;

        for (const auto& m : modes)
            if (std::abs (m.frequencyHz - top) < nearest)
            {
                nearest = std::abs (m.frequencyHz - top);
                best = m;
            }

        return best;
    };

    const auto matte = topPeak (0.5);
    const auto gloss = topPeak (1.0);

    CHECK_NEAR (gainToDb (gloss.gain / matte.gain), -0.5, 0.01);
    CHECK_NEAR (gloss.q / matte.q, 0.92, 0.005);
}

//==============================================================================
/*  SPEC-SWEEP: PA-40 - twice the turns is +6 dB of output. */
LUTHIER_TEST (PartAcoustics, coilTurnsSetTheOutput)
{
    auto base = factory ("Electric/Vintage Double-Cut.luthierguitar");
    const auto pickup = base.get (GuitarSlot::pickupBridge);
    CHECK (pickup != nullptr);

    auto with = [&] (double turns)
    {
        auto g = base;
        g.parts[(size_t) GuitarSlot::pickupBridge] = withField (pickup, "coil_turns", turns);
        return mapSpec (g).pickups[0].spec.outputTrimDb;
    };

    CHECK_NEAR (with (16000.0) - with (8000.0), 20.0 * std::log10 (2.0), 1.0e-6);
}

/*  SPEC-SWEEP: PA-35 - a bridge with a saddle piezo makes the guitar a piezo
    source without using a pickup slot. */
LUTHIER_TEST (PartAcoustics, aPiezoBridgeAddsAPiezoSource)
{
    auto base = factory ("Electric/Vintage Double-Cut.luthierguitar");
    CHECK (! mapSpec (base).hasPiezo);

    auto g = base;
    g.parts[(size_t) GuitarSlot::bridge] = withField (base.get (GuitarSlot::bridge), "piezo", true);
    const auto d = mapSpec (g);

    CHECK (d.hasPiezo && d.spec.hasPiezo);
    CHECK (d.numPickups == mapSpec (base).numPickups);
}

//==============================================================================
//  SPEC-SWEEP: part-acoustics NO-TEST rows (PA-6, 13, 16, 18, 22, 24, 27, 28,
//  29, 34, 36/58, 37, 48, 57).
//==============================================================================
namespace
{
    WorkshopGuitar withPart (const WorkshopGuitar& g, GuitarSlot slot, const char* field, const juce::var& value)
    {
        auto copy = g;
        copy.parts[(size_t) slot] = withField (g.get (slot), field, value);
        return copy;
    }
}

LUTHIER_TEST (PartAcoustics, theWoodTableIsTheSpecs)
{
    struct Row { const char* id; double rho, e, tan; };

    const Row rows[] =
    {
        { "alder", 420, 9.5, 8.5 },         { "ash_swamp", 480, 11.0, 7.5 },   { "ash_northern", 680, 13.0, 6.5 },
        { "basswood", 420, 9.0, 11.0 },     { "mahogany", 550, 10.5, 9.0 },    { "mahogany_african", 530, 9.8, 9.5 },
        { "maple_hard", 705, 12.6, 6.0 },   { "maple_soft", 545, 10.0, 7.5 },  { "korina", 480, 10.0, 8.5 },
        { "poplar", 455, 10.9, 10.0 },      { "walnut", 610, 11.5, 7.0 },      { "rosewood", 830, 12.0, 6.0 },
        { "ebony", 1040, 16.0, 4.5 },       { "pau_ferro", 860, 13.5, 5.5 },   { "spruce", 400, 11.0, 7.0 },
        { "cedar", 350, 8.0, 9.0 },         { "koa", 610, 10.5, 8.0 },
    };

    for (const auto& r : rows)
    {
        WoodData w {};
        CHECK_MSG (lookUpWood (r.id, w), juce::String ("no wood ") + r.id);
        CHECK_NEAR (w.densityKgM3, r.rho, 1.0e-9);
        CHECK_NEAR (w.youngsGPa, r.e, 1.0e-9);
        CHECK_NEAR (w.lossTangent, r.tan * 1.0e-3, 1.0e-12);
    }
}

LUTHIER_TEST (PartAcoustics, chamberingPicksItsShapeAndSustain)
{
    auto base = factory ("Electric/Vintage Double-Cut.luthierguitar");

    auto mapped = [&base] (const char* c) { return mapSpec (withPart (base, GuitarSlot::body, "chambering", c)); };

    const auto solid = mapped ("solid");
    CHECK (mapped ("chambered").body.shape == BodyShape::Chambered);
    CHECK (mapped ("semi_hollow").body.shape == BodyShape::SemiHollow);
    CHECK (mapped ("hollow").body.shape == BodyShape::Hollow);

    // PA-13: more cavity, more modes (never fewer).
    int last = 0;
    for (const char* c : { "solid", "chambered", "semi_hollow", "hollow" })
    {
        std::vector<BodyMode> modes;
        BodyModels::buildModes (mapped (c).body, modes);
        CHECK_MSG ((int) modes.size() >= last, juce::String (c) + " has fewer modes than the step before");
        last = (int) modes.size();
    }

    // PA-16: -5 / -12 / -20 % sustain against solid.
    CHECK_NEAR (mapped ("chambered").sustainScale / solid.sustainScale, 0.95, 1.0e-9);
    CHECK_NEAR (mapped ("semi_hollow").sustainScale / solid.sustainScale, 0.88, 1.0e-9);
    CHECK_NEAR (mapped ("hollow").sustainScale / solid.sustainScale, 0.80, 1.0e-9);
}

LUTHIER_TEST (PartAcoustics, bracingSplitsTheAcousticModes)
{
    auto base = factory ("Acoustic/Dreadnought.luthierguitar");

    auto modes = [&base] (const char* bracing)
    {
        const auto d = mapSpec (withPart (base, GuitarSlot::body, "bracing", bracing));
        std::vector<BodyMode> m;
        BodyModels::buildModes (d.body, m);
        return BodyModels::computeTopFundamental (d.body);
    };

    const double x = modes ("x"), fan = modes ("fan"), ladder = modes ("ladder");
    CHECK (x != fan && x != ladder && fan != ladder);
}

LUTHIER_TEST (PartAcoustics, fretboardFretsAndNutSetTheirBrightness)
{
    auto base = factory ("Electric/Vintage Double-Cut.luthierguitar");
    const auto ref = mapSpec (base);

    // PA-22: an ebony board is brighter than rosewood.
    CHECK (mapSpec (withPart (base, GuitarSlot::fretboard, "wood", "ebony")).fretBrightness
             > mapSpec (withPart (base, GuitarSlot::fretboard, "wood", "rosewood")).fretBrightness);

    // PA-24: the material table, against nickel-silver.
    auto fret = [&base] (const char* m) { return mapSpec (withPart (base, GuitarSlot::frets, "material", m)).fretBrightness; };
    const double ns = fret ("nickel_silver");
    CHECK_NEAR (fret ("stainless") / ns, 0.90 / 0.70, 1.0e-9);
    CHECK_NEAR (fret ("gold_evo") / ns, 0.80 / 0.70, 1.0e-9);
    CHECK_NEAR (fret ("brass") / ns, 0.60 / 0.70, 1.0e-9);

    // PA-28: the nut sets open strings only.
    const auto brassNut = mapSpec (withPart (base, GuitarSlot::nut, "material", "brass"));
    CHECK_NEAR (brassNut.nutBrightness, 0.85, 1.0e-9);
    CHECK_NEAR (brassNut.fretBrightness, ref.fretBrightness, 1.0e-12);

    // PA-37: a steeper break angle is brighter, monotonically.
    if (base.get (GuitarSlot::tailpiece) != nullptr)
    {
        double previous = 0.0;
        for (double angle : { 4.0, 8.0, 12.0, 16.0, 20.0 })
        {
            const double b = mapSpec (withPart (base, GuitarSlot::tailpiece, "break_angle_deg", angle)).fretBrightness;
            CHECK (b >= previous);
            previous = b;
        }
    }
}

LUTHIER_TEST (PartAcoustics, fretCountAndNutSlotsReachTheSetup)
{
    auto base = factory ("Electric/Vintage Double-Cut.luthierguitar");

    // PA-27.
    CHECK (mapSpec (withPart (base, GuitarSlot::frets, "count", 21)).spec.maxFrets == 21);
    CHECK (mapSpec (withPart (base, GuitarSlot::frets, "count", 24)).spec.maxFrets == 24);

    // PA-29: the nut part's slots fill in what the guitar's setup omits.
    auto g = withPart (base, GuitarSlot::nut, "slot_depths_mm", juce::var (juce::Array<juce::var> { 0.61, 0.62, 0.63, 0.64, 0.65, 0.66 }));
    g.setup.nutSlotDepthsMm.clear();
    const auto d = mapSpec (g);
    CHECK_NEAR (d.setup.nutDepth[0], 0.61, 1.0e-9);
    CHECK_NEAR (d.setup.nutDepth[5], 0.66, 1.0e-9);
}

LUTHIER_TEST (PartAcoustics, bridgeAndTailpieceMapTheirFields)
{
    auto single = factory ("Electric/Vintage Single-Cut.luthierguitar");

    // PA-34.
    auto trem = [&single] (const char* t) { return mapSpec (withPart (single, GuitarSlot::bridge, "tremolo_type", t)).spec.bridge; };
    CHECK (trem ("floyd") == WhammyEngine::BridgeType::FloydRose);
    CHECK (trem ("bigsby") == WhammyEngine::BridgeType::Bigsby);
    CHECK (trem ("vintage") == WhammyEngine::BridgeType::VintageTrem);
    CHECK (trem ("none") == WhammyEngine::BridgeType::Fixed);

    // PA-36 / PA-58: masses add, exactly.
    CHECK_MSG (single.get (GuitarSlot::tailpiece) != nullptr, "the single-cut has no tailpiece");

    if (single.get (GuitarSlot::tailpiece) != nullptr)
    {
        const double a = mapSpec (withPart (single, GuitarSlot::tailpiece, "mass_g", 40.0)).terminationMassG;
        const double b = mapSpec (withPart (single, GuitarSlot::tailpiece, "mass_g", 90.0)).terminationMassG;
        CHECK_NEAR (b - a, 50.0, 1.0e-9);
    }
}

LUTHIER_TEST (PartAcoustics, stringsAndFinishMapTheirFields)
{
    auto base = factory ("Electric/Vintage Double-Cut.luthierguitar");

    // PA-48: the winding style decides before the metal.
    auto mat = [&base] (const char* field, const char* v) { return mapSpec (withPart (base, GuitarSlot::strings, field, v)).stringMaterial; };
    CHECK (mat ("winding", "flat") == StringMaterial::Flatwound);
    CHECK (mat ("winding", "half") == StringMaterial::Halfwound);
    CHECK (mat ("winding", "coated") == StringMaterial::Coated);
    CHECK (mat ("winding_material", "stainless") == StringMaterial::StainlessSteel);
    CHECK (mat ("winding_material", "pure_nickel") == StringMaterial::PureNickel);
    CHECK (mat ("winding_material", "phosphor_bronze") == StringMaterial::PhosphorBronze);

    // PA-57: finish aging moves the body's age, and the modes' Q with it.
    auto aged = base;
    auto fresh = base;
    aged.finish.aging = 1.0;
    fresh.finish.aging = 0.0;
    const auto da = mapSpec (aged), df = mapSpec (fresh);
    CHECK (da.body.age > df.body.age);

    std::vector<BodyMode> ma, mf;
    BodyModels::buildModes (da.body, ma);
    BodyModels::buildModes (df.body, mf);
    CHECK (! ma.empty() && ma.size() == mf.size() && ma[0].q > mf[0].q);
}
