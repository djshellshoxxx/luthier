#pragma once

#include <juce_core/juce_core.h>

namespace luthier
{

/*  Opening a file the operating system handed the standalone: a double-click
    in Explorer or a file manager (installer.md 3.1, file-formats.md 1), or the
    same on a second launch that passes it to the running window.

    This half is pure - what kind of file it is, and which files a command line
    names - so it can be tested without a window. The editor's openFile does the
    loading, through the same paths as File > Open and the drop. */
enum class OpenFileKind
{
    preset,     ///< .luthierpreset: the current preset
    guitar,     ///< .luthierguitar: the instrument
    tune,       ///< .luthiertune: the Tune Builder
    midi,       ///< .mid / .midi: imported into the Tune Builder
    unknown     ///< anything else, including the Luthier types there is no open for yet
};

/** By extension, ignoring case. */
OpenFileKind classifyOpenFile (const juce::File& file);

/*  The files a launch names. `commandLine` is as the OS passed it: Windows
    quotes a path with spaces, a .desktop Exec line's %f passes one path, macOS
    and the Linux hand-off quote as needed. Options (anything starting with '-')
    are skipped, so a debugger's or the OS's own flags are not taken for files,
    and so is a word with no extension (an option's value: every file Luthier
    opens has one).
    A relative path is resolved against `workingDirectory`. */
juce::Array<juce::File> filesFromCommandLine (const juce::String& commandLine,
                                              const juce::File& workingDirectory);

/** Joins paths into a command line that filesFromCommandLine reads back. */
juce::String commandLineForFiles (const juce::Array<juce::File>& files);

} // namespace luthier
