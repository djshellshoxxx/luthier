#pragma once

/*  The user folder tree an install creates on first load (installer.md 6).

    ensure() makes the eighteen section-6 subfolders under the root (normally
    ~/Documents/Luthier), writes config/plugin.json if it is absent, and reads
    and rewrites the .installed_version marker:

      - no marker              -> first run (onboarding.md flow);
      - marker != running      -> upgrade from that version (installer.md 8);
      - marker == running      -> quiet.

    Message thread only (file I/O). It never throws and never deletes; a folder
    it cannot create is reported through Result::failures and the rest go on.

    Not to be confused with Source/WIP's onboarding FirstRun, which consumes
    the firstRun flag this reports.
*/

#include <juce_core/juce_core.h>

namespace luthier
{

class InstallLayout
{
public:
    struct Result
    {
        bool firstRun = false;          ///< no marker was present
        juce::String upgradedFrom;      ///< the marker's version when it differs, else empty
        juce::StringArray failures;     ///< anything that could not be created

        bool isUpgrade() const noexcept { return upgradedFrom.isNotEmpty(); }
    };

    /** The subfolders installer.md 6 lists, relative to the root. */
    static const juce::StringArray& subfolders();

    static constexpr const char* markerName = ".installed_version";

    /** Creates what is missing and updates the marker to runningVersion. */
    static Result ensure (const juce::File& root, const juce::String& runningVersion);

    /** ~/Documents/Luthier. */
    static juce::File defaultRoot();

    static juce::File configFile (const juce::File& root) { return root.getChildFile ("config/plugin.json"); }
};

} // namespace luthier
