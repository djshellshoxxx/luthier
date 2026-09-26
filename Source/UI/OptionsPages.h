#pragma once

/*  The pages of the Options overlay.

    gui-integration.md section 5 fixes the tab list:

        AUDIO | MIDI | APPEARANCE | ACCESSIBILITY | LOCALIZATION | EXPRESSION
        | RANGES | UPDATES | PRIVACY | DIAGNOSTICS | FILE LOCATIONS

    Every one of those is a page here.

    ControllersPage is declared here too and is no longer one of them. Section 19
    puts controller setup in the Advanced column 4 tab strip; it sat on this
    overlay while that strip did not exist, and AdvancedPanel owns it now. It
    still derives from OptionsPage because that base is a Component holding the
    processor with a refresh hook, which is all it ever needed of it - the name
    says where the page came from, not where it lives.

    Each page is a plain component that knows nothing about the others, so adding
    the next one is adding a class rather than editing a switch. OptionsPanel owns
    them in tab order and shows one at a time.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include "Theme.h"
#include "Widgets.h"
#include "AudioPathView.h"
#include "../Controllers/ControllerProfile.h"
#include "../Updates/Telemetry.h"
#include "../Updates/UpdateDownloader.h"

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
/** A page of the Options overlay. */
class OptionsPage : public juce::Component
{
public:
    explicit OptionsPage (LuthierAudioProcessor& p) : processor (p) {}

    /** Called when the page becomes visible, and periodically while it is. */
    virtual void refresh() {}

protected:
    LuthierAudioProcessor& processor;
};

//==============================================================================
/*  Options -> Audio (section 5).

    "Output device, buffer, sample rate, sidechain input", all of which belong to
    the host or to the standalone wrapper rather than to the plugin, so what this
    page can honestly offer is an explanation of where they live plus the one
    audio setting the plugin does own.

    Oversampling is that setting. Section 5 does not list it anywhere, and it is
    plainly an audio-quality choice rather than an appearance or a MIDI one, so
    this is the nearest tab. Recorded in GAPS.md A3 as a judgement call.
*/
class AudioPage final : public OptionsPage
{
public:
    explicit AudioPage (LuthierAudioProcessor& processor);

    void refresh() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    LuthierChoice oversampling { "Oversampling" };

    // noise-floor.md 3: the user-global default mains region (REALISM-C).
    juce::ComboBox mainsRegion;
    juce::Label mainsLabel;

    juce::TextButton deviceButton { "Where are the device settings?" };
    juce::Label deviceNote, sidechainNote, latencyLabel;
};

//==============================================================================
/*  Options -> MIDI (section 5).

    The port picker and the virtual MIDI out belong to the wrapper, the same way
    the audio devices do. What the plugin owns is how it reads the MIDI it is
    given, and the chord window is the setting that decides it, so it is here
    rather than on a tab of its own.
*/
class MidiPage final : public OptionsPage
{
public:
    explicit MidiPage (LuthierAudioProcessor& processor);

    void refresh() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    LuthierKnob chordWindow { "Chord Window" };

    juce::Label portNote, outNote, learnLabel;
    juce::TextButton clearLearnButton { "Clear all MIDI mappings" };
};

//==============================================================================
/** Options -> Appearance (section 5): what the window looks like. */
class AppearancePage final : public OptionsPage
{
public:
    explicit AppearancePage (LuthierAudioProcessor& processor);

    void refresh() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::ComboBox paletteBox, scaleBox;
    juce::ToggleButton reducedMotionToggle { "Reduced motion" };
    juce::ToggleButton tooltipsToggle { "Show tooltips on hover" };

    // gui-integration 5 / visual-polish.md 5: the accent, the data stream, the noise strip.
    juce::ComboBox accentBox;
    juce::ToggleButton dataStreamToggle { "Scrolling data stream in the footer" };
    juce::ToggleButton noiseStripToggle { "Noise-event strip (CHARACTER)" };
    juce::ToggleButton vuToggle { "VU meter (Easy window)" };

    // piano-roll-chord-display.md 5: "Visual aids", beside Show tooltips.
    juce::ToggleButton chordNamesToggle { "Show chord names on the guitar" };
    juce::ToggleButton announceChordsToggle { "Announce chord names" };
    juce::ToggleButton pianoRollAdvancedToggle { "Show piano roll (Advanced)" };
    juce::ToggleButton pianoRollEasyToggle { "Show piano roll (Easy)" };
    juce::ComboBox pianoRollShowsBox;

public:
    juce::ToggleButton& getChordNamesToggle() noexcept { return chordNamesToggle; }
    juce::ToggleButton& getAnnounceChordsToggle() noexcept { return announceChordsToggle; }
    juce::ToggleButton& getPianoRollToggle (bool advanced) noexcept { return advanced ? pianoRollAdvancedToggle : pianoRollEasyToggle; }
    juce::ComboBox& getPianoRollShowsBox() noexcept { return pianoRollShowsBox; }
private:

    juce::Label contrastLabel, accentNote;

    bool updatingControls = false;
};

//==============================================================================
/*  Options -> Accessibility (section 5, accessibility.md 2 and 9).

    Screen-reader verbosity, the font override, and the rebindable shortcut table
    with search and reset. The palette, the UI scale and reduced motion used to
    share this page; section 5 puts them under APPEARANCE and they moved there.
*/
class AccessibilityPage final : public OptionsPage
{
public:
    explicit AccessibilityPage (LuthierAudioProcessor& processor);

    /** gui-integration 16 item 13: the table filtered to one action's row. */
    void filterShortcuts (const juce::String& text)  { searchBox.setText (text, true); }
    juce::String getShortcutFilter() const            { return searchBox.getText(); }

    void refresh() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void rebuildShortcutList();

    juce::ComboBox verbosityBox, fontBox;

    // The rebind table (accessibility 2).
    juce::ListBox shortcutList;
    juce::TextEditor searchBox;
    juce::TextButton resetAllButton { "Reset all shortcuts" };
    juce::Label rebindHint;

    juce::Array<int> visibleShortcuts;

    class ShortcutModel : public juce::ListBoxModel
    {
    public:
        explicit ShortcutModel (AccessibilityPage& o) : owner (o) {}

        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    private:
        AccessibilityPage& owner;
    };

    ShortcutModel shortcutModel { *this };

    /** Which row is waiting for a key, or -1. */
    int capturingRow = -1;

    bool updatingControls = false;

    /** Catches the key press for a rebind. */
    bool keyPressed (const juce::KeyPress& key) override;

    /** global-search.md 7 (FEAT-SEARCH): the "Search" group. */
    std::unique_ptr<juce::Component> searchGroup;
};

//==============================================================================
/** Options -> Localization (section 5, accessibility.md 6). */
class LocalizationPage final : public OptionsPage
{
public:
    explicit LocalizationPage (LuthierAudioProcessor& processor);

    void refresh() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::ComboBox localeBox, fallbackBox;
    juce::TextButton catalogButton { "Custom catalog..." };
    juce::Label localeNote, catalogLabel;

    std::unique_ptr<juce::FileChooser> chooser;

    bool updatingControls = false;
};

//==============================================================================
/** Advanced column 4 -> CONTROLLERS (controllers.md sections 3 and 5, and
    gui-integration.md section 19). Owned by AdvancedPanel, not by OptionsPanel. */
class ControllersPage final : public OptionsPage
{
public:
    explicit ControllersPage (LuthierAudioProcessor& processor);

    void refresh() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void applySelectedProfile();
    void runWizardStep();

    ControllerProfileLibrary library;
    LatencyWizard wizard;

    juce::ComboBox profileBox;
    juce::Label profileNotes, routingLabel;

    juce::Slider latencySlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider deadZoneSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider minimumNoteSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    juce::ToggleButton guitarModeToggle { "LinnStrument guitar mode: rows map to strings" };

    // The latency wizard (controllers 3).
    juce::TextButton wizardButton { "Measure latency" };
    juce::Label wizardLabel;

    juce::TextButton saveProfileButton { "Save as my profile" };

    bool updatingControls = false;
};

//==============================================================================
/** Options -> Expression (live-performance.md section 8). */
class ExpressionPage final : public OptionsPage
{
public:
    explicit ExpressionPage (LuthierAudioProcessor& processor);

    void refresh() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void advanceWizard();

    juce::ComboBox ccBox, curveBox;
    juce::Label promptLabel, rangeLabel;

    juce::TextButton calibrateButton { "Calibrate" }, cancelButton { "Cancel" };

    juce::Slider heelDeadZone { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider toeDeadZone { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label heelLabel { {}, "Heel dead zone" }, toeLabel { {}, "Toe dead zone" };

    juce::ListBox calibratedList;
    juce::Array<int> calibratedCcs;

    class ListModel : public juce::ListBoxModel
    {
    public:
        explicit ListModel (ExpressionPage& o) : owner (o) {}

        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;

    private:
        ExpressionPage& owner;
    };

    ListModel listModel { *this };

    bool updatingControls = false;
};

//==============================================================================
/*  Options -> Ranges (section 5, advanced-ranges.md 6.2).

    Four things, in the spec's order: the per-preset master toggle, the two
    user-global preferences, and the out-of-stock summary. The master toggle
    shows the clamp count before a lock commits, because a lock that moves
    values without saying so is the silent change ground rule 0.2 forbids.
*/
class RangesPage final : public OptionsPage
{
public:
    explicit RangesPage (LuthierAudioProcessor& processor);

    void refresh() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    /** The ids the summary is listing, for tests. */
    const juce::StringArray& getListedParameters() const noexcept { return outside; }

    /** What the master toggle does, without the confirmation. For tests and for
        the confirmation's own callback. Returns the clamp count. */
    int setAllFamilies (bool advanced);

    /** The summary row's Clamp button (advanced-ranges.md 6.2 item 4). Public for tests. */
    void clampOne (const juce::String& parameterId);

private:
    void masterToggled();

    juce::ToggleButton masterToggle { "Advanced ranges for this preset" };
    juce::ToggleButton warningToggle { "Always show marked values as warning colour" };
    juce::ToggleButton randomiseToggle { "Randomise respects stock range" };
    juce::Label masterNote, emptyNote;

    juce::StringArray outside;
    juce::ListBox summary;

    class SummaryModel : public juce::ListBoxModel
    {
    public:
        explicit SummaryModel (RangesPage& o) : owner (o) {}

        int getNumRows() override;
        void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
        juce::Component* refreshComponentForRow (int row, bool selected,
                                                 juce::Component* existing) override;

    private:
        RangesPage& owner;
    };

    SummaryModel summaryModel { *this };

    bool updatingControls = false;
};

//==============================================================================
/** Options -> Updates (section 5, updates-telemetry.md 1). */
class UpdatesPage final : public OptionsPage
{
public:
    explicit UpdatesPage (LuthierAudioProcessor& processor);

    void refresh() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Telemetry& telemetry();

    void checkForUpdate();

    juce::ToggleButton updateCheckToggle { "Check for updates automatically" };
    juce::ToggleButton betaToggle { "Include beta releases" };
    juce::TextButton checkNowButton { "Check now" };
    juce::Label updateStatus, policyLabel, changelogNote;

    // installer.md 5.1: the release notes open in the browser; the installer
    // downloads to Downloads and is never launched.
    juce::TextButton releaseNotesButton { "Release notes" };
    juce::TextButton downloadButton { "Download" };
    juce::String changelogUrl, downloadUrl;
    UpdateDownloader downloader;

    void startDownload();

    juce::TextEditor releaseNotes;

    bool updatingControls = false;
};

//==============================================================================
/** Options -> Privacy (section 5, updates-telemetry.md 6). */
class PrivacyPage final : public OptionsPage
{
public:
    explicit PrivacyPage (LuthierAudioProcessor& processor);

    void refresh() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Telemetry& telemetry();

    // Telemetry, each explained in plain English as the spec asks.
    juce::ToggleButton usageToggle { "Usage telemetry" };
    juce::ToggleButton diagnosticsToggle { "Diagnostics telemetry" };
    juce::ToggleButton crashToggle { "Crash reports" };

    juce::Label usageExplanation, diagnosticsExplanation, crashExplanation;

    // The log, and the buttons that act on it.
    juce::TextButton viewLogButton { "View last upload" };
    juce::TextButton clearLogsButton { "Clear all local logs" };
    juce::TextButton paranoiaButton { "Turn everything off and delete all diagnostic files" };

    juce::TextEditor logView;

    // Endpoints, for an enterprise proxy.
    juce::TextEditor manifestUrlBox, telemetryUrlBox, crashUrlBox;
    juce::Label endpointsHeading, policyLabel;

    bool updatingControls = false;
};

//==============================================================================
/*  Options -> Diagnostics (section 5).

    The debug window itself stays an overlay - it is a live stream that wants the
    whole window - and this page is the route to it, alongside the switches and
    the one-shot actions the section lists. They call the same processor and
    Diagnostics methods the debug window's own buttons do, so the two surfaces
    cannot drift apart.

    The Workshop / Slide / advanced-ranges mirror the section also asks for is
    left out: those three booleans do not exist yet.
*/
class DiagnosticsPage final : public OptionsPage
{
public:
    explicit DiagnosticsPage (LuthierAudioProcessor& processor);

    /** Wired by the editor, which is the only thing that can open an overlay. */
    std::function<void()> onShowDebugWindow;

    /** onboarding.md 12 (TUNE-HELP-ONBOARDING): what the confirmation's OK does -
        clears the user-global settings (TODO 14c: ranges_first_unlock_explained
        too) and tells the processor the restored range preference. */
    void restoreFirstRun();
    juce::TextButton& getRestoreFirstRunButton() noexcept { return restoreFirstRunButton; }

    void refresh() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::TextButton debugWindowButton { "Open the debug window" };
    juce::ToggleButton crashLogToggle { "Create a log file if Luthier crashes" };
    juce::ToggleButton recorderToggle { "Keep the last hour of audio for the session recorder" };
    juce::ToggleButton cpuDropToggle { "Under CPU overload, drop the least active strings" };   // performance-budget.md 8
    juce::ToggleButton undoDepthToggle { "Show undo depth in the footer" };   // action-and-undo.md 12

    juce::TextButton troubleshootButton { "Export troubleshooting file" };
    juce::TextButton openFolderButton { "Open diagnostics folder" };
    juce::TextButton hardResetButton { "Reset all settings and clear caches" };
    juce::TextButton restoreFirstRunButton { "Restore first-run experience" };

    juce::Label explanation, recorderNote, mirrorNote;

    /** gui-integration 20: "What's on the audio path right now", with section 5's flags mirror. */
    std::unique_ptr<AudioPathView> audioPath;

public:
    AudioPathView* getAudioPathView() const noexcept { return audioPath.get(); }
};

//==============================================================================
/*  Options -> File locations (section 5).

    Every user data folder, with a button that opens it, plus the preset search
    path: the folders Luthier scans, and the buttons that add to or rescan them.

    ~/Documents/Luthier/Guitars/ and /Parts/ have their own buttons.
*/
class FileLocationsPage final : public OptionsPage
{
public:
    explicit FileLocationsPage (LuthierAudioProcessor& processor);

    void refresh() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::TextButton openUserFolder { "Open user preset folder" };
    juce::TextButton openRenderFolder { "Open render folder" };
    juce::TextButton openFactoryFolder { "Open factory preset folder" };
    juce::TextButton openDiagnosticsFolder { "Open diagnostics folder" };
    juce::TextButton openGuitarsFolder { "Open guitars folder" };   // gui-integration 5
    juce::TextButton openPartsFolder { "Open parts folder" };
    juce::TextButton addFolderButton { "Add a preset folder..." };
    juce::TextButton rescanButton { "Rescan presets" };

    juce::Label pathLabel, formatNote;
    juce::ListBox folderList;

    std::unique_ptr<juce::FileChooser> chooser;

    class FolderListModel : public juce::ListBoxModel
    {
    public:
        explicit FolderListModel (FileLocationsPage& o) : owner (o) {}

        int getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;

    private:
        FileLocationsPage& owner;
    };

    FolderListModel folderModel { *this };
};

} // namespace luthier
