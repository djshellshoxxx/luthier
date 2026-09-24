/*  The noise pool, the pick and the fretting hand (pick-noise.md 9,
    string-squeak.md 13).

    Most checks are on the events PlayingNoise builds, because that is where
    the physics is: pitch from winding and speed, level from angle and
    velocity. The pool and the engine are then checked for the things only
    they can get wrong - stealing, determinism, and noise actually reaching
    the output through the instrument.
*/

#include "TestFramework.h"

#include "../DSP/Noise/PlayingNoise.h"
#include "../LuthierEngine.h"
#include "../PluginProcessor.h"
#include "../UI/NoiseGroups.h"
#include "../UI/RangesUi.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    StringNoiseInfo woundLowE (StringMaterial material = StringMaterial::PhosphorBronze)
    {
        StringNoiseInfo info;
        info.wound = true;
        info.windingPitchPerMm = 6.5;
        info.windingDepth = 1.0;
        info.material = material;
        return info;
    }

    StringNoiseInfo plainString()
    {
        StringNoiseInfo info;
        info.wound = false;
        return info;
    }

    double renderPeak (const NoiseEvent& e, NoiseEngine& pool)
    {
        pool.trigger (e);

        std::array<double, 12> exc {}, surf {};
        double peakValue = 0.0;

        for (int i = 0; i < 48000; ++i)
            peakValue = juce::jmax (peakValue, std::abs (pool.processSample (exc.data(), surf.data(), 6)));

        return peakValue;
    }
}

//==============================================================================
LUTHIER_TEST (NoisePool, aFullPoolStealsTheOldest)
{
    NoiseEngine pool;
    pool.prepare (48000.0);

    NoiseEvent e;
    e.noiseClass = NoiseClass::pickClick;
    e.level = 0.1;
    e.holdMs = 500.0;

    juce::Array<int> slots;

    for (int i = 0; i < 20; ++i)
        slots.add (pool.trigger (e));

    CHECK (pool.getActiveCount (NoiseClass::pickClick) == 16);
    CHECK_MSG (pool.getStealCount (NoiseClass::pickClick) == 4,
               "expected 4 steals, got " + juce::String (pool.getStealCount (NoiseClass::pickClick)));

    // The four that were stolen are the first four started: triggers 17-20
    // land in the slots triggers 1-4 had.
    for (int i = 0; i < 4; ++i)
        CHECK (slots[16 + i] == slots[i]);

    // Degraded, the pool is eight (performance-budget.md 9 step 4).
    pool.setDegraded (true);
    CHECK (pool.getPoolLimit (NoiseClass::pickClick) == 8);
}

LUTHIER_TEST (NoisePool, zeroIsFree)
{
    NoiseEngine pool;
    pool.prepare (48000.0);

    NoiseEvent silent;
    silent.level = 0.0;

    for (int i = 0; i < 10000; ++i)
        CHECK_MSG (pool.trigger (silent) < 0, "a silent event took a generator");

    CHECK (pool.getTriggerCount (NoiseClass::pickClick) == 0);
    CHECK (pool.isIdle());
}

LUTHIER_TEST (NoisePool, aSeedRepeatsExactly)
{
    auto run = [] (juce::uint32 seed)
    {
        PlayingNoise noise;
        noise.prepare (48000.0);
        noise.setSeed (seed);

        std::vector<double> out;
        std::array<double, 12> exc {}, surf {};

        for (int note = 0; note < 8; ++note)
        {
            noise.onPluck (note % 6, woundLowE(), 0.8);
            noise.onShift (note % 6, woundLowE(), 648.0, 2.0, 7.0, 0.1, (juce::uint32) note);

            for (int i = 0; i < 2000; ++i)
                out.push_back (noise.processSample (exc.data(), surf.data(), 6));
        }

        return out;
    };

    const auto a = run (1234), b = run (1234), c = run (99);

    CHECK_MSG (a == b, "the same seed did not produce the same noise");
    CHECK_MSG (a != c, "a different seed produced the same noise");
}

//==============================================================================
LUTHIER_TEST (PickNoise, clickScalesWithVelocityToThePower0_7)
{
    PickSettings pick;

    for (int velocity = 20; velocity <= 127; velocity += 13)
    {
        const double v = velocity / 127.0;
        const double ratio = PlayingNoise::makeClick (pick, 0, v).level
                           / PlayingNoise::makeClick (pick, 0, 1.0).level;

        CHECK_MSG (std::abs (gainToDb (ratio) - gainToDb (std::pow (v, 0.7))) < 1.0,
                   "velocity " + juce::String (velocity) + " off the v^0.7 law");
    }
}

LUTHIER_TEST (PickNoise, clickPitchTracksMaterialAndThickness)
{
    PickSettings nylon;
    nylon.material = Excitation::Material::PickNylon;
    nylon.thicknessMm = 0.6;

    PickSettings metal;
    metal.material = Excitation::Material::PickMetal;
    metal.thicknessMm = 3.0;

    const double nylonHz = PlayingNoise::makeClick (nylon, 0, 0.8).startHz;
    const double metalHz = PlayingNoise::makeClick (metal, 0, 0.8).startHz;

    CHECK_MSG (nylonHz >= 2.0 * metalHz,
               "nylon 0.6 mm clicks at " + juce::String (nylonHz, 0) + " Hz, metal 3 mm at "
                 + juce::String (metalHz, 0) + " Hz - not an octave apart");

    // And the spec's own anchors: a light nylon around 2.2 kHz, a thick metal
    // under a kilohertz.
    CHECK (nylonHz > 1800.0 && nylonHz < 2800.0);
    CHECK (metalHz < 1000.0);
}

LUTHIER_TEST (PickNoise, angleTradesClickForChirp)
{
    const auto string = woundLowE();

    auto at = [&string] (double degrees)
    {
        PickSettings pick;
        pick.angleDegrees = degrees;
        return std::make_pair (PlayingNoise::makeClick (pick, 0, 0.8).level,
                               PlayingNoise::makeChirp (pick, string, 0, 0.8).level);
    };

    CHECK_MSG (at (0.0).second == 0.0, "a pick at 0 degrees chirped");

    double maxClick = 0.0, maxChirp = 0.0;

    for (double degrees = 0.0; degrees <= 60.0; degrees += 1.0)
    {
        maxClick = juce::jmax (maxClick, at (degrees).first);
        maxChirp = juce::jmax (maxChirp, at (degrees).second);
    }

    CHECK (at (0.0).first >= maxClick);
    CHECK_MSG (gainToDb (at (45.0).second / maxChirp) > -3.0,
               "chirp at 45 degrees is more than 3 dB under its maximum");
}

LUTHIER_TEST (PickNoise, plainStringsNeverChirp)
{
    // Every factory guitar, every string: the chirp is triggered for wound
    // strings and never for plain ones.
    for (int type = 0; type < (int) GuitarType::NumTypes; ++type)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.setGuitarType ((GuitarType) type);
        engine.setPickMaterialAndFingers (Excitation::Material::PickCelluloid, false);

        PickSettings pick;
        pick.angleDegrees = 30.0;
        engine.setPickNoise (pick);

        int woundStrings = 0;

        for (int s = 0; s < engine.getNumStrings(); ++s)
        {
            if (engine.getStringSpec (s).wound)
                ++woundStrings;

            NoteOnEvent e;
            e.stringIndex = s;
            e.velocity = 0.8;
            e.pitchHz = 220.0;
            engine.triggerNoteNow (e);
        }

        const auto chirps = engine.getPlayingNoise().getPool().getTriggerCount (NoiseClass::pickChirp);

        CHECK_MSG (chirps == woundStrings,
                   juce::String (GuitarLibrary::getName ((GuitarType) type)) + ": "
                     + juce::String (chirps) + " chirps for " + juce::String (woundStrings) + " wound strings");
    }

    CHECK (PlayingNoise::makeChirp (PickSettings(), plainString(), 0, 1.0).level == 0.0);
}

LUTHIER_TEST (PickNoise, fingersNeitherClickNorChirp)
{
    PickSettings pick;
    pick.fingers = true;

    CHECK (PlayingNoise::makeClick (pick, 0, 1.0).level == 0.0);
    CHECK (PlayingNoise::makeChirp (pick, woundLowE(), 0, 1.0).level == 0.0);

    // What fingers do make is about 12 dB under the pick's chirp.
    PickSettings asPick;
    asPick.angleDegrees = 30.0;

    const double fingertip = PlayingNoise::makeFingertipNoise (pick, woundLowE(), 0, 0.8).level;
    const double chirp = PlayingNoise::makeChirp (asPick, woundLowE(), 0, 0.8).level;

    CHECK (fingertip > 0.0);
    CHECK (std::abs (gainToDb (fingertip / chirp) + 12.0) < 1.0);
}

LUTHIER_TEST (PickNoise, aClickSitsAboutThirtyDecibelsUnderTheNote)
{
    // Ground rule 4, measured through the real engine: the difference between a
    // note with the click and the same note without it, against the note's peak.
    auto render = [] (double clickAmount)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.setGuitarType (GuitarType::Stratocaster);
        engine.getCabinetEngine().setEnabled (false);
        engine.getRoomEngine().setEnabled (false);
        engine.getAmpEngine().setGain (0.0);
        engine.setPickMaterialAndFingers (Excitation::Material::PickCelluloid, false);

        PickSettings pick;
        pick.clickAmount = clickAmount;
        pick.chirpAmount = 0.0;
        engine.setPickNoise (pick);

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);

        juce::AudioBuffer<float> block (2, 256);
        std::vector<double> out;

        for (int b = 0; b < 40; ++b)
        {
            block.clear();
            juce::MidiBuffer emptyMidi;
            engine.processBlock (block, b == 0 ? midi : emptyMidi);

            for (int i = 0; i < 256; ++i)
                out.push_back (block.getSample (0, i));
        }

        return out;
    };

    const auto withClick = render (0.5), without = render (0.0);

    double notePeak = 0.0, clickPeak = 0.0;

    for (size_t i = 0; i < without.size(); ++i)
    {
        notePeak = juce::jmax (notePeak, std::abs (without[i]));
        clickPeak = juce::jmax (clickPeak, std::abs (withClick[i] - without[i]));
    }

    const double belowDb = gainToDb (notePeak / juce::jmax (1.0e-12, clickPeak));

    CHECK_MSG (clickPeak > 0.0, "the click never reached the output");
    CHECK_MSG (belowDb > 20.0 && belowDb < 42.0,
               "the click sits " + juce::String (belowDb, 1) + " dB under the note, not 25-35");
}

//==============================================================================
LUTHIER_TEST (Squeak, pitchTracksSpeedAndWinding)
{
    SqueakSettings s;

    // 300 mm/s on 6.5 wraps/mm: 90 mm in 0.3 s.
    const auto e = PlayingNoise::makeSqueak (s, woundLowE(), 0, 90.0, 0.3, 4.0);
    CHECK_MSG (std::abs (e.endHz - 1950.0) / 1950.0 < 0.1,
               "peak at " + juce::String (e.endHz, 0) + " Hz, expected about 1950");

    const auto faster = PlayingNoise::makeSqueak (s, woundLowE(), 0, 90.0, 0.15, 4.0);
    CHECK (std::abs (faster.endHz / e.endHz - 2.0) < 0.05);

    // The glide: an accelerating shift starts well below where it peaks.
    CHECK (e.endHz > e.startHz * 1.15);
}

LUTHIER_TEST (Squeak, flatwoundIsNearlySilentAndPlainIsSilent)
{
    SqueakSettings s;

    const double bronze = PlayingNoise::makeSqueak (s, woundLowE (StringMaterial::PhosphorBronze), 0, 90.0, 0.3, 4.0).level;
    const double flat = PlayingNoise::makeSqueak (s, woundLowE (StringMaterial::Flatwound), 0, 90.0, 0.3, 4.0).level;

    CHECK_MSG (gainToDb (bronze / flat) >= 15.0,
               "flatwound is only " + juce::String (gainToDb (bronze / flat), 1) + " dB quieter");

    CHECK (PlayingNoise::makeSqueak (s, plainString(), 0, 90.0, 0.3, 4.0).level == 0.0);
}

LUTHIER_TEST (Squeak, theMinimumTravelIsRespected)
{
    SqueakSettings s;
    s.minTravelFrets = 1.5;

    CHECK (PlayingNoise::makeSqueak (s, woundLowE(), 0, 30.0, 0.1, 1.0).level == 0.0);
    CHECK (PlayingNoise::makeSqueak (s, woundLowE(), 0, 60.0, 0.1, 2.0).level > 0.0);
}

LUTHIER_TEST (Squeak, theProbabilityRollIsDeterministic)
{
    auto pattern = [] (juce::uint32 seed)
    {
        PlayingNoise noise;
        noise.prepare (48000.0);
        noise.setSeed (seed);

        juce::String hits;

        for (juce::uint32 i = 0; i < 64; ++i)
            hits << (noise.onShift (0, woundLowE(), 648.0, 2.0, 7.0, 0.1, i) ? "1" : "0");

        return hits;
    };

    CHECK (pattern (7) == pattern (7));
    CHECK (pattern (7) != pattern (8));

    // And it is a probability: at 0.65 x (1.35 - 0.35) some shifts are silent.
    const auto p = pattern (7);
    CHECK (p.containsChar ('0') && p.containsChar ('1'));
}

LUTHIER_TEST (Squeak, zeroIsFreeAndSlideModeSuppressesIt)
{
    PlayingNoise noise;
    noise.prepare (48000.0);

    SqueakSettings off;
    off.amount = 0.0;
    noise.setSqueak (off);

    for (juce::uint32 i = 0; i < 10000; ++i)
        noise.onShift (0, woundLowE(), 648.0, 2.0, 7.0, 0.1, i);

    CHECK (noise.getPool().getTriggerCount (NoiseClass::squeak) == 0);

    SqueakSettings slide;
    slide.slideMode = true;
    CHECK (PlayingNoise::makeSqueak (slide, woundLowE(), 0, 90.0, 0.3, 4.0).level == 0.0);
}

LUTHIER_TEST (Squeak, aLegatoSlideInTheEngineSqueaksAndABendDoesNot)
{
    LuthierEngine engine;
    engine.prepare (48000.0, 256);
    engine.setGuitarType (GuitarType::Dreadnought);

    SqueakSettings always;
    always.probability = 1.0;
    always.moisture = 0.0;
    engine.setSqueak (always);

    // Find a wound string.
    int wound = -1;

    for (int s = 0; s < engine.getNumStrings() && wound < 0; ++s)
        if (engine.getStringSpec (s).wound)
            wound = s;

    CHECK (wound >= 0);

    NoteOnEvent pluck;
    pluck.stringIndex = wound;
    pluck.fretPosition = 2.0;
    pluck.velocity = 0.8;
    engine.triggerNoteNow (pluck);

    auto& pool = engine.getPlayingNoise().getPool();
    CHECK (pool.getTriggerCount (NoiseClass::squeak) == 0);

    NoteOnEvent slide = pluck;
    slide.technique = Technique::Slide;
    slide.slideFromFret = 2.0;
    slide.fretPosition = 7.0;
    slide.slideSeconds = 0.12;
    engine.triggerNoteNow (slide);

    CHECK_MSG (pool.getTriggerCount (NoiseClass::squeak) == 1, "a five-fret legato slide did not squeak");
}

LUTHIER_TEST (Squeak, aShiftSitsTwentyToThirtyDecibelsUnderTheNote)
{
    // string-squeak.md 0.4, on an acoustic where it is most obvious: a normal
    // shift with every squeak forced to happen, against the same shift silent.
    auto render = [] (double amount)
    {
        LuthierEngine engine;
        engine.prepare (48000.0, 256);
        engine.setGuitarType (GuitarType::Dreadnought);
        engine.getCabinetEngine().setEnabled (false);
        engine.getRoomEngine().setEnabled (false);
        engine.getAmpEngine().setGain (0.0);

        PickSettings quietPick;
        quietPick.clickAmount = 0.0;
        quietPick.chirpAmount = 0.0;
        engine.setPickNoise (quietPick);

        SqueakSettings s;
        s.amount = amount;
        s.probability = 1.0;
        engine.setSqueak (s);

        int wound = 0;

        for (int i = 0; i < engine.getNumStrings(); ++i)
            if (engine.getStringSpec (i).wound)
                wound = i;

        juce::AudioBuffer<float> block (2, 256);
        std::vector<double> out;

        for (int b = 0; b < 60; ++b)
        {
            if (b == 0 || b == 20)
            {
                NoteOnEvent e;
                e.stringIndex = wound;
                e.velocity = 0.8;
                e.fretPosition = b == 0 ? 2.0 : 7.0;
                e.pitchHz = 110.0 * std::pow (2.0, e.fretPosition / 12.0);

                if (b == 20)
                {
                    e.technique = Technique::Slide;
                    e.slideFromFret = 2.0;
                    e.slideSeconds = 0.15;
                }

                engine.triggerNoteNow (e);
            }

            block.clear();
            juce::MidiBuffer none;
            engine.processBlock (block, none);

            for (int i = 0; i < 256; ++i)
                out.push_back (block.getSample (0, i));
        }

        return out;
    };

    const auto squeaky = render (0.35), silent = render (0.0);

    double notePeak = 0.0, squeakPeak = 0.0;

    for (size_t i = 0; i < silent.size(); ++i)
    {
        notePeak = juce::jmax (notePeak, std::abs (silent[i]));
        squeakPeak = juce::jmax (squeakPeak, std::abs (squeaky[i] - silent[i]));
    }

    const double belowDb = gainToDb (notePeak / juce::jmax (1.0e-12, squeakPeak));

    CHECK_MSG (squeakPeak > 0.0, "the squeak never reached the output");
    CHECK_MSG (belowDb > 15.0 && belowDb < 35.0,
               "the squeak sits " + juce::String (belowDb, 1) + " dB under the note, not 20-30");
}

//==============================================================================
LUTHIER_TEST (NoiseUi, squeakStylesApplyAndReadModified)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    NoiseGroups groups (processor);
    groups.setSize (400, groups.preferredHeight());

    // onboarding.md 1: the ship default is Natural with the amount at 0.25.
    CHECK_MSG (groups.describeSqueakStyle() == "Natural (modified)",
               "a fresh instance reads \"" + groups.describeSqueakStyle() + "\"");

    groups.applySqueakStyle (4);
    CHECK (groups.describeSqueakStyle() == "Exaggerated");

    auto* amount = processor.getState().getParameter (ParamIDs::squeakAmount);
    CHECK (std::abs (amount->getValue() - 1.0f) < 1.0e-4f);

    // Moving one control marks the style modified; undo puts the style back.
    amount->setValueNotifyingHost (0.5f);
    CHECK (groups.describeSqueakStyle() == "Exaggerated (modified)");

    processor.undo();
    CHECK_MSG (groups.describeSqueakStyle() == "Natural (modified)",
               "undo left the style as \"" + groups.describeSqueakStyle() + "\"");
}

LUTHIER_TEST (NoiseUi, theEventStripShowsWhatTheEngineTriggered)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    NoiseEventStrip strip (processor);
    strip.setSize (300, NoiseEventStrip::preferredHeight);

    CHECK (strip.pollNow() == 0);

    NoiseEvent e;
    e.level = 0.1;
    e.noiseClass = NoiseClass::squeak;

    for (int i = 0; i < 5; ++i)
        processor.getEngine().getPlayingNoise().getPool().trigger (e);

    CHECK_MSG (strip.pollNow() == 5, "the strip shows " + juce::String (strip.getNumShown()) + " of 5 events");
    CHECK (! strip.isStale());
}

LUTHIER_TEST (NoiseUi, theCharacterTabCarriesAPadlockWhenUnlocked)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    RangesUi::RangeTabButton tab ("CHARACTER", processor, { RangeFamily::pick, RangeFamily::squeak });
    CHECK (tab.getPadlockState() == 0);

    RangeState unlocked;
    unlocked.setFamilyAdvanced (RangeFamily::squeak, true);
    processor.changeRanges (unlocked, "test");
    CHECK (tab.getPadlockState() == 2);

    // Unlocking the amp family is not the CHARACTER tab's business.
    RangeState amp;
    amp.setFamilyAdvanced (RangeFamily::amp, true);
    processor.changeRanges (amp, "test");
    CHECK (tab.getPadlockState() == 0);
}
