#include "PresetManager.h"
#include "TechniquePresets.h"   // TECHNIQUES
#include "Search/PresetFeatures.h"   // FEAT-BROWSER
#include "FactoryPresets.h"
#include "MicPlacementMigration.h"   // mic-placement.md 4
#include "../Support/IrLibrary.h"
#include "../Support/ErrorLog.h"
#include "../UI/UiPreferences.h"   // REALISM-C

namespace luthier
{

const char* const PresetManager::kFileExtension = ".luthierpreset";
const char* const PresetManager::kMagic = "luthier.preset";
const char* const PresetManager::kLegacyMagic = "luthierpreset";

//==============================================================================
std::array<PresetManager::LegacyPlacement, 3> PresetManager::takeLegacyPickupPlacements()
{
    auto out = legacyPlacements;
    legacyPlacements = {};
    return out;
}

//==============================================================================
PresetManager::PresetManager (juce::AudioProcessor& p,
                              juce::AudioProcessorValueTreeState& state,
                              LuthierEngine& e,
                              RangeState& r)
    : processor (p), apvts (state), engine (e), ranges (r)
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

    // SPEC-SWEEP: ER-79/FF-34 - the error log's 30-day retention, in the same
    // startup sweep (error-recovery 12).
    ErrorLog::pruneOldLogs (kBackupRetentionDays);

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

            // SPEC-SWEEP: FF-20 - `meta` wins over the flat keys when present.
            if (auto* meta = obj->getProperty ("meta").getDynamicObject())
            {
                if (meta->getProperty ("name").toString().isNotEmpty())        info.name = meta->getProperty ("name").toString();
                if (meta->getProperty ("author").toString().isNotEmpty())      info.author = meta->getProperty ("author").toString();
                if (meta->getProperty ("description").toString().isNotEmpty()) info.description = meta->getProperty ("description").toString();
                if (meta->getProperty ("category").toString().isNotEmpty())    info.category = meta->getProperty ("category").toString();
            }
            // gui-techniques-updates.md 7 (TECHNIQUES): which techniques it arms.
            if (auto* params = obj->getProperty ("parameters").getDynamicObject())
                for (const auto& id : getTechniqueArmParameterIds())
                    if (params->hasProperty (id) && (double) params->getProperty (id) > 0.5)
                        info.armedTechniques.add (id);
            // preset-browser-previews 5.4 / 5.6 (FEAT-BROWSER): the uid keys
            // favourites and ratings; the gear names feed search.
            info.uid = obj->getProperty ("uid").toString();

            if (info.uid.isEmpty() && factory)
                info.uid = "factory:" + info.name;

            if (featureReader == nullptr)
                featureReader = std::make_unique<PresetFeatureReader> (processor);

            if (obj->getProperty ("parameters").getDynamicObject() != nullptr)
            {
                const auto features = featureReader->read (parsed);
                info.guitarName = features.guitarName;
                info.ampName = features.ampName;
                info.family = PresetFeatures::getFamilyName (features.family);
            }
        }

        info.modified = file.getLastModificationTime();

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

    // SPEC-SWEEP: FC-1 - a factory preset asked for by the name it had before
    // the trademark sweep.
    if (userMatch < 0)
    {
        const auto renamed = FactoryPresets::renamedPreset (name);

        if (renamed != name)
            return indexOfPreset (renamed);
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
    root->setProperty ("schema", kSchemaVersion);          // SPEC-SWEEP: FF-2
    root->setProperty ("schemaVersion", kSchemaVersion);   // read by builds before FF-2
    root->setProperty ("pluginVersion", JucePlugin_VersionString);
    root->setProperty ("name", name.isNotEmpty() ? name : currentName);
    root->setProperty ("category", category.isNotEmpty() ? category : currentCategory);
    root->setProperty ("author", "");
    root->setProperty ("description", description);

    juce::Array<juce::var> tagArray;

    for (const auto& t : tags)
        tagArray.add (t);

    root->setProperty ("tags", tagArray);

    /*  SPEC-SWEEP: FF-20, file-formats 2. The spec's `meta` object; the flat
        keys above stay beside it for builds that read only those. */
    {
        auto* meta = new juce::DynamicObject();
        meta->setProperty ("name", root->getProperty ("name"));
        meta->setProperty ("author", root->getProperty ("author"));
        meta->setProperty ("category", root->getProperty ("category"));
        meta->setProperty ("tags", tagArray);
        meta->setProperty ("description", description);
        meta->setProperty ("notes", metaNotes);

        if (metaCreated.isNotEmpty())
            meta->setProperty ("created", metaCreated);

        if (metaModified.isNotEmpty())
            meta->setProperty ("modified", metaModified);

        meta->setProperty ("version_created", metaVersionCreated.isNotEmpty() ? metaVersionCreated
                                                                              : juce::String (JucePlugin_VersionString));
        meta->setProperty ("version_modified", JucePlugin_VersionString);

        root->setProperty ("meta", juce::var (meta));
    }
    // preset-browser-previews 5.4 (FEAT-BROWSER): optional, beside the name.
    if (currentUid.isNotEmpty() && ! currentUid.startsWith ("factory:"))
        root->setProperty ("uid", currentUid);

    if (currentPreviewPhrase.isNotEmpty())
        root->setProperty ("previewPhrase", currentPreviewPhrase);

    // ---- parameters ----------------------------------------------------------
    auto* params = new juce::DynamicObject();

    // The preset-morph position is a performance control, not part of a sound:
    // saving it would make loading a morph slot drag the slider back.
    for (auto* p : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            if (withId->paramID != ParamIDs::presetMorphPosition
                  && ! ParamIDs::isJamTransient (withId->paramID))   // FEAT-JAM: jam-mode 10
            {
                // Store the normalised value exactly as get() reports it. Load
                // applies it back with setValueNotifyingHost (a normalised set,
                // no plain-value round trip), so (float) of this double restores
                // the identical value: save -> load -> save is stable, and a
                // reload reproduces get() to the bit (clap-validator
                // state-reproducibility). An earlier fixed-point nudge through
                // convertFrom0to1/convertTo0to1 shifted skewed params by up to a
                // float ULP away from get(), which that validator flags.
                const double v = (double) withId->getValue();

                params->setProperty (withId->paramID, v);
            }

    // mic-placement.md 4: the nearest discrete Position / Distance goes into
    // the file for an older Luthier to read; the live parameters are untouched.
    juce::var written (params);
    MicPlacementMigration::mirror (written, apvts);

    root->setProperty ("parameters", written);
    root->setProperty (MicPlacementMigration::kLegacyBlockKey, MicPlacementMigration::captureLiveLegacy (apvts));

    /*  advanced-ranges.md 4: which families this preset has unlocked. Written
        beside the parameters because it is what makes their normalised values
        mean anything - see the ordering note in fromVar. */
    root->setProperty ("ranges", ranges.toVar());

    // guitar-workshop.md 8: which guitar, and the whole guitar if it was edited.
    if (captureGuitarBlock != nullptr)
        root->setProperty ("guitar", captureGuitarBlock());

    // jam-mode.md 12 (FEAT-JAM): the band's style file, rhythm-kit link and seed.
    if (captureJamBlock != nullptr)
        root->setProperty ("jam", captureJamBlock());
    // TECHNIQUES: engine-technique-layer.md 7.
    if (captureTechniquesBlock != nullptr)
        root->setProperty ("techniques", captureTechniquesBlock());

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
    // Derived from a float parameter, so it carries float noise; 0.0001 cents
    // is far below hearing and makes save -> load -> save stable.
    {
        auto rounded = extra.realismDetuneCents;

        for (auto& c : rounded)
            c = std::round (c * 1.0e4) / 1.0e4;

        strings->setProperty ("realismDetuneCents", writeArray (rounded, n));
    }
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

    // SPEC-SWEEP: SM-1/SM-16/FF-24..29 - modulation, snapshots, MIDI Learn,
    // rhythm, routing, character and tone-match travel in the file.
    if (capturePresetBlocks != nullptr)
        capturePresetBlocks (*root);

    return juce::var (root);
}

//==============================================================================
bool PresetManager::fromVar (const juce::var& data)
{
    struct LoadFade
    {
        explicit LoadFade (PresetManager& m) : manager (m) { if (manager.onBeforeLoad != nullptr) manager.onBeforeLoad(); }
        ~LoadFade() { if (manager.onAfterLoad != nullptr) manager.onAfterLoad(); }
        PresetManager& manager;
    } fade (*this);

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

    lastRefusal.clear();

    /*  SPEC-SWEEP: FF-2. file-formats 0.2 names the field `schema`; files from
        before carry `schemaVersion`. A file with neither is schema 1 and is
        migrated rather than refused; only a value that is not a number is. */
    const auto schemaValue = obj->hasProperty ("schema") ? obj->getProperty ("schema")
                                                         : obj->getProperty ("schemaVersion");

    if (! schemaValue.isVoid() && ! (schemaValue.isInt() || schemaValue.isInt64() || schemaValue.isDouble()))
    {
        ErrorLog::write (ErrorLog::Severity::warn, "PresetSystem", "BAD_SCHEMA",
                         "Preset has no usable schema version");
        return false;
    }

    const int schema = schemaValue.isVoid() ? 1 : (int) schemaValue;

    if (schema < 1)
    {
        // SPEC-SWEEP: ER-13, error-recovery 1: older than anything migratable.
        ErrorLog::write (ErrorLog::Severity::warn, "PresetSystem", "UNSUPPORTED_SCHEMA",
                         "Preset uses a schema older than any this build can migrate");
        lastRefusal = "uses a format Luthier no longer supports.";
        return false;
    }

    if (schema > kSchemaVersion)
    {
        /*  SPEC-SWEEP: ER-12, error-recovery 1: "schema newer than the plugin
            supports" is refused with a banner. docs/spec-coverage.md C-20
            resolves the conflict with section 0.4's partial success this way:
            the specific row beats the general rule, and a half-understood file
            that then gets re-saved would lose what the newer version wrote. */
        ErrorLog::write (ErrorLog::Severity::warn, "PresetSystem", "NEWER_SCHEMA",
                         "Preset was written by a newer version of Luthier",
                         [&]
                         {
                             auto* context = new juce::DynamicObject();
                             context->setProperty ("file_schema", schema);
                             context->setProperty ("supported_schema", kSchemaVersion);
                             return juce::var (context);
                         }());

        lastRefusal = "was made by a newer Luthier version. Update to open.";
        return false;
    }

    /*  file-formats 0.3: hold on to every top-level key this build does not know
        about, so saving does not delete a newer version's work. */
    {
        static const juce::StringArray known
        {
            "magic", "format", "schema", "schemaVersion", "pluginVersion", "name", "category", "meta",
            "author", "description", "tags", "parameters", "strings", "extras",
            "lockedParameters", "midiMappings", "modulation", "snapshots",
            "rhythmEngine", "routing", "character", "toneMatch",
            // Written by this build too (a known key read back as unknown moved
            // to the front of the next save, so save -> load -> save differed).
            "ranges", "guitar", "midiMap",
            // SPEC-SWEEP: the spec's spellings of the processor blocks.
            "midi_mappings", "rhythm_engine", "tone_match",
            "jam",   // FEAT-JAM (jam-mode 12)
            MicPlacementMigration::kLegacyBlockKey,   // mic-placement.md 4
            "techniques"   // TECHNIQUES: engine-technique-layer.md 7
            "uid", "previewPhrase"   // preset-browser-previews 5.4 (FEAT-BROWSER)
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
        /*  advanced-ranges.md 4: the ranges block is applied BEFORE the
            parameter values, and the order is load-bearing.

            Parameters are stored normalised. A preset saved with `amp`
            unlocked and its gain at the advanced maximum stored 1.0; writing
            that 1.0 while the parameter is still on its stock range would
            produce stockMax and silently halve the value. Widening first means
            the normalised number lands where it was written.

            A preset with no block is left on stock here and derived from
            afterwards, which is 4.1: its values were saved against the stock
            ranges because there was nothing else to save them against. */
        const bool hasRangesBlock = obj->hasProperty ("ranges");

        // installer.md 8: what this load had to migrate, for the info banner.
        juce::StringArray migrations;

        if (! hasRangesBlock && obj->hasProperty ("pluginVersion")
              && obj->getProperty ("pluginVersion").toString() != JucePlugin_VersionString)
            migrations.add ("ranges");

        // guitar-workshop.md 9: retired placement parameters, kept for the guitar.
        for (int slot = 0; slot < 3; ++slot)
        {
            const auto positionId = ParamIDs::pickupPosition (slot);
            const auto heightId = ParamIDs::pickupHeight (slot);

            auto& legacy = legacyPlacements[(size_t) slot];
            legacy = {};

            if (params->hasProperty (positionId))
            {
                // Their old ranges: 0.02-0.48 linear, and 1-6 mm skewed to 3.5.
                legacy.present = true;
                migrations.addIfNotAlreadyThere ("pickup placements");
                legacy.positionFraction = juce::jmap ((double) params->getProperty (positionId), 0.02, 0.48);

                juce::NormalisableRange<float> heightRange (1.0f, 6.0f);
                heightRange.setSkewForCentre (3.5f);
                legacy.heightMm = params->hasProperty (heightId)
                                    ? (double) heightRange.convertFrom0to1 ((float) (double) params->getProperty (heightId))
                                    : 2.5;
            }
        }

        RangeState incoming;

        if (hasRangesBlock)
            incoming.fromVar (obj->getProperty ("ranges"), {});

        ranges = incoming;
        ranges.applyTo (apvts);

        // mic-placement.md 4: an older file's discrete mic placement gains its
        // continuous values, converted through the ranges just applied.
        {
            auto stored = obj->getProperty ("parameters");
            MicPlacementMigration::apply (stored, apvts);
        }

        for (auto* p : processor.getParameters())
        {
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            {
                if (params->hasProperty (withId->paramID) && withId->paramID != ParamIDs::presetMorphPosition
                      && ! ParamIDs::isJamTransient (withId->paramID)   // FEAT-JAM
                      && ! (keepOnLoad != nullptr && keepOnLoad (withId->paramID)))
                {
                    const double v = (double) params->getProperty (withId->paramID);
                    withId->setValueNotifyingHost ((float) juce::jlimit (0.0, 1.0, v));
                }
                else if (! params->hasProperty (withId->paramID) && ! keepsValueWhenAbsent (withId->paramID)
                           && ! (keepOnLoad != nullptr && keepOnLoad (withId->paramID)))   // merge: FEAT-JAM's keep-on-load wins
                {
                    /*  SPEC-SWEEP: PF-14 (docs/PRESET_FORMAT.md): a key the file
                        leaves out means that parameter's default, not whatever
                        the last preset set - so a hand-written three-line preset
                        is the same sound whatever was loaded before it. */
                    const float d = withId->getDefaultValue();

                    if (withId->getValue() != d)
                        withId->setValueNotifyingHost (d);
                }
            }
        }

        // mic-placement.md 4: the live legacy values this build saved beside
        // the mirror, so a round trip restores exactly what was there.
        if (obj->hasProperty (MicPlacementMigration::kLegacyBlockKey))
            MicPlacementMigration::restoreLiveLegacy (obj->getProperty (MicPlacementMigration::kLegacyBlockKey), apvts);

        /*  ambiguity-resolutions.md 1: a preset from before the physical loop
            switched feedback on with feedback_on, which no longer does anything;
            the loop is on when it has an amount. Half is where the old switch's
            default threshold began to sustain a loud note. */
        if (! params->hasProperty (ParamIDs::feedbackAmount)
              && (double) params->getProperty (ParamIDs::feedbackOn) > 0.5)
        {
            if (auto* amount = apvts.getParameter (ParamIDs::feedbackAmount))
                amount->setValueNotifyingHost (amount->convertTo0to1 (50.0f));

            migrations.add ("feedback");
        }

        /*  strum-dynamics.md 1.1: live chords cross at strum_crossing_sps. A
            preset from before it set strum_speed, ms per string - the same thing
            upside down. 0 ms meant no spread; the nearest is the fastest, 800. */
        if (! params->hasProperty (ParamIDs::strumCrossingSps) && params->hasProperty (ParamIDs::strumSpeed))
        {
            auto* oldSpeed = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::strumSpeed));
            auto* crossing = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::strumCrossingSps));

            if (oldSpeed != nullptr && crossing != nullptr)
            {
                const double ms = oldSpeed->convertFrom0to1 ((float) juce::jlimit (0.0, 1.0, (double) params->getProperty (ParamIDs::strumSpeed)));
                const double sps = ms > 0.0 ? 1000.0 / ms : 800.0;
                crossing->setValueNotifyingHost (crossing->convertTo0to1 ((float) juce::jlimit (20.0, 800.0, sps)));
                migrations.add ("strum speed");
            }
        }

        // ==== BEGIN REALISM-A legacy load ====
        /*  string-aging.md 8: a preset from before the continuous model had only
            the three-step choice. Its hours are that row's anchor and the
            detail is 0, which is the old table exactly - it sounds as it did.
            body-coupling.md 6: likewise without the body's return path. */
        {
            auto setPlain = [this] (const char* id, float plain)
            {
                if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
                    p->setValueNotifyingHost (p->convertTo0to1 (plain));
            };

            if (! params->hasProperty (ParamIDs::stringAgeHours))
            {
                int age = 1;   // the old default, Broken In

                if (auto* old = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (ParamIDs::stringAge)))
                    age = juce::jlimit (0, 2, juce::roundToInt (old->convertFrom0to1 (old->getValue())));

                static constexpr float kAnchorHours[] = { 0.0f, 12.0f, 120.0f };
                setPlain (ParamIDs::stringAgeHours, kAnchorHours[age]);
                setPlain (ParamIDs::stringAgeDetail, 0.0f);
                setPlain (ParamIDs::stringCoating, 0.0f);
            }

            if (! params->hasProperty (ParamIDs::bodyCouplingAmount))
                setPlain (ParamIDs::bodyCouplingAmount, 0.0f);
        }
        // ==== END REALISM-A legacy load ====

        /*  auto-articulation.md 8 (FEAT-ASSIST): a preset from before Performance
            Assist has no aa_* keys and loads with the defaults - off, which is
            the sound it was saved with - whatever the last preset had. */
        for (const char* id : { ParamIDs::aaEnabled, ParamIDs::aaStyle, ParamIDs::aaAmount, ParamIDs::aaRules })
            if (! params->hasProperty (id))
                if (auto* p = apvts.getParameter (id))
                    p->setValueNotifyingHost (p->getDefaultValue());

        /*  ambiguity-resolutions.md 3: the doubler became a post-amp pedal. A
            preset that had the old engine doubler on gets a Doubler in its first
            empty post-amp slot, at the pedal's own defaults (the old amount
            meant something else); with no slot free it goes without. */
        if ((double) params->getProperty (ParamIDs::doublerOn) > 0.5)
        {
            bool already = false;
            int freeSlot = -1;

            for (int slot = 0; slot < EffectsChain::kNumSlots; ++slot)
            {
                if (auto* type = apvts.getParameter (ParamIDs::slotType (true, slot)))
                {
                    const int index = juce::roundToInt (type->convertFrom0to1 (type->getValue()));
                    already = already || index == (int) PedalType::Doubler;

                    if (index == (int) PedalType::None && freeSlot < 0)
                        freeSlot = slot;
                }
            }

            if (! already && freeSlot >= 0)
            {
                if (auto* type = apvts.getParameter (ParamIDs::slotType (true, freeSlot)))
                    type->setValueNotifyingHost (type->convertTo0to1 ((float) (int) PedalType::Doubler));

                // The pedal's own defaults: the pedals are built keeping the
                // parameters a load wrote (onPedalTypesLoaded), so write them.
                if (auto doubler = Pedal::create (PedalType::Doubler))
                    for (int i = 0; i < juce::jmin (doubler->getNumParameters(), Pedal::kMaxParams); ++i)
                        if (auto* p = apvts.getParameter (ParamIDs::slotParam (true, freeSlot, i)))
                        {
                            const auto& d = doubler->getParameterDescriptor (i);
                            p->setValueNotifyingHost ((float) d.toNormalised (d.defaultValue));
                        }
            }

            if (auto* old = apvts.getParameter (ParamIDs::doublerOn))
                old->setValueNotifyingHost (0.0f);

            migrations.add ("doubler");
        }

        if (onPedalTypesLoaded != nullptr)
            onPedalTypesLoaded();

        /*  No block: derive per family from the plain values the parameters now
            hold. The stored numbers cannot be used for this - a normalised
            value is always inside whatever range is live and so carries no
            information about which one it was written against. */
        if (! hasRangesBlock)
        {
            incoming.deriveFromCurrentValues (apvts);

            if (incoming.isAnythingAdvanced())
            {
                ranges = incoming;
                ranges.applyTo (apvts);
            }
        }

        if (! migrations.isEmpty())
            noteMigration (migrations.joinIntoString (", "));
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

        extraStateValid = true;
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
    else
    {
        // No block (every factory preset): the defaults, over every string, not
        // whatever detune, gauges and temperament the last preset left.
        resetExtraState();
        extra.numStrings = kMaxStrings;
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
    else
    {
        engine.getMidiInterpreter().resetCcMapToDefaults();   // not the last preset's map
    }

    // Last, so the guitar type parameter it may depend on has its new value.
    if (onGuitarBlockLoaded != nullptr)
        onGuitarBlockLoaded (obj->getProperty ("guitar"));

    // tuning-stability.md 7 (REALISM-C): sigma from the preset's string age,
    // capoComp cleared, every offset cleared.
    if (auto* age = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::stringAge)))
        engine.getStabilityModel().beginPresetLoad ((StringAge) juce::jlimit (0, (int) StringAge::NumAges - 1, age->getIndex()));
    if (onJamBlockLoaded != nullptr)   // FEAT-JAM: a missing block means defaults
        onJamBlockLoaded (obj->getProperty ("jam"));

    // SPEC-SWEEP: SM-1/SM-16/FF-24..29 - after the parameters and the guitar,
    // so a snapshot bank or a mod route lands on the preset it belongs to.
    if (onPresetBlocksLoaded != nullptr)
        onPresetBlocksLoaded (*obj);
    if (onTechniquesBlockLoaded != nullptr)   // TECHNIQUES: engine-technique-layer.md 6-7
        onTechniquesBlockLoaded (obj->getProperty ("techniques"));

    currentName = obj->getProperty ("name").toString();
    currentCategory = obj->getProperty ("category").toString();
    currentUid = obj->getProperty ("uid").toString();                       // FEAT-BROWSER (5.4)
    currentPreviewPhrase = obj->getProperty ("previewPhrase").toString();

    // SPEC-SWEEP: FF-20 - `meta` first, the flat keys for files from before it.
    {
        const auto meta = obj->getProperty ("meta");

        if (meta.hasProperty ("name") && meta.getProperty ("name", {}).toString().isNotEmpty())
            currentName = meta.getProperty ("name", {}).toString();

        if (meta.hasProperty ("category") && meta.getProperty ("category", {}).toString().isNotEmpty())
            currentCategory = meta.getProperty ("category", {}).toString();

        metaCreated        = meta.getProperty ("created", {}).toString();
        metaModified       = meta.getProperty ("modified", {}).toString();
        metaVersionCreated = meta.getProperty ("version_created", {}).toString();
        metaNotes          = meta.getProperty ("notes", {}).toString();
    }

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
    extraStateValid = true;
    const auto& tuningEngine = engine.getTuningEngine();
    extra.numStrings = engine.getNumStrings();

    for (int i = 0; i < extra.numStrings; ++i)
    {
        const auto& t = tuningEngine.getStringTuning (i);
        extra.detuneCents[(size_t) i] = t.detuneCents;
        extra.realismDetuneCents[(size_t) i] = t.realismDetuneCents;
        // string-aging.md 5 (REALISM-A): the aging detune rides on the fine
        // tune and is rebuilt from the parameters on load, so only what is
        // left beyond it is state.
        extra.fineTuneCents[(size_t) i] = t.fineTuneCents - engine.getStringAging().computeNow (i).detuneCents;
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

namespace
{
    /*  SPEC-SWEEP: ER-9. juce::CharPointer_UTF8::isValidString does not check
        that continuation bytes are 10xxxxxx, so a Latin-1 "é" before a quote
        passes it. This checks the lead byte, every continuation byte, overlong
        forms and surrogates. */
    bool isStrictUtf8 (const juce::uint8* data, size_t size) noexcept
    {
        for (size_t i = 0; i < size;)
        {
            const auto b = data[i];

            if (b < 0x80) { ++i; continue; }

            int extra = 0;
            juce::uint32 cp = 0;

            if      ((b & 0xe0) == 0xc0) { extra = 1; cp = b & 0x1f; }
            else if ((b & 0xf0) == 0xe0) { extra = 2; cp = b & 0x0f; }
            else if ((b & 0xf8) == 0xf0) { extra = 3; cp = b & 0x07; }
            else return false;

            if (i + (size_t) extra >= size)
                return false;

            for (int k = 1; k <= extra; ++k)
            {
                const auto c = data[i + (size_t) k];

                if ((c & 0xc0) != 0x80)
                    return false;

                cp = (cp << 6) | (c & 0x3f);
            }

            static constexpr juce::uint32 minimum[] = { 0, 0x80, 0x800, 0x10000 };

            if (cp < minimum[extra] || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
                return false;

            i += (size_t) extra + 1;
        }

        return true;
    }
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

    // SPEC-SWEEP: ER-8, error-recovery 1: "no read permission".
    if (! file.hasReadAccess())
    {
        ErrorLog::write (ErrorLog::Severity::warn, "PresetSystem", "FILE_PERMISSION",
                         "Preset cannot be read (permission denied)", context());

        lastLoadError = "Cannot read " + file.getFileName() + " (permission denied).";
        return false;
    }

    juce::MemoryBlock bytes;

    if (! file.loadFileAsData (bytes) || bytes.getSize() == 0)
    {
        ErrorLog::write (ErrorLog::Severity::warn, "PresetSystem", "FILE_UNREADABLE",
                         "Preset could not be read, or is empty", context());

        lastLoadError = file.getFileName() + " could not be read, or is empty.";
        return false;
    }

    // SPEC-SWEEP: ER-9, error-recovery 1: "file is not UTF-8".
    if (! isStrictUtf8 (static_cast<const juce::uint8*> (bytes.getData()), bytes.getSize()))
    {
        ErrorLog::write (ErrorLog::Severity::warn, "PresetSystem", "NOT_UTF8",
                         "Preset is not UTF-8 text", context());

        lastLoadError = "File " + file.getFileName() + " is not a valid Luthier file.";
        return false;
    }

    const auto text = juce::String::fromUTF8 (static_cast<const char*> (bytes.getData()), (int) bytes.getSize());
    const auto parsed = juce::JSON::parse (text);

    if (! parsed.isObject())
    {
        // error-recovery 1: "file is not JSON". SPEC-SWEEP: ER-10, its wording.
        ErrorLog::write (ErrorLog::Severity::warn, "PresetSystem", "NOT_JSON",
                         "Preset is not valid JSON", context());

        lastLoadError = file.getFileName()
                          + " is not a valid Luthier file. Re-save it from a working install of Luthier.";
        return false;
    }

    if (! fromVar (parsed))
    {
        ErrorLog::write (ErrorLog::Severity::warn, "PresetSystem", "LOAD_REFUSED",
                         "Preset was refused; the file is untouched", context());

        /*  error-recovery 1 again: the session is left alone on a refusal, and
            saying so is the difference between a refusal and an apparent freeze.
            SPEC-SWEEP: ER-12/13 - a schema refusal says which. */
        lastLoadError = lastRefusal.isNotEmpty()
                          ? file.getFileName() + " " + lastRefusal
                          : file.getFileName() + " was refused. The current sound is unchanged.";
        return false;
    }

    currentName = file.getFileNameWithoutExtension();
    currentFile = file;
    applyExtraState();

    // file-formats 2 (MODEL-GAPS): a migrated file's original is kept.
    lastMigrationBackup = needsMigration (parsed)
                            ? backupMigratedOriginal (file, (int) parsed.getProperty ("schemaVersion", 0))
                            : juce::File();

    lastLoadError.clear();

    modified = false;

    sendChangeMessage();

    if (onPresetLoaded != nullptr)
        onPresetLoaded();   // output-normalization.md 4.4

    return true;
}

bool PresetManager::keepsValueWhenAbsent (const juce::String& paramId)
{
    // SPEC-SWEEP: PF-14. The morph slider is a performance control, and Slide
    // Mode persists across a load (state-model.md 8.1).
    // Merge: jam-mode 10's performance controls (jam_play, jam_fill_now) are
    // never in a preset, so a load leaves them alone too.
    return paramId == ParamIDs::presetMorphPosition
        || paramId == ParamIDs::slideGuitar
        || paramId == ParamIDs::slideMode
        || ParamIDs::isJamTransient (paramId);
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
juce::File PresetManager::backupFolderFor (const juce::File& target)
{
    // installer.md 8: ~/Documents/Luthier/Presets/Backup/<yyyy-mm-dd>/. The
    // root is the nearest ancestor called Presets (a user preset lives in
    // Presets/User/<category>/); outside any Presets tree, beside the file.
    for (auto dir = target.getParentDirectory(); ; dir = dir.getParentDirectory())
    {
        if (dir.getFileName() == "Presets")
            return dir.getChildFile ("Backup");

        if (dir.getParentDirectory() == dir)
            break;
    }

    return target.getParentDirectory().getChildFile ("Backup");
}

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

    auto folder = backupFolderFor (target).getChildFile (today);

    if (! folder.createDirectory())
        return;

    auto destination = folder.getChildFile (target.getFileName());

    // Several saves in one day keep several versions rather than one.
    for (int i = 2; destination.existsAsFile() && i < 1000; ++i)
        destination = folder.getChildFile (target.getFileNameWithoutExtension()
                                             + "-" + juce::String (i) + kFileExtension);

    target.copyFileTo (destination);
}

bool PresetManager::needsMigration (const juce::var& data)
{
    auto* obj = data.getDynamicObject();

    if (obj == nullptr)
        return false;

    // The spelling of the magic before file-formats.md named it.
    if (obj->getProperty ("magic").toString() != kMagic)
        return true;

    // Schema 1 (pre-M42): no ranges block.
    if (! obj->hasProperty ("ranges"))
        return true;

    // Pre-M49: the guitar by name rather than by reference.
    const auto guitar = obj->getProperty ("guitar");

    if (guitar.isObject() && guitar.hasProperty ("name") && ! guitar.hasProperty ("reference"))
        return true;

    // guitar-workshop.md 9: pickup placements that were parameters.
    if (auto* params = obj->getProperty ("parameters").getDynamicObject())
        for (int slot = 0; slot < 3; ++slot)
            if (params->hasProperty (ParamIDs::pickupPosition (slot)))
                return true;

    return false;
}

juce::File PresetManager::backupMigratedOriginal (const juce::File& original, int schema)
{
    if (! original.existsAsFile())
        return {};

    const auto backups = original.getParentDirectory().getChildFile ("Backup");
    const auto name = original.getFileNameWithoutExtension() + "-v" + juce::String (juce::jmax (0, schema)) + kFileExtension;

    // Once per file and schema, whichever day it was filed.
    if (backups.isDirectory())
        for (const auto& entry : juce::RangedDirectoryIterator (backups, false, "*", juce::File::findDirectories))
            if (entry.getFile().getChildFile (name).existsAsFile())
                return entry.getFile().getChildFile (name);

    auto folder = backups.getChildFile (juce::Time::getCurrentTime().formatted ("%Y-%m-%d"));

    if (! folder.createDirectory())
        return {};

    const auto destination = folder.getChildFile (name);
    return original.copyFileTo (destination) ? destination : juce::File();
}

std::atomic<int> PresetManager::failNextWriteForTesting { 0 };

void PresetManager::pruneOldBackups()
{
    // installer.md 8: Presets/Backup is the current place; the two below it
    // are where earlier builds filed backups. SPEC-SWEEP PF-7: each root's
    // per-category Backup folders (where earlier builds filed a user preset's
    // backup) are swept too.
    for (const auto& root : { getUserPresetFolder().getParentDirectory(), getUserPresetFolder(), getFactoryPresetFolder() })
        pruneOldBackupsUnder (root, juce::Time::getCurrentTime());
}

void PresetManager::pruneOldBackupsUnder (const juce::File& root, juce::Time now)
{
    const auto cutoff = now - juce::RelativeTime::days ((double) kBackupRetentionDays);

    /*  SPEC-SWEEP: PF-7. backupBeforeOverwrite files a replaced preset beside
        it, so a user preset's backups are in <root>/<Category>/Backup; the
        sweep used to look only in <root>/Backup and never found them. */
    juce::Array<juce::File> backupFolders { root.getChildFile ("Backup") };

    for (const auto& category : root.findChildFiles (juce::File::findDirectories, false))
        if (category.getFileName() != "Backup")
            backupFolders.add (category.getChildFile ("Backup"));

    for (const auto& backups : backupFolders)
    {
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
    lastSaveError.clear();

    // SPEC-SWEEP: FF-32/PF-5 - the injected failure, taken once.
    const int injected = failNextWriteForTesting.exchange (0);

    file.getParentDirectory().createDirectory();

    juce::TemporaryFile temp (file);

    auto stream = injected == 1 ? std::unique_ptr<juce::FileOutputStream>()
                                : temp.getFile().createOutputStream();

    if (stream != nullptr && ! stream->failedToOpen())
    {
        stream->setPosition (0);
        stream->truncate();
        stream->writeText (juce::JSON::toString (data, false), false, false, "\n");
        stream->flush();

        // SPEC-SWEEP: ER-20 - a full disk shows up as a failed write, and the
        // target must not be replaced by a truncated file.
        const bool written = stream->getStatus().wasOk();
        stream.reset();

        /*  The backup is taken once the new version is safely in the temp
            file, so a save that cannot be written leaves no backup of a file
            it never replaced. */
        if (written)
            backupBeforeOverwrite (file);

        if (written && injected != 2 && temp.overwriteTargetFileWithTemporary())
            return true;

        lastSaveError = "Could not save " + file.getFileName()
                          + ". The previous version is intact.";

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
    lastSaveError = "Cannot save to " + file.getParentDirectory().getFullPathName()
                      + " (permission denied or disk full).";

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

void PresetManager::stampSaveTime()
{
    // SPEC-SWEEP: FF-20. `created` survives every later save.
    metaModified = juce::Time::getCurrentTime().toISO8601 (true);

    if (metaCreated.isEmpty())
        metaCreated = metaModified;

    if (metaVersionCreated.isEmpty())
        metaVersionCreated = JucePlugin_VersionString;
}

bool PresetManager::saveCurrent()
{
    const auto* info = getPreset (currentIndex);

    // A factory preset is never overwritten: saving one becomes a user copy.
    if (info == nullptr || info->isFactory)
        return saveAs (currentName, "User");

    captureExtraState();
    stampSaveTime();   // SPEC-SWEEP: FF-20

    // preset-browser-previews 5.4: a uid on the first save of a user preset.
    if (currentUid.isEmpty() || currentUid.startsWith ("factory:"))
        currentUid = info->uid.isNotEmpty() && ! info->uid.startsWith ("factory:") ? info->uid
                                                                                   : juce::Uuid().toString();

    if (writeToFile (info->file, toVar (info->name, info->category, info->description, info->tags)))
    {
        modified = false;
        const auto savedFile = info->file;
        sendChangeMessage();

        if (onPresetSaved)
            onPresetSaved (savedFile);   // 2: a high-priority preview render

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
    stampSaveTime();   // SPEC-SWEEP: FF-20

    // preset-browser-previews 5.4: a new file gets a new uid; saving over an
    // existing one keeps its uid, so its favourite and rating stay with it.
    {
        const auto previousUid = currentUid;
        juce::String existingUid;

        if (file.existsAsFile())
            existingUid = juce::JSON::parse (file.loadFileAsString()).getProperty ("uid", {}).toString();

        currentUid = existingUid.isNotEmpty() ? existingUid : juce::Uuid().toString();

        if (! writeToFile (file, toVar (name, safeCategory, description, tags)))
        {
            currentUid = previousUid;
            return false;
        }
    }

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

    if (onPresetSaved)
        onPresetSaved (file);   // preset-browser-previews 2 (FEAT-BROWSER)

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
    stampSaveTime();   // SPEC-SWEEP: FF-20
    return writeToFile (destination, toVar (currentName, currentCategory));
}

//==============================================================================
bool PresetManager::defaultMainsRegionIs50Hz()
{
    // noise-floor.md 3: Auto from the OS region, or the user's 50 / 60.
    const int pref = UiPreferences::get().getInt ("defaultMainsRegion", 0);

    if (pref == 1) return true;
    if (pref == 2) return false;

    static const juce::StringArray sixtyHz { "US", "CA", "MX", "BR", "CO", "VE", "KR", "TW", "PH", "SA",
                                             "CR", "PA", "GT", "HN", "NI", "SV", "DO", "PR", "CU", "EC",
                                             "PE", "JP", "LR", "BS", "BZ", "GU", "AS", "TT" };
    const auto region = juce::SystemStats::getUserRegion().toUpperCase();
    return region.isNotEmpty() && ! sixtyHz.contains (region);
}

void PresetManager::resetExtraState()
{
    extra = ExtraState {};
    extra.customGaugeInches.fill (0.0);
    extra.detuneCents.fill (0.0);
    extra.realismDetuneCents.fill (0.0);
    extra.fineTuneCents.fill (0.0);
    extra.openFrequencyHz.fill (0.0);
    extra.stringMuted.fill (false);

    for (int i = 0; i < 12; ++i)
        extra.customTemperament[(size_t) i] = std::pow (2.0, i / 12.0);
}

void PresetManager::resetToDefaults()
{
    for (auto* p : processor.getParameters())
        p->setValueNotifyingHost (p->getDefaultValue());

    resetExtraState();

    engine.getMidiInterpreter().resetCcMapToDefaults();
    applyExtraState();

    // noise-floor.md 3 (REALISM-C): the user's default mains region seeds an
    // Init preset. A loaded preset keeps its own.
    if (auto* mains = apvts.getParameter (ParamIDs::noiseMainsHz))
        mains->setValueNotifyingHost (mains->convertTo0to1 (defaultMainsRegionIs50Hz() ? 1.0f : 0.0f));

    // The default guitar type's factory guitar, as shipped: its parts win over the
    // layout defaults just written (a reset used to leave an X-braced spruce top
    // and 500k pots on the default solid-body, BETA_TEST_REPORT B-05).
    if (onGuitarBlockLoaded != nullptr)
    {
        auto* block = new juce::DynamicObject();
        block->setProperty ("partsWin", true);
        onGuitarBlockLoaded (juce::var (block));
    }

    if (onJamBlockLoaded != nullptr)   // FEAT-JAM
        onJamBlockLoaded ({});

    if (onTechniquesBlockLoaded != nullptr)   // TECHNIQUES
        onTechniquesBlockLoaded ({});

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

    // tune-builder 14 (TUNE-HELP-ONBOARDING): the tune's timeline controls are
    // not part of a sound.
    return ! excluded.contains (paramId) && ! paramId.startsWith ("tune_")
        && ! ParamIDs::isJamTransient (paramId);   // FEAT-JAM
}

void PresetManager::randomise (uint64_t seed, const juce::StringArray& lockedParameters,
                               bool respectStockRanges)
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

        // A physical parameter on its advanced range draws from the stock part
        // of it, so "random" still means "a guitar that could exist".
        if (respectStockRanges)
            if (const auto* physical = RangeRegistry::find (id))
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                {
                    const double lo = ranged->convertTo0to1 (physical->stockMin);
                    const double hi = ranged->convertTo0to1 (physical->stockMax);
                    v = lo + v * (hi - lo);
                }

        p->setValueNotifyingHost ((float) v);
    }

    // mic-placement.md 9 (FEAT-MIC): respecting stock ranges keeps each mic on
    // the cone (u <= 1) and within 30 cm, where a real session puts it.
    if (respectStockRanges)
        MicPlacementMigration::keepPlacementPlausible (apvts, lockedParameters);

    currentName = "Random";
    modified = true;
    sendChangeMessage();
}

} // namespace luthier
