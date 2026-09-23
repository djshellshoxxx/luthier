#include "PartLibrary.h"
#include "../../Support/ThreadProbe.h"
#include "../../Support/IrLibrary.h"

namespace luthier
{

//==============================================================================
namespace
{
    struct SlotRow { GuitarSlot slot; const char* id; PartType type; bool required; };

    const SlotRow slotRows[] =
    {
        { GuitarSlot::body,         "body",           PartType::body,      true  },
        { GuitarSlot::top,          "top",            PartType::top,       false },
        { GuitarSlot::neck,         "neck",           PartType::neck,      true  },
        { GuitarSlot::fretboard,    "fretboard",      PartType::fretboard, true  },
        { GuitarSlot::frets,        "frets",          PartType::frets,     true  },
        { GuitarSlot::nut,          "nut",            PartType::nut,       true  },
        { GuitarSlot::bridge,       "bridge",         PartType::bridge,    true  },
        { GuitarSlot::tailpiece,    "tailpiece",      PartType::tailpiece, false },
        { GuitarSlot::tuners,       "tuners",         PartType::tuners,    true  },
        { GuitarSlot::pickupNeck,   "pickups.neck",   PartType::pickup,    false },
        { GuitarSlot::pickupMiddle, "pickups.middle", PartType::pickup,    false },
        { GuitarSlot::pickupBridge, "pickups.bridge", PartType::pickup,    false },
        { GuitarSlot::wiring,       "wiring",         PartType::wiring,    true  },
        { GuitarSlot::strings,      "strings",        PartType::strings,   true  },
        { GuitarSlot::pickguard,    "pickguard",      PartType::pickguard, false },
    };

    const char* const pickupKeys[3] = { "neck", "middle", "bridge" };

    juce::var numbersToVar (const juce::Array<double>& values)
    {
        juce::Array<juce::var> out;

        for (auto v : values)
            out.add (v);

        return out;
    }

    juce::Array<double> varToNumbers (const juce::var& v)
    {
        juce::Array<double> out;

        if (auto* array = v.getArray())
            for (const auto& item : *array)
                out.add ((double) item);

        return out;
    }

    juce::String displayNameOf (PartType type)
    {
        auto id = juce::String (getPartTypeId (type));
        return id.substring (0, 1).toUpperCase() + id.substring (1);
    }
}

const char* getSlotId (GuitarSlot slot) noexcept
{
    for (const auto& row : slotRows)
        if (row.slot == slot)
            return row.id;

    return "";
}

PartType getSlotPartType (GuitarSlot slot) noexcept
{
    for (const auto& row : slotRows)
        if (row.slot == slot)
            return row.type;

    return PartType::numTypes;
}

bool isSlotRequired (GuitarSlot slot) noexcept
{
    for (const auto& row : slotRows)
        if (row.slot == slot)
            return row.required;

    return false;
}

//==============================================================================
//  WorkshopGuitar
//==============================================================================
GuitarSlot WorkshopGuitar::pickupSlot (int index) noexcept
{
    switch (index)
    {
        case 0:  return GuitarSlot::pickupNeck;
        case 1:  return GuitarSlot::pickupMiddle;
        default: return GuitarSlot::pickupBridge;
    }
}

int WorkshopGuitar::getStringCount (int* excess) const noexcept
{
    const auto neckStrings = get (GuitarSlot::neck) != nullptr ? (int) get (GuitarSlot::neck)->number ("strings", 6.0) : 6;
    const auto bridgeStrings = get (GuitarSlot::bridge) != nullptr ? (int) get (GuitarSlot::bridge)->number ("strings", 6.0) : 6;

    const int count = juce::jlimit (1, 12, juce::jmin (neckStrings, bridgeStrings));

    if (excess != nullptr)
        *excess = std::abs (neckStrings - bridgeStrings);

    return count;
}

juce::StringArray WorkshopGuitar::getCompatibilityWarnings() const
{
    juce::StringArray warnings;

    for (int i = 0; i < kNumGuitarSlots; ++i)
        if (const auto& part = parts[(size_t) i]; part != nullptr && ! part->suits (family))
            warnings.add ("This is a " + (part->compatibility.isEmpty() ? juce::String ("other")
                                                                        : part->compatibility[0])
                          + " " + juce::String (getPartTypeId (part->type)) + " on "
                          + (family.startsWithIgnoreCase ("a") || family.startsWithIgnoreCase ("e") ? "an " : "a ")
                          + family + " guitar. It will work; it is an unusual combination.");

    return warnings;
}

juce::var WorkshopGuitar::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("schema", kSchema);
    root->setProperty ("magic", kMagic);

    auto* meta = new juce::DynamicObject();
    meta->setProperty ("name", name);
    meta->setProperty ("family", family);
    meta->setProperty ("body_style", bodyStyle);
    meta->setProperty ("author", author);

    juce::Array<juce::var> tagArray;
    for (const auto& t : tags) tagArray.add (t);
    meta->setProperty ("tags", tagArray);

    root->setProperty ("meta", juce::var (meta));

    auto* partsObject = new juce::DynamicObject();
    auto* pickups = new juce::DynamicObject();

    for (int i = 0; i < kNumGuitarSlots; ++i)
    {
        const auto slot = (GuitarSlot) i;
        const auto& part = parts[(size_t) i];

        const bool isPickup = getSlotPartType (slot) == PartType::pickup;

        juce::var entry;

        if (part != nullptr)
        {
            auto* ref = new juce::DynamicObject();
            ref->setProperty ("reference", part->getReferenceName());
            entry = juce::var (ref);
        }

        if (isPickup)
        {
            const int index = i - (int) GuitarSlot::pickupNeck;

            if (auto* ref = entry.getDynamicObject())
            {
                const auto& placement = placements[(size_t) index];
                ref->setProperty ("position_mm", placement.positionMm);
                ref->setProperty ("height_treble_mm", placement.heightTrebleMm);
                ref->setProperty ("height_bass_mm", placement.heightBassMm);
            }

            pickups->setProperty (pickupKeys[index], entry);
        }
        else
        {
            partsObject->setProperty (getSlotId (slot), entry);
        }
    }

    partsObject->setProperty ("pickups", juce::var (pickups));
    partsObject->setProperty ("hardware_color", hardwareColour);

    auto* finishObject = new juce::DynamicObject();
    finishObject->setProperty ("type", finish.type);
    finishObject->setProperty ("color_a", finish.colourA);
    finishObject->setProperty ("color_b", finish.colourB);
    finishObject->setProperty ("burst_shape", finish.burstShape);
    finishObject->setProperty ("gloss", finish.gloss);
    finishObject->setProperty ("aging", finish.aging);
    partsObject->setProperty ("finish", juce::var (finishObject));

    root->setProperty ("parts", juce::var (partsObject));

    auto* setupObject = new juce::DynamicObject();
    setupObject->setProperty ("action_treble_mm", setup.actionTrebleMm);
    setupObject->setProperty ("action_bass_mm", setup.actionBassMm);
    setupObject->setProperty ("relief_mm", setup.reliefMm);
    setupObject->setProperty ("nut_slot_depths_mm", numbersToVar (setup.nutSlotDepthsMm));
    setupObject->setProperty ("intonation_mm", numbersToVar (setup.intonationMm));
    root->setProperty ("setup", juce::var (setupObject));

    // As a string: JSON numbers are doubles, and a 64-bit seed does not survive one.
    root->setProperty ("character_seed", juce::String ((juce::int64) seed));
    return juce::var (root);
}

juce::var WorkshopGuitar::toEmbeddedVar() const
{
    auto json = toVar();

    // Each reference gains the part itself, so resolution can fall back to it
    // on a machine that has never seen the part file.
    auto embed = [] (juce::var& entry, const PartPtr& part)
    {
        if (auto* ref = entry.getDynamicObject(); ref != nullptr && part != nullptr)
            ref->setProperty ("embedded", part->toVar());
    };

    auto partsObject = json.getProperty ("parts", juce::var());

    for (int i = 0; i < kNumGuitarSlots; ++i)
    {
        const auto slot = (GuitarSlot) i;

        if (getSlotPartType (slot) == PartType::pickup)
        {
            auto entry = partsObject.getProperty ("pickups", juce::var())
                                    .getProperty (pickupKeys[i - (int) GuitarSlot::pickupNeck], juce::var());
            embed (entry, parts[(size_t) i]);
        }
        else
        {
            auto entry = partsObject.getProperty (getSlotId (slot), juce::var());
            embed (entry, parts[(size_t) i]);
        }
    }

    return json;
}

bool WorkshopGuitar::save (const juce::File& destination) const
{
    ThreadProbe::noteFileAccess();
    destination.getParentDirectory().createDirectory();
    const auto temp = destination.getSiblingFile (destination.getFileName() + ".tmp");

    if (! temp.replaceWithText (juce::JSON::toString (toVar(), false)))
        return false;

    return temp.moveFileTo (destination);
}

bool WorkshopGuitar::operator== (const WorkshopGuitar& o) const
{
    if (name != o.name || family != o.family || bodyStyle != o.bodyStyle || seed != o.seed
        || hardwareColour != o.hardwareColour)
        return false;

    for (int i = 0; i < kNumGuitarSlots; ++i)
    {
        const auto& a = parts[(size_t) i];
        const auto& b = o.parts[(size_t) i];

        if ((a == nullptr) != (b == nullptr))
            return false;

        if (a != nullptr && (a->name != b->name || a->type != b->type
                             || juce::JSON::toString (a->fields) != juce::JSON::toString (b->fields)))
            return false;
    }

    for (size_t i = 0; i < placements.size(); ++i)
        if (placements[i].positionMm != o.placements[i].positionMm
            || placements[i].heightTrebleMm != o.placements[i].heightTrebleMm
            || placements[i].heightBassMm != o.placements[i].heightBassMm)
            return false;

    return setup.actionTrebleMm == o.setup.actionTrebleMm && setup.actionBassMm == o.setup.actionBassMm
        && setup.reliefMm == o.setup.reliefMm && setup.nutSlotDepthsMm == o.setup.nutSlotDepthsMm
        && setup.intonationMm == o.setup.intonationMm
        && finish.type == o.finish.type && finish.colourA == o.finish.colourA
        && finish.colourB == o.finish.colourB && finish.burstShape == o.finish.burstShape
        && finish.gloss == o.finish.gloss && finish.aging == o.finish.aging;
}

//==============================================================================
//  PartLibrary
//==============================================================================
PartLibrary::PartLibrary() = default;

juce::File PartLibrary::getFactoryPartsFolder()   { return IrLibrary::getResourcesFolder().getChildFile ("Parts"); }
juce::File PartLibrary::getFactoryGuitarsFolder() { return IrLibrary::getResourcesFolder().getChildFile ("Guitars"); }

juce::File PartLibrary::getUserPartsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier").getChildFile ("Parts");
}

juce::File PartLibrary::getUserGuitarsFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier").getChildFile ("Guitars");
}

void PartLibrary::refresh()
{
    refreshFrom (getFactoryPartsFolder(), getUserPartsFolder());
}

void PartLibrary::refreshFrom (const juce::File& factoryParts, const juce::File& userParts)
{
    for (auto& map : byType)
        map.clear();

    scanErrors.clear();

    // Factory first, then user: a user part of the same name replaces it (4).
    scanFolder (factoryParts, true);
    scanFolder (userParts, false);
}

void PartLibrary::scanFolder (const juce::File& root, bool factory)
{
    ThreadProbe::noteFileAccess();
    if (! root.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator (root, true, juce::String ("*") + Part::kExtension))
    {
        juce::String error;
        auto part = Part::load (entry.getFile(), error);

        if (part == nullptr)
        {
            scanErrors.add (error);
            continue;
        }

        part->isFactory = factory;

        // Only factory parts can be a category's fallback: a user part named
        // like a default is a user's part, not the thing to fall back to.
        if (! factory)
            part->isCategoryDefault = false;

        byType[(size_t) part->type][part->name.toLowerCase()] = part;
    }
}

juce::Array<PartPtr> PartLibrary::getParts (PartType type) const
{
    juce::Array<PartPtr> out;

    if (type != PartType::numTypes)
        for (const auto& [key, part] : byType[(size_t) type])
            out.add (part);

    return out;
}

int PartLibrary::getNumParts() const noexcept
{
    int n = 0;

    for (const auto& map : byType)
        n += (int) map.size();

    return n;
}

PartPtr PartLibrary::find (PartType type, const juce::String& name) const
{
    if (type == PartType::numTypes)
        return nullptr;

    const auto& map = byType[(size_t) type];
    const auto it = map.find (name.trim().toLowerCase());
    return it != map.end() ? it->second : nullptr;
}

PartPtr PartLibrary::resolve (const juce::String& reference, PartType expected) const
{
    // "Factory/Bodies/Alder Double-Cut.luthierpart" -> "Alder Double-Cut".
    const auto name = reference.fromLastOccurrenceOf ("/", false, false)
                               .upToLastOccurrenceOf (Part::kExtension, false, true);

    return find (expected, name.isNotEmpty() ? name : reference);
}

PartPtr PartLibrary::getDefault (PartType type) const
{
    if (type == PartType::numTypes)
        return nullptr;

    PartPtr anyFactory;

    for (const auto& [key, part] : byType[(size_t) type])
    {
        if (part->isFactory && part->isCategoryDefault)
            return part;

        if (part->isFactory && anyFactory == nullptr)
            anyFactory = part;
    }

    return anyFactory;
}

bool PartLibrary::buildGuitar (const juce::var& json, WorkshopGuitar& out, LoadReport& report) const
{
    if (json.getProperty ("magic", juce::var()).toString() != WorkshopGuitar::kMagic)
    {
        report.errors.add ("not a Luthier guitar (no \"luthier.guitar\" marker)");
        return false;
    }

    WorkshopGuitar g;

    const auto meta = json.getProperty ("meta", juce::var());
    g.name = meta.getProperty ("name", "Untitled").toString();
    g.family = meta.getProperty ("family", "electric").toString();
    g.bodyStyle = meta.getProperty ("body_style", juce::var()).toString();
    g.author = meta.getProperty ("author", juce::var()).toString();

    if (auto* tags = meta.getProperty ("tags", juce::var()).getArray())
        for (const auto& t : *tags)
            g.tags.add (t.toString());

    const auto partsObject = json.getProperty ("parts", juce::var());

    for (int i = 0; i < kNumGuitarSlots; ++i)
    {
        const auto slot = (GuitarSlot) i;
        const auto type = getSlotPartType (slot);
        const bool isPickup = type == PartType::pickup;
        const int pickupIndex = i - (int) GuitarSlot::pickupNeck;

        const auto entry = isPickup ? partsObject.getProperty ("pickups", juce::var())
                                                 .getProperty (pickupKeys[pickupIndex], juce::var())
                                    : partsObject.getProperty (getSlotId (slot), juce::var());

        if (entry.isVoid() || ! entry.isObject())
        {
            // An empty optional slot is legal - zero pickups is an acoustic.
            if (isSlotRequired (slot))
            {
                g.parts[(size_t) i] = getDefault (type);
                report.missing.add (displayNameOf (type) + " not specified, using factory default.");
            }

            continue;
        }

        const auto reference = entry.getProperty ("reference", juce::var()).toString();
        PartPtr part = resolve (reference, type);

        // A preset's embedded override carries the part itself (8): used when
        // this machine does not have the file.
        if (part == nullptr && entry.hasProperty ("embedded"))
        {
            juce::String error;

            if (auto embedded = Part::fromVar (entry.getProperty ("embedded", juce::var()), error))
                part = embedded;
        }

        if (part == nullptr)
        {
            part = getDefault (type);

            const auto shownName = reference.fromLastOccurrenceOf ("/", false, false)
                                            .upToLastOccurrenceOf (Part::kExtension, false, true);

            report.missing.add (displayNameOf (type) + " " + (shownName.isNotEmpty() ? shownName : reference)
                                + " not found, using factory default.");
        }

        g.parts[(size_t) i] = part;

        if (isPickup)
        {
            auto& placement = g.placements[(size_t) pickupIndex];
            placement.positionMm = (double) entry.getProperty ("position_mm", placement.positionMm);
            placement.heightTrebleMm = (double) entry.getProperty ("height_treble_mm", placement.heightTrebleMm);
            placement.heightBassMm = (double) entry.getProperty ("height_bass_mm", placement.heightBassMm);
        }
    }

    g.hardwareColour = partsObject.getProperty ("hardware_color", "nickel").toString();

    const auto finish = partsObject.getProperty ("finish", juce::var());
    g.finish.type = finish.getProperty ("type", g.finish.type).toString();
    g.finish.colourA = finish.getProperty ("color_a", g.finish.colourA).toString();
    g.finish.colourB = finish.getProperty ("color_b", g.finish.colourB).toString();
    g.finish.burstShape = finish.getProperty ("burst_shape", g.finish.burstShape).toString();
    g.finish.gloss = (double) finish.getProperty ("gloss", g.finish.gloss);
    g.finish.aging = (double) finish.getProperty ("aging", g.finish.aging);

    const auto setup = json.getProperty ("setup", juce::var());
    g.setup.actionTrebleMm = (double) setup.getProperty ("action_treble_mm", g.setup.actionTrebleMm);
    g.setup.actionBassMm = (double) setup.getProperty ("action_bass_mm", g.setup.actionBassMm);
    g.setup.reliefMm = (double) setup.getProperty ("relief_mm", g.setup.reliefMm);
    g.setup.nutSlotDepthsMm = varToNumbers (setup.getProperty ("nut_slot_depths_mm", juce::var()));
    g.setup.intonationMm = varToNumbers (setup.getProperty ("intonation_mm", juce::var()));

    g.seed = (juce::uint64) json.getProperty ("character_seed", 0).toString().getLargeIntValue();

    g.getStringCount (&report.stringExcess);
    report.warnings.addArray (g.getCompatibilityWarnings());

    out = g;
    return true;
}

bool PartLibrary::loadGuitar (const juce::File& file, WorkshopGuitar& out, LoadReport& report) const
{
    ThreadProbe::noteFileAccess();
    juce::var json;
    const auto result = juce::JSON::parse (file.loadFileAsString(), json);

    if (result.failed())
    {
        report.errors.add (file.getFileName() + ": " + result.getErrorMessage());
        return false;
    }

    return buildGuitar (json, out, report);
}

juce::Array<juce::File> PartLibrary::getGuitarFiles() const
{
    juce::Array<juce::File> files;

    for (const auto& root : { getFactoryGuitarsFolder(), getUserGuitarsFolder() })
        if (root.isDirectory())
            for (const auto& entry : juce::RangedDirectoryIterator (root, true, juce::String ("*") + WorkshopGuitar::kExtension))
                files.add (entry.getFile());

    return files;
}

} // namespace luthier
