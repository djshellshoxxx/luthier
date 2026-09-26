/*  The TUNE tab in the plugin (tune-builder.md 2.1, 12, 14, 15; gui-integration
    17; action-and-undo 3.9). TUNE-HELP-ONBOARDING workstream. */

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Tune/TuneExamples.h"
#include "../Tune/TuneHarmony.h"
#include "../UI/TunePanel.h"
#include "../Support/TuneExport.h"
#include "../UI/TuneExportDialog.h"
#include "../Tune/TuneFile.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    Tune verseTune (const juce::String& kit = "Folk Fingerstyle")
    {
        Tune t;
        t.meta.title = "Integration";
        TuneSection verse;
        verse.name = "Verse";
        verse.lengthBars = 4;
        verse.genreKitId = kit;
        verse.rhythmPatternId = "Folk Down Up";
        t.addSection (verse);
        applyProgressionText (t, 0, "C Am F G");
        return t;
    }
}

//==============================================================================
/*  2.1: a kit brings its suggested tempo, swing and feel while the tempo is
    still the default or the old kit's, never over one the player chose; its
    chord palette adds chords; KIT sets the tempo on request. */
LUTHIER_TEST (TuneIntegration, aGenreKitBringsItsTempoFeelAndPalette)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto& session = processor->getTuneSession();
    session.newTune (verseTune (""));
    TunePanel panel (*processor, processor->getTunePlayer(), session);

    auto chooseKit = [&panel, &processor] (const juce::String& name)
    {
        panel.getKitBox().setSelectedId (processor->getGenreKits().indexOf (name) + 1, juce::dontSendNotification);
        panel.getKitBox().onChange();
    };

    chooseKit ("Reggae Skank");
    const auto reggae = TuneKits::getSuggestion ("Reggae Skank");
    CHECK_NEAR (session.getTune().meta.tempoBpm, reggae.tempoBpm, 1.0e-9);
    CHECK_NEAR (session.getTune().meta.swingPercent, reggae.swingPercent, 1.0e-9);
    CHECK_NEAR (session.getTune().getSection (0)->feel, reggae.feel, 1.0e-9);

    // Still on the old kit's tempo: the next kit's comes with it.
    chooseKit ("Metal Chug");
    CHECK_NEAR (session.getTune().meta.tempoBpm, TuneKits::getSuggestion ("Metal Chug").tempoBpm, 1.0e-9);

    // A tempo the player chose stays.
    session.edit (TuneEditClass::other, "tempo", [] (Tune& t) { return t.setTempo (97.0); });
    chooseKit ("Bossa Nova");
    CHECK_NEAR (session.getTune().meta.tempoBpm, 97.0, 1.0e-9);
    CHECK (panel.getKitTempoButton().getButtonText() == "KIT 132");
    CHECK (panel.applyKitTempo());
    CHECK_NEAR (session.getTune().meta.tempoBpm, 132.0, 1.0e-9);

    // The palette, in the key, appends.
    const auto palette = panel.getPaletteChords();
    CHECK (palette.size() == 4);
    CHECK (getChordSymbol (palette[0], false) == "Cmaj7");
    const auto before = session.getTune().getSection (0)->chords.size();
    CHECK (panel.appendPaletteChord (1));
    CHECK (session.getTune().getSection (0)->chords.size() == before + 1);
    CHECK (getChordSymbol (session.getTune().getSection (0)->chords.back(), false) == "Am7");
}

//==============================================================================
/*  gui-integration 17: "New tune  Ctrl+T", rebindable, from anywhere. */
LUTHIER_TEST (TuneIntegration, ctrlTIsInTheShortcutRegistryAndOpensTheTuneTab)
{
    const auto* binding = AccessibilitySettings::get().findShortcut ("newTune");
    CHECK (binding != nullptr);

    if (binding == nullptr)
        return;

    CHECK (binding->key == juce::KeyPress ('t', juce::ModifierKeys::commandModifier, 0));

    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto editor = std::make_unique<LuthierAudioProcessorEditor> (*processor);
    editor->setSize (1400, 840);

    CHECK (editor->keyPressed (binding->key));
    CHECK_MSG (processor->getUiState().advancedMode, "Ctrl+T did not open Advanced mode");
    CHECK (editor->openNewTune() != nullptr);

    // Too narrow for Advanced: it says why rather than doing nothing.
    editor->setSize (940, 560);
    CHECK (editor->openNewTune() == nullptr);
    CHECK (editor->getNotifications().contains ("new-tune"));
}

//==============================================================================
/*  action-and-undo 3.9 / DECISIONS "TUNE in the plugin": the tune keeps its own
    undo stack. A tune edit is not a plugin undo step; the plugin's undo does
    not undo the tune; the tab's Ctrl+Z undoes the tune and not the plugin. */
LUTHIER_TEST (TuneIntegration, theTunesUndoStackIsSeparateFromThePlugins)
{
    auto processor = std::make_unique<LuthierAudioProcessor>();
    auto& session = processor->getTuneSession();
    session.newTune (verseTune());
    TunePanel panel (*processor, processor->getTunePlayer(), session);

    processor->pushUndoState ("A plugin edit");
    const int pluginSteps = processor->getNumUndoSteps();

    panel.getChordPills().performMenuItem (0, TuneChordPills::deleteItem);
    CHECK (session.getNumUndoSteps() == 1);
    CHECK_MSG (processor->getNumUndoSteps() == pluginSteps, "a tune edit became a plugin undo step");

    processor->undo();
    CHECK_MSG (session.getTune().getSection (0)->chords.size() == 3, "the plugin's undo undid the tune");

    processor->pushUndoState ("Another plugin edit");
    const int stepsNow = processor->getNumUndoSteps();

    CHECK (panel.keyPressed (juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)));
    CHECK (session.getTune().getSection (0)->chords.size() == 4);
    CHECK_MSG (processor->getNumUndoSteps() == stepsNow, "the tune's Ctrl+Z undid a plugin step");

    CHECK (panel.keyPressed (juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)));
    CHECK (session.getTune().getSection (0)->chords.size() == 3);
}

//==============================================================================
namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 512;

    /** A tune with chords, melody, bass and a pad: every part a render carries. */
    Tune fullTune()
    {
        auto t = verseTune();
        t.meta.tempoBpm = 120.0;
        t.getSection (0)->lengthBars = 2;
        applyProgressionText (t, 0, "Am F");
        t.setMelodyEnabled (0, true);
        t.addMelodyNote (0, MelodyNote::make (0.0, 2.0, 76, 110));
        t.addMelodyNote (0, MelodyNote::make (4.0, 2.0, 77, 110));
        t.setBassMode (0, BassMode::root);
        return t;
    }

    std::unique_ptr<LuthierAudioProcessor> livePlugin (const Tune& tune)
    {
        auto p = std::make_unique<LuthierAudioProcessor>();
        p->prepareToPlay (kSr, kBlock);

        // Humanise is random per note by design. (Realism detune is left alone:
        // moving it after load changes the live strings without re-rolling what
        // a reload restores - a tuning-state matter outside this workstream.)
        for (const auto* id : { ParamIDs::macroHumanize })
            if (auto* param = p->getState().getParameter (id))
                param->setValueNotifyingHost (0.0f);

        p->getTuneSession().newTune (tune);
        return p;
    }

    /** "Live": the plugin as a host drives it, block by block on its own clock. */
    juce::AudioBuffer<float> playLive (LuthierAudioProcessor& p, double seconds)
    {
        auto& player = p.getTunePlayer();
        player.setLoop (false);
        player.setCountInBars (0);
        player.setMetronome (false);
        p.serviceTune();
        player.play();

        const auto total = (int) std::ceil (seconds * kSr);
        juce::AudioBuffer<float> out (2, total), block (juce::jmax (2, p.getTotalNumOutputChannels()), kBlock);
        out.clear();

        for (int pos = 0; pos < total; pos += kBlock)
        {
            const int n = juce::jmin (kBlock, total - pos);
            p.serviceTune();
            block.setSize (block.getNumChannels(), n, false, false, true);
            block.clear();
            juce::MidiBuffer midi;
            p.processBlock (block, midi);

            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, pos, block, ch, 0, n);
        }

        return out;
    }

    double nullDbfs (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        const int n = juce::jmin (a.getNumSamples(), b.getNumSamples());
        double sum = 0.0;

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                const double d = (double) a.getSample (ch, i) - (double) b.getSample (ch, i);
                sum += d * d;
            }

        return juce::Decibels::gainToDecibels (std::sqrt (sum / juce::jmax (1, 2 * n)), -400.0);
    }

    double rmsDbfs (const juce::AudioBuffer<float>& a)
    {
        return juce::Decibels::gainToDecibels ((double) juce::jmax (a.getRMSLevel (0, 0, a.getNumSamples()),
                                                                    a.getRMSLevel (1, 0, a.getNumSamples())), -400.0);
    }
}

/*  15-07, 9.1: "Export audio: offline render matches live render within
    -80 dBFS RMS null." */
LUTHIER_TEST (TuneIntegration, theOfflineRenderNullsAgainstTheLiveOne)
{
    auto live = livePlugin (fullTune());
    const auto state = live->captureStateBlock();
    const double seconds = TuneExport::getTuneSeconds (live->getTuneSession().getTune()) + 1.0;

    TuneExport::Render render;
    CHECK (TuneExport::renderAudio (state, kSr, kBlock, 1.0, false, render));
    CHECK_NEAR (render.seconds, seconds, 1.0 / kSr + 1.0e-9);

    const auto played = playLive (*live, seconds);

    CHECK_MSG (rmsDbfs (played) > -60.0, "the live render was silent");
    const double null = nullDbfs (played, render.main);
    CHECK_MSG (null <= -80.0, "offline against live nulls at only " + juce::String (null, 1) + " dBFS RMS");
}

/*  9.1: the files - the main mix, and with stems every aux bus; the tail. */
LUTHIER_TEST (TuneIntegration, audioExportWritesTheMixAndEveryAuxStem)
{
    auto live = livePlugin (fullTune());
    const auto state = live->captureStateBlock();

    juce::TemporaryFile folderHandle;
    TuneExport::AudioOptions options;
    options.folder = folderHandle.getFile();
    options.baseName = "Stems";
    options.stems = true;
    options.tailSeconds = 0.5;
    options.format = AudioExporter::Format::Flac;
    options.bitDepth = 16;

    juce::String error;
    const auto files = TuneExport::exportAudio (state, options, error);
    CHECK_MSG (error.isEmpty(), error);
    CHECK (files.size() == 1 + TuneExport::kNumAuxStems);

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    if (! files.isEmpty())
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (files[0]));
        CHECK (reader != nullptr);

        if (reader != nullptr)
        {
            const double expected = TuneExport::getTuneSeconds (live->getTuneSession().getTune()) + 0.5;
            CHECK_NEAR ((double) reader->lengthInSamples / reader->sampleRate, expected, 0.01);
            CHECK (reader->numChannels == 2);
        }

        CHECK (files[1].getFileName().contains ("Aux 1 DI"));

        // The DI stem carries the guitar before the amp.
        std::unique_ptr<juce::AudioFormatReader> di (formats.createReaderFor (files[1]));
        CHECK (di != nullptr);

        if (di != nullptr)
        {
            juce::AudioBuffer<float> samples (2, (int) di->lengthInSamples);
            di->read (&samples, 0, samples.getNumSamples(), 0, true, true);
            CHECK_MSG (samples.getMagnitude (0, samples.getNumSamples()) > 1.0e-4f, "the DI stem is silent");
        }
    }

    options.folder.deleteRecursively();
}

//==============================================================================
/*  15-08, 9.2 / C-53: the MIDI goes out through midi-export's profiles, and a
    Luthier-profile export re-imported renders the same audio as the tune's
    performance did - held to midi-export.md 12's -60 dBFS RMS round-trip
    bar, which is what every factory preset is held to. */
LUTHIER_TEST (TuneIntegration, aLuthierProfileMidiExportReimportsToTheSameAudio)
{
    const auto tune = fullTune();
    const auto performance = TuneExport::buildPerformance (tune, kSr, true);
    CHECK (performance.getMessages().size() > 8);
    CHECK (performance.getSectionNames().contains ("Verse"));

    juce::TemporaryFile file (".mid");
    TuneExport::MidiOptions options;
    options.profile.split = MidiTrackSplit::perInstrument;
    juce::String error;
    CHECK_MSG (TuneExport::exportMidi (tune, file.getFile(), options, error), error);

    MidiPerformance back;
    const auto result = MidiProfiles::importFromFile (file.getFile(), back, kSr);
    CHECK_MSG (result.ok, result.error);
    CHECK (result.detectedProfile == MidiProfile::luthier);

    juce::String why;
    CHECK_MSG (performance.isEquivalentTo (back, &why), why);

    // The two performances through the plugin, from the same clean state.
    auto renderOf = [] (const MidiPerformance& perf)
    {
        auto p = livePlugin (Tune());
        const int total = (int) (kSr * 5.0);
        juce::AudioBuffer<float> out (2, total), block (juce::jmax (2, p->getTotalNumOutputChannels()), kBlock);
        size_t cursor = 0;

        for (int pos = 0; pos < total; pos += kBlock)
        {
            const int n = juce::jmin (kBlock, total - pos);
            block.setSize (block.getNumChannels(), n, false, false, true);
            block.clear();
            juce::MidiBuffer midi;
            perf.renderBlock (midi, pos, n, cursor);
            p->processBlock (block, midi);

            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, pos, block, ch, 0, n);
        }

        return out;
    };

    const auto original = renderOf (performance);
    const auto reimported = renderOf (back);
    CHECK (rmsDbfs (original) > -60.0);
    const double null = nullDbfs (original, reimported);
    CHECK_MSG (null <= -60.0, "the re-imported MIDI nulls at only " + juce::String (null, 1) + " dBFS RMS");

    // Generic, plain: still readable, and it carries the notes.
    options.profile.profile = MidiProfile::generic;
    options.includeRealism = false;
    CHECK (TuneExport::exportMidi (tune, file.getFile(), options, error));
    MidiPerformance generic;
    CHECK (MidiProfiles::importFromFile (file.getFile(), generic, kSr).ok);
    CHECK (generic.getMessages().size() > 8);
}

//==============================================================================
/*  9.3 and 9.4: notation with its sections and chords; the project with its
    preset and guitar bundled, and still an ordinary tune to an older reader. */
LUTHIER_TEST (TuneIntegration, notationAndProjectExportKeepSectionsChordsAndTheBundle)
{
    auto tune = fullTune();
    juce::TemporaryFile xml (".musicxml");
    juce::String error;
    CHECK_MSG (TuneExport::exportNotation (tune, NotationFormat::musicXml, xml.getFile(), {}, error), error);
    const auto text = xml.getFile().loadFileAsString();
    CHECK (text.contains ("<harmony") || text.contains ("Am"));

    juce::TemporaryFile tab (".txt");
    CHECK (TuneExport::exportNotation (tune, NotationFormat::asciiTab, tab.getFile(), {}, error));
    CHECK (tab.getFile().getSize() > 0);

    auto processor = std::make_unique<LuthierAudioProcessor>();
    juce::TemporaryFile project (".luthiertune");
    const juce::var preset (processor->getPresetManager().toVar());
    CHECK (TuneExport::exportProject (tune, project.getFile(), true, preset, juce::var ("guitar-state"), error));

    Tune back;
    CHECK (TuneFile::load (project.getFile(), back).ok());
    CHECK (TuneExport::getBundledGuitar (back) == juce::var ("guitar-state"));
    CHECK (TuneExport::getBundledPreset (back).isObject());
    CHECK (back.arrangement == tune.arrangement);

    CHECK (TuneExport::exportProject (tune, project.getFile(), false, {}, {}, error));
    CHECK (TuneFile::load (project.getFile(), back).ok());
    CHECK (TuneExport::getBundledPreset (back).isVoid());
}

//==============================================================================
/*  2.6 / 9: "the export dialog is one screen" with four destinations. */
LUTHIER_TEST (TuneIntegration, theExportDialogWritesEachDestinationFromOneScreen)
{
    auto live = livePlugin (fullTune());
    TuneExportDialog dialog (*live);
    dialog.setSize (560, 360);

    juce::TemporaryFile folderHandle;
    const auto folder = folderHandle.getFile();
    folder.createDirectory();
    dialog.setFolder (folder);
    dialog.getNameEditor().setText ("Dialog", false);

    // Only the chosen destination's settings are on show.
    CHECK (dialog.getFormatBox().isVisible() && ! dialog.getProfileBox().isVisible());
    dialog.getDestinationButton (TuneExportDialog::Destination::midi).onClick();
    CHECK (dialog.getDestination() == TuneExportDialog::Destination::midi);
    CHECK (dialog.getProfileBox().isVisible() && ! dialog.getFormatBox().isVisible());

    juce::String message;

    // MIDI, Generic, per section.
    dialog.getProfileBox().setSelectedId (2, juce::dontSendNotification);
    dialog.getSplitBox().setSelectedId (2, juce::dontSendNotification);
    CHECK (dialog.getMidiOptions().profile.profile == MidiProfile::generic);
    CHECK (dialog.getMidiOptions().profile.split == MidiTrackSplit::perSection);
    auto files = dialog.exportNow (message);
    CHECK_MSG (files.size() == 1 && files[0].getFileName() == "Dialog.mid" && files[0].getSize() > 0, message);

    // Notation, ASCII tab.
    dialog.setDestination (TuneExportDialog::Destination::notation);
    dialog.getNotationBox().setSelectedId (3, juce::dontSendNotification);
    files = dialog.exportNow (message);
    CHECK_MSG (files.size() == 1 && files[0].getSize() > 0, message);

    // Project, bundled.
    dialog.setDestination (TuneExportDialog::Destination::project);
    dialog.getBundleToggle().setToggleState (true, juce::dontSendNotification);
    files = dialog.exportNow (message);
    CHECK_MSG (files.size() == 1 && files[0].hasFileExtension ("luthiertune"), message);

    Tune back;
    CHECK (files.size() == 1 && TuneFile::load (files[0], back).ok());
    CHECK (TuneExport::getBundledGuitar (back).isObject());

    // Audio: 16-bit WAV at 44.1 kHz, no stems.
    dialog.setDestination (TuneExportDialog::Destination::audio);
    dialog.getBitDepthBox().setSelectedId (16, juce::dontSendNotification);
    dialog.getSampleRateBox().setSelectedId (2, juce::dontSendNotification);
    dialog.getTailSlider().setValue (0.0, juce::dontSendNotification);
    CHECK_NEAR (dialog.getAudioOptions().sampleRate, 44100.0, 1.0e-9);
    files = dialog.exportNow (message);
    CHECK_MSG (files.size() == 1 && files[0].getFileName() == "Dialog.wav", message);

    folder.deleteRecursively();
}

//==============================================================================
namespace
{
    /** Two sections, the second marked as a state boundary (8). */
    Tune twoSectionTune (bool boundary)
    {
        auto t = verseTune();
        t.getSection (0)->lengthBars = 1;
        applyProgressionText (t, 0, "C");
        TuneSection chorus = *t.getSection (0);
        chorus.name = "Chorus";
        chorus.stateBoundary = boundary;
        t.addSection (chorus);
        return t;
    }

    void runFor (LuthierAudioProcessor& p, double seconds)
    {
        juce::AudioBuffer<float> block (juce::jmax (2, p.getTotalNumOutputChannels()), kBlock);

        for (int pos = 0; pos < (int) (seconds * kSr); pos += kBlock)
        {
            p.serviceTune();
            block.clear();
            juce::MidiBuffer midi;
            p.processBlock (block, midi);
            // The processor's timer: a snapshot recall's crossfade runs on the
            // message thread since the recall race fix (live-performance 1).
            p.getSnapshots().advancePending();
        }
    }
}

/*  8, at the processor: a section marked as a state boundary resets the rhythm
    engine and the mod envelopes where it starts, and one that is not does not. */
LUTHIER_TEST (TuneIntegration, aStateBoundarySectionResetsAtItsStart)
{
    for (bool boundary : { true, false })
    {
        auto p = livePlugin (twoSectionTune (boundary));
        p->getTunePlayer().setLoop (false);
        p->getTunePlayer().play();
        runFor (*p, 4.5);   // 2 bars at 120 = 4 s

        CHECK_MSG (p->getNumTuneStateBoundaries() == (boundary ? 1 : 0),
                   juce::String (boundary ? "with" : "without") + " the flag: "
                     + juce::String (p->getNumTuneStateBoundaries()) + " resets");
    }
}

//==============================================================================
/*  14: mod routes (and automation) move section parameters over the timeline:
    Tune Tempo Drift speeds the tune's own clock, Tune Feel moves the feel. */
LUTHIER_TEST (TuneIntegration, theTunesTimelineParametersDriftTempoAndFeel)
{
    auto p = livePlugin (twoSectionTune (false));
    auto* drift = p->getState().getParameter (ParamIDs::tuneTempoDrift);
    auto* feel = p->getState().getParameter (ParamIDs::tuneFeelMod);
    CHECK (drift != nullptr && feel != nullptr);

    if (drift == nullptr || feel == nullptr)
        return;

    // Defaults leave every tune as written.
    CHECK_NEAR (p->tuneModValue (ParamIDs::tuneTempoDrift), 0.0, 1.0e-6);
    CHECK_NEAR (p->tuneModValue (ParamIDs::tuneFeelMod), 0.0, 1.0e-6);
    CHECK (! PresetManager::isRandomisable (ParamIDs::tuneFeelMod));

    drift->setValueNotifyingHost (drift->convertTo0to1 (10.0f));
    p->getTunePlayer().setLoop (true);
    p->getTunePlayer().play();
    runFor (*p, 1.0);

    // 120 bpm + 10 %: 2.2 beats a second.
    CHECK_NEAR (p->getTunePlayer().getTempoScale(), 1.1, 1.0e-4);
    CHECK_NEAR (p->getTunePlayer().getPositionPpq(), 2.2, 0.05);

    // Feel: the rhythm engine's humanise moves with the parameter, mid-section.
    const double before = p->getEngine().getRhythmEngine().getHumanise().amount;
    feel->setValueNotifyingHost (feel->convertTo0to1 (0.5f));
    runFor (*p, 0.1);
    CHECK_NEAR (p->getTuneSession().getFeelOffset(), 0.5, 1.0e-4);
    CHECK_MSG (p->getEngine().getRhythmEngine().getHumanise().amount > before,
               "Tune Feel did not loosen the rhythm");
}

//==============================================================================
/*  14: "live-performance.md snapshots capture the current section state, so a
    live rig can switch sections with a footswitch." */
LUTHIER_TEST (TuneIntegration, aSnapshotRecallsTheTunesSection)
{
    auto p = livePlugin (twoSectionTune (false));
    auto& session = p->getTuneSession();

    session.setSelectedSection (1);
    CHECK (p->captureSnapshot (0, "Chorus", 1));
    session.setSelectedSection (0);
    CHECK (p->captureSnapshot (1, "Verse", 2));

    // Stopped: recall selects the section.
    CHECK (p->recallSnapshot (0));
    runFor (*p, 0.2);
    CHECK (session.getSelectedSection() == 1);

    // Playing: recall jumps there.
    p->getTunePlayer().play();
    runFor (*p, 0.2);
    CHECK (p->recallSnapshot (1));
    runFor (*p, 0.2);
    CHECK (session.getSelectedSection() == 0);
    CHECK (p->getTunePlayer().getPlayingSection() == 0);

    CHECK (p->recallSnapshot (0));
    runFor (*p, 0.2);
    CHECK_MSG (p->getTunePlayer().getPlayingSection() == 1, "the snapshot did not switch the playing section");
}

//==============================================================================
/*  14: "practice-tools.md looper can capture a whole Tune render into a loop
    layer for practising over." */
LUTHIER_TEST (TuneIntegration, theLooperCapturesAWholeTuneRender)
{
    auto p = livePlugin (twoSectionTune (false));
    TunePanel panel (*p, p->getTunePlayer(), p->getTuneSession());

    const int samples = panel.sendToLooper (true);
    const int expected = (int) std::llround (TuneExport::getTuneSeconds (p->getTuneSession().getTune()) * kSr);

    CHECK_MSG (std::abs (samples - expected) <= kBlock, "imported " + juce::String (samples) + " of " + juce::String (expected));
    CHECK (p->getLooper().getLayer (0).hasContent());
    CHECK (std::abs (p->getLooper().getLoopLengthSamples() - samples) == 0);
    CHECK (p->getLooper().getLayer (0).getAudio().getMagnitude (0, samples) > 1.0e-4f);

    // A second one goes to the next layer.
    CHECK (panel.sendToLooper (true) > 0);
    CHECK (p->getLooper().getLayer (1).hasContent());
}

//==============================================================================
/*  15-10: "launch standalone, save a tune, close, relaunch, the last tune loads
    and plays." The standalone keeps the plugin's state between launches, so a
    relaunch is a new processor given the old one's state. */
LUTHIER_TEST (TuneIntegration, aRelaunchLoadsTheLastTuneAndPlaysIt)
{
    juce::TemporaryFile file (".luthiertune");
    juce::MemoryBlock state;

    {
        auto first = livePlugin (fullTune());
        juce::String error;
        CHECK_MSG (first->getTuneSession().saveAs (file.getFile(), error), error);
        first->getStateInformation (state);
    }

    auto relaunched = std::make_unique<LuthierAudioProcessor>();
    relaunched->prepareToPlay (kSr, kBlock);
    relaunched->setStateInformation (state.getData(), (int) state.getSize());

    auto& session = relaunched->getTuneSession();
    CHECK (session.getTune().meta.title == "Integration");
    CHECK (session.getFile() == file.getFile());
    CHECK (! session.isDirty());
    CHECK (session.getTune().getSection (0)->melody.has_value());

    const auto played = playLive (*relaunched, 2.0);
    CHECK_MSG (rmsDbfs (played) > -60.0, "the relaunched tune did not play");
}
