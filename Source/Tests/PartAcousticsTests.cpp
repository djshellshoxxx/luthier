/*  From parts to engine numbers (part-acoustics.md 11). */

#include "TestFramework.h"

#include "../Model/Workshop/PartAcoustics.h"
#include "../LuthierEngine.h"

#include <algorithm>
#include <vector>

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

LUTHIER_TEST (PartAcoustics, theWoodTableIsTheSpecs)
{
    // 1: the 17-wood table (plus cypress and steel), exactly.
    struct Row { const char* id; double density, youngs, tanDelta; };

    static const Row rows[] =
    {
        { "alder", 420, 9.5, 8.5e-3 }, { "ash_swamp", 480, 11.0, 7.5e-3 }, { "ash_northern", 680, 13.0, 6.5e-3 },
        { "basswood", 420, 9.0, 11.0e-3 }, { "mahogany", 550, 10.5, 9.0e-3 }, { "mahogany_african", 530, 9.8, 9.5e-3 },
        { "maple_hard", 705, 12.6, 6.0e-3 }, { "maple_soft", 545, 10.0, 7.5e-3 }, { "korina", 480, 10.0, 8.5e-3 },
        { "poplar", 455, 10.9, 10.0e-3 }, { "walnut", 610, 11.5, 7.0e-3 }, { "rosewood", 830, 12.0, 6.0e-3 },
        { "ebony", 1040, 16.0, 4.5e-3 }, { "pau_ferro", 860, 13.5, 5.5e-3 }, { "spruce", 400, 11.0, 7.0e-3 },
        { "cedar", 350, 8.0, 9.0e-3 }, { "koa", 610, 10.5, 8.0e-3 }, { "cypress", 510, 9.0, 7.8e-3 },
        { "steel", 7850, 200.0, 0.5e-3 },
    };

    for (const auto& row : rows)
    {
        WoodData data {};
        CHECK_MSG (lookUpWood (row.id, data), juce::String (row.id) + " missing from the wood table");
        CHECK_MSG (std::abs (data.densityKgM3 - row.density) < 1.0e-9
                     && std::abs (data.youngsGPa - row.youngs) < 1.0e-9
                     && std::abs (data.lossTangent - row.tanDelta) < 1.0e-12,
                   juce::String (row.id) + " does not match the spec's row");
    }

    WoodData unknown {};
    CHECK (! lookUpWood ("unobtanium", unknown));
}

LUTHIER_TEST (PartAcoustics, chamberingPicksItsShape)
{
    // 2.1: solid/chambered/semi_hollow/hollow map to their BodyShape, and the
    // modal count rises in that order; acoustic falls back to the guitar's
    // own family/style (a dreadnought here).
    auto base = factory ("Electric/Classic T-Style.luthierguitar");

    struct Row { const char* chambering; BodyShape shape; };

    const Row rows[] =
    {
        { "solid",       BodyShape::SolidStandard },
        { "chambered",   BodyShape::Chambered },
        { "semi_hollow", BodyShape::SemiHollow },
        { "hollow",      BodyShape::Hollow },
    };

    int previousModes = 0;

    for (const auto& row : rows)
    {
        auto g = base;
        g.parts[(size_t) GuitarSlot::body] = withField (base.get (GuitarSlot::body), "chambering", juce::String (row.chambering));
        const auto d = mapSpec (g);

        CHECK_MSG (d.body.shape == row.shape, juce::String (row.chambering) + " picked the wrong BodyShape");

        std::vector<BodyMode> modes;
        BodyModels::buildModes (d.body, modes);
        CHECK_MSG ((int) modes.size() >= previousModes,
                   juce::String (row.chambering) + ": " + juce::String ((int) modes.size())
                     + " modes, fewer than the chambering before it");
        previousModes = (int) modes.size();
    }

    auto acousticGuitar = base;
    acousticGuitar.parts[(size_t) GuitarSlot::body] = withField (base.get (GuitarSlot::body), "chambering", "acoustic");
    CHECK (mapSpec (acousticGuitar).body.shape == BodyShape::Dreadnought);
}

LUTHIER_TEST (PartAcoustics, fretMaterialsAreTheSpecsTable)
{
    // 4: stainless/gold_evo/brass/nickel-silver termination brightness
    // 0.90/0.80/0.60/0.70. Every other multiplier (fretboard wood, break
    // angle, fret width) is held fixed, so the ratio to nickel-silver is exact.
    auto base = factory ("Electric/Classic T-Style.luthierguitar");

    auto brightnessFor = [&base] (const char* material)
    {
        auto g = base;
        g.parts[(size_t) GuitarSlot::frets] = withField (base.get (GuitarSlot::frets), "material", juce::String (material));
        return mapSpec (g).fretBrightness;
    };

    const double nickel = brightnessFor ("nickel_silver");
    struct Row { const char* material; double expected; };

    for (const auto& row : { Row { "stainless", 0.90 }, Row { "gold_evo", 0.80 }, Row { "brass", 0.60 } })
    {
        const double ratio = brightnessFor (row.material) / nickel;
        CHECK_MSG (std::abs (ratio - row.expected / 0.70) < 0.02,
                   juce::String (row.material) + ": ratio " + juce::String (ratio, 3));
    }
}

LUTHIER_TEST (PartAcoustics, nutMaterialsAreTheSpecsTable)
{
    // 4: brass/graphite/plastic/tusq/bone open-string termination brightness
    // 0.85/0.70/0.60/0.72/0.75, applied with no other multiplier.
    auto base = factory ("Electric/Classic T-Style.luthierguitar");

    auto brightnessFor = [&base] (const char* material)
    {
        auto g = base;
        g.parts[(size_t) GuitarSlot::nut] = withField (base.get (GuitarSlot::nut), "material", juce::String (material));
        return mapSpec (g).nutBrightness;
    };

    struct Row { const char* material; double expected; };

    for (const auto& row : { Row { "bone", 0.75 }, Row { "brass", 0.85 }, Row { "graphite", 0.70 },
                             Row { "plastic", 0.60 }, Row { "tusq", 0.72 } })
        CHECK_MSG (std::abs (brightnessFor (row.material) - row.expected) < 1.0e-9,
                   juce::String (row.material) + ": " + juce::String (brightnessFor (row.material), 3));
}

LUTHIER_TEST (PartAcoustics, fretsCountSetsThePlayableRange)
{
    // 4: frets.count sets the playable range (clamped 12-27).
    auto base = factory ("Electric/Classic T-Style.luthierguitar");

    auto maxFretsFor = [&base] (double count)
    {
        auto g = base;
        g.parts[(size_t) GuitarSlot::frets] = withField (base.get (GuitarSlot::frets), "count", count);
        return mapSpec (g).spec.maxFrets;
    };

    CHECK (maxFretsFor (21.0) == 21);
    CHECK (maxFretsFor (24.0) == 24);
    CHECK (maxFretsFor (21.0) != maxFretsFor (24.0));
}

LUTHIER_TEST (PartAcoustics, nutSlotDepthsReachSetupWhenTheGuitarOmitsThem)
{
    // 4: slot_depths_mm from the nut part reaches d.setup.nutDepth when the
    // guitar's own setup measurement omits per-string values.
    auto base = factory ("Electric/Classic T-Style.luthierguitar");
    auto g = base;
    g.setup.nutSlotDepthsMm.clear();

    juce::Array<juce::var> depths;
    for (int s = 0; s < 6; ++s)
        depths.add (0.40 + s * 0.02);

    g.parts[(size_t) GuitarSlot::nut] = withField (base.get (GuitarSlot::nut), "slot_depths_mm", juce::var (depths));

    const auto d = mapSpec (g);

    for (int s = 0; s < 6; ++s)
        CHECK_MSG (std::abs (d.setup.nutDepth[(size_t) s] - (double) depths[s]) < 1.0e-9,
                   "string " + juce::String (s));
}

LUTHIER_TEST (PartAcoustics, tremoloTypeSelectsTheWhammyBridge)
{
    // 5: has_tremolo / tremolo_type maps to WhammyEngine's bridge type.
    auto base = factory ("Electric/Vintage Double-Cut.luthierguitar");

    auto bridgeFor = [&base] (const char* tremolo)
    {
        auto g = base;
        g.parts[(size_t) GuitarSlot::bridge] = withField (base.get (GuitarSlot::bridge), "tremolo_type", juce::String (tremolo));
        return mapSpec (g).spec.bridge;
    };

    CHECK (bridgeFor ("floyd") == WhammyEngine::BridgeType::FloydRose);
    CHECK (bridgeFor ("bigsby") == WhammyEngine::BridgeType::Bigsby);
    CHECK (bridgeFor ("vintage") == WhammyEngine::BridgeType::VintageTrem);
    CHECK (bridgeFor ("two_point") == WhammyEngine::BridgeType::VintageTrem);
    CHECK (bridgeFor ("none") == WhammyEngine::BridgeType::Fixed);
}

LUTHIER_TEST (PartAcoustics, everyFactoryBridgeMatchesItsTypeRow)
{
    // 5: the bridge.type -> mass/coupling table. Bass bridges reuse "hardtail"
    // with intentionally heavier values (heavier anchors), so they are excluded.
    struct Row { const char* type; double mass, coupling; };

    static const Row rows[] =
    {
        { "tune_o_matic", 95.0, 0.55 }, { "hardtail", 110.0, 0.70 }, { "vintage_tremolo", 165.0, 0.45 },
        { "two_point_tremolo", 150.0, 0.48 }, { "floyd_rose", 320.0, 0.30 }, { "bigsby", 480.0, 0.35 },
        { "pin_bridge", 28.0, 0.92 }, { "resonator_spider", 45.0, 0.88 },
    };

    const auto folder = PartLibrary::getFactoryPartsFolder().getChildFile ("Bridges");
    int checked = 0;

    for (const auto& file : folder.findChildFiles (juce::File::findFiles, false, "*.luthierpart"))
    {
        const auto json = juce::JSON::parse (file);
        const auto fields = json.getProperty ("fields", juce::var());
        const auto compat = json.getProperty ("meta", juce::var()).getProperty ("compatibility", juce::var());

        if (auto* compatArray = compat.getArray())
            if (std::any_of (compatArray->begin(), compatArray->end(),
                              [] (const juce::var& v) { return v.toString() == "bass"; }))
                continue;

        const auto type = fields.getProperty ("type", juce::var()).toString();

        for (const auto& row : rows)
        {
            if (type != row.type)
                continue;

            const double mass = (double) fields.getProperty ("mass_g", 0.0);
            const double coupling = (double) fields.getProperty ("coupling", 0.0);

            CHECK_MSG (std::abs (mass - row.mass) / row.mass < 0.35,
                       file.getFileName() + ": mass " + juce::String (mass) + " vs the spec's " + juce::String (row.mass));
            CHECK_MSG (std::abs (coupling - row.coupling) < 0.15,
                       file.getFileName() + ": coupling " + juce::String (coupling) + " vs the spec's "
                         + juce::String (row.coupling));
            ++checked;
        }
    }

    CHECK_MSG (checked >= (int) (sizeof (rows) / sizeof (rows[0])), "not every bridge type row was exercised by a factory part");
}

LUTHIER_TEST (PartAcoustics, windingMapsToItsStringMaterial)
{
    // 8: winding style decides before the metal does.
    auto base = factory ("Electric/Classic T-Style.luthierguitar");

    auto materialFor = [&base] (const char* winding, const char* material)
    {
        auto g = base;
        auto part = withField (base.get (GuitarSlot::strings), "winding", juce::String (winding));
        part = withField (part, "winding_material", juce::String (material));
        g.parts[(size_t) GuitarSlot::strings] = part;
        return mapSpec (g).stringMaterial;
    };

    CHECK (materialFor ("round", "pure_nickel") == StringMaterial::PureNickel);
    CHECK (materialFor ("round", "stainless") == StringMaterial::StainlessSteel);
    CHECK (materialFor ("round", "cobalt") == StringMaterial::Cobalt);
    CHECK (materialFor ("round", "phosphor_bronze") == StringMaterial::PhosphorBronze);
    CHECK (materialFor ("round", "bronze_8020") == StringMaterial::Bronze8020);
    CHECK (materialFor ("round", "silk_steel") == StringMaterial::SilkAndSteel);
    CHECK (materialFor ("round", "nylon") == StringMaterial::Nylon);
    CHECK (materialFor ("round", "fluorocarbon") == StringMaterial::Fluorocarbon);
    CHECK (materialFor ("flat", "pure_nickel") == StringMaterial::Flatwound);
    CHECK (materialFor ("half", "pure_nickel") == StringMaterial::Halfwound);
    CHECK (materialFor ("coated", "pure_nickel") == StringMaterial::Coated);
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
