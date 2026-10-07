/*  Jam mode's UI (jam-mode.md 17: JM-47 to JM-51): the JAM tab, the Easy
    strip's group, the Live pill, the shortcuts, the 8.4 messages and the
    screen-reader text, and the edition table. */

#include "TestFramework.h"

#include "../PluginProcessor.h"
#include "../UI/AdvancedPanel.h"
#include "../UI/EasyPanel.h"
#include "../UI/HelpContent.h"
#include "../UI/JamPanel.h"
#include "../UI/LiveStrip.h"
#include "../UI/MidiOutPanel.h"
#include "../UI/TuneLayersStrip.h"
#include "../UI/UiPreferences.h"
#include "../Jam/JamEdition.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int kBlock = 256;

    /** UiPreferences writes through to the user's config file; put it back. */
    struct PreservedPreferences
    {
        PreservedPreferences()
            : file (UiPreferences::getConfigFile()), existed (file.existsAsFile()),
              contents (existed ? file.loadFileAsString() : juce::String()) {}

        ~PreservedPreferences()
        {
            if (existed) file.replaceWithText (contents);
            else         file.deleteFile();

            UiPreferences::get().reset();
            UiPreferences::get().load();
        }

        juce::File file;
        bool existed;
        juce::String contents;
    };

    std::unique_ptr<LuthierAudioProcessor> makeProcessor()
    {
        auto p = std::make_unique<LuthierAudioProcessor>();
        p->prepareToPlay (kSr, kBlock);
        return p;
    }

    void set (LuthierAudioProcessor& p, const char* id, float plain)
    {
        if (auto* prm = p.getState().getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
    }

    float get (LuthierAudioProcessor& p, const char* id)
    {
        auto* prm = p.getState().getParameter (id);
        return prm != nullptr ? prm->convertFrom0to1 (prm->getValue()) : 0.0f;
    }

    void runBlocks (LuthierAudioProcessor& p, int blocks)
    {
        juce::AudioBuffer<float> buffer (juce::jmax (2, p.getTotalNumOutputChannels()), kBlock);

        for (int i = 0; i < blocks; ++i)
        {
            buffer.clear();
            juce::MidiBuffer midi;
            p.processBlock (buffer, midi);
        }
    }

    bool isInside (juce::Component& c, juce::Component& root)
    {
        return root.getLocalBounds().contains (root.getLocalArea (c.getParentComponent(), c.getBounds()));
    }
}

//==============================================================================
LUTHIER_TEST (JamPanel, JM47_theTabSitsBetweenTuneAndLiveAndLaysOutAt480To1600)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor();

    {
        AdvancedPanel panel (*processor);
        panel.setSize (1600, 900);

        int tune = -1, jam = -1, live = -1;

        for (int i = 0; i < panel.getNumWorkspaceTabs(); ++i)
        {
            const auto name = panel.getWorkspaceTabName (i);
            if (name == "TUNE") tune = i;
            if (name == "JAM")  jam = i;
            if (name == "LIVE") live = i;
        }

        // INTEGRATE-2: RIFFS (riff-library 7.1) is also between TUNE and LIVE; the order is TUNE, JAM, RIFFS, LIVE.
        CHECK_MSG (jam == tune + 1 && live > jam, "JAM belongs between TUNE and LIVE");
        CHECK (panel.setWorkspaceTabNamed ("JAM"));
        CHECK (dynamic_cast<JamPanel*> (panel.getWorkspacePanel (panel.getWorkspaceTab())) != nullptr);
    }

    JamPanel panel (*processor);
    panel.setLookAndFeel (nullptr);

    for (int width : { 480, 560, 720, 900, 1200, 1600 })
    {
        panel.setSize (width, panel.getPreferredHeightFor (width));

        // Every control inside the panel and none on top of another.
        std::vector<juce::Component*> shown;

        for (auto* c : panel.getChildren())
            if (c->isVisible() && ! c->getBounds().isEmpty())
                shown.push_back (c);

        for (auto* c : shown)
            CHECK_MSG (isInside (*c, panel), "at " + juce::String (width) + " px a control is clipped: "
                                               + c->getTitle() + " " + c->getBounds().toString());

        int overlaps = 0;

        for (size_t i = 0; i < shown.size(); ++i)
            for (size_t j = i + 1; j < shown.size(); ++j)
                if (shown[i]->getBounds().intersects (shown[j]->getBounds()))
                    ++overlaps;

        CHECK_MSG (overlaps == 0, "at " + juce::String (width) + " px " + juce::String (overlaps) + " controls overlap");

        // 8.1: the lanes keep 96 px at the minimum width.
        CHECK (panel.getLaneView().getHeight() >= 96);

        // It paints.
        juce::Image image (juce::Image::ARGB, width, panel.getHeight(), true);
        juce::Graphics g (image);
        panel.paintEntireComponent (g, true);
    }

    // Every control focusable, and Tab walks them in the sketch's order.
    const auto order = panel.getFocusOrder();
    CHECK (order.size() >= 38);

    for (auto* c : order)
    {
        // editions.md 2.3: Free's user-style loader is Pro-only, shown disabled (a disabled
        // control is not a tab stop), so only the controls that are enabled must be focusable.
        if (c->isEnabled())
            CHECK_MSG (c->getWantsKeyboardFocus(), c->getTitle() + " is not focusable");
        else
            CHECK_MSG (JamEdition::kIsFree, c->getTitle() + " is disabled in a Pro build");
    }

    auto traverser = panel.createKeyboardFocusTraverser();
    const auto walked = traverser->getAllComponents (&panel);
    int last = -1;
    bool inOrder = true;

    for (auto* c : order)
    {
        const auto it = std::find (walked.begin(), walked.end(), c);

        if (it == walked.end())
        {
            // A disabled control (Free's user-style loader) is not a tab stop.
            if (! c->isEnabled())
                continue;

            inOrder = false;
            continue;
        }

        const int index = (int) (it - walked.begin());
        inOrder = inOrder && index > last;
        last = index;
    }

    CHECK_MSG (inOrder, "Tab does not follow the sketch's order");
}

//==============================================================================
LUTHIER_TEST (JamPanel, JM48_easyGroupAndLivePill)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor();

    EasyPanel easy (*processor);
    easy.setSize (1280, 800);
    auto* group = easy.getJamGroup();
    CHECK (group != nullptr);

    if (group == nullptr)
        return;

    CHECK_MSG (group->isVisible() && group->getWidth() >= JamStripGroup::preferredWidth - 60,
               "the Easy strip's JAM group is hidden or squeezed (" + group->getBounds().toString() + ")");
    CHECK (group->getPill().getWidth() == 88 && group->getPill().getHeight() == 32);

    // The style, intensity and Band volume drive their parameters.
    group->getStyleBox().setSelectedItemIndex (2, juce::sendNotificationSync);
    CHECK ((int) get (*processor, ParamIDs::jamStyle) == 2);

    group->getIntensity().setValue (5.0, juce::sendNotificationSync);
    CHECK ((int) get (*processor, ParamIDs::jamIntensity) == 5);
    CHECK (group->getIntensity().valueAtX ((float) group->getIntensity().getWidth() - 1.0f) == 5);
    CHECK (group->getIntensity().valueAtX (0.0f) == 1);

    group->getBandVolume().setValue (-12.0, juce::sendNotificationSync);
    CHECK_NEAR (get (*processor, ParamIDs::jamVolume), -12.0, 0.6);

    // The pill: the first press arms; then it follows ARMED, PLAYING, ENDING.
    auto& pill = group->getPill();
    CHECK (pill.getPillText() == "JAM");
    pill.press();
    CHECK (get (*processor, ParamIDs::jamEnabled) > 0.5f);
    runBlocks (*processor, 4);
    pill.refresh();
    CHECK_MSG (pill.getPillText() == "ARMED", "after arming the pill reads " + pill.getPillText());

    pill.press();   // START: drums first
    runBlocks (*processor, 400);
    pill.refresh();
    CHECK_MSG (pill.getPillText() == "PLAYING", "after START the pill reads " + pill.getPillText());

    pill.press();   // STOP: the ending plays
    runBlocks (*processor, 8);
    pill.refresh();
    CHECK_MSG (pill.getPillText() == "ENDING" || pill.getPillText() == "ARMED",
               "after STOP the pill reads " + pill.getPillText());

    // The Live Strip's pill shows only while jam_enabled is on, at 44 px.
    set (*processor, ParamIDs::jamEnabled, 0.0f);
    runBlocks (*processor, 4);   // the band sees it switched off
    LiveStrip strip (*processor);
    strip.setSize (1280, LiveStrip::preferredHeight);
    strip.refreshJamPill();
    CHECK (! strip.getJamPill().isVisible());

    set (*processor, ParamIDs::jamEnabled, 1.0f);
    runBlocks (*processor, 4);
    strip.refreshJamPill();
    CHECK (strip.getJamPill().isVisible());
    CHECK (strip.getJamPill().getHeight() >= LiveStrip::kTouchTargetHeight);

    // Live: a tap starts, a long press is FILL.
    strip.getJamPill().press (false);
    runBlocks (*processor, 400);
    CHECK_MSG (processor->getJam().isBandRunning(), juce::String ("after a tap the band is ") + getJamStateName (processor->getJam().getState()));
}

//==============================================================================
LUTHIER_TEST (JamPanel, JM49_shortcutsAreRebindableListedAndIgnoredWhileTyping)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor();
    auto& shortcuts = AccessibilitySettings::get();
    shortcuts.resetAllShortcuts();

    using KP = juce::KeyPress;
    CHECK (shortcuts.findShortcut ("jamStartStop") != nullptr && shortcuts.findShortcut ("jamStartStop")->key == KP ('j', 0, 0));
    CHECK (shortcuts.findShortcut ("jamFill") != nullptr && shortcuts.findShortcut ("jamFill")->key == KP ('j', juce::ModifierKeys::shiftModifier, 0));
    CHECK (shortcuts.findShortcut ("jamArm") != nullptr && shortcuts.findShortcut ("jamArm")->key == KP ('j', juce::ModifierKeys::altModifier, 0));

    // In the cheat sheet, under Playing.
    int listed = 0;

    for (const auto& row : HelpContent::getShortcutRows())
        if (row.actionId.startsWith ("jam"))
        {
            ++listed;
            CHECK (row.group == "Playing");
            CHECK (! row.description.startsWith ("accessibility."));
        }

    CHECK (listed == 3);

    // Alt+J arms, J starts.
    CHECK (JamShortcuts::handle (*processor, KP ('j', juce::ModifierKeys::altModifier, 0)));
    CHECK (get (*processor, ParamIDs::jamEnabled) > 0.5f);
    CHECK (JamShortcuts::handle (*processor, KP ('j', 0, 0)));
    runBlocks (*processor, 400);
    CHECK (processor->getJam().isBandRunning());

    // Rebound: J no longer, K does.
    CHECK (shortcuts.rebind ("jamStartStop", KP ('k', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0)));
    CHECK (! JamShortcuts::handle (*processor, KP ('j', 0, 0)));
    CHECK (JamShortcuts::handle (*processor, KP ('k', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0)));
    shortcuts.resetShortcut ("jamStartStop");

    // Nothing while a text field has focus (the focused component passed in:
    // a desktop window cannot take focus under the headless test display).
    {
        juce::TextEditor field;
        set (*processor, ParamIDs::jamEnabled, 0.0f);

        for (const auto& key : { KP ('j', 0, 0), KP ('j', juce::ModifierKeys::shiftModifier, 0), KP ('j', juce::ModifierKeys::altModifier, 0) })
            CHECK (! JamShortcuts::handle (*processor, key, &field));

        CHECK (get (*processor, ParamIDs::jamEnabled) < 0.5f);
    }
}

//==============================================================================
LUTHIER_TEST (JamPanel, JM50_messagesLabelsAndTheLaneDescription)
{
    PreservedPreferences preserved;
    auto processor = makeProcessor();
    JamPanel panel (*processor);
    panel.setSize (900, panel.getPreferredHeightFor (900));

    auto shows = [&panel] (const juce::String& text) { return panel.getMessages().contains (text); };

    JamUiFacts f;
    f.enabled = true;
    f.haveStatus = true;
    f.status.state = JamState::armed;
    f.status.waitingForChord = true;
    panel.showFacts (f);
    CHECK (shows (JamUiText::kArmedNoChord));

    f.chordSource = 2;
    f.tunePlaying = false;
    panel.showFacts (f);
    CHECK (shows (JamUiText::kNoTunePlaying));

    f.separateFallback = true;
    panel.showFacts (f);
    CHECK (shows (JamUiText::kSeparateNeedsAux));

    f.status.genericGroove = true;
    f.status.style = 0;
    f.status.meterNumerator = 7;
    f.status.meterDenominator = 8;
    panel.showFacts (f);
    CHECK (shows ("Generic groove - Rock has no 7/8."));

    f.status.noPlayHead = true;
    panel.showFacts (f);
    CHECK (shows (JamUiText::kNoPlayHead));

    f.status.bassResting = true;
    panel.showFacts (f);
    CHECK (shows (JamUiText::kBassistResting));

    f.styleWarning = "Could not read Broken.luthierjam - playing Funk.";
    panel.showFacts (f);
    CHECK (shows (f.styleWarning));

    f.backingTrackPlaying = true;
    panel.showFacts (f);
    CHECK (shows (JamUiText::kBackingTrackToo));

    // Stale: the status reads "-" and the playhead hides.
    f.haveStatus = false;
    panel.showFacts (f);
    CHECK (panel.getStatusText() == "-");
    CHECK (panel.getLaneView().getPlayheadStep() < 0);

    // The status line and the announcement.
    JamUiFacts playing;
    playing.enabled = true;
    playing.haveStatus = true;
    playing.status.state = JamState::playing;
    playing.status.style = 0;
    playing.status.variation = 1;
    playing.status.baseIntensity = 3;
    playing.status.effectiveIntensity = 4;
    panel.showFacts (playing);
    CHECK_MSG (panel.getStatusText() == "PLAYING . Rock B . 3(+1)", panel.getStatusText());
    CHECK_MSG (panel.getLastAnnouncement() == "Band playing, Rock, intensity 4", panel.getLastAnnouncement());

    // The lane description: kick 1 and 3, snare 2 and 4, hats in 8ths; the bass's notes.
    JamStatus bar;
    bar.stepsInBar = 16;
    bar.stepsPerBeat = 4;
    bar.laneHits[0] = (1u << 0) | (1u << 8);
    bar.laneHits[1] = (1u << 4) | (1u << 12);

    for (int s = 0; s < 16; s += 2)
        bar.laneHits[2] |= 1u << s;

    const int notes[] = { 45, 45, 40, 43 };

    for (int i = 0; i < 4; ++i)
        bar.bassNotes[(size_t) i] = (int8_t) notes[i];

    bar.numBassNotes = 4;
    CHECK_MSG (JamUiText::laneDescription (bar) == "Kick 1 and 3, snare 2 and 4, hats 8ths; bass A A E G",
               JamUiText::laneDescription (bar));

    // Every control has a screen-reader label.
    for (auto* c : panel.getFocusOrder())
    {
        auto* handler = c->getAccessibilityHandler();
        juce::ignoreUnused (handler);
        CHECK_MSG (c->getTitle().isNotEmpty(),
                   "a JAM control has no screen-reader label (" + juce::String (typeid (*c).name()) + ")");
    }

    // The tune's percussion row says it is replaced, and MIDI OUT has the band's switch.
    TuneSession session;
    TuneLayersStrip layers (session, { "Travis" });
    layers.setSize (600, layers.getPreferredHeight());
    CHECK (! layers.isPercussionReplaced());
    layers.setPercussionReplaced (true);
    CHECK (layers.isPercussionReplaced() && layers.getPercussionNote() == "Replaced by Jam drums");

    MidiOutPanel midiOut (*processor);
    midiOut.setSize (600, midiOut.getPreferredHeight());
    auto& jamSwitch = midiOut.getSourceToggle (MidiOutPanel::Source::jam);
    jamSwitch.setToggleState (true, juce::sendNotificationSync);

    if (jamSwitch.onClick)
        jamSwitch.onClick();

    CHECK (processor->getRouting().getMidiOutConfig().jamParts);
    midiOut.getJamChannelBox (true).setSelectedId (12, juce::sendNotificationSync);
    CHECK (processor->getRouting().getMidiOutConfig().jamBassChannel == 12);
}

//==============================================================================
LUTHIER_TEST (JamPanel, JM51_editionTable)
{
    using JamEdition::Item;

    // Free keeps Rock, Pop, Blues Shuffle and Ballad; Studio and Vintage; Finger and Pick; Main.
    const juce::StringArray styles = JamStyleLibrary::getStyleChoiceNames();
    juce::StringArray free;

    for (int i = 0; i < styles.size(); ++i)
        if (JamEdition::isInFree (Item::style, i))
            free.add (styles[i]);

    CHECK_MSG (free.joinIntoString (",") == "Rock,Pop,Blues Shuffle,Ballad", free.joinIntoString (","));
    CHECK (JamEdition::isInFree (Item::kit, 0) && JamEdition::isInFree (Item::kit, 1) && ! JamEdition::isInFree (Item::kit, 2));
    CHECK (JamEdition::isInFree (Item::bassVoice, 1) && JamEdition::isInFree (Item::bassVoice, 2)
           && ! JamEdition::isInFree (Item::bassVoice, 3) && ! JamEdition::isInFree (Item::bassVoice, 4));
    CHECK (JamEdition::isInFree (Item::output, 0) && ! JamEdition::isInFree (Item::output, 1));

    // A Pro preset plays the nearest Free choice, and every mapping lands in Free.
    for (int i = 0; i < styles.size(); ++i)
        CHECK (JamEdition::isInFree (Item::style, JamEdition::nearestFree (Item::style, i)));

    CHECK (styles[JamEdition::nearestFree (Item::style, styles.indexOf ("Jazz Swing"))] == "Blues Shuffle");
    CHECK (JamEdition::nearestFree (Item::kit, 3) == 1);
    CHECK (JamEdition::nearestFree (Item::bassVoice, 4) == 1);

    // Pro locks nothing; Free (editions.md 2.3) locks exactly what is not in its table.
    CHECK (JamEdition::kIsFree == ! edition::isPro);

    for (int i = 0; i < styles.size(); ++i)
        CHECK (JamEdition::isLocked (Item::style, i) == (JamEdition::kIsFree && ! JamEdition::isInFree (Item::style, i)));

    auto processor = makeProcessor();
    set (*processor, ParamIDs::jamStyle, 7.0f);
    // The stored value is what the bridge reads in Pro; Free plays the nearest Free style (editions 5.1).
    CHECK (processor->getParameterBridge().readJam().style == (JamEdition::kIsFree ? JamEdition::nearestFree (Item::style, 7) : 7));
    CHECK (juce::String (JamEdition::upsellText()).contains ("Luthier Pro"));
}
