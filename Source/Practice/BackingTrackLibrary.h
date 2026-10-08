#pragma once

/*  The factory practice backing tracks (onboarding.md 6; factory-content.md 8).

    Six original loops rendered by Luthier itself (scripts/make_backing_tracks.py
    drives LuthierRender, so there is nothing to license), shipped as FLAC under
    `Resources/Practice/BackingTracks/`. The Practice drawer's track tab lists
    them beside its "Open..." chooser, and they load through the same
    BackingTrackPlayer as any file the user picks.
*/

#include <juce_core/juce_core.h>

namespace luthier
{

class BackingTrackLibrary
{
public:
    static constexpr int kNumFactoryTracks = 6;

    /** `Resources/Practice/BackingTracks`, or an invalid File when no Resources
        folder was found. */
    static juce::File getFactoryDirectory();

    /** Every backing track in a folder, in file-name order. */
    static juce::Array<juce::File> findTracks (const juce::File& directory);

    static juce::Array<juce::File> findFactoryTracks();

    /** "01-twelve-bar-blues-in-a.flac" -> "Twelve Bar Blues In A". */
    static juce::String getDisplayName (const juce::File& file);
};

} // namespace luthier
