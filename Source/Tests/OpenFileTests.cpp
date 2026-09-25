/*  Opening a file the OS handed the standalone (Source/Standalone/StandaloneApp.cpp):
    the command-line parsing, and the editor's openFile dispatch - each kind of
    file reaches its loader and changes what it should, and anything else is a
    banner rather than a crash. */

#include "TestFramework.h"

#include "../PluginEditor.h"
#include "../PluginProcessor.h"
#include "../Support/OpenFile.h"
#include "../Tune/TuneFile.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    /** A scratch folder of the test's own, removed when it goes. */
    struct ScratchFolder
    {
        juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("luthier-open-file-" + juce::Uuid().toString());

        ScratchFolder()  { folder.createDirectory(); }
        ~ScratchFolder() { folder.deleteRecursively(); }

        juce::File operator[] (const juce::String& name) const { return folder.getChildFile (name); }
    };

    struct Window
    {
        LuthierAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor;
        LuthierAudioProcessorEditor* window = nullptr;

        Window()
        {
            processor.prepareToPlay (kSr, kBlock);
            editor.reset (processor.createEditor());
            window = dynamic_cast<LuthierAudioProcessorEditor*> (editor.get());

            // Wide enough for Advanced Mode, which the TUNE tab lives in.
            if (editor != nullptr)
                editor->setSize (LuthierAudioProcessorEditor::defaultWidth,
                                 LuthierAudioProcessorEditor::defaultHeight);

            if (window != nullptr)
                window->getNotifications().clear();
        }

        ~Window() { editor = nullptr; }

        float plain (const char* id)
        {
            auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id));
            return p != nullptr ? p->convertFrom0to1 (p->getValue()) : -1.0f;
        }

        void setPlain (const char* id, float value)
        {
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.getState().getParameter (id)))
                p->setValueNotifyingHost (p->convertTo0to1 (value));
        }
    };
}

//==============================================================================
LUTHIER_TEST (OpenFile, eachExtensionIsItsKind)
{
    CHECK (classifyOpenFile (juce::File ("/x/Clean.luthierpreset")) == OpenFileKind::preset);
    CHECK (classifyOpenFile (juce::File ("/x/CLEAN.LUTHIERPRESET")) == OpenFileKind::preset);
    CHECK (classifyOpenFile (juce::File ("/x/Tele.luthierguitar"))  == OpenFileKind::guitar);
    CHECK (classifyOpenFile (juce::File ("/x/Song.luthiertune"))    == OpenFileKind::tune);
    CHECK (classifyOpenFile (juce::File ("/x/song.mid"))            == OpenFileKind::midi);
    CHECK (classifyOpenFile (juce::File ("/x/song.MIDI"))           == OpenFileKind::midi);

    // Registered types there is no open for yet, and strangers.
    CHECK (classifyOpenFile (juce::File ("/x/Neck.luthierpart"))    == OpenFileKind::unknown);
    CHECK (classifyOpenFile (juce::File ("/x/Gig.luthierset"))      == OpenFileKind::unknown);
    CHECK (classifyOpenFile (juce::File ("/x/notes.txt"))           == OpenFileKind::unknown);
    CHECK (classifyOpenFile (juce::File ("/x/no-extension"))        == OpenFileKind::unknown);
}

LUTHIER_TEST (OpenFile, aCommandLineNamesItsFiles)
{
    const auto cwd = juce::File::getSpecialLocation (juce::File::tempDirectory);

    // Windows quotes a path with spaces; the OS and debuggers add flags.
    const auto spaced = cwd.getChildFile ("My Presets").getChildFile ("Warm Clean.luthierpreset");
    auto files = filesFromCommandLine ("-NSDocumentRevisionsDebugMode YES "
                                       + spaced.getFullPathName().quoted() + " --verbose",
                                       cwd);

    // "YES" is the flag's value, not a file: it has no extension.
    CHECK (files.size() == 1 && files[0] == spaced);

    // .desktop's %f: one bare path. A relative path is the launch folder's.
    files = filesFromCommandLine ("song.mid", cwd);
    CHECK (files.size() == 1 && files[0] == cwd.getChildFile ("song.mid"));

    // A file:// URI (%u) is its path.
    files = filesFromCommandLine ("file:///tmp/My%20Song.luthiertune", cwd);
    CHECK (files.size() == 1 && files[0] == juce::File ("/tmp/My Song.luthiertune"));

    CHECK (filesFromCommandLine ({}, cwd).isEmpty());
    CHECK (filesFromCommandLine ("   ", cwd).isEmpty());

    // What the Linux hand-off writes, it reads back.
    const juce::Array<juce::File> two { spaced, cwd.getChildFile ("b.luthierguitar") };
    CHECK (filesFromCommandLine (commandLineForFiles (two), cwd) == two);
}

//==============================================================================
LUTHIER_TEST (OpenFile, aPresetBecomesTheCurrentPreset)
{
    Window w;
    CHECK (w.window != nullptr);

    if (w.window == nullptr)
        return;

    ScratchFolder scratch;
    const auto file = scratch["Capo Five.luthierpreset"];

    w.setPlain (ParamIDs::capoFret, 5.0f);
    CHECK_MSG (w.processor.getPresetManager().exportPreset (file), "could not write the test preset");
    w.setPlain (ParamIDs::capoFret, 0.0f);

    CHECK (w.window->openFile (file));
    CHECK (w.processor.getPresetManager().getCurrentPresetFile() == file);
    CHECK_NEAR (w.plain (ParamIDs::capoFret), 5.0, 0.01);
    CHECK (! w.window->getNotifications().contains ("preset-load"));

    // A broken preset: refused, named in the preset-load banner, nothing thrown.
    const auto broken = scratch["Broken.luthierpreset"];
    broken.replaceWithText ("{ this is not json");

    CHECK (! w.window->openFile (broken));
    CHECK (w.window->getNotifications().contains ("preset-load"));
    CHECK_NEAR (w.plain (ParamIDs::capoFret), 5.0, 0.01);
}

LUTHIER_TEST (OpenFile, aGuitarBecomesTheInstrument)
{
    Window w;

    if (w.window == nullptr)
        return;

    const auto factory = PartLibrary::getFactoryGuitarsFolder()
                           .getChildFile (LuthierAudioProcessor::getFactoryGuitarPath (GuitarType::Telecaster));

    CHECK_MSG (factory.existsAsFile(), "no factory guitars next to the runner: " + factory.getFullPathName());

    if (! factory.existsAsFile())
        return;

    // From outside the guitar folders: loaded whole into the override.
    ScratchFolder scratch;
    const auto outside = scratch["Borrowed Tele.luthierguitar"];
    CHECK (factory.copyFileTo (outside));

    w.setPlain (ParamIDs::guitarType, (float) (int) GuitarType::LesPaul);

    CHECK (w.window->openFile (outside));
    CHECK (w.processor.getGuitarReference().isEmpty());
    CHECK (w.processor.isGuitarEdited());
    CHECK (w.processor.hasPartsGuitar());
    // Outside the factory folder it stands for its family's template (electric: the double-cut).
    CHECK_NEAR (w.plain (ParamIDs::guitarType), (int) GuitarType::Stratocaster, 0.01);
    CHECK (! w.window->getNotifications().contains ("open-file"));

    // The factory file itself: referenced, as a preset would reference it.
    CHECK (w.window->openFile (factory));
    CHECK (w.processor.getGuitarReference()
             == "Factory/" + LuthierAudioProcessor::getFactoryGuitarPath (GuitarType::Telecaster));
    CHECK (! w.processor.isGuitarEdited());
    CHECK_NEAR (w.plain (ParamIDs::guitarType), (int) GuitarType::Telecaster, 0.01);

    // Not a guitar: refused with a banner, and the guitar stays.
    const auto fake = scratch["Fake.luthierguitar"];
    fake.replaceWithText ("{ \"magic\": \"luthier.preset\" }");

    const auto before = w.processor.getGuitarReference();
    CHECK (! w.window->openFile (fake));
    CHECK (w.window->getNotifications().contains ("open-file"));
    CHECK (w.processor.getGuitarReference() == before);
}

LUTHIER_TEST (OpenFile, aTuneOpensInTheTuneBuilder)
{
    Window w;

    if (w.window == nullptr)
        return;

    ScratchFolder scratch;
    const auto file = scratch["Opened.luthiertune"];

    Tune tune;
    tune.meta.title = "Opened From Explorer";
    TuneSection verse;
    verse.name = "Verse";
    verse.lengthBars = 2;
    tune.addSection (verse);

    juce::String error;
    CHECK_MSG (TuneFile::save (tune, file, error, false), error);

    CHECK (w.window->openFile (file));
    CHECK (w.processor.getTuneSession().getTune().meta.title == "Opened From Explorer");
    CHECK (w.processor.getTuneSession().getFile() == file);

    // A broken tune: a banner, and the open tune stays.
    const auto broken = scratch["Broken.luthiertune"];
    broken.replaceWithText ("[1, 2, 3]");

    CHECK (! w.window->openFile (broken));
    CHECK (w.window->getNotifications().contains ("open-file"));
    CHECK (w.processor.getTuneSession().getTune().meta.title == "Opened From Explorer");
}

LUTHIER_TEST (OpenFile, aMidiFileIsImportedIntoTheTuneBuilder)
{
    Window w;

    if (w.window == nullptr)
        return;

    Tune before;
    before.meta.title = "Before The Import";
    w.processor.getTuneSession().newTune (before);

    constexpr int tpq = 480;
    juce::MidiFile midi;
    midi.setTicksPerQuarterNote (tpq);

    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);

    for (int beat = 0; beat < 16; ++beat)
    {
        const int pitch = 60 + (beat % 4) * 2;
        track.addEvent (juce::MidiMessage::noteOn (1, pitch, (juce::uint8) 100), beat * tpq);
        track.addEvent (juce::MidiMessage::noteOff (1, pitch), beat * tpq + tpq / 2);
    }

    track.updateMatchedPairs();
    midi.addTrack (track);

    ScratchFolder scratch;
    const auto file = scratch["melody.mid"];

    {
        juce::FileOutputStream out (file);
        CHECK (out.openedOk() && midi.writeTo (out, 1));
    }

    CHECK (w.window->openFile (file));
    CHECK (w.processor.getTuneSession().getTune().meta.title != "Before The Import");

    // Not MIDI inside: the import's banner, the tune stays.
    const auto fake = scratch["fake.midi"];
    fake.replaceWithText ("not a midi file");

    const auto title = w.processor.getTuneSession().getTune().meta.title;
    CHECK (! w.window->openFile (fake));
    CHECK (w.window->getNotifications().contains ("tune-import"));
    CHECK (w.processor.getTuneSession().getTune().meta.title == title);
}

LUTHIER_TEST (OpenFile, unknownAndMissingFilesAreABannerNotACrash)
{
    Window w;

    if (w.window == nullptr)
        return;

    ScratchFolder scratch;

    const auto text = scratch["notes.txt"];
    text.replaceWithText ("hello");

    const auto presetBefore = w.processor.getPresetManager().getCurrentPresetFile();

    CHECK (! w.window->openFile (text));
    CHECK (w.window->getNotifications().contains ("open-file"));
    CHECK (w.window->getNotifications().getCurrentMessage().contains ("notes.txt"));

    w.window->getNotifications().clear();

    for (const auto* name : { "gone.luthierpreset", "gone.luthierguitar", "gone.luthiertune", "gone.mid" })
    {
        CHECK (! w.window->openFile (scratch[name]));
        CHECK (w.window->getNotifications().contains ("open-file"));
        w.window->getNotifications().clear();
    }

    // A folder named like a preset is not a preset.
    const auto folder = scratch["Folder.luthierpreset"];
    folder.createDirectory();
    CHECK (! w.window->openFile (folder));

    // An empty File (a malformed command line) as well.
    CHECK (! w.window->openFile (juce::File()));

    CHECK (w.processor.getPresetManager().getCurrentPresetFile() == presetBefore);
}
