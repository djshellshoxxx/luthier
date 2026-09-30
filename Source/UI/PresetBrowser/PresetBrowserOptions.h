#pragma once

/*  preset-browser-previews.md 8, 5.2 and 7.4: the browser's three Options
    groups, each a self-contained component the Options pages host.

    - PresetBrowserAppearanceGroup: APPEARANCE -> PRESET BROWSER, the five
      presetPreview.* keys (UiPreferences, written immediately).
    - PresetCacheGroup: FILE LOCATIONS -> "Preview cache: Open / Clear".
    - PresetBrowserKeysGroup: ACCESSIBILITY -> "Preset browser" key group.
*/

#include <juce_gui_basics/juce_gui_basics.h>

namespace luthier
{

class LuthierAudioProcessor;

//==============================================================================
class PresetBrowserAppearanceGroup : public juce::Component
{
public:
    explicit PresetBrowserAppearanceGroup (LuthierAudioProcessor&);

    /** Reads the keys back (the footer may have changed them). */
    void refresh();

    static constexpr int kHeight = 110;

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::ToggleButton enabledToggle { "Previews" };
    juce::ComboBox triggerBox;
    juce::ToggleButton onSelectToggle { "Preview on keyboard selection" };
    juce::Slider volumeSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ToggleButton whileTransportToggle { "Preview while the host or a tune plays" };

private:
    void push();

    LuthierAudioProcessor& processor;
    juce::Label triggerLabel, volumeLabel;
    bool updating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowserAppearanceGroup)
};

//==============================================================================
class PresetCacheGroup : public juce::Component
{
public:
    explicit PresetCacheGroup (LuthierAudioProcessor&);

    void refresh();

    static constexpr int kHeight = 28;

    void resized() override;

    juce::TextButton openButton { "Open" };
    juce::TextButton clearButton { "Clear" };

private:
    LuthierAudioProcessor& processor;
    juce::Label label;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetCacheGroup)
};

//==============================================================================
class PresetBrowserKeysGroup : public juce::Component
{
public:
    PresetBrowserKeysGroup();

    static constexpr int kHeight = 26;

    void refresh();
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    juce::ComboBox actionBox;
    juce::TextButton setKeyButton { "Set key..." };
    juce::TextButton resetButton { "Reset" };

private:
    juce::Label label;
    bool capturing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowserKeysGroup)
};

} // namespace luthier
