#include "BodyModels.h"

namespace luthier
{

namespace
{
    constexpr double kSpeedOfSound = 343.0;      // m/s at 20 C
    constexpr double kPoissonRatio = 0.30;

    //==========================================================================
    // name, density, Young's modulus, loss factor, brightness tilt (dB)
    const WoodProperties kWoods[(size_t) Wood::NumWoods] =
    {
        { "Sitka Spruce", 430.0, 11.0e9, 0.0080,  1.8 },
        { "Cedar",        350.0,  8.0e9, 0.0105,  0.2 },
        { "Mahogany",     510.0, 10.0e9, 0.0120, -0.8 },
        { "Maple",        650.0, 12.0e9, 0.0090,  1.5 },
        { "Alder",        450.0,  9.5e9, 0.0130,  0.4 },
        { "Ash",          600.0, 11.5e9, 0.0110,  1.2 },
        { "Rosewood",     850.0, 14.0e9, 0.0070,  0.9 },
        { "Koa",          610.0, 10.5e9, 0.0110,  0.6 },
        { "Basswood",     420.0,  8.5e9, 0.0160, -0.6 },
        { "Korina",       540.0, 10.0e9, 0.0120, -0.2 },
        { "Poplar",       450.0,  9.0e9, 0.0140, -0.3 },
        { "Walnut",       640.0, 11.0e9, 0.0100,  0.5 },
        { "Ebony",       1100.0, 16.0e9, 0.0060,  2.2 },
        { "Sapele",       640.0, 10.8e9, 0.0105,  0.3 },
        { "Agathis",      480.0,  9.2e9, 0.0155, -0.9 },
        { "Nato",         560.0, 10.2e9, 0.0125, -0.5 }
    };

    //==========================================================================
    // name, lowerBout, depth, soundHole, topThk, backThk, volume(L), acoustic
    const BodyShapeProperties kShapes[(size_t) BodyShape::NumShapes] =
    {
        { "Parlor",          336.0,  95.0,  92.0, 2.5, 2.6,  9.5, true  },
        { "Concert",         368.0, 105.0,  98.0, 2.6, 2.7, 12.5, true  },
        { "Auditorium / OM", 381.0, 110.0, 100.0, 2.7, 2.8, 14.5, true  },
        { "Dreadnought",     397.0, 121.0, 102.0, 2.8, 2.9, 17.5, true  },
        { "Jumbo",           432.0, 127.0, 102.0, 2.9, 3.0, 22.0, true  },
        { "Classical",       368.0, 100.0,  86.0, 2.2, 2.4, 12.0, true  },
        { "Flamenco",        365.0,  88.0,  86.0, 1.9, 2.1, 10.0, true  },
        { "Resonator",       390.0, 105.0,   0.0, 0.6, 1.2, 13.0, true  },
        { "12-String Dread", 400.0, 122.0, 102.0, 2.9, 3.0, 18.0, true  },

        { "Solid (Thin)",    330.0,  38.0,   0.0, 0.0, 0.0,  0.0, false },
        { "Solid",           330.0,  45.0,   0.0, 0.0, 0.0,  0.0, false },
        { "Solid (Heavy)",   330.0,  52.0,   0.0, 0.0, 0.0,  0.0, false },
        { "Semi-Hollow",     406.0,  42.0,   0.0, 4.5, 4.5,  4.0, false },
        { "Hollow",          406.0,  75.0,   0.0, 4.0, 4.0,  9.0, false },
        { "Offset",          340.0,  42.0,   0.0, 0.0, 0.0,  0.0, false },
        { "Chambered",       330.0,  48.0,   0.0, 5.0, 0.0,  1.8, false },

        { "Bass Solid",      356.0,  45.0,   0.0, 0.0, 0.0,  0.0, false },
        { "Bass Hollow",     400.0,  70.0,  95.0, 3.2, 3.4, 11.0, false }
    };

    /** Clamped plate-mode coefficients (lambda) for a circular plate, used as the
        skeleton of the top and back plate mode series. */
    const double kPlateLambda[] =
    {
        10.2158, 21.2604, 34.8770, 39.7710, 51.0300, 60.8280,
        69.6660, 84.5830, 94.1000, 108.720, 122.440, 139.050
    };

    double plateModeFrequency (double lambda, double thicknessMm, double radiusMm,
                               double youngs, double density) noexcept
    {
        const double t = juce::jmax (0.3, thicknessMm) * 0.001;
        const double a = juce::jmax (0.05, radiusMm * 0.001);

        // f = (lambda / 2*pi) * (t / a^2) * sqrt(E / (12 * rho * (1 - v^2)))
        const double speed = std::sqrt (youngs / (12.0 * density * (1.0 - kPoissonRatio * kPoissonRatio)));

        return (lambda / constants::kTwoPi) * (t / (a * a)) * speed;
    }
}

//==============================================================================
const WoodProperties& BodyModels::getWood (Wood w) noexcept
{
    return kWoods[(size_t) juce::jlimit (0, (int) Wood::NumWoods - 1, (int) w)];
}

const BodyShapeProperties& BodyModels::getShape (BodyShape s) noexcept
{
    return kShapes[(size_t) juce::jlimit (0, (int) BodyShape::NumShapes - 1, (int) s)];
}

const char* BodyModels::getWoodName (Wood w) noexcept   { return getWood (w).name; }
const char* BodyModels::getShapeName (BodyShape s) noexcept { return getShape (s).name; }

const char* BodyModels::getBracingName (Bracing b) noexcept
{
    switch (b)
    {
        case Bracing::XBrace:           return "X-Brace";
        case Bracing::ForwardShiftedX:  return "Forward-Shifted X";
        case Bracing::FanBrace:         return "Fan Brace";
        case Bracing::LadderBrace:      return "Ladder Brace";
        case Bracing::VBrace:           return "V-Brace";
        case Bracing::SolidBodyNone:    return "Solid Body";
        case Bracing::SemiHollowBlock:  return "Centre Block";
        case Bracing::HollowParallel:   return "Parallel Braces";
        case Bracing::NumBracings:
        default:                        return "X-Brace";
    }
}

double BodyModels::getBracingStiffness (Bracing b) noexcept
{
    switch (b)
    {
        case Bracing::XBrace:          return 1.00;
        case Bracing::ForwardShiftedX: return 0.94;   // looser top, more bass
        case Bracing::FanBrace:        return 0.88;   // classical, very responsive
        case Bracing::LadderBrace:     return 0.82;   // old parlour, midrange bark
        case Bracing::VBrace:          return 1.06;   // modern, stiff and focused
        case Bracing::SolidBodyNone:   return 3.20;   // a plank: modes are way up
        case Bracing::SemiHollowBlock: return 1.85;
        case Bracing::HollowParallel:  return 1.20;
        case Bracing::NumBracings:
        default:                       return 1.00;
    }
}

//==============================================================================
double BodyModels::computeAirResonance (const BodyConfig& cfg) noexcept
{
    const auto& shape = getShape (cfg.shape);

    const double holeMm = shape.soundHoleMm * juce::jlimit (0.2, 2.0, cfg.soundHoleScale);

    if (holeMm < 1.0 || shape.volumeLitres < 0.1)
        return 0.0;

    const double radiusM = holeMm * 0.0005;
    const double area = constants::kPi * radiusM * radiusM;

    // Volume scales with width squared and depth.
    const double volume = shape.volumeLitres * 0.001
                          * cfg.scaleWidth * cfg.scaleWidth
                          * juce::jlimit (0.3, 3.0, cfg.scaleDepth);

    const double topThk = (cfg.topThicknessMm > 0.0 ? cfg.topThicknessMm : shape.topThicknessMm) * 0.001;

    // Effective neck length of the "port": plate thickness plus the end correction.
    const double effectiveLength = topThk + 1.7 * radiusM;

    if (volume <= 0.0 || effectiveLength <= 0.0)
        return 0.0;

    const double f = (kSpeedOfSound / constants::kTwoPi)
                     * std::sqrt (area / (volume * effectiveLength));

    return juce::jlimit (40.0, 400.0, f * cfg.resonanceTrim);
}

double BodyModels::computeTopFundamental (const BodyConfig& cfg) noexcept
{
    const auto& shape = getShape (cfg.shape);
    const auto& wood = getWood (cfg.topWood);

    const double radiusMm = shape.lowerBoutMm * 0.5 * juce::jlimit (0.5, 2.0, cfg.scaleWidth);
    const double thicknessMm = (cfg.topThicknessMm > 0.0 ? cfg.topThicknessMm
                                                         : juce::jmax (0.8, shape.topThicknessMm));

    const double base = plateModeFrequency (kPlateLambda[0], thicknessMm, radiusMm,
                                            wood.youngsModulusPa, wood.densityKgM3);

    return juce::jlimit (60.0, 1200.0, base * getBracingStiffness (cfg.bracing) * cfg.resonanceTrim);
}

//==============================================================================
void BodyModels::buildModes (const BodyConfig& cfg, std::vector<BodyMode>& dest)
{
    dest.clear();
    dest.reserve ((size_t) kMaxModes);

    const auto& shape = getShape (cfg.shape);
    const auto& topWood = getWood (cfg.topWood);
    const auto& backWood = getWood (cfg.backWood);
    const auto& sideWood = getWood (cfg.sideWood);

    const double widthScale = juce::jlimit (0.5, 2.0, cfg.scaleWidth);
    const double radiusMm = shape.lowerBoutMm * 0.5 * widthScale;

    const double topThk = (cfg.topThicknessMm > 0.0 ? cfg.topThicknessMm
                                                    : juce::jmax (0.8, shape.topThicknessMm));
    const double backThk = juce::jmax (0.8, shape.backThicknessMm > 0.0 ? shape.backThicknessMm : topThk);

    const double stiffness = getBracingStiffness (cfg.bracing);

    // Age: seasoned wood has lost some of its internal damping, so every mode
    // gets a higher Q - the "opened up" quality of an old instrument.
    const double ageQ = 1.0 + juce::jlimit (0.0, 1.0, cfg.age) * 0.55;

    auto addMode = [&dest, &cfg] (double hz, double q, double gain)
    {
        if (dest.size() >= (size_t) kMaxModes)
            return;

        if (hz < 25.0 || hz > 18000.0 || gain <= 1.0e-5)
            return;

        BodyMode m;
        m.frequencyHz = hz * cfg.resonanceTrim;
        m.q = juce::jlimit (1.5, 220.0, q);
        m.gain = gain;
        dest.push_back (m);
    };

    // ---- 1. Air resonance and its coupled partner ---------------------------
    const double airHz = computeAirResonance (cfg);

    if (airHz > 0.0)
    {
        // The Helmholtz mode and the top's fundamental couple into a pair split
        // either side of the uncoupled frequencies; this is the classic guitar
        // "double resonance" in the low end.
        addMode (airHz, 16.0 * ageQ, 1.00);
        addMode (airHz * 1.62, 22.0 * ageQ, 0.42);

        // Long-air mode running the length of the box.
        const double lengthM = shape.lowerBoutMm * 0.0016 * widthScale;
        addMode (kSpeedOfSound / (2.0 * juce::jmax (0.15, lengthM)), 12.0 * ageQ, 0.22);
    }
    else
    {
        // A solid body has no cavity, but the plank still has a bending resonance
        // that colours the attack, and the neck has its own "dead spot" mode.
        addMode (168.0 / std::pow (widthScale, 0.8), 9.0 * ageQ, 0.30);
        addMode (243.0 / std::pow (widthScale, 0.8), 11.0 * ageQ, 0.22);
    }

    // ---- 2. Top plate modes --------------------------------------------------
    const int topModeCount = shape.acoustic ? 10 : 5;

    for (int i = 0; i < topModeCount; ++i)
    {
        const double f = plateModeFrequency (kPlateLambda[i], topThk, radiusMm,
                                             topWood.youngsModulusPa, topWood.densityKgM3)
                         * stiffness;

        // Q from the wood's loss factor: less internal damping means a longer,
        // more singing resonance.
        const double q = (1.0 / juce::jmax (1.0e-4, topWood.lossFactor)) * 0.42 * ageQ;

        // Higher modes radiate less efficiently.
        const double gain = 0.95 / (1.0 + 0.55 * (double) i);

        addMode (f, q, gain);
    }

    // ---- 3. Back plate modes -------------------------------------------------
    const int backModeCount = shape.acoustic ? 7 : 3;

    for (int i = 0; i < backModeCount; ++i)
    {
        const double f = plateModeFrequency (kPlateLambda[i], backThk, radiusMm * 0.98,
                                             backWood.youngsModulusPa, backWood.densityKgM3)
                         * 1.12;   // the back is stiffer and less loaded than the top

        const double q = (1.0 / juce::jmax (1.0e-4, backWood.lossFactor)) * 0.35 * ageQ;
        const double gain = 0.45 / (1.0 + 0.6 * (double) i);

        addMode (f, q, gain);
    }

    // ---- 4. Side and higher-order modes --------------------------------------
    // Above roughly 1 kHz the body response stops being a handful of clean modes
    // and becomes a dense, irregular thicket. A deterministic scatter reproduces
    // that far better than a neat harmonic series would.
    RtRandom rng { 0xB0D1E5ull
                   + (uint64_t) cfg.shape * 131ull
                   + (uint64_t) cfg.topWood * 17ull
                   + (uint64_t) cfg.backWood * 29ull };

    const double sideBase = plateModeFrequency (kPlateLambda[0], juce::jmax (1.2, shape.depthMm * 0.022),
                                                radiusMm * 0.55,
                                                sideWood.youngsModulusPa, sideWood.densityKgM3);

    const int scatterCount = shape.acoustic ? 18 : 10;
    double f = juce::jmax (700.0, sideBase);

    for (int i = 0; i < scatterCount; ++i)
    {
        f *= 1.16 + rng.nextDouble() * 0.22;

        if (f > 11000.0)
            break;

        const double q = (14.0 + rng.nextDouble() * 40.0) * ageQ;
        const double gain = (shape.acoustic ? 0.20 : 0.11)
                            * (0.45 + rng.nextDouble() * 0.55)
                            / (1.0 + f / 3200.0);

        addMode (f, q, gain);
    }

    // ---- 5. Normalise ---------------------------------------------------------
    // Keep the summed modal output at a predictable level so switching bodies is
    // a change of character, not a jump in volume.
    double gainSum = 0.0;

    for (const auto& m : dest)
        gainSum += m.gain;

    if (gainSum > 1.0e-6)
    {
        const double norm = 2.6 / gainSum;

        for (auto& m : dest)
            m.gain *= norm;
    }
}

//==============================================================================
juce::String BodyModels::irFileNameFor (const BodyConfig& cfg)
{
    auto sanitiseName = [] (const char* s)
    {
        return juce::String (s).toLowerCase()
                               .replaceCharacters (" /-&", "____")
                               .removeCharacters (".");
    };

    const auto size = (cfg.scaleWidth < 0.92) ? "small"
                    : (cfg.scaleWidth > 1.08) ? "large" : "medium";

    const auto age = (cfg.age < 0.33) ? "new" : (cfg.age < 0.66) ? "played" : "vintage";

    return sanitiseName (getShapeName (cfg.shape)) + "/"
           + juce::String (size) + "_"
           + sanitiseName (getWoodName (cfg.topWood)) + "_"
           + juce::String (age) + ".wav";
}

} // namespace luthier
