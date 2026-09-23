#pragma once

/*  A swappable guitar part (guitar-workshop.md 2, file-formats.md 4).

    One schema, many part types. The `fields` object's shape depends on the
    type and is documented field by field in part-acoustics.md; this class
    does not interpret it. It holds the fields as parsed JSON and offers typed
    reads with fallbacks, because a user part may omit a field and a missing
    field must fall back to something sensible rather than to zero.

    Parts are immutable once loaded and shared by pointer: a guitar holds
    `std::shared_ptr<const Part>`, so a hundred presets using one pickup hold
    one pickup.
*/

#include <juce_core/juce_core.h>
#include <memory>

namespace luthier
{

//==============================================================================
enum class PartType
{
    body = 0, top, neck, fretboard, frets, nut, bridge, tailpiece, tuners,
    pickup, wiring, strings, pickguard,
    slide, pick, capo,       // the player's accessories, not fitted to the guitar
    numTypes
};

const char* getPartTypeId (PartType t) noexcept;          ///< "body", "pickup", ...
const char* getPartCategoryFolder (PartType t) noexcept;  ///< "Bodies", "Pickups", ...
PartType partTypeFromId (const juce::String& id) noexcept;  ///< numTypes if unknown

/** Guitar families (guitar-workshop.md 5). */
inline const juce::StringArray& getGuitarFamilies()
{
    static const juce::StringArray families { "electric", "acoustic", "bass", "classical", "resonator", "any" };
    return families;
}

//==============================================================================
class Part
{
public:
    static constexpr int kSchema = 1;
    static constexpr const char* kMagic = "luthier.part";
    static constexpr const char* kExtension = ".luthierpart";

    juce::String name, author;
    PartType type = PartType::numTypes;
    juce::StringArray tags, compatibility;

    /** The part-type-specific fields, as parsed. */
    juce::var fields;

    /** Drawing hints for the illustration (guitar-illustration.md). */
    juce::var illustration;

    /** The factory part a slot falls back to when a reference is missing (4.1). */
    bool isCategoryDefault = false;

    bool isFactory = false;
    juce::File file;

    //==========================================================================
    double number (const char* field, double fallback) const;
    juce::String text (const char* field, const juce::String& fallback = {}) const;
    bool flag (const char* field, bool fallback = false) const;
    juce::Array<double> numbers (const char* field) const;

    /** True if the part says it suits `family` (or "any"). Advisory only (5). */
    bool suits (const juce::String& family) const;

    //==========================================================================
    juce::var toVar() const;

    /** Parses a part. Returns nullptr and sets `error` if it is not one. */
    static std::shared_ptr<Part> fromVar (const juce::var& json, juce::String& error);

    static std::shared_ptr<Part> load (const juce::File& file, juce::String& error);
    bool save (const juce::File& destination) const;

    /** The reference a guitar file uses: "<Folder>/<Name>.luthierpart". */
    juce::String getReferenceName() const;
};

using PartPtr = std::shared_ptr<const Part>;

} // namespace luthier
