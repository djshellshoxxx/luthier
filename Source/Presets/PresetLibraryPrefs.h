#pragma once

/*  preset-browser-previews.md 5.5: favourites, ratings and recent loads.

    User-global, written through immediately like UiPreferences, to
    ~/Documents/Luthier/config/preset-library.json. Keyed by the preset's uid
    (so a rename or a move keeps them), or by its path relative to its search
    folder for a preset with no uid. Never written into a preset file, never on
    the undo stack (9).

    Message thread only.
*/

#include <juce_core/juce_core.h>

namespace luthier
{

class PresetLibraryPrefs
{
public:
    static constexpr int kMaxRecent = 30;
    static const char* const kMagic;   ///< "luthier.presetlibrary"

    static PresetLibraryPrefs& get();

    static juce::File getDefaultFile();

    /** Tests point the store at a scratch file; an empty File restores the default. */
    void setFile (const juce::File& file);
    juce::File getFile() const { return file; }

    bool load();
    bool save() const;

    bool isFavourite (const juce::String& key) const;
    void setFavourite (const juce::String& key, bool favourite);

    /** 0 = unrated, 1-5 stars. */
    int getRating (const juce::String& key) const;
    void setRating (const juce::String& key, int stars);

    /** Records a load: recent list, last-loaded time and count. */
    void noteLoaded (const juce::String& key);
    int getLoadCount (const juce::String& key) const;
    juce::Time getLastLoaded (const juce::String& key) const;

    /** Newest first. */
    juce::StringArray getRecent() const { return recent; }

    /** A rename in the browser re-keys the entry (5.5). */
    void rekey (const juce::String& from, const juce::String& to);

    /** Drops everything (tests). */
    void reset();

private:
    PresetLibraryPrefs();

    juce::DynamicObject* entryFor (const juce::String& key, bool create);
    const juce::DynamicObject* entryFor (const juce::String& key) const;

    juce::File file;
    juce::var entries;
    juce::StringArray recent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetLibraryPrefs)
};

} // namespace luthier
