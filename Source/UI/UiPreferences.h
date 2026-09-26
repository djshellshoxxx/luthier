#pragma once

/*  User-global UI preferences.

    gui-integration.md section 4.4 asks for one thing this store exists to hold:
    "the last-used tab persists across sessions in the plugin's user-global
    settings". There was nowhere to put it. `AccessibilitySettings` has a config
    file, but it describes the person - palette, scale, verbosity, shortcuts -
    and a workspace tab is not that. `PluginProcessor::uiState` is per-preset and
    travels inside the session, so a tab stored there would come back different
    depending on which preset was loaded, which is the opposite of what section
    4.4 asks for.

    So this is the third thing: settings that belong to the person's copy of the
    plugin rather than to the person or to the sound. It is a flat key/value map
    in `Documents/Luthier/config/ui.json`, the same folder
    `AccessibilitySettings` and `ExpressionCalibrationSet` already use.

    Deliberately not a mirror of anything. A value belongs here only if it is
    genuinely global to the user and genuinely about the window: losing the file
    must cost nothing but a preference, because a fresh install has no file and
    every getter therefore has to have a usable default at the call site.

    Writes go through to disk immediately. The file is a few hundred bytes and
    the alternative - saving on editor teardown - loses the value whenever a host
    is killed rather than closed, which is most of the time a plugin dies.
*/

#include <juce_core/juce_core.h>

namespace luthier
{

class UiPreferences
{
public:
    static UiPreferences& get();

    int getInt (const juce::String& key, int fallback) const;
    void setInt (const juce::String& key, int value);

    bool getBool (const juce::String& key, bool fallback) const;
    void setBool (const juce::String& key, bool value);

    juce::String getString (const juce::String& key, const juce::String& fallback) const;
    void setString (const juce::String& key, const juce::String& value);

    /** FEAT-BROWSER (preset-browser-previews 7.4 / 8): whether a key is set, and
        forgetting one so its getter's fallback applies again. */
    bool has (const juce::String& key) const;
    void remove (const juce::String& key);

    static juce::File getConfigFile();

    /** Drops everything and forgets the file's contents. The file itself is left
        alone until the next write, so this is not a way to delete it. */
    void reset();

    /** Re-reads the file. Returns false if there is nothing readable there, which
        is the normal state on a fresh install rather than an error. */
    bool load();
    bool save() const;

private:
    UiPreferences();

    juce::var values;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UiPreferences)
};

} // namespace luthier
