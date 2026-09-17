#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/HeaderBar.h"
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

    /** The easter egg's target: one specific pixel, inside the signature notch in
        the top-left corner. Clicking it opens the hidden effect. */
    juce::Rectangle<int> getSecretPixelBounds() const;

    LuthierAudioProcessor& processor;

    LuthierLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, Metrics::tooltipDelayMs };

    HeaderBar header;
    EasyPanel easyPanel;
    AdvancedPanel advancedPanel;

    OverlayHost overlayHost;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuthierAudioProcessorEditor)
};

} // namespace luthier
