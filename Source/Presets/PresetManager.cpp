#include "PresetManager.h"
#include "FactoryPresets.h"
#include "../Support/IrLibrary.h"
#include "../Support/ErrorLog.h"

namespace luthier
{

const char* const PresetManager::kFileExtension = ".luthierpreset";
const char* const PresetManager::kMagic = "luthier.preset";
const char* const PresetManager::kLegacyMagic = "luthierpreset";

//==============================================================================
PresetManager::PresetManager (juce::AudioProcessor& p,
                              juce::AudioProcessorValueTreeState& state,
                              LuthierEngine& e)
    : processor (p), apvts (state), engine (e)
{
    extra.customGaugeInches.fill (0.0);
    extra.detuneCents.fill (0.0);
    extra.realismDetuneCents.fill (0.0);
    extra.fineTuneCents.fill (0.0);
    extra.openFrequencyHz.fill (0.0);
    extra.stringMuted.fill (false);

    for (int i = 0; i < 12; ++i)
        extra.customTemperament[(size_t) i] = std::pow (2.0, i / 12.0);

    searchFolders.add (getFactoryPresetFolder());
    searchFolders.add (getUserPresetFolder());

    // Only when the install folder is somewhere the bank could not be written, so
    // it is not already the folder above.
    if (const auto shipped = getShippedPresetFolder();
        shipped != juce::File() && ! searchFolders.contains (shipped))
        searchFolders.add (shipped);

    ensureFactoryPresetsInstalled();

    // file-formats 13: the retention sweep runs once, at startup.
    pruneOldBackups();

    refresh();
}

PresetManager::~PresetManager() = default;

//==============================================================================
juce::File PresetManager::getUserPresetFolder()
{
    auto documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    auto folder = documents.getChildFile ("Luthier").getChildFile ("Presets").getChildFile ("User");

    if (! folder.exists())
        folder.createDirectory();

    return folder;
}

namespace
{
    /** Whether a folder can actually be written to, tested by writing to it.

        File::hasWriteAccess() only looks at the read-only attribute on Windows,
        which says nothing about a Program Files folder a standard user cannot
        write to. Since getting this wrong leaves the preset browser silently
        empty, it is worth the one probe file. */
    bool folderIsWritable (const juce::File& folder)
    {
        if (! folder.isDirectory())
            return false;

        auto probe = folder.getChildFile (".luthier_write_test");
        probe.deleteFile();

        if (! probe.replaceWithText ("x"))
            return false;

        probe.deleteFile();
        return true;
    }
}

juce::File PresetManager::getFactoryPresetFolder()
{
    // Wherever the shipped Resources folder turned out to be - inside the VST3
    // bundle, beside the standalone binary, or hand-installed. IrLibrary already
    // knows how to find it, so there is one answer rather than two.
    //
    // Answered once per process: the probe below touches the disk, and this is
    // called from the browser, from refresh() and from every save.
    static const juce::File resolved = []
    {
        const auto resources = IrLibrary::getResourcesFolder();

        if (resources != juce::File())
        {
            auto shipped = resources.getChildFile ("Presets").getChildFile ("Factory");

            if (folderIsWritable (shipped))
                return shipped;
        }

        // Installed somewhere read-only, which is the normal case for a plugin
        // under Program Files. The bank goes next to the user's own presets
        // instead, where it can always be rewritten.
        auto documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        auto folder = documents.getChildFile ("Luthier").getChildFile ("Presets").getChildFile ("Factory");

        if (! folder.exists())
            folder.createDirectory();

        return folder;
    }();

    if (! resolved.exists())
        resolved.createDirectory();

    return resolved;
}

juce::File PresetManager::getShippedPresetFolder()
{
    // The folder inside the installed bundle, writable or not. It is scanned as
    // well as the writable one, so a bank someone installed by hand alongside the
    // plugin still shows up.
    const auto resources = IrLibrary::getResourcesFolder();

    if (resources == juce::File())
        return {};

    auto shipped = resources.getChildFile ("Presets").getChildFile ("Factory");
    return shipped.isDirectory() ? shipped : juce::File();
}

juce::File PresetManager::getRenderFolder()
{
    auto documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    auto folder = documents.getChildFile ("Luthier").getChildFile ("Renders");

    if (! folder.exists())
        folder.createDirectory();

    return folder;
}

void PresetManager::addSearchFolder (const juce::File& folder)
{
    if (folder.isDirectory() && ! searchFolders.contains (folder))
    {
        searchFolders.add (folder);
        refresh();
    }
}

void PresetManager::removeSearchFolder (const juce::File& folder)
{
    // The two built-in folders are not removable: losing them would leave the
    // browser empty with no obvious way back.
    if (folder == getFactoryPresetFolder() || folder == getUserPresetFolder())
        return;

    searchFolders.removeAllInstancesOf (folder);
    refresh();
}

//==============================================================================
void PresetManager::ensureFactoryPresetsInstalled()
{
    auto factoryFolder = getFactoryPresetFolder();

    if (! factoryFolder.exists())
        factoryFolder.createDirectory();

    FactoryPresets::writeAll (factoryFolder);
}

//==============================================================================
void PresetManager::refresh()
{
    presets.clear();

    const auto factoryFolder = getFactoryPresetFolder();
    const auto shippedFolder = getShippedPresetFolder();

    for (const auto& folder : searchFolders)
        scanFolder (folder, folder == factoryFolder || folder == shippedFolder);

    // Category first, then name, so the browser reads sensibly.
    struct Sorter
    {
        static int compareElements (const PresetInfo& a, const PresetInfo& b)
        {
            const int byCategory = a.category.compareNatural (b.category);
            return byCategory != 0 ? byCategory : a.name.compareNatural (b.name);
        }
    };

    Sorter sorter;
    presets.sort (sorter);

    // Keep the current selection pointing at the same preset after a rescan.
    currentIndex = -1;

    for (int i = 0; i < presets.size(); ++i)
    {
        if (presets[i].name == currentName)
        {
            currentIndex = i;
            break;
        }
    }

    sendChangeMessage();
}

void PresetManager::scanFolder (const juce::File& folder, bool factory)
{
    if (! folder.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator (folder, true, juce::String ("*") + kFileExtension,
                                                            juce::File::findFiles))
    {
        const auto file = entry.getFile();

        /*  The backup folder is inside the preset folder and this scan recurses,
            so without this every superseded version would reappear in the browser
            as a preset of its own - and the category, taken from the sub-folder
            name, would be a date. */
        if (file.getParentDirectory().getParentDirectory().getFileName() == "Backup")
            continue;

        PresetInfo info;
        info.file = file;
        info.name = file.getFileNameWithoutExtension();
        info.isFactory = factory;

        // The category is the sub-folder the preset sits in.
        const auto relative = file.getParentDirectory().getRelativePathFrom (folder);
        info.category = (relative == "." || relative.isEmpty()) ? (factory ? "Factory" : "User")
                                                                : relative.replaceCharacter ('\\', '/')
                                                                          .upToFirstOccurrenceOf ("/", false, false);

        // Read just the metadata; the full parameter block is only parsed on load.
        if (auto parsed = juce::JSON::parse (file.loadFileAsString()); auto* obj = parsed.getDynamicObject())
        {
            if (obj->hasProperty ("name"))        info.name = obj->getProperty ("name").toString();
            if (obj->hasProperty ("author"))      info.author = obj->getProperty ("author").toString();
            if (obj->hasProperty ("description")) info.description = obj->getProperty ("description").toString();

            if (obj->hasProperty ("category"))
            {
                const auto cat = obj->getProperty ("category").toString();

                if (cat.isNotEmpty())
                    info.category = cat;
            }

            if (auto* tagArray = obj->getProperty ("tags").getArray())
                for (const auto& t : *tagArray)
                    info.tags.add (t.toString());
        }

        presets.add (info);
    }
}

//==============================================================================
const PresetInfo* PresetManager::getPreset (int index) const noexcept
{
    if (! juce::isPositiveAndBelow (index, presets.size()))
        return nullptr;

    return &presets.getReference (index);
}

int PresetManager::indexOfPreset (const juce::String& name) const noexcept
{
    int userMatch = -1;

    for (int i = 0; i < presets.size(); ++i)
    {
        const auto& info = presets.getReference (i);

        if (! info.name.equalsIgnoreCase (name))
            continue;

        // A factory preset wins outright; a user one is remembered in case there
        // is no factory preset by that name at all.
        if (info.isFactory)
            return i;

        if (userMatch < 0)
            userMatch = i;
    }

    return userMatch;
}

juce::Array<int> PresetManager::getPresetsInCategory (const juce::String& category) const
{
    juce::Array<int> result;

    for (int i = 0; i < presets.size(); ++i)
        if (category.isEmpty() || presets[i].category == category)
            result.add (i);

    return result;
}

juce::StringArray PresetManager::getCategories() const
{
    juce::StringArray categories;

    for (const auto& p : presets)
        categories.addIfNotAlreadyThere (p.category);

    categories.sortNatural();
    return categories;
}

void PresetManager::markModified() noexcept
{
    if (! modified)
    {
        modified = true;
        sendChangeMessage();
    }
}

//==============================================================================
juce::var PresetManager::toVar (const juce::String& name,
                                const juce::String& category,
                                const juce::String& description,
                                const juce::StringArray& tags) const
{
    auto* root = new juce::DynamicObject();

    /*  file-formats 0.3: anything this build did not recognise on load is written
        back out first, so a newer version's fields survive a round trip through
        this one. Known keys are set afterwards and therefore win. */
    if (auto* preserved = unknownFields.getDynamicObject())
        for (const auto& property : preserved->getProperties())
            root->setProperty (property.name, property.value);

    root->setProperty ("magic", kMagic);
    root->setProperty ("schemaVersion", kSchemaVersion);
    root->setProperty ("pluginVersion", JucePlugin_VersionString);
    root->setProperty ("name", name.isNotEmpty() ? name : currentName);
    root->setProperty ("category", category.isNotEmpty() ? category : currentCategory);
    root->setProperty ("author", "");
    root->setProperty ("description", description);

    juce::Array<juce::var> tagArray;

    for (const auto& t : tags)
        tagArray.add (t);

    root->setProperty ("tags", tagArray);

    // ---- parameters ----------------------------------------------------------
    auto* params = new juce::DynamicObject();

    for (auto* p : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            params->setProperty (withId->paramID, (double) withId->getValue());

    root->setProperty ("parameters", juce::var (params));

    // ---- per-string extras ----------------------------------------------------
    auto* strings = new juce::DynamicObject();

    auto writeArray = [] (const auto& source, int count)
    {
        juce::Array<juce::var> a;

        for (int i = 0; i < count; ++i)
            a.add ((double) source[(size_t) i]);

        return juce::var (a);
    };

    const int n = juce::jlimit (1, kMaxStrings, extra.numStrings);

    strings->setProperty ("numStrings", n);
    strings->setProperty ("customGaugeInches", writeArray (extra.customGaugeInches, n));
    strings->setProperty ("detuneCents", writeArray (extra.detuneCents, n));
    strings->setProperty ("realismDetuneCents", writeArray (extra.realismDetuneCents, n));
    strings->setProperty ("fineTuneCents", writeArray (extra.fineTuneCents, n));
    strings->setProperty ("openFrequencyHz", writeArray (extra.openFrequencyHz, n));
    strings->setProperty ("useCustomTuning", extra.useCustomTuning);

    juce::Array<juce::var> muted;

    for (int i = 0; i < n; ++i)
        muted.add (extra.stringMuted[(size_t) i]);

    strings->setProperty ("muted", muted);
    strings->setProperty ("customTemperament", writeArray (extra.customTemperament, 12));

    root->setProperty ("strings", juce::var (strings));

    // ---- MIDI map --------------------------------------------------------------
    auto* midiMap = new juce::DynamicObject();
    const auto& interp = const_cast<LuthierEngine&> (engine).getMidiInterpreter();

    for (int cc = 0; cc < 128; ++cc)
    {
        const auto target = interp.getCcTarget (cc);

        if (target != MidiTarget::None)
            midiMap->setProperty (juce::String (cc), (int) target);
    }

    root->setProperty ("midiMap", juce::var (midiMap));

    return juce::var (root);
}

//==============================================================================
bool PresetManager::fromVar (const juce::var& data)
{
    auto* obj = data.getDynamicObject();

    if (obj == nullptr)
        return false;

    /*  file-formats 14.2: the magic is checked before anything else, so a JSON
        file that is not a Luthier preset is refused rather than half-applied.

        `format` is the spelling used before file-formats.md named the field, and
        is still accepted so existing user presets keep loading. */
    const auto magic = obj->getProperty ("magic").toString();
    const auto legacy = obj->getProperty ("format").toString();

    if (magic != kMagic && legacy != kLegacyMagic)
    {
        // error-recovery 1: "magic field missing or wrong".
        ErrorLog::write (ErrorLog::Severity::warn, "PresetSystem", "BAD_MAGIC",
                         "File does not appear to be a Luthier preset",
                         [&]
                         {
                             auto* context = new juce::DynamicObject();
                             context->setProperty ("magic", magic);
                             context->setProperty ("format", legacy);
                             return juce::var (context);
                         }());

        return false;
    }

    // A file from a future schema is loaded as best we can rather than refused:
    // unknown keys are preserved, and every parameter has a default.
    const int schema = (int) obj->getProperty ("schemaVersion");

    if (schema <= 0)
    {
        ErrorLog::write (ErrorLog::Severity::warn, "PresetSystem", "BAD_SCHEMA",
                         "Preset has no usable schema version");
        return false;
    }

    if (schema > kSchemaVersion)
    {
        /*  error-recovery 1: "schema newer than the plugin supports".

            Loaded rather than refused, because every parameter has a default and
            section 0.4 prefers partial success - but it is recorded, because a
            preset that half-loads and says nothing is exactly the silent
            degradation ground rule 2 forbids. */
        ErrorLog::write (ErrorLog::Severity::info, "PresetSystem", "NEWER_SCHEMA",
                         "Preset was written by a newer version of Luthier",
                         [&]
                         {
                             auto* context = new juce::DynamicObject();
                             context->setProperty ("file_schema", schema);
                             context->setProperty ("supported_schema", kSchemaVersion);
                             return juce::var (context);
                         }());
    }

    /*  file-formats 0.3: hold on to every top-level key this build does not know
        about, so saving does not delete a newer version's work. */
    {
        static const juce::StringArray known
        {
            "magic", "format", "schemaVersion", "pluginVersion", "name", "category",
            "author", "description", "tags", "parameters", "strings", "extras",
            "lockedParameters", "midiMappings", "modulation", "snapshots",
            "rhythmEngine", "routing", "character", "toneMatch"
        };

        auto* preserved = new juce::DynamicObject();

        for (const auto& property : obj->getProperties())
            if (! known.contains (property.name.toString()))
                preserved->setProperty (property.name, property.value);

        unknownFields = juce::var (preserved);
    }

    // ---- parameters ------------------------------------------------------------
    if (auto* params = obj->getProperty ("parameters").getDynamicObject())
    {
        for (auto* p : processor.getParameters())
        {
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            {
                if (params->hasProperty (withId->paramID))
                {
                    const double v = (double) params->getProperty (withId->paramID);
                    withId->setValueNotifyingHost ((float) juce::jlimit (0.0, 1.0, v));
                }
            }
        }
    }

    // ---- per-string extras -------------------------------------------------------
    if (auto* strings = obj->getProperty ("strings").getDynamicObject())
    {
        auto readArray = [strings] (const char* key, auto& dest, int maxCount)
        {
            if (auto* a = strings->getProperty (key).getArray())
                for (int i = 0; i < juce::jmin (a->size(), maxCount); ++i)
                    dest[(size_t) i] = (double) (*a)[i];
        };

        extra.numStrings = juce::jlimit (1, kMaxStrings, (int) strings->getProperty ("numStrings"));
        extra.useCustomTuning = strings->getProperty ("useCustomTuning");

        readArray ("customGaugeInches", extra.customGaugeInches, kMaxStrings);
        readArray ("detuneCents", extra.detuneCents, kMaxStrings);
        readArray ("realismDetuneCents", extra.realismDetuneCents, kMaxStrings);
        readArray ("fineTuneCents", extra.fineTuneCents, kMaxStrings);
        readArray ("openFrequencyHz", extra.openFrequencyHz, kMaxStrings);
        readArray ("customTemperament", extra.customTemperament, 12);

        if (auto* a = strings->getProperty ("muted").getArray())
            for (int i = 0; i < juce::jmin (a->size(), kMaxStrings); ++i)
                extra.stringMuted[(size_t) i] = (bool) (*a)[i];
    }

    // ---- MIDI map ----------------------------------------------------------------
    if (auto* midiMap = obj->getProperty ("midiMap").getDynamicObject())
    {
        auto& interp = engine.getMidiInterpreter();
        interp.resetCcMapToDefaults();

        for (const auto& prop : midiMap->getProperties())
        {
            const int cc = prop.name.toString().getIntValue();
            const int target = (int) prop.value;

            if (juce::isPositiveAndBelow (cc, 128)
                && juce::isPositiveAndBelow (target, (int) MidiTarget::NumTargets))
                interp.setCcTarget (cc, (MidiTarget) target);
        }
    }

    currentName = obj->getProperty ("name").toString();
    currentCategory = obj->getProperty ("category").toString();

    if (currentName.isEmpty())
        currentName = "Untitled";

    modified = false;
    return true;
}

//==============================================================================
void PresetManager::applyExtraState()
{
    auto& tuningEngine = engine.getTuningEngine();
    const int n = juce::jlimit (1, kMaxStrings, extra.numStrings);

    for (int i = 0; i < n; ++i)
    {
        if (extra.useCustomTuning && extra.openFrequencyHz[(size_t) i] > 10.0)
            tuningEngine.setOpenFrequency (i, extra.openFrequencyHz[(size_t) i]);

        tuningEngine.setDetuneCents (i, extra.detuneCents[(size_t) i]);
        tuningEngine.setFineTuneCents (i, extra.fineTuneCents[(size_t) i]);

        auto t = tuningEngine.getStringTuning (i);
        t.realismDetuneCents = extra.realismDetuneCents[(size_t) i];
        tuningEngine.setStringTuning (i, t);

        engine.setCustomStringGauge (i, extra.customGaugeInches[(size_t) i]);
    }

    tuningEngine.setCustomTemperament (extra.customTemperament);
    engine.refreshStringPhysics();
}

void PresetManager::captureExtraState()
{
    const auto& tuningEngine = engine.getTuningEngine();
    extra.numStrings = engine.getNumStrings();

    for (int i = 0; i < extra.numStrings; ++i)
    {
        const auto& t = tuningEngine.getStringTuning (i);
        extra.detuneCents[(size_t) i] = t.detuneCents;
        extra.realismDetuneCents[(size_t) i] = t.realismDetuneCents;
        extra.fineTuneCents[(size_t) i] = t.fineTuneCents;
        extra.openFrequencyHz[(size_t) i] = t.openFrequencyHz;
        extra.customGaugeInches[(size_t) i] = engine.getCustomStringGauge (i);
    }

    extra.customTemperament = tuningEngine.getCustomTemperament();
}

//==============================================================================
bool PresetManager::loadPreset (int index)
{
    if (const auto* info = getPreset (index))
    {
        if (loadPreset (info->file))
        {
            currentIndex = index;
            sendChangeMessage();
            return true;
        }

        // The file overload has already said why, in lastLoadError.
        return false;
    }

    /*  No preset at that index. The list is rescanned whenever the folder
        changes, so this is a caller holding an index from before a rescan rather
        than a corrupt file - which is still worth saying, because from the user's
        side a preset they clicked has vanished. */
    lastLoadError = "That preset is no longer in the library.";
    return false;
}

bool PresetManager::loadPreset (const juce::File& file)
{
    auto context = [&file]
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("path", file.getFullPathName());
        return juce::var (object);
    };

    // error-recovery 1: "file does not exist at path". The current session is
    // left alone rather than cleared.
    /*  Each failure below sets lastLoadError beside its log line, so the window
        has something to put in a banner. The log is for support; this is for the
        person at the keyboard, who otherwise watches a preset simply not load.
        The wording names the file, because "could not load preset" about one of
        several hundred is not useful. */
    if (! file.existsAsFile())
    {
        ErrorLog::write (ErrorLog::Severity::warn, "PresetSystem", "FILE_NOT_FOUND",
                         "Preset not found", context());

        lastLoadError = "Preset not found: " + file.getFileName();
        return false;
    }

    const auto text = file.loadFileAsString();

    if (text.isEmpty())
    {
        ErrorLog::write (ErrorLog::Severity::warn, "PresetSystem", "FILE_UNREADABLE",
                         "Preset could not be read, or is empty", context());

        lastLoadError = file.getFileName() + " could not be read, or is empty.";
        return false;
    }

    const auto parsed = juce::JSON::parse (text);

    if (! parsed.isObject())
    {
        // error-recovery 1: "file is not JSON".
        ErrorLog::write (ErrorLog::Severity::warn, "PresetSystem", "NOT_JSON",
                         "Preset is not valid JSON", context());

        lastLoadError = file.getFileName() + " is not a Luthier preset.";
        return false;
    }

    if (! fromVar (parsed))
    {
        ErrorLog::write (ErrorLog::Severity::warn, "PresetSystem", "LOAD_REFUSED",
                         "Preset was refused; the file is untouched", context());

        /*  error-recovery 1 again: the session is left alone on a refusal, and
            saying so is the difference between a refusal and an apparent freeze. */
        lastLoadError = file.getFileName()
                          + " was refused. The current sound is unchanged.";
        return false;
    }

    currentName = file.getFileNameWithoutExtension();
    currentFile = file;
    applyExtraState();

    lastLoadError.clear();

    modified = false;
    sendChangeMessage();
    return true;
}

bool PresetManager::loadNext()
{
    if (presets.isEmpty())
        return false;

    return loadPreset ((currentIndex + 1) % presets.size());
}

bool PresetManager::loadPrevious()
{
    if (presets.isEmpty())
        return false;

    const int index = (currentIndex <= 0) ? presets.size() - 1 : currentIndex - 1;
    return loadPreset (index);
}

//==============================================================================
void PresetManager::backupBeforeOverwrite (const juce::File& target)
{
    /*  file-formats 13.4: the version being replaced is kept, filed by the day it
        was replaced.

        This is the step that makes "save" recoverable rather than final. The
        temp-then-rename below already guarantees the file on disk is never a
        half-written one; it does nothing for a user who saved over the sound they
        wanted. */
    if (! target.existsAsFile())
        return;

    const auto today = juce::Time::getCurrentTime().formatted ("%Y-%m-%d");

    auto folder = target.getParentDirectory().getChildFile ("Backup").getChildFile (today);

    if (! folder.createDirectory())
        return;

    auto destination = folder.getChildFile (target.getFileName());

    // Several saves in one day keep several versions rather than one.
    for (int i = 2; destination.existsAsFile() && i < 1000; ++i)
        destination = folder.getChildFile (target.getFileNameWithoutExtension()
                                             + "-" + juce::String (i) + kFileExtension);

    target.copyFileTo (destination);
}

void PresetManager::pruneOldBackups()
{
    const auto cutoff = juce::Time::getCurrentTime()
                          - juce::RelativeTime::days ((double) kBackupRetentionDays);

    for (const auto& root : { getUserPresetFolder(), getFactoryPresetFolder() })
    {
        auto backups = root.getChildFile ("Backup");

        if (! backups.isDirectory())
            continue;

        for (const auto& entry : juce::RangedDirectoryIterator (backups, false, "*",
                                                                juce::File::findDirectories))
        {
            const auto folder = entry.getFile();

            /*  Dated by name rather than by the filesystem's timestamp, because a
                copy or a restore rewrites the timestamp and would either resurrect
                expired backups or delete live ones. The name is what the sweep
                promised to honour. */
            const auto name = folder.getFileName();

            if (name.length() != 10)
                continue;

            const juce::Time stamp (name.substring (0, 4).getIntValue(),
                                    name.substring (5, 7).getIntValue() - 1,
                                    name.substring (8, 10).getIntValue(),
                                    0, 0);

            if (stamp.toMilliseconds() > 0 && stamp < cutoff)
                folder.deleteRecursively();
        }
    }
}

bool PresetManager::writeToFile (const juce::File& file, const juce::var& data) const
{
    file.getParentDirectory().createDirectory();

    backupBeforeOverwrite (file);

    juce::TemporaryFile temp (file);

    if (auto stream = temp.getFile().createOutputStream())
    {
        stream->setPosition (0);
        stream->truncate();
        stream->writeText (juce::JSON::toString (data, false), false, false, "\n");
        stream->flush();
        stream.reset();

        if (temp.overwriteTargetFileWithTemporary())
            return true;

        // error-recovery 2: "save succeeded but rename failed". TemporaryFile's
        // destructor removes the temp, so nothing partial is left behind.
        ErrorLog::write (ErrorLog::Severity::error, "PresetSystem", "SAVE_RENAME_FAILED",
                         "Could not replace the preset file; the previous version is intact",
                         [&]
                         {
                             auto* context = new juce::DynamicObject();
                             context->setProperty ("path", file.getFullPathName());
                             return juce::var (context);
                         }());

        return false;
    }

    // error-recovery 2: "destination folder not writable" / "disk full".
    ErrorLog::write (ErrorLog::Severity::error, "PresetSystem", "SAVE_UNWRITABLE",
                     "Could not open the preset for writing; nothing on disk changed",
                     [&]
                     {
                         auto* context = new juce::DynamicObject();
                         context->setProperty ("path", file.getFullPathName());
                         return juce::var (context);
                     }());

    return false;
}

bool PresetManager::saveCurrent()
{
    const auto* info = getPreset (currentIndex);

    // A factory preset is never overwritten: saving one becomes a user copy.
    if (info == nullptr || info->isFactory)
        return saveAs (currentName, "User");

    captureExtraState();

    if (writeToFile (info->file, toVar (info->name, info->category, info->description, info->tags)))
    {
        modified = false;
        sendChangeMessage();
        return true;
    }

    return false;
}

bool PresetManager::saveAs (const juce::String& name, const juce::String& category,
                            const juce::String& description, const juce::StringArray& tags)
{
    const auto safeName = juce::File::createLegalFileName (name.trim());

    if (safeName.isEmpty())
        return false;

    const auto safeCategory = category.isNotEmpty() ? juce::File::createLegalFileName (category) : "User";

    auto file = getUserPresetFolder().getChildFile (safeCategory)
                                     .getChildFile (safeName + kFileExtension);

    captureExtraState();

    if (! writeToFile (file, toVar (name, safeCategory, description, tags)))
        return false;

    currentName = name;
    currentCategory = safeCategory;
    modified = false;

    refresh();

    for (int i = 0; i < presets.size(); ++i)
    {
        if (presets[i].file == file)
        {
            currentIndex = i;
            break;
        }
    }

    sendChangeMessage();
    return true;
}

bool PresetManager::deletePreset (int index)
{
    const auto* info = getPreset (index);

    if (info == nullptr || info->isFactory)
        return false;

    const auto file = info->file;

    if (! file.deleteFile())
        return false;

    refresh();
    return true;
}

bool PresetManager::importPreset (const juce::File& source)
{
    if (! source.existsAsFile())
        return false;

    auto destination = getUserPresetFolder().getChildFile ("Imported")
                                            .getChildFile (source.getFileName());

    destination.getParentDirectory().createDirectory();

    if (! source.copyFileTo (destination))
        return false;

    refresh();
    return loadPreset (destination);
}

bool PresetManager::exportPreset (const juce::File& destination)
{
    captureExtraState();
    return writeToFile (destination, toVar (currentName, currentCategory));
}

//==============================================================================
void PresetManager::resetToDefaults()
{
    for (auto* p : processor.getParameters())
        p->setValueNotifyingHost (p->getDefaultValue());

    extra = ExtraState {};
    extra.customGaugeInches.fill (0.0);
    extra.detuneCents.fill (0.0);
    extra.realismDetuneCents.fill (0.0);
    extra.fineTuneCents.fill (0.0);
    extra.openFrequencyHz.fill (0.0);
    extra.stringMuted.fill (false);

    for (int i = 0; i < 12; ++i)
        extra.customTemperament[(size_t) i] = std::pow (2.0, i / 12.0);

    engine.getMidiInterpreter().resetCcMapToDefaults();
    applyExtraState();

    currentName = "Init";
    currentCategory = "User";
    currentIndex = -1;
    modified = false;

    sendChangeMessage();
}

//==============================================================================
bool PresetManager::isRandomisable (const juce::String& paramId)
{
    // Randomising these produces something broken rather than something new.
    static const juce::StringArray excluded
    {
        ParamIDs::masterGain, ParamIDs::limiterOn, ParamIDs::oversample,
        ParamIDs::ampStandby, ParamIDs::concertA, ParamIDs::guitarVolume,
        ParamIDs::whammyPos, ParamIDs::ebowEnable, ParamIDs::freezeEnable,
        ParamIDs::mpeEnabled,
        ParamIDs::playingMode, ParamIDs::bendRange, ParamIDs::transposeLock,
        ParamIDs::tuningDrift, ParamIDs::chordWindow,
        ParamIDs::secretOn, ParamIDs::secretRate, ParamIDs::secretDepth,
        ParamIDs::secretFeedback, ParamIDs::secretMix
    };

    return ! excluded.contains (paramId);
}

void PresetManager::randomise (uint64_t seed, const juce::StringArray& lockedParameters)
{
    // Each press starts from the defaults, so repeated presses give genuinely new
    // sounds rather than drifting further from anything usable.
    for (auto* p : processor.getParameters())
    {
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            if (! lockedParameters.contains (withId->paramID))
                p->setValueNotifyingHost (p->getDefaultValue());
    }

    RtRandom rng { seed };

    for (auto* p : processor.getParameters())
    {
        auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p);

        if (withId == nullptr)
            continue;

        const auto& id = withId->paramID;

        if (lockedParameters.contains (id) || ! isRandomisable (id))
            continue;

        // Effect slot types get a lighter hand: filling all sixteen slots at random
        // produces noise, not a patch.
        const bool isSlotType = id.endsWith ("_type") && (id.startsWith ("pre") || id.startsWith ("post"));

        if (isSlotType && ! rng.nextBool (0.30))
        {
            p->setValueNotifyingHost (0.0f);   // empty
            continue;
        }

        // Bias continuous parameters toward the middle of their range: fully
        // uniform randomisation lands on the extremes far too often.
        double v = rng.nextDouble();

        if (dynamic_cast<juce::AudioParameterFloat*> (p) != nullptr)
            v = juce::jlimit (0.0, 1.0, 0.5 + rng.nextGaussian() * 0.26);

        p->setValueNotifyingHost ((float) v);
    }

    currentName = "Random";
    modified = true;
    sendChangeMessage();
}

} // namespace luthier
