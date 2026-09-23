#include "MidiOutPanel.h"
#include "MidiExportDefaults.h"
#include "../PluginProcessor.h"
#include "../Presets/PresetManager.h"
#include "../Accessibility/Accessibility.h"

namespace luthier
{

namespace
{
    constexpr int kPpqChoices[] = { 96, 192, 240, 384, 480, 960, 1920, 3840 };

    const char* const kSourceNames[] = { "PASS-THRU", "RHYTHM", "STRINGS", "MACRO CC",
                                         "TUNE", "EVENTS", "WORKSHOP" };

    const char* const kSourceTips[] = {
        "Echoes incoming MIDI unchanged, at its original timestamps.",
        "Sends the rhythm engine's generated events, so a strum can be bounced to MIDI.",
        "Sends a note per string that is actually ringing.",
        "Echoes each macro as a CC on the number assigned below.",
        "Sends the tune builder's playback, each event on its own sample.",
        "Sends character and noise events (squeaks, pick, buzz, clank) as Luthier SysEx. "
        "Other hosts drop SysEx; nothing else in the stream depends on it.",
        "Sends Workshop part changes as Luthier SysEx, for automation lanes."
    };

    bool& sourceFlag (MidiOutConfig& cfg, MidiOutPanel::Source source)
    {
        switch (source)
        {
            case MidiOutPanel::Source::passThrough: return cfg.passThrough;
            case MidiOutPanel::Source::rhythm:      return cfg.rhythmEngine;
            case MidiOutPanel::Source::strings:     return cfg.stringActivity;
            case MidiOutPanel::Source::macroCc:     return cfg.ccBroadcast;
            case MidiOutPanel::Source::tune:        return cfg.tunePlayback;
            case MidiOutPanel::Source::events:      return cfg.luthierEvents;
            case MidiOutPanel::Source::workshop:
            case MidiOutPanel::Source::numSources:  break;
        }

        return cfg.workshopChanges;
    }

    double captureSampleRate (LuthierAudioProcessor& processor)
    {
        const double sr = processor.getSampleRate();
        return sr > 0.0 ? sr : 48000.0;
    }

    constexpr int kClassColumns = 3;
    constexpr int kRowGap = Metrics::gridHalf;
    constexpr int kHeader = 26;
}

//==============================================================================
MidiPerformance MidiTakeExport::capturedPerformance (LuthierAudioProcessor& processor)
{
    auto performance = MidiPerformance::fromCapture (processor.getMidiCapture(), captureSampleRate (processor),
                                                     processor.getHostTempo());
    performance.getMeta().title = "Luthier take";
    performance.getMeta().presetName = processor.getPresetManager().getCurrentPresetName();
    return performance;
}

bool MidiTakeExport::exportCapture (LuthierAudioProcessor& processor, const juce::File& destination,
                                    const MidiExportOptions& options, double lastSeconds, juce::String* error)
{
    const auto performance = capturedPerformance (processor);

    if (performance.getLengthInSamples() <= 0)
    {
        if (error != nullptr)
            *error = "There is nothing in the capture buffer yet. Play something first.";

        return false;
    }

    auto chosen = options;
    chosen.range = lastSeconds > 0.0 ? performance.getLastSecondsRange (lastSeconds)
                                     : juce::Range<juce::int64>();

    return MidiProfiles::exportToFile (performance, chosen, destination, error);
}

//==============================================================================
/*  midi-export 4.2: press, drag out of the window, drop on a DAW or a folder.
    The file is written when the drag starts, not before, so it is always the
    take as it stands at that moment. */
class MidiOutPanel::DragSource : public juce::Component,
                                 public juce::SettableTooltipClient
{
public:
    explicit DragSource (MidiOutPanel& p) : panel (p)
    {
        setTooltip ("Drag the take out of the plugin into your DAW or a folder. "
                    "Luthier profile; hold Alt while dragging for Generic.");
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        AccessibleSetup::configureDescriptive (*this, "Drag MIDI",
                                               "Drag the captured take out as a MIDI file.");
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (1.0f);
        const bool live = isEnabled();

        g.setColour (Palette::panelSunken);
        g.fillRoundedRectangle (area, Metrics::controlCorner);

        const float dashes[] = { 4.0f, 3.0f };
        juce::Path outline;
        outline.addRoundedRectangle (area, Metrics::controlCorner);
        juce::Path dashed;
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
        g.setColour (live ? Palette::accent : Palette::textDisabled);
        g.fillPath (dashed);

        g.setFont (Fonts::ui (11.0f, true));
        g.drawFittedText ("DRAG MIDI", getLocalBounds(), juce::Justification::centred, 1);
    }

    void mouseDown (const juce::MouseEvent&) override { dragStarted = false; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragStarted || e.getDistanceFromDragStart() < 6 || ! isEnabled())
            return;

        dragStarted = true;

        const auto performance = MidiTakeExport::capturedPerformance (panel.processor);

        if (performance.getLengthInSamples() <= 0)
            return;

        auto defaults = panel.options;
        defaults.range = panel.rangeSeconds() > 0.0 ? performance.getLastSecondsRange (panel.rangeSeconds())
                                                    : juce::Range<juce::int64>();

        const auto file = MidiProfiles::writeDragOutFile (performance, e.mods.isAltDown(), defaults);

        if (file.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
    }

private:
    MidiOutPanel& panel;
    bool dragStarted = false;
};

//==============================================================================
MidiOutPanel::MidiOutPanel (LuthierAudioProcessor& p)
    : processor (p)
{
    auto makeToggle = [this] (const juce::String& text, const juce::String& tip, std::function<void()> onClick)
    {
        auto toggle = std::make_unique<LuthierToggle> (text);
        toggle->setTooltip (tip);
        toggle->getButton().setClickingTogglesState (true);
        toggle->getButton().onClick = std::move (onClick);
        AccessibleSetup::configureButton (toggle->getButton(), text, tip);
        addAndMakeVisible (*toggle);
        return toggle;
    };

    // --- export profile -------------------------------------------------------------
    /*  Exclusive by hand: a radio group only reaches buttons with the same
        parent, and each of these sits inside its own LuthierToggle. Clicking
        the lit one leaves it lit. */
    auto chooseProfile = [this] (bool generic)
    {
        luthierProfile->getButton().setToggleState (! generic, juce::dontSendNotification);
        genericProfile->getButton().setToggleState (generic, juce::dontSendNotification);
        writeOptions();
    };

    luthierProfile = makeToggle ("LUTHIER", "Luthier profile: every realism event rides along and "
                                            "comes back exactly on re-import.", [chooseProfile] { chooseProfile (false); });
    genericProfile = makeToggle ("GENERIC", "Generic profile: plain per-string MIDI any instrument can "
                                            "play. The realism is lost by design.", [chooseProfile] { chooseProfile (true); });

    for (const int ppq : kPpqChoices)
        ppqBox.addItem (juce::String (ppq) + " PPQ", ppq);

    ppqBox.setTooltip ("Ticks per quarter note. Luthier files keep the exact sample at any setting.");
    ppqBox.onChange = [this] { writeOptions(); };
    AccessibleSetup::configureComboBox (ppqBox, "PPQ resolution");
    addAndMakeVisible (ppqBox);

    splitBox.addItem ("Single track", 1);
    splitBox.addItem ("Per section", 2);
    splitBox.addItem ("Per instrument", 3);
    splitBox.addItem ("Per string", 4);
    splitBox.setTooltip ("How the export is split into tracks.");
    splitBox.onChange = [this] { writeOptions(); };
    AccessibleSetup::configureComboBox (splitBox, "Track split");
    addAndMakeVisible (splitBox);

    realismToggle = makeToggle ("REALISM TEXT", "Generic only: each realism event as a text marker, "
                                                "for a person to read.", [this] { writeOptions(); });
    sysExToggle = makeToggle ("SYSEX COPY", "Luthier only: a SysEx copy of every realism event. "
                                            "Off makes smaller files.", [this] { writeOptions(); });
    stripIdsToggle = makeToggle ("STRIP IDS", "Leave out the guitar name, preset name and character "
                                              "seed. Generic files never carry them.", [this] { writeOptions(); });

    for (int c = 0; c < LuthierEvents::kNumClasses; ++c)
    {
        const auto name = juce::String (LuthierEvents::getClassName ((LuthierEventClass) c)).replaceCharacter ('_', ' ');
        classToggles.add (makeToggle (name, "Include " + name + " events in Luthier files.",
                                      [this] { writeOptions(); }).release());
    }

    saveProfileButton.setTooltip ("Save these settings as a .midprofile to reuse or share.");
    saveProfileButton.onClick = [this] { saveProfileAs(); };
    AccessibleSetup::configureButton (saveProfileButton, "Save profile");
    addAndMakeVisible (saveProfileButton);

    loadProfileButton.setTooltip ("Load settings from a .midprofile.");
    loadProfileButton.onClick = [this] { loadProfile(); };
    AccessibleSetup::configureButton (loadProfileButton, "Load profile");
    addAndMakeVisible (loadProfileButton);

    // --- export ---------------------------------------------------------------------
    rangeBox.addItem ("Entire capture", 1);
    rangeBox.addItem ("Last N seconds", 2);
    rangeBox.setSelectedId (1, juce::dontSendNotification);
    rangeBox.setTooltip ("How much of the capture to export.");
    rangeBox.onChange = [this] { updateEnablement(); updateCaptureReadout (true); };
    AccessibleSetup::configureComboBox (rangeBox, "Export range");
    addAndMakeVisible (rangeBox);

    secondsSlider.setRange (1.0, 600.0, 1.0);
    secondsSlider.setValue (30.0, juce::dontSendNotification);
    secondsSlider.setTextValueSuffix (" s");
    secondsSlider.setTooltip ("Seconds from the end of the capture.");
    secondsSlider.onValueChange = [this] { updateCaptureReadout (true); };
    AccessibleSetup::configureSlider (secondsSlider, "Last seconds", " s");
    addAndMakeVisible (secondsSlider);

    exportButton.setTooltip ("Write the capture to a MIDI file with the profile above.");
    exportButton.onClick = [this] { exportWithChooser(); };
    AccessibleSetup::configureButton (exportButton, "Export MIDI");
    addAndMakeVisible (exportButton);

    dragSource = std::make_unique<DragSource> (*this);
    addAndMakeVisible (*dragSource);

    // --- live -----------------------------------------------------------------------
    liveEnable = makeToggle ("MIDI OUT", "Master switch. With this off the plugin emits no MIDI at all.",
                             [this] { writeLiveConfig(); });

    for (int s = 0; s < (int) Source::numSources; ++s)
        sourceToggles.add (makeToggle (kSourceNames[s], kSourceTips[s], [this] { writeLiveConfig(); }).release());

    for (int ch = 1; ch <= 16; ++ch)
        liveChannel.addItem ("Ch " + juce::String (ch), ch);

    liveChannel.setTooltip ("MIDI channel for generated events. Pass-through keeps its own channel.");
    liveChannel.onChange = [this] { writeLiveConfig(); };
    AccessibleSetup::configureComboBox (liveChannel, "MIDI out channel");
    addAndMakeVisible (liveChannel);

    for (int m = 0; m < ParamIDs::kNumMacros; ++m)
    {
        const auto name = juce::String (ParamIDs::macroByIndex (m)).fromFirstOccurrenceOf ("_", false, false).toUpperCase();
        macroCcLabels[m].setText (name, juce::dontSendNotification);
        macroCcLabels[m].setFont (Fonts::ui (9.0f));
        macroCcLabels[m].setColour (juce::Label::textColourId, Palette::textMuted);
        addAndMakeVisible (macroCcLabels[m]);

        macroCc[m].addItem ("off", 1);

        for (int cc = 0; cc < 128; ++cc)
            macroCc[m].addItem ("CC " + juce::String (cc), cc + 2);

        macroCc[m].setTooltip ("The CC the " + name + " macro is echoed on when MACRO CC is on.");
        macroCc[m].onChange = [this] { writeLiveConfig(); };
        AccessibleSetup::configureComboBox (macroCc[m], name + " macro CC");
        addAndMakeVisible (macroCc[m]);
    }

    setSize (320, getPreferredHeight());
    refresh();
    startTimerHz (4);
}

MidiOutPanel::~MidiOutPanel()
{
    stopTimer();
}

//==============================================================================
juce::Button& MidiOutPanel::getProfileButton (MidiProfile profile) noexcept
{
    return profile == MidiProfile::generic ? genericProfile->getButton() : luthierProfile->getButton();
}

juce::Button& MidiOutPanel::getClassToggle (LuthierEventClass eventClass) noexcept
{
    return classToggles[juce::jlimit (0, classToggles.size() - 1, (int) eventClass)]->getButton();
}

juce::Button& MidiOutPanel::getSourceToggle (Source source) noexcept
{
    return sourceToggles[juce::jlimit (0, sourceToggles.size() - 1, (int) source)]->getButton();
}

//==============================================================================
void MidiOutPanel::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);

    options = MidiExportDefaults::load();

    luthierProfile->getButton().setToggleState (options.profile == MidiProfile::luthier, juce::dontSendNotification);
    genericProfile->getButton().setToggleState (options.profile == MidiProfile::generic, juce::dontSendNotification);

    int ppqId = MidiExportOptions::kDefaultPpq;

    for (const int ppq : kPpqChoices)
        if (ppq <= options.getPpq())
            ppqId = ppq;

    ppqBox.setSelectedId (ppqId, juce::dontSendNotification);
    splitBox.setSelectedId ((int) options.split + 1, juce::dontSendNotification);
    realismToggle->getButton().setToggleState (options.includeRealism, juce::dontSendNotification);
    sysExToggle->getButton().setToggleState (options.sysExRedundancy, juce::dontSendNotification);
    stripIdsToggle->getButton().setToggleState (options.stripIdentifiers, juce::dontSendNotification);

    for (int c = 0; c < classToggles.size(); ++c)
        classToggles[c]->getButton().setToggleState (options.includesClass ((LuthierEventClass) c),
                                                     juce::dontSendNotification);

    shownLive = processor.getRouting().getMidiOutConfig();
    liveEnable->getButton().setToggleState (shownLive.enabled, juce::dontSendNotification);

    for (int s = 0; s < sourceToggles.size(); ++s)
        sourceToggles[s]->getButton().setToggleState (sourceFlag (shownLive, (Source) s), juce::dontSendNotification);

    liveChannel.setSelectedId (juce::jlimit (1, 16, shownLive.channel), juce::dontSendNotification);

    for (int m = 0; m < ParamIDs::kNumMacros; ++m)
    {
        const int cc = shownLive.macroCc[(size_t) m];
        macroCc[m].setSelectedId (cc < 0 ? 1 : cc + 2, juce::dontSendNotification);
    }

    updateEnablement();
    updateCaptureReadout (true);
}

void MidiOutPanel::writeOptions()
{
    if (updating)
        return;

    options.profile = genericProfile->getButton().getToggleState() ? MidiProfile::generic : MidiProfile::luthier;
    options.ppq = ppqBox.getSelectedId() > 0 ? ppqBox.getSelectedId() : MidiExportOptions::kDefaultPpq;
    options.split = (MidiTrackSplit) juce::jlimit (0, 3, splitBox.getSelectedId() - 1);
    options.includeRealism = realismToggle->getButton().getToggleState();
    options.sysExRedundancy = sysExToggle->getButton().getToggleState();
    options.stripIdentifiers = stripIdsToggle->getButton().getToggleState();

    for (int c = 0; c < classToggles.size(); ++c)
        options.setClassIncluded ((LuthierEventClass) c, classToggles[c]->getButton().getToggleState());

    MidiExportDefaults::save (options);
    updateEnablement();
    updateCaptureReadout (true);
}

void MidiOutPanel::writeLiveConfig()
{
    if (updating)
        return;

    auto cfg = processor.getRouting().getMidiOutConfig();
    cfg.enabled = liveEnable->getButton().getToggleState();

    for (int s = 0; s < sourceToggles.size(); ++s)
        sourceFlag (cfg, (Source) s) = sourceToggles[s]->getButton().getToggleState();

    cfg.channel = juce::jlimit (1, 16, liveChannel.getSelectedId());

    for (int m = 0; m < ParamIDs::kNumMacros; ++m)
    {
        const int id = macroCc[m].getSelectedId();
        cfg.macroCc[(size_t) m] = (id <= 1) ? -1 : id - 2;
    }

    processor.getRouting().setMidiOutConfig (cfg);
    shownLive = cfg;
    updateEnablement();
}

void MidiOutPanel::updateEnablement()
{
    const bool generic = options.profile == MidiProfile::generic;

    // 4.1 / 8: realism text is a Generic option, the SysEx copy a Luthier one,
    // and the class subset only shapes a Luthier file.
    realismToggle->setEnabled (generic);
    sysExToggle->setEnabled (! generic);

    for (auto* toggle : classToggles)
        toggle->setEnabled (! generic);

    secondsSlider.setVisible (rangeBox.getSelectedId() == 2);

    const bool live = shownLive.enabled;

    for (auto* toggle : sourceToggles)
        toggle->setEnabled (live);

    liveChannel.setEnabled (live);

    for (auto& box : macroCc)
        box.setEnabled (live);
}

double MidiOutPanel::rangeSeconds() const
{
    return rangeBox.getSelectedId() == 2 ? secondsSlider.getValue() : 0.0;
}

void MidiOutPanel::updateCaptureReadout (bool force)
{
    auto& capture = processor.getMidiCapture();
    const int events = capture.getEventCount();

    if (! force && events == shownEventCount)
        return;

    shownEventCount = events;

    const bool any = events > 0;
    captureText = any ? "Capture: " + juce::String (capture.getCapturedSeconds(), 1) + " s, "
                          + juce::String (events) + " events"
                      : "Capture: empty. Play something and it is kept here.";

    previewText = {};

    if (any)
    {
        const auto performance = MidiTakeExport::capturedPerformance (processor);
        auto chosen = options;
        chosen.range = rangeSeconds() > 0.0 ? performance.getLastSecondsRange (rangeSeconds())
                                            : juce::Range<juce::int64>();
        previewText = MidiProfiles::describeOpeningBar (performance, chosen);
    }

    exportButton.setEnabled (any);
    dragSource->setEnabled (any);
    dragSource->repaint();
    repaint (captureBounds.getUnion (previewBounds));
}

void MidiOutPanel::timerCallback()
{
    // The routing panel edits the same config; show its changes here.
    if (processor.getRouting().getMidiOutConfig() != shownLive)
        refresh();
    else
        updateCaptureReadout (false);
}

//==============================================================================
bool MidiOutPanel::exportTo (const juce::File& destination, juce::String* error)
{
    return MidiTakeExport::exportCapture (processor, destination, options, rangeSeconds(), error);
}

void MidiOutPanel::exportWithChooser()
{
    chooser = std::make_unique<juce::FileChooser> ("Export the take as MIDI",
                                                   PresetManager::getRenderFolder().getChildFile (MidiCapture::makeDefaultFileName()),
                                                   "*.mid;*.midi");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<MidiOutPanel> (this)] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();

        if (safe == nullptr || file == juce::File())
            return;

        juce::String error;
        const bool ok = safe->exportTo (file, &error);

        juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                               .withIconType (ok ? juce::MessageBoxIconType::InfoIcon
                                                                 : juce::MessageBoxIconType::WarningIcon)
                                               .withTitle (ok ? "MIDI exported" : "Could not export")
                                               .withMessage (ok ? "Saved to\n" + file.getFullPathName() : error)
                                               .withButton ("OK"),
                                           nullptr);
    });
}

void MidiOutPanel::saveProfileAs()
{
    chooser = std::make_unique<juce::FileChooser> ("Save MIDI export profile",
                                                   PresetManager::getUserPresetFolder().getChildFile (
                                                       juce::String ("My export") + MidiProfiles::kProfileExtension),
                                                   juce::String ("*") + MidiProfiles::kProfileExtension);

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<MidiOutPanel> (this)] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();

        if (safe != nullptr && file != juce::File())
            MidiProfiles::saveProfile (file.withFileExtension (MidiProfiles::kProfileExtension),
                                       safe->options, file.getFileNameWithoutExtension());
    });
}

void MidiOutPanel::loadProfile()
{
    chooser = std::make_unique<juce::FileChooser> ("Load MIDI export profile",
                                                   PresetManager::getUserPresetFolder(),
                                                   juce::String ("*") + MidiProfiles::kProfileExtension);

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<MidiOutPanel> (this)] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();

        if (safe == nullptr || file == juce::File())
            return;

        MidiExportOptions loaded;
        juce::String name, error;
        juce::StringArray warnings;

        if (MidiProfiles::loadProfile (file, loaded, &name, &error, &warnings))
        {
            MidiExportDefaults::save (loaded);
            safe->refresh();

            if (warnings.isEmpty())
                return;

            error = warnings.joinIntoString ("\n");
        }

        juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                               .withIconType (juce::MessageBoxIconType::WarningIcon)
                                               .withTitle ("MIDI export profile")
                                               .withMessage (error)
                                               .withButton ("OK"),
                                           nullptr);
    });
}

//==============================================================================
int MidiOutPanel::getPreferredHeight() const
{
    const int button = Metrics::buttonHeight;
    const int classRows = (LuthierEvents::kNumClasses + kClassColumns - 1) / kClassColumns;

    return kHeader + 3 * (button + kRowGap) + classRows * (button + kRowGap) + button + Metrics::grid
         + kHeader + 2 * (button + kRowGap) + 32 + 40 + Metrics::grid
         + kHeader + 3 * (button + kRowGap) + 2 * 40 + Metrics::grid;
}

void MidiOutPanel::paint (juce::Graphics& g)
{
    LuthierLookAndFeel::drawSectionHeader (g, profileHeader, "EXPORT PROFILE");
    LuthierLookAndFeel::drawSectionHeader (g, exportHeader, "EXPORT");
    LuthierLookAndFeel::drawSectionHeader (g, liveHeader, "LIVE MIDI OUT");

    g.setFont (Fonts::ui (11.0f));
    g.setColour (Palette::textMuted);
    g.drawFittedText (captureText, captureBounds, juce::Justification::centredLeft, 2);

    g.setFont (Fonts::mono (10.0f));
    g.setColour (previewText.isEmpty() ? Palette::textDisabled : Palette::textPrimary);
    g.drawFittedText (previewText.isEmpty() ? juce::String ("Preview: the opening bar shows here.")
                                            : "Preview: " + previewText,
                      previewBounds, juce::Justification::topLeft, 3);
}

void MidiOutPanel::resized()
{
    auto bounds = getLocalBounds().reduced (Metrics::grid, 0);
    const int button = Metrics::buttonHeight;

    auto row = [&bounds, button]
    {
        auto r = bounds.removeFromTop (button);
        bounds.removeFromTop (kRowGap);
        return r;
    };

    auto split = [] (juce::Rectangle<int> r, std::initializer_list<juce::Component*> parts)
    {
        const int w = juce::jmax (1, r.getWidth() / (int) parts.size());

        for (auto* c : parts)
            c->setBounds (r.removeFromLeft (w).reduced (1));
    };

    // --- export profile ---------------------------------------------------------------
    profileHeader = bounds.removeFromTop (kHeader);
    split (row(), { luthierProfile.get(), genericProfile.get() });
    split (row(), { &ppqBox, &splitBox });
    split (row(), { realismToggle.get(), sysExToggle.get(), stripIdsToggle.get() });

    for (int first = 0; first < classToggles.size(); first += kClassColumns)
    {
        auto r = row();
        const int w = juce::jmax (1, r.getWidth() / kClassColumns);

        for (int c = first; c < juce::jmin (first + kClassColumns, classToggles.size()); ++c)
            classToggles[c]->setBounds (r.removeFromLeft (w).reduced (1));
    }

    split (bounds.removeFromTop (button), { &saveProfileButton, &loadProfileButton });
    bounds.removeFromTop (Metrics::grid);

    // --- export -----------------------------------------------------------------------
    exportHeader = bounds.removeFromTop (kHeader);
    split (row(), { &rangeBox, &secondsSlider });
    split (row(), { &exportButton, dragSource.get() });
    captureBounds = bounds.removeFromTop (32);
    previewBounds = bounds.removeFromTop (40);
    bounds.removeFromTop (Metrics::grid);

    // --- live -------------------------------------------------------------------------
    liveHeader = bounds.removeFromTop (kHeader);
    split (row(), { liveEnable.get(), &liveChannel });

    for (int first = 0; first < sourceToggles.size(); first += 4)
    {
        auto r = row();
        const int w = juce::jmax (1, r.getWidth() / 4);

        for (int s = first; s < juce::jmin (first + 4, sourceToggles.size()); ++s)
            sourceToggles[s]->setBounds (r.removeFromLeft (w).reduced (1));
    }

    for (int line = 0; line < 2; ++line)
    {
        auto table = bounds.removeFromTop (40);
        const int w = juce::jmax (1, table.getWidth() / 3);

        for (int col = 0; col < 3; ++col)
        {
            const int m = line * 3 + col;

            if (m >= ParamIDs::kNumMacros)
                break;

            auto cell = table.removeFromLeft (w).reduced (1);
            macroCcLabels[m].setBounds (cell.removeFromTop (14));
            macroCc[m].setBounds (cell.removeFromTop (24));
        }
    }
}

} // namespace luthier
