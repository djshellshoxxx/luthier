#include "PartAcoustics.h"
#include "../../Support/ThreadProbe.h"

namespace luthier
{

//==============================================================================
//  Tables
//==============================================================================
bool lookUpWood (const juce::String& id, WoodData& out)
{
    // part-acoustics.md 1: along-grain, 12% moisture, standard timber
    // references. Cypress and steel are not in its table: cypress from the
    // same references (flamenco guitars), steel as the resonator body's metal
    // (7850 kg/m3, 200 GPa, very low internal loss).
    struct Row { const char* id; WoodData data; };

    static const Row rows[] =
    {
        { "alder",            { 420, 9.5,  8.5e-3 } },
        { "ash_swamp",        { 480, 11.0, 7.5e-3 } },
        { "ash_northern",     { 680, 13.0, 6.5e-3 } },
        { "basswood",         { 420, 9.0,  11.0e-3 } },
        { "mahogany",         { 550, 10.5, 9.0e-3 } },
        { "mahogany_african", { 530, 9.8,  9.5e-3 } },
        { "maple_hard",       { 705, 12.6, 6.0e-3 } },
        { "maple_soft",       { 545, 10.0, 7.5e-3 } },
        { "korina",           { 480, 10.0, 8.5e-3 } },
        { "poplar",           { 455, 10.9, 10.0e-3 } },
        { "walnut",           { 610, 11.5, 7.0e-3 } },
        { "rosewood",         { 830, 12.0, 6.0e-3 } },
        { "ebony",            { 1040, 16.0, 4.5e-3 } },
        { "pau_ferro",        { 860, 13.5, 5.5e-3 } },
        { "spruce",           { 400, 11.0, 7.0e-3 } },
        { "cedar",            { 350, 8.0,  9.0e-3 } },
        { "koa",              { 610, 10.5, 8.0e-3 } },
        { "cypress",          { 510, 9.0,  7.8e-3 } },
        { "steel",            { 7850, 200.0, 0.5e-3 } },
    };

    for (const auto& row : rows)
        if (id == row.id)
        {
            out = row.data;
            return true;
        }

    return false;
}

MagnetData lookUpMagnet (const juce::String& id)
{
    // part-acoustics.md 6.2.
    if (id == "alnico2")   return { 0.55, 0.020 };
    if (id == "alnico3")   return { 0.45, 0.016 };
    if (id == "alnico4")   return { 0.65, 0.024 };
    if (id == "alnico8")   return { 0.95, 0.040 };
    if (id == "ceramic")   return { 1.00, 0.045 };
    if (id == "neodymium") return { 1.20, 0.055 };
    if (id == "none")      return { 0.0, 0.0 };
    return { 0.80, 0.032 };   // alnico5, the standard
}

namespace
{
    Wood engineWood (const juce::String& id)
    {
        // The body model's own wood enum is coarser than the table; each table
        // wood maps to the closest one it has.
        if (id == "spruce")                               return Wood::SitkaSpruce;
        if (id == "cedar")                                return Wood::Cedar;
        if (id.startsWith ("mahogany"))                   return Wood::Mahogany;
        if (id.startsWith ("maple") || id == "steel")     return Wood::Maple;
        if (id == "alder")                                return Wood::Alder;
        if (id.startsWith ("ash"))                        return Wood::Ash;
        if (id == "rosewood" || id == "pau_ferro")        return Wood::Rosewood;
        if (id == "koa")                                  return Wood::Koa;
        if (id == "basswood")                             return Wood::Basswood;
        if (id == "korina")                               return Wood::Korina;
        if (id == "poplar")                               return Wood::Poplar;
        if (id == "walnut")                               return Wood::Walnut;
        if (id == "ebony")                                return Wood::Ebony;
        if (id == "cypress")                              return Wood::Sapele;
        return Wood::Alder;
    }

    Bracing engineBracing (const juce::String& id, const juce::String& chambering)
    {
        if (id == "x" || id == "scalloped_x") return id == "x" ? Bracing::XBrace : Bracing::ForwardShiftedX;
        if (id == "fan")                      return Bracing::FanBrace;
        if (id == "ladder")                   return Bracing::LadderBrace;
        if (chambering == "semi_hollow")      return Bracing::SemiHollowBlock;
        if (chambering == "hollow")           return Bracing::HollowParallel;
        return Bracing::SolidBodyNone;
    }

    /*  The compiled guitar a parts guitar starts from, for everything the
        parts do not describe (the default amp and cabinet, the tuning preset).
        Chosen by family, string count and body style. */
    GuitarType baseTypeFor (const WorkshopGuitar& g, int strings)
    {
        const auto& style = g.bodyStyle;

        if (g.family == "bass")
            return strings >= 5 ? GuitarType::FiveStringBass : GuitarType::PrecisionBass;

        if (g.family == "classical")
            return style.contains ("flamenco") || g.name.containsIgnoreCase ("flamenc")
                     ? GuitarType::Flamenco : GuitarType::Classical;

        if (g.family == "resonator")
            return GuitarType::Resonator;

        if (g.family == "acoustic")
        {
            if (strings >= 12)            return GuitarType::TwelveString;
            if (style.contains ("parlor")) return GuitarType::Parlor;
            if (style.contains ("jumbo"))  return GuitarType::Jumbo;
            if (style.contains ("auditorium")) return GuitarType::Auditorium;
            return GuitarType::Dreadnought;
        }

        if (strings >= 8) return GuitarType::EightString;
        if (strings == 7) return GuitarType::SevenString;
        if (style.contains ("semi"))                 return GuitarType::ES335;
        if (style.contains ("archtop"))              return GuitarType::ES335;
        if (style.contains ("slab"))                 return GuitarType::Telecaster;
        if (style.contains ("single_cutaway"))       return GuitarType::LesPaul;
        if (style.contains ("thin"))                 return GuitarType::SG;
        if (style.contains ("angular"))              return GuitarType::Explorer;
        if (style.contains ("superstrat"))           return GuitarType::IbanezRG;
        if (style.contains ("offset"))               return GuitarType::Jazzmaster;
        return GuitarType::Stratocaster;
    }

    BodyShape shapeFor (const WorkshopGuitar& g, const juce::String& chambering, double thicknessMm)
    {
        const auto& style = g.bodyStyle;

        if (g.family == "bass")
            return chambering == "solid" ? BodyShape::BassSolid : BodyShape::BassHollow;

        if (chambering == "acoustic")
        {
            if (g.family == "resonator")              return BodyShape::Resonator;
            if (g.family == "classical")              return style.contains ("flamenc") ? BodyShape::Flamenco : BodyShape::Classical;
            if (style.contains ("parlor"))            return BodyShape::Parlor;
            if (style.contains ("jumbo"))             return g.getStringCount() >= 12 ? BodyShape::TwelveStringDread : BodyShape::Jumbo;
            if (style.contains ("auditorium") || style.contains ("gypsy")) return BodyShape::Auditorium;
            return BodyShape::Dreadnought;
        }

        if (chambering == "semi_hollow") return BodyShape::SemiHollow;
        if (chambering == "hollow")      return BodyShape::Hollow;
        if (chambering == "chambered")   return BodyShape::Chambered;
        if (style.contains ("offset"))   return BodyShape::Offset;

        // 2: a solid body's modes are a plate's, so thickness picks the size.
        return thicknessMm < 38.0 ? BodyShape::SolidThin
             : (thicknessMm > 48.0 ? BodyShape::SolidHeavy : BodyShape::SolidStandard);
    }

    StringMaterial materialFor (const Part* strings)
    {
        if (strings == nullptr)
            return StringMaterial::NickelPlatedSteel;

        const auto winding = strings->text ("winding", "round");

        // 8: the winding style decides before the metal does.
        if (winding == "flat")   return StringMaterial::Flatwound;
        if (winding == "half")   return StringMaterial::Halfwound;
        if (winding == "coated") return StringMaterial::Coated;

        const auto m = strings->text ("winding_material", "nickel_plated_steel");

        if (m == "pure_nickel")     return StringMaterial::PureNickel;
        if (m == "stainless")       return StringMaterial::StainlessSteel;
        if (m == "cobalt")          return StringMaterial::Cobalt;
        if (m == "phosphor_bronze") return StringMaterial::PhosphorBronze;
        if (m == "bronze_8020")     return StringMaterial::Bronze8020;
        if (m == "silk_steel")      return StringMaterial::SilkAndSteel;
        if (m == "nylon")           return StringMaterial::Nylon;
        if (m == "fluorocarbon")    return StringMaterial::Fluorocarbon;
        return StringMaterial::NickelPlatedSteel;
    }

    PickupType pickupTypeFor (const juce::String& family)
    {
        if (family == "humbucker" || family == "active" || family == "split_coil") return PickupType::Humbucker;
        if (family == "mini_humbucker")                                           return PickupType::Humbucker;
        if (family == "p90")                                                      return PickupType::P90;
        if (family == "piezo")                                                    return PickupType::Piezo;
        if (family == "soundhole")                                                return PickupType::MagneticSoundhole;
        return PickupType::SingleCoil;
    }

    MagnetType magnetTypeFor (const juce::String& id)
    {
        if (id == "alnico2") return MagnetType::Alnico2;
        if (id == "alnico3") return MagnetType::Alnico3;
        if (id == "ceramic" || id == "neodymium" || id == "alnico8") return MagnetType::Ceramic;
        return MagnetType::Alnico5;
    }

    // part-acoustics.md 4: termination brightness by material.
    double fretMaterialBrightness (const juce::String& m)
    {
        if (m == "stainless") return 0.90;
        if (m == "gold_evo")  return 0.80;
        if (m == "brass")     return 0.60;
        if (m == "none")      return 0.55;   // fretless: the finger on wood, duller than any fret
        return 0.70;                         // nickel-silver
    }

    double nutMaterialBrightness (const juce::String& m)
    {
        if (m == "brass")    return 0.85;
        if (m == "graphite") return 0.70;
        if (m == "plastic")  return 0.60;
        if (m == "tusq")     return 0.72;
        return 0.75;                         // bone
    }

    // part-acoustics.md 3: joint coupling.
    double jointCoupling (const juce::String& joint)
    {
        if (joint == "set")     return 0.80;
        if (joint == "through") return 0.95;
        return 0.55;                         // bolt
    }

    // part-acoustics.md 6: pole piece eddy losses (steel dulls, ceramic is brightest).
    double poleBrightness (const juce::String& m)
    {
        if (m == "steel")   return 0.92;
        if (m == "ceramic") return 1.05;
        return 1.0;                          // alnico
    }
}

//==============================================================================
//  Physics helpers
//==============================================================================
double stringTensionNewtons (double scaleLengthMm, double frequencyHz, double mu)
{
    const double twoLf = 2.0 * (scaleLengthMm / 1000.0) * frequencyHz;
    return twoLf * twoLf * mu;
}

double stringLinearDensity (double gaugeInches, bool wound, StringMaterial material)
{
    // 8: mass per length from the wire's cross-section. A wound string is a
    // core plus a winding and weighs less than a solid rod of its diameter;
    // the material table's wound mass factor is that ratio.
    const auto& props = StringMaterials::get (material);
    const double radius = gaugeInches * 0.0254 * 0.5;
    const double area = juce::MathConstants<double>::pi * radius * radius;
    const double density = wound ? props.woundDensityKgM3 : props.densityKgM3;
    return area * density * (wound ? props.woundMassFactor : 1.0);
}

double firstCombNullHz (double positionMm, double scaleLengthMm, double openHz)
{
    // 6.1: nulls at f = n v / (2 x position); v = 2 L f0.
    const double velocity = 2.0 * (scaleLengthMm / 1000.0) * openHz;
    return velocity / (2.0 * juce::jmax (1.0e-3, positionMm / 1000.0));
}

//==============================================================================
bool DerivedAcoustics::operator== (const DerivedAcoustics& o) const
{
    // A byte-level comparison of every numeric output the engine consumes -
    // the determinism test (11) is about exactly this.
    auto same = [] (double a, double b) { return a == b; };

    if (numStrings != o.numStrings || numPickups != o.numPickups || hasPiezo != o.hasPiezo
        || stringMaterial != o.stringMaterial || spec.scaleLengthMm != o.spec.scaleLengthMm
        || body.shape != o.body.shape || body.topWood != o.body.topWood || body.backWood != o.body.backWood)
        return false;

    for (size_t i = 0; i < gaugesIn.size(); ++i)
        if (! same (gaugesIn[i], o.gaugesIn[i]) || ! same (tensionNewtons[i], o.tensionNewtons[i])
            || ! same (windingPitchPerMm[i], o.windingPitchPerMm[i]))
            return false;

    for (size_t i = 0; i < pickups.size(); ++i)
    {
        const auto& a = pickups[i];
        const auto& b = o.pickups[i];

        if (! same (a.spec.inductanceHenries, b.spec.inductanceHenries) || ! same (a.spec.position, b.spec.position)
            || ! same (a.spec.heightMm, b.spec.heightMm) || ! same (a.magnetPull, b.magnetPull)
            || ! same (a.spec.resistanceKOhm, b.spec.resistanceKOhm)
            || ! same (a.spec.capacitancePf, b.spec.capacitancePf)
            || ! same (a.spec.outputTrimDb, b.spec.outputTrimDb)
            || ! same (a.coverLossDbAt4k, b.coverLossDbAt4k))
            return false;
    }

    return same (couplingFraction, o.couplingFraction) && same (terminationMassG, o.terminationMassG)
        && same (sustainScale, o.sustainScale) && same (fretBrightness, o.fretBrightness)
        && same (nutBrightness, o.nutBrightness) && same (bodyGainDb, o.bodyGainDb)
        && same (airResonanceHz, o.airResonanceHz) && same (body.scaleWidth, o.body.scaleWidth)
        && same (body.resonanceTrim, o.body.resonanceTrim) && same (body.age, o.body.age)
        && same (body.scaleDepth, o.body.scaleDepth) && same (body.topThicknessMm, o.body.topThicknessMm)
        && wiring == o.wiring
        && same (setup.actionTreble, o.setup.actionTreble) && same (setup.fretHeight, o.setup.fretHeight);
}

//==============================================================================
//  mapSpec
//==============================================================================
DerivedAcoustics mapSpec (const WorkshopGuitar& g)
{
    ThreadProbe::noteMapSpec();

    DerivedAcoustics d;

    const auto* body = g.get (GuitarSlot::body).get();
    const auto* top = g.get (GuitarSlot::top).get();
    const auto* neck = g.get (GuitarSlot::neck).get();
    const auto* board = g.get (GuitarSlot::fretboard).get();
    const auto* frets = g.get (GuitarSlot::frets).get();
    const auto* nut = g.get (GuitarSlot::nut).get();
    const auto* bridge = g.get (GuitarSlot::bridge).get();
    const auto* tail = g.get (GuitarSlot::tailpiece).get();
    const auto* wiring = g.get (GuitarSlot::wiring).get();
    const auto* strings = g.get (GuitarSlot::strings).get();
    const auto* guard = g.get (GuitarSlot::pickguard).get();

    auto num = [] (const Part* p, const char* field, double fallback) { return p != nullptr ? p->number (field, fallback) : fallback; };
    auto str = [] (const Part* p, const char* field, const juce::String& fallback) { return p != nullptr ? p->text (field, fallback) : fallback; };

    d.numStrings = g.getStringCount (&d.stringExcess);

    // ---- the compiled base, re-pointed ------------------------------------------
    const auto baseType = baseTypeFor (g, d.numStrings);
    d.spec = GuitarLibrary::get (baseType);
    d.spec.name = "Workshop guitar";

    d.spec.numStrings = d.numStrings;
    d.spec.twelveString = d.numStrings == 12;
    d.spec.category = g.family == "bass" ? GuitarCategory::Bass
                    : (g.family == "electric" ? GuitarCategory::Electric : GuitarCategory::Acoustic);

    // 3: the neck's scale is the single most audible neck field.
    d.spec.scaleLengthMm = num (neck, "scale_length_mm", d.spec.scaleLengthMm);
    d.spec.maxFrets = juce::jlimit (12, 27, (int) num (frets, "count", num (neck, "frets", d.spec.maxFrets)));
    d.spec.fretless = str (frets, "material", "nickel_silver") == "none" || num (frets, "count", 1.0) <= 0.0;

    if (d.spec.fretless)
        d.spec.maxFrets = juce::jlimit (12, 27, (int) num (neck, "frets", 24.0));

    // ---- body (2) -------------------------------------------------------------------------
    const auto chambering = str (body, "chambering", "solid");
    const double bodyThickness = num (body, "thickness_mm", 44.5);

    d.body.shape = shapeFor (g, chambering, bodyThickness);
    d.body.backWood = engineWood (str (body, "wood", "alder"));
    d.body.sideWood = d.body.backWood;
    d.body.topWood = top != nullptr ? engineWood (top->text ("wood", "maple_hard")) : d.body.backWood;
    d.body.bracing = engineBracing (str (body, "bracing", "none"), chambering);

    // Dimensions against the shape's own defaults: mode frequency goes as
    // 1 / area and, for a plate, with thickness (2).
    const auto& shape = BodyModels::getShape (d.body.shape);
    const double areaCm2 = num (body, "area_cm2", 0.0);

    if (areaCm2 > 0.0 && shape.lowerBoutMm > 0.0)
    {
        // The shape's plan area, estimated from its lower bout: a guitar body
        // is about 1.3 bouts long and an hourglass fills about 0.78 of the
        // rectangle around it.
        const double shapeAreaCm2 = shape.lowerBoutMm * shape.lowerBoutMm * 1.3 * 0.78 / 100.0;
        d.body.scaleWidth = juce::jlimit (0.6, 1.6, std::sqrt (areaCm2 / shapeAreaCm2));
    }

    if (shape.depthMm > 0.0)
        d.body.scaleDepth = juce::jlimit (0.5, 2.0, bodyThickness / shape.depthMm);

    if (top != nullptr)
        d.body.topThicknessMm = top->number ("thickness_mm", 0.0);

    // Density overriding the table (2) moves the modes as sqrt(E / rho).
    {
        WoodData wood {};

        if (body != nullptr && lookUpWood (body->text ("wood", "alder"), wood))
        {
            const double density = body->number ("density_kg_m3", wood.densityKgM3);
            d.body.resonanceTrim = juce::jlimit (0.7, 1.4, std::sqrt (wood.densityKgM3 / juce::jmax (100.0, density)));
        }
    }

    d.body.age = juce::jlimit (0.0, 1.0, 0.3 + 0.7 * g.finish.aging);

    // 2.1: chambering's mode gain, air resonance, sustain and feedback.
    struct Chamber { double gainDb, airLo, airHi, airQ, sustain, feedback; };
    Chamber chamber { 0.0, 0.0, 0.0, 0.0, 1.0, 0.1 };

    if (chambering == "chambered")        chamber = { 3.0,  180.0, 240.0, 8.0,  0.95, 0.25 };
    else if (chambering == "semi_hollow") chamber = { 6.0,  140.0, 190.0, 12.0, 0.88, 0.5 };
    else if (chambering == "hollow")      chamber = { 10.0, 90.0,  140.0, 18.0, 0.80, 0.8 };
    else if (chambering == "acoustic")    chamber = { 14.0, 90.0,  110.0, 20.0, 0.75, 0.0 };

    d.bodyGainDb = chamber.gainDb;
    d.airResonanceQ = chamber.airQ;
    d.feedbackGain = chamber.feedback;

    if (chamber.airHi > 0.0)
    {
        // 2.1: air resonance goes as 1 / sqrt(V). The range's middle belongs
        // to the table's typical volume; a bigger body resonates lower, held
        // inside the row's range.
        const double volume = juce::jmax (100.0, areaCm2) * bodyThickness / 10.0;           // cm3
        const double typical = (chambering == "acoustic" ? 2200.0 * 110.0
                             : chambering == "hollow" ? 1800.0 * 76.0
                             : chambering == "semi_hollow" ? 1500.0 * 44.5 : 1100.0 * 50.0) / 10.0;
        const double mid = 0.5 * (chamber.airLo + chamber.airHi);
        d.airResonanceHz = juce::jlimit (chamber.airLo, chamber.airHi, mid * std::sqrt (typical / volume));
    }

    // ---- termination: masses add, couplings multiply (10) ------------------------------------
    d.terminationMassG = num (bridge, "mass_g", 100.0) + num (tail, "mass_g", 0.0) + num (guard, "mass_g", 0.0);
    d.couplingFraction = jointCoupling (str (neck, "joint", "bolt")) * num (bridge, "coupling", 0.55);

    // 5: heavier is less lossy and sustains more. Normalised to a 100 g
    // tune-o-matic, a gentle cube root so a 480 g Bigsby is not magic; then
    // chambering's own sustain (2.1).
    d.sustainScale = juce::jlimit (0.5, 1.5, std::cbrt (d.terminationMassG / 100.0)) * chamber.sustain;

    // 5: break angle is downforce; steeper is tighter and brighter.
    const double breakAngle = num (tail, "break_angle_deg", 12.0);

    // 4: termination brightness. The fretboard wood adds its own: harder
    // wood, brighter attack (3), from its damping.
    d.fretBrightness = fretMaterialBrightness (str (frets, "material", "nickel_silver"));
    {
        WoodData wood {};

        if (board != nullptr && lookUpWood (board->text ("wood", "rosewood"), wood))
            d.fretBrightness *= juce::jlimit (0.9, 1.1, 1.0 + (6.0e-3 - wood.lossTangent) * 20.0);
    }

    d.fretBrightness *= juce::jlimit (0.95, 1.05, 1.0 + (breakAngle - 12.0) * 0.004);

    // Wider frets are a larger contact: slightly duller (4).
    d.fretBrightness *= juce::jlimit (0.95, 1.02, 1.0 - (num (frets, "width_mm", 2.5) - 2.5) * 0.02);

    d.nutBrightness = nutMaterialBrightness (str (nut, "material", "bone"));

    // 9: thick gloss damps an acoustic top slightly (up to -0.5 dB).
    if (chambering == "acoustic")
        d.finishDampingDb = -0.5 * juce::jlimit (0.0, 1.0, (g.finish.gloss - 0.5) * 2.0);

    // Hardware colour does nothing, and this file says so (9).

    // ---- strings (8) -------------------------------------------------------------------------------
    d.stringMaterial = materialFor (strings);
    d.spec.stringMaterial = d.stringMaterial;

    const auto gauges = strings != nullptr ? strings->numbers ("gauges_in") : juce::Array<double>();
    const auto pitches = strings != nullptr ? strings->numbers ("winding_pitch_per_mm") : juce::Array<double>();

    double open[12] {};
    TuningEngine::getPresetFrequencies (d.spec.tuning, open, juce::jmin (6, d.numStrings));

    for (int s = 0; s < juce::jmin (12, d.numStrings); ++s)
    {
        // Strings past the preset's six: each a fourth below the last (7- and
        // 8-strings), and a 12-string's courses from their pair.
        double hz = s < 6 ? open[s] : open[5] * std::pow (2.0, -5.0 * (s - 5) / 12.0);

        if (d.numStrings == 12)
            hz = open[juce::jlimit (0, 5, GuitarLibrary::courseForString (s))]
                 * semitonesToRatio (GuitarLibrary::twelveStringOctaveOffset (s));

        const double gauge = juce::isPositiveAndBelow (s, gauges.size()) ? gauges[s] : 0.0;
        d.gaugesIn[(size_t) s] = gauge;

        const auto computed = StringMaterials::computeSpec (d.stringMaterial, d.spec.stringGauge, StringAge::Fresh,
                                                            s, hz, d.spec.scaleLengthMm, gauge);
        d.tensionNewtons[(size_t) s] = computed.tensionNewtons;

        if (computed.wound)
        {
            const double wrapMm = juce::jmax (0.05, (computed.diameterMm - computed.coreDiameterMm) * 0.5);
            d.windingPitchPerMm[(size_t) s] = juce::isPositiveAndBelow (s, pitches.size()) && pitches[s] > 0.0
                                                ? pitches[s] : 1.0 / wrapMm;
        }
    }

    // ---- pickups (6) --------------------------------------------------------------------------------------
    // Neck, middle, bridge in the file; the engine's slot 0 is the bridge.
    int slot = 0;

    for (int fileIndex = 2; fileIndex >= 0; --fileIndex)
    {
        const auto* p = g.get (WorkshopGuitar::pickupSlot (fileIndex)).get();

        if (p == nullptr)
            continue;

        const auto family = p->text ("family", "single_coil");

        if (family == "piezo")
        {
            d.hasPiezo = true;
            continue;
        }

        auto& out = d.pickups[(size_t) slot];
        const auto& placement = g.placements[(size_t) fileIndex];

        out.spec = PickupSpec::makeDefault (pickupTypeFor (family), 0.13);
        out.spec.inductanceHenries = p->number ("inductance_h", out.spec.inductanceHenries);
        out.spec.resistanceKOhm = p->number ("dc_resistance_k", out.spec.resistanceKOhm);
        out.spec.capacitancePf = p->number ("capacitance_pf", out.spec.capacitancePf);
        out.spec.magnet = magnetTypeFor (p->text ("magnet", "alnico5"));

        // 6: output goes with turns; the reference level is the part's own.
        out.spec.outputTrimDb = juce::jlimit (-12.0, 12.0, p->number ("output_dbfs_reference", -6.0) + 6.0);

        out.positionMm = placement.positionMm;
        out.heightMm = 0.5 * (placement.heightTrebleMm + placement.heightBassMm);
        out.spec.position = juce::jlimit (0.02, 0.48, placement.positionMm / d.spec.scaleLengthMm);
        out.spec.heightMm = out.heightMm;

        const auto magnet = lookUpMagnet (p->text ("magnet", "alnico5"));
        out.magnetPull = magnet.pull;
        out.magnetDamping = magnet.damping;

        // 6: a nickel cover costs 0.8 dB at 4 kHz; chrome is nickel-plated
        // brass and costs about the same; plastic nothing.
        const auto cover = p->text ("cover", "none");
        out.coverLossDbAt4k = (cover == "nickel" || cover == "chrome") ? -0.8 : 0.0;
        out.poleBrightness = poleBrightness (p->text ("pole_piece_material", "alnico"));

        if (++slot >= 3)
            break;
    }

    d.numPickups = slot;
    d.spec.numPickups = juce::jmax (0, slot);
    d.spec.hasPiezo = d.hasPiezo;

    for (int i = 0; i < 3; ++i)
    {
        d.spec.pickupTypes[i] = d.pickups[(size_t) i].spec.type;
        d.spec.pickupPositions[i] = d.pickups[(size_t) i].spec.position;
        d.spec.pickupMagnets[i] = d.pickups[(size_t) i].spec.magnet;
    }

    // ---- wiring (7) ------------------------------------------------------------------------------------------
    d.wiring.volumePot = num (wiring, "volume_pot_ohm", 500.0e3);
    d.wiring.tonePot = num (wiring, "tone_pot_ohm", 500.0e3);
    d.wiring.toneCap = num (wiring, "tone_cap_f", 22.0e-9);
    d.wiring.active = wiring != nullptr && wiring->flag ("active");

    const auto taper = str (wiring, "taper", "audio");
    d.wiring.taper = taper == "linear" ? PotTaper::linear : (taper == "fifties" ? PotTaper::fiftiesWiring : PotTaper::audio);

    const auto bleed = str (wiring, "treble_bleed", "none");
    // "modern" / "vintage" since the trademark sweep; the old names still read.
    d.wiring.bleed = (bleed == "modern" || bleed == "kinman") ? TrebleBleed::kinman
                   : (bleed == "vintage" || bleed == "fender") ? TrebleBleed::fender : TrebleBleed::none;

    // ---- setup, frets and nut (fret-buzz.md) --------------------------------------------------------------------
    d.setup.actionTreble = g.setup.actionTrebleMm;
    d.setup.actionBass = g.setup.actionBassMm;
    d.setup.relief = g.setup.reliefMm;
    d.setup.fretHeight = juce::jmax (0.1, num (frets, "height_mm", 1.0));
    d.setup.scaleLengthMm = d.spec.scaleLengthMm;
    d.setup.numStrings = d.numStrings;
    d.setup.numFrets = d.spec.maxFrets;

    // The guitar's setup wins over the nut part's slots (it is the tech's
    // measurement of this guitar); the part supplies what the setup omits.
    const auto nutSlots = nut != nullptr ? nut->numbers ("slot_depths_mm") : juce::Array<double>();

    for (int s = 0; s < SetupGeometry::kMaxStrings; ++s)
    {
        if (juce::isPositiveAndBelow (s, g.setup.nutSlotDepthsMm.size()))
            d.setup.nutDepth[(size_t) s] = g.setup.nutSlotDepthsMm[s];
        else if (! nutSlots.isEmpty())
            d.setup.nutDepth[(size_t) s] = nutSlots[juce::jmin (s, nutSlots.size() - 1)];
    }

    d.spec.fretActionMm = 0.5 * (d.setup.actionTreble + d.setup.actionBass);

    // The whammy is the bridge's (5).
    const auto trem = str (bridge, "tremolo_type", "none");
    d.spec.bridge = trem == "floyd" ? WhammyEngine::BridgeType::FloydRose
                  : trem == "bigsby" ? WhammyEngine::BridgeType::Bigsby
                  : (trem == "vintage" || trem == "two_point") ? WhammyEngine::BridgeType::VintageTrem
                  : WhammyEngine::BridgeType::Fixed;

    d.spec.couplingAmount = juce::jlimit (0.0, 1.0, d.couplingFraction * 1.4);

    return d;
}

} // namespace luthier
