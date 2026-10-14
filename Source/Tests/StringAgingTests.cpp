/*  String aging (string-aging.md 10, SA-01 to SA-16).

    The model is tested where it lives: StringAging's pure compute() for the
    curves and anchors, a lone StringEngine for what a listener measures (T60,
    spectral centroid), and the whole LuthierEngine for the wiring (legacy
    null, accrual, no clicks).
*/

#include "TestFramework.h"

#include "../DSP/String/StringAging.h"
#include "../DSP/String/StringEngine.h"
#include "../Model/Guitar/StringMaterials.h"
#include "../Character/CharacterEngine.h"
#include "../Character/EnvironmentModel.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../PhysicalRange.h"

using namespace luthier;
using namespace luthier::tests;

// CircuitTests.cpp's per-thread allocation counter (engine.md 0).
long luthierAllocationCount() noexcept;   // CircuitTests.cpp

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    // The old kAgeEffects rows: sustain, brightness, detune.
    constexpr double kRows[3][3] = { { 1.00, 1.00, 0.0 }, { 0.92, 0.80, 1.5 }, { 0.66, 0.48, 5.0 } };
    constexpr double kAnchorHours[3] = { 0.0, 12.0, 120.0 };

    StringAging::Inputs inputs (double detail, StringCoating coating = StringCoating::none)
    {
        StringAging::Inputs in;
        in.detail = detail;
        in.coating = coating;
        return in;
    }

    //==========================================================================
    /** A lone string, plucked, for T60 and centroid measurements. */
    std::vector<double> pluckString (int stringIndex, double hz, const AgingFactors& f, double seconds)
    {
        const auto spec = StringMaterials::computeSpec (StringMaterial::NickelPlatedSteel, StringGauge::Regular,
                                                        StringAge::Fresh, stringIndex, hz, 648.0);
        StringEngine s;
        s.setIndex (stringIndex);
        s.prepare (kSr, kBlock);
        s.setPhysical (StringMaterials::toPhysical (spec, 648.0));
        s.setAgingFactors (f.brightness, f.sustain, f.dispersion);
        s.snapToFrequency (hz);

        Excitation::Params p;
        p.velocity = 0.9;
        p.pluckPosition = 0.16;
        s.excite (p);

        std::vector<double> out ((size_t) (seconds * kSr));

        for (auto& x : out)
            x = s.processSample (0.0);

        return out;
    }

    /** T60 from the slope of the 20 ms-window RMS in dB, fitted from 5 to 35 dB down. */
    double measureT60 (const std::vector<double>& x)
    {
        const int w = (int) (0.020 * kSr);
        std::vector<double> db;

        for (size_t i = 0; i + (size_t) w <= x.size(); i += (size_t) w)
        {
            double e = 0.0;

            for (int n = 0; n < w; ++n)
                e += x[i + (size_t) n] * x[i + (size_t) n];

            db.push_back (10.0 * std::log10 (juce::jmax (1.0e-30, e / w)));
        }

        const double peak = *std::max_element (db.begin(), db.end());
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        int n = 0;
        bool started = false;

        for (size_t i = 0; i < db.size(); ++i)
        {
            if (db[i] > peak - 5.0 && ! started)
                continue;

            started = true;

            if (db[i] < peak - 35.0)
                break;

            const double t = (double) i * 0.020;
            sx += t; sy += db[i]; sxx += t * t; sxy += t * db[i]; ++n;
        }

        if (n < 3)
            return 0.0;

        const double slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);
        return slope < 0.0 ? -60.0 / slope : 1.0e9;
    }

    double spectralCentroid (const std::vector<double>& x, double seconds)
    {
        constexpr int order = 15;
        const int size = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> buffer ((size_t) size * 2, 0.0f);

        const int n = juce::jmin ((int) (seconds * kSr), size, (int) x.size());

        for (int i = 0; i < n; ++i)
            buffer[(size_t) i] = (float) (x[(size_t) i] * (0.5 - 0.5 * std::cos (constants::kTwoPi * i / (n - 1))));

        fft.performFrequencyOnlyForwardTransform (buffer.data());

        double num = 0.0, den = 0.0;

        for (int k = 1; k < size / 2; ++k)
        {
            const double hz = k * kSr / size;
            num += hz * buffer[(size_t) k];
            den += buffer[(size_t) k];
        }

        return den > 0.0 ? num / den : 0.0;
    }

    //==========================================================================
    std::unique_ptr<LuthierEngine> makeEngine()
    {
        auto e = std::make_unique<LuthierEngine>();
        e->prepare (kSr, kBlock);
        e->setGuitarType (GuitarType::Stratocaster);
        e->getCharacterEngine().setEnabled (false);
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

    void pluck (LuthierEngine& e, int s, int fret = 0, double velocity = 0.8)
    {
        NoteOnEvent n;
        n.stringIndex = s;
        n.fretPosition = fret;
        n.velocity = velocity;
        n.midiNote = 40 + s;
        n.technique = Technique::Pluck;
        n.pitchHz = e.getTuningEngine().computeFrequency (s, (double) fret);
        e.triggerNoteNow (n);
    }

    void chord (LuthierEngine& e)
    {
        for (int s = 0; s < e.getNumStrings(); ++s)
            pluck (e, s, s == 0 || s == 5 ? 0 : 2);
    }

    double rmsDiff (const std::vector<double>& a, const std::vector<double>& b)
    {
        double e = 0.0;
        const size_t n = juce::jmin (a.size(), b.size());

        for (size_t i = 0; i < n; ++i)
            e += (a[i] - b[i]) * (a[i] - b[i]);

        return std::sqrt (e / (double) juce::jmax ((size_t) 1, n));
    }
}

//==============================================================================
LUTHIER_TEST (StringAging, SA01_legacyAnchorsAreExact)
{
    for (int row = 0; row < 3; ++row)
    {
        for (bool wound : { false, true })
        {
            for (int s = 0; s < 6; ++s)
            {
                const auto f = StringAging::compute (kAnchorHours[row], wound, false, 0.7, s, inputs (0.0));

                CHECK_NEAR (f.sustain, kRows[row][0], 1.0e-9);
                CHECK_NEAR (f.brightness, kRows[row][1], 1.0e-9);
                CHECK_NEAR (f.detuneCents, kRows[row][2] * StringAging::detuneSign (s), 1.0e-9);
                CHECK (f.dispersion == 1.0 && f.intonationCentsPerFret == 0.0 && f.squeakCentroid == 1.0);
            }
        }
    }
}

LUTHIER_TEST (StringAging, SA02_aLegacyPresetNullsAgainstThePreSpecPath)
{
    // New path: the loader's mapping of string_age = Old (120 h, detail 0).
    auto fresh = makeEngine();
    StringAging::Inputs in;
    in.hours = 120.0;
    in.detail = 0.0;
    fresh->getStringAging().setInputs (in);
    fresh->getStringAging().reset();
    renderBlocks (*fresh, 1);

    // Reference: the strings built the way refreshStringPhysics built them
    // before this spec - computeSpec (..., Old, ...) and the old detune draw.
    auto reference = makeEngine();
    StringAging::Inputs zero;
    zero.hours = 0.0;
    zero.detail = 0.0;
    reference->getStringAging().setInputs (zero);
    reference->getStringAging().reset();
    renderBlocks (*reference, 1);

    const auto& spec = reference->getGuitarSpec();

    for (int i = 0; i < reference->getNumStrings(); ++i)
    {
        const double openHz = reference->getTuningEngine().getEffectiveOpenFrequency (i);
        const auto s = StringMaterials::computeSpec (spec.stringMaterial, spec.stringGauge, StringAge::Old,
                                                     i, openHz, spec.scaleLengthMm);
        reference->getString (i).setPhysical (StringMaterials::toPhysical (s, spec.scaleLengthMm));

        RtRandom r { 0xA6E0000ull + (uint64_t) i };
        reference->getTuningEngine().setFineTuneCents (i, r.nextBipolar() * s.ageDetuneCents);
    }

    std::vector<double> a, b;
    chord (*fresh);
    chord (*reference);
    renderBlocks (*fresh, (int) (4.0 * kSr / kBlock), &a);
    renderBlocks (*reference, (int) (4.0 * kSr / kBlock), &b);

    const double diffDb = juce::Decibels::gainToDecibels (rmsDiff (a, b), -200.0);
    CHECK_MSG (diffDb < -60.0, "legacy null is only " + juce::String (diffDb, 1) + " dBFS");
    CHECK (rmsDiff (a, std::vector<double> (a.size(), 0.0)) > 1.0e-3);   // it did make sound
}

LUTHIER_TEST (StringAging, SA03_physicalCurvesMeetTheAnchors)
{
    // d = 1 with unit weights and no jitter: a 3 / 3 set's average string.
    for (int row = 1; row < 3; ++row)
    {
        const auto f = StringAging::computeWithWeights (kAnchorHours[row], 1.0, 1.0, false, 0, inputs (1.0));
        CHECK_NEAR (f.brightness / kRows[row][1], 1.0, 0.01);
        CHECK_NEAR (f.sustain / kRows[row][0], 1.0, 0.01);
        CHECK_NEAR (std::abs (f.detuneCents) / (kRows[row][2] * std::abs (StringAging::detuneSign (0))), 1.0, 0.01);
    }
}

LUTHIER_TEST (StringAging, SA04_everyCurveIsMonotonic)
{
    for (double d : { 0.0, 0.5, 1.0 })
        for (bool wound : { false, true })
            for (double j : { -1.0, 0.0, 1.0 })
                for (auto coating : { StringCoating::none, StringCoating::thick })
                {
                    AgingFactors last = StringAging::compute (0.0, wound, false, j, 2, inputs (d, coating));
                    bool ok = true;

                    for (int h = 1; h <= 2000; ++h)
                    {
                        const auto f = StringAging::compute ((double) h, wound, false, j, 2, inputs (d, coating));
                        ok = ok && f.brightness <= last.brightness && f.sustain <= last.sustain
                                && std::abs (f.detuneCents) >= std::abs (last.detuneCents)
                                && f.dispersion >= last.dispersion;
                        last = f;
                    }

                    CHECK_MSG (ok, "not monotonic at d " + juce::String (d) + (wound ? " wound" : " plain"));
                }
}

LUTHIER_TEST (StringAging, SA05_oldStringsLoseAThirdOfTheirSustain)
{
    // Open low E, index 5 of a six-string, uncoated, d = 1.
    const double hz = 82.407;
    const auto fresh = StringAging::compute (0.0, true, false, 0.0, 5, inputs (1.0));
    const auto old = StringAging::compute (120.0, true, false, 0.0, 5, inputs (1.0));

    const double t60Fresh = measureT60 (pluckString (5, hz, fresh, 8.0));
    const double t60Old = measureT60 (pluckString (5, hz, old, 8.0));
    const double shorter = 1.0 - t60Old / t60Fresh;

    CHECK_MSG (shorter >= 0.28 && shorter <= 0.40,
               "T60 " + juce::String (t60Fresh, 2) + " s -> " + juce::String (t60Old, 2) + " s: "
                 + juce::String (100.0 * shorter, 1) + " % shorter (28-40 %)");
}

LUTHIER_TEST (StringAging, SA06_oldStringsAreDuller)
{
    const double hz = 329.63;
    const auto fresh = StringAging::compute (0.0, false, false, 0.0, 0, inputs (1.0));
    const auto old = StringAging::compute (120.0, false, false, 0.0, 0, inputs (1.0));

    const double cFresh = spectralCentroid (pluckString (0, hz, fresh, 0.6), 0.5);
    const double cOld = spectralCentroid (pluckString (0, hz, old, 0.6), 0.5);

    CHECK_MSG (cOld <= 0.85 * cFresh, "centroid " + juce::String (cFresh, 0) + " Hz -> " + juce::String (cOld, 0) + " Hz");
}

LUTHIER_TEST (StringAging, SA07_woundStringsDullFaster)
{
    const auto lowE = StringAging::compute (24.0, true, false, 0.0, 5, inputs (1.0));
    const auto highE = StringAging::compute (24.0, false, false, 0.0, 0, inputs (1.0));
    CHECK_MSG (highE.brightness - lowE.brightness >= 0.10,
               "wound " + juce::String (lowE.brightness, 3) + " vs plain " + juce::String (highE.brightness, 3));
}

LUTHIER_TEST (StringAging, SA08_coatingIsARate)
{
    for (bool wound : { false, true })
    {
        const auto coated = StringAging::compute (100.0, wound, false, 0.3, 1, inputs (1.0, StringCoating::thick));
        const auto bare = StringAging::compute (25.0, wound, false, 0.3, 1, inputs (1.0));

        CHECK_NEAR (coated.contaminationHours, bare.contaminationHours, 1.0e-9);
        CHECK_NEAR (coated.corrosionHours, bare.corrosionHours, 1.0e-9);
        CHECK_NEAR (coated.fatigueHours, 100.0, 1.0e-9);
    }
}

LUTHIER_TEST (StringAging, SA09_restringOneMakesOnlyThatStringNew)
{
    StringAging aging;
    aging.prepare (kSr);
    aging.setNumStrings (6);

    for (int s = 0; s < 6; ++s)
    {
        aging.setStringInfo (s, s >= 3, false);
        aging.setJitter (s, 0.1 * s - 0.2);
    }

    auto in = inputs (1.0);
    in.hours = 80.0;
    aging.setInputs (in);
    aging.reset();
    aging.advance (0.01, nullptr, 0);

    std::array<AgingFactors, 6> before {};

    for (int s = 0; s < 6; ++s)
        before[(size_t) s] = aging.getFactors (s);

    aging.requestRestring (3);
    CHECK (aging.advance (0.01, nullptr, 0));

    const auto zero = StringAging::compute (0.0, true, false, 0.1 * 3 - 0.2, 3, in);
    CHECK (aging.getFactors (3).brightness == zero.brightness && aging.getFactors (3).sustain == zero.sustain
           && aging.getFactors (3).detuneCents == zero.detuneCents);

    for (int s = 0; s < 6; ++s)
        if (s != 3)
            CHECK (std::memcmp (&aging.getFactors (s), &before[(size_t) s], sizeof (AgingFactors)) == 0);

    // It is as old as the set was when it went on: 20 h later it is 20 h old.
    in.hours = 100.0;
    aging.setInputs (in);
    aging.reset();
    CHECK_NEAR (aging.getFactors (3).hours, 20.0, 1.0e-9);
}

LUTHIER_TEST (StringAging, SA10_theSeedIsDeterministic)
{
    auto factorsFor = [] (uint64_t seed)
    {
        CharacterEngine character;
        character.setSeed (seed);

        std::array<double, 6> j {};
        std::array<AgingFactors, 6> f {};

        for (int s = 0; s < 6; ++s)
        {
            j[(size_t) s] = 2.0 * character.hashedValue (CharacterEngine::kCategoryStringAge, s) - 1.0;
            f[(size_t) s] = StringAging::compute (60.0, s >= 3, false, j[(size_t) s], s, inputs (1.0));
        }

        return std::make_pair (j, f);
    };

    const auto a = factorsFor (0x1234ull), b = factorsFor (0x1234ull), c = factorsFor (0x4321ull);

    CHECK (std::memcmp (a.second.data(), b.second.data(), sizeof (AgingFactors) * 6) == 0);
    CHECK (a.first != c.first);

    for (double j : a.first)
        CHECK (j >= -1.0 && j <= 1.0);
}

LUTHIER_TEST (StringAging, SA11_accrualIsPlayedTime)
{
    // Off: ten minutes of playing accrues nothing.
    {
        StringAging aging;
        aging.prepare (kSr);
        const std::array<double, 6> loud { 0.5, 0.5, 0.5, 0.5, 0.5, 0.5 };

        for (int b = 0; b < (int) (600.0 * kSr / kBlock); ++b)
            aging.advance (kBlock / kSr, loud.data(), 6);

        for (int s = 0; s < 6; ++s)
            CHECK (aging.getAccruedHours (s) == 0.0);
    }

    // x100: 36 s of one string, re-plucked every 2 s, is an hour on that string.
    auto engine = makeEngine();
    StringAging::Inputs in;
    in.accrual = AgeAccrual::x100;
    engine->getStringAging().setInputs (in);
    engine->getCouplingMatrix().setAmount (0.0);   // no sympathetic ring: the others really are silent

    const int blocksPer2s = (int) (2.0 * kSr / kBlock);

    // The low E, sustained; the fretting hand keeps the others quiet, or they
    // ring sympathetically - which is playing them, and would accrue.
    for (int b = 0; b < (int) (36.0 * kSr / kBlock); ++b)
    {
        if (b % blocksPer2s == 0)
            pluck (*engine, 5, 0, 1.0);

        for (int s = 0; s < 5; ++s)
            engine->getString (s).setDamping (StringEngine::Damping::Choked, 1.0);

        renderBlocks (*engine, 1);
    }

    const double hours = engine->getStringAging().getAccruedHours (5);
    CHECK_MSG (std::abs (hours - 1.0) <= 0.03, "accrued " + juce::String (hours, 4) + " h");

    for (int s = 0; s < 5; ++s)
        CHECK_MSG (engine->getStringAging().getAccruedHours (s) == 0.0,
                   "silent string " + juce::String (s) + " accrued " + juce::String (engine->getStringAging().getAccruedHours (s), 6));
}

LUTHIER_TEST (StringAging, SA12_humidityDrivesCorrosion)
{
    auto dry = inputs (1.0), humid = inputs (1.0);
    humid.kRH = 1.0 + 0.03 * (80.0 - 45.0);
    CHECK_NEAR (humid.kRH, 2.05, 1.0e-12);

    const auto a = StringAging::compute (50.0, false, false, 0.0, 0, dry);
    const auto b = StringAging::compute (50.0, false, false, 0.0, 0, humid);

    CHECK_NEAR (b.corrosionHours / a.corrosionHours, 2.05, 1.0e-9);
    CHECK (b.sustain < a.sustain);

    // With the environment at its defaults, k_RH is exactly 1.
    EnvironmentModel env;
    env.prepare (kSr);
    env.advance (0.01, -1.0, false);
    CHECK (env.getState().corrosionRate == 1.0);
}

LUTHIER_TEST (StringAging, SA13_squeakReconciliation)
{
    const auto old = StringAging::computeWithWeights (120.0, 1.0, 1.0, false, 0, inputs (1.0));
    CHECK_NEAR (old.roughness, 1.40, 0.01);
    CHECK_NEAR (old.squeakCentroid, 0.70, 0.01);

    for (double h : { 0.0, 12.0, 60.0, 120.0, 500.0 })
        CHECK (StringAging::compute (h, true, false, 0.5, 4, inputs (0.0)).squeakCentroid == 1.0);

    // d = 0 keeps PlayingNoise's old roughness table.
    CHECK_NEAR (StringAging::compute (12.0, true, false, 0.0, 4, inputs (0.0)).roughness, 1.15, 1.0e-12);
}

LUTHIER_TEST (StringAging, SA14_automatingHoursDoesNotClick)
{
    auto render = [] (bool jump)
    {
        auto engine = makeEngine();
        StringAging::Inputs in;
        in.detail = 1.0;
        in.hours = 0.0;
        engine->getStringAging().setInputs (in);
        engine->getStringAging().reset();
        renderBlocks (*engine, 1);
        chord (*engine);

        std::vector<double> out;
        renderBlocks (*engine, 20, &out);

        if (jump)
        {
            in.hours = 200.0;
            engine->getStringAging().setInputs (in);
        }

        renderBlocks (*engine, 60, &out);
        return out;
    };

    auto maxStep = [] (const std::vector<double>& x)
    {
        double m = 0.0;

        for (size_t i = 1; i < x.size(); ++i)
            m = juce::jmax (m, std::abs (x[i] - x[i - 1]));

        return m;
    };

    const double still = maxStep (render (false));
    const double moved = maxStep (render (true));
    const double overDb = juce::Decibels::gainToDecibels (moved / still);

    CHECK_MSG (overDb <= 1.0, "the jump's largest step is " + juce::String (overDb, 2) + " dB above an unautomated render");
}

LUTHIER_TEST (StringAging, SA15_theStringsRangeFamily)
{
    for (const char* id : { ParamIDs::stringAgeHours, ParamIDs::stringCorrosivity })
    {
        const auto* r = RangeRegistry::find (id);
        CHECK (r != nullptr && r->isValid() && r->family == RangeFamily::strings);
    }

    CHECK (rangeFamilyFromName ("strings") == RangeFamily::strings);
    CHECK (juce::String (getRangeFamilyName (RangeFamily::strings)) == "strings");

    // Advanced reaches 2000 h and 5x corrosivity; the model is finite there.
    const auto f = StringAging::compute (2000.0, true, false, 1.0, 5, [] { auto in = inputs (1.0); in.corrosivity = 5.0; in.kRH = 3.0; return in; }());
    CHECK (std::isfinite (f.brightness) && std::isfinite (f.sustain) && f.brightness > 0.0 && f.sustain > 0.0);

    // A legacy state (no ranges block) loads with `strings` on stock.
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);
    RangeState state;
    state.fromVar ({}, {});
    CHECK (! state.isFamilyAdvanced (RangeFamily::strings));
}

LUTHIER_TEST (StringAging, SA16_budgetAndSafety)
{
    StringAging aging;
    aging.prepare (kSr);
    aging.setNumStrings (12);

    for (int s = 0; s < 12; ++s)
        aging.setStringInfo (s, s % 2 == 0, false);

    std::array<double, 12> levels {};
    levels.fill (0.1);

    auto in = inputs (1.0);
    in.accrual = AgeAccrual::x10;
    constexpr int blocks = 20000;
    double best = 1.0e9;

    for (int run = 0; run < 3; ++run)
    {
        const long allocationsBefore = luthierAllocationCount();
        const auto start = juce::Time::getHighResolutionTicks();

        for (int b = 0; b < blocks; ++b)
        {
            // Hours on a 1 Hz LFO over the stock range.
            in.hours = 100.0 + 100.0 * std::sin (constants::kTwoPi * b * kBlock / kSr);
            aging.setInputs (in);
            aging.advance (kBlock / kSr, levels.data(), 12);
        }

        best = juce::jmin (best, juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start));
        CHECK (luthierAllocationCount() == allocationsBefore);
    }

    // One unit is 1 % of a core; the budget is 0.02 units.
    // Machine-relative CPU budget: enforced only under LUTHIER_PERF=1 (the nightly).
    if (luthier::tests::perfRunRequested())
    {
        const double units = 100.0 * best / (blocks * kBlock / kSr);
        CHECK_MSG (units <= 0.02, "StringAging costs " + juce::String (units, 4) + " units (budget 0.02)");
    }
}
