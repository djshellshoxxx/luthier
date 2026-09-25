/*  piano-roll-chord-display.md 8: PR-01 .. PR-07, CD-01 .. CD-05, OP-01 .. OP-02. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include "../Accessibility/Accessibility.h"
#include "../Model/Playing/RubricVoicer.h"
#include "../UI/PianoRollStrip.h"
#include "../UI/ChordNaming.h"
#include "../UI/ChordNameOverlay.h"
#include "../UI/VisualAids.h"
#include "../UI/OptionsPages.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/EasyPanel.h"
#include "../UI/UiPreferences.h"
#include "../Support/SoundingNotesPublisher.h"

#if defined (LUTHIER_ALLOCATION_COUNTER)
namespace luthier::tests
{
    long allocationsOnThisThread() noexcept;
}
#endif

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    struct Rig
    {
        LuthierAudioProcessor processor;
        juce::AudioBuffer<float> buffer;

        Rig()
        {
            processor.prepareToPlay (kSr, kBlock);
            buffer.setSize (juce::jmax (2, processor.getTotalNumOutputChannels()), kBlock);
            set (ParamIDs::macroHumanize, 0.0f);
        }

        void set (const char* id, float plain)
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
                p->setValueNotifyingHost (p->convertTo0to1 (plain));

            processor.getParameterBridge().applyAllNow();
        }

        void run (double seconds, juce::MidiBuffer first = {})
        {
            for (int b = 0; b < juce::jmax (1, (int) (seconds * kSr / kBlock)); ++b)
            {
                buffer.clear();
                juce::MidiBuffer midi;

                if (b == 0)
                    midi = first;

                processor.processBlock (buffer, midi);
            }
        }

        std::set<int> soundingNotes()
        {
            std::set<int> notes;
            auto& engine = processor.getEngine();

            for (int s = 0; s < engine.getNumStrings(); ++s)
                if (engine.getStringMidiNote (s) >= 0)
                    notes.insert (engine.getStringMidiNote (s));

            return notes;
        }
    };

    juce::MidiBuffer chord (std::initializer_list<int> notes)
    {
        juce::MidiBuffer midi;

        for (int n : notes)
            midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);

        return midi;
    }

    SoundingNotes::Frame frameOf (std::initializer_list<std::pair<int, std::int64_t>> stringsNoteStart, int numStrings = 6)
    {
        SoundingNotes::Frame f;
        f.numStrings = numStrings;
        f.sequence = 1;
        int s = 0;

        for (const auto& [note, start] : stringsNoteStart)
        {
            f.strings[(size_t) s].note = note;
            f.strings[(size_t) s].startSample = start;

            if (note >= 0)
                f.noteBits[(size_t) (note >> 6)] |= (std::uint64_t) 1 << (note & 63);

            ++s;
        }

        return f;
    }

    /** Remembers the Visual aids preferences and puts them back. */
    struct PrefsScope
    {
        bool names = VisualAids::showChordNames(), announce = VisualAids::announceChordNamesSetting();
        bool rollA = VisualAids::showPianoRoll (true), rollE = VisualAids::showPianoRoll (false);
        bool rolls = VisualAids::pianoRollShowsRoll();
        bool reduced = AccessibilitySettings::get().isReducedMotion();

        ~PrefsScope()
        {
            VisualAids::setShowChordNames (names);
            VisualAids::setAnnounceChordNames (announce);
            VisualAids::setShowPianoRoll (true, rollA);
            VisualAids::setShowPianoRoll (false, rollE);
            VisualAids::setPianoRollShowsRoll (rolls);
            AccessibilitySettings::get().setReducedMotion (reduced);
        }
    };
}

//==============================================================================
/*  PR-01: a chord lights exactly its sounding keys within two UI frames, in
    the colours of the strings sounding them - voiced notes included. */
LUTHIER_TEST (PianoRoll, aChordLightsItsSoundingKeysInTheStringsColours)
{
    Rig rig;
    PianoRollStrip strip (rig.processor, true);
    strip.setSize (900, 72);

    rig.run (0.3, chord ({ 48, 52, 55, 60 }));   // voiced across the strings by the interpreter
    const auto sounding = rig.soundingNotes();
    CHECK (sounding.size() >= 3);

    strip.tick (1000.0);
    strip.tick (1033.0);

    SoundingNotes::Frame frame;
    CHECK (rig.processor.getSoundingNotes().read (frame));

    for (int note = 0; note < 128; ++note)
        CHECK_MSG (strip.getModel().isLit (note, 1033.0) == frame.isSounding (note),
                   "key " + juce::String (note) + " lit " + juce::String ((int) strip.getModel().isLit (note, 1033.0)));

    // Each lit key is in its string's colour, and every sounding string's note is lit.
    for (int s = 0; s < frame.numStrings; ++s)
    {
        const int note = frame.strings[(size_t) s].note;

        if (note < 0)
            continue;

        CHECK (strip.getModel().isLit (note, 1033.0));
        const auto& key = strip.getModel().getKey (note);
        CHECK (key.colour == StringColours::forString (key.string, frame.numStrings));
        CHECK_NEAR (strip.getModel().keyAlpha (note, 1033.0), PianoRollModel::kLitAlpha, 1.0e-6);
    }

    // The keys lit are the notes the engine's strings are playing (the voicer's placement).
    CHECK ((int) sounding.size() == frame.countSounding() || frame.countSounding() >= 3);
}

/*  PR-02: releasing clears the keys; after 200 ms none are lit. */
LUTHIER_TEST (PianoRoll, releasingClearsTheKeysWithin200ms)
{
    Rig rig;
    PianoRollStrip strip (rig.processor, true);
    strip.setSize (900, 72);

    rig.run (0.3, chord ({ 48, 52, 55 }));
    strip.tick (1000.0);
    CHECK (strip.getModel().countLit (1000.0) >= 3);

    juce::MidiBuffer off;
    for (int n : { 48, 52, 55 })
        off.addEvent (juce::MidiMessage::noteOff (1, n), 0);

    rig.run (0.05, off);
    strip.tick (1034.0);
    CHECK (strip.getModel().countLit (1034.0 + 200.0) == 0);

    // A stale snapshot (nothing published for 250 ms) lights nothing either.
    rig.run (0.2, chord ({ 64 }));
    strip.tick (2000.0);
    CHECK (strip.getModel().countLit (2000.0) >= 1);
    strip.tick (2400.0);   // no blocks since: stale
    CHECK (strip.getModel().countLit (2400.0 + 200.0) == 0);
}

/*  PR-03: a click plays the note on the processor's keyboard path, and the
    engine sounds it. */
LUTHIER_TEST (PianoRoll, clickingAKeyPlaysTheGuitar)
{
    Rig rig;
    PianoRollStrip strip (rig.processor, true);
    strip.setSize (900, 72);

    const auto key = strip.getKeyBounds (64);
    CHECK (! key.isEmpty());

    float velocity = 0.0f;
    CHECK (strip.noteAt (key.getCentre().withY (key.getBottom() - 0.1f), &velocity) == 64);
    CHECK_NEAR (velocity * 127.0f, 110.0f, 3.0f);   // the bottom of the key
    CHECK (strip.noteAt (key.getCentre().withY (key.getY() + 0.5f), &velocity) == 64);
    CHECK_NEAR (velocity * 127.0f, 40.0f, 3.0f);    // the top

    strip.pressKey (64, 0.8f);
    rig.run (0.2);
    CHECK (rig.soundingNotes().count (64) == 1);

    strip.releaseKey (64);
    rig.run (0.1);
    CHECK (rig.soundingNotes().count (64) == 0);
}

/*  PR-04: Latch, C E G, Play: a strummed C major - three pitch classes, one
    note-on per voiced string, the strings starting at different times. */
LUTHIER_TEST (PianoRoll, latchedKeysPlayAsOneStrummedChord)
{
    Rig rig;
    PianoRollStrip strip (rig.processor, true);
    strip.setSize (900, 72);

    strip.setLatch (true);
    strip.pressKey (60, 0.8f);
    strip.pressKey (64, 0.8f);
    strip.pressKey (67, 0.8f);
    CHECK (strip.getLatched().size() == 3);

    rig.run (0.2);
    CHECK_MSG (rig.soundingNotes().empty(), "latched keys sounded before Play");

    strip.playLatched();

    int noteOns = 0;
    std::set<int> strings;

    for (int b = 0; b < (int) (0.5 * kSr / kBlock); ++b)
    {
        rig.buffer.clear();
        juce::MidiBuffer midi;
        rig.processor.processBlock (rig.buffer, midi);

        const auto& activity = rig.processor.getEngine().getStringActivity();

        for (int i = 0; i < activity.size(); ++i)
            if (activity[i].isNoteOn)
            {
                ++noteOns;
                strings.insert (activity[i].stringIndex);
            }
    }

    std::set<int> classes;
    SoundingNotes::Frame frame;
    CHECK (rig.processor.getSoundingNotes().read (frame));
    std::set<std::int64_t> starts;

    for (int s = 0; s < frame.numStrings; ++s)
        if (frame.strings[(size_t) s].note >= 0)
        {
            classes.insert (frame.strings[(size_t) s].note % 12);
            starts.insert (frame.strings[(size_t) s].startSample);
        }

    CHECK_MSG (classes == std::set<int> ({ 0, 4, 7 }), "pitch classes " + juce::String ((int) classes.size()));
    CHECK_MSG (noteOns == (int) strings.size(), juce::String (noteOns) + " note-ons on " + juce::String ((int) strings.size()) + " strings");
    CHECK_MSG (starts.size() > 1, "the chord was not strummed");

    strip.clearLatched();
    rig.run (0.2);
    CHECK (strip.getLatched().isEmpty());
}

/*  PR-05: Show fingering draws RubricVoicer's voicing of the latched keys as
    ghost dots on the fretboard, before anything sounds. */
LUTHIER_TEST (PianoRoll, showFingeringDrawsTheVoicersShapeBeforeItSounds)
{
    Rig rig;
    FretboardComponent board (rig.processor);
    PianoRollStrip strip (rig.processor, true);
    strip.setSize (900, 72);
    strip.setFretboard (&board);

    strip.setLatch (true);
    strip.setShowFingering (true);

    for (int n : { 60, 64, 67 })
        strip.pressKey (n, 0.8f);

    auto& engine = rig.processor.getEngine();
    RubricVoicer voicer;
    voicer.prepare (&engine.getTuningEngine(), engine.getNumStrings());
    voicer.setMaxFret (engine.getGuitarSpec().maxFrets);
    const int notes[] = { 60, 64, 67 };
    const auto expected = voicer.voice (notes, nullptr, 3);

    std::vector<std::pair<int, double>> want;
    for (int i = 0; i < expected.numNotes; ++i)
        if (expected.notes[(size_t) i].valid)
            want.emplace_back (expected.notes[(size_t) i].stringIndex, expected.notes[(size_t) i].fretPosition);

    std::vector<std::pair<int, double>> got;
    for (const auto& d : board.getGhostDots())
        got.emplace_back (d.string, d.fret);

    CHECK (! want.empty());
    CHECK_MSG (got == want, juce::String ((int) got.size()) + " ghost dots against " + juce::String ((int) want.size()));
    CHECK (rig.soundingNotes().empty());

    // A note under the lowest string is marked, not placed.
    strip.pressKey (30, 0.8f);
    CHECK (strip.getUnreachable().contains (30));

    // Off: the dots go.
    strip.setShowFingering (false);
    CHECK (board.getGhostDots().empty());
}

/*  PR-06: the range follows tuning and capo. */
LUTHIER_TEST (PianoRoll, theRangeFollowsTuningAndCapo)
{
    Rig rig;
    PianoRollStrip strip (rig.processor, true);
    strip.setSize (900, 72);
    strip.tick (0.0);

    CHECK (strip.getRange().lowestPlayable == 40);     // E2
    CHECK (strip.getRange().drawLow % 12 == 0);         // whole octaves
    CHECK (strip.getRange().drawHigh % 12 == 11);

    rig.set (ParamIDs::tuningPreset, 1.0f);             // Drop D
    rig.run (0.05);
    strip.tick (40.0);
    CHECK_MSG (strip.getRange().lowestPlayable == 38, "lowest " + juce::String (strip.getRange().lowestPlayable));   // D2

    rig.set (ParamIDs::capoFret, 2.0f);
    rig.run (0.05);
    strip.tick (80.0);
    CHECK_MSG (strip.getRange().lowestPlayable == 40, "with capo 2, lowest " + juce::String (strip.getRange().lowestPlayable));

    // Keys outside the playable range still play.
    CHECK (! strip.getRange().isPlayable (strip.getRange().drawLow));
    CHECK (! strip.getKeyBounds (strip.getRange().drawLow).isEmpty());
}

/*  PR-07: publishing the snapshot does not allocate on the audio thread. */
LUTHIER_TEST (PianoRoll, publishingTheSnapshotDoesNotAllocate)
{
    Rig rig;
    rig.run (0.2, chord ({ 40, 47, 52 }));

    SoundingNotesPublisher publisher;
    SoundingNotes notes;
    publisher.publish (rig.processor.getEngine(), 0, notes);   // warm-up

   #if defined (LUTHIER_ALLOCATION_COUNTER)
    const long before = allocationsOnThisThread();

    for (int i = 0; i < 1000; ++i)
        publisher.publish (rig.processor.getEngine(), i * kBlock, notes);

    CHECK_MSG (allocationsOnThisThread() == before,
               juce::String (allocationsOnThisThread() - before) + " allocations in 1000 publishes");
   #endif

    SoundingNotes::Frame frame;
    CHECK (notes.read (frame));
    CHECK (frame.countSounding() >= 3);
    CHECK (frame.sequence == notes.getSequence());

    // The packing round-trips.
    const auto s = SoundingNotes::unpack (SoundingNotes::pack (67, -23, 123456789));
    CHECK (s.note == 67 && s.bendCents == -23 && s.startSample == 123456789);
    CHECK (SoundingNotes::unpack (SoundingNotes::pack (-1, 0, 0)).note == -1);
}

//==============================================================================
/*  CD-01: the names. */
LUTHIER_TEST (ChordName, singleNotesPowerChordsAndChordsAreNamed)
{
    const int gSharp[] = { 56 };
    CHECK (ChordNaming::nameFor (gSharp, 1) == "G#");

    const int gMajor[] = { 43, 47, 50, 55, 59, 67 };
    CHECK_MSG (ChordNaming::nameFor (gMajor, 6) == "G", ChordNaming::nameFor (gMajor, 6));

    const int aMinor7[] = { 45, 52, 55, 60, 64 };
    CHECK_MSG (ChordNaming::nameFor (aMinor7, 5) == "Am7", ChordNaming::nameFor (aMinor7, 5));

    const int e5[] = { 40, 47, 52 };
    CHECK_MSG (ChordNaming::nameFor (e5, 3) == "E5", ChordNaming::nameFor (e5, 3));

    const int ce[] = { 48, 52 };
    CHECK (ChordNaming::nameFor (ce, 2) == "C E");

    // Bb and Eb are always flats without a tune; a flat key spells flats.
    const int bb[] = { 58 };
    CHECK (ChordNaming::nameFor (bb, 1) == "Bb");
    const int dSharp[] = { 51 };
    CHECK (ChordNaming::nameFor (dSharp, 1) == "Eb");
    const int fSharp[] = { 54 };
    CHECK (ChordNaming::nameFor (fSharp, 1) == "F#");
    CHECK (ChordNaming::nameFor (fSharp, 1, ChordNaming::Spelling::flats) == "Gb");
}

/*  CD-02: peak 0.35 within 80 ms, held while the notes sound up to 1.2 s,
    gone by 2.1 s. */
LUTHIER_TEST (ChordName, fadesInHoldsAndFadesOut)
{
    PrefsScope scope;
    VisualAids::setShowChordNames (true);
    AccessibilitySettings::get().setReducedMotion (false);

    Rig rig;
    ChordNameOverlay overlay (rig.processor);

    const auto g = frameOf ({ { 43, 1000 }, { 47, 1000 }, { 50, 1000 }, { 55, 1000 }, { 59, 1000 }, { 67, 1000 } });
    overlay.observe (0.0, g, kSr);

    for (double t = 33.0; t <= 2100.0; t += 33.0)
        overlay.observe (t, g, kSr);   // still sounding throughout

    CHECK (overlay.getText() == "G");
    CHECK_NEAR (overlay.getOpacity (80.0), 0.35f, 0.02f);
    CHECK_NEAR (overlay.getOpacity (1000.0), 0.35f, 0.02f);
    CHECK (overlay.getOpacity (1600.0) < 0.35f);   // past the 1.2 s hold
    CHECK (overlay.getOpacity (2100.0) == 0.0f);

    // Released early: the hold ends at the release.
    ChordNameOverlay early (rig.processor);
    early.observe (0.0, g, kSr);
    early.observe (300.0, SoundingNotes::Frame {}, kSr);
    CHECK_NEAR (early.getOpacity (299.0), 0.35f, 0.02f);
    CHECK (early.getOpacity (400.0) < 0.35f);
    CHECK (early.getOpacity (1101.0) == 0.0f);
}

/*  CD-03: a strum spread over 25 ms is one name, not a name per string. */
LUTHIER_TEST (ChordName, aStrumIsOneName)
{
    PrefsScope scope;
    VisualAids::setShowChordNames (true);
    AccessibilitySettings::get().setReducedMotion (false);

    Rig rig;
    ChordNameOverlay overlay (rig.processor);
    const std::int64_t t0 = 48000;
    const auto step = (std::int64_t) (0.005 * kSr);   // 5 ms a string, 25 ms in all

    // The UI sees it in two frames: the first two strings, then the rest.
    overlay.observe (0.0, frameOf ({ { 43, t0 }, { 47, t0 + step } }), kSr);
    const float early = overlay.getOpacity (20.0);
    overlay.observe (33.0, frameOf ({ { 43, t0 }, { 47, t0 + step }, { 50, t0 + 2 * step }, { 55, t0 + 3 * step },
                                      { 59, t0 + 4 * step }, { 67, t0 + 5 * step } }), kSr);

    CHECK (overlay.getText() == "G");
    CHECK (overlay.getFader().getOutgoingText (40.0).isEmpty());   // no crossfade: one name
    CHECK_NEAR (overlay.getOpacity (20.0), early, 1.0e-6f);          // the fade was not restarted
    CHECK_NEAR (overlay.getOpacity (40.0), 0.35f * 40.0f / 60.0f, 0.01f);
}

/*  CD-04: off, nothing is drawn and no detection runs. */
LUTHIER_TEST (ChordName, offMeansNoDrawingAndNoDetection)
{
    PrefsScope scope;
    VisualAids::setShowChordNames (false);

    Rig rig;
    GuitarBodyComponent body (rig.processor);
    body.setSize (600, 300);

    rig.run (0.2, chord ({ 43, 47, 50, 55 }));

    for (int i = 0; i < 5; ++i)
        body.updateLiveOverlay (1000.0 + 33.0 * i);

    CHECK (body.getChordName().getDetectionCount() == 0);
    CHECK (body.getChordName().getOpacity (1100.0) == 0.0f);

    // On: the same frames are named.
    VisualAids::setShowChordNames (true);

    for (int i = 0; i < 3; ++i)
        body.updateLiveOverlay (2000.0 + 33.0 * i);

    CHECK (body.getChordName().getDetectionCount() > 0);
    CHECK (body.getChordName().getText().isNotEmpty());
    CHECK (! body.getChordNameArea().isEmpty());
    CHECK (body.getLocalBounds().toFloat().contains (body.getChordNameArea()));
}

/*  CD-05: reduced motion has no intermediate opacities. */
LUTHIER_TEST (ChordName, reducedMotionHasNoFades)
{
    PrefsScope scope;
    VisualAids::setShowChordNames (true);
    AccessibilitySettings::get().setReducedMotion (true);

    Rig rig;
    ChordNameOverlay overlay (rig.processor);
    const auto e = frameOf ({ { 40, 100 }, { 47, 100 }, { 52, 100 } });

    for (double t = 0.0; t <= 2500.0; t += 33.0)
        overlay.observe (t, e, kSr);

    for (double t = 0.0; t <= 2500.0; t += 5.0)
    {
        const float a = overlay.getOpacity (t);
        CHECK_MSG (a == 0.0f || a == (float) ChordNameFader::kPeak, "opacity " + juce::String (a) + " at " + juce::String (t));
    }

    CHECK (overlay.getOpacity (1.0) == (float) ChordNameFader::kPeak);
    CHECK (overlay.getOpacity (2400.0) == 0.0f);
}

/*  Section 4: the size and the announcement's rate limit. */
LUTHIER_TEST (ChordName, sizedFromTheIllustrationAndAnnouncedPolitely)
{
    CHECK_NEAR (ChordNameOverlay::fontHeightFor (100.0f), 28.0f, 1.0e-4f);
    CHECK_NEAR (ChordNameOverlay::fontHeightFor (500.0f), 60.0f, 1.0e-4f);
    CHECK_NEAR (ChordNameOverlay::fontHeightFor (2000.0f), 96.0f, 1.0e-4f);

    // The lower bout: the tail end of the body, away from the nut.
    const auto bout = ChordNameOverlay::lowerBout ({ 0.0f, 0.0f, 400.0f, 200.0f }, { 600.0f, 100.0f });
    CHECK (bout.getRight() <= 200.0f && bout.getX() == 0.0f);

    PrefsScope scope;
    VisualAids::setShowChordNames (true);
    VisualAids::setAnnounceChordNames (true);
    AccessibilitySettings::get().setReducedMotion (false);

    Rig rig;
    ChordNameOverlay overlay (rig.processor);
    overlay.observe (0.0, frameOf ({ { 40, 100 }, { 47, 100 } }), kSr);
    CHECK (overlay.getLastAnnouncement() == "E5");

    overlay.observe (500.0, frameOf ({ { 45, 30000 }, { 52, 30000 } }), kSr);   // A5, 0.5 s later: held back
    CHECK (overlay.getLastAnnouncement() == "E5");

    overlay.observe (1600.0, frameOf ({ { 43, 90000 }, { 50, 90000 } }), kSr);  // G5 after 1.5 s
    CHECK (overlay.getLastAnnouncement() == "G5");

    // Announcing needs the names on.
    VisualAids::setShowChordNames (false);
    CHECK (! VisualAids::announceChordNames());
}

//==============================================================================
/*  OP-01: the four options persist across editors and processor instances,
    and are not preset data. UiState's fields go in the plugin state. */
LUTHIER_TEST (VisualAids, theOptionsPersistAndStayOutOfPresets)
{
    PrefsScope scope;

    {
        LuthierAudioProcessor processor;
        AppearancePage page (processor);
        page.refresh();

        page.getChordNamesToggle().setToggleState (false, juce::sendNotificationSync);
        page.getPianoRollToggle (true).setToggleState (false, juce::sendNotificationSync);
        page.getPianoRollToggle (false).setToggleState (true, juce::sendNotificationSync);
        page.getPianoRollShowsBox().setSelectedId (1, juce::sendNotificationSync);
    }

    {
        LuthierAudioProcessor processor;   // a new instance reads the same user preferences
        AppearancePage page (processor);
        page.refresh();

        CHECK (! page.getChordNamesToggle().getToggleState());
        CHECK (! page.getAnnounceChordsToggle().isEnabled());   // only with the names on
        CHECK (! page.getPianoRollToggle (true).getToggleState());
        CHECK (page.getPianoRollToggle (false).getToggleState());
        CHECK (page.getPianoRollShowsBox().getSelectedId() == 1);

        page.getChordNamesToggle().setToggleState (true, juce::sendNotificationSync);
        CHECK (page.getAnnounceChordsToggle().isEnabled());

        // Not in presets.
        const auto preset = juce::JSON::toString (processor.getPresetManager().toVar ("x", "y"));
        CHECK (! preset.contains ("visualAids") && ! preset.contains ("pianoRoll") && ! preset.contains ("pianoLatch"));

        // The strip's own state is session state.
        auto& ui = processor.getUiState();
        ui.pianoRollExpanded = false;
        ui.pianoRollHeight = 110;
        ui.pianoLatch = true;
        ui.pianoShowFingering = true;
        ui.pianoLatchedNotes = { 60, 64, 67 };

        juce::MemoryBlock state;
        processor.getStateInformation (state);

        LuthierAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        const auto& r = restored.getUiState();
        CHECK (! r.pianoRollExpanded && r.pianoRollHeight == 110 && r.pianoLatch && r.pianoShowFingering);
        CHECK (r.pianoLatchedNotes == juce::Array<int> ({ 60, 64, 67 }));
    }
}

/*  OP-02: every control is reachable in both modes, focusable and named. */
LUTHIER_TEST (VisualAids, everyControlIsReachableFocusableAndNamed)
{
    PrefsScope scope;
    VisualAids::setShowPianoRoll (true, true);
    VisualAids::setShowPianoRoll (false, true);

    LuthierAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    CHECK (editor != nullptr);

    if (editor == nullptr)
        return;

    editor->setSize (1280, 800);

    auto check = [&ctx] (PianoRollStrip& strip, const juce::String& mode)
    {
        CHECK_MSG (strip.isVisible() && strip.getHeight() > 0, mode + ": no piano roll");
        CHECK (strip.getWantsKeyboardFocus());
        CHECK (strip.getTitle() == "Piano roll");

        for (auto* b : { &strip.getModeButton(), &strip.getLatchButton(), &strip.getFingeringButton() })
        {
            CHECK_MSG (b->isVisible() && b->getWantsKeyboardFocus() && b->getTitle().isNotEmpty(),
                       mode + ": " + b->getButtonText() + " is not reachable");
            CHECK_MSG (strip.getLocalBounds().contains (b->getBounds()), mode + ": " + b->getButtonText() + " outside the strip");
        }

        strip.setLatch (true);
        CHECK (strip.getPlayButton().isVisible() && strip.getClearButton().isVisible());
        CHECK (strip.getPlayButton().getTitle().isNotEmpty() && strip.getClearButton().getTitle().isNotEmpty());
        strip.setLatch (false);
    };

    PianoRollStrip* easy = nullptr;
    PianoRollStrip* advanced = nullptr;

    std::function<void (juce::Component&)> find = [&] (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (auto* s = dynamic_cast<PianoRollStrip*> (child))
                (dynamic_cast<EasyPanel*> (&c) != nullptr ? easy : advanced) = s;

            find (*child);
        }
    };

    find (*editor);
    CHECK (easy != nullptr && advanced != nullptr);

    if (easy != nullptr)
        check (*easy, "Easy");

    editor->keyPressed (AccessibilitySettings::get().findShortcut ("toggleAdvanced")->key);

    if (advanced != nullptr)
    {
        check (*advanced, "Advanced");
        CHECK (advanced->getCollapseButton().isVisible() && advanced->getCollapseButton().getTitle().isNotEmpty());

        // Collapsing keeps the header; the columns get the room back.
        const int open = advanced->getHeight();
        advanced->setCollapsed (true);
        CHECK (advanced->getHeight() < open);
        advanced->setCollapsed (false);
    }

    // The Options controls have names too.
    AppearancePage page (processor);
    CHECK (page.getChordNamesToggle().getButtonText().isNotEmpty());
    CHECK (page.getPianoRollShowsBox().getTitle().isNotEmpty());
}
