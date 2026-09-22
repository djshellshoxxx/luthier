#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/HeaderBar.h"
#include "UI/LiveStrip.h"
#include "UI/PracticePanel.h"
#include "UI/EasyPanel.h"
#include "UI/AdvancedPanel.h"
#include "UI/Overlays.h"
#include "UI/Notifications.h"

namespace luthier
{

//==============================================================================
class LuthierAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
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

    /*  gui-integration 15: the triggers the plugin can raise on its own, checked
        once when the window opens. Public so a test can drive it against a
        processor it has arranged, rather than waiting for a real crash, a real
        expiry or a real administrator. */
    void postStartupNotifications();

    NotificationCentre& getNotifications() noexcept { return notifications; }

    /*  Opens the Options overlay on one named page. Used by the banners, which
        send the user somewhere specific rather than just saying a thing happened.
        Returns false when there is no such tab, so a caller can tell the
        difference between "opened it" and "that page does not exist here". */
    bool showOptionsPage (const juce::String& tabName);

private:
    void timerCallback() override;
    void setAdvancedMode (bool advanced);
    void showOverlay (OverlayPanel* panel);

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

    HeaderBar header;

    /*  Section 15 puts the banner strip "under the header strip", so it is laid
        out directly beneath the header and above the live strip: the live strip
        is a permanent fixture of Live Mode and a banner is passing news, and news
        that pushed the live controls up every time it arrived would move the
        buttons under a player's hand mid-set. */
    NotificationCentre notifications;

    LiveStrip liveStrip;
    InlineNotice inlineNotice;
    PracticePanel practicePanel;
    EasyPanel easyPanel;
    AdvancedPanel advancedPanel;

    OverlayHost overlayHost;
    MidiLearnArmLayer midiLearnArmLayer;

    HelpPanel helpPanel;
    DebugPanel debugPanel;
    OptionsPanel optionsPanel;
    ExportPanel exportPanel;
    PresetBrowserPanel presetBrowser;
    SaveAsPanel saveAsPanel;
    ChordAndTabPanel chordPanel;
    SecretPanel secretPanel;

    juce::TextButton chordButton { "Chords / Tab" };

    bool advancedMode = false;
    bool secretHovered = false;

    /** Remembered so the layout is only redone when Live Mode actually changes. */
    bool liveModeShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuthierAudioProcessorEditor)
};

} // namespace luthier
