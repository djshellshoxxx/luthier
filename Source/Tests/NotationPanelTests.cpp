/*  The capture wired into the plugin, and the NOTATION tab (notation-export.md
    3-6, gui-integration.md 4.4). */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/NotationPanel.h"
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
            button.setToggleState (true, juce::dontSendNotification);

        button.onClick();
        return true;
    }

    void choose (juce::ComboBox& box, int id)
    {
        box.setSelectedId (id, juce::dontSendNotification);

        if (box.onChange != nullptr)
            box.onChange();
    }

    /** Plays a short phrase through the whole plugin, then drains the capture. */
    void playPhrase (LuthierAudioProcessor& processor)
    {
        if (auto* p = processor.getState().getParameter (ParamIDs::macroHumanize))
            p->setValueNotifyingHost (0.0f);

        processor.prepareToPlay (kSr, kBlock);

        const int notes[] = { 52, 55, 57, 59 };
        juce::AudioBuffer<float> buffer (juce::jmax (processor.getTotalNumOutputChannels(), 2), kBlock);

        for (int b = 0; b < 200; ++b)
        {
            buffer.clear();
            juce::MidiBuffer midi;

            if (b % 40 == 0 && b / 40 < 4)
                midi.addEvent (juce::MidiMessage::noteOn (1, notes[b / 40], (juce::uint8) 100), 10);

            if (b % 40 == 30 && b / 40 < 4)
                midi.addEvent (juce::MidiMessage::noteOff (1, notes[b / 40]), 0);

            processor.processBlock (buffer, midi);
        }

        processor.drainPerformanceCapture();
    }
}

//==============================================================================
LUTHIER_TEST (NotationTab, thePluginCapturesWhatTheEnginePlayed)
{
    LuthierAudioProcessor processor;
    CHECK (processor.getPerformanceCapture().getState() == CaptureState::rolling);   // 6.3's default

    playPhrase (processor);

    const auto& notes = processor.getPerformanceCapture().getNotes();
    CHECK_MSG (notes.size() == 4, "captured " + juce::String ((int) notes.size()) + " notes, not 4");

    if (notes.size() == 4)
    {
        CHECK (notes[0].midiNote == 52 && notes[3].midiNote == 59);

        // 6.1: the string and fret the voicer chose, which sound the note.
        for (const auto& n : notes)
            CHECK (n.stringIndex >= 0 && n.stringIndex < processor.getEngine().getNumStrings() && n.fret >= 0.0);
    }
}

LUTHIER_TEST (NotationTab, stateButtonsLiveTabAndPreview)
{
    LuthierAudioProcessor processor;
    NotationPanel panel (processor);

    CHECK (press (panel.getStateButton (CaptureState::off)));
    CHECK (processor.getPerformanceCapture().getState() == CaptureState::off);
    CHECK (press (panel.getStateButton (CaptureState::rolling)));
    CHECK (processor.getPerformanceCapture().getState() == CaptureState::rolling);

    playPhrase (processor);
    panel.refresh();

    CHECK_MSG (panel.getStatusText().startsWith ("4 notes"), "status: " + panel.getStatusText());
    CHECK_MSG (panel.getLiveTabText().contains ("-"), "the live tab is empty:\n" + panel.getLiveTabText());

    choose (panel.getFormatBox(), 1 + (int) NotationFormat::asciiTab);
    CHECK_MSG (panel.getPreviewText().isNotEmpty() && ! panel.getPreviewText().startsWith ("Play something"),
               "no ASCII preview");

    choose (panel.getFormatBox(), 1 + (int) NotationFormat::musicXml);
    CHECK_MSG (panel.getPreviewText().contains ("<measure"), "the MusicXML preview is not a measure");

    // Armed clears the take and waits for the next note.
    CHECK (press (panel.getStateButton (CaptureState::armed)));
    processor.drainPerformanceCapture();
    CHECK (processor.getPerformanceCapture().getNotes().empty());
}

/*  Task X: the STAFF NOTATION toggle is MusicXML's own option (like the tab
    lane's line width and chord diagrams), and it actually changes what gets
    exported. */
LUTHIER_TEST (NotationTab, staffNotationToggleIsMusicXmlOnlyAndTakesEffect)
{
    LuthierAudioProcessor processor;
    NotationPanel panel (processor);

    playPhrase (processor);
    panel.refresh();

    choose (panel.getFormatBox(), 1 + (int) NotationFormat::asciiTab);
    CHECK_MSG (! panel.isStaffNotationShown(), "shown for a format with no staff to choose");

    choose (panel.getFormatBox(), 1 + (int) NotationFormat::musicXml);
    CHECK_MSG (panel.isStaffNotationShown(), "hidden for MusicXML, where it belongs");

    const auto folder = juce::File::createTempFile ("notationStaff");
    folder.createDirectory();

    panel.getStaffNotationButton().setToggleState (false, juce::dontSendNotification);
    const auto tabFile = folder.getChildFile ("tab.musicxml");
    juce::String error;
    CHECK_MSG (panel.exportTo (tabFile, &error), error);
    CHECK (tabFile.loadFileAsString().contains ("<sign>TAB</sign>"));

    panel.getStaffNotationButton().setToggleState (true, juce::dontSendNotification);
    const auto staffFile = folder.getChildFile ("staff.musicxml");
    CHECK_MSG (panel.exportTo (staffFile, &error), error);
    const auto staffXml = staffFile.loadFileAsString();
    CHECK (staffXml.contains ("<sign>G</sign>"));
    CHECK (! staffXml.contains ("<sign>TAB</sign>"));

    folder.deleteRecursively();
}

LUTHIER_TEST (NotationTab, exportsEveryFormat)
{
    LuthierAudioProcessor processor;
    NotationPanel panel (processor);

    const auto folder = juce::File::createTempFile ("notation");
    folder.createDirectory();

    juce::String error;
    CHECK_MSG (! panel.exportTo (folder.getChildFile ("empty.musicxml"), &error), "an empty take exported");

    playPhrase (processor);
    panel.refresh();

    for (int f = 0; f < (int) NotationFormat::numFormats; ++f)
    {
        const auto format = (NotationFormat) f;
        choose (panel.getFormatBox(), f + 1);

        const auto file = folder.getChildFile (juce::String ("take") + getNotationFormatExtension (format));
        CHECK_MSG (panel.exportTo (file, &error), juce::String (getNotationFormatName (format)) + ": " + error);
        CHECK_MSG (file.getSize() > 0, juce::String (getNotationFormatName (format)) + " wrote nothing");
        CHECK (NotationTakeExport::formatForFile (file) == format);
    }

    // MIDI goes out in the MIDI OUT profile and reads back.
    MidiPerformance read;
    const auto result = MidiProfiles::importFromFile (folder.getChildFile ("take.mid"), read, kSr);
    CHECK_MSG (result.ok, result.error);

    folder.deleteRecursively();
}
