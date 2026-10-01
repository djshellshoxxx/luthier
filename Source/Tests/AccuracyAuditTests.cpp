/*  Accuracy audit of the existing instrument models
    (docs/audit/ACCURACY_AUDIT_EXISTING.md).

    Two kinds of test live here:

      - the regression tests for the audit's applied fixes, each citing the
        reference value it holds the model to;
      - AccuracyAudit.dumpDerivedValues, which prints what mapSpec and the body
        model actually produce for every factory guitar when the environment
        variable LUTHIER_AUDIT_DUMP is set (silent otherwise), so the audit's
        "modelled value" column can be regenerated rather than retyped.
*/

#include "TestFramework.h"

#include "../Model/Workshop/PartAcoustics.h"
#include "../Model/Guitar/BodyModels.h"
#include "../Model/Guitar/StringMaterials.h"

#include <cstdio>
#include <cstdlib>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    PartLibrary& auditLibrary()
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

    WorkshopGuitar auditFactory (const juce::String& relativePath)
    {
        WorkshopGuitar g;
        PartLibrary::LoadReport report;
        auditLibrary().loadGuitar (PartLibrary::getFactoryGuitarsFolder().getChildFile (relativePath), g, report);
        return g;
    }
}

//==============================================================================
LUTHIER_TEST (AccuracyAudit, dumpDerivedValues)
{
    if (std::getenv ("LUTHIER_AUDIT_DUMP") == nullptr)
    {
        CHECK (true);
        return;
    }

    auto files = PartLibrary::getFactoryGuitarsFolder().findChildFiles (juce::File::findFiles, true, "*.luthierguitar");
    files.sort();

    for (const auto& f : files)
    {
        WorkshopGuitar g;
        PartLibrary::LoadReport report;
        auditLibrary().loadGuitar (f, g, report);
        const auto d = mapSpec (g);

        std::printf ("\n### %s  (%s)\n", g.name.toRawUTF8(), f.getParentDirectory().getFileName().toRawUTF8());
        std::printf ("scale %.1f mm, strings %d, tuning %d, shape %s, top %s, back %s, bracing %s\n",
                     d.spec.scaleLengthMm, d.numStrings, (int) d.spec.tuning,
                     BodyModels::getShapeName (d.body.shape), BodyModels::getWoodName (d.body.topWood),
                     BodyModels::getWoodName (d.body.backWood), BodyModels::getBracingName (d.body.bracing));
        std::printf ("topThk %.1f  scaleW %.2f  scaleD %.2f  air(BodyModels) %.1f Hz  air(mapSpec, unused) %.1f Hz  topFund %.1f Hz\n",
                     d.body.topThicknessMm, d.body.scaleWidth, d.body.scaleDepth,
                     BodyModels::computeAirResonance (d.body), d.airResonanceHz, BodyModels::computeTopFundamental (d.body));

        std::vector<BodyMode> modes;
        BodyModels::buildModes (d.body, modes);
        std::printf ("modes (%d):", (int) modes.size());

        for (size_t i = 0; i < juce::jmin ((size_t) 10, modes.size()); ++i)
            std::printf (" %.0f/Q%.0f/g%.2f", modes[i].frequencyHz, modes[i].q, modes[i].gain);

        std::printf ("\ncoupling %.2f  termMass %.0f g  sustain %.2f  feedback %.2f\n",
                     d.couplingFraction, d.terminationMassG, d.sustainScale, d.feedbackGain);

        double open[12] {};
        TuningEngine::getPresetFrequencies (d.spec.tuning, open, juce::jmin (6, d.numStrings));

        for (int s = 0; s < juce::jmin (12, d.numStrings); ++s)
        {
            const double hz = s < 6 ? open[s] : open[5] * std::pow (2.0, -5.0 * (s - 5) / 12.0);
            const auto spec = StringMaterials::computeSpec (d.stringMaterial, d.spec.stringGauge, StringAge::Fresh,
                                                            s, hz, d.spec.scaleLengthMm, d.gaugesIn[(size_t) s], -1);
            std::printf ("  s%d %.3f\" %s %.1f Hz  T %.1f N (%.1f lb)  mu %.3e  B %.2e\n", s, spec.diameterInches,
                         spec.wound ? "w" : "p", hz, spec.tensionNewtons, spec.tensionNewtons / 4.448,
                         spec.linearDensity, spec.inharmonicityB);
        }

        for (int p = 0; p < d.numPickups; ++p)
            std::printf ("  pickup slot %d type %d pos %.1f mm (%.3f)  L %.2f H  R %.1fk  C %.0f pF  h %.1f\n", p,
                         (int) d.pickups[(size_t) p].spec.type, d.pickups[(size_t) p].positionMm,
                         d.pickups[(size_t) p].spec.position, d.pickups[(size_t) p].spec.inductanceHenries,
                         d.pickups[(size_t) p].spec.resistanceKOhm, d.pickups[(size_t) p].spec.capacitancePf,
                         d.pickups[(size_t) p].heightMm);
    }

    CHECK (files.size() > 0);
}

//==============================================================================
//  Applied fixes (docs/audit/ACCURACY_AUDIT_EXISTING.md, "Fixes applied")
//==============================================================================
LUTHIER_TEST (AccuracyAudit, baritoneIsTunedToBStandard)
{
    // A-03: a baritone is tuned a fourth low. At E standard the factory 14-68
    // set on 686 mm sat at 150-185 N a string, about double what a baritone
    // set is designed for at B (roughly 17-24 lb, 75-105 N, per string).
    const auto d = mapSpec (auditFactory ("Electric/Baritone Electric.luthierguitar"));

    CHECK (d.spec.tuning == TuningPreset::BaritoneB);

    for (int s = 0; s < 6; ++s)
        CHECK_MSG (d.tensionNewtons[(size_t) s] > 50.0 && d.tensionNewtons[(size_t) s] < 115.0,
                   "baritone string " + juce::String (s) + " at " + juce::String (d.tensionNewtons[(size_t) s], 1) + " N");
}

LUTHIER_TEST (AccuracyAudit, flamencaGetsTheFlamencoBody)
{
    // A-07: baseTypeFor already made the Flamenca Blanca a Flamenco type by its
    // name; its body shape now follows the same test.
    const auto d = mapSpec (auditFactory ("Classical/Flamenca Blanca.luthierguitar"));
    CHECK (d.body.shape == BodyShape::Flamenco);
    CHECK (mapSpec (auditFactory ("Classical/Classical.luthierguitar")).body.shape == BodyShape::Classical);
}

LUTHIER_TEST (AccuracyAudit, bronzeSetsHavePlainSteelTreblesAndSteelCores)
{
    // A-05: D'Addario EJ16 (12-53 phosphor bronze) lists plain STEEL .012 and
    // .016, and its published high-E tension is 23.36 lb (103.9 N) at 25.5".
    // The wrap metal adds mass to the wound strings only.
    const double e4 = 329.63, scale = 647.7;

    for (auto m : { StringMaterial::PhosphorBronze, StringMaterial::Bronze8020, StringMaterial::SilkAndSteel,
                    StringMaterial::PureNickel, StringMaterial::Cobalt })
    {
        const auto plain = StringMaterials::computeSpec (m, StringGauge::AcousticLight, StringAge::Fresh, 0, e4, scale, 0.012, -1);
        const auto steel = StringMaterials::computeSpec (StringMaterial::NickelPlatedSteel, StringGauge::AcousticLight,
                                                         StringAge::Fresh, 0, e4, scale, 0.012, -1);
        CHECK_NEAR (plain.linearDensity, steel.linearDensity, steel.linearDensity * 0.01);
        CHECK_NEAR (plain.inharmonicityB, steel.inharmonicityB, steel.inharmonicityB * 0.03);
    }

    const auto ej16e = StringMaterials::computeSpec (StringMaterial::PhosphorBronze, StringGauge::AcousticLight,
                                                     StringAge::Fresh, 0, e4, scale, 0.012, -1);
    CHECK_MSG (std::abs (ej16e.tensionNewtons / 4.448 - 23.36) / 23.36 < 0.04,
               "EJ16 .012 at E4 on 25.5\": " + juce::String (ej16e.tensionNewtons / 4.448, 2) + " lb, published 23.36");

    // A bronze wound string is still heavier than the same gauge in nickel.
    const auto pbWound = StringMaterials::computeSpec (StringMaterial::PhosphorBronze, StringGauge::AcousticLight,
                                                       StringAge::Fresh, 5, 82.41, scale, 0.053, -1);
    const auto npsWound = StringMaterials::computeSpec (StringMaterial::NickelPlatedSteel, StringGauge::AcousticLight,
                                                        StringAge::Fresh, 5, 82.41, scale, 0.053, -1);
    CHECK (pbWound.linearDensity > npsWound.linearDensity);
}

LUTHIER_TEST (AccuracyAudit, p90InductanceIsInTheMeasuredRange)
{
    // A-11: vintage P-90s measure about 6 H, a modern 8.6k one 6.5 H
    // (guitar.com "All About P-90s"; Seymour Duncan, "Inductance").
    auto part = auditLibrary().find (PartType::pickup, "P90 Alnico 5 8.2k");

    if (part == nullptr)
    {
        CHECK_MSG (false, "P90 part not found");
        return;
    }

    const double l = part->number ("inductance_h", 0.0);
    CHECK_MSG (l >= 5.5 && l <= 8.0, "P90 inductance " + juce::String (l, 2) + " H");
}

LUTHIER_TEST (AccuracyAudit, archtopIsParallelBraced)
{
    // A-08: carved and laminated archtops use parallel tone bars, not an
    // acoustic flat-top's X.
    const auto d = mapSpec (auditFactory ("Electric/Full Hollow Archtop.luthierguitar"));
    CHECK (d.body.bracing == Bracing::HollowParallel);
}
