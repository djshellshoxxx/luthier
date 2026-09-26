#include "StringMaterials.h"

namespace luthier
{

namespace
{
    constexpr double kInchToMm = 25.4;
    constexpr double kReferenceScaleMm = 648.0;

    //==========================================================================
    // name, density, woundDensity, Young's, woundMass, coreRatio, sustain,
    // brightness, squeak, alwaysWound, plainTrebles
    const MaterialProperties kMaterials[(size_t) StringMaterial::NumMaterials] =
    {
        { "Nickel-Plated Steel", 7900.0, 7900.0, 2.00e11, 0.78, 0.45, 5.0, 5600.0, 1.00, false, false },
        { "Pure Nickel",         8900.0, 8900.0, 2.10e11, 0.76, 0.45, 4.6, 4400.0, 0.90, false, false },
        { "Stainless Steel",     7900.0, 7900.0, 1.93e11, 0.79, 0.45, 5.6, 6600.0, 1.15, false, false },
        { "Cobalt",              8400.0, 8400.0, 2.07e11, 0.78, 0.45, 5.3, 6200.0, 1.05, false, false },
        { "Phosphor Bronze",     8800.0, 8800.0, 1.10e11, 0.74, 0.42, 4.8, 5000.0, 1.10, false, false },
        { "80/20 Bronze",        8750.0, 8750.0, 1.15e11, 0.74, 0.42, 4.4, 5900.0, 1.12, false, false },
        { "Silk & Steel",        6200.0, 7400.0, 9.00e10, 0.62, 0.40, 3.6, 3400.0, 0.70, false, false },

        // Nylon and fluorocarbon are the two materials where the plain trebles and
        // the wound basses are made of entirely different things: the trebles are
        // the polymer, the basses are silver-plated copper wound over a floss core.
        // Using the polymer density for the basses makes a classical low E about
        // eight times too light, and the instrument nearly silent.
        { "Nylon",               1150.0, 7000.0, 4.00e09, 0.70, 0.50, 3.4, 3000.0, 0.20, false, true  },
        { "Fluorocarbon",        1780.0, 7200.0, 4.50e09, 0.72, 0.50, 3.8, 3900.0, 0.22, false, true  },

        { "Flatwound",           8100.0, 8100.0, 1.95e11, 0.86, 0.55, 4.2, 2600.0, 0.18, true,  false },
        { "Halfwound",           8000.0, 8000.0, 1.97e11, 0.82, 0.50, 4.5, 3600.0, 0.45, true,  false },
        { "Coated",              7900.0, 7900.0, 2.00e11, 0.77, 0.45, 5.8, 4900.0, 0.55, false, false }
    };

    //==========================================================================
    // Gauge sets, in inches, string 0 = highest-pitched.
    struct GaugeSet { const char* name; int count; double d[kMaxStrings]; };

    const GaugeSet kGauges[(size_t) StringGauge::NumGauges] =
    {
        { "Extra Light .008",  6, { 0.008, 0.010, 0.015, 0.021, 0.030, 0.038 } },
        { "Light .009",        6, { 0.009, 0.011, 0.016, 0.024, 0.032, 0.042 } },
        { "Regular .010",      6, { 0.010, 0.013, 0.017, 0.026, 0.036, 0.046 } },
        { "Medium .011",       6, { 0.011, 0.014, 0.018, 0.028, 0.038, 0.048 } },
        { "Heavy .012",        6, { 0.012, 0.016, 0.020, 0.032, 0.042, 0.052 } },
        { "Acoustic Light",    6, { 0.012, 0.016, 0.024, 0.032, 0.042, 0.053 } },
        { "Acoustic Medium",   6, { 0.013, 0.017, 0.026, 0.035, 0.045, 0.056 } },
        { "Classical Normal",  6, { 0.028, 0.032, 0.040, 0.029, 0.035, 0.043 } },
        { "Classical Hard",    6, { 0.029, 0.033, 0.041, 0.030, 0.036, 0.044 } },
        { "Bass .045",         5, { 0.045, 0.065, 0.085, 0.105, 0.130 } },
        { "Bass .050",         5, { 0.050, 0.070, 0.090, 0.110, 0.135 } },
        { "Custom",            6, { 0.010, 0.013, 0.017, 0.026, 0.036, 0.046 } }
    };

    /** A wire is wound once it gets too thick to stay flexible as a solid core. */
    bool diameterIsWound (double inches) noexcept { return inches >= 0.0205; }

    /** Age changes three things: the winding fills with skin oil and grime (duller),
        the metal work-hardens and the windings loosen (less sustain), and the
        string stops holding pitch. */
    struct AgeEffect { double sustain; double brightness; double detuneCents; double squeak; };

    const AgeEffect kAgeEffects[(size_t) StringAge::NumAges] =
    {
        { 1.00, 1.00, 0.0, 1.20 },   // Fresh: brightest, squeakiest
        { 0.92, 0.80, 1.5, 1.00 },   // Broken in
        { 0.66, 0.48, 5.0, 0.62 }    // Old: dull, short, drifts sharp/flat
    };
}

//==============================================================================
const MaterialProperties& StringMaterials::get (StringMaterial m) noexcept
{
    const auto i = (size_t) juce::jlimit (0, (int) StringMaterial::NumMaterials - 1, (int) m);
    return kMaterials[i];
}

const char* StringMaterials::getMaterialName (StringMaterial m) noexcept
{
    return get (m).name;
}

const char* StringMaterials::getGaugeName (StringGauge g) noexcept
{
    const auto i = (size_t) juce::jlimit (0, (int) StringGauge::NumGauges - 1, (int) g);
    return kGauges[i].name;
}

const char* StringMaterials::getAgeName (StringAge a) noexcept
{
    switch (a)
    {
        case StringAge::Fresh:    return "Fresh";
        case StringAge::BrokenIn: return "Broken In";
        case StringAge::Old:      return "Old";
        case StringAge::NumAges:
        default:                  return "Broken In";
    }
}

int StringMaterials::getGaugeStringCount (StringGauge g) noexcept
{
    const auto i = (size_t) juce::jlimit (0, (int) StringGauge::NumGauges - 1, (int) g);
    return kGauges[i].count;
}

double StringMaterials::getGaugeDiameterInches (StringGauge g, int stringIndex) noexcept
{
    const auto gi = (size_t) juce::jlimit (0, (int) StringGauge::NumGauges - 1, (int) g);
    const auto& set = kGauges[gi];

    if (stringIndex < set.count)
        return set.d[stringIndex];

    // Extended range beyond the set: extrapolate the last step geometrically so a
    // 7th or 8th string gets a sensibly thicker wire.
    const double last = set.d[set.count - 1];
    const double prev = set.d[juce::jmax (0, set.count - 2)];
    const double ratio = (prev > 0.0) ? juce::jlimit (1.05, 1.5, last / prev) : 1.25;

    return last * std::pow (ratio, (double) (stringIndex - set.count + 1));
}

//==============================================================================
StringSpec StringMaterials::computeSpec (StringMaterial material,
                                         StringGauge gauge,
                                         StringAge age,
                                         int stringIndex,
                                         double targetHz,
                                         double scaleLengthMm,
                                         double diameterInchesOverride) noexcept
{
    const auto& mat = get (material);
    const auto& ageFx = kAgeEffects[(size_t) juce::jlimit (0, (int) StringAge::NumAges - 1, (int) age)];

    StringSpec spec;

    // ---- geometry -----------------------------------------------------------
    const double inches = (diameterInchesOverride > 0.0)
                            ? diameterInchesOverride
                            : getGaugeDiameterInches (gauge, stringIndex);

    spec.diameterInches = juce::jlimit (0.005, 0.200, inches);
    spec.diameterMm = spec.diameterInches * kInchToMm;

    // Nylon sets use plain trebles (strings 0-2) and wound basses regardless of
    // diameter; flatwound and halfwound sets wind everything.
    bool wound = diameterIsWound (spec.diameterInches);

    if (mat.plainTrebles)
        wound = (stringIndex >= 3);
    else if (mat.alwaysWound)
        wound = true;

    spec.wound = wound;
    spec.coreDiameterMm = wound ? spec.diameterMm * mat.coreRatio : spec.diameterMm;

    // ---- linear mass density -------------------------------------------------
    // mu = rho * pi * r^2, reduced for wound strings because the winding does not
    // fill the cylinder the way a solid wire does.
    const double radiusM = spec.diameterMm * 0.0005;   // mm -> m, then /2
    const double density = wound ? mat.woundDensityKgM3 : mat.densityKgM3;
    const double solidMu = density * constants::kPi * radiusM * radiusM;
    spec.linearDensity = solidMu * (wound ? mat.woundMassFactor : 1.0);

    // ---- tension -------------------------------------------------------------
    // The vibrating-string equation, solved for T:  f = (1 / 2L) * sqrt(T / mu)
    const double lengthM = juce::jmax (0.05, scaleLengthMm * 0.001);
    const double hz = juce::jmax (1.0, targetHz);
    const double twoLf = 2.0 * lengthM * hz;
    spec.tensionNewtons = spec.linearDensity * twoLf * twoLf;

    // ---- inharmonicity -------------------------------------------------------
    // B = pi^3 * E * d^4 / (64 * T * L^2), where d is the *core* diameter: only
    // the core resists bending, which is why a wound low E is far less stiff than
    // a solid wire of the same outside diameter would be.
    const double dCoreM = spec.coreDiameterMm * 0.001;
    const double d4 = dCoreM * dCoreM * dCoreM * dCoreM;
    const double denom = 64.0 * juce::jmax (1.0, spec.tensionNewtons) * lengthM * lengthM;

    spec.inharmonicityB = juce::jlimit (1.0e-6, 5.0e-3,
                                        (constants::kPi * constants::kPi * constants::kPi)
                                        * mat.youngsModulusPa * d4 / denom);

    // ---- decay and brightness -------------------------------------------------
    // Longer scale lengths sustain a little better; heavier strings sustain more
    // than light ones because they carry more energy for the same displacement.
    const double scaleBonus = std::pow (scaleLengthMm / kReferenceScaleMm, 0.5);
    const double massBonus = std::pow (juce::jlimit (0.3, 3.0, spec.linearDensity / 0.004), 0.16);

    spec.sustainSeconds = mat.sustainSeconds * ageFx.sustain * scaleBonus * massBonus;
    spec.sustainSeconds = juce::jlimit (0.4, 20.0, spec.sustainSeconds);

    // Thicker strings are darker: the same loop filter cutoff on a .046 and a .009
    // would make the wound string far too bright.
    const double gaugeDarkening = std::pow (juce::jlimit (0.2, 5.0, 0.013 / spec.diameterInches), 0.28);

    spec.brightnessHz = mat.brightnessHz * ageFx.brightness * gaugeDarkening;
    spec.brightnessHz = juce::jlimit (600.0, 12000.0, spec.brightnessHz);

    // ---- noise and tuning stability -------------------------------------------
    spec.squeak = mat.squeakFactor * ageFx.squeak * (wound ? 1.0 : 0.20);
    spec.ageDetuneCents = ageFx.detuneCents;

    return spec;
}

//==============================================================================
StringEngine::Physical StringMaterials::toPhysical (const StringSpec& spec, double scaleLengthMm) noexcept
{
    StringEngine::Physical p;

    p.scaleLengthMm    = scaleLengthMm;
    p.diameterMm       = spec.diameterMm;
    p.linearDensity    = spec.linearDensity;
    p.inharmonicityB   = spec.inharmonicityB;
    p.sustainSeconds   = spec.sustainSeconds;
    p.openBrightnessHz = spec.brightnessHz;
    p.wound            = spec.wound;

    // Young's modulus is carried through for the docs and the validator; the
    // engine consumes it only via inharmonicityB.
    p.youngsModulus = 2.0e11;

    // Heavier strings drive the bridge harder, so they feed the sympathetic
    // coupling network more strongly.
    p.couplingSend = juce::jlimit (0.4, 1.6, std::pow (spec.linearDensity / 0.004, 0.30));

    // body-coupling.md 3: the string's characteristic impedance at the bridge.
    p.waveImpedance = std::sqrt (juce::jmax (0.0, spec.tensionNewtons) * juce::jmax (0.0, spec.linearDensity));

    return p;
}

//==============================================================================
double StringMaterials::suggestDiameterInches (StringMaterial material,
                                               double targetHz,
                                               double scaleLengthMm,
                                               bool wound) noexcept
{
    const auto& mat = get (material);

    // Invert the tension equation for the diameter that lands mid-range.
    const auto range = getTensionRange (scaleLengthMm);
    const double targetTension = 0.5 * (range.comfortableMin + range.comfortableMax);
    const double lengthM = juce::jmax (0.05, scaleLengthMm * 0.001);
    const double twoLf = 2.0 * lengthM * juce::jmax (1.0, targetHz);

    const double muNeeded = targetTension / juce::jmax (1.0e-9, twoLf * twoLf);
    const double effectiveDensity = (wound ? mat.woundDensityKgM3 * mat.woundMassFactor
                                           : mat.densityKgM3);

    const double radiusM = std::sqrt (muNeeded / (effectiveDensity * constants::kPi));

    return juce::jlimit (0.005, 0.200, radiusM * 2000.0 / kInchToMm);
}

} // namespace luthier
