/*  The MIDI OUT workspace tab (gui-integration.md 4.4, midi-export.md 4, 6-8). */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/MidiExportDefaults.h"
#include "../UI/MidiOutPanel.h"
#include "../UI/RoutingPanel.h"
#include "../UI/UiPreferences.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    /*  UiPreferences writes through to the user's real config file; put it back
        as it was, so a test run does not decide the next session's defaults. */
    struct PreservedPreferences
    {
        PreservedPreferences()
            : file (UiPreferences::getConfigFile()),
              existed (file.existsAsFile()),
              contents (existed ? file.loadFileAsString() : juce::String())
        {
            UiPreferences::get().setString (MidiExportDefaults::kPreferenceKey, {});
        }

        ~PreservedPreferences()
        {
            if (existed)
                file.replaceWithText (contents);
            else
                file.deleteFile();

            UiPreferences::get().reset();
            UiPreferences::get().load();
        }

        juce::File file;
        bool existed;
        juce::String contents;
    };

    /** What a click does: flips a toggle (or selects a radio button) and runs
        the same callback. Refuses a disabled control. */
    bool press (juce::Button& button)
    {
        if (! button.isEnabled() || button.onClick == nullptr)
            return false;

        if (button.getClickingTogglesState())
            button.setToggleState (button.getRadioGroupId() != 0 || ! button.getToggleState(),
                                   juce::dontSendNotification);

        button.onClick();
        return true;
    }

    juce::Button* findButton (juce::Component& root, const juce::String& text)
    {
        if (auto* b = dynamic_cast<juce::Button*> (&root))
            if (b->getButtonText() == text)
                return b;

        for (auto* child : root.getChildren())
            if (auto* found = findButton (*child, text))
                return found;

        return nullptr;
    }

    void choose (juce::ComboBox& box, int id)
    {
        box.setSelectedId (id, juce::dontSendNotification);

        if (box.onChange != nullptr)
            box.onChange();
    }

    /** A few notes into the processor's retrospective capture. */
    void playSomething (LuthierAudioProcessor& processor)
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100), 10);
        midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 90), 120);
        processor.getMidiCapture().capture (midi, 0);

        juce::MidiBuffer later;
        later.addEvent (juce::MidiMessage::noteOff (1, 52), 5);
        later.addEvent (juce::MidiMessage::noteOff (1, 57), 9);
        processor.getMidiCapture().capture (later, 24000);
    }
}

//==============================================================================
LUTHIER_TEST (MidiOutPanel, profileEditsAreTheExportDefaults)
{
    PreservedPreferences preserved;
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    MidiOutPanel panel (processor);

    CHECK (panel.getProfileButton (MidiProfile::luthier).getToggleState());
    CHECK_MSG (! panel.getRealismToggle().isEnabled(), "realism text is a Generic option (4.1)");
    CHECK_MSG (panel.getSysExToggle().isEnabled(), "the SysEx copy is a Luthier option (8)");

    CHECK (press (panel.getProfileButton (MidiProfile::generic)));
    CHECK (MidiExportDefaults::load().profile == MidiProfile::generic);
    CHECK (! panel.getProfileButton (MidiProfile::luthier).getToggleState());
    CHECK (panel.getRealismToggle().isEnabled());
    CHECK (! panel.getSysExToggle().isEnabled());
    CHECK_MSG (! panel.getClassToggle (LuthierEventClass::squeak).isEnabled(),
               "the class subset only shapes a Luthier file");

    CHECK (press (panel.getRealismToggle()));
    CHECK (MidiExportDefaults::load().includeRealism);

    CHECK (press (panel.getProfileButton (MidiProfile::luthier)));
    CHECK (press (panel.getClassToggle (LuthierEventClass::squeak)));
    CHECK (! MidiExportDefaults::load().includesClass (LuthierEventClass::squeak));
    CHECK (MidiExportDefaults::load().includesClass (LuthierEventClass::pick));

    choose (panel.getPpqBox(), 480);
    choose (panel.getSplitBox(), 4);
    CHECK (MidiExportDefaults::load().ppq == 480);
    CHECK (MidiExportDefaults::load().split == MidiTrackSplit::perString);

    // A second panel (another window, the next session) starts from them.
    MidiOutPanel again (processor);
    CHECK (again.getPpqBox().getSelectedId() == 480);
    CHECK (! again.getClassToggle (LuthierEventClass::squeak).getToggleState());
    CHECK (again.getRealismToggle().getToggleState());
}

LUTHIER_TEST (MidiOutPanel, liveSwitchesAreTheRoutingPanelsSwitches)
{
    PreservedPreferences preserved;
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    MidiOutPanel panel (processor);

    CHECK_MSG (! panel.getSourceToggle (MidiOutPanel::Source::events).isEnabled(),
               "sources wait for the master switch");

    CHECK (press (panel.getLiveEnable()));
    CHECK (processor.getRouting().getMidiOutConfig().enabled);

    CHECK (press (panel.getSourceToggle (MidiOutPanel::Source::events)));
    CHECK (press (panel.getSourceToggle (MidiOutPanel::Source::tune)));
    choose (panel.getMacroCcBox (2), 21 + 2);

    auto cfg = processor.getRouting().getMidiOutConfig();
    CHECK (cfg.luthierEvents && cfg.tunePlayback && ! cfg.workshopChanges);
    CHECK (cfg.macroCc[2] == 21);

    // The routing panel shows them, and editing there keeps them.
    RoutingPanel routing (processor);
    auto* events = findButton (routing, "EVENTS");
    auto* rhythm = findButton (routing, "RHYTHM");
    CHECK (events != nullptr && events->getToggleState());
    CHECK (rhythm != nullptr && press (*rhythm));

    cfg = processor.getRouting().getMidiOutConfig();
    CHECK_MSG (cfg.rhythmEngine, "the routing panel's switch was not written");
    CHECK_MSG (cfg.luthierEvents && cfg.tunePlayback,
               "the routing panel reset switches it does not own the old way");

    // And back: the tab picks up the routing panel's change.
    panel.refresh();
    CHECK (panel.getSourceToggle (MidiOutPanel::Source::rhythm).getToggleState());

    // Saved with the session.
    const auto state = processor.getRouting().toVar();
    RoutingMatrix restored;
    restored.fromVar (state);
    CHECK (restored.getMidiOutConfig() == cfg);
}

LUTHIER_TEST (MidiOutPanel, exportWritesTheCaptureInTheChosenProfile)
{
    PreservedPreferences preserved;
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    MidiOutPanel panel (processor);
    const auto folder = juce::File::createTempFile ("midiout");
    folder.createDirectory();
    const auto file = folder.getChildFile ("take.mid");

    juce::String error;
    CHECK_MSG (! panel.exportTo (file, &error), "an empty capture exported");
    CHECK (error.isNotEmpty());

    playSomething (processor);
    panel.refresh();
    CHECK_MSG (panel.getPreviewText().isNotEmpty(), "no opening-bar preview (4.1)");
    CHECK (panel.getCaptureText().contains ("4 events"));

    CHECK_MSG (panel.exportTo (file, &error), error);

    MidiPerformance read;
    auto result = MidiProfiles::importFromFile (file, read, kSr);
    CHECK_MSG (result.ok, result.error);
    CHECK (result.detectedProfile == MidiProfile::luthier);
    CHECK (read.getMessages().size() == 4);

    CHECK (press (panel.getProfileButton (MidiProfile::generic)));
    CHECK_MSG (panel.exportTo (file, &error), error);
    result = MidiProfiles::importFromFile (file, read, kSr);
    CHECK (result.ok && result.detectedProfile == MidiProfile::generic);

    folder.deleteRecursively();

    // For a person to look at.
    LuthierLookAndFeel lookAndFeel;
    panel.setLookAndFeel (&lookAndFeel);
    panel.setSize (360, panel.getPreferredHeight());
    panel.refresh();

    juce::Image image (juce::Image::ARGB, panel.getWidth(), panel.getHeight(), true, juce::SoftwareImageType());
    {
        juce::Graphics g (image);
        g.fillAll (Palette::panel);
        panel.paintEntireComponent (g, true);
    }

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("luthier-guitar-renders");
    dir.createDirectory();
    const auto png = dir.getChildFile ("_midi_out.png");
    png.deleteFile();
    {
        juce::FileOutputStream out (png);
        juce::PNGImageFormat().writeImageToStream (image, out);
    }

    panel.setLookAndFeel (nullptr);
}

LUTHIER_TEST (MidiOutPanel, theTabSitsInTheFixedOrderAndIsRememberedByName)
{
    PreservedPreferences preserved;
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    {
        AdvancedPanel panel (processor);
        panel.setSize (1600, 900);

        int midiOut = -1, character = -1, controllers = -1;

        for (int i = 0; i < panel.getNumWorkspaceTabs(); ++i)
        {
            const auto name = panel.getWorkspaceTabName (i);
            if (name == "MIDI OUT")    midiOut = i;
            if (name == "CHARACTER")   character = i;
            if (name == "CONTROLLERS") controllers = i;
        }

        CHECK_MSG (midiOut > character && midiOut < controllers,
                   "MIDI OUT belongs between CHARACTER and CONTROLLERS (gui-integration 4.4)");

        auto* tab = findButton (panel, "MIDI OUT");
        CHECK (tab != nullptr && press (*tab));
        CHECK (dynamic_cast<MidiOutPanel*> (panel.getWorkspacePanel (panel.getWorkspaceTab())) != nullptr);
    }

    CHECK (UiPreferences::get().getString (AdvancedPanel::workspaceTabNamePreferenceKey, {}) == "MIDI OUT");

    // A stale index (from a build with a different strip) loses to the name.
    UiPreferences::get().setInt (AdvancedPanel::workspaceTabPreferenceKey, 1);

    AdvancedPanel reopened (processor);
    CHECK (reopened.getWorkspaceTabName (reopened.getWorkspaceTab()) == "MIDI OUT");
}

//==============================================================================
/*  midi-export.md 6: the EVENTS and WORKSHOP sources really send, as Luthier
    SysEx another instance can read, on the sample they happened. */
LUTHIER_TEST (MidiOutPanel, liveEventsAndWorkshopChangesGoOutAsLuthierSysEx)
{
    LuthierAudioProcessor processor;
    processor.prepareToPlay (kSr, kBlock);

    if (auto* p = processor.getState().getParameter (ParamIDs::pickNoise))
        p->setValueNotifyingHost (1.0f);

    // Humanised timing moves a note by up to a few ms; this is about where the
    // event goes relative to the note, so the note stays where it was played.
    if (auto* p = processor.getState().getParameter (ParamIDs::macroHumanize))
        p->setValueNotifyingHost (0.0f);

    auto run = [&processor] (juce::MidiBuffer midi)
    {
        juce::AudioBuffer<float> audio (2, kBlock);
        juce::Array<LuthierEvent> found;

        for (int block = 0; block < 8; ++block)
        {
            audio.clear();
            processor.processBlock (audio, midi);

            for (const auto metadata : midi)
            {
                const auto message = metadata.getMessage();

                if (! message.isSysEx())
                    continue;

                LuthierEvent event;
                juce::int64 correction = 0;
                juce::String error;

                if (LuthierEvents::decodeSysEx (message.getSysExData(), message.getSysExDataSize(), event, correction, error))
                {
                    event.sample = block * kBlock + metadata.samplePosition;
                    found.add (event);
                }
            }

            midi.clear();
        }

        return found;
    };

    juce::MidiBuffer note;
    note.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 110), 37);

    // Off: nothing Luthier goes out.
    auto cfg = processor.getRouting().getMidiOutConfig();
    cfg.enabled = true;
    processor.getRouting().setMidiOutConfig (cfg);
    CHECK_MSG (run (note).isEmpty(), "SysEx went out with the EVENTS source off");

    cfg.luthierEvents = true;
    processor.getRouting().setMidiOutConfig (cfg);

    juce::MidiBuffer again;
    again.addEvent (juce::MidiMessage::noteOn (1, 55, (juce::uint8) 110), 37);
    const auto events = run (again);

    const LuthierEvent* pick = nullptr;

    for (const auto& e : events)
        if (e.eventClass == LuthierEventClass::pick)
            pick = &e;

    CHECK_MSG (pick != nullptr, "no PICK event for a picked note");

    juce::String seen;

    for (const auto& e : events)
        seen << e.className << "@" << e.sample << " ";

    /*  A Poly-mode note sounds one chord window after it was played - the
        latency the interpreter reports and the host compensates - so the PICK
        event goes out with the note as it sounds, in that note's block. */
    const int window = processor.getEngine().getMidiInterpreter().getLatencySamples();

    if (pick != nullptr)
        CHECK_MSG (pick->sample >= 37 + window && pick->sample < 37 + window + 256,
                   "the PICK event went out at sample " + juce::String (pick->sample) + ", not with its note: " + seen);

    // Workshop: a fitted part goes out on the next block, only with its source on.
    processor.postWorkshopChange ("bridge", "Vintage Adjustable Bridge", "Wraparound");
    CHECK (run ({}).isEmpty());

    cfg.workshopChanges = true;
    processor.getRouting().setMidiOutConfig (cfg);
    processor.postWorkshopChange ("bridge", "Vintage Adjustable Bridge", "Wraparound");
    const auto workshop = run ({});

    CHECK (workshop.size() == 1);

    if (workshop.size() == 1)
    {
        CHECK (workshop[0].eventClass == LuthierEventClass::workshop);
        CHECK (workshop[0].get ("slot") == "bridge");
        CHECK (workshop[0].get ("fit") == "Vintage Adjustable Bridge");
    }
}
