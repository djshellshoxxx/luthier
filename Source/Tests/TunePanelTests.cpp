/*  The TUNE workspace tab (tune-builder.md 2, 3; gui-integration.md 4.4) and
    the TuneSession behind it.

    Controls are driven through their callbacks, as MidiOutPanelTests does:
    triggerClick is asynchronous and never runs in the console test runner. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/TunePanel.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    bool press (juce::Button& button)
    {
        if (! button.isEnabled() || button.onClick == nullptr)
            return false;

        if (button.getClickingTogglesState())
            button.setToggleState (! button.getToggleState(), juce::dontSendNotification);

        button.onClick();
        return true;
    }

    void choose (juce::ComboBox& box, int id)
    {
        box.setSelectedId (id, juce::dontSendNotification);

        if (box.onChange != nullptr)
            box.onChange();
    }

    void slide (juce::Slider& slider, double value)
    {
        slider.setValue (value, juce::dontSendNotification);

        if (slider.onValueChange != nullptr)
            slider.onValueChange();
    }

    void type (TunePanel& panel, const juce::String& text)
    {
        panel.getProgressionEditor().setText (text, false);
        panel.progressionTextChanged();
    }

    /** A tune with one four-bar verse, so there is something to edit. */
    void startWithAVerse (TuneSession& session)
    {
        Tune t;
        t.meta.title = "Sketch";
        TuneSection verse;
        verse.name = "Verse";
        verse.lengthBars = 4;
        verse.rhythmPatternId = "Folk Down Up";
        verse.genreKitId = "Folk Fingerstyle";
        t.addSection (verse);
        session.newTune (t);
    }

    /** The processor, and a player and session of the test's own that the
        panel wires together, as the processor would. Destroyed panel first. */
    struct Fixture
    {
        LuthierAudioProcessor processor;
        TunePlayer player;
        TuneSession session;
        std::unique_ptr<TunePanel> panel;

        Fixture()
        {
            processor.prepareToPlay (kSr, kBlock);
            player.prepare (kSr, kBlock);
            panel = std::make_unique<TunePanel> (processor, player, session);
            panel->setSize (420, panel->getPreferredHeight());
            startWithAVerse (session);
        }

        const TuneSection& verse() { return session.getTune().arrangement.sections[0]; }

        void renderBlocks (int blocks)
        {
            juce::MidiBuffer toEngine, toOut;
            toEngine.ensureSize (TunePlayer::kRecommendedMidiBytes);
            toOut.ensureSize (TunePlayer::kRecommendedMidiBytes);

            for (int b = 0; b < blocks; ++b)
            {
                toEngine.clear();
                toOut.clear();
                player.renderBlock (kBlock, {}, toEngine, toOut);
            }
        }
    };
}

//==============================================================================
LUTHIER_TEST (TunePanel, theProgressionFieldWritesTheSectionAndShowsErrorsWhereTheyAre)
{
    Fixture f;

    type (*f.panel, "Am F C G");
    CHECK (f.panel->getProgressionError().isEmpty());
    CHECK (f.verse().chords.size() == 4);
    CHECK (f.verse().chords[0].root == 9 && f.verse().chords[0].quality == "m");

    // 2.2 / 15: a malformed token is a named error at its position, and the
    // section keeps the chords it had.
    type (*f.panel, "Am Cfoo");
    CHECK (f.panel->getProgressionError().startsWith ("UnknownQuality"));
    CHECK (f.panel->getProgressionErrorPosition() == 3);
    CHECK (f.verse().chords.size() == 4);

    // Section headers make or fill sections.
    type (*f.panel, "[Verse] Am F [Chorus] C G");
    CHECK (f.session.getTune().getNumSections() == 2);
    CHECK (f.session.getTune().arrangement.sections[1].name == "Chorus");

    // A typed edit is an undo entry, and undo puts the chords back.
    CHECK (f.session.canUndo());
    const auto chordsBefore = f.verse().chords;
    CHECK (f.session.undo());
    CHECK (f.verse().chords != chordsBefore);

    // 3.2: pills by diatonic function; a non-diatonic chord is neutral.
    const auto& tune = f.session.getTune();
    CHECK (TuneChordPills::colourForDegree (getDiatonicDegree (ChordCell::make (0, ""), tune.meta.keyTonic, tune.meta.mode))
             == Palette::accent);
    CHECK (TuneChordPills::colourForDegree (getDiatonicDegree (ChordCell::make (7, ""), tune.meta.keyTonic, tune.meta.mode))
             == Palette::warning);
    CHECK (TuneChordPills::colourForDegree (getDiatonicDegree (ChordCell::make (10, ""), tune.meta.keyTonic, tune.meta.mode))
             == Palette::edge);
}

LUTHIER_TEST (TunePanel, theSectionStripsMenuRenamesDuplicatesRepeatsTagsLinksAndDeletes)
{
    Fixture f;
    auto& strip = f.panel->getSectionStrip();

    strip.performMenuItem (0, TuneSectionStrip::duplicateItem);
    CHECK (f.session.getTune().getNumSections() == 2);
    CHECK (f.session.getTune().arrangement.sections[1].name == "Verse 2");
    CHECK (f.session.getSelectedSection() == 1);

    CHECK (strip.renameSection (1, "Chorus"));
    CHECK (f.session.getTune().arrangement.sections[1].name == "Chorus");

    strip.performMenuItem (0, TuneSectionStrip::repeatBase + 2);
    CHECK (f.session.getTune().arrangement.setlist.size() == 2);
    CHECK (f.session.getTune().arrangement.setlist[0].repeats == 2);

    // 3.3's custom repeat count, as its prompt applies it.
    CHECK (strip.setRepeatCount (0, 6));
    CHECK (f.session.getTune().arrangement.setlist[0].repeats == 6);

    strip.performMenuItem (1, TuneSectionStrip::roleBase + (int) SectionRole::chorus);
    CHECK (f.session.getTune().arrangement.sections[1].role == SectionRole::chorus);

    // 3.5: link the chorus's rhythm to the verse, and back.
    strip.performMenuItem (1, TuneSectionStrip::linkBase + 0);
    CHECK (f.session.getTune().arrangement.sections[1].rhythmLinkedTo == "Verse");
    strip.performMenuItem (1, TuneSectionStrip::unlinkItem);
    CHECK (f.session.getTune().arrangement.sections[1].rhythmLinkedTo.isEmpty());

    // 8: the per-section state boundary.
    strip.performMenuItem (1, TuneSectionStrip::stateBoundaryItem);
    CHECK (f.session.getTune().arrangement.sections[1].stateBoundary);

    strip.performMenuItem (-1, TuneSectionStrip::addItem);
    CHECK (f.session.getTune().getNumSections() == 3);
    CHECK (f.session.getTune().arrangement.sections[2].genreKitId.isNotEmpty());
    CHECK (f.session.getTune().arrangement.setlist.size() == 3);   // and it plays

    strip.performMenuItem (2, TuneSectionStrip::deleteItem);
    CHECK (f.session.getTune().getNumSections() == 2);

    // action-and-undo 3.9: section edits never group, each is its own entry.
    const int steps = f.session.getNumUndoSteps();
    strip.performMenuItem (0, TuneSectionStrip::roleBase + (int) SectionRole::verse);
    strip.performMenuItem (0, TuneSectionStrip::roleBase + (int) SectionRole::intro);
    CHECK (f.session.getNumUndoSteps() == steps + 2);

    // The menu offers what performMenuItem does.
    CHECK (strip.buildMenu (0).getNumItems() > 0);
}

LUTHIER_TEST (TunePanel, deletingTheOnlySectionPostsARefusalWithoutAnEdit)
{
    Fixture f;
    auto& strip = f.panel->getSectionStrip();
    int refusals = 0;
    strip.onLastSectionDeleteRefused = [&refusals] { ++refusals; };

    const auto before = f.session.getTune().arrangement;
    const int undoSteps = f.session.getNumUndoSteps();
    strip.performMenuItem (0, TuneSectionStrip::deleteItem);

    CHECK (refusals == 1);
    CHECK (f.session.getTune().arrangement == before);
    CHECK (f.session.getNumUndoSteps() == undoSteps);
}

LUTHIER_TEST (TunePanel, theRhythmStripSetsTheSectionsKitFeelStrumAndOn)
{
    Fixture f;
    const auto& kits = f.processor.getGenreKits();
    const int reggae = kits.indexOf ("Reggae Skank");
    CHECK (reggae >= 0);

    choose (f.panel->getKitBox(), reggae + 1);
    CHECK (f.verse().genreKitId == "Reggae Skank");
    CHECK (f.verse().rhythmPatternId == kits.getKit (reggae).strumPatterns[0]);

    slide (f.panel->getFeelSlider(), 0.8);
    CHECK_NEAR (f.verse().feel, 0.8, 1.0e-9);

    slide (f.panel->getStrumSlider(), 0.2);
    CHECK_NEAR (f.verse().strum, 0.2, 1.0e-9);

    CHECK (f.verse().rhythmOn);
    CHECK (press (f.panel->getRhythmOnButton()));
    CHECK (! f.verse().rhythmOn);
}

LUTHIER_TEST (TunePanel, thePianoRollDrawsSnappedLockedNotesAndDeletesThem)
{
    Fixture f;
    auto& roll = f.panel->getPianoRoll();

    // 3.4: snapped to the grid and to the key (C#4 in C major is C4).
    CHECK (roll.addNote (1.2, 61, 0.7));
    const auto& notes = f.verse().melody->notes;
    CHECK (notes.size() == 1);
    CHECK_NEAR (notes[0].startBeat, 1.0, 1.0e-9);
    CHECK_NEAR (notes[0].durationBeats, 0.5, 1.0e-9);
    CHECK (notes[0].pitch.value == 60);
    CHECK (notes[0].locked);

    // 'C' toggles chromatic.
    CHECK (f.panel->keyPressed (juce::KeyPress ('c')));
    CHECK (roll.isChromatic());
    CHECK (roll.addNote (3.0, 61, 1.0));
    CHECK (f.verse().melody->notes[1].pitch.value == 61);

    CHECK (roll.toggleLockAt (3.2, 61));
    CHECK (! f.verse().melody->notes[1].locked);

    CHECK (roll.deleteNoteAt (1.1, 60));
    CHECK (f.verse().melody->notes.size() == 1);
    CHECK (! roll.deleteNoteAt (1.1, 60));

    // The quantise grid is the drawing grid.
    choose (f.panel->getQuantiseBox(), (int) QuantiseGrid::sixteenth + 1);
    CHECK_NEAR (roll.getGridBeats(), 0.25, 1.0e-9);

    // Draw off: the roll does not draw.
    CHECK (press (f.panel->getDrawButton()));
    CHECK (! roll.isDrawEnabled());
    CHECK (press (f.panel->getDrawButton()));
    CHECK (roll.isDrawEnabled());

    // Auto writes a melody around the locked note, and says with what (action-and-undo 3.9).
    type (*f.panel, "C F G C");
    CHECK (press (f.panel->getAutoButton()));
    CHECK (f.verse().melody->source == MelodySource::autoGenerate);
    CHECK (f.verse().melody->notes.size() > 1);
    CHECK (f.session.getUndoDescription().startsWith ("Generate melody with seed"));

    // Auto again is Regenerate: the next seed (4.1).
    const int seed = f.verse().melody->seed;
    CHECK (press (f.panel->getAutoButton()));
    CHECK (f.verse().melody->seed == seed + 1);

    // Improvise marks the section; Freeze keeps a pass as written notes.
    CHECK (! f.panel->getFreezeButton().isEnabled());
    CHECK (press (f.panel->getImproviseButton()));
    CHECK (f.verse().melody->source == MelodySource::improvise);
    CHECK (f.panel->getFreezeButton().isEnabled());

    CHECK (press (f.panel->getFreezeButton()));
    CHECK (f.verse().melody->source != MelodySource::improvise);
    CHECK (! f.verse().melody->notes.empty());
}

LUTHIER_TEST (TunePanel, recordingQuantisesATakeIntoTheSection)
{
    Fixture f;
    type (*f.panel, "C F");

    CHECK (press (f.panel->getRecordButton()));
    CHECK (f.session.isRecording());
    CHECK (f.player.isRecordArmed());
    CHECK (f.player.isPlaying());   // a take is played against the section

    f.session.recordNote (64, 90, 0.1, 0.9);
    f.session.recordNote (65, 80, 4.05, 4.6);

    CHECK (press (f.panel->getRecordButton()));
    CHECK (! f.session.isRecording());
    CHECK (! f.player.isRecordArmed());

    const auto& notes = f.verse().melody->notes;
    CHECK (notes.size() == 2);
    CHECK_NEAR (notes[0].startBeat, 0.0, 1.0e-9);
    CHECK (notes[0].velocity == 90);
    CHECK (f.verse().melody->source == MelodySource::record);
    CHECK (f.session.getUndoDescription() == "Record melody (Record)");
}

LUTHIER_TEST (TunePanel, aTakeFromMidiInLandsInTheSectionItWasPlayedIn)
{
    Fixture f;
    type (*f.panel, "C F");

    CHECK (press (f.panel->getRecordButton()));
    f.renderBlocks (1);

    // One beat in at 120 bpm is 24000 samples: a note from MIDI in, held half a beat.
    juce::MidiBuffer toEngine, toOut, incoming;
    toEngine.ensureSize (TunePlayer::kRecommendedMidiBytes);
    toOut.ensureSize (TunePlayer::kRecommendedMidiBytes);

    for (int block = 1; block < 160; ++block)
    {
        toEngine.clear();
        toOut.clear();
        incoming.clear();
        f.player.renderBlock (kBlock, {}, toEngine, toOut);

        const auto start = (juce::int64) block * kBlock;

        if (start <= 24000 && 24000 < start + kBlock)
            incoming.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), (int) (24000 - start));

        if (start <= 36000 && 36000 < start + kBlock)
            incoming.addEvent (juce::MidiMessage::noteOff (1, 67), (int) (36000 - start));

        f.player.captureInput (incoming);
    }

    f.panel->updateTransport();   // the timer gathers the take
    CHECK (f.session.getNumRecordedNotes() == 1);

    CHECK (press (f.panel->getRecordButton()));

    const auto& notes = f.verse().melody->notes;
    CHECK (notes.size() == 1);
    CHECK_NEAR (notes[0].startBeat, 1.0, 1.0e-9);
    CHECK_NEAR (notes[0].durationBeats, 0.5, 1.0e-9);
    CHECK (notes[0].pitch.value == 67);
}

LUTHIER_TEST (TunePanel, theTransportAndSpaceDriveThePlayer)
{
    Fixture f;
    type (*f.panel, "Am F C G");

    CHECK (press (f.panel->getPlayButton()));
    CHECK (f.player.isPlaying());
    f.renderBlocks (4);
    CHECK (f.player.getPositionPpq() > 0.0);

    // Space pauses and plays.
    CHECK (f.panel->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    CHECK (! f.player.isPlaying());
    CHECK (f.panel->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    CHECK (f.player.isPlaying());

    // Shift+Space: the section from its start.
    f.renderBlocks (100);
    CHECK (f.panel->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey, juce::ModifierKeys::shiftModifier, 0)));
    f.renderBlocks (1);
    CHECK (f.player.getPositionPpq() < 0.1);

    // gui-engine-dataflow 24: the readout and the playhead name the section.
    // 400 blocks of 256 samples at 120 bpm is 4.27 beats: bar 2 of the verse.
    f.renderBlocks (400);
    f.panel->updateTransport();
    CHECK (f.panel->getPositionText().startsWith ("Verse  bar 2"));
    CHECK (f.panel->getPlayhead().section == 0);
    CHECK (f.panel->getPlayhead().beat > 1.0);

    CHECK (press (f.panel->getLoopButton()));
    CHECK (f.player.isLooping() == f.panel->getLoopButton().getToggleState());

    CHECK (press (f.panel->getMetronomeButton()));
    CHECK (f.player.isMetronomeOn());

    CHECK (press (f.panel->getCountInButton()));
    CHECK (f.player.getCountInBars() == 1);

    // Paused and played again, it counts in.
    f.panel->getPlayButton().onClick();   // pause
    f.renderBlocks (1);
    f.panel->getPlayButton().onClick();   // play
    f.renderBlocks (8);
    f.panel->updateTransport();
    CHECK (f.player.isCountingIn());
    CHECK (f.panel->getPositionText() == "Count-in");

    // Stop: back to the start.
    f.player.stop();
    f.renderBlocks (1);
    f.panel->updateTransport();
    CHECK (f.panel->getPositionText() == "Stopped");
}

LUTHIER_TEST (TunePanel, theSessionAppliesSectionRhythmToTheRhythmEngine)
{
    Fixture f;
    type (*f.panel, "C F G C");
    choose (f.panel->getKitBox(), f.processor.getGenreKits().indexOf ("Reggae Skank") + 1);

    f.player.play();
    f.renderBlocks (2);
    f.panel->updateTransport();

    auto& engine = f.processor.getEngine().getRhythmEngine();
    CHECK (engine.getPattern().getName() == "Reggae Skank");
    CHECK (engine.isEnabled());
}

LUTHIER_TEST (TunePanel, theHeaderEditsTitleTempoKeyAndSavesAndLoads)
{
    Fixture f;
    type (*f.panel, "C F G C");

    f.panel->getTitleEditor().setText ("Morning Tune", false);
    f.panel->getTitleEditor().onTextChange();
    CHECK (f.session.getTune().meta.title == "Morning Tune");

    slide (f.panel->getTempoSlider(), 96.0);
    CHECK_NEAR (f.session.getTune().meta.tempoBpm, 96.0, 1.0e-9);

    // The key transposes: C to D moves every chord up a tone.
    choose (f.panel->getKeyBox(), 2 + 1);
    CHECK (f.session.getTune().meta.keyTonic == 2);
    CHECK (f.verse().chords[0].root == 2);

    choose (f.panel->getModeBox(), (int) TuneMode::aeolian + 1);
    CHECK (f.session.getTune().meta.mode == TuneMode::aeolian);
    CHECK (f.verse().chords[0].quality == "m");

    CHECK (f.session.isDirty());
    CHECK (f.panel->getSaveButton().getButtonText() == "SAVE *");

    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LuthierTunePanelTest");
    folder.deleteRecursively();
    folder.createDirectory();
    const auto file = folder.getChildFile ("Morning Tune.luthiertune");

    juce::String error;
    CHECK_MSG (f.panel->saveTo (file, error), error);
    CHECK (! f.session.isDirty());
    CHECK (f.session.getDisplayTitle() == "Morning Tune");

    const auto saved = f.session.getTune();
    startWithAVerse (f.session);
    CHECK_MSG (f.panel->loadFrom (file, error), error);
    CHECK (f.session.getTune() == saved);
    CHECK (! f.session.canUndo());   // a load is a state boundary

    CHECK_MSG (f.panel->exportMidiTo (folder.getChildFile ("Morning Tune.mid"), error), error);
    CHECK (folder.getChildFile ("Morning Tune.mid").getSize() > 0);

    // The shortcuts undo and redo the tune.
    slide (f.panel->getTempoSlider(), 90.0);
    CHECK (f.panel->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)));
    CHECK_NEAR (f.session.getTune().meta.tempoBpm, 96.0, 1.0e-9);
    CHECK (f.panel->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)));
    CHECK_NEAR (f.session.getTune().meta.tempoBpm, 90.0, 1.0e-9);

    folder.deleteRecursively();
}

LUTHIER_TEST (TunePanel, theSessionRoundTripsThroughPluginState)
{
    TuneSession a;
    Tune t;
    t.meta.title = "Kept";
    TuneSection verse;
    verse.name = "Verse";
    t.addSection (verse);
    a.newTune (t);
    a.edit (TuneEditClass::other, "Change tempo", [] (Tune& x) { return x.setTempo (88.0); });

    TuneSession b;
    CHECK (b.restoreState (a.toState()));
    CHECK (b.getTune() == a.getTune());
    CHECK (b.isDirty());
    CHECK (! b.canUndo());   // a restore is a state boundary
    CHECK (! b.restoreState (juce::var()));
    CHECK (b.getTune() == a.getTune());
}

LUTHIER_TEST (TunePanel, sessionUndoGroupsSameTargetEditsWithin200ms)
{
    TuneSession session;
    double now = 1000.0;
    session.setClock ([&now] { return now; });

    auto tempo = [&session] (double bpm)
    {
        return session.edit (TuneEditClass::other, "Change tempo", [bpm] (Tune& t) { return t.setTempo (bpm); }, 3001);
    };

    CHECK (tempo (100.0));
    now += 50.0;
    CHECK (tempo (101.0));
    now += 150.0;
    CHECK (tempo (102.0));
    CHECK (session.getNumUndoSteps() == 1);   // one drag, one entry

    now += 500.0;
    CHECK (tempo (110.0));
    CHECK (session.getNumUndoSteps() == 2);

    CHECK (! tempo (110.0));                  // no change, no entry
    CHECK (session.getNumUndoSteps() == 2);

    CHECK (session.undo());
    CHECK_NEAR (session.getTune().meta.tempoBpm, 102.0, 1.0e-9);
    CHECK (session.undo());
    CHECK_NEAR (session.getTune().meta.tempoBpm, 120.0, 1.0e-9);
    CHECK (session.redo());
    CHECK_NEAR (session.getTune().meta.tempoBpm, 102.0, 1.0e-9);

    // A new edit clears redo (action-and-undo 2).
    now += 500.0;
    CHECK (tempo (90.0));
    CHECK (! session.canRedo());
}

LUTHIER_TEST (TunePanel, aRhythmChangeReachesTheRhythmEngine)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    auto& engine = processor.getEngine().getRhythmEngine();

    TuneRhythmChange change;
    change.genreKitId = "Reggae Skank";
    change.patternId = "Rocksteady Skank";
    change.rhythmOn = true;
    change.feel = 0.5;
    change.strum = 0.5;

    TuneSession::applyRhythmChange (change, engine, processor.getGenreKits(), processor.getPatternLibrary());

    CHECK (engine.getPattern().getName() == "Rocksteady Skank");
    CHECK (engine.isEnabled());

    change.rhythmOn = false;
    TuneSession::applyRhythmChange (change, engine, processor.getGenreKits(), processor.getPatternLibrary());
    CHECK (! engine.isEnabled());
}

LUTHIER_TEST (TunePanel, rendersWithTheLookAndFeel)
{
    Fixture f;
    type (*f.panel, "[Verse] Am F C G [Chorus] F G Am Am");
    f.panel->getSectionStrip().performMenuItem (0, TuneSectionStrip::roleBase + (int) SectionRole::verse);
    f.panel->getSectionStrip().performMenuItem (1, TuneSectionStrip::roleBase + (int) SectionRole::chorus);
    f.panel->getPianoRoll().addNote (0.0, 69, 1.0);
    f.panel->getPianoRoll().addNote (1.0, 72, 2.0);
    f.panel->getPianoRoll().addNote (3.0, 76, 1.0);

    // A playhead part-way through the verse.
    f.player.play();
    f.renderBlocks (250);
    f.panel->updateTransport();

    type (*f.panel, "Am Cfoo F");   // show the error underline too

    LuthierLookAndFeel lookAndFeel;
    f.panel->setLookAndFeel (&lookAndFeel);
    f.panel->setSize (420, f.panel->getPreferredHeight());

    juce::Image image (juce::Image::ARGB, f.panel->getWidth(), f.panel->getHeight(), true, juce::SoftwareImageType());

    {
        juce::Graphics g (image);
        f.panel->paintEntireComponent (g, true);
    }

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-guitar-renders");
    dir.createDirectory();
    const auto png = dir.getChildFile ("_tune.png");
    png.deleteFile();

    {
        juce::FileOutputStream out (png);
        juce::PNGImageFormat().writeImageToStream (image, out);
    }

    CHECK (png.existsAsFile() && png.getSize() > 0);

    // Something was drawn: not one flat colour.
    const auto corner = image.getPixelAt (2, 2);
    bool varied = false;

    for (int y = 0; y < image.getHeight() && ! varied; y += 7)
        for (int x = 0; x < image.getWidth() && ! varied; x += 7)
            varied = image.getPixelAt (x, y) != corner;

    CHECK (varied);

    f.panel->setLookAndFeel (nullptr);
}

// action-and-undo.md 3.9: drawing and deleting the same note within 200 ms is
// one melody entry; a different note is its own.
LUTHIER_TEST (TunePanel, melodyNoteEditsGroupOnTheSameNote)
{
    Fixture f;
    double now = 5000.0;
    f.session.setClock ([&now] { return now; });

    auto& roll = f.panel->getPianoRoll();
    const int before = f.session.getNumUndoSteps();

    CHECK (roll.addNote (1.0, 60, 0.5));
    now += 100.0;
    CHECK (roll.deleteNoteAt (1.1, 60));
    CHECK_MSG (f.session.getNumUndoSteps() == before + 1,
               "draw + delete of one note made " + juce::String (f.session.getNumUndoSteps() - before) + " entries");

    now += 100.0;
    CHECK (roll.addNote (2.0, 64, 0.5));
    CHECK (f.session.getNumUndoSteps() == before + 2);
}
