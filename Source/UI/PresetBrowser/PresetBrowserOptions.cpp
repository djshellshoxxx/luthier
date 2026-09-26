#include "PresetBrowserOptions.h"
#include "PresetBrowserKeys.h"
#include "../Theme.h"
#include "../UiPreferences.h"
#include "../../PluginProcessor.h"
#include "../../Presets/PresetLibrary.h"

namespace luthier
{

namespace
{
    void styleLabel (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (Fonts::ui (11.5f));
        l.setColour (juce::Label::textColourId, Palette::textMuted);
    }

    /** Pushes the keys into a live library, if the processor has one. */
    void pushToLibrary (LuthierAudioProcessor& processor)
    {
        if (! processor.hasPresetLibrary())
            return;

        auto& prefs = UiPreferences::get();
        PresetLibrary::Settings s;
        s.enabled = prefs.getBool ("presetPreview.enabled", true);
        s.hoverTrigger = prefs.getString ("presetPreview.trigger", "hover") != "click";
        s.onSelect = prefs.getBool ("presetPreview.onSelect", true);
        s.volumeDb = juce::jlimit (-40, 0, prefs.getInt ("presetPreview.volumeDb", -6));
        s.whileTransport = prefs.getBool ("presetPreview.whileTransport", false);
        processor.getPresetLibrary().setSettings (s);
    }
}

//==============================================================================
PresetBrowserAppearanceGroup::PresetBrowserAppearanceGroup (LuthierAudioProcessor& p) : processor (p)
{
    setTitle ("Preset browser");

    for (auto* c : std::initializer_list<juce::Component*> { &enabledToggle, &triggerBox, &onSelectToggle,
                                                             &volumeSlider, &whileTransportToggle,
                                                             &triggerLabel, &volumeLabel })
        addAndMakeVisible (c);

    enabledToggle.setTitle ("Preset previews on or off");
    styleLabel (triggerLabel, "Start previews on");
    triggerBox.addItem ("Hover", 1);
    triggerBox.addItem ("Click only", 2);
    triggerBox.setTitle ("Preview trigger");
    styleLabel (volumeLabel, "Preview volume");
    volumeSlider.setRange (-40.0, 0.0, 1.0);
    volumeSlider.setTextValueSuffix (" dB");
    volumeSlider.setTitle ("Preview volume");

    enabledToggle.onClick = [this] { push(); };
    triggerBox.onChange = [this] { push(); };
    onSelectToggle.onClick = [this] { push(); };
    volumeSlider.onValueChange = [this] { push(); };
    whileTransportToggle.onClick = [this] { push(); };

    refresh();
}

void PresetBrowserAppearanceGroup::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);
    auto& prefs = UiPreferences::get();

    enabledToggle.setToggleState (prefs.getBool ("presetPreview.enabled", true), juce::dontSendNotification);
    triggerBox.setSelectedId (prefs.getString ("presetPreview.trigger", "hover") == "click" ? 2 : 1, juce::dontSendNotification);
    onSelectToggle.setToggleState (prefs.getBool ("presetPreview.onSelect", true), juce::dontSendNotification);
    volumeSlider.setValue (juce::jlimit (-40, 0, prefs.getInt ("presetPreview.volumeDb", -6)), juce::dontSendNotification);
    whileTransportToggle.setToggleState (prefs.getBool ("presetPreview.whileTransport", false), juce::dontSendNotification);
}

void PresetBrowserAppearanceGroup::push()
{
    if (updating)
        return;

    // 8: written immediately.
    auto& prefs = UiPreferences::get();
    prefs.setBool ("presetPreview.enabled", enabledToggle.getToggleState());
    prefs.setString ("presetPreview.trigger", triggerBox.getSelectedId() == 2 ? "click" : "hover");
    prefs.setBool ("presetPreview.onSelect", onSelectToggle.getToggleState());
    prefs.setInt ("presetPreview.volumeDb", (int) std::round (volumeSlider.getValue()));
    prefs.setBool ("presetPreview.whileTransport", whileTransportToggle.getToggleState());
    pushToLibrary (processor);
}

void PresetBrowserAppearanceGroup::paint (juce::Graphics& g)
{
    g.setColour (Palette::textMuted);
    Fonts::drawTrackedText (g, "PRESET BROWSER", getLocalBounds().removeFromTop (18), juce::Justification::centredLeft);
}

void PresetBrowserAppearanceGroup::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (20);

    auto row = r.removeFromTop (26);
    enabledToggle.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (8);
    triggerLabel.setBounds (row.removeFromLeft (110));
    triggerBox.setBounds (row.removeFromLeft (120));
    row.removeFromLeft (8);
    onSelectToggle.setBounds (row.removeFromLeft (240));

    r.removeFromTop (4);
    row = r.removeFromTop (26);
    volumeLabel.setBounds (row.removeFromLeft (110));
    volumeSlider.setBounds (row.removeFromLeft (220));
    row.removeFromLeft (8);
    whileTransportToggle.setBounds (row.removeFromLeft (300));
}

//==============================================================================
PresetCacheGroup::PresetCacheGroup (LuthierAudioProcessor& p) : processor (p)
{
    addAndMakeVisible (label);
    addAndMakeVisible (openButton);
    addAndMakeVisible (clearButton);

    openButton.setTitle ("Open the preview cache folder");
    clearButton.setTitle ("Clear the preview cache");

    openButton.onClick = []
    {
        auto folder = PreviewCache::getDefaultFolder();
        folder.createDirectory();
        folder.revealToUser();
    };

    clearButton.onClick = [this]
    {
        PreviewCache (PreviewCache::getDefaultFolder()).clear();
        refresh();
    };

    refresh();
}

void PresetCacheGroup::refresh()
{
    const PreviewCache cache (PreviewCache::getDefaultFolder());
    const auto mb = (double) cache.getTotalBytes() / (1024.0 * 1024.0);
    styleLabel (label, "Preview cache (" + juce::String (mb, 1) + " MB of 128):");
    juce::ignoreUnused (processor);
}

void PresetCacheGroup::resized()
{
    auto r = getLocalBounds();
    clearButton.setBounds (r.removeFromRight (80));
    r.removeFromRight (4);
    openButton.setBounds (r.removeFromRight (80));
    r.removeFromRight (8);
    label.setBounds (r);
}

//==============================================================================
PresetBrowserKeysGroup::PresetBrowserKeysGroup()
{
    setWantsKeyboardFocus (true);
    addAndMakeVisible (label);
    addAndMakeVisible (actionBox);
    addAndMakeVisible (setKeyButton);
    addAndMakeVisible (resetButton);

    actionBox.setTitle ("Preset browser key");
    int id = 1;

    for (const auto& a : PresetBrowserKeys::actions())
        actionBox.addItem (a.label, id++);

    actionBox.setSelectedId (1, juce::dontSendNotification);
    actionBox.onChange = [this] { refresh(); };

    setKeyButton.onClick = [this]
    {
        capturing = true;
        setKeyButton.setButtonText ("Press a key...");
        grabKeyboardFocus();
    };

    resetButton.onClick = [this]
    {
        const int i = actionBox.getSelectedId() - 1;

        if (juce::isPositiveAndBelow (i, (int) PresetBrowserKeys::actions().size()))
            PresetBrowserKeys::reset (PresetBrowserKeys::actions()[(size_t) i].id);

        refresh();
    };

    refresh();
}

void PresetBrowserKeysGroup::refresh()
{
    const int i = actionBox.getSelectedId() - 1;
    juce::String key;

    if (juce::isPositiveAndBelow (i, (int) PresetBrowserKeys::actions().size()))
        key = PresetBrowserKeys::get (PresetBrowserKeys::actions()[(size_t) i].id).getTextDescriptionWithIcons();

    styleLabel (label, "Preset browser: " + key);
    setKeyButton.setButtonText ("Set key...");
}

bool PresetBrowserKeysGroup::keyPressed (const juce::KeyPress& key)
{
    if (! capturing)
        return false;

    capturing = false;
    const int i = actionBox.getSelectedId() - 1;

    if (key != juce::KeyPress::escapeKey && juce::isPositiveAndBelow (i, (int) PresetBrowserKeys::actions().size()))
        if (! PresetBrowserKeys::rebind (PresetBrowserKeys::actions()[(size_t) i].id, key))
        {
            refresh();
            label.setText ("That key is already used in the preset browser.", juce::dontSendNotification);
            return true;
        }

    refresh();
    return true;
}

void PresetBrowserKeysGroup::resized()
{
    auto r = getLocalBounds();
    resetButton.setBounds (r.removeFromRight (70));
    r.removeFromRight (4);
    setKeyButton.setBounds (r.removeFromRight (110));
    r.removeFromRight (4);
    actionBox.setBounds (r.removeFromRight (220));
    r.removeFromRight (8);
    label.setBounds (r);
}

} // namespace luthier
