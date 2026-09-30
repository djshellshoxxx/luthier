#pragma once

/*  The host state blob's envelope (SPEC-SWEEP HI-20, HI-24, HI-25;
    host-integration 4, 4.1, 4.2).

    - Every blob carries a root "formatVersion" (a u32, from 1). The blob stays
      JSON rather than the spec's XML; the version tag is what 4 needs.
    - A blob written by a later build keeps what this build does not know: the
      root keys it does not read are held and written back unchanged, and the
      user is told the session came from a newer Luthier.
    - A blob older than this build (or without a version) is copied to
      Documents/Luthier/Diagnostics before anything migrates it, so a migration
      that goes wrong can be undone by hand. The last kMaxBackups are kept.

    Message thread; the processor calls it from get/setStateInformation.
*/

#include <juce_core/juce_core.h>

namespace luthier
{

class HostStateEnvelope
{
public:
    static constexpr int kFormatVersion = 1;
    static constexpr int kMaxBackups = 10;

    /** The root keys this build reads or writes. Anything else is kept. */
    static bool isKnownKey (const juce::Identifier& key);

    /** Adds the version tag and the kept sections to a blob being written. */
    void stamp (juce::DynamicObject& root) const;

    /** What the envelope made of a blob being read. */
    struct Reading
    {
        int formatVersion = 0;      ///< 0: the blob had none (written before HI-20)
        bool newer = false;         ///< written by a later build
        bool backedUp = false;
        juce::File backup;
    };

    /*  Reads the envelope of a parsed blob: keeps its unknown sections, and
        backs up `raw` when the blob is older than this build. */
    Reading read (const juce::DynamicObject& root, const juce::String& raw);

    const juce::NamedValueSet& getKeptSections() const noexcept { return kept; }

    /** Where backups go; tests point it at a temporary folder. */
    void setBackupFolder (const juce::File& folder) { backupFolder = folder; }
    juce::File getBackupFolder() const;

private:
    juce::NamedValueSet kept;
    juce::File backupFolder;
};

} // namespace luthier
