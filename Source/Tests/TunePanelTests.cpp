/*  The TUNE workspace tab (tune-builder.md 2, 3; gui-integration.md 4.4) and
    the TuneSession behind it.

    Controls are driven through their callbacks, as MidiOutPanelTests does:
    triggerClick is asynchronous and never runs in the console test runner. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/TunePanel.h"
#include "../UI/TuneExportPanel.h"
#include "../Tune/TuneExport.h"
#include "../Export/MidiProfiles.h"

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

//==============================================================================
LUTHIER_TEST (TunePanel, importMidiReplacesTheTuneStopsThePlayerAndReportsWhatItDid)
{
    Fixture f;

    // Something to import: the verse with chords, exported the way EXPORT does.
    type (*f.panel, "Am F C G");
    f.session.edit (TuneEditClass::other, "Title", [] (Tune& t) { t.meta.title = "Exported Sketch"; return true; });

    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LuthierTunePanelImport");
    folder.createDirectory();
    const auto midi = folder.getChildFile ("sketch.mid");

    juce::String error;
    CHECK_MSG (f.panel->exportMidiTo (midi, error), error);

    // Now a different tune is loaded and playing.
    startWithAVerse (f.session);
    CHECK (f.session.getTune().meta.title == "Sketch");
    press (f.panel->getPlayButton());
    f.renderBlocks (4);

    juce::String report;
    bool reportedWarning = false;
    f.panel->onNotification = [&] (const juce::String& message, bool warning)
    {
        report = message;
        reportedWarning = warning;
    };

    CHECK_MSG (f.panel->importMidiFrom (midi, error), error);

    // 15-08 through the panel: the export came back as the tune it was.
    CHECK (f.session.getTune().meta.title == "Exported Sketch");
    CHECK (f.verse().chords.size() == 4);
    CHECK (f.verse().chords.size() == 4 && f.verse().chords[0].root == 9 && f.verse().chords[0].quality == "m");
    CHECK (f.verse().chords.size() == 4 && f.verse().chords[3].root == 7 && f.verse().chords[3].quality.isEmpty());

    // A state boundary: no undo into the previous tune, no file, not dirty.
    CHECK (! f.session.canUndo());
    CHECK (f.session.getFile() == juce::File());
    CHECK (! f.session.isDirty());
    CHECK (! f.player.isPlaying());

    // The report names the file and says what an import is.
    CHECK (report.contains ("sketch.mid"));
    CHECK (report.contains ("strummed"));
    juce::ignoreUnused (reportedWarning);

    // A refused file: the tune stays, the error names the file.
    const auto corrupt = folder.getChildFile ("corrupt.mid");
    corrupt.replaceWithText ("nothing like a midi file");
    error.clear();
    CHECK (! f.panel->importMidiFrom (corrupt, error));
    CHECK (error.contains ("corrupt.mid"));
    CHECK (f.session.getTune().meta.title == "Exported Sketch");

    // The button is there, beside LOAD, and does the same thing.
    CHECK (f.panel->getImportButton().isVisible());
    CHECK (f.panel->getImportButton().onClick != nullptr);
    CHECK (f.panel->getImportButton().getBounds().getX() > f.panel->getLoadButton().getBounds().getX());
    CHECK (f.panel->getImportButton().getBounds().getY() == f.panel->getLoadButton().getBounds().getY());

    folder.deleteRecursively();
}

//==============================================================================
// tune-builder 3.2: the chord pills' popover, drag, right edge and menu.
//==============================================================================
LUTHIER_TEST (TunePanel, chordPillsEditReorderResizeLockCopyAndSubstitute)
{
    Fixture f;
    type (*f.panel, "Am F C G");
    auto& pills = f.panel->getChordPills();
    pills.setBounds (0, 0, 400, 34);

    // Four pills across the strip, in order, each hit-testable.
    CHECK (! pills.getPillBounds (0).isEmpty());
    CHECK (pills.getPillBounds (1).getX() > pills.getPillBounds (0).getRight() - 1.0f);
    CHECK (pills.getCellAt (pills.getPillBounds (2).getCentre().toInt()) == 2);
    CHECK (pills.getCellAt ({ 0, 100 }) == -1);
    CHECK (pills.isOnRightEdge ({ (int) pills.getPillBounds (0).getRight() - 3, 17 }));
    CHECK (! pills.isOnRightEdge (pills.getPillBounds (0).getCentre().toInt()));

    // The popover edits the cell: root, quality, bass, beats, fill, lock.
    {
        TuneChordEditor editor (f.session, 0, 1);
        choose (editor.getRootBox(), 2 + 1);                       // D
        CHECK (f.verse().chords[1].root == 2);

        const auto qualities = TuneChordEditor::getQualityChoices();
        CHECK (qualities.contains ("") && qualities.contains ("m") && qualities.contains ("7") && qualities.contains ("maj7"));
        choose (editor.getQualityBox(), qualities.indexOf ("m7") + 1);
        CHECK (f.verse().chords[1].quality == "m7");

        choose (editor.getBassBox(), 9 + 2);                       // /A
        CHECK (f.verse().chords[1].bass == 9);
        choose (editor.getBassBox(), 1);                           // back to the root
        CHECK (f.verse().chords[1].bass == -1);

        slide (editor.getBeatsSlider(), 2.0);
        CHECK_NEAR (f.verse().chords[1].durationBeats, 2.0, 1.0e-9);

        CHECK (press (editor.getFillToggle()));
        CHECK (f.verse().chords[1].holdsToFill());
        CHECK (press (editor.getFillToggle()));
        CHECK (! f.verse().chords[1].holdsToFill());

        CHECK (press (editor.getLockToggle()));
        CHECK (f.verse().chords[1].locked);

        editor.getExtensionsEditor().setText ("9, add9", false);
        editor.getExtensionsEditor().onFocusLost();
        CHECK (f.verse().chords[1].extensions.size() == 2);

        // Every popover edit is on the tune's undo stack.
        CHECK (f.session.canUndo());
        CHECK (f.session.getUndoDescription().isNotEmpty());
    }

    // Drag to reorder (3.2): Am to the end.
    CHECK (pills.reorder (0, 3));
    CHECK (f.verse().chords[3].root == 9 && f.verse().chords[3].quality == "m");
    CHECK (f.verse().chords[0].root == 2);   // the edited Dm7 is first now

    // Drag the right edge: the duration, in half beats.
    CHECK (pills.setDuration (0, 3.2));
    CHECK_NEAR (f.verse().chords[0].durationBeats, 3.0, 1.0e-9);

    // The menu: duplicate, insert, delete, lock, copy and paste.
    const auto cells = (int) f.verse().chords.size();
    pills.performMenuItem (0, TuneChordPills::duplicateItem);
    CHECK ((int) f.verse().chords.size() == cells + 1);
    CHECK (f.verse().chords[1].root == f.verse().chords[0].root && ! f.verse().chords[1].locked);

    pills.performMenuItem (1, TuneChordPills::deleteItem);
    CHECK ((int) f.verse().chords.size() == cells);

    pills.performMenuItem (1, TuneChordPills::insertAfterItem);
    CHECK ((int) f.verse().chords.size() == cells + 1);
    CHECK (f.verse().chords[2].root == f.session.getTune().meta.keyTonic);   // the key's tonic, to edit
    pills.performMenuItem (2, TuneChordPills::deleteItem);

    pills.performMenuItem (1, TuneChordPills::lockItem);
    CHECK (f.verse().chords[1].locked);
    pills.performMenuItem (1, TuneChordPills::lockItem);
    CHECK (! f.verse().chords[1].locked);

    pills.performMenuItem (3, TuneChordPills::copyItem);
    CHECK (TuneChordPills::getClipboard().has_value());
    pills.performMenuItem (1, TuneChordPills::pasteItem);
    CHECK (f.verse().chords[1] == f.verse().chords[3]);

    // Suggest substitution offers something for an unlocked dominant and nothing for a locked one.
    type (*f.panel, "C G7 C F");
    CHECK (pills.buildMenu (1).getNumItems() > 0);
    const auto offers = suggestSubstitutions (f.verse().chords, 1, f.session.getTune().meta.keyTonic, f.session.getTune().meta.mode);
    CHECK (! offers.empty());
    pills.performMenuItem (1, TuneChordPills::substitutionBase + 0);
    CHECK (f.verse().chords[1].root == offers[0].cells[0].root);

    // Suggest next chord appends after the cell.
    const auto before = (int) f.verse().chords.size();
    pills.performMenuItem (0, TuneChordPills::suggestBase + 0);
    CHECK ((int) f.verse().chords.size() == before + 1);

    // A locked cell sits out of Reharmonize (3.2 Lock, 5).
    type (*f.panel, "C G7 C F");
    pills.performMenuItem (1, TuneChordPills::lockItem);
    ReharmonizeOptions options;
    Tune copy = f.session.getTune();
    reharmonizeSection (copy, 0, options);
    // The locked G7 survives as written (the C before it may have been split for a secondary dominant).
    bool lockedKept = false;

    for (const auto& c : copy.arrangement.sections[0].chords)
        lockedKept = lockedKept || (c.locked && c.root == 7 && c.quality == "7" && c.durationBeats == 4.0);

    CHECK (lockedKept);

    // The lock survives the file.
    const auto json = TuneFile::toJson (f.session.getTune());
    Tune back;
    CHECK (TuneFile::fromJson (json, back).ok());
    CHECK (back == f.session.getTune());
    CHECK (back.arrangement.sections[0].chords[1].locked);
}

//==============================================================================
// tune-builder 3.3: drag to reorder the strip, and Vary.
//==============================================================================
LUTHIER_TEST (TunePanel, sectionStripDragReordersAndVaryMakesASibling)
{
    Fixture f;
    type (*f.panel, "[Verse] Am F C G [Chorus] F G Am Am");
    auto& strip = f.panel->getSectionStrip();
    strip.setBounds (0, 0, 400, 34);

    // A setlist, so the drag has to move play order too.
    strip.performMenuItem (0, TuneSectionStrip::repeatBase + 2);
    CHECK (f.session.getTune().arrangement.setlist.size() == 2);

    CHECK (strip.getTabAt (strip.getTabBounds (1).getCentre()) == 1);
    CHECK (strip.reorder (1, 0));
    CHECK (f.session.getTune().arrangement.sections[0].name == "Chorus");
    CHECK (f.session.getTune().arrangement.setlist[0].section == "Chorus");
    CHECK (f.session.getTune().arrangement.setlist[1].section == "Verse");
    CHECK (f.session.getTune().arrangement.setlist[1].repeats == 2);   // repeats kept
    CHECK (f.session.getSelectedSection() == 0);
    CHECK (f.session.getUndoDescription().isNotEmpty());

    // Vary: a sibling after the verse, on the next melody seed, locked notes kept.
    f.session.setSelectedSection (1);
    CHECK (press (f.panel->getAutoButton()));
    f.panel->getPianoRoll().addNote (0.0, 72, 1.0);   // a locked, hand-drawn note
    const auto verse = f.session.getTune().arrangement.sections[1];   // a copy: Vary moves the vector
    const int seed = verse.melody->seed;
    const auto pattern = verse.rhythmPatternId;

    const int created = strip.vary (1);
    CHECK (created == 2);
    CHECK (f.session.getTune().getNumSections() == 3);
    const auto& sibling = f.session.getTune().arrangement.sections[2];
    CHECK (sibling.name == "Verse var");
    CHECK (sibling.melody.has_value() && sibling.melody->seed == seed + 1);
    CHECK (sibling.melody->notes != verse.melody->notes);

    bool keptLocked = false;

    for (const auto& n : sibling.melody->notes)
        keptLocked = keptLocked || (n.locked && n.pitch.value == 72 && n.startBeat == 0.0);

    CHECK (keptLocked);
    CHECK (sibling.chords == verse.chords);

    // The kit has more than one pattern, so the sibling steps to the next.
    const auto patterns = f.session.getKitPatterns (1);
    CHECK (patterns.size() > 1);
    CHECK (sibling.rhythmPatternId != pattern);

    // It plays: the setlist got an entry right after the verse's.
    const auto& setlist = f.session.getTune().arrangement.setlist;
    CHECK (setlist.size() == 3 && setlist[2].section == "Verse var");
    CHECK (f.session.getSelectedSection() == 2);

    // The menu offers it, and performMenuItem does it.
    strip.performMenuItem (0, TuneSectionStrip::varyItem);
    CHECK (f.session.getTune().getNumSections() == 4);
    CHECK (f.session.getTune().arrangement.sections[1].name == "Chorus var");
}

//==============================================================================
// tune-builder 3.4: selection, clipboard, nudge, velocity and the note menu.
//==============================================================================
LUTHIER_TEST (TunePanel, pianoRollSelectsNudgesCopiesPastesAndEditsVelocity)
{
    Fixture f;
    type (*f.panel, "C F G C");
    auto& roll = f.panel->getPianoRoll();
    roll.setBounds (0, 0, 400, 200);

    CHECK (roll.addNote (0.0, 60, 1.0));
    CHECK (roll.addNote (2.0, 64, 1.0));
    CHECK (roll.addNote (4.0, 67, 1.0));
    CHECK (roll.getSelection().size() == 1 && roll.getSelection().count (2) == 1);   // the drawn note is selected

    // Click selects, Shift-click adds and takes away again.
    roll.select (0, false);
    CHECK ((roll.getSelection() == std::set<int> { 0 }));
    roll.select (1, true);
    CHECK ((roll.getSelection() == std::set<int> { 0, 1 }));
    roll.select (1, true);
    CHECK ((roll.getSelection() == std::set<int> { 0 }));

    // A drag-box.
    roll.selectInBox (1.5, 4.5, 60, 70, false);
    CHECK ((roll.getSelection() == std::set<int> { 1, 2 }));
    roll.selectAll();
    CHECK (roll.getSelection().size() == 3);

    // Nudge: right by the grid, up by a scale step (E4 to F4 in C major), Shift for a bar / octave.
    roll.select (1, false);
    CHECK (roll.keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    CHECK_NEAR (f.verse().melody->notes[1].startBeat, 2.5, 1.0e-9);
    CHECK (roll.keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
    CHECK (f.verse().melody->notes[1].pitch.value == 65);
    CHECK (roll.keyPressed (juce::KeyPress (juce::KeyPress::upKey, juce::ModifierKeys::shiftModifier, 0)));
    CHECK (f.verse().melody->notes[1].pitch.value == 77);
    CHECK (roll.keyPressed (juce::KeyPress (juce::KeyPress::rightKey, juce::ModifierKeys::shiftModifier, 0)));
    CHECK_NEAR (f.verse().melody->notes[1].startBeat, 6.5, 1.0e-9);
    CHECK (f.verse().melody->notes[1].locked);

    // Chromatic: a semitone.
    roll.setChromatic (true);
    CHECK (roll.keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
    CHECK (f.verse().melody->notes[1].pitch.value == 76);
    roll.setChromatic (false);

    // Velocity: Ctrl+Down by ten, and the menu's presets.
    CHECK (roll.keyPressed (juce::KeyPress (juce::KeyPress::downKey, juce::ModifierKeys::commandModifier, 0)));
    CHECK (f.verse().melody->notes[1].velocity == 90);
    roll.performNoteMenuItem (1, TunePianoRoll::velocityBase + 0);
    CHECK (f.verse().melody->notes[1].velocity == TunePianoRoll::kVelocityPresets[0]);
    CHECK (roll.buildNoteMenu (1).getNumItems() > 0);

    // Articulation, technique, lock through the menu.
    roll.performNoteMenuItem (1, TunePianoRoll::articulationBase + (int) NoteArticulation::staccato);
    CHECK (f.verse().melody->notes[1].articulation == NoteArticulation::staccato);
    roll.performNoteMenuItem (1, TunePianoRoll::techniqueBase + (int) NoteTechnique::bend);
    CHECK (f.verse().melody->notes[1].technique == NoteTechnique::bend);
    roll.performNoteMenuItem (1, TunePianoRoll::lockItem);
    CHECK (! f.verse().melody->notes[1].locked);
    roll.performNoteMenuItem (1, TunePianoRoll::lockItem);
    CHECK (f.verse().melody->notes[1].locked);

    // The menu on a note outside the selection acts on that note.
    roll.select (0, false);
    roll.performNoteMenuItem (2, TunePianoRoll::velocityBase + 4);
    CHECK (f.verse().melody->notes[2].velocity == 127);
    CHECK (f.verse().melody->notes[0].velocity == 100);

    // Copy two notes, paste them a bar later at the cursor, keeping their spacing.
    roll.select (0, false);
    roll.select (2, true);
    CHECK (roll.keyPressed (juce::KeyPress ('c', juce::ModifierKeys::commandModifier, 0)));
    CHECK (TunePianoRoll::getClipboard().size() == 2);
    roll.setCursorBeat (8.0);
    CHECK (roll.keyPressed (juce::KeyPress ('v', juce::ModifierKeys::commandModifier, 0)));
    CHECK (f.verse().melody->notes.size() == 5);
    CHECK_NEAR (f.verse().melody->notes[3].startBeat, 8.0, 1.0e-9);
    CHECK_NEAR (f.verse().melody->notes[4].startBeat, 12.0, 1.0e-9);
    CHECK (f.verse().melody->notes[4].pitch.value == 67);
    CHECK ((roll.getSelection() == std::set<int> { 3, 4 }));   // pasted notes are the selection

    // The panel forwards the keys when it has focus rather than the roll.
    CHECK (f.panel->keyPressed (juce::KeyPress ('x', juce::ModifierKeys::commandModifier, 0)));
    CHECK (f.verse().melody->notes.size() == 3);
    CHECK (TunePianoRoll::getClipboard().size() == 2);

    // Delete key, and Escape clears the selection.
    roll.select (0, false);
    CHECK (roll.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    CHECK (f.verse().melody->notes.size() == 2);
    roll.selectAll();
    CHECK (roll.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK (roll.getSelection().empty());
    CHECK (! roll.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));

    // Every one of those was an undo entry the tune can take back.
    const auto notes = f.verse().melody->notes;
    CHECK (f.session.undo());
    CHECK (f.verse().melody->notes != notes);

    // A paste past the section's end is refused, not truncated into nothing.
    roll.setCursorBeat (100.0);
    CHECK (! roll.paste());
}

//==============================================================================
// tune-builder 6 and 7: the same roll edits the bass line and the layer.
//==============================================================================
LUTHIER_TEST (TunePanel, pianoRollEditsBassAndLayerPartsWithTheSameEditor)
{
    Fixture f;
    type (*f.panel, "C F G C");
    auto& roll = f.panel->getPianoRoll();
    roll.setBounds (0, 0, 400, 200);

    // The rhythm strip's bass mode.
    choose (f.panel->getBassModeBox(), (int) BassMode::rootFifth + 1);
    CHECK (f.verse().bass.mode == BassMode::rootFifth);

    // BASS shows the derived line, grey, and a first edit makes it Manual with that line kept.
    CHECK (press (f.panel->getPartButton (TuneNotePart::bass)));
    CHECK (f.panel->getPart() == TuneNotePart::bass);
    CHECK (roll.isShowingDerivedBass());
    const auto derived = roll.getShownNotes();
    CHECK (! derived.empty());
    CHECK (roll.getNotes() == nullptr);
    CHECK (roll.getLowestPitch() < 40);   // the bass register

    CHECK (roll.addNote (0.0, 31, 1.0));
    CHECK (f.verse().bass.mode == BassMode::manual);
    CHECK (f.verse().bass.notes.size() == derived.size() + 1);
    CHECK (! roll.isShowingDerivedBass());
    CHECK (roll.getNotes() != nullptr && roll.getNotes()->size() == derived.size() + 1);

    // Switching the box to Manual from a pattern keeps the pattern's line to edit from.
    choose (f.panel->getBassModeBox(), (int) BassMode::walking + 1);
    CHECK (f.verse().bass.mode == BassMode::walking);
    choose (f.panel->getBassModeBox(), (int) BassMode::manual + 1);
    CHECK (f.verse().bass.mode == BassMode::manual);
    CHECK (! f.verse().bass.notes.empty());

    // Bass notes nudge and delete like melody notes, and the roll's edit class is the section's.
    roll.selectAll();
    CHECK (roll.nudgeSelected (0.5, 0));
    CHECK (roll.deleteSelected());
    CHECK (f.verse().bass.notes.empty());

    // LAYER edits the countermelody, creating the layer on the first note.
    CHECK (f.verse().findLayer (LayerType::countermelody) == nullptr);
    f.panel->setPart (TuneNotePart::countermelody);
    CHECK (f.panel->getPartButton (TuneNotePart::countermelody).getToggleState());
    CHECK (roll.addNote (1.0, 55, 0.5));
    const auto* layer = f.verse().findLayer (LayerType::countermelody);
    CHECK (layer != nullptr && layer->notes.size() == 1 && layer->notes[0].locked);

    // The layer toggles: PAD on makes a pad layer, COUNTER off keeps its notes.
    CHECK (press (f.panel->getLayerButton (LayerType::pad)));
    CHECK (f.verse().findLayer (LayerType::pad) != nullptr && f.verse().findLayer (LayerType::pad)->enabled);
    f.panel->refresh();
    CHECK (f.panel->getLayerButton (LayerType::countermelody).getToggleState());
    CHECK (press (f.panel->getLayerButton (LayerType::countermelody)));
    CHECK (! f.verse().findLayer (LayerType::countermelody)->enabled);
    CHECK (f.verse().findLayer (LayerType::countermelody)->notes.size() == 1);

    // Back to MELODY: the melody is untouched by all of it.
    f.panel->setPart (TuneNotePart::melody);
    CHECK (! f.verse().melody.has_value() || f.verse().melody->notes.empty());

    // The parts round-trip through the file.
    const auto json = TuneFile::toJson (f.session.getTune());
    Tune back;
    CHECK (TuneFile::fromJson (json, back).ok());
    CHECK (back == f.session.getTune());
}

//==============================================================================
// tune-builder 3.1: "1-4 bars visible, scrollable"; the keyboard column.
//==============================================================================
LUTHIER_TEST (TunePanel, pianoRollScrollsAndZoomsAlongTheBars)
{
    Fixture f;
    type (*f.panel, "C F G C C F G C");   // eight bars, 32 beats
    auto& roll = f.panel->getPianoRoll();
    roll.setBounds (0, 0, 430, 200);

    roll.setView (0.0, 16.0);
    CHECK_NEAR (roll.getVisibleBeats(), 16.0, 1.0e-9);
    CHECK_NEAR (roll.beatAt ((float) TunePianoRoll::kKeyboardWidth), 0.0, 1.0e-9);
    CHECK_NEAR (roll.beatAt (430.0f), 16.0, 1.0e-6);

    // A note's bounds and beatAt agree, right of the keyboard column.
    const auto bounds = roll.getNoteBounds (8.0, 60, 1.0);
    CHECK (bounds.getX() >= (float) TunePianoRoll::kKeyboardWidth);
    CHECK_NEAR (roll.beatAt (bounds.getX()), 8.0, 1.0e-6);

    // Scroll: the second half.
    roll.scrollBy (16.0);
    CHECK_NEAR (roll.getViewStartBeat(), 16.0, 1.0e-9);
    CHECK_NEAR (roll.beatAt ((float) TunePianoRoll::kKeyboardWidth), 16.0, 1.0e-9);
    roll.scrollBy (100.0);                                    // clamped to the end
    CHECK_NEAR (roll.getViewStartBeat(), 16.0, 1.0e-9);

    // Zoom in about the centre, and the buttons.
    roll.zoom (2.0);
    CHECK_NEAR (roll.getVisibleBeats(), 8.0, 1.0e-9);
    CHECK_NEAR (roll.getViewStartBeat(), 20.0, 1.0e-9);
    CHECK (press (f.panel->getZoomOutButton()));
    CHECK_NEAR (roll.getVisibleBeats(), 12.0, 1.0e-9);
    CHECK (press (f.panel->getZoomInButton()));
    CHECK_NEAR (roll.getVisibleBeats(), 8.0, 1.0e-9);

    // Never more than the section, never less than a beat.
    roll.zoom (0.01);
    CHECK_NEAR (roll.getVisibleBeats(), 32.0, 1.0e-9);
    CHECK_NEAR (roll.getViewStartBeat(), 0.0, 1.0e-9);
    roll.zoom (1000.0);
    CHECK_NEAR (roll.getVisibleBeats(), 1.0, 1.0e-9);

    // showBeat brings a beat into view.
    roll.setView (0.0, 8.0);
    roll.showBeat (20.0);
    CHECK (roll.getViewStartBeat() <= 20.0 && roll.getViewStartBeat() + roll.getVisibleBeats() >= 20.0);

    // Drawing past the view's end still lands in the section.
    CHECK (roll.addNote (30.0, 60, 1.0));
    CHECK_NEAR (f.verse().melody->notes[0].startBeat, 30.0, 1.0e-9);
}

//==============================================================================
// tune-builder 3.6: STOP; 2.1: the kit's suggested tempo.
//==============================================================================
LUTHIER_TEST (TunePanel, stopReturnsToTheStartAndKitsSuggestATempo)
{
    Fixture f;
    type (*f.panel, "Am F C G");

    CHECK (press (f.panel->getPlayButton()));
    f.renderBlocks (100);
    CHECK (f.player.getPositionPpq() > 0.5);

    CHECK (press (f.panel->getStopButton()));
    CHECK (! f.player.isPlaying());
    f.renderBlocks (1);
    f.panel->updateTransport();
    CHECK_NEAR (f.player.getPositionPpq(), 0.0, 1.0e-9);
    CHECK (f.panel->getPositionText() == "Stopped");
    CHECK (f.panel->getStopButton().getTooltip().isNotEmpty());

    // 2.1: an untouched tempo follows the kit's suggestion; one the user set stays.
    const auto& kits = f.processor.getGenreKits();
    CHECK_NEAR (f.session.getTune().meta.tempoBpm, 120.0, 1.0e-9);
    choose (f.panel->getKitBox(), kits.indexOf ("Reggae Skank") + 1);
    CHECK_NEAR (f.session.getTune().meta.tempoBpm, getKitSuggestedTempo ("Reggae Skank"), 1.0e-9);
    CHECK (getKitSuggestedTempo ("Reggae Skank") < getKitSuggestedTempo ("Punk Downstroke"));

    choose (f.panel->getKitBox(), kits.indexOf ("Punk Downstroke") + 1);
    CHECK_NEAR (f.session.getTune().meta.tempoBpm, getKitSuggestedTempo ("Punk Downstroke"), 1.0e-9);

    slide (f.panel->getTempoSlider(), 100.0);
    choose (f.panel->getKitBox(), kits.indexOf ("Reggae Skank") + 1);
    CHECK_NEAR (f.session.getTune().meta.tempoBpm, 100.0, 1.0e-9);

    // Every new control carries a tooltip and an accessible name (accessibility 0.1).
    for (auto* c : { static_cast<juce::Component*> (&f.panel->getStopButton()),
                     static_cast<juce::Component*> (&f.panel->getZoomInButton()),
                     static_cast<juce::Component*> (&f.panel->getPartButton (TuneNotePart::bass)),
                     static_cast<juce::Component*> (&f.panel->getLayerButton (LayerType::pad)),
                     static_cast<juce::Component*> (&f.panel->getBassModeBox()),
                     static_cast<juce::Component*> (&f.panel->getChordPills()),
                     static_cast<juce::Component*> (&f.panel->getPianoRoll()) })
    {
        CHECK_MSG (c->getTitle().isNotEmpty() || c->getName().isNotEmpty(), "a control without an accessible name");

        if (auto* tip = dynamic_cast<juce::TooltipClient*> (c))
            CHECK_MSG (tip->getTooltip().isNotEmpty(), "a control without a tooltip");
    }
}

//==============================================================================
// tune-builder 9: the one-screen export dialog writes every ticked destination.
//==============================================================================
LUTHIER_TEST (TunePanel, theExportDialogWritesAudioMidiNotationAndProject)
{
    Fixture f;
    type (*f.panel, "Am F");
    f.session.edit (TuneEditClass::other, "Title", [] (Tune& t) { t.meta.title = "Export Me"; t.setTempo (240.0); return true; });
    f.session.edit (TuneEditClass::other, "Short", [] (Tune& t) { return t.setSectionLength (0, 1); });
    f.panel->getPianoRoll().addNote (0.0, 69, 1.0);

    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LuthierTuneExportDialog");
    folder.deleteRecursively();

    auto& dialog = f.panel->getExportPanel();
    dialog.setSize (640, 560);
    dialog.overlayShown();
    dialog.setFolder (folder);

    CHECK (dialog.getNameEditor().getText() == "Export Me");
    CHECK (dialog.getAudioToggle().getToggleState() && dialog.getMidiToggle().getToggleState()
             && dialog.getProjectToggle().getToggleState());

    dialog.getNotationToggle().setToggleState (true, juce::sendNotificationSync);
    dialog.getTailSlider().setValue (0.2, juce::sendNotificationSync);
    choose (dialog.getSampleRateBox(), 44100);
    choose (dialog.getProfileBox(), (int) MidiProfile::luthier + 1);

    const auto request = dialog.getRequest();
    CHECK (request.audio && request.midi && request.notation && request.project);
    CHECK (request.folder == folder && request.baseName == "Export Me");
    CHECK_NEAR (request.sampleRate, 44100.0, 1.0e-9);
    CHECK (request.profile == MidiProfile::luthier);

    CHECK (dialog.startExport());
    CHECK (dialog.isBusy());

    // The files that need no render are there at once.
    CHECK (folder.getChildFile ("Export Me.mid").existsAsFile());
    CHECK (folder.getChildFile ("Export Me.musicxml").existsAsFile() || folder.getChildFile ("Export Me.xml").existsAsFile()
             || folder.findChildFiles (juce::File::findFiles, false, "Export Me.*xml").size() > 0);
    CHECK (folder.getChildFile ("Export Me.luthiertune").existsAsFile());
    CHECK (dialog.getReport().errors.isEmpty());

    // The render runs on the exporter's thread; the file appears when it is done.
    for (int waited = 0; f.processor.getExporter().isExporting() && waited < 1200; ++waited)
        juce::Thread::sleep (50);

    CHECK (! f.processor.getExporter().isExporting());
    const auto wav = folder.getChildFile ("Export Me.wav");
    CHECK_MSG (wav.existsAsFile() && wav.getSize() > 1000, "no audio render: " + wav.getFullPathName());

    // The MIDI file went through midi-export's profiles (C-53): the Luthier
    // header is there, and the import reads the tune back.
    {
        MidiPerformance back (44100.0);
        const auto result = MidiProfiles::importFromFile (folder.getChildFile ("Export Me.mid"), back, 44100.0);
        CHECK_MSG (result.ok, result.error);
        CHECK (result.detectedProfile == MidiProfile::luthier);
        CHECK (back.getSectionNames().contains ("Verse"));
        CHECK (! back.getMessages().empty());
    }

    // A project that opens as the tune it was.
    {
        Tune back;
        CHECK (TuneFile::load (folder.getChildFile ("Export Me.luthiertune"), back).ok());
        CHECK (back == f.session.getTune());
    }

    // Nothing ticked: refused, with a reason on the screen.
    for (auto* t : { &dialog.getAudioToggle(), &dialog.getMidiToggle(), &dialog.getNotationToggle(), &dialog.getProjectToggle() })
        t->setToggleState (false, juce::sendNotificationSync);

    CHECK (! dialog.startExport());
    CHECK (dialog.getStatusText().isNotEmpty());

    // Ctrl+E on the panel opens the dialog (through the editor's host when wired).
    OverlayPanel* shown = nullptr;
    f.panel->onShowOverlay = [&shown] (OverlayPanel* p) { shown = p; };
    CHECK (f.panel->keyPressed (juce::KeyPress ('e', juce::ModifierKeys::commandModifier, 0)));
    CHECK (shown == &dialog);
    CHECK (f.panel->getExportButton().getTooltip().contains ("Ctrl+E"));

    folder.deleteRecursively();
}

//==============================================================================
// 9.2 both profiles come back through the importer (15-08 via profiles).
//==============================================================================
LUTHIER_TEST (TunePanel, exportedMidiInEitherProfileImportsAsTheSameChords)
{
    Fixture f;
    type (*f.panel, "Am F C G");
    f.session.edit (TuneEditClass::other, "Title", [] (Tune& t) { t.meta.title = "Round Trip"; return true; });
    f.panel->getPianoRoll().addNote (0.0, 69, 1.0);
    f.panel->getPianoRoll().addNote (4.0, 72, 2.0);
    const auto original = f.session.getTune();

    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("LuthierTuneProfiles");
    folder.deleteRecursively();
    folder.createDirectory();

    for (auto profile : { MidiProfile::luthier, MidiProfile::generic })
    {
        TuneExportRequest request;
        request.folder = folder;
        request.baseName = juce::String ("Round Trip ") + MidiProfiles::getProfileName (profile);
        request.profile = profile;
        request.split = MidiTrackSplit::perInstrument;

        const auto file = request.fileFor ({}, ".mid");
        juce::String error;
        CHECK_MSG (TuneExport::writeMidi (original, f.session.getMidiOptions(), request, file, error), error);

        f.session.newTune (Tune());
        CHECK_MSG (f.panel->importMidiFrom (file, error), error);

        const auto& back = f.session.getTune();
        CHECK_MSG (back.getNumSections() >= 1, juce::String ("no sections back from the ") + MidiProfiles::getProfileName (profile));

        if (back.getNumSections() >= 1)
        {
            const auto& chords = back.arrangement.sections[0].chords;
            CHECK_MSG (chords.size() == 4, juce::String ("chords back: ") + juce::String ((int) chords.size()));

            if (chords.size() == 4)
            {
                CHECK (chords[0].root == 9 && chords[0].quality == "m");
                CHECK (chords[3].root == 7 && chords[3].quality.isEmpty());
            }

            CHECK (back.arrangement.sections[0].melody.has_value() && back.arrangement.sections[0].melody->notes.size() == 2);
        }
    }

    folder.deleteRecursively();
}
