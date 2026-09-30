#include "PresetLibraryPrefs.h"

namespace luthier
{

const char* const PresetLibraryPrefs::kMagic = "luthier.presetlibrary";

PresetLibraryPrefs& PresetLibraryPrefs::get()
{
    static PresetLibraryPrefs instance;
    return instance;
}

juce::File PresetLibraryPrefs::getDefaultFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier").getChildFile ("config").getChildFile ("preset-library.json");
}

PresetLibraryPrefs::PresetLibraryPrefs()
    : file (getDefaultFile()), entries (new juce::DynamicObject())
{
    load();
}

void PresetLibraryPrefs::setFile (const juce::File& f)
{
    file = f == juce::File() ? getDefaultFile() : f;
    reset();
    load();
}

void PresetLibraryPrefs::reset()
{
    entries = juce::var (new juce::DynamicObject());
    recent.clear();
}

bool PresetLibraryPrefs::load()
{
    if (! file.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());

    if (parsed.getProperty ("magic", {}).toString() != kMagic)
        return false;

    reset();

    if (auto* e = parsed.getProperty ("entries", {}).getDynamicObject())
        for (const auto& p : e->getProperties())
            if (auto* entry = p.value.getDynamicObject())
                entries.getDynamicObject()->setProperty (p.name, juce::var (entry->clone().release()));

    if (auto* r = parsed.getProperty ("recent", {}).getArray())
        for (const auto& k : *r)
            if (recent.size() < kMaxRecent)
                recent.add (k.toString());

    return true;
}

bool PresetLibraryPrefs::save() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("schema", 1);
    root->setProperty ("magic", kMagic);
    root->setProperty ("entries", entries);

    juce::Array<juce::var> r;

    for (const auto& k : recent)
        r.add (k);

    root->setProperty ("recent", r);

    file.getParentDirectory().createDirectory();

    // file-formats.md 13: temp file then rename.
    juce::TemporaryFile temp (file);

    if (! temp.getFile().replaceWithText (juce::JSON::toString (juce::var (root), false)))
        return false;

    return temp.overwriteTargetFileWithTemporary();
}

juce::DynamicObject* PresetLibraryPrefs::entryFor (const juce::String& key, bool create)
{
    auto* all = entries.getDynamicObject();

    if (all == nullptr || key.isEmpty())
        return nullptr;

    if (auto* existing = all->getProperty (key).getDynamicObject())
        return existing;

    if (! create)
        return nullptr;

    auto* entry = new juce::DynamicObject();
    all->setProperty (key, juce::var (entry));
    return entry;
}

const juce::DynamicObject* PresetLibraryPrefs::entryFor (const juce::String& key) const
{
    auto* all = entries.getDynamicObject();
    return all != nullptr && key.isNotEmpty() ? all->getProperty (key).getDynamicObject() : nullptr;
}

bool PresetLibraryPrefs::isFavourite (const juce::String& key) const
{
    auto* e = entryFor (key);
    return e != nullptr && (bool) e->getProperty ("favourite");
}

void PresetLibraryPrefs::setFavourite (const juce::String& key, bool favourite)
{
    if (auto* e = entryFor (key, true))
    {
        e->setProperty ("favourite", favourite);
        save();
    }
}

int PresetLibraryPrefs::getRating (const juce::String& key) const
{
    auto* e = entryFor (key);
    return e != nullptr ? juce::jlimit (0, 5, (int) e->getProperty ("rating")) : 0;
}

void PresetLibraryPrefs::setRating (const juce::String& key, int stars)
{
    if (auto* e = entryFor (key, true))
    {
        e->setProperty ("rating", juce::jlimit (0, 5, stars));
        save();
    }
}

void PresetLibraryPrefs::noteLoaded (const juce::String& key)
{
    if (auto* e = entryFor (key, true))
    {
        e->setProperty ("lastLoaded", juce::Time::getCurrentTime().toISO8601 (true));
        e->setProperty ("loadCount", (int) e->getProperty ("loadCount") + 1);

        recent.removeString (key);
        recent.insert (0, key);

        while (recent.size() > kMaxRecent)
            recent.remove (recent.size() - 1);

        save();
    }
}

int PresetLibraryPrefs::getLoadCount (const juce::String& key) const
{
    auto* e = entryFor (key);
    return e != nullptr ? (int) e->getProperty ("loadCount") : 0;
}

juce::Time PresetLibraryPrefs::getLastLoaded (const juce::String& key) const
{
    auto* e = entryFor (key);
    return e != nullptr ? juce::Time::fromISO8601 (e->getProperty ("lastLoaded").toString()) : juce::Time();
}

void PresetLibraryPrefs::rekey (const juce::String& from, const juce::String& to)
{
    auto* all = entries.getDynamicObject();

    if (all == nullptr || from.isEmpty() || to.isEmpty() || from == to)
        return;

    if (all->hasProperty (from))
    {
        all->setProperty (to, all->getProperty (from));
        all->removeProperty (from);
    }

    const int i = recent.indexOf (from);

    if (i >= 0)
        recent.set (i, to);

    save();
}

} // namespace luthier
