#include "Setlist.h"
#include "../Presets/FactoryPresets.h"   // SPEC-SWEEP: FC-1

namespace luthier
{

const char* const Setlist::kFileExtension = ".luthierset";
const char* const Setlist::kMagic = "luthier.setlist";   // SPEC-SWEEP: FF-5

//==============================================================================
juce::String SetlistEntry::getDisplayName() const
{
    if (presetPath.isEmpty())
        return "(empty)";

    return juce::File::isAbsolutePath (presetPath)
             ? juce::File (presetPath).getFileNameWithoutExtension()
             : presetPath;
}

juce::var SetlistEntry::toVar() const
{
    auto* object = new juce::DynamicObject();

    object->setProperty ("preset_path", presetPath);
    object->setProperty ("snapshot_index", snapshotIndex);
    object->setProperty ("notes", notes);

    return { object };
}

SetlistEntry SetlistEntry::fromVar (const juce::var& state)
{
    SetlistEntry entry;

    if (auto* object = state.getDynamicObject())
    {
        entry.presetPath    = object->getProperty ("preset_path").toString();
        entry.snapshotIndex = juce::jmax (0, (int) object->getProperty ("snapshot_index"));
        entry.notes         = object->getProperty ("notes").toString();
    }

    return entry;
}

//==============================================================================
Setlist::Setlist() = default;

void Setlist::clear()
{
    entries.clear();
    name = "Untitled Set";
    notes.clear();
    defaultBpm = 120.0;
}

void Setlist::setDefaultBpm (double bpm) noexcept
{
    defaultBpm = juce::jlimit (20.0, 300.0, bpm);
}

const SetlistEntry& Setlist::getEntry (int index) const noexcept
{
    static const SetlistEntry empty;

    return juce::isPositiveAndBelow (index, (int) entries.size())
             ? entries[(size_t) index] : empty;
}

void Setlist::addEntry (const SetlistEntry& entry)
{
    entries.push_back (entry);
}

bool Setlist::insertEntry (int index, const SetlistEntry& entry)
{
    if (index < 0 || index > (int) entries.size())
        return false;

    entries.insert (entries.begin() + index, entry);
    return true;
}

bool Setlist::removeEntry (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) entries.size()))
        return false;

    entries.erase (entries.begin() + index);
    return true;
}

bool Setlist::moveEntry (int fromIndex, int toIndex)
{
    if (! juce::isPositiveAndBelow (fromIndex, (int) entries.size())
          || ! juce::isPositiveAndBelow (toIndex, (int) entries.size()))
        return false;

    if (fromIndex == toIndex)
        return true;

    const auto entry = entries[(size_t) fromIndex];
    entries.erase (entries.begin() + fromIndex);
    entries.insert (entries.begin() + toIndex, entry);

    return true;
}

//==============================================================================
juce::var Setlist::toVar() const
{
    auto* root = new juce::DynamicObject();

    // SPEC-SWEEP: FF-5 - the canonical marker and schema (file-formats 0.2, 0.5).
    root->setProperty ("magic", kMagic);
    root->setProperty ("schema", kSchema);
    root->setProperty ("name", name);
    root->setProperty ("notes", notes);
    root->setProperty ("bpm_default", defaultBpm);

    juce::Array<juce::var> array;

    for (const auto& entry : entries)
        array.add (entry.toVar());

    root->setProperty ("entries", array);

    return { root };
}

void Setlist::fromVar (const juce::var& state)
{
    auto* root = state.getDynamicObject();

    if (root == nullptr)
        return;

    clear();

    name  = root->getProperty ("name").toString();
    notes = root->getProperty ("notes").toString();

    if (name.isEmpty())
        name = "Untitled Set";

    if (root->hasProperty ("bpm_default"))
        setDefaultBpm ((double) root->getProperty ("bpm_default"));

    if (const auto* array = root->getProperty ("entries").getArray())
        for (const auto& item : *array)
            entries.push_back (SetlistEntry::fromVar (item));
}

bool Setlist::loadFrom (const juce::File& file)
{
    /*  SPEC-SWEEP: FF-5, FF-35 (error-recovery 1). Each refusal says why and
        leaves the current setlist alone. */
    loadError.clear();

    if (! file.existsAsFile())
    {
        loadError = "Setlist not found: " + file.getFileName();
        return false;
    }

    const auto parsed = juce::JSON::parse (file.loadFileAsString());
    auto* root = parsed.getDynamicObject();

    if (root == nullptr)
    {
        loadError = file.getFileName() + " is not a valid Luthier file.";
        return false;
    }

    if (root->getProperty ("magic").toString() != kMagic
          && root->getProperty ("format").toString() != "luthierset")
    {
        loadError = file.getFileName() + " is not a Luthier setlist.";
        return false;
    }

    if (root->hasProperty ("schema") && (int) root->getProperty ("schema") > kSchema)
    {
        loadError = file.getFileName() + " was made by a newer Luthier version. Update to open.";
        return false;
    }

    fromVar (parsed);

    // SPEC-SWEEP: SM-31 (state-model 5.2): every entry's preset is checked now,
    // so the LIVE tab can mark the missing ones before the gig rather than at
    // the song. A renamed factory preset still counts as found.
    for (auto& entry : entries)
    {
        const juce::File preset (juce::File::isAbsolutePath (entry.presetPath) ? juce::File (entry.presetPath)
                                                                                : juce::File());
        entry.resolved = preset.existsAsFile()
                           || (preset != juce::File()
                                 && preset.getSiblingFile (FactoryPresets::renamedPreset (preset.getFileNameWithoutExtension())
                                                             + preset.getFileExtension()).existsAsFile());
    }

    return true;
}

int Setlist::getNumUnresolvedEntries() const noexcept
{
    int count = 0;

    for (const auto& entry : entries)
        if (! entry.resolved)
            ++count;

    return count;
}

bool Setlist::saveTo (const juce::File& file) const
{
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (juce::JSON::toString (toVar(), false));
}

juce::File Setlist::getUserDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("Setlists");
}

juce::Array<juce::File> Setlist::findSetlists()
{
    juce::Array<juce::File> found;

    const auto directory = getUserDirectory();

    if (! directory.isDirectory())
        return found;

    for (const auto& entry : juce::RangedDirectoryIterator (directory, false,
                                                            juce::String ("*") + kFileExtension))
        found.add (entry.getFile());

    return found;
}

//==============================================================================
SetlistPlayer::SetlistPlayer() = default;

void SetlistPlayer::setSetlist (const Setlist& s)
{
    setlist = s;
    position = 0;
    preloadedIndex = -1;

    currentEntryData = juce::var();
    nextEntryData = juce::var();
    lastError.clear();

    if (setlist.getNumEntries() > 0)
    {
        currentEntryData = readEntry (0);
        preloadNext();
    }
}

const SetlistEntry* SetlistPlayer::getCurrentEntry() const noexcept
{
    return juce::isPositiveAndBelow (position, setlist.getNumEntries())
             ? &setlist.getEntry (position) : nullptr;
}

const SetlistEntry* SetlistPlayer::getPreviousEntry() const noexcept
{
    return (position > 0) ? &setlist.getEntry (position - 1) : nullptr;
}

const SetlistEntry* SetlistPlayer::getNextEntry() const noexcept
{
    return (position + 1 < setlist.getNumEntries())
             ? &setlist.getEntry (position + 1) : nullptr;
}

juce::var SetlistPlayer::readEntry (int index)
{
    if (! juce::isPositiveAndBelow (index, setlist.getNumEntries()))
        return {};

    const auto& entry = setlist.getEntry (index);
    juce::File file (juce::File::isAbsolutePath (entry.presetPath) ? juce::File (entry.presetPath) : juce::File());

    // SPEC-SWEEP: FC-1 - a factory preset renamed in the trademark sweep is
    // found under its new name beside where the old one was.
    if (! file.existsAsFile() && file != juce::File())
    {
        const auto renamed = FactoryPresets::renamedPreset (file.getFileNameWithoutExtension());

        if (renamed != file.getFileNameWithoutExtension())
            file = file.getSiblingFile (renamed + file.getFileExtension());
    }

    if (! file.existsAsFile())
    {
        lastError = "Preset not found: " + entry.presetPath;
        return {};
    }

    const auto parsed = juce::JSON::parse (file.loadFileAsString());

    if (parsed.getDynamicObject() == nullptr)
    {
        lastError = "Preset could not be read: " + entry.presetPath;
        return {};
    }

    lastError.clear();
    return parsed;
}

bool SetlistPlayer::goTo (int index)
{
    if (! juce::isPositiveAndBelow (index, setlist.getNumEntries()))
        return false;

    // The common case is stepping onto the entry that was pre-loaded, and that
    // case must not touch the disk at all.
    if (index == preloadedIndex && nextEntryData.getDynamicObject() != nullptr)
    {
        currentEntryData = nextEntryData;
        nextEntryData = juce::var();
        preloadedIndex = -1;
    }
    else
    {
        currentEntryData = readEntry (index);
    }

    position = index;
    return true;
}

bool SetlistPlayer::next()
{
    return goTo (position + 1);
}

bool SetlistPlayer::previous()
{
    return goTo (position - 1);
}

void SetlistPlayer::preloadNext()
{
    const int index = position + 1;

    if (! juce::isPositiveAndBelow (index, setlist.getNumEntries()))
    {
        nextEntryData = juce::var();
        preloadedIndex = -1;
        return;
    }

    if (preloadedIndex == index)
        return;

    nextEntryData = readEntry (index);
    preloadedIndex = index;
}

} // namespace luthier
