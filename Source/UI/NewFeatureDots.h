#pragma once

/*  gui-integration.md 20: "Every new feature added after 1.0 shows a NEW dot on
    its entry point for one week after first launch of the introducing
    version."

    The table below names each such entry point (a workspace tab, a header
    button) and the version that introduced it. The first launch of a version
    is recorded in the user preferences; for seven days after it, the entry
    points introduced in that version carry the `luthier.new` property, which
    the look and feel draws as a small accent dot. 1.0.0 introduced everything
    it has, so the table starts empty: the mechanism is in place for 1.1. */

#include <juce_gui_basics/juce_gui_basics.h>

namespace luthier
{

namespace NewFeatureDots
{
    struct Entry
    {
        const char* entryPoint;    ///< a button's text ("TECHNIQUES") or a component's title
        const char* version;       ///< "1.1.0"
    };

    /** The features added after 1.0, with the version that brought each. */
    const std::vector<Entry>& getTable();

    static constexpr double kShowDays = 7.0;
    static constexpr const char* kProperty = "luthier.new";

    /** Records the first launch of `version` if it is the first (user preferences). */
    void noteLaunch (const juce::String& version, juce::Time now);

    /** Whether `entryPoint` is new for a user running `version` at `now`, given `table`. */
    bool isNew (const juce::String& entryPoint, const juce::String& version, juce::Time now,
                const std::vector<Entry>& table);

    /** Sets or clears the property on every button under `root` whose text is a table entry. */
    void apply (juce::Component& root, const juce::String& version, juce::Time now,
                const std::vector<Entry>& table = getTable());

    /** Draws the dot on a component that carries the property. */
    void paintDot (juce::Graphics& g, juce::Component& c);
}

} // namespace luthier
