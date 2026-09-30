#include "RiffLibrary.h"

#include "../Support/IrLibrary.h"

#include <juce_events/juce_events.h>

#include <algorithm>
#include <cmath>

namespace luthier
{

//==============================================================================
juce::String RiffIndexEntry::describe() const
{
    juce::StringArray parts;
    parts.add (name);
    parts.add (type);

    if (genreIndex >= 0)
        parts.add (RiffVocabulary::genres()[(size_t) genreIndex].displayName);

    parts.add (keyRoot + " " + RiffVocabulary::scaleDisplayName (scale));
    parts.add (juce::String (juce::roundToInt (tempoBpm)) + " bpm");
    parts.add ("difficulty " + juce::String (difficulty) + " of 5");

    for (const auto& t : techniques)
        parts.add (RiffVocabulary::techniqueDisplayName (t).toLowerCase());

    return parts.joinIntoString (", ");
}

void RiffIndexEntry::finalise()
{
    genreIndex = RiffVocabulary::indexOfGenre (genre);
    typeIndex = RiffVocabulary::types().indexOf (type);
    instrumentIndex = RiffVocabulary::instruments().indexOf (instrument);
    rootPitchClass = RiffVocabulary::pitchClassOfRoot (keyRoot);

    techniqueMask.reset();

    for (const auto& t : techniques)
    {
        const int bit = RiffVocabulary::allTechniques().indexOf (t);

        if (bit >= 0 && bit < 64)
            techniqueMask.set ((size_t) bit);
    }

    juce::String text = name + " " + tags.joinIntoString (" ") + " " + genre;

    if (genreIndex >= 0)
        text << " " << RiffVocabulary::genres()[(size_t) genreIndex].displayName;

    searchText = text.toLowerCase();
}

RiffIndexEntry RiffIndexEntry::fromRiff (const Riff& riff, const juce::File& file, bool factory)
{
    RiffIndexEntry e;
    e.id = riff.meta.id;
    e.name = riff.meta.name;
    e.type = riff.type;
    e.genre = riff.genre;
    e.instrument = riff.instrument;
    e.tuningName = riff.tuningName;
    e.tuning = riff.tuning;
    e.capo = riff.capo;
    e.keyRoot = riff.keyRoot;
    e.scale = riff.scale;
    e.feel = riff.feel;
    e.author = riff.meta.author;
    e.origin = riff.meta.origin;
    e.tags = riff.meta.tags;
    e.techniques = riff.computeTechniques();   // 3: filtering uses the recomputed set
    e.tempoBpm = riff.tempoBpm;
    e.lengthBeats = riff.lengthBeats;
    e.meterNumerator = riff.meterNumerator;
    e.meterDenominator = riff.meterDenominator;
    e.difficulty = riff.difficulty;
    e.numNotes = (int) riff.notes.size();
    e.factory = factory;
    e.file = file;
    e.finalise();
    return e;
}

//==============================================================================
namespace
{
    juce::var bitsToVar (const std::bitset<64>& bits)
    {
        juce::Array<juce::var> a;

        for (size_t i = 0; i < bits.size(); ++i)
            if (bits.test (i))
                a.add ((int) i);

        return a;
    }

    std::bitset<64> bitsFromVar (const juce::var& v)
    {
        std::bitset<64> bits;

        if (auto* a = v.getArray())
            for (const auto& e : *a)
                if (juce::isPositiveAndBelow ((int) e, 64))
                    bits.set ((size_t) (int) e);

        return bits;
    }
}

juce::var RiffQuery::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("text", text);
    o->setProperty ("genres", bitsToVar (genres));
    o->setProperty ("types", bitsToVar (types));
    o->setProperty ("instruments", bitsToVar (instruments));
    o->setProperty ("techniques", bitsToVar (techniques));
    o->setProperty ("minDifficulty", minDifficulty);
    o->setProperty ("maxDifficulty", maxDifficulty);
    o->setProperty ("minTempo", minTempo);
    o->setProperty ("maxTempo", maxTempo);
    o->setProperty ("keyRoot", keyRoot);
    o->setProperty ("favouritesOnly", favouritesOnly);
    o->setProperty ("userOnly", userOnly);
    o->setProperty ("fitsInstrument", fitsInstrument);
    o->setProperty ("sort", (int) sort);
    return juce::var (o);
}

RiffQuery RiffQuery::fromVar (const juce::var& v)
{
    RiffQuery q;

    if (v.getDynamicObject() == nullptr)
        return q;

    q.text = v["text"].toString();
    q.genres = bitsFromVar (v["genres"]);
    q.types = bitsFromVar (v["types"]);
    q.instruments = bitsFromVar (v["instruments"]);
    q.techniques = bitsFromVar (v["techniques"]);
    q.minDifficulty = juce::jlimit (1, 5, (int) v.getProperty ("minDifficulty", 1));
    q.maxDifficulty = juce::jlimit (q.minDifficulty, 5, (int) v.getProperty ("maxDifficulty", 5));
    q.minTempo = juce::jlimit (0.0, 1000.0, (double) v.getProperty ("minTempo", 0.0));
    q.maxTempo = juce::jlimit (q.minTempo, 1000.0, (double) v.getProperty ("maxTempo", 1000.0));
    q.keyRoot = juce::jlimit (-1, 11, (int) v.getProperty ("keyRoot", -1));
    q.favouritesOnly = (bool) v.getProperty ("favouritesOnly", false);
    q.userOnly = (bool) v.getProperty ("userOnly", false);
    q.fitsInstrument = (bool) v.getProperty ("fitsInstrument", true);
    q.sort = (Sort) juce::jlimit (0, (int) Sort::recent, (int) v.getProperty ("sort", 0));
    return q;
}

//==============================================================================
RiffLibrary::RiffLibrary()
    : factoryFolder (getDefaultFactoryFolder()),
      userFolder (getDefaultUserFolder()),
      globalFile (getDefaultGlobalFile())
{
}

RiffLibrary::~RiffLibrary() = default;

juce::File RiffLibrary::getDefaultFactoryFolder()
{
    const auto resources = IrLibrary::getResourcesFolder();
    return resources == juce::File() ? juce::File() : resources.getChildFile ("Riffs");
}

juce::File RiffLibrary::getRiffsRoot()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier").getChildFile ("Riffs");
}

juce::File RiffLibrary::getDefaultUserFolder() { return getRiffsRoot().getChildFile ("User"); }
juce::File RiffLibrary::getDragFolder()        { return getRiffsRoot().getChildFile ("Drag"); }
juce::File RiffLibrary::getDefaultGlobalFile() { return getRiffsRoot().getChildFile ("library.json"); }

void RiffLibrary::setFolders (const juce::File& factory, const juce::File& user, const juce::File& global)
{
    factoryFolder = factory;
    userFolder = user;
    globalFile = global;
    loaded = false;
    globalLoaded = false;
    cache.clear();
}

//==============================================================================
RiffLibrary::IndexData RiffLibrary::buildIndex (const juce::File& factory, const juce::File& user)
{
    IndexData data;

    // ---- factory: the catalog, or a scan when it is missing (15) -------------
    const auto catalogFile = factory.getChildFile ("catalog.json");
    bool haveCatalog = false;

    if (factory != juce::File() && catalogFile.existsAsFile())
    {
        const auto root = juce::JSON::parse (catalogFile.loadFileAsString());

        if (root["magic"].toString() == "luthier.riffcatalog")
        {
            if (auto* items = root["items"].getArray())
            {
                haveCatalog = true;
                data.entries.reserve ((size_t) items->size());

                for (const auto& item : *items)
                {
                    RiffIndexEntry e;
                    e.id = item["id"].toString();
                    e.name = item["name"].toString();
                    e.type = item["type"].toString();
                    e.genre = item["genre"].toString();
                    e.instrument = item["instrument"].toString();
                    e.tuningName = item["tuning_name"].toString();

                    if (auto* t = item["tuning"].getArray())
                        for (const auto& n : *t)
                            e.tuning.push_back ((int) n);

                    e.capo = (int) item["capo"];
                    e.keyRoot = item["key"]["root"].toString();
                    e.scale = item["key"]["scale"].toString();
                    e.tempoBpm = (double) item["tempo_bpm"];
                    e.lengthBeats = (double) item["length_beats"];

                    if (auto* m = item["meter"].getArray(); m != nullptr && m->size() == 2)
                    {
                        e.meterNumerator = (int) m->getReference (0);
                        e.meterDenominator = (int) m->getReference (1);
                    }

                    e.feel = item["feel"].toString();
                    e.difficulty = juce::jlimit (1, 5, (int) item["difficulty"]);

                    if (auto* t = item["techniques"].getArray())
                        for (const auto& s : *t)
                            e.techniques.add (s.toString());

                    if (auto* t = item["tags"].getArray())
                        for (const auto& s : *t)
                            e.tags.add (s.toString());

                    e.free = (bool) item["free"];
                    e.author = item["author"].toString();
                    e.origin = item["origin"].toString();
                    e.numNotes = (int) item["notes"];
                    e.factory = true;
                    e.file = factory.getChildFile (item["file"].toString());

                    if (e.id.isNotEmpty() && e.name.isNotEmpty())
                    {
                        e.finalise();
                        data.entries.push_back (std::move (e));
                    }
                }
            }
        }
    }

    if (! haveCatalog)
    {
        const auto files = factory == juce::File() ? juce::Array<juce::File>()
                                                    : factory.findChildFiles (juce::File::findFiles, true, "*.luthierriff");

        if (files.isEmpty())
        {
            data.factoryMissing = true;
        }
        else
        {
            data.diagnostics.add ("Riffs: catalog.json missing; rebuilt the factory index by scanning "
                                  + juce::String (files.size()) + " files");

            for (const auto& f : files)
            {
                Riff riff;

                if (Riff::loadFromFile (f, riff).wasOk())
                    data.entries.push_back (RiffIndexEntry::fromRiff (riff, f, true));
                else
                    data.unreadable.add (f.getFullPathName());
            }
        }
    }

    // ---- user riffs ---------------------------------------------------------------
    if (user != juce::File() && user.isDirectory())
    {
        auto files = user.findChildFiles (juce::File::findFiles, true, "*.luthierriff");
        files.sort();

        for (const auto& f : files)
        {
            Riff riff;
            const auto result = Riff::loadFromFile (f, riff);

            if (result.wasOk())
                data.entries.push_back (RiffIndexEntry::fromRiff (riff, f, false));
            else
                data.unreadable.add (f.getFullPathName());   // left untouched (file-formats 14)
        }
    }

    return data;
}

void RiffLibrary::applyIndex (IndexData data)
{
    entries = std::move (data.entries);
    factoryMissing = data.factoryMissing;
    unreadable = std::move (data.unreadable);
    diagnostics = std::move (data.diagnostics);
    loaded = true;
    loading = false;
    cache.clear();
    rebuildLookup();

    if (! globalLoaded)
        loadGlobal();

    if (onIndexChanged)
        onIndexChanged();
}

void RiffLibrary::rebuildLookup()
{
    byId.clear();

    for (size_t i = 0; i < entries.size(); ++i)
        byId[entries[i].id] = (int) i;   // a user riff with a factory id cannot shadow: ids differ by prefix
}

void RiffLibrary::loadIndexNow()
{
    applyIndex (buildIndex (factoryFolder, userFolder));
}

void RiffLibrary::loadIndexAsync (juce::ThreadPool& pool)
{
    if (loaded || loading)
        return;

    loading = true;

    juce::WeakReference<RiffLibrary> weak (this);
    const auto factory = factoryFolder;
    const auto user = userFolder;

    pool.addJob ([weak, factory, user]
    {
        auto data = std::make_shared<IndexData> (buildIndex (factory, user));

        juce::MessageManager::callAsync ([weak, data]
        {
            if (auto* library = weak.get())
                library->applyIndex (std::move (*data));
        });
    });
}

const RiffIndexEntry* RiffLibrary::getEntry (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, (int) entries.size()) ? &entries[(size_t) index] : nullptr;
}

int RiffLibrary::indexOf (const juce::String& id) const
{
    const auto it = byId.find (id);
    return it != byId.end() ? it->second : -1;
}

const RiffIndexEntry* RiffLibrary::findEntry (const juce::String& id) const
{
    return getEntry (indexOf (id));
}

int RiffLibrary::getNumFactory() const noexcept
{
    int n = 0;

    for (const auto& e : entries)
        n += e.factory ? 1 : 0;

    return n;
}

//==============================================================================
bool RiffLibrary::matches (const RiffIndexEntry& e, const RiffQuery& q) const
{
    if (q.genres.any() && (e.genreIndex < 0 || ! q.genres.test ((size_t) e.genreIndex)))
        return false;

    if (q.types.any() && (e.typeIndex < 0 || ! q.types.test ((size_t) e.typeIndex)))
        return false;

    if (q.instruments.any() && (e.instrumentIndex < 0 || ! q.instruments.test ((size_t) e.instrumentIndex)))
        return false;

    if ((e.techniqueMask & q.techniques) != q.techniques)
        return false;

    if (e.difficulty < q.minDifficulty || e.difficulty > q.maxDifficulty)
        return false;

    if (e.tempoBpm < q.minTempo || e.tempoBpm > q.maxTempo)
        return false;

    if (q.keyRoot >= 0 && e.rootPitchClass != q.keyRoot)
        return false;

    if (q.userOnly && e.factory)
        return false;

    if (q.favouritesOnly && ! isFavourite (e.id))
        return false;

    if (q.fitsInstrument && (e.isBass() != q.instrument.isBass || (int) e.tuning.size() > q.instrument.numStrings))
        return false;

    if (q.text.isNotEmpty())
    {
        for (const auto& word : juce::StringArray::fromTokens (q.text.toLowerCase(), " \t", ""))
            if (word.isNotEmpty() && ! e.searchText.contains (word))
                return false;
    }

    return true;
}

std::vector<int> RiffLibrary::query (const RiffQuery& q) const
{
    std::vector<int> result;
    result.reserve (entries.size());

    for (size_t i = 0; i < entries.size(); ++i)
        if (matches (entries[i], q))
            result.push_back ((int) i);

    auto byName = [this] (int a, int b)
    {
        const int c = entries[(size_t) a].name.compareNatural (entries[(size_t) b].name);
        return c != 0 ? c < 0 : entries[(size_t) a].id < entries[(size_t) b].id;
    };

    switch (q.sort)
    {
        case RiffQuery::Sort::name:
            std::sort (result.begin(), result.end(), byName);
            break;

        case RiffQuery::Sort::tempo:
            std::sort (result.begin(), result.end(), [&] (int a, int b)
            {
                const auto ta = entries[(size_t) a].tempoBpm, tb = entries[(size_t) b].tempoBpm;
                return ta != tb ? ta < tb : byName (a, b);
            });
            break;

        case RiffQuery::Sort::difficulty:
            std::sort (result.begin(), result.end(), [&] (int a, int b)
            {
                const auto da = entries[(size_t) a].difficulty, db = entries[(size_t) b].difficulty;
                return da != db ? da < db : byName (a, b);
            });
            break;

        case RiffQuery::Sort::key:
            std::sort (result.begin(), result.end(), [&] (int a, int b)
            {
                const auto ka = entries[(size_t) a].rootPitchClass, kb = entries[(size_t) b].rootPitchClass;
                return ka != kb ? ka < kb : byName (a, b);
            });
            break;

        case RiffQuery::Sort::recent:
            std::sort (result.begin(), result.end(), [&] (int a, int b)
            {
                int ra = recents.indexOf (entries[(size_t) a].id), rb = recents.indexOf (entries[(size_t) b].id);
                ra = ra < 0 ? 1 << 20 : ra;
                rb = rb < 0 ? 1 << 20 : rb;
                return ra != rb ? ra < rb : byName (a, b);
            });
            break;

        case RiffQuery::Sort::genreThenName:
        default:
            std::sort (result.begin(), result.end(), [&] (int a, int b)
            {
                const auto ga = entries[(size_t) a].genreIndex, gb = entries[(size_t) b].genreIndex;
                return ga != gb ? ga < gb : byName (a, b);
            });
            break;
    }

    return result;
}

//==============================================================================
std::shared_ptr<const Riff> RiffLibrary::getRiff (const juce::String& id)
{
    for (auto it = cache.begin(); it != cache.end(); ++it)
    {
        if (it->first == id)
        {
            cache.splice (cache.begin(), cache, it);
            return cache.front().second;
        }
    }

    const auto* entry = findEntry (id);

    if (entry == nullptr)
        return nullptr;

    auto riff = std::make_shared<Riff>();
    juce::StringArray warnings;

    if (Riff::loadFromFile (entry->file, *riff, &warnings).failed())
    {
        unreadable.addIfNotAlreadyThere (entry->file.getFullPathName());
        return nullptr;
    }

    for (const auto& w : warnings)
        diagnostics.addIfNotAlreadyThere (w);

    cache.emplace_front (id, riff);

    while ((int) cache.size() > kCacheSize)
        cache.pop_back();

    return riff;
}

//==============================================================================
juce::String RiffLibrary::makeUserId (const juce::String& type)
{
    return "user." + (RiffVocabulary::types().contains (type) ? type : juce::String ("lick")) + "."
             + juce::Uuid().toDashedString();
}

juce::Result RiffLibrary::saveUserRiff (Riff& riff, juce::File* written)
{
    if (riff.meta.name.trim().isEmpty())
        return juce::Result::fail ("A riff needs a name");

    if (! riff.meta.id.startsWith ("user."))
        riff.meta.id = makeUserId (riff.type);

    const auto now = juce::Time::getCurrentTime().toISO8601 (true);

    if (riff.meta.created.isEmpty())
        riff.meta.created = now;

    riff.meta.modified = now;

    if (riff.meta.author.isEmpty() || riff.meta.author == "Factory")
        riff.meta.author = juce::SystemStats::getFullUserName().isNotEmpty() ? juce::SystemStats::getFullUserName()
                                                                           : juce::String ("User");

    if (riff.meta.origin != "import")
        riff.meta.origin = "capture";

    if (riff.meta.versionCreated.isEmpty())
        riff.meta.versionCreated = JucePlugin_VersionString;

    riff.meta.versionModified = JucePlugin_VersionString;
    riff.techniques = riff.computeTechniques();

    const auto file = userFolder.getChildFile (riff.meta.id + ".luthierriff");
    const auto result = riff.saveToFile (file);

    if (result.failed())
        return result;

    if (written != nullptr)
        *written = file;

    // Index it now, without rescanning.
    auto entry = RiffIndexEntry::fromRiff (riff, file, false);
    const int existing = indexOf (riff.meta.id);

    if (existing >= 0)
        entries[(size_t) existing] = std::move (entry);
    else
        entries.push_back (std::move (entry));

    cache.remove_if ([&riff] (const auto& p) { return p.first == riff.meta.id; });
    rebuildLookup();
    loaded = true;

    if (onIndexChanged)
        onIndexChanged();

    return juce::Result::ok();
}

juce::Result RiffLibrary::deleteUserRiff (const juce::String& id)
{
    const int index = indexOf (id);

    if (index < 0)
        return juce::Result::fail ("No such riff");

    const auto& entry = entries[(size_t) index];

    if (entry.factory)
        return juce::Result::fail ("Factory riffs cannot be deleted");

    if (entry.file.existsAsFile() && ! entry.file.moveToTrash() && ! entry.file.deleteFile())
        return juce::Result::fail ("Could not move " + entry.file.getFullPathName() + " to the trash");

    entries.erase (entries.begin() + index);
    cache.remove_if ([&id] (const auto& p) { return p.first == id; });
    favourites.removeString (id);
    recents.removeString (id);
    playCounts.erase (id);
    rebuildLookup();
    saveGlobal();

    if (onIndexChanged)
        onIndexChanged();

    return juce::Result::ok();
}

//==============================================================================
bool RiffLibrary::isFavourite (const juce::String& id) const
{
    return favourites.contains (id);
}

void RiffLibrary::setFavourite (const juce::String& id, bool favourite)
{
    if (favourite)
        favourites.addIfNotAlreadyThere (id);
    else
        favourites.removeString (id);

    saveGlobal();
}

void RiffLibrary::notePlayed (const juce::String& id)
{
    recents.removeString (id);
    recents.insert (0, id);

    while (recents.size() > kMaxRecents)
        recents.remove (recents.size() - 1);

    ++playCounts[id];
    saveGlobal();
}

int RiffLibrary::getPlayCount (const juce::String& id) const
{
    const auto it = playCounts.find (id);
    return it != playCounts.end() ? it->second : 0;
}

void RiffLibrary::loadGlobal()
{
    globalLoaded = true;
    favourites.clear();
    recents.clear();
    playCounts.clear();

    if (! globalFile.existsAsFile())
        return;

    const auto root = juce::JSON::parse (globalFile.loadFileAsString());

    if (auto* f = root["favourites"].getArray())
        for (const auto& id : *f)
            favourites.addIfNotAlreadyThere (id.toString());

    if (auto* r = root["recents"].getArray())
        for (const auto& id : *r)
            if (recents.size() < kMaxRecents)
                recents.addIfNotAlreadyThere (id.toString());

    if (auto* counts = root["play_counts"].getDynamicObject())
        for (const auto& p : counts->getProperties())
            playCounts[p.name.toString()] = juce::jmax (0, (int) p.value);
}

bool RiffLibrary::saveGlobal() const
{
    if (globalFile == juce::File())
        return false;

    auto* root = new juce::DynamicObject();
    root->setProperty ("schema", 1);
    root->setProperty ("magic", "luthier.rifflibrary");

    juce::Array<juce::var> f, r;

    for (const auto& id : favourites) f.add (id);
    for (const auto& id : recents)    r.add (id);

    root->setProperty ("favourites", f);
    root->setProperty ("recents", r);

    auto* counts = new juce::DynamicObject();

    for (const auto& [id, n] : playCounts)
        counts->setProperty (id, n);

    root->setProperty ("play_counts", juce::var (counts));

    if (! globalFile.getParentDirectory().createDirectory())
        return false;

    juce::TemporaryFile temp (globalFile);

    return temp.getFile().replaceWithText (RiffJson::write (juce::var (root)), false, false, "\n")
             && temp.overwriteTargetFileWithTemporary();
}

void RiffLibrary::setEntriesForTesting (std::vector<RiffIndexEntry> newEntries)
{
    for (auto& e : newEntries)
        e.finalise();

    IndexData data;
    data.entries = std::move (newEntries);
    applyIndex (std::move (data));
}

} // namespace luthier
