/*  SPEC-SWEEP HI-20/24/25: the host state blob's format version
    (host-integration.md 4, 4.1, 4.2; error-recovery.md 0.2, 0.3).

    - Every blob carries `stateFormat` (kStateFormatVersion) and `savedBy`.
    - A newer blob: the sections this build does not read are kept and written
      back unchanged, a preset block it refused (a newer preset schema) is
      written back until the user loads another preset, and the user is told.
    - An older blob (no `stateFormat` = 0): copied to the diagnostics folder
      before it is migrated, so the project can be taken back to the build that
      made it.
    - A blob that is not a Luthier state at all: kept there too, and said.

    Message thread (the host's state calls), like the rest of restoreState.
*/

#include "PluginProcessor.h"
#include "Support/ErrorLog.h"

namespace luthier
{

namespace
{
    // The top-level keys getStateInformation writes or restoreState reads;
    // anything else in a newer blob is a section this build does not know.
    const char* const kKnownStateKeys[] =
    {
        "preset", "midiLearn", "ui", "setlist", "lockedParameters", "slotBActive", "controllerProfile",
        "aftertouchBends", "bankSelectsPreset", "liveMode", "stability", "metronome", "clickToMain",
        "normalization", "tune", "presetMorphPosition", "stateFormat", "savedBy",
        // read only, from sessions saved before the preset blocks (SM-1)
        "routing", "modulation", "rhythm", "snapshots", "character", "toneMatch"
    };

    bool isKnownStateKey (const juce::Identifier& key)
    {
        for (const auto* known : kKnownStateKeys)
            if (key.toString() == known)
                return true;

        return false;
    }

    constexpr int kMaxStateBackups = 20;

    void pruneStateBackups (const juce::File& folder)
    {
        auto files = folder.findChildFiles (juce::File::findFiles, false, "session-state-*.json");

        if (files.size() <= kMaxStateBackups)
            return;

        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
                   { return a.getLastModificationTime() < b.getLastModificationTime(); });

        for (int i = 0; i < files.size() - kMaxStateBackups; ++i)
            files.getReference (i).deleteFile();
    }

    juce::File backUpBlob (const void* data, int sizeInBytes, const juce::String& why)
    {
        auto folder = LuthierAudioProcessor::getStateBackupFolder();

        if (! folder.createDirectory())
            return {};

        const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
        auto file = folder.getNonexistentChildFile ("session-state-" + stamp + "-" + why, ".json", false);

        if (! file.replaceWithData (data, (size_t) sizeInBytes))
            return {};

        pruneStateBackups (folder);
        return file;
    }
}

//==============================================================================
juce::File LuthierAudioProcessor::getStateBackupFolder()
{
    // The error log's folder, so a test that points the log elsewhere moves this too.
    return ErrorLog::getFolder().getChildFile ("StateBackups");
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
    root.setProperty ("stateFormat", kStateFormatVersion);
    root.setProperty ("savedBy", JucePlugin_VersionString);

    // host-integration 4.1: "save preserves the unknown sections on write-back".
    if (auto* sections = newerStateSections.getDynamicObject())
        for (const auto& section : sections->getProperties())
            if (! root.hasProperty (section.name))
                root.setProperty (section.name, section.value);

    if (! refusedPresetBlock.isVoid())
        root.setProperty ("preset", refusedPresetBlock);
}

void LuthierAudioProcessor::readStateFormat (const juce::DynamicObject& root, const void* data, int sizeInBytes)
{
    const auto formatValue = root.getProperty ("stateFormat");
    const int format = formatValue.isVoid() ? 0 : (int) formatValue;

    newerStateSections = juce::var();
    refusedPresetBlock = juce::var();

    if (format > kStateFormatVersion)
    {
        auto* kept = new juce::DynamicObject();

        for (const auto& section : root.getProperties())
            if (! isKnownStateKey (section.name))
                kept->setProperty (section.name, section.value);

        newerStateSections = juce::var (kept);

        const auto savedBy = root.getProperty ("savedBy").toString();

        ErrorLog::write (ErrorLog::Severity::warn, "HostState", "NEWER_STATE_FORMAT",
                         "Session state was saved by a newer version of Luthier",
                         [&]
                         {
                             auto* context = new juce::DynamicObject();
                             context->setProperty ("state_format", format);
                             context->setProperty ("supported_format", kStateFormatVersion);
                             context->setProperty ("saved_by", savedBy);
                             context->setProperty ("kept_sections", kept->getProperties().size());
                             return juce::var (context);
                         }());

        stateWarnings.addIfNotAlreadyThere ("This project was saved by a newer Luthier"
                                            + (savedBy.isNotEmpty() ? " (" + savedBy + ")" : juce::String())
                                            + ". Settings this version does not know are kept but not used; "
                                              "update Luthier to use them.");
    }
    else if (format < kStateFormatVersion)
    {
        // host-integration 4.2: "back up the old blob to the diagnostics folder
        // before overwriting".
        const auto backup = backUpBlob (data, sizeInBytes, "format" + juce::String (format));

        ErrorLog::write (ErrorLog::Severity::info, "HostState", "OLDER_STATE_MIGRATED",
                         "Session state from an older format was migrated; the original was kept",
                         [&]
                         {
                             auto* context = new juce::DynamicObject();
                             context->setProperty ("state_format", format);
                             context->setProperty ("backup", backup.getFullPathName());
                             return juce::var (context);
                         }());
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
    const auto backup = backUpBlob (data, sizeInBytes, "unreadable");

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
