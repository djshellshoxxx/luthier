#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "Accessibility/Accessibility.h"
#include "UI/HeaderBar.h"
#include "UI/LiveStrip.h"
#include "UI/PracticePanel.h"
#include "UI/EasyPanel.h"
#include "UI/AdvancedPanel.h"
#include "UI/Overlays.h"
#include "UI/Notifications.h"
#include "UI/ValidatorNotices.h"   // SP-111 / SP-114
#include "Export/MidiImportTargets.h"   // midi-export 5 (MODEL-GAPS)
#include "UI/Onboarding.h"
#include "UI/QualityBadge.h"   // cpu-quality-modes

namespace luthier
{

//==============================================================================
class LuthierAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::DragAndDropContainer,   // gui-integration 11.2: drag-to-modulate
                                    public juce::FileDragAndDropTarget,   // midi-export 5 (MODEL-GAPS)
                                    private juce::Timer,
                                    private juce::ChangeListener
{
public:
    /*  midi-export 5 (MODEL-GAPS): "File -> Import -> MIDI, or drag a .mid file
        onto the plugin window", then the target: the session, the Tune
        Builder or the looper. With `target` given (the tests) there is no
        menu. The outcome is posted as a banner and returned when known. */
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void importMidiFile (const juce::File& file, std::optional<MidiImportTarget> target = {});
    MidiImportOutcome getLastMidiImport() const { return lastMidiImport; }

    explicit LuthierAudioProcessorEditor (LuthierAudioProcessor&);
    ~LuthierAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    static constexpr int defaultWidth = 1200;
    static constexpr int defaultHeight = 720;
    static constexpr int minimumWidth = 940;
    static constexpr int minimumHeight = 560;

    /** SPEC-SWEEP (USER_MANUAL UM-60): the footer's CPU share and reported
        latency, as drawn. */
    juce::String getFooterText() const;

    /** SPEC-SWEEP (include.md INC-12): applies the tooltip on/off preference to
        the tooltip window (the timer calls this) and reports the delay it set. */
    void applyTooltipPreference();
    int getTooltipDelayMs() const noexcept { return tooltipDelayMs; }

    /*  gui-integration 15: the triggers the plugin can raise on its own, checked
        once when the window opens. Public so a test can drive it against a
        processor it has arranged, rather than waiting for a real crash, a real
        expiry or a real administrator. */
    void postStartupNotifications();

    /*  gui-integration 15's triggers that can happen at any time rather than only
        at startup: a preset that would not load, and an IR a preset asked for
        that is no longer on disk. Called from the timer.

        Polled rather than pushed because presets are loaded from five places -
        the header, the browser, the Easy panel's style list, a host program
        change, and the processor's own state restore - and five call sites each
        remembering to report would be five chances to forget. It posts only when
        the message changes, so a dismissed banner stays dismissed instead of
        coming back four times a second. */
    void pollForNotifications();

    NotificationCentre& getNotifications() noexcept { return notifications; }

    /*  Opens the Options overlay on one named page. Used by the banners, which
        send the user somewhere specific rather than just saying a thing happened.
        Returns false when there is no such tab, so a caller can tell the
        difference between "opened it" and "that page does not exist here". */
    bool showOptionsPage (const juce::String& tabName);

    /** output-normalization.md 5.1: Options -> AUDIO with the switch focused
        (the header / Easy badge, the banner's [Options]). */
    void openNormalizationOptions();
    /*  cpu-quality-modes 5: the footer badge's destination - Options -> AUDIO
        with focus in the CPU quality group. */
    void openQualityOptions();
    QualityBadge& getQualityBadge() noexcept { return qualityBadge; }
    QualityEditorLink& getQualityLink() noexcept { return qualityLink; }

    //==========================================================================
    // onboarding.md 2-4 (TUNE-HELP-ONBOARDING; PluginEditorOnboarding.cpp).

    /** Starts the tour (the welcome banner's Yes, Help -> Take the tour). */
    void startTour();
    TourOverlay& getTour() noexcept                { return tour; }
    WelcomeBanner& getWelcomeBanner() noexcept     { return welcomeBanner; }
    DiscoveryLayer& getDiscoveryLayer() noexcept   { return discovery; }
    DiscoveryTooltip& getRandomiseTooltip() noexcept { return randomiseTooltip; }

    /** Where a tour stop points, in this component's coordinates. */
    juce::Rectangle<int> findTourTarget (const juce::String& stepId);

    /** Makes a stop's target visible: Advanced for the column stops, LIVE for snapshots. */
    void prepareTourStep (const juce::String& stepId);

    /** Records the launch and puts up the welcome banner if one is due. */
    void runWelcome();

    /** gui-integration 17 "New tune" (Ctrl+T): the TUNE tab, with its New menu.
        Returns the tab's panel, or nullptr when it cannot be shown. */
    TunePanel* openNewTune();

private:
    void timerCallback() override;

    /** accessibility.md 6: a palette change reaches every panel at once. */
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    /** accessibility 4 (A11Y-29): the user's scale, stepped down to the largest
        that lets the minimum window fit this screen. Returns the warning to show,
        once per session, otherwise empty. */
    juce::String applyUiScale();
    void setAdvancedMode (bool advanced);
    void showOverlay (OverlayPanel* panel);

    /** gui-integration 17 (W) and the header wrench: the Workshop on, or off
        again back to the tab or window it came from. VISUAL-WORKSHOP-QA. */
    void toggleWorkshop();
    int tabBeforeWorkshop = -1;
    bool newDotsApplied = false;   // gui-integration 20

    /** guitar-workshop.md 6 (Ctrl+G): asks for a name and saves the guitar. */
    void showSaveGuitarDialog();

    /*  gui-integration 4.5: Advanced Mode is unavailable below 1000 points. The
        window's own minimum is 940, so this is a state a user can reach by
        dragging rather than a theoretical one, and the mode toggle has to refuse
        rather than lay out three unreadable columns. */
    bool isAdvancedModeAvailable() const noexcept;

    /** The message the toggle and the resize both show when it is not. */
    static juce::String advancedUnavailableMessage();

    /** live-performance 10: shows or hides the live strip and re-lays out. */
    void updateLiveStripVisibility();

    /** gui-integration 19: arms MIDI Learn from the header or Ctrl+L, so the
        feature is not reachable only by right-click (ground rule 4). */
    void setMidiLearnArmed (bool armed);

    /** The easter egg's target: one specific pixel, inside the signature notch in
        the top-left corner. Clicking it opens the hidden effect. */
    juce::Rectangle<int> getSecretPixelBounds() const;

    LuthierAudioProcessor& processor;

    LuthierLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, Metrics::tooltipDelayMs };
    int tooltipDelayMs = Metrics::tooltipDelayMs;   // SPEC-SWEEP INC-12

    HeaderBar header;
    MidiImportOutcome lastMidiImport;   // MODEL-GAPS

    /*  Section 15 puts the banner strip "under the header strip", so it is laid
        out directly beneath the header and above the live strip: the live strip
        is a permanent fixture of Live Mode and a banner is passing news, and news
        that pushed the live controls up every time it arrived would move the
        buttons under a player's hand mid-set. */
    NotificationCentre notifications;
    ValidatorNotices validatorNotices;   // SP-111 / SP-114: the validator's corrections, said out loud

    LiveStrip liveStrip;
    InlineNotice inlineNotice;
    PracticePanel practicePanel;
    EasyPanel easyPanel;
    AdvancedPanel advancedPanel;

    OverlayHost overlayHost;
    MidiLearnArmLayer midiLearnArmLayer;

    HelpPanel helpPanel;

    /** F1 and the header's ?: help on the panel in use (accessibility 2). */
    juce::String getHelpContext() const;
    void openHelp (const juce::String& topic);
    DebugPanel debugPanel;
    OptionsPanel optionsPanel;
    ExportPanel exportPanel;
    PresetBrowserPanel presetBrowser;
    SaveAsPanel saveAsPanel;
    ChordAndTabPanel chordPanel;
    WorkshopOverlay workshopOverlay;
    SecretPanel secretPanel;

    // onboarding.md 2-4 (TUNE-HELP-ONBOARDING).
    void setupOnboarding();

    /** onboarding 1: a fresh install starts on the rock overdrive preset. */
    void applyFirstRunPreset();
    WelcomeBanner welcomeBanner;
    TourOverlay tour;
    DiscoveryLayer discovery;
    DiscoveryTooltip randomiseTooltip;

    juce::TextButton chordButton { "Chords / Tab" };

    // cpu-quality-modes 5 / 6: the footer badge and this editor's link.
    QualityBadge qualityBadge { processor };
    QualityEditorLink qualityLink { processor, notifications };
    /** gui-integration 1 / 12: the footer's scrolling data stream (Options ->
        Appearance can hide it). VISUAL-WORKSHOP-QA. */
    DataStreamDisplay dataStream;

    bool advancedMode = false;
    bool secretHovered = false;

    /*  The last messages pollForNotifications raised, so an unchanged condition
        is not reposted. Without these, dismissing a banner about a preset that
        still will not load would put it straight back on screen. */
    juce::String reportedPresetError, reportedIrError;
    juce::String reportedPresetSaveError;   // SPEC-SWEEP: ER-19
    juce::uint32 seenMigrationGeneration = 0;   // installer.md 8
    bool migrationBannerShown = false;

    /** Remembered so the layout is only redone when Live Mode actually changes. */
    bool liveModeShown = false;

    /*  RangeState::getGeneration() when the controls were last re-attached. A
        preset load or a RANGES toggle swaps parameter ranges under attached
        sliders; the timer notices and re-attaches them (RangesUi). */
    juce::uint32 seenRangeGeneration = 0;

    /** The palette this window's components were last coloured with. */
    PaletteColours shownPalette;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuthierAudioProcessorEditor)
};

} // namespace luthier
