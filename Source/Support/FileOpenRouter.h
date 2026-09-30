#pragma once

/*  installer.md 1.3 / 13.6: double-clicking a Luthier file opens it in the
    standalone. The router is the pure part - which loader a file goes to, by
    its extension - so it can be tested without a window; open() then hands
    the file to that loader. Message thread.
*/

#include <juce_core/juce_core.h>

namespace luthier
{

class LuthierAudioProcessor;

namespace FileOpenRouter
{
    enum class Target
    {
        unknown = 0,
        preset,         ///< .luthierpreset -> PresetManager::loadPreset
        guitar,         ///< .luthierguitar -> the Workshop's guitar
        tune,           ///< .luthiertune   -> the tune session
        loop,           ///< .luthierloop   -> the looper
        setlist,        ///< .luthierset    -> the setlist player
        midiProfile     ///< .midprofile    -> the MIDI export defaults
    };

    /** Which loader a file belongs to, by extension (case-insensitive). */
    Target route (const juce::String& fileNameOrPath) noexcept;

    const char* describe (Target target) noexcept;

    /** The first argument of a command line that names a file (quotes
        stripped), or an empty File. Options starting with '-' are skipped. */
    juce::File fileFromCommandLine (const juce::String& commandLine);

    /** Opens `file` in `processor`. False, with `error` set, if it is not a
        Luthier file or its loader refused it. */
    bool open (LuthierAudioProcessor& processor, const juce::File& file, juce::String& error);
}

} // namespace luthier
