#include "HostStateEnvelope.h"
#include "Diagnostics.h"

namespace luthier
{

bool HostStateEnvelope::isKnownKey (const juce::Identifier& key)
{
    // Written today, or read for sessions saved before (SM-1 moved several
    // into the preset block; they are still read at the root).
    static const char* const known[] =
    {
        "formatVersion", "preset", "ui", "lockedParameters", "slotBActive", "controllerProfile",
        "aftertouchBends", "bankSelectsPreset", "liveMode", "stability", "metronome", "clickToMain",
        "tune", "presetMorphPosition",
        "midiLearn", "routing", "modulation", "rhythm", "snapshots", "character", "toneMatch"
    };

    for (const auto* name : known)
        if (key.toString() == name)
            return true;

    return false;
}

void HostStateEnvelope::stamp (juce::DynamicObject& root) const
{
    root.setProperty ("formatVersion", kFormatVersion);

    // 4.1: what a later build wrote is written back as it came.
    for (const auto& section : kept)
        if (! root.hasProperty (section.name))
            root.setProperty (section.name, section.value);
}

juce::File HostStateEnvelope::getBackupFolder() const
{
    return backupFolder != juce::File() ? backupFolder : Diagnostics::getDiagnosticsFolder();
}

HostStateEnvelope::Reading HostStateEnvelope::read (const juce::DynamicObject& root, const juce::String& raw)
{
    Reading reading;
    reading.formatVersion = root.hasProperty ("formatVersion") ? (int) root.getProperty ("formatVersion") : 0;
    reading.newer = reading.formatVersion > kFormatVersion;

    kept.clear();

    for (const auto& property : root.getProperties())
        if (! isKnownKey (property.name))
            kept.set (property.name, property.value);

    // 4.2: an older blob is kept, as it was, before it is migrated.
    if (reading.formatVersion < kFormatVersion)
    {
        auto folder = getBackupFolder();
        folder.createDirectory();

        const auto file = folder.getNonexistentChildFile (
            "state-backup-" + juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S"), ".json", false);

        if (file.replaceWithText (raw))
        {
            reading.backedUp = true;
            reading.backup = file;
        }

        // Only the most recent few: an old session reopened every day must not
        // fill the folder.
        auto backups = folder.findChildFiles (juce::File::findFiles, false, "state-backup-*.json");

        std::sort (backups.begin(), backups.end(),
                   [] (const juce::File& a, const juce::File& b)
                   { return a.getLastModificationTime() > b.getLastModificationTime(); });

        for (int i = kMaxBackups; i < backups.size(); ++i)
            backups.getReference (i).deleteFile();
    }

    return reading;
}

} // namespace luthier
