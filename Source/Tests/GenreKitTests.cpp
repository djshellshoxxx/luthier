/*  Genre kit tests (rhythm-engine.md section 7 and 10).

    The kits are the layer where the rest of the rhythm engine gets used in
    anger, so these tests are mostly about referential integrity: every kit has
    to name patterns that exist, and applying one has to leave the engine in a
    state that actually plays.

    The string-mask tests live here too. They belong with the factory data
    rather than with the scheduler, because what they check is that the masks in
    the pattern table select strings that a six-string guitar has.
*/

#include "TestFramework.h"

#include "../Rhythm/GenreKit.h"
#include "../Rhythm/RhythmEngine.h"
#include "../Rhythm/Patterns.h"
#include "../Model/Playing/TuningEngine.h"
#include "../Model/Playing/ChordVoicer.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    /** An engine holding an E minor chord with humanisation off, ready to be
        asked what a pattern does. */
    struct KitFixture
    {
        KitFixture()
        {
            tuning.prepare (kSr);
            tuning.setNumStrings (6);
            tuning.setTuningPreset (TuningPreset::Standard);

            voicer.prepare (&tuning, 6);

            engine.prepare (kSr, kBlock, &tuning, &voicer);
            engine.setNumStrings (6);
            engine.setEnabled (true);

            RhythmHumanise flat;
            flat.timingMs = 0.0;
            flat.velocityPercent = 0.0;
            flat.missPercent = 0.0;
            flat.ghostPercent = 0.0;
            flat.amount = 0.0;
            engine.setHumanise (flat);

            engine.setStrumDurationMs (1.0);
        }

        void holdChord()
        {
            juce::MidiBuffer midi;

            // E minor across all six strings, so that any mask has something to
            // select on every string.
            for (int note : { 40, 47, 52, 55, 59, 64 })
                midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);

            engine.handleMidi (midi, 0);
        }

        /** Runs one bar at 120 bpm and returns how many note-ons came out. */
        int countNotesInOneBar()
        {
            int total = 0;

            const double beatsPerBlock = (double) kBlock / kSr * (120.0 / 60.0);

            RhythmTransport transport;
            transport.bpm = 120.0;
            transport.isPlaying = true;
            transport.ppqPosition = 0.0;

            PlayEventQueue out;

            // Four beats is one bar of the sixteenth grid every factory pattern
            // is written on.
            while (transport.ppqPosition < 4.0)
            {
                out.clear();
                engine.processBlock (kBlock, transport, out);
                total += out.getNumNoteOns();

                transport.ppqPosition += beatsPerBlock;
            }

            return total;
        }

        TuningEngine tuning;
        ChordVoicer voicer;
        RhythmEngine engine;
    };
}

//==============================================================================
/*  rhythm-engine 7 names the kits that must ship. Check the library holds at
    least that many and that each is internally coherent. */
LUTHIER_TEST (GenreKits, factoryKitsAreWellFormed)
{
    GenreKitLibrary kits;

    CHECK_MSG (kits.getNumKits() >= 26,
               "only " + juce::String (kits.getNumKits()) + " genre kits; the spec names 26");

    for (int i = 0; i < kits.getNumKits(); ++i)
    {
        const auto& kit = kits.getKit (i);

        CHECK_MSG (kit.isValid(), "kit " + juce::String (i) + " has no name");

        CHECK_MSG (kit.voicingDensity >= 0.0 && kit.voicingDensity <= 100.0,
                   kit.name + ": density " + juce::String (kit.voicingDensity) + " out of range");

        CHECK_MSG (kit.strumDurationMs >= 1.0 && kit.strumDurationMs <= 250.0,
                   kit.name + ": strum duration out of range");

        CHECK_MSG (kit.strumEvenness >= 0.0 && kit.strumEvenness <= 1.0,
                   kit.name + ": evenness out of range");

        CHECK_MSG (! kit.tags.isEmpty(), kit.name + " carries no tags");

        CHECK_MSG (kit.preferredPreset.isNotEmpty(),
                   kit.name + " names no preferred rig");

        CHECK_MSG (! kit.strumPatterns.isEmpty() || ! kit.fingerpickPatterns.isEmpty(),
                   kit.name + " names no patterns at all");
    }
}

//==============================================================================
/*  A kit that names a pattern the library does not hold would silently do
    nothing when applied, which is the failure mode worth catching early. */
LUTHIER_TEST (GenreKits, everyKitResolvesEveryPatternItNames)
{
    GenreKitLibrary kits;
    PatternLibrary patterns;

    for (int i = 0; i < kits.getNumKits(); ++i)
    {
        const auto& kit = kits.getKit (i);

        for (const auto* list : { &kit.strumPatterns, &kit.fingerpickPatterns })
            for (const auto& name : *list)
                CHECK_MSG (patterns.indexOf (name) >= 0,
                           kit.name + " names pattern \"" + name + "\", which the library has not got");
    }
}

//==============================================================================
/*  Applying a kit has to install a pattern and push every one of its settings
    into the engine. */
LUTHIER_TEST (GenreKits, applyingAKitInstallsItsPatternAndSettings)
{
    GenreKitLibrary kits;
    PatternLibrary patterns;
    KitFixture fixture;

    for (int i = 0; i < kits.getNumKits(); ++i)
    {
        const auto& kit = kits.getKit (i);

        CHECK_MSG (GenreKitLibrary::apply (kit, fixture.engine, patterns),
                   kit.name + " installed no pattern");

        CHECK_MSG (fixture.engine.getVoicingStyle() == kit.voicingStyle,
                   kit.name + ": voicing style not applied");

        CHECK_NEAR (fixture.engine.getVoicingDensity(), kit.voicingDensity, 0.001);
        CHECK_NEAR (fixture.engine.getStrumDurationMs(), kit.strumDurationMs, 0.001);

        const auto humanise = fixture.engine.getHumanise();
        CHECK_NEAR (humanise.timingMs, kit.humanise.timingMs, 0.001);
        CHECK_NEAR (humanise.missPercent, kit.humanise.missPercent, 0.001);

        const auto installed = fixture.engine.getPattern();
        CHECK_MSG (! installed.isEmpty(), kit.name + " installed an empty pattern");
    }
}

//==============================================================================
/*  rhythm-engine 7: the rig reference is a soft one. Applying a kit must not
    change any parameter, only name the preset it was written against. */
LUTHIER_TEST (GenreKits, applyingAKitDoesNotLoadItsRig)
{
    GenreKitLibrary kits;
    PatternLibrary patterns;
    KitFixture fixture;

    const int index = kits.indexOf ("Metal Chug");
    CHECK (index >= 0);

    if (index < 0)
        return;

    const auto& kit = kits.getKit (index);

    CHECK (kit.preferredPreset == "Modern Metal Chug");

    // apply() takes an engine and a pattern library and nothing else: there is
    // no route from here to the parameter tree, which is the guarantee.
    GenreKitLibrary::apply (kit, fixture.engine, patterns);

    CHECK (kit.preferredPreset == "Modern Metal Chug");
}

//==============================================================================
/*  A kit round-trips through JSON, because the factory kits ship as files and a
    user can edit them. */
LUTHIER_TEST (GenreKits, kitsRoundTripThroughJson)
{
    GenreKitLibrary kits;

    for (int i = 0; i < kits.getNumKits(); ++i)
    {
        const auto& original = kits.getKit (i);

        // Through a real JSON string rather than just the var, so that anything
        // the serialiser cannot express is caught.
        const auto text = juce::JSON::toString (original.toVar(), false);
        const auto restored = GenreKit::fromVar (juce::JSON::parse (text));

        CHECK_MSG (restored.name == original.name,
                   original.name + ": name did not survive the round trip");

        CHECK_MSG (restored.voicingStyle == original.voicingStyle,
                   original.name + ": voicing style did not survive the round trip");

        CHECK_NEAR (restored.voicingDensity, original.voicingDensity, 0.001);
        CHECK_NEAR (restored.strumDurationMs, original.strumDurationMs, 0.001);
        CHECK_NEAR (restored.strumEvenness, original.strumEvenness, 0.001);
        CHECK_NEAR (restored.humanise.timingMs, original.humanise.timingMs, 0.001);
        CHECK_NEAR (restored.humanise.ghostPercent, original.humanise.ghostPercent, 0.001);

        CHECK_MSG (restored.strumPatterns == original.strumPatterns,
                   original.name + ": strum pattern list did not survive");

        CHECK_MSG (restored.fingerpickPatterns == original.fingerpickPatterns,
                   original.name + ": fingerpick pattern list did not survive");

        CHECK_MSG (restored.preferredPreset == original.preferredPreset,
                   original.name + ": rig reference did not survive");
    }
}

//==============================================================================
/*  The dice in the rhythm panel picks a pattern from within the kit, so it must
    never wander outside the kit or return an index the library cannot serve. */
LUTHIER_TEST (GenreKits, randomPatternStaysInsideTheKit)
{
    GenreKitLibrary kits;
    PatternLibrary patterns;

    juce::Random random (0x5eed);

    for (int i = 0; i < kits.getNumKits(); ++i)
    {
        const auto& kit = kits.getKit (i);

        for (int trial = 0; trial < 32; ++trial)
        {
            const int index = GenreKitLibrary::chooseRandomPattern (kit, patterns, random);

            CHECK_MSG (index >= 0, kit.name + " produced no pattern from the dice");

            if (index < 0)
                continue;

            const auto name = patterns.getPattern (index).getName();

            CHECK_MSG (kit.strumPatterns.contains (name) || kit.fingerpickPatterns.contains (name),
                       kit.name + " rolled \"" + name + "\", which is not one of its patterns");
        }
    }
}

//==============================================================================
/*  Regression: string index 0 is the high E, so the low strings carry the high
    indices. A mask built for the wrong end of the range selects no string at
    all on a six-string guitar, and the pattern is silent.

    "Metal Chug" and "Djent Grid" are masked to the low strings on every step,
    which makes them the patterns that fail first if the masks are wrong. */
LUTHIER_TEST (GenreKits, factoryMasksSelectStringsASixStringHas)
{
    PatternLibrary library;

    for (int i = 0; i < library.getNumPatterns(); ++i)
    {
        const auto& pattern = library.getPattern (i);

        if (pattern.getKind() != RhythmPattern::Kind::strum)
            continue;

        for (int step = 0; step < pattern.getLength(); ++step)
        {
            const auto cell = pattern.getStrumStep (step);

            if (cell.isRest())
                continue;

            // The bits a six-string instrument occupies are 0 to 5.
            CHECK_MSG ((cell.stringMask & 0x003Fu) != 0,
                       pattern.getName() + " step " + juce::String (step)
                         + ": mask " + juce::String::toHexString ((int) cell.stringMask)
                         + " selects no string on a six-string guitar");
        }
    }
}

//==============================================================================
/*  The same regression, proven through the scheduler rather than by reading the
    data: every factory strum pattern must actually produce notes. */
LUTHIER_TEST (GenreKits, everyFactoryStrumPatternSounds)
{
    PatternLibrary library;

    for (int i = 0; i < library.getNumPatterns(); ++i)
    {
        const auto& pattern = library.getPattern (i);

        if (pattern.getKind() != RhythmPattern::Kind::strum || pattern.isEmpty())
            continue;

        KitFixture fixture;
        fixture.holdChord();
        fixture.engine.setPattern (pattern);

        CHECK_MSG (fixture.countNotesInOneBar() > 0,
                   pattern.getName() + " produced no notes in a bar");
    }
}

//==============================================================================
/*  And every kit, applied, must produce notes: the end-to-end version of the
    two tests above. */
LUTHIER_TEST (GenreKits, everyKitSoundsWhenApplied)
{
    GenreKitLibrary kits;
    PatternLibrary patterns;

    for (int i = 0; i < kits.getNumKits(); ++i)
    {
        const auto& kit = kits.getKit (i);

        KitFixture fixture;
        fixture.holdChord();

        GenreKitLibrary::apply (kit, fixture.engine, patterns);

        // apply() overwrites the humanisation with the kit's own, and a kit with
        // a miss chance could in principle drop every stroke in a bar. Flatten it
        // again so the test measures the pattern, not the dice.
        RhythmHumanise flat;
        flat.amount = 0.0;
        fixture.engine.setHumanise (flat);

        CHECK_MSG (fixture.countNotesInOneBar() > 0,
                   kit.name + " produced no notes in a bar");
    }
}

//==============================================================================
/*  A capo removes every fret below it from play, shortens the neck, and moves
    the pitch (rhythm-engine 3, ambiguity-resolutions 4.5).

    This test used to assert `fretPosition >= capo`, because the capo lived in
    RhythmEngine, fret positions were absolute, and the voicer was filtered by a
    minFret set to the capo. The capo is TuningEngine's now and fret positions are
    measured from it, so "below the capo" is not expressible any more and that
    assertion would only be re-stating the coordinate system.

    What is worth checking in the new arrangement is what can still go wrong:

      - nothing is voiced below the capo, which is now fret 0;
      - nothing is voiced past the end of a neck the capo has shortened, which is
        a real hazard because the voicer's own maxFret does not know about capos;
      - the capo actually changes the pitch, which is the whole point and which
        the old test could not see at all - it only ever looked at fret numbers,
        and a capo that transposed nothing would have passed it.
*/
LUTHIER_TEST (GenreKits, capoRemovesFretsBelowItAndMovesThePitch)
{
    for (int capo : { 0, 2, 5, 7 })
    {
        KitFixture fixture;
        fixture.engine.setCapoFret (capo);

        CHECK_MSG (fixture.engine.getCapoFret() == capo,
                   "the capo did not take: asked for " + juce::String (capo)
                     + ", got " + juce::String (fixture.engine.getCapoFret()));

        fixture.holdChord();
        fixture.engine.setPattern (PatternLibrary().getPattern (
            PatternLibrary().indexOf ("Folk Down Up")));

        RhythmTransport transport;
        transport.bpm = 120.0;
        transport.isPlaying = true;
        transport.ppqPosition = 0.0;

        PlayEventQueue out;

        const double beatsPerBlock = (double) kBlock / kSr * 2.0;

        int notesSeen = 0;

        while (transport.ppqPosition < 4.0)
        {
            out.clear();
            fixture.engine.processBlock (kBlock, transport, out);

            for (int i = 0; i < out.getNumNoteOns(); ++i)
            {
                const auto& note = out.getNoteOn (i);
                ++notesSeen;

                CHECK_MSG (note.fretPosition >= -0.001,
                           "capo at " + juce::String (capo) + ": note below the capo, at fret "
                             + juce::String (note.fretPosition));

                /*  The neck is shorter by the capo. Checked per string because
                    max frets is per string, and a note past the end would be a
                    pitch no guitar can make. */
                const int highest = fixture.tuning.getHighestPlayableFret (note.stringIndex);

                CHECK_MSG (note.fretPosition <= (double) highest + 0.001,
                           "capo at " + juce::String (capo) + ": note at fret "
                             + juce::String (note.fretPosition)
                             + " is past the end of a neck that stops at "
                             + juce::String (highest));
            }

            transport.ppqPosition += beatsPerBlock;
        }

        CHECK_MSG (notesSeen > 0,
                   "capo at " + juce::String (capo) + ": the bar produced no notes at all, "
                   "so nothing above was actually checked");
    }

    //--------------------------------------------------------------------------
    /*  The capo moves the pitch. Five frets is a fourth, so an open string with a
        capo at 5 must sound exactly what fret 5 sounded without one - not merely
        "higher", which a wrong-but-plausible implementation would also manage. */
    {
        TuningEngine tuning;
        tuning.setNumStrings (6);

        // No intonation slope: this is about the capo, and the slope would add a
        // few cents that have nothing to do with it.
        for (int s = 0; s < 6; ++s)
            tuning.setIntonationSlope (s, 0.0);

        const double fretFiveNoCapo = tuning.computeFrequency (0, 5.0);
        const double openNoCapo     = tuning.getEffectiveOpenFrequency (0);

        tuning.setCapoFret (5);

        const double openWithCapo = tuning.getEffectiveOpenFrequency (0);

        CHECK_MSG (std::abs (openWithCapo - fretFiveNoCapo) < 0.01,
                   "a capo at 5 does not make the open string sound like fret 5: open is "
                     + juce::String (openWithCapo) + " Hz, fret 5 was "
                     + juce::String (fretFiveNoCapo) + " Hz");

        CHECK_MSG (openWithCapo > openNoCapo * 1.3,
                   "the capo did not raise the open string at all");

        // And fret 0 with the capo on is that same note, because fret positions
        // are measured from the capo.
        CHECK_MSG (std::abs (tuning.computeFrequency (0, 0.0) - fretFiveNoCapo) < 0.01,
                   "fret 0 with a capo at 5 is not the capo'd note");

        // The neck lost five frets.
        CHECK_MSG (tuning.getHighestPlayableFret (0)
                     == tuning.getStringTuning (0).maxFrets - 5,
                   "the playable span did not shrink by the capo");

        // Taking it off puts everything back.
        tuning.setCapoFret (0);

        CHECK_MSG (std::abs (tuning.getEffectiveOpenFrequency (0) - openNoCapo) < 1.0e-9,
                   "removing the capo did not restore the open string");
    }
}
