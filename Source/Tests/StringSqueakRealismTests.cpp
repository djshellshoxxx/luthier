/*  Finger-squeak realism (string-squeak.md 13, owner 2026-09-26): the squeak
    is measured at the engine's output, not only at the generator - a chord
    change with a large position shift on the factory acoustics, on the
    Classical and on an electric, in the 1-6 kHz band of the inter-note
    window, against the same change rendered silent. */

#include "TestFramework.h"

#include "../DSP/Noise/PlayingNoise.h"
#include "../LuthierEngine.h"
#include "../Model/Guitar/GuitarLibrary.h"
#include "../Presets/FactoryPresets.h"
#include "../PluginProcessor.h"

#include <iostream>

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    struct ChordChange
    {
        GuitarType type = GuitarType::Dreadnought;
        double amount = 0.25, probability = 0.65, moisture = 0.35, pressure = 0.5;
        double fromFret = 2.0, toFret = 7.0;
        bool legato = false;          ///< true: slide (finger down); false: lift, gap, re-pluck
        double gapMs = 80.0;          ///< hand-shift gap between note-off and the next pluck
        juce::uint64 seed = 1;
        StringMaterial material = StringMaterial::NumMaterials;   ///< NumMaterials = the guitar's own
        int blocksAfterChange = 60;
    };

    struct Rendered
    {
        std::vector<float> out;
        int changeSample = 0, settleSample = 0;
        juce::int64 squeaks = 0;
    };

    Rendered render (const ChordChange& c)
    {
        LuthierEngine engine;
        engine.prepare (kSr, kBlock);
        engine.setGuitarType (c.type);
        engine.getCabinetEngine().setEnabled (false);
        engine.getRoomEngine().setEnabled (false);
        engine.getAmpEngine().setGain (0.0);
        engine.getPlayingNoise().setSeed ((juce::uint32) c.seed);

        if (c.material != StringMaterial::NumMaterials)
            engine.setStringMaterial (c.material);

        PickSettings quietPick;
        quietPick.clickAmount = quietPick.chirpAmount = quietPick.scrapeAmount = 0.0;
        engine.setPickNoise (quietPick);

        SqueakSettings s;
        s.amount = c.amount;
        s.probability = c.probability;
        s.moisture = c.moisture;
        s.pressure = c.pressure;
        engine.setSqueak (s);

        // The three lowest strings - the bass side, where shifts squeak.
        const int n = engine.getNumStrings();
        std::vector<int> voices { n - 1, n - 2, n - 3 };

        auto pluck = [&] (int string, double fret, bool slideFrom, double from)
        {
            NoteOnEvent e;
            e.stringIndex = string;
            e.velocity = 0.8;
            e.fretPosition = fret;
            e.pitchHz = engine.getStringSpec (string).wound ? 110.0 * std::pow (2.0, fret / 12.0)
                                                            : 220.0 * std::pow (2.0, fret / 12.0);
            if (slideFrom)
            {
                e.technique = Technique::Slide;
                e.slideFromFret = from;
                e.slideSeconds = 0.15;
            }
            engine.triggerNoteNow (e);
        };

        auto release = [&] (int string)
        {
            NoteOffEvent off;
            off.stringIndex = string;
            engine.releaseNoteNow (off);
        };

        Rendered r;
        juce::AudioBuffer<float> block (2, kBlock);
        const int gapBlocks = juce::jmax (1, (int) std::round (c.gapMs * 0.001 * kSr / kBlock));
        const int changeBlock = 40;
        const int total = changeBlock + gapBlocks + c.blocksAfterChange;

        for (int b = 0; b < total; ++b)
        {
            if (b == 0)
                for (int v : voices) pluck (v, c.fromFret, false, 0.0);

            if (b == changeBlock)
            {
                r.changeSample = b * kBlock;

                if (c.legato)
                    for (int v : voices) pluck (v, c.toFret, true, c.fromFret);
                else
                    for (int v : voices) release (v);
            }

            if (! c.legato && b == changeBlock + gapBlocks)
            {
                r.settleSample = b * kBlock;
                for (int v : voices) pluck (v, c.toFret, false, 0.0);
            }

            block.clear();
            juce::MidiBuffer none;
            engine.processBlock (block, none);

            for (int i = 0; i < kBlock; ++i)
                r.out.push_back (block.getSample (0, i));
        }

        if (c.legato)
            r.settleSample = r.changeSample + (int) (0.15 * kSr);

        r.squeaks = engine.getPlayingNoise().getPool().getTriggerCount (NoiseClass::squeak);
        return r;
    }

    /** Band-limited (1-6 kHz) RMS of (a - b) over [from, to). */
    double bandRms (const std::vector<float>& a, const std::vector<float>& b, int from, int to)
    {
        juce::IIRFilter hp1, hp2, lp1, lp2;
        hp1.setCoefficients (juce::IIRCoefficients::makeHighPass (kSr, 1000.0));
        hp2.setCoefficients (juce::IIRCoefficients::makeHighPass (kSr, 1000.0));
        lp1.setCoefficients (juce::IIRCoefficients::makeLowPass (kSr, 6000.0));
        lp2.setCoefficients (juce::IIRCoefficients::makeLowPass (kSr, 6000.0));

        double sum = 0.0;
        int count = 0;

        for (int i = 0; i < (int) juce::jmin (a.size(), b.size()); ++i)
        {
            float x = a[(size_t) i] - b[(size_t) i];
            x = lp2.processSingleSampleRaw (lp1.processSingleSampleRaw (hp2.processSingleSampleRaw (hp1.processSingleSampleRaw (x))));

            if (i >= from && i < to)
            {
                sum += (double) x * x;
                ++count;
            }
        }

        return count > 0 ? std::sqrt (sum / count) : 0.0;
    }

    double rms (const std::vector<float>& a, int from, int to)
    {
        double sum = 0.0;
        int count = 0;

        for (int i = from; i < to && i < (int) a.size(); ++i, ++count)
            sum += (double) a[(size_t) i] * a[(size_t) i];

        return count > 0 ? std::sqrt (sum / count) : 0.0;
    }

    struct Measure
    {
        double squeakDbBelowNote;   ///< 1-6 kHz squeak RMS in the shift window vs the note's RMS
        juce::int64 squeaks;
    };

    /** The squeak's band energy during the shift, relative to the sounding note. */
    Measure measure (ChordChange c)
    {
        auto silent = c;
        silent.amount = 0.0;

        const auto a = render (c), b = render (silent);
        const int windowEnd = juce::jmax (a.settleSample, a.changeSample) + (int) (0.06 * kSr);

        const double squeak = bandRms (a.out, b.out, a.changeSample, windowEnd);
        const double note = rms (b.out, 2000, a.changeSample);

        return { gainToDb (juce::jmax (1.0e-9, squeak) / juce::jmax (1.0e-9, note)) * -1.0, a.squeaks };
    }

    juce::String report (const char* label, const Measure& m)
    {
        return juce::String (label) + ": squeak " + juce::String (m.squeakDbBelowNote, 1)
               + " dB below the note, " + juce::String (m.squeaks) + " squeak events";
    }
}

//==============================================================================
LUTHIER_TEST (SqueakRealism, surveyChordChangesAcrossGuitars)
{
    // Not an assertion test: it prints the audit numbers for the report.
    for (auto type : { GuitarType::Dreadnought, GuitarType::Auditorium, GuitarType::Jumbo, GuitarType::Parlor,
                       GuitarType::TwelveString, GuitarType::Classical, GuitarType::Stratocaster, GuitarType::LesPaul })
    {
        for (bool legato : { true, false })
            for (double amount : { 0.25, 1.0, 0.0 })
            {
                ChordChange c;
                c.type = type;
                c.legato = legato;
                c.amount = amount;
                c.probability = 1.0;
                const auto m = measure (c);
                std::cerr << "SQ| " << report ((juce::String (GuitarLibrary::getName (type)).paddedRight (' ', 14)
                                                 + (legato ? " legato" : " lift  ")
                                                 + " amount " + juce::String (amount, 2)).toRawUTF8(), m) << "\n";
            }
    }

    CHECK (true);
}

//==============================================================================
//  (a) audible at the output, by instrument
//==============================================================================
LUTHIER_TEST (SqueakRealism, aChordChangeIsAudibleOnAnAcousticAndQuieterElsewhere)
{
    auto at = [] (GuitarType type, bool legato, double amount)
    {
        ChordChange c;
        c.type = type;
        c.legato = legato;
        c.amount = amount;
        c.probability = 1.0;
        return measure (c);
    };

    // string-squeak.md 0.4 / 13: the ship default on the Dreadnought, lifted
    // (a chord change) and held (a legato slide), sits 18-38 dB under the note.
    for (bool legato : { false, true })
    {
        const auto m = at (GuitarType::Dreadnought, legato, 0.25);
        CHECK_MSG (m.squeaks == 3, juce::String (legato ? "legato" : "lifted") + " change: " + juce::String (m.squeaks) + " squeaks, expected 3");
        CHECK_MSG (m.squeakDbBelowNote > 18.0 && m.squeakDbBelowNote < 36.0,
                   report (legato ? "Dreadnought legato default" : "Dreadnought lifted default", m));
    }

    // Every steel-string factory acoustic is in the same band.
    for (auto type : { GuitarType::Auditorium, GuitarType::Jumbo, GuitarType::Parlor, GuitarType::TwelveString })
    {
        const auto m = at (type, false, 0.25);
        // Their bodies and mics differ in how much of 1-6 kHz they pass against the note.
        CHECK_MSG (m.squeakDbBelowNote > 18.0 && m.squeakDbBelowNote < 42.0, report (GuitarLibrary::getName (type), m));
    }

    // Nylon basses squeak less than bronze, and a magnetic pickup hears less still.
    const auto dread = at (GuitarType::Dreadnought, false, 0.25);
    const auto classical = at (GuitarType::Classical, false, 0.25);
    const auto strat = at (GuitarType::Stratocaster, false, 0.25);

    CHECK_MSG (classical.squeakDbBelowNote > dread.squeakDbBelowNote + 2.0 && classical.squeakDbBelowNote < 55.0,
               report ("Classical", classical) + " vs " + report ("Dreadnought", dread));
    CHECK_MSG (strat.squeakDbBelowNote > dread.squeakDbBelowNote + 3.0,
               report ("Stratocaster", strat) + " vs " + report ("Dreadnought", dread));
}

//==============================================================================
//  (b) amount scales level, probability scales occurrence
//==============================================================================
LUTHIER_TEST (SqueakRealism, amountScalesLevelAndProbabilityScalesOccurrence)
{
    ChordChange c;
    c.probability = 1.0;

    auto at = [&] (double amount) { auto v = c; v.amount = amount; return measure (v); };

    const auto quarter = at (0.25), full = at (1.0), off = at (0.0);
    CHECK_MSG (std::abs ((quarter.squeakDbBelowNote - full.squeakDbBelowNote) - 12.0) < 2.5,
               report ("0.25", quarter) + " / " + report ("1.0", full));
    CHECK (off.squeaks == 0 && off.squeakDbBelowNote > 100.0);

    // The roll, over many lifted chord changes on one engine.
    auto hits = [] (double probability, double moisture, juce::uint32 seed)
    {
        LuthierEngine engine;
        engine.prepare (kSr, kBlock);
        engine.setGuitarType (GuitarType::Dreadnought);
        engine.getPlayingNoise().setSeed (seed);

        SqueakSettings s;
        s.probability = probability;
        s.moisture = moisture;
        engine.setSqueak (s);

        const int string = engine.getNumStrings() - 1;
        juce::AudioBuffer<float> block (2, kBlock);
        juce::MidiBuffer none;

        for (int i = 0; i < 40; ++i)
        {
            NoteOnEvent on;
            on.stringIndex = string;
            on.fretPosition = (i % 2 == 0) ? 2.0 : 8.0;
            on.pitchHz = 110.0;
            engine.triggerNoteNow (on);
            block.clear();
            engine.processBlock (block, none);

            NoteOffEvent off;
            off.stringIndex = string;
            engine.releaseNoteNow (off);
            block.clear();
            engine.processBlock (block, none);
        }

        return engine.getPlayingNoise().getPool().getTriggerCount (NoiseClass::squeak);
    };

    CHECK_MSG (hits (0.0, 0.35, 1) == 0, "probability 0 squeaked");
    CHECK_MSG (hits (1.0, 0.35, 1) == 39, "probability 1 at default moisture: " + juce::String (hits (1.0, 0.35, 1)) + " of 39");
    CHECK_MSG (hits (1.0, 1.0, 1) == 39, "probability 1 with wet fingers: " + juce::String (hits (1.0, 1.0, 1)) + " of 39");

    const auto half = hits (0.5, 0.35, 1);
    CHECK_MSG (half > 8 && half < 31, "probability 0.5 gave " + juce::String (half) + " of 39");
    CHECK (hits (0.5, 0.35, 1) == half);           // deterministic under the seed
    CHECK (hits (0.5, 0.35, 2) != half || hits (0.5, 0.35, 3) != half);
    CHECK (hits (0.5, 0.9, 1) < half);             // damp fingers roll less often
}

//==============================================================================
//  (c) travel distance and speed
//==============================================================================
LUTHIER_TEST (SqueakRealism, levelLengthAndPitchFollowTravelAndSpeed)
{
    SqueakSettings s;
    StringNoiseInfo e;
    e.wound = true;
    e.windingPitchPerMm = 2.9;      // a 0.046" phosphor-bronze low E
    e.windingDepth = 1.0;
    e.material = StringMaterial::PhosphorBronze;

    const double scale = 648.0;
    auto shift = [&] (double from, double to, double seconds)
    {
        return PlayingNoise::makeSqueak (s, e, 5, PlayingNoise::fretDistanceMm (scale, from, to), seconds,
                                         std::abs (to - from));
    };

    // Farther at the hand's natural pace (25 ms a fret): a longer whistle at
    // much the same pitch, since the hand moves at much the same speed.
    const auto two = shift (2.0, 4.0, 0.018 + 2 * 0.025), seven = shift (2.0, 9.0, 0.018 + 7 * 0.025);
    CHECK (seven.holdMs > two.holdMs * 1.8);
    CHECK (seven.peakHz > two.peakHz * 0.9);
    CHECK (seven.level >= two.level);

    // The same distance faster: higher pitch, shorter tail.
    const auto slow = shift (2.0, 7.0, 0.3), fast = shift (2.0, 7.0, 0.1);
    CHECK_NEAR (fast.peakHz / slow.peakHz, 3.0, 0.05);
    CHECK (fast.decayMs < slow.decayMs);
    CHECK (fast.holdMs < slow.holdMs);
    CHECK (fast.level >= slow.level);

    // A crawl is quieter (3: speed / 300 mm/s) and the floor keeps it a tone.
    const auto crawl = shift (2.0, 4.0, 0.5);
    CHECK (crawl.level < two.level);
    CHECK (crawl.peakHz >= PlayingNoise::kMinSqueakHz);

    // The glide is bell-shaped: up to the peak, down to the end.
    CHECK (seven.startHz < seven.peakHz && seven.endHz < seven.peakHz && seven.endHz > seven.startHz);
}

//==============================================================================
//  (d) material
//==============================================================================
LUTHIER_TEST (SqueakRealism, theWindingSetsTheSqueak)
{
    SqueakSettings s;
    auto on = [&] (StringMaterial m)
    {
        StringNoiseInfo e;
        e.wound = true;
        e.windingPitchPerMm = 2.9;
        e.windingDepth = 1.0;
        e.material = m;
        return PlayingNoise::makeSqueak (s, e, 5, 120.0, 0.15, 5.0);
    };

    using M = StringMaterial;
    const auto pb = on (M::PhosphorBronze), b8020 = on (M::Bronze8020), nps = on (M::NickelPlatedSteel),
               nickel = on (M::PureNickel), stainless = on (M::StainlessSteel), flat = on (M::Flatwound),
               half = on (M::Halfwound), coated = on (M::Coated), nylon = on (M::Nylon);

    // string-squeak.md 4's order.
    CHECK (stainless.level > b8020.level && b8020.level > pb.level && pb.level > nps.level
           && nps.level > nickel.level && nickel.level > coated.level && coated.level > half.level
           && half.level > flat.level);
    CHECK (nylon.level < pb.level);
    CHECK_MSG (gainToDb (pb.level / flat.level) > 15.0, "flatwound only " + juce::String (gainToDb (pb.level / flat.level), 1) + " dB under bronze");

    // The texture differs too, not only the level.
    CHECK (b8020.texture == NoiseTexture::coarse && pb.texture == NoiseTexture::medium
           && nps.texture == NoiseTexture::fine && flat.texture == NoiseTexture::smooth);
    CHECK (b8020.brightness > pb.brightness && pb.brightness > nps.brightness && nps.brightness > coated.brightness);

    // And at the output: flatwound against phosphor bronze on the same guitar.
    ChordChange c;
    c.probability = 1.0;
    c.material = M::PhosphorBronze;
    const auto bronze = measure (c);
    c.material = M::Flatwound;
    const auto flatOut = measure (c);
    CHECK_MSG (flatOut.squeakDbBelowNote > bronze.squeakDbBelowNote + 15.0,
               report ("flatwound", flatOut) + " vs " + report ("bronze", bronze));

    // Plain strings never squeak, whatever the material.
    StringNoiseInfo plain;
    plain.material = M::StainlessSteel;
    CHECK (PlayingNoise::makeSqueak (s, plain, 0, 120.0, 0.15, 5.0).level == 0.0);
}

//==============================================================================
//  (e) no two alike, and the same seed repeats
//==============================================================================
LUTHIER_TEST (SqueakRealism, consecutiveSqueaksDifferAndASeedRepeatsThem)
{
    auto sequence = [] (juce::uint32 seed)
    {
        PlayingNoise noise;
        noise.prepare (kSr);
        noise.setSeed (seed);

        SqueakSettings s;
        s.probability = 1.0;
        noise.setSqueak (s);

        StringNoiseInfo e;
        e.wound = true;
        e.windingPitchPerMm = 2.9;
        e.windingDepth = 1.0;
        e.material = StringMaterial::PhosphorBronze;

        std::vector<NoiseEvent> events;

        for (juce::uint32 i = 0; i < 6; ++i)
        {
            noise.onShift (5, e, 648.0, 2.0, 7.0, 0.15, i);
            events.push_back (noise.getPool().getGenerator (NoiseClass::squeak, (int) i)->getEvent());
        }

        return events;
    };

    const auto a = sequence (11), b = sequence (11), other = sequence (12);

    int differentPitch = 0, differentLength = 0, differentTone = 0;

    for (size_t i = 1; i < a.size(); ++i)
    {
        differentPitch += std::abs (a[i].peakHz - a[i - 1].peakHz) > 1.0;
        differentLength += std::abs (a[i].holdMs - a[i - 1].holdMs) > 0.5;
        differentTone += std::abs (a[i].brightness - a[i - 1].brightness) > 0.005;
    }

    CHECK_MSG (differentPitch >= 4 && differentLength >= 4 && differentTone >= 4,
               "same move, same squeak: " + juce::String (differentPitch) + " pitches, " + juce::String (differentLength)
               + " lengths, " + juce::String (differentTone) + " tones differed of 5");

    for (size_t i = 0; i < a.size(); ++i)
    {
        CHECK (a[i].peakHz == b[i].peakHz && a[i].holdMs == b[i].holdMs && a[i].level == b[i].level);
        CHECK (std::abs (a[i].peakHz / a[i].startHz - 1.0) > 0.3);   // every one glides
    }

    CHECK (a[0].peakHz != other[0].peakHz);

    // The scatter stays within what a finger does: +/- 9 % of pitch.
    for (const auto& ev : a)
        CHECK (ev.peakHz > a[0].peakHz / 1.2 && ev.peakHz < a[0].peakHz * 1.2);
}

//==============================================================================
//  (f), (g) the controls and the presets
//==============================================================================
LUTHIER_TEST (SqueakRealism, theControlsExistAndTheFactoryAcousticsKeepTheDefault)
{
    // (f): every squeak parameter is a real, automatable parameter with a
    // sensible default; the CHARACTER tab's STRING NOISE group attaches each
    // (NoiseUi.squeakStylesApplyAndReadModified covers the sliders live).
    LuthierAudioProcessor processor;
    auto& state = processor.getState();

    auto defaultOf = [&] (const char* id)
    {
        auto* p = state.getParameter (id);
        CHECK_MSG (p != nullptr, juce::String (id) + " is not a parameter");
        return p == nullptr ? -1.0 : (double) p->convertFrom0to1 (p->getDefaultValue());
    };

    CHECK_NEAR (defaultOf (ParamIDs::squeakAmount), 0.25, 1.0e-6);
    CHECK_NEAR (defaultOf (ParamIDs::squeakProbability), 0.65, 1.0e-6);
    CHECK_NEAR (defaultOf (ParamIDs::squeakMoisture), 0.35, 1.0e-6);
    CHECK_NEAR (defaultOf (ParamIDs::squeakPressure), 0.5, 1.0e-6);
    CHECK_NEAR (defaultOf (ParamIDs::squeakMinTravel), 1.5, 1.0e-6);
    CHECK (state.getParameter (ParamIDs::squeakStyle) != nullptr);

    // (g): no factory acoustic turns the squeak down below the ship default.
    int acoustics = 0;

    for (int i = 0; i < FactoryPresets::getNumPresets(); ++i)
    {
        const auto& def = FactoryPresets::getPreset (i);
        const juce::String category (def.category);

        if (! category.containsIgnoreCase ("acoustic"))
            continue;

        ++acoustics;

        for (int k = 0; k < def.numEntries; ++k)
            if (juce::String (def.entries[k].paramId) == ParamIDs::squeakAmount)
                CHECK_MSG (def.entries[k].plainValue >= 0.25, juce::String (def.name) + " turns the squeak down");
    }

    CHECK (acoustics > 0);
}

