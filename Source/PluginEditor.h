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

private:
    void timerCallback() override;
    void setAdvancedMode (bool advanced);
    void showOverlay (OverlayPanel* panel);

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
    LiveStrip liveStrip;
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
