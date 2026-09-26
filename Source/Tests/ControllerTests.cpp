/*  Controller integration tests (controllers.md section 7).

    The profiles are data, so most of what can go wrong is a profile that names
    something the plugin has not got, or a routing table that sends a string to a
    channel nothing will ever arrive on. Those are what the first tests check.

    The rest exercise the behaviour the profiles turn on: per-channel routing,
    MPE stickiness, the latency wizard's stability, and what happens when two
    controllers fight over the same string.
*/

#include "TestFramework.h"

#include "../Controllers/ControllerProfile.h"
#include "../Model/Playing/MidiInterpreter.h"
#include "../Model/Playing/TuningEngine.h"
#include "../Model/Playing/TechniqueEngine.h"
#include "../Model/Playing/ChordVoicer.h"
#include "../Model/Playing/RubricVoicer.h"
#include "../PluginProcessor.h"
#include "../UI/OptionsPages.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;

    /** An interpreter wired up the way the engine wires one. */
    struct InterpreterFixture
    {
        InterpreterFixture()
        {
            tuning.prepare (kSr);
            tuning.setNumStrings (6);
            tuning.setTuningPreset (TuningPreset::Standard);

            voicer.prepare (&tuning, 6);
            technique.prepare (kSr, 6);

            interpreter.prepare (kSr, 6);
            interpreter.setEngines (&tuning, &technique, &voicer);
            interpreter.setNumStrings (6);

            // The chord window groups near-simultaneous notes, which would make
            // "which string did this note land on" ambiguous. These tests are
            // about routing, so it is closed.
            interpreter.setChordWindowMs (0.0);

            MidiInterpreter::Humanisation flat;
            flat.amount = 0.0;
            interpreter.setHumanisation (flat);
        }

        /** Sends one note-on and returns the string it landed on, or -1. */
        int noteOnString (int channel, int midiNote, int64_t& position)
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (channel, midiNote, 0.8f), 0);

            PlayEventQueue out;
            interpreter.processBlock (midi, 256, position, out);
            position += 256;

            return (out.getNumNoteOns() > 0) ? out.getNoteOn (0).stringIndex : -1;
        }

        void noteOff (int channel, int midiNote, int64_t& position)
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOff (channel, midiNote), 0);

            PlayEventQueue out;
            interpreter.processBlock (midi, 256, position, out);
            position += 256;
        }

        TuningEngine tuning;
        TechniqueEngine technique;
        RubricVoicer voicer;
        MidiInterpreter interpreter;
    };
}

//==============================================================================
/*  controllers.md 1 names nine profiles that must ship. Each has to be coherent:
    named, in a known mode, with a bend range and a latency budget that make
    sense. */
LUTHIER_TEST (Controllers, everyShipProfileIsWellFormed)
{
    ControllerProfileLibrary library;

    CHECK_MSG (library.getNumProfiles() >= 9,
               "only " + juce::String (library.getNumProfiles())
                 + " controller profiles; the spec names nine");

    juce::StringArray seenIds;

    for (int i = 0; i < library.getNumProfiles(); ++i)
    {
        const auto& profile = library.getProfile (i);

        CHECK_MSG (profile.isValid(), "profile " + juce::String (i) + " has no id");
        CHECK_MSG (profile.displayName.isNotEmpty(), profile.id + " has no display name");

        CHECK_MSG (! seenIds.contains (profile.id), "duplicate profile id " + profile.id);
        seenIds.add (profile.id);

        CHECK_MSG (profile.pitchBendSemis > 0.0 && profile.pitchBendSemis <= 96.0,
                   profile.id + ": pitch bend range out of range");

        CHECK_MSG (profile.latencyMsDefault >= 0.0 && profile.latencyMsDefault <= 200.0,
                   profile.id + ": latency budget out of range");

        CHECK_MSG (profile.pitchDeadZoneCents >= 0.0 && profile.pitchDeadZoneCents <= 100.0,
                   profile.id + ": pitch dead zone out of range");

        // A per-channel profile that routes no string would silently play
        // nothing, which is the failure worth catching here.
        if (profile.mode == ControllerMode::perChannel)
            CHECK_MSG (profile.hasPerStringRouting(),
                       profile.id + " is per-channel but routes no string");

        if (profile.mode == ControllerMode::mpe)
        {
            CHECK_MSG (profile.mpeFirstMemberChannel > profile.mpeMasterChannel,
                       profile.id + ": MPE member channels overlap the master");

            CHECK_MSG (profile.mpeLastMemberChannel >= profile.mpeFirstMemberChannel,
                       profile.id + ": MPE member range is inverted");
        }
    }

    // The specific profiles the spec calls out by name.
    for (const char* id : { "generic-midi", "generic-mpe", "roli-seaboard",
                            "linnstrument", "osmose", "roland-gk",
                            "fishman-tripleplay", "jamstik" })
        CHECK_MSG (library.indexOf (id) >= 0, juce::String ("missing profile ") + id);
}

//==============================================================================
/*  controllers.md 7: every cc_map value must resolve to a valid destination. A
    CC mapped to nothing is a control the user will turn and see no response
    from. */
LUTHIER_TEST (Controllers, everyCcMappingResolvesToARealTarget)
{
    ControllerProfileLibrary library;

    for (int i = 0; i < library.getNumProfiles(); ++i)
    {
        const auto& profile = library.getProfile (i);

        int mapped = 0;

        for (int cc = 0; cc < 128; ++cc)
        {
            const auto target = profile.ccMap[(size_t) cc];

            if (target == MidiTarget::None)
                continue;

            ++mapped;

            CHECK_MSG ((int) target > 0 && (int) target < (int) MidiTarget::NumTargets,
                       profile.id + ": CC " + juce::String (cc) + " maps outside the target list");

            CHECK_MSG (juce::String (getMidiTargetName (target)).isNotEmpty(),
                       profile.id + ": CC " + juce::String (cc) + " maps to an unnamed target");
        }

        juce::ignoreUnused (mapped);
    }

    // The spec's example: the GK profile puts CC 74 on the whammy.
    const int gk = library.indexOf ("roland-gk");
    CHECK (gk >= 0);

    if (gk >= 0)
        CHECK (library.getProfile (gk).ccMap[74] == MidiTarget::WhammyBar);
}

//==============================================================================
/*  A profile is a file, so it has to survive being written and read. */
LUTHIER_TEST (Controllers, profilesRoundTripThroughJson)
{
    ControllerProfileLibrary library;

    for (int i = 0; i < library.getNumProfiles(); ++i)
    {
        const auto& original = library.getProfile (i);

        const auto text = juce::JSON::toString (original.toVar(), false);
        const auto restored = ControllerProfile::fromVar (juce::JSON::parse (text));

        CHECK_MSG (restored.id == original.id, original.id + ": id lost");
        CHECK_MSG (restored.mode == original.mode, original.id + ": mode lost");

        CHECK_NEAR (restored.pitchBendSemis, original.pitchBendSemis, 0.001);
        CHECK_NEAR (restored.memberPitchBendSemis, original.memberPitchBendSemis, 0.001);
        CHECK_NEAR (restored.latencyMsDefault, original.latencyMsDefault, 0.001);
        CHECK_NEAR (restored.pitchDeadZoneCents, original.pitchDeadZoneCents, 0.001);
        CHECK_NEAR (restored.minimumNoteDurationMs, original.minimumNoteDurationMs, 0.001);

        for (int s = 0; s < kMaxStrings; ++s)
        {
            CHECK_MSG (restored.perString[(size_t) s].channel
                         == original.perString[(size_t) s].channel,
                       original.id + ": string " + juce::String (s) + " channel lost");

            CHECK_NEAR (restored.perString[(size_t) s].pitchBendSemis,
                        original.perString[(size_t) s].pitchBendSemis, 0.001);
        }

        for (int cc = 0; cc < 128; ++cc)
            CHECK_MSG (restored.ccMap[(size_t) cc] == original.ccMap[(size_t) cc],
                       original.id + ": CC " + juce::String (cc) + " mapping lost");

        CHECK_MSG (restored.pitchCurve.size() == original.pitchCurve.size(),
                   original.id + ": pitch curve lost");
    }
}

//==============================================================================
/*  The spec's own example profile, parsed verbatim. If the shipped parser cannot
    read the document's example, the document is wrong or the parser is. */
LUTHIER_TEST (Controllers, parsesTheSpecExampleProfile)
{
    const char* json = R"({
      "id": "roland-gk",
      "display_name": "Roland GK Series",
      "mode": "per_channel",
      "per_string": {
        "1": { "channel": 11, "pitch_bend_semis": 24 },
        "2": { "channel": 12, "pitch_bend_semis": 24 },
        "3": { "channel": 13, "pitch_bend_semis": 24 },
        "4": { "channel": 14, "pitch_bend_semis": 24 },
        "5": { "channel": 15, "pitch_bend_semis": 24 },
        "6": { "channel": 16, "pitch_bend_semis": 24 }
      },
      "cc_map": {
        "74": "whammy_amount",
        "1":  "vibrato_depth"
      },
      "latency_ms_default": 3.0,
      "notes": "GK pickup latency typical for GR-55; calibrate for GR-D"
    })";

    const auto profile = ControllerProfile::fromVar (juce::JSON::parse (json));

    CHECK (profile.isValid());
    CHECK (profile.id == "roland-gk");
    CHECK (profile.mode == ControllerMode::perChannel);
    CHECK_NEAR (profile.latencyMsDefault, 3.0, 0.001);

    // String 1 in the file is string index 0 in the code.
    CHECK (profile.perString[0].channel == 11);
    CHECK (profile.perString[5].channel == 16);
    CHECK_NEAR (profile.perString[0].pitchBendSemis, 24.0, 0.001);

    CHECK (profile.ccMap[74] == MidiTarget::WhammyBar);
    CHECK (profile.ccMap[1] == MidiTarget::VibratoDepth);

    CHECK (profile.stringForChannel (11) == 0);
    CHECK (profile.stringForChannel (16) == 5);
    CHECK (profile.stringForChannel (1) == -1);
}

//==============================================================================
/*  controllers.md 7: fuzz events across all sixteen channels and verify that the
    right string is triggered for each profile. */
LUTHIER_TEST (Controllers, perChannelRoutingSendsEachChannelToItsString)
{
    ControllerProfileLibrary library;

    for (int p = 0; p < library.getNumProfiles(); ++p)
    {
        const auto& profile = library.getProfile (p);

        if (profile.mode != ControllerMode::perChannel)
            continue;

        InterpreterFixture fixture;
        ControllerProfileLibrary::apply (profile, fixture.interpreter);

        int64_t position = 0;

        juce::Random random (0x9ade);

        for (int trial = 0; trial < 600; ++trial)
        {
            const int channel = 1 + random.nextInt (16);
            const int expected = profile.stringForChannel (channel);

            // A pitch every string can reach, so a failure means the routing is
            // wrong rather than the note being unplayable.
            const int note = 52 + random.nextInt (8);

            const int landed = fixture.noteOnString (channel, note, position);

            if (expected >= 0)
            {
                CHECK_MSG (landed == expected,
                           profile.id + ": channel " + juce::String (channel)
                             + " played string " + juce::String (landed)
                             + ", expected " + juce::String (expected));
            }
            else
            {
                /*  A channel the profile does not route still sounds, on a
                    string chosen by pitch.

                    That is deliberate rather than an oversight. controllers.md 6
                    has the player plugging in two controllers at once - an MPE
                    keyboard alongside a GK, say - and the GK profile routes only
                    channels 11 to 16. Dropping everything else would silence the
                    keyboard. What the profile guarantees is the mapping above:
                    a routed channel always reaches its own string. */
                CHECK_MSG (landed >= 0,
                           profile.id + ": channel " + juce::String (channel)
                             + " was dropped rather than falling back to a string");
            }

            fixture.noteOff (channel, note, position);
        }
    }
}

//==============================================================================
/*  Applying a profile has to put the interpreter into the state the profile
    describes, since that is the only thing applying one does. */
LUTHIER_TEST (Controllers, applyingAProfileConfiguresTheInterpreter)
{
    ControllerProfileLibrary library;
    InterpreterFixture fixture;

    // ---- a hex pickup ---------------------------------------------------------------
    const int gk = library.indexOf ("roland-gk");
    CHECK (gk >= 0);

    if (gk >= 0)
    {
        const auto& profile = library.getProfile (gk);

        ControllerProfileLibrary::apply (profile, fixture.interpreter);

        CHECK (! fixture.interpreter.isMpeEnabled());
        CHECK (fixture.interpreter.getPlayingMode() == PlayingMode::GuitarController);

        for (int s = 0; s < 6; ++s)
            CHECK_MSG (fixture.interpreter.getChannelForString (s) == 11 + s,
                       "string " + juce::String (s) + " went to channel "
                         + juce::String (fixture.interpreter.getChannelForString (s)));

        CHECK (fixture.interpreter.getCcTarget (74) == MidiTarget::WhammyBar);
        CHECK_NEAR (fixture.interpreter.getPitchDeadZoneCents(), profile.pitchDeadZoneCents, 0.001);
    }

    // ---- then an MPE controller, which must undo the hex routing ----------------------
    const int mpe = library.indexOf ("generic-mpe");
    CHECK (mpe >= 0);

    if (mpe >= 0)
    {
        ControllerProfileLibrary::apply (library.getProfile (mpe), fixture.interpreter);

        CHECK (fixture.interpreter.isMpeEnabled());
        CHECK_NEAR (fixture.interpreter.getPitchBendRange(), 48.0, 0.001);

        // The channel map must be back to the default, or the GK's channels
        // would still be in force under a controller that does not use them.
        for (int s = 0; s < 6; ++s)
            CHECK (fixture.interpreter.getChannelForString (s) == s + 1);
    }
}

//==============================================================================
/*  controllers.md 5: a sustained note on a hex pickup wobbles. Bends inside the
    dead zone must not reach the string engine, and bends outside it must. */
LUTHIER_TEST (Controllers, pitchDeadZoneRejectsTrackingNoiseButNotRealBends)
{
    InterpreterFixture fixture;

    fixture.interpreter.setPlayingMode (PlayingMode::GuitarController);
    fixture.interpreter.setPitchDeadZoneCents (5.0);
    fixture.interpreter.setStringBendRange (0, 24.0);

    int64_t position = 0;

    // A note on string 0, held.
    CHECK (fixture.noteOnString (1, 64, position) == 0);

    auto sendBend = [&fixture, &position] (double cents, double rangeSemis)
    {
        const double normalised = cents / (rangeSemis * 100.0);
        const int wheel = juce::jlimit (0, 16383, (int) std::round (8192.0 + normalised * 8192.0));

        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::pitchWheel (1, wheel), 0);

        PlayEventQueue out;
        fixture.interpreter.processBlock (midi, 256, position, out);
        position += 256;

        return out.getNumBends();
    };

    // Tracking noise: a couple of cents either way, repeatedly. None of it
    // should produce a bend event.
    int noiseBends = 0;

    for (int i = 0; i < 40; ++i)
        noiseBends += sendBend ((i % 2 == 0) ? 2.0 : -3.0, 24.0);

    CHECK_MSG (noiseBends == 0,
               juce::String (noiseBends) + " bend events came from tracking noise");

    // A real bend, well outside the dead zone, must get through.
    CHECK_MSG (sendBend (200.0, 24.0) > 0, "a two-semitone bend was swallowed by the dead zone");

    // And coming back down through the dead zone must not be truncated: the
    // string has to arrive back at pitch, not stop five cents short.
    CHECK_MSG (sendBend (0.0, 24.0) > 0, "the return to pitch was swallowed by the dead zone");
}

//==============================================================================
/*  controllers.md 5: a controller with lazy note-offs sends one immediately. The
    note must still sound for its minimum duration, and must still end. */
LUTHIER_TEST (Controllers, minimumNoteDurationSurvivesAnEarlyNoteOff)
{
    InterpreterFixture fixture;

    fixture.interpreter.setPlayingMode (PlayingMode::GuitarController);
    fixture.interpreter.setMinimumNoteDurationMs (40.0);

    int64_t position = 0;

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 64, 0.8f), 0);

    // The note-off arrives ten samples later: a fifth of a millisecond.
    midi.addEvent (juce::MidiMessage::noteOff (1, 64), 10);

    PlayEventQueue out;
    fixture.interpreter.processBlock (midi, 256, position, out);
    position += 256;

    CHECK_MSG (out.getNumNoteOns() == 1, "the note did not sound");
    CHECK_MSG (out.getNumNoteOffs() == 0, "the early note-off was obeyed");

    // Run on until the minimum duration has passed. Forty milliseconds at 48 kHz
    // is 1920 samples, so eight more blocks of 256 is comfortably past it.
    int offsEmitted = 0;

    for (int block = 0; block < 12; ++block)
    {
        juce::MidiBuffer empty;
        PlayEventQueue blockOut;

        fixture.interpreter.processBlock (empty, 256, position, blockOut);
        position += 256;

        offsEmitted += blockOut.getNumNoteOffs();
    }

    CHECK_MSG (offsEmitted == 1,
               juce::String (offsEmitted) + " note-offs after the minimum duration; expected one");
}

//==============================================================================
/*  controllers.md 7: the wizard must be stable across ten runs to within half a
    millisecond, and must say so when it is not. */
LUTHIER_TEST (Controllers, latencyWizardIsStableAndReportsItsScatter)
{
    // A steady player: the same latency every time, with a fraction of a
    // millisecond of hand noise.
    {
        LatencyWizard wizard;
        wizard.begin (10);

        juce::Random random (0x1a7e);

        bool finished = false;

        for (int run = 0; run < 10; ++run)
            finished = wizard.addMeasurement (7.0 + (random.nextDouble() - 0.5) * 0.4);

        CHECK (finished);
        CHECK (! wizard.isRunning());
        CHECK (wizard.getNumMeasurements() == 10);

        CHECK_MSG (std::abs (wizard.getMeasuredLatencyMs() - 7.0) <= 0.5,
                   "measured " + juce::String (wizard.getMeasuredLatencyMs(), 3)
                     + " ms for a 7 ms controller");

        CHECK_MSG (wizard.getSigmaMs() <= 0.5,
                   "sigma was " + juce::String (wizard.getSigmaMs(), 3) + " ms");

        CHECK (wizard.isReliable());
    }

    // A player who cannot keep time: the measurement has to be reported as
    // unreliable rather than written into a profile.
    {
        LatencyWizard wizard;
        wizard.begin (10);

        juce::Random random (0x5b2);

        for (int run = 0; run < 10; ++run)
            wizard.addMeasurement (7.0 + (random.nextDouble() - 0.5) * 30.0);

        CHECK_MSG (! wizard.isReliable(),
                   "a scatter of " + juce::String (wizard.getSigmaMs(), 2)
                     + " ms was reported as reliable");
    }

    // And a run that missed the click entirely is rejected outright.
    {
        LatencyWizard wizard;
        wizard.begin (4);

        CHECK (! wizard.addMeasurement (5000.0));
        CHECK (wizard.getNumMeasurements() == 0);
    }
}

//==============================================================================
/*  controllers.md 3: the effective latency is the measurement when there is one,
    and the budget when there is not. */
LUTHIER_TEST (Controllers, measuredLatencyOverridesTheBudget)
{
    ControllerProfile profile;
    profile.id = "test";
    profile.latencyMsDefault = 3.0;

    CHECK_NEAR (profile.getEffectiveLatencyMs(), 3.0, 0.001);

    profile.latencyMsMeasured = 4.75;
    CHECK_NEAR (profile.getEffectiveLatencyMs(), 4.75, 0.001);

    // Zero is a real measurement, not an absent one.
    profile.latencyMsMeasured = 0.0;
    CHECK_NEAR (profile.getEffectiveLatencyMs(), 0.0, 0.001);
}

//==============================================================================
/*  controllers.md 7: dual-source stress. No string may be lost, and a stale
    note-off from a displaced source must not cut the note that replaced it. */
LUTHIER_TEST (Controllers, multiControllerMergeNeverLosesAString)
{
    ControllerMerge merge;
    merge.reset();

    for (int s = 0; s < kMaxStrings; ++s)
        CHECK (merge.getOwner (s) == -1);

    juce::Random random (0x2c0de);

    int64_t timestamp = 0;

    for (int trial = 0; trial < 20000; ++trial)
    {
        const int stringIndex = random.nextInt (6);
        const int source = random.nextInt (2);

        if (random.nextBool())
        {
            merge.claim (stringIndex, source, timestamp++);

            CHECK_MSG (merge.getOwner (stringIndex) == source,
                       "the most recent claim did not win on string "
                         + juce::String (stringIndex));
        }
        else
        {
            merge.release (stringIndex, source);
        }
    }

    // ---- the stale note-off case ---------------------------------------------------
    merge.reset();

    merge.claim (0, 0, 1);      // source 0 takes string 0
    merge.claim (0, 1, 2);      // source 1 takes it away

    CHECK (merge.getOwner (0) == 1);

    // Source 0's note-off now arrives. It must be ignored: the note it refers to
    // is gone, and obeying it would cut short source 1's note.
    merge.release (0, 0);
    CHECK_MSG (merge.getOwner (0) == 1,
               "a stale note-off from a displaced source ended the current note");

    // Source 1's own note-off does end it.
    merge.release (0, 1);
    CHECK (merge.getOwner (0) == -1);

    // And the contention was counted, so the UI can warn about it.
    CHECK_MSG (merge.getContentionCount() > 0, "contention went unreported");

    merge.clearContentionCount();
    CHECK (merge.getContentionCount() == 0);
}

//==============================================================================
/*  A single source playing normally must never register contention: the warning
    has to mean something when it appears. */
LUTHIER_TEST (Controllers, oneControllerPlayingNormallyReportsNoContention)
{
    ControllerMerge merge;
    merge.reset();

    int64_t timestamp = 0;

    for (int trial = 0; trial < 1000; ++trial)
    {
        const int stringIndex = trial % 6;

        merge.claim (stringIndex, 0, timestamp++);
        merge.release (stringIndex, 0);
    }

    CHECK_MSG (merge.getContentionCount() == 0,
               "one controller playing by itself reported "
                 + juce::String (merge.getContentionCount()) + " contentions");
}

//==============================================================================
/*  controllers.md 1: the Osmose's pitch response is not linear, and the profile
    carries a curve to undo it. The curve must be monotonic and pinned at both
    ends, or a bend would fold back on itself. */
LUTHIER_TEST (Controllers, osmosePitchCurveIsMonotonicAndPinned)
{
    ControllerProfileLibrary library;

    const int index = library.indexOf ("osmose");
    CHECK (index >= 0);

    if (index < 0)
        return;

    const auto& profile = library.getProfile (index);

    CHECK_MSG (profile.pitchCurve.size() >= 2, "the Osmose profile carries no pitch curve");

    CHECK_NEAR (profile.applyPitchCurve (0.0), 0.0, 0.001);
    CHECK_NEAR (profile.applyPitchCurve (1.0), 1.0, 0.001);
    CHECK_NEAR (profile.applyPitchCurve (-1.0), -1.0, 0.001);

    double previous = -1.001;

    for (int i = 0; i <= 100; ++i)
    {
        const double x = (double) i / 100.0;
        const double y = profile.applyPitchCurve (x);

        CHECK_MSG (y >= previous - 0.001,
                   "the Osmose curve went backwards at " + juce::String (x, 3));

        CHECK (y >= -0.001 && y <= 1.001);

        previous = y;

        // And the curve must be odd: bending down by x must mirror bending up.
        CHECK_NEAR (profile.applyPitchCurve (-x), -y, 0.001);
    }

    // A profile with no curve passes its input straight through.
    ControllerProfile linear;
    CHECK_NEAR (linear.applyPitchCurve (0.37), 0.37, 0.001);
}

//==============================================================================
/*  controllers.md 0.4: a profile in the user's folder replaces the factory one
    of the same id. The library is what enforces that, so check the rule holds
    without needing the file system. */
LUTHIER_TEST (Controllers, savingAProfileReplacesTheOneOfTheSameId)
{
    ControllerProfileLibrary library;

    const int before = library.getNumProfiles();
    const int index = library.indexOf ("jamstik");

    CHECK (index >= 0);

    if (index < 0)
        return;

    auto edited = library.getProfile (index);
    edited.latencyMsMeasured = 6.25;
    edited.displayName = "Jamstik (mine)";

    // save() writes to disk as well; what is being checked here is that the
    // in-memory table replaced rather than appended.
    library.save (edited);

    CHECK_MSG (library.getNumProfiles() == before,
               "saving over a profile added one instead of replacing it");

    const int after = library.indexOf ("jamstik");
    CHECK (after >= 0);

    if (after >= 0)
    {
        CHECK (library.getProfile (after).displayName == "Jamstik (mine)");
        CHECK_NEAR (library.getProfile (after).getEffectiveLatencyMs(), 6.25, 0.001);
    }

    // A profile with no id is not a profile.
    ControllerProfile empty;
    CHECK (! library.save (empty));
}

//==============================================================================
/*  A chord group sounds one chord window after its first note - the latency
    the interpreter reports - on that exact sample, not on its block's first
    sample (which is where every chord used to land). */
LUTHIER_TEST (Controllers, chordGroupsSoundOneWindowAfterTheyWerePlayed)
{
    InterpreterFixture f;
    f.interpreter.setChordWindowMs (2.0);
    f.interpreter.setStrumSpeedMs (0.0);   // the strum spreads a chord from here; not under test
    const int window = f.interpreter.getLatencySamples();
    CHECK (window == 96);

    auto block = [&f] (std::initializer_list<std::pair<int, int>> notes, int64_t start)
    {
        juce::MidiBuffer midi;

        for (const auto& n : notes)
            midi.addEvent (juce::MidiMessage::noteOn (1, n.second, 0.8f), n.first);

        PlayEventQueue out;
        f.interpreter.processBlock (midi, 256, start, out);

        juce::Array<int> offsets;

        for (int i = 0; i < out.getNumNoteOns(); ++i)
            offsets.add (out.getNoteOn (i).sampleOffset);

        offsets.sort();
        return offsets;
    };

    // One note, inside the block.
    auto offsets = block ({ { 37, 52 } }, 1024);
    CHECK (offsets.size() == 1 && offsets[0] == 37 + window);

    // Two notes inside one window: one chord, strummed from its window's close.
    f.interpreter.reset();
    offsets = block ({ { 10, 48 }, { 40, 55 } }, 2048);
    CHECK (offsets.size() == 2 && offsets[0] == 10 + window);

    // Two notes further apart than the window: two groups, each on its own sample.
    f.interpreter.reset();
    offsets = block ({ { 10, 48 }, { 150, 60 } }, 4096);
    CHECK_MSG (offsets.size() == 2 && offsets[0] == 10 + window && offsets[1] == 150 + window,
               "notes 140 samples apart were grouped as one chord");

    // A window that runs past the block closes in the next one, on its sample.
    f.interpreter.reset();
    offsets = block ({ { 220, 52 } }, 8192);
    CHECK (offsets.isEmpty());
    offsets = block ({}, 8192 + 256);
    CHECK (offsets.size() == 1 && offsets[0] == 220 + window - 256);
}

//==============================================================================
// CT-7: ParameterBridge::applyToEngine used to rewrite setMpeEnabled/
// setPitchBendRange from the mpe_enabled/bend_range parameters on every block,
// undoing an MPE profile's flag and 48-semitone member bend the instant the next
// block ran. ControllersPage::applySelectedProfile now pushes the profile's
// values into those parameters, so the bridge re-applies the same thing.
LUTHIER_TEST (Controllers, anMpeProfileSurvivesTheParameterBridge)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 128);

    ControllersPage page (processor);
    page.setSize (400, 500);

    ControllerProfileLibrary library;
    library.refresh();
    const int index = library.indexOf ("roli-seaboard");
    CHECK (index >= 0);

    // Simulate choosing the Seaboard MPE profile in the combo box.
    bool foundBox = false;

    for (int i = 0; i < page.getNumChildComponents(); ++i)
    {
        if (auto* box = dynamic_cast<juce::ComboBox*> (page.getChildComponent (i)))
        {
            box->setSelectedId (index + 1, juce::sendNotificationSync);
            foundBox = true;
        }
    }

    CHECK (foundBox);
    CHECK (processor.getEngine().getMidiInterpreter().isMpeEnabled());
    CHECK_NEAR (processor.getEngine().getMidiInterpreter().getPitchBendRange(), 48.0, 1.0e-6);

    // The bug: a block used to run the bridge straight from the (still Generic)
    // mpe_enabled/bend_range parameters and stomp on what the profile just set.
    processor.getParameterBridge().applyToEngine();

    CHECK (processor.getEngine().getMidiInterpreter().isMpeEnabled());
    CHECK_NEAR (processor.getEngine().getMidiInterpreter().getPitchBendRange(), 48.0, 1.0e-6);
    CHECK (processor.getControllerProfileId() == "roli-seaboard");
}

//==============================================================================
// CT-2: the chosen profile used to live only in the ControllersPage combo box,
// so reopening the editor after a session round trip always fell back to
// Generic MIDI. It now travels in the processor's state.
LUTHIER_TEST (Controllers, theChosenProfileSurvivesTheSessionRoundTrip)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (48000.0, 128);

    {
        ControllersPage page (processor);
        page.setSize (400, 500);

        ControllerProfileLibrary library;
        library.refresh();
        const int index = library.indexOf ("roli-seaboard");
        CHECK (index >= 0);

        for (int i = 0; i < page.getNumChildComponents(); ++i)
            if (auto* box = dynamic_cast<juce::ComboBox*> (page.getChildComponent (i)))
                box->setSelectedId (index + 1, juce::sendNotificationSync);
    }

    CHECK (processor.getControllerProfileId() == "roli-seaboard");

    juce::MemoryBlock saved;
    processor.getStateInformation (saved);

    LuthierAudioProcessor restored;
    restored.prepareToPlay (48000.0, 128);
    restored.setStateInformation (saved.getData(), (int) saved.getSize());

    CHECK (restored.getControllerProfileId() == "roli-seaboard");

    // Reopening the page selects and re-applies it rather than resetting to
    // Generic MIDI (id 1).
    ControllersPage page (restored);
    page.setSize (400, 500);

    CHECK (restored.getEngine().getMidiInterpreter().isMpeEnabled());
}

//==============================================================================
// CT-16: channel pressure on a per-channel controller's channel drives that
// string's vibrato, not every string's.
LUTHIER_TEST (Controllers, pressureOnAChannelVibratesOnlyItsString)
{
    ControllerProfileLibrary library;
    const int gk = library.indexOf ("roland-gk");
    CHECK (gk >= 0);

    if (gk < 0)
        return;

    InterpreterFixture fixture;
    ControllerProfileLibrary::apply (library.getProfile (gk), fixture.interpreter);

    // roland-gk maps string index 2 (the third string) to channel 13.
    CHECK (fixture.interpreter.getChannelForString (2) == 13);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::channelPressureChange (13, 100), 0);

    PlayEventQueue out;
    int64_t position = 0;
    fixture.interpreter.processBlock (midi, 256, position, out);

    CHECK (out.getNumPressures() == 1);

    if (out.getNumPressures() == 1)
    {
        const auto& e = out.getPressure (0);
        CHECK (e.stringIndex == 2);
        CHECK_NEAR (e.value, 100.0 / 127.0, 1.0e-6);
    }
}

//==============================================================================
// CT-17: in MPE mode, the master channel (default 1) carries zone-wide
// messages only. A note-on there used to fall through to the same per-channel
// routing as a member channel and get voiced on whatever string channel 1
// mapped to.
LUTHIER_TEST (Controllers, mpeMasterChannelNotesAreIgnored)
{
    ControllerProfileLibrary library;
    const int mpe = library.indexOf ("roli-seaboard");
    CHECK (mpe >= 0);

    if (mpe < 0)
        return;

    InterpreterFixture fixture;
    const auto& profile = library.getProfile (mpe);
    ControllerProfileLibrary::apply (profile, fixture.interpreter);

    CHECK (fixture.interpreter.isMpeEnabled());
    CHECK (fixture.interpreter.getMpeMasterChannel() == profile.mpeMasterChannel);

    int64_t position = 0;
    const int master = profile.mpeMasterChannel;

    CHECK (fixture.noteOnString (master, 52, position) == -1);

    // A member channel still plays normally.
    CHECK (fixture.noteOnString (profile.mpeFirstMemberChannel, 52, position) >= 0);
}
