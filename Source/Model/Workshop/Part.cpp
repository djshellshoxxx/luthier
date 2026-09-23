#include "Part.h"

namespace luthier
{

namespace
{
    struct TypeRow { PartType type; const char* id; const char* folder; };

    const TypeRow typeRows[] =
    {
        { PartType::body,      "body",      "Bodies" },
        { PartType::top,       "top",       "Tops" },
        { PartType::neck,      "neck",      "Necks" },
        { PartType::fretboard, "fretboard", "Fretboards" },
        { PartType::frets,     "frets",     "Frets" },
        { PartType::nut,       "nut",       "Nuts" },
        { PartType::bridge,    "bridge",    "Bridges" },
        { PartType::tailpiece, "tailpiece", "Tailpieces" },
        { PartType::tuners,    "tuners",    "Tuners" },
        { PartType::pickup,    "pickup",    "Pickups" },
        { PartType::wiring,    "wiring",    "Wiring" },
        { PartType::strings,   "strings",   "Strings" },
        { PartType::pickguard, "pickguard", "Pickguards" },
        { PartType::slide,     "slide",     "Slides" },
        { PartType::pick,      "pick",      "Picks" },
        { PartType::capo,      "capo",      "Capos" },
    };

    juce::StringArray toStrings (const juce::var& v)
    {
        juce::StringArray out;

        if (auto* array = v.getArray())
            for (const auto& item : *array)
                out.add (item.toString());

        return out;
    }

    juce::var fromStrings (const juce::StringArray& strings)
    {
        juce::Array<juce::var> out;

        for (const auto& s : strings)
            out.add (s);

        return out;
    }
}

const char* getPartTypeId (PartType t) noexcept
{
    for (const auto& row : typeRows)
        if (row.type == t)
            return row.id;

    return "";
}

const char* getPartCategoryFolder (PartType t) noexcept
{
    for (const auto& row : typeRows)
        if (row.type == t)
            return row.folder;

    return "";
}

PartType partTypeFromId (const juce::String& id) noexcept
{
    for (const auto& row : typeRows)
        if (id == row.id)
            return row.type;

    return PartType::numTypes;
}

//==============================================================================
double Part::number (const char* field, double fallback) const
{
    const auto v = fields.getProperty (field, juce::var());

    if (v.isDouble() || v.isInt() || v.isInt64())
        return (double) v;

    return fallback;
}

juce::String Part::text (const char* field, const juce::String& fallback) const
{
    const auto v = fields.getProperty (field, juce::var());
    return v.isString() ? v.toString() : fallback;
}

bool Part::flag (const char* field, bool fallback) const
{
    const auto v = fields.getProperty (field, juce::var());
    return v.isBool() ? (bool) v : fallback;
}

juce::Array<double> Part::numbers (const char* field) const
{
    juce::Array<double> out;

    if (auto* array = fields.getProperty (field, juce::var()).getArray())
        for (const auto& item : *array)
            out.add ((double) item);

    return out;
}

bool Part::suits (const juce::String& family) const
{
    return compatibility.isEmpty() || compatibility.contains ("any") || compatibility.contains (family);
}

juce::String Part::getReferenceName() const
{
    return juce::String (getPartCategoryFolder (type)) + "/" + name + kExtension;
}

//==============================================================================
juce::var Part::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("schema", kSchema);
    root->setProperty ("magic", kMagic);

    auto* meta = new juce::DynamicObject();
    meta->setProperty ("name", name);
    meta->setProperty ("part_type", getPartTypeId (type));
    meta->setProperty ("author", author);
    meta->setProperty ("tags", fromStrings (tags));
    meta->setProperty ("compatibility", fromStrings (compatibility));

    if (isCategoryDefault)
        meta->setProperty ("is_default", true);

    root->setProperty ("meta", juce::var (meta));
    root->setProperty ("fields", fields.isObject() ? fields : juce::var (new juce::DynamicObject()));

    if (! illustration.isVoid())
        root->setProperty ("illustration", illustration);

    return juce::var (root);
}

std::shared_ptr<Part> Part::fromVar (const juce::var& json, juce::String& error)
{
    if (json.getProperty ("magic", juce::var()).toString() != kMagic)
    {
        error = "not a Luthier part (no \"luthier.part\" marker)";
        return nullptr;
    }

    const auto meta = json.getProperty ("meta", juce::var());
    auto part = std::make_shared<Part>();

    part->name = meta.getProperty ("name", juce::var()).toString().trim();
    part->type = partTypeFromId (meta.getProperty ("part_type", juce::var()).toString());
    part->author = meta.getProperty ("author", juce::var()).toString();
    part->tags = toStrings (meta.getProperty ("tags", juce::var()));
    part->compatibility = toStrings (meta.getProperty ("compatibility", juce::var()));
    part->isCategoryDefault = (bool) meta.getProperty ("is_default", false);

    if (part->name.isEmpty())
    {
        error = "the part has no name";
        return nullptr;
    }

    if (part->type == PartType::numTypes)
    {
        error = "unknown part type \"" + meta.getProperty ("part_type", juce::var()).toString() + "\"";
        return nullptr;
    }

    part->fields = json.getProperty ("fields", juce::var (new juce::DynamicObject()));
    part->illustration = json.getProperty ("illustration", juce::var());
    return part;
}

std::shared_ptr<Part> Part::load (const juce::File& file, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = file.getFileName() + " does not exist";
        return nullptr;
    }

    juce::var json;
    const auto result = juce::JSON::parse (file.loadFileAsString(), json);

    if (result.failed())
    {
        error = file.getFileName() + ": " + result.getErrorMessage();
        return nullptr;
    }

    auto part = fromVar (json, error);

    if (part != nullptr)
        part->file = file;
    else
        error = file.getFileName() + ": " + error;

    return part;
}

bool Part::save (const juce::File& destination) const
{
    destination.getParentDirectory().createDirectory();

    // Written beside and moved into place, so a crash mid-write never leaves
    // half a part where a whole one was (file-formats.md).
    const auto temp = destination.getSiblingFile (destination.getFileName() + ".tmp");

    if (! temp.replaceWithText (juce::JSON::toString (toVar(), false)))
        return false;

    return temp.moveFileTo (destination);
}

} // namespace luthier
