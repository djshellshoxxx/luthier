/*  The host state blob's format version: host-integration.md 4, 4.1, 4.2
    (HI-20/24/25); error-recovery.md 0.2, 0.3. One implementation, used by
    getStateInformation / restoreState.

    - Every blob carries `formatVersion` (kCurrentStateFormatVersion) and
      `savedBy` (the build that wrote it).
    - A root-level key this build does not recognise is kept and written back
      unchanged (getUnknownHostSections). A blob from a newer format raises one
      notice, and a preset block this build refused (a newer preset schema) is
      written back until the user loads another preset.
    - An older blob (no `formatVersion` = 0) is copied to the diagnostics folder
      as `state-backup-<stamp>.json` before it is migrated.
    - A blob that is not a Luthier state at all changes nothing, is kept there
      as `state-backup-<stamp>-unreadable.json`, and is reported.
    The newest 20 backups are kept.

    Message thread (the host's state calls), like the rest of restoreState.
*/

#include "PluginProcessor.h"
#include "Support/Diagnostics.h"
#include "Support/ErrorLog.h"

#include <set>

namespace luthier
{

namespace
{
    constexpr int kMaxStateBackups = 20;

    bool isKnownRootKey (const juce::Identifier& key)
    {
        static const std::set<juce::String> known
        {
            "preset", "midiLearn", "ui", "setlist", "lockedParameters", "slotBActive",
            "routing", "modulation", "rhythm", "snapshots", "liveMode", "controllerProfile",
            "character", "stability", "toneMatch", "metronome", "clickToMain", "normalization",
            "tune", "presetMorphPosition", "formatVersion", "savedBy",
            "aftertouchBends", "bankSelectsPreset",
            "jamPlayValue", "jamFillNowValue"   // jam-mode 10 (FEAT-JAM)
        };

        return known.count (key.toString()) > 0;
    }

    void pruneStateBackups (const juce::File& folder)
    {
        auto files = folder.findChildFiles (juce::File::findFiles, false, "state-backup-*.json");

        if (files.size() <= kMaxStateBackups)
            return;

        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
                   { return a.getLastModificationTime() < b.getLastModificationTime(); });

        for (int i = 0; i < files.size() - kMaxStateBackups; ++i)
            files.getReference (i).deleteFile();
    }

    juce::File backUpBlob (const void* data, int sizeInBytes, const juce::String& suffix)
    {
        auto folder = LuthierAudioProcessor::getStateBackupFolder();

        if (! folder.createDirectory())
            return {};

        const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
        auto file = folder.getNonexistentChildFile ("state-backup-" + stamp + suffix, ".json", false);

        if (! file.replaceWithData (data, (size_t) sizeInBytes))
            return {};

        pruneStateBackups (folder);
        return file;
    }
}

//==============================================================================
juce::File LuthierAudioProcessor::getStateBackupFolder()
{
    return Diagnostics::getDiagnosticsFolder();
}

juce::String LuthierAudioProcessor::stateBlobToText (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return {};

    auto* bytes = static_cast<const char*> (data);

    // A host that stores the terminator hands it back too.
    while (sizeInBytes > 0 && bytes[sizeInBytes - 1] == 0)
        --sizeInBytes;

    if (! juce::CharPointer_UTF8::isValidString (bytes, sizeInBytes))
        return {};

    return juce::String::fromUTF8 (bytes, sizeInBytes);
}

//==============================================================================
void LuthierAudioProcessor::writeStateFormat (juce::DynamicObject& root) const
{
    root.setProperty ("formatVersion", (int) kCurrentStateFormatVersion);
    root.setProperty ("savedBy", JucePlugin_VersionString);

    // HI-24: whatever root-level section this build did not recognise on load
    // goes back out unchanged (keys written above win).
    for (const auto& section : unknownHostSections)
        if (! root.hasProperty (section.name))
            root.setProperty (section.name, section.value);

    if (! refusedPresetBlock.isVoid())
        root.setProperty ("preset", refusedPresetBlock);
}

void LuthierAudioProcessor::readStateFormat (const juce::DynamicObject& root, const void* data, int sizeInBytes)
{
    const auto versionValue = root.getProperty ("formatVersion");
    const int formatVersion = versionValue.isVoid() ? 0 : (int) versionValue;
    const auto savedBy = root.getProperty ("savedBy").toString();

    refusedPresetBlock = juce::var();
    unknownHostSections.clear();

    for (const auto& section : root.getProperties())
        if (! isKnownRootKey (section.name))
            unknownHostSections.set (section.name, section.value);

    if (formatVersion < kCurrentStateFormatVersion)
    {
        // HI-25: "back up the old blob to the diagnostics folder before overwriting".
        const auto backup = backUpBlob (data, sizeInBytes, {});

        ErrorLog::write (ErrorLog::Severity::info, "HostState", "OLDER_STATE_MIGRATED",
                         "Session state from an older format was migrated; the original was kept",
                         [&]
                         {
                             auto* context = new juce::DynamicObject();
                             context->setProperty ("format_version", formatVersion);
                             context->setProperty ("backup", backup.getFullPathName());
                             return juce::var (context);
                         }());
    }
    else if (formatVersion > kCurrentStateFormatVersion)
    {
        // HI-24: "warn the user" - once.
        ErrorLog::write (ErrorLog::Severity::warn, "HostState", "NEWER_STATE_FORMAT",
                         "Session state was saved by a newer version of Luthier",
                         [&]
                         {
                             auto* context = new juce::DynamicObject();
                             context->setProperty ("format_version", formatVersion);
                             context->setProperty ("supported_format", kCurrentStateFormatVersion);
                             context->setProperty ("saved_by", savedBy);
                             context->setProperty ("kept_sections", unknownHostSections.size());
                             return juce::var (context);
                         }());

        guitarNotices.addIfNotAlreadyThere ("This session was saved by a newer version of Luthier"
                                            + (savedBy.isNotEmpty() ? " (" + savedBy + ")" : juce::String())
                                            + ". Some settings may not carry over; the ones this version "
                                              "does not know are kept.");
    }
}

void LuthierAudioProcessor::noteRestoredPresetBlock (const juce::var& block, bool loaded)
{
    if (loaded)
        return;

    const auto reason = presets.getLastRefusal();

    // A newer preset schema: the sound is that build's, so it goes back out as
    // it came rather than being replaced by whatever this build falls back to.
    if (reason.contains ("newer"))
        refusedPresetBlock = block;

    ErrorLog::write (ErrorLog::Severity::warn, "HostState", "SESSION_PRESET_REFUSED",
                     "The session's preset block was refused; the current sound was kept",
                     [&]
                     {
                         auto* context = new juce::DynamicObject();
                         context->setProperty ("reason", reason);
                         return juce::var (context);
                     }());

    stateWarnings.addIfNotAlreadyThere ("The project's sound could not be loaded"
                                        + (reason.isNotEmpty() ? ": it " + reason : juce::String (".")));
}

void LuthierAudioProcessor::reportUnreadableState (const void* data, int sizeInBytes)
{
    const auto backup = backUpBlob (data, sizeInBytes, "-unreadable");

    ErrorLog::write (ErrorLog::Severity::error, "HostState", "STATE_UNREADABLE",
                     "The host's saved state is not a Luthier session; nothing was changed",
                     [&]
                     {
                         auto* context = new juce::DynamicObject();
                         context->setProperty ("bytes", sizeInBytes);
                         context->setProperty ("backup", backup.getFullPathName());
                         return juce::var (context);
                     }());

    stateWarnings.addIfNotAlreadyThere ("The project's saved Luthier state could not be read, so the "
                                        "current settings were kept. A copy is in the diagnostics folder.");
}

} // namespace luthier
