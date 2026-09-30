/*  Tests for the musical model: tuning, string physics, technique detection,
    chord voicing and the guitar library.

    These are the tests that catch the errors a listener would notice first - a
    tuning that is a few cents out, a chord voiced somewhere no hand could reach,
    a string set whose tension is physically impossible.
*/

#include "TestFramework.h"

#include "../Model/Playing/TuningEngine.h"
#include "../Model/Playing/TechniqueEngine.h"
#include "../Model/Playing/ChordVoicer.h"
#include "../Model/Guitar/StringMaterials.h"
#include "../Model/Guitar/GuitarLibrary.h"
#include "../Validator.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;

    double centsBetween (double a, double b)
    {
        return 1200.0 * std::log2 (juce::jmax (1.0e-9, a) / juce::jmax (1.0e-9, b));
    }
}

//==============================================================================
//  Tuning
//==============================================================================
LUTHIER_TEST (Tuning, standardTuningIsExact)
{
    // Engine spec 19: every tuning correct within 0.1 cents.
    TuningEngine t;
    t.prepare (kSr);
    t.setTuningPreset (TuningPreset::Standard);
    t.randomiseRealismDetune (0.0, 1);

    const double expected[6] = { 329.628, 246.942, 195.998, 146.832, 110.000, 82.407 };

    for (int s = 0; s < 6; ++s)
    {
        t.setIntonationSlope (s, 0.0);

        const double open = t.computeFrequency (s, 0.0, 0.0);
        const double cents = centsBetween (open, expected[s]);

        CHECK_MSG (std::abs (cents) < 0.1,
                   "string " + juce::String (s) + ": " + juce::String (open, 4)
                   + " Hz vs expected " + juce::String (expected[s], 4)
                   + " (" + juce::String (cents, 4) + " cents)");
    }
}

LUTHIER_TEST (Tuning, everyPresetProducesSaneFrequencies)
{
    TuningEngine t;
    t.prepare (kSr);

    for (int p = 0; p < (int) TuningPreset::NumPresets; ++p)
    {
        const auto preset = (TuningPreset) p;

        // Custom deliberately leaves the tuning alone - it means "whatever the
        // user set" - so there is no factory table to check.
        if (preset == TuningPreset::Custom)
            continue;

        t.setTuningPreset (preset);
        t.randomiseRealismDetune (0.0, 1);

        const int count = TuningEngine::getPresetStringCount (preset);

        CHECK_MSG (count >= 4 && count <= kMaxStrings,
                   juce::String (TuningEngine::getTuningPresetName (preset))
                   + " has an implausible string count: " + juce::String (count));

        double previous = 1.0e9;

        for (int s = 0; s < count; ++s)
        {
            t.setIntonationSlope (s, 0.0);

            const double hz = t.computeFrequency (s, 0.0, 0.0);

            CHECK_MSG (hz > 25.0 && hz < 700.0,
                       juce::String (TuningEngine::getTuningPresetName (preset))
                       + " string " + juce::String (s) + " is at "
                       + juce::String (hz, 2) + " Hz");

            // String 0 is the highest; pitch must fall as the index rises. The
            // Nashville tuning is the deliberate exception, since its point is
            // that the lower courses are an octave up.
            if (preset != TuningPreset::Nashville)
                CHECK_MSG (hz < previous + 0.01,
                           juce::String (TuningEngine::getTuningPresetName (preset))
                           + ": string " + juce::String (s) + " is not below string "
                           + juce::String (s - 1));

            previous = hz;
        }
    }
}

LUTHIER_TEST (Tuning, everyFactoryPresetIsWithinOneTenthCent)
{
    // Independent MIDI-note targets catch a wrong preset entry as well as a
    // frequency-conversion regression. String 0 is the highest course.
    struct Expected { TuningPreset preset; std::initializer_list<int> notes; };
    const Expected expected[] = {
        { TuningPreset::Standard,       { 64, 59, 55, 50, 45, 40 } },
        { TuningPreset::DropD,          { 64, 59, 55, 50, 45, 38 } },
        { TuningPreset::DropC,          { 62, 57, 53, 48, 43, 36 } },
        { TuningPreset::DropB,          { 61, 56, 52, 47, 42, 35 } },
        { TuningPreset::DADGAD,         { 62, 57, 55, 50, 45, 38 } },
        { TuningPreset::OpenG,          { 62, 59, 55, 50, 43, 38 } },
        { TuningPreset::OpenD,          { 62, 57, 54, 50, 45, 38 } },
        { TuningPreset::OpenE,          { 64, 59, 56, 52, 47, 40 } },
        { TuningPreset::OpenC,          { 64, 60, 55, 48, 43, 36 } },
        { TuningPreset::HalfStepDown,   { 63, 58, 54, 49, 44, 39 } },
        { TuningPreset::FullStepDown,   { 62, 57, 53, 48, 43, 38 } },
        { TuningPreset::Nashville,      { 64, 59, 67, 62, 57, 52 } },
        { TuningPreset::SevenString,    { 64, 59, 55, 50, 45, 40, 35 } },
        { TuningPreset::EightString,    { 64, 59, 55, 50, 45, 40, 35, 30 } },
        { TuningPreset::BaritoneB,      { 59, 54, 50, 45, 40, 35 } },
        { TuningPreset::BassStandard,   { 43, 38, 33, 28 } },
        { TuningPreset::BassFiveString, { 43, 38, 33, 28, 23 } }
    };

    TuningEngine tuning;
    tuning.prepare (kSr);
    tuning.randomiseRealismDetune (0.0, 1);

    for (const auto& entry : expected)
    {
        tuning.setTuningPreset (entry.preset);
        CHECK_MSG (TuningEngine::getPresetStringCount (entry.preset) == (int) entry.notes.size(),
                   juce::String (TuningEngine::getTuningPresetName (entry.preset)) + " string count changed");

        int stringIndex = 0;
        for (int midiNote : entry.notes)
        {
            tuning.setIntonationSlope (stringIndex, 0.0);
            const double actual = tuning.computeFrequency (stringIndex, 0.0, 0.0);
            const double reference = 440.0 * std::pow (2.0, (midiNote - 69) / 12.0);
            const double errorCents = centsBetween (actual, reference);
            CHECK_MSG (std::abs (errorCents) < 0.1,
                       juce::String (TuningEngine::getTuningPresetName (entry.preset))
                       + " string " + juce::String (stringIndex)
                       + " differs by " + juce::String (errorCents, 5) + " cents");
            ++stringIndex;
        }
    }
}

LUTHIER_TEST (Tuning, twelfthFretIsAnOctave)
{
    TuningEngine t;
    t.prepare (kSr);
    t.setTuningPreset (TuningPreset::Standard);
    t.randomiseRealismDetune (0.0, 1);

    for (int s = 0; s < 6; ++s)
    {
        t.setIntonationSlope (s, 0.0);

        const double open = t.computeFrequency (s, 0.0, 0.0);
        const double octave = t.computeFrequency (s, 12.0, 0.0);

        CHECK_NEAR (centsBetween (octave, open * 2.0), 0.0, 0.1);
    }
}

LUTHIER_TEST (Tuning, fretPositionIsContinuous)
{
    // Pitfall 13: fretless mode and bends need genuinely continuous pitch, so
    // fractional fret positions must not quantise.
    TuningEngine t;
    t.prepare (kSr);
    t.setTuningPreset (TuningPreset::Standard);
    t.randomiseRealismDetune (0.0, 1);
    t.setIntonationSlope (0, 0.0);

    double previous = t.computeFrequency (0, 0.0, 0.0);
    int distinctValues = 0;

    for (int i = 1; i <= 240; ++i)
    {
        const double fret = (double) i / 10.0;
        const double hz = t.computeFrequency (0, fret, 0.0);

        CHECK_MSG (hz > previous, "pitch must rise monotonically with fret position");

        if (std::abs (hz - previous) > 1.0e-9)
            ++distinctValues;

        previous = hz;
    }

    CHECK_MSG (distinctValues == 240, "every tenth of a fret must give a distinct pitch, got "
               + juce::String (distinctValues));
}

LUTHIER_TEST (Tuning, intonationErrorGoesSharpUpTheNeck)
{
    TuningEngine t;
    t.prepare (kSr);
    t.setTuningPreset (TuningPreset::Standard);
    t.randomiseRealismDetune (0.0, 1);
    t.setIntonationSlope (0, 0.5);

    const double open = t.computeFrequency (0, 0.0, 0.0);
    const double twelfth = t.computeFrequency (0, 12.0, 0.0);

    const double error = centsBetween (twelfth, open * 2.0);

    CHECK_MSG (error > 4.0 && error < 8.0,
               "0.5 cents per fret should be about 6 cents sharp at the 12th, got "
               + juce::String (error, 2));
}

LUTHIER_TEST (Tuning, temperamentsDifferButStayInRange)
{
    TuningEngine t;
    t.prepare (kSr);
    t.setTuningPreset (TuningPreset::Standard);
    t.randomiseRealismDetune (0.0, 1);
    t.setIntonationSlope (0, 0.0);
    t.setTemperamentRoot (4);

    const double equalThird = [&t]
    {
        t.setTemperament (Temperament::EqualTemp12);
        return t.computeFrequency (0, 4.0, 0.0);
    }();

    const double justThird = [&t]
    {
        t.setTemperament (Temperament::JustIntonation);
        return t.computeFrequency (0, 4.0, 0.0);
    }();

    // A just major third is about 14 cents flat of an equal-tempered one.
    const double difference = centsBetween (justThird, equalThird);

    CHECK_MSG (difference < -8.0 && difference > -20.0,
               "a just third should sit about 14 cents below equal, got "
               + juce::String (difference, 2));

    for (int temp = 0; temp < (int) Temperament::NumTemperaments; ++temp)
    {
        t.setTemperament ((Temperament) temp);

        for (double fret = 0.0; fret <= 24.0; fret += 0.5)
        {
            const double hz = t.computeFrequency (0, fret, 0.0);

            CHECK_MSG (hz > 300.0 && hz < 1500.0,
                       juce::String (TuningEngine::getTemperamentName ((Temperament) temp))
                       + " at fret " + juce::String (fret, 1) + " gave "
                       + juce::String (hz, 2) + " Hz");
        }
    }
}

LUTHIER_TEST (Tuning, frequencyToFretRoundTrips)
{
    TuningEngine t;
    t.prepare (kSr);
    t.setTuningPreset (TuningPreset::Standard);
    t.randomiseRealismDetune (0.0, 1);

    for (int s = 0; s < 6; ++s)
    {
        for (double fret = 0.0; fret <= 20.0; fret += 2.5)
        {
            const double hz = t.computeFrequency (s, fret, 0.0);
            const double back = t.frequencyToFretPosition (s, hz);

            CHECK_NEAR (back, fret, 0.05);
        }
    }
}

LUTHIER_TEST (Tuning, noteNameParsingRoundTrips)
{
    const char* names[] = { "E2", "A2", "D3", "G3", "B3", "E4", "C#4", "Bb1", "F#1" };

    for (const char* name : names)
    {
        const int note = TuningEngine::parseNoteName (name);
        CHECK_MSG (note >= 0, juce::String ("failed to parse ") + name);

        if (note >= 0)
        {
            const auto back = TuningEngine::noteName (note);
            const int reparsed = TuningEngine::parseNoteName (back);
            CHECK_MSG (reparsed == note,
                       juce::String (name) + " -> " + juce::String (note) + " -> " + back);
        }
    }

    CHECK (TuningEngine::parseNoteName ("nonsense") < 0);
    CHECK (TuningEngine::parseNoteName ("") < 0);
}

//==============================================================================
//  String physics
//==============================================================================
LUTHIER_TEST (StringPhysics, standardSetsLandInTheUsualTensionRange)
{
    // A .010-.046 set on a 25.5" scale in standard tuning should come out at the
    // tensions a manufacturer prints on the packet: roughly 70 to 80 N per string.
    TuningEngine t;
    t.prepare (kSr);
    t.setTuningPreset (TuningPreset::Standard);
    t.randomiseRealismDetune (0.0, 1);

    for (int s = 0; s < 6; ++s)
    {
        const double hz = t.computeFrequency (s, 0.0, 0.0);

        const auto spec = StringMaterials::computeSpec (
            StringMaterial::NickelPlatedSteel, StringGauge::Regular, StringAge::BrokenIn,
            s, hz, 647.7);

        CHECK_MSG (StringMaterials::isTensionPlayable (spec.tensionNewtons, 647.7),
                   "string " + juce::String (s) + " tension "
                   + juce::String (spec.tensionNewtons, 1) + " N is outside the guitar range");

        CHECK_MSG (spec.linearDensity > 0.0 && spec.linearDensity < 0.05,
                   "implausible mass per metre: " + juce::String (spec.linearDensity, 6));

        CHECK_MSG (spec.inharmonicityB > 0.0 && spec.inharmonicityB < 0.003,
                   "implausible inharmonicity: " + juce::String (spec.inharmonicityB, 6));
    }
}

LUTHIER_TEST (StringPhysics, thickerStringsAreHeavierAndTighter)
{
    const auto light = StringMaterials::computeSpec (
        StringMaterial::NickelPlatedSteel, StringGauge::ExtraLight, StringAge::Fresh,
        5, 82.407, 647.7);

    const auto heavy = StringMaterials::computeSpec (
        StringMaterial::NickelPlatedSteel, StringGauge::Heavy, StringAge::Fresh,
        5, 82.407, 647.7);

    CHECK_MSG (heavy.diameterInches > light.diameterInches, "a heavy set must be thicker");
    CHECK_MSG (heavy.linearDensity > light.linearDensity, "a thicker string must be heavier");
    CHECK_MSG (heavy.tensionNewtons > light.tensionNewtons,
               "at the same pitch, a heavier string needs more tension");
}

LUTHIER_TEST (StringPhysics, woundStringsAreLessStiffThanTheirDiameterSuggests)
{
    // Only the core resists bending. A wound low E is far less inharmonic than a
    // solid wire of the same outside diameter would be, and getting this wrong
    // makes low notes sound like a piano.
    const auto wound = StringMaterials::computeSpec (
        StringMaterial::NickelPlatedSteel, StringGauge::Regular, StringAge::Fresh,
        5, 82.407, 647.7);

    CHECK_MSG (wound.wound, "the low E of a .010 set is wound");
    CHECK_MSG (wound.coreDiameterMm < wound.diameterMm,
               "a wound string's core must be thinner than its outside diameter");

    const auto plain = StringMaterials::computeSpec (
        StringMaterial::NickelPlatedSteel, StringGauge::Regular, StringAge::Fresh,
        0, 329.628, 647.7);

    CHECK_MSG (! plain.wound, "the high E of a .010 set is plain");
    CHECK_NEAR (plain.coreDiameterMm, plain.diameterMm, 1.0e-9);
}

LUTHIER_TEST (StringPhysics, ageDullsAndShortens)
{
    const auto fresh = StringMaterials::computeSpec (
        StringMaterial::NickelPlatedSteel, StringGauge::Regular, StringAge::Fresh,
        3, 146.832, 647.7);

    const auto old = StringMaterials::computeSpec (
        StringMaterial::NickelPlatedSteel, StringGauge::Regular, StringAge::Old,
        3, 146.832, 647.7);

    CHECK_MSG (old.brightnessHz < fresh.brightnessHz, "old strings are duller");
    CHECK_MSG (old.sustainSeconds < fresh.sustainSeconds, "old strings die sooner");
    CHECK_MSG (old.ageDetuneCents > fresh.ageDetuneCents, "old strings do not hold pitch");
}

LUTHIER_TEST (StringPhysics, nylonIsQuiteDifferentFromSteel)
{
    const auto steel = StringMaterials::computeSpec (
        StringMaterial::NickelPlatedSteel, StringGauge::Regular, StringAge::BrokenIn,
        0, 329.628, 647.7);

    const auto nylon = StringMaterials::computeSpec (
        StringMaterial::Nylon, StringGauge::ClassicalNormal, StringAge::BrokenIn,
        0, 329.628, 650.0);

    CHECK_MSG (nylon.linearDensity < steel.linearDensity * 3.0,
               "nylon is much less dense than steel for its diameter");
    CHECK_MSG (nylon.squeak < steel.squeak, "nylon trebles barely squeak");
    CHECK_MSG (nylon.brightnessHz < steel.brightnessHz, "nylon is warmer");
}

LUTHIER_TEST (StringPhysics, everyFactoryGuitarIsStringedPlausibly)
{
    // Every shipped instrument, with its own default string set and tuning, must
    // produce tensions a real luthier would accept. This is identity rule 1
    // applied across the whole factory bank at once.
    for (int g = 0; g < (int) GuitarType::NumTypes; ++g)
    {
        const auto& spec = GuitarLibrary::get ((GuitarType) g);

        TuningEngine t;
        t.prepare (kSr);
        t.setTuningPreset (spec.tuning);
        t.randomiseRealismDetune (0.0, 1);

        const int strings = juce::jmin (spec.numStrings,
                                        TuningEngine::getPresetStringCount (spec.tuning));

        for (int s = 0; s < strings; ++s)
        {
            const double hz = t.computeFrequency (s, 0.0, 0.0);

            const auto stringSpec = StringMaterials::computeSpec (
                spec.stringMaterial, spec.stringGauge, StringAge::BrokenIn,
                s, hz, spec.scaleLengthMm);

            // The playable range depends on the instrument: a bass neck is built
            // for two to three times the tension a guitar neck is.
            CHECK_MSG (StringMaterials::isTensionPlayable (stringSpec.tensionNewtons,
                                                           spec.scaleLengthMm),
                       juce::String (spec.name) + " string " + juce::String (s + 1)
                       + " at " + juce::String (hz, 1) + " Hz needs "
                       + juce::String (stringSpec.tensionNewtons, 1) + " N");
        }
    }
}

//==============================================================================
//  Technique detection
//==============================================================================
LUTHIER_TEST (Technique, legatoBecomesHammerOnAndPullOff)
{
    TechniqueEngine tech;
    tech.prepare (kSr, 6);
    tech.setLegatoWindowMs (40.0);

    int partial = 0;
    double slideFrom = -1.0;

    // First note is always a pluck.
    auto t1 = tech.decide (0, 5.0, 0.9, 0, partial, slideFrom);
    CHECK (t1 == Technique::Pluck);

    // A higher note, softly, well after the slide window: hammer-on.
    const int64_t later = (int64_t) (kSr * 0.2);
    auto t2 = tech.decide (0, 7.0, 0.4, later, partial, slideFrom);
    CHECK_MSG (t2 == Technique::HammerOn,
               "expected a hammer-on, got " + juce::String (getTechniqueName (t2)));

    // A lower note, softly: pull-off.
    auto t3 = tech.decide (0, 5.0, 0.4, later * 2, partial, slideFrom);
    CHECK_MSG (t3 == Technique::PullOff,
               "expected a pull-off, got " + juce::String (getTechniqueName (t3)));

    // A hard strike is a re-pluck, not a hammer-on.
    auto t4 = tech.decide (0, 9.0, 1.0, later * 3, partial, slideFrom);
    CHECK_MSG (t4 == Technique::Pluck,
               "a hard strike should re-pick, got " + juce::String (getTechniqueName (t4)));
}

LUTHIER_TEST (Technique, fastNotesBecomeASlide)
{
    TechniqueEngine tech;
    tech.prepare (kSr, 6);
    tech.setLegatoWindowMs (40.0);

    int partial = 0;
    double slideFrom = -1.0;

    tech.decide (0, 5.0, 0.8, 0, partial, slideFrom);

    // 10 ms later, inside the window.
    const int64_t soon = (int64_t) (kSr * 0.01);
    auto t = tech.decide (0, 9.0, 0.6, soon, partial, slideFrom);

    CHECK_MSG (t == Technique::Slide, "expected a slide, got " + juce::String (getTechniqueName (t)));
    CHECK_NEAR (slideFrom, 5.0, 1.0e-9);
}

LUTHIER_TEST (Technique, controllersTakePriorityOverInference)
{
    TechniqueEngine tech;
    tech.prepare (kSr, 6);

    int partial = 0;
    double slideFrom = -1.0;

    tech.setPalmMuteAmount (0.9);
    CHECK (tech.decide (0, 3.0, 0.9, 0, partial, slideFrom) == Technique::PalmMute);
    tech.setPalmMuteAmount (0.0);

    tech.reset();
    tech.setPinchHarmonicTrigger (true);
    CHECK (tech.decide (0, 3.0, 0.9, 0, partial, slideFrom) == Technique::PinchHarmonic);
    // harmonic-realism.md 3: the pinch's partial follows from where the thumb
    // grazes (the engine's node search at the pick), no longer from velocity.
    CHECK_MSG (partial == 0, "the pinch's partial is the engine's to find, not the technique engine's");
    tech.setPinchHarmonicTrigger (false);

    tech.reset();
    tech.setTapTrigger (true);
    CHECK (tech.decide (0, 12.0, 0.7, 0, partial, slideFrom) == Technique::Tap);
    tech.setTapTrigger (false);

    tech.reset();
    tech.setSlideGuitarMode (true);
    CHECK (tech.decide (0, 5.0, 0.8, 0, partial, slideFrom) == Technique::SlideGuitar);
}

LUTHIER_TEST (Technique, harmonicNodesAreDetected)
{
    CHECK (TechniqueEngine::harmonicPartialForFret (12.0) == 2);
    CHECK (TechniqueEngine::harmonicPartialForFret (7.0) == 3);
    CHECK (TechniqueEngine::harmonicPartialForFret (19.0) == 3);
    CHECK (TechniqueEngine::harmonicPartialForFret (5.0) == 4);
    CHECK (TechniqueEngine::harmonicPartialForFret (4.0) == 5);

    // Somewhere that is not a node.
    CHECK (TechniqueEngine::harmonicPartialForFret (6.0) == 0);
    CHECK (TechniqueEngine::harmonicPartialForFret (10.0) == 0);
}

LUTHIER_TEST (Technique, fretlessTurnsLegatoIntoGlide)
{
    TechniqueEngine tech;
    tech.prepare (kSr, 6);
    tech.setFretlessMode (true);
    tech.setLegatoWindowMs (10.0);

    int partial = 0;
    double slideFrom = -1.0;

    tech.decide (0, 5.0, 0.9, 0, partial, slideFrom);

    const int64_t later = (int64_t) (kSr * 0.2);
    auto t = tech.decide (0, 7.0, 0.4, later, partial, slideFrom);

    CHECK_MSG (t == Technique::Slide,
               "a fretless instrument slides rather than hammering, got "
               + juce::String (getTechniqueName (t)));
}

LUTHIER_TEST (Technique, slideDurationScalesWithDistance)
{
    TechniqueEngine tech;
    tech.prepare (kSr, 6);

    const double shortSlide = tech.slideDurationFor (2.0);
    const double longSlide = tech.slideDurationFor (12.0);

    CHECK_MSG (longSlide > shortSlide, "a longer slide must take longer");
    CHECK_MSG (shortSlide > 0.01 && longSlide < 0.6,
               "slide durations should stay musical: " + juce::String (shortSlide, 3)
               + " s and " + juce::String (longSlide, 3) + " s");
}

//==============================================================================
//  Chord voicing
//==============================================================================
LUTHIER_TEST (ChordVoicer, commonChordsAreVoicedPlayably)
{
    // Identity rule 9: the voicer must never return a fingering no hand could make.
    TuningEngine t;
    t.prepare (kSr);
    t.setTuningPreset (TuningPreset::Standard);
    t.randomiseRealismDetune (0.0, 1);

    for (int s = 0; s < 6; ++s)
        t.setIntonationSlope (s, 0.0);

    ChordVoicer v;
    v.prepare (&t, 6);
    v.setMaxFretSpan (4);
    v.setMaxFret (22);

    struct Chord { const char* name; std::vector<int> notes; };

    const std::vector<Chord> chords =
    {
        { "E major",   { 40, 47, 52, 56, 59, 64 } },
        { "A minor",   { 45, 52, 57, 60, 64 } },
        { "C major",   { 48, 52, 55, 60, 64 } },
        { "G major",   { 43, 47, 50, 55, 59, 67 } },
        { "D major",   { 50, 57, 62, 66 } },
        { "F major",   { 41, 48, 53, 57, 60, 65 } },
        { "B minor 7", { 47, 54, 57, 62, 66 } },
        { "E7",        { 40, 47, 52, 56, 59, 62 } },
        { "D minor",   { 50, 57, 62, 65 } },
        { "G7",        { 43, 47, 50, 55, 59, 65 } },
    };

    for (const auto& chord : chords)
    {
        const auto voicing = v.voice (chord.notes.data(), nullptr, (int) chord.notes.size());

        CHECK_MSG (voicing.numNotes > 0,
                   juce::String (chord.name) + " produced no notes at all");

        CHECK_MSG (voicing.fretSpan <= v.getMaxFretSpan(),
                   juce::String (chord.name) + " needs a span of "
                   + juce::String (voicing.fretSpan) + " frets");

        CHECK_MSG (voicing.droppedNotes <= 1,
                   juce::String (chord.name) + " dropped "
                   + juce::String (voicing.droppedNotes) + " notes");

        // One note per string, and every note actually reachable.
        bool used[kMaxStrings] = {};

        for (int i = 0; i < voicing.numNotes; ++i)
        {
            const auto& note = voicing.notes[(size_t) i];

            if (! note.valid)
                continue;

            CHECK_MSG (! used[note.stringIndex],
                       juce::String (chord.name) + " put two notes on string "
                       + juce::String (note.stringIndex));

            used[note.stringIndex] = true;

            CHECK_MSG (note.fretPosition >= 0.0 && note.fretPosition <= 22.0,
                       juce::String (chord.name) + " used fret "
                       + juce::String (note.fretPosition, 1));

            // The assigned string really can produce that pitch.
            const double hz = t.computeFrequency (note.stringIndex, note.fretPosition, 0.0);
            const double expected = midiToHz ((double) note.midiNote);

            CHECK_MSG (std::abs (centsBetween (hz, expected)) < 5.0,
                       juce::String (chord.name) + ": string " + juce::String (note.stringIndex)
                       + " fret " + juce::String (note.fretPosition, 1) + " gives "
                       + juce::String (hz, 2) + " Hz, wanted " + juce::String (expected, 2));
        }
    }
}

LUTHIER_TEST (ChordVoicer, pitchOrderFollowsStringOrder)
{
    TuningEngine t;
    t.prepare (kSr);
    t.setTuningPreset (TuningPreset::Standard);
    t.randomiseRealismDetune (0.0, 1);

    ChordVoicer v;
    v.prepare (&t, 6);

    const int notes[] = { 40, 47, 52, 56, 59, 64 };   // open E major
    const auto voicing = v.voice (notes, nullptr, 6);

    for (int i = 0; i < voicing.numNotes; ++i)
    {
        for (int j = i + 1; j < voicing.numNotes; ++j)
        {
            const auto& a = voicing.notes[(size_t) i];
            const auto& b = voicing.notes[(size_t) j];

            if (! a.valid || ! b.valid)
                continue;

            // A lower note must be on a lower-pitched (higher-indexed) string.
            if (a.midiNote < b.midiNote)
                CHECK_MSG (a.stringIndex >= b.stringIndex,
                           "note " + juce::String (a.midiNote) + " on string "
                           + juce::String (a.stringIndex) + " but note "
                           + juce::String (b.midiNote) + " on string "
                           + juce::String (b.stringIndex));
        }
    }
}

LUTHIER_TEST (ChordVoicer, impossibleChordDegradesGracefully)
{
    // Six notes a semitone apart cannot be played on a guitar. The voicer must
    // return the best playable subset rather than nothing, or nonsense.
    TuningEngine t;
    t.prepare (kSr);
    t.setTuningPreset (TuningPreset::Standard);
    t.randomiseRealismDetune (0.0, 1);

    ChordVoicer v;
    v.prepare (&t, 6);
    v.setMaxFretSpan (4);

    const int cluster[] = { 60, 61, 62, 63, 64, 65 };
    const auto voicing = v.voice (cluster, nullptr, 6);

    CHECK_MSG (voicing.fretSpan <= v.getMaxFretSpan(),
               "even a degraded voicing must be reachable, span was "
               + juce::String (voicing.fretSpan));

    for (int i = 0; i < voicing.numNotes; ++i)
        CHECK (voicing.notes[(size_t) i].fretPosition >= 0.0);
}

LUTHIER_TEST (ChordVoicer, singleNotesStayNearTheHand)
{
    TuningEngine t;
    t.prepare (kSr);
    t.setTuningPreset (TuningPreset::Standard);
    t.randomiseRealismDetune (0.0, 1);

    ChordVoicer v;
    v.prepare (&t, 6);
    v.setPreferredPosition (12);

    // A note playable in several places should be taken near the 12th fret.
    const auto note = v.voiceSingleNote (64, 0.8);   // E4

    CHECK (note.valid);
    CHECK_MSG (std::abs (note.fretPosition - 12.0) < 8.0,
               "with the hand at the 12th fret, E4 was voiced at fret "
               + juce::String (note.fretPosition, 1));
}

LUTHIER_TEST (ChordVoicer, chordNamesAreIdentified)
{
    auto identify = [] (std::initializer_list<int> notes)
    {
        std::vector<int> v (notes);
        return ChordVoicer::identifyChord (v.data(), (int) v.size());
    };

    CHECK (identify ({ 60, 64, 67 }) == "C");
    CHECK (identify ({ 60, 63, 67 }) == "Cm");
    CHECK (identify ({ 60, 64, 67, 70 }) == "C7");
    CHECK (identify ({ 60, 64, 67, 71 }) == "Cmaj7");
    CHECK (identify ({ 60, 63, 67, 70 }) == "Cm7");
    CHECK (identify ({ 57, 60, 64 }) == "Am");
    CHECK (identify ({ 60, 67 }) == "C5");

    // An inversion is named with its bass note.
    CHECK_MSG (identify ({ 64, 67, 72 }).startsWith ("C"),
               "first-inversion C should still be named as a C chord, got "
               + identify ({ 64, 67, 72 }));

    // A cluster is not a chord.
    CHECK (identify ({ 60, 61, 62 }).isEmpty());
}

LUTHIER_TEST (ChordVoicer, libraryShapesAreSane)
{
    CHECK_MSG (ChordVoicer::getNumLibraryChords() >= 40,
               "the chord library should cover the common shapes, has "
               + juce::String (ChordVoicer::getNumLibraryChords()));

    for (int i = 0; i < ChordVoicer::getNumLibraryChords(); ++i)
    {
        const auto& chord = ChordVoicer::getLibraryChord (i);

        CHECK_MSG (juce::String (chord.name).isNotEmpty(),
                   "library chord " + juce::String (i) + " has no name");

        int sounding = 0;

        for (int s = 0; s < 6; ++s)
        {
            CHECK_MSG (chord.frets[s] >= -1 && chord.frets[s] <= 24,
                       juce::String (chord.name) + " string " + juce::String (s)
                       + " at fret " + juce::String (chord.frets[s]));

            if (chord.frets[s] >= 0)
                ++sounding;
        }

        CHECK_MSG (sounding >= 2, juce::String (chord.name) + " has fewer than two notes");

        // The fretted notes must be within a hand's reach.
        int lowest = 99, highest = 0;

        for (int s = 0; s < 6; ++s)
        {
            if (chord.frets[s] > 0)
            {
                lowest = juce::jmin (lowest, chord.frets[s]);
                highest = juce::jmax (highest, chord.frets[s]);
            }
        }

        if (highest > 0)
            CHECK_MSG (highest - lowest <= 4,
                       juce::String (chord.name) + " spans "
                       + juce::String (highest - lowest) + " frets");
    }
}

//==============================================================================
//  Validator
//==============================================================================
LUTHIER_TEST (Validator, correctsRatherThanCrashing)
{
    Validator v;
    v.reset();
    v.setEnabled (true);
    v.setStrict (false);

    bool accepted = true;

    const double absurd = v.checkTension (0, 5000.0, 648.0, 0, accepted);

    CHECK_MSG (accepted, "non-strict mode must correct rather than reject");
    CHECK_MSG (absurd <= StringMaterials::getTensionRange (648.0).absoluteMax,
               "tension should have been clamped, got " + juce::String (absurd, 1));
    CHECK (v.getFailureCount (ValidationCheck::StringTension) == 1);

    const double fret = v.checkFretRange (0, 99.0, 22, 0, accepted);
    CHECK_NEAR (fret, 22.0, 1.0e-9);
    CHECK (v.getFailureCount (ValidationCheck::FretRange) == 1);

    CHECK (! v.checkFinite (std::numeric_limits<double>::quiet_NaN(), 0));
    CHECK (v.checkFinite (0.5, 0));

    CHECK (v.hasFailures());
    CHECK (v.getSummary().isNotEmpty());
}

LUTHIER_TEST (Validator, strictModeRejects)
{
    Validator v;
    v.reset();
    v.setEnabled (true);
    v.setStrict (true);

    bool accepted = true;
    v.checkTension (0, 5000.0, 648.0, 0, accepted);

    CHECK_MSG (! accepted, "strict mode must reject a physically impossible tension");
}

LUTHIER_TEST (Validator, passesGoodValues)
{
    Validator v;
    v.reset();

    bool accepted = true;

    CHECK_NEAR (v.checkTension (0, 70.0, 648.0, 0, accepted), 70.0, 1.0e-9);
    CHECK (accepted);

    CHECK_NEAR (v.checkFretRange (0, 7.0, 22, 0, accepted), 7.0, 1.0e-9);
    CHECK (accepted);

    CHECK (! v.hasFailures());
    CHECK (v.getSummary() == "All checks passing.");
}

//==============================================================================
//  Guitar library
//==============================================================================
LUTHIER_TEST (GuitarLibrary, everyEntryIsInternallyConsistent)
{
    for (int g = 0; g < (int) GuitarType::NumTypes; ++g)
    {
        const auto& spec = GuitarLibrary::get ((GuitarType) g);
        const juce::String name (spec.name);

        CHECK_MSG (name.isNotEmpty(), "guitar " + juce::String (g) + " has no name");

        CHECK_MSG (spec.numStrings >= 4 && spec.numStrings <= kMaxStrings,
                   name + " has " + juce::String (spec.numStrings) + " strings");

        CHECK_MSG (spec.scaleLengthMm > 500.0 && spec.scaleLengthMm < 900.0,
                   name + " scale length is " + juce::String (spec.scaleLengthMm, 1) + " mm");

        CHECK_MSG (spec.maxFrets >= 12 && spec.maxFrets <= 27,
                   name + " has " + juce::String (spec.maxFrets) + " frets");

        CHECK_MSG (spec.numPickups >= 1 && spec.numPickups <= PickupEngine::kMaxPickups,
                   name + " has " + juce::String (spec.numPickups) + " pickups");

        for (int p = 0; p < spec.numPickups; ++p)
            CHECK_MSG (spec.pickupPositions[p] >= 0.0 && spec.pickupPositions[p] <= 0.5,
                       name + " pickup " + juce::String (p) + " at "
                       + juce::String (spec.pickupPositions[p], 3));

        CHECK_MSG (spec.fretActionMm > 0.4 && spec.fretActionMm < 4.5,
                   name + " action is " + juce::String (spec.fretActionMm, 2) + " mm");

        CHECK_MSG (spec.defaultPluckPosition > 0.02 && spec.defaultPluckPosition <= 0.5,
                   name + " pluck position is " + juce::String (spec.defaultPluckPosition, 3));

        // A twelve-string really has twelve strings.
        if (spec.twelveString)
            CHECK_MSG (spec.numStrings == 12, name + " claims to be a 12-string");

        // The tuning preset must supply enough strings for the instrument.
        CHECK_MSG (TuningEngine::getPresetStringCount (spec.tuning) >= 4,
                   name + " has an unusable default tuning");
    }
}

LUTHIER_TEST (GuitarLibrary, twelveStringCoursesAreOctavePaired)
{
    // The top two courses are unison; the lower four are octaves.
    CHECK_NEAR (GuitarLibrary::twelveStringOctaveOffset (0), 0.0, 1.0e-9);
    CHECK_NEAR (GuitarLibrary::twelveStringOctaveOffset (1), 0.0, 1.0e-9);
    CHECK_NEAR (GuitarLibrary::twelveStringOctaveOffset (3), 0.0, 1.0e-9);

    CHECK_NEAR (GuitarLibrary::twelveStringOctaveOffset (5), 12.0, 1.0e-9);
    CHECK_NEAR (GuitarLibrary::twelveStringOctaveOffset (7), 12.0, 1.0e-9);
    CHECK_NEAR (GuitarLibrary::twelveStringOctaveOffset (11), 12.0, 1.0e-9);

    CHECK (GuitarLibrary::courseForString (0) == 0);
    CHECK (GuitarLibrary::courseForString (1) == 0);
    CHECK (GuitarLibrary::courseForString (2) == 1);
    CHECK (GuitarLibrary::courseForString (11) == 5);
}
