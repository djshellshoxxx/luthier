#pragma once

/*  The parts library and the guitar as a bill of parts (guitar-workshop.md 1, 3, 4).

    PartLibrary scans the factory folder (Resources/Parts, read-only) and the
    user folder (Documents/Luthier/Parts), indexes by type, and resolves a
    guitar file's references: user first, then factory, then the category's
    default - a missing part is a fallback and a report, never a failure
    (4.1, error-recovery.md).

    WorkshopGuitar is what guitar-workshop.md 3 calls GuitarSpec: resolved
    part pointers per slot, pickup placements, setup, finish and seed. It is
    named differently here because the build already has a `GuitarSpec` -
    the compiled engine description - which this maps onto (3.1, and see
    PartAcoustics.h); the file format keeps the spec's names.

    Message thread only. The audio thread never sees this; it sees what
    mapSpec derives from it.
*/

#include "Part.h"
#include <array>
#include <map>

namespace luthier
{

//==============================================================================
enum class GuitarSlot
{
    body = 0, top, neck, fretboard, frets, nut, bridge, tailpiece, tuners,
    pickupNeck, pickupMiddle, pickupBridge,
    wiring, strings, pickguard,
    numSlots
};

inline constexpr int kNumGuitarSlots = (int) GuitarSlot::numSlots;

const char* getSlotId (GuitarSlot slot) noexcept;         ///< "body", "pickups.neck", ...
PartType getSlotPartType (GuitarSlot slot) noexcept;
bool isSlotRequired (GuitarSlot slot) noexcept;          ///< guitar-workshop.md 1's table

/** Where a pickup sits and how high (file-formats.md 3). */
struct PickupPlacement
{
    double positionMm = 100.0;       ///< from the bridge saddle
    double heightTrebleMm = 2.5;
    double heightBassMm = 2.8;
};

/** fret-buzz.md 1, as the guitar file stores it. */
struct GuitarSetup
{
    double actionTrebleMm = 1.6, actionBassMm = 2.0, reliefMm = 0.20;
    juce::Array<double> nutSlotDepthsMm;
    juce::Array<double> intonationMm;
};

/*  guitar-workshop.md 3.3, workshop-ui.md 3.3: one string of the set replaced
    - a heavier third, a wound G on an otherwise plain-G set. Indexed the
    engine's way (0 = the high E, engine.md 1); a field left at its default
    keeps the set's value. In the guitar file this is the strings entry's
    `per_string_override` list (file-formats.md 3), numbered for people
    (1 = the high E); a file without the list has no overrides, so guitars
    written before this loaded unchanged and guitars written with it load
    on an older build minus the overrides. */
struct StringOverride
{
    int stringIndex = -1;
    double gaugeIn = 0.0;          ///< 0 = the set's gauge
    juce::String material;         ///< "" = the set's winding_material ("phosphor_bronze", ...)
    int wound = -1;                ///< -1 as the set decides, 0 plain, 1 wound

    bool isEmpty() const noexcept { return gaugeIn <= 0.0 && material.isEmpty() && wound < 0; }
    bool operator== (const StringOverride& o) const noexcept
    {
        return stringIndex == o.stringIndex && gaugeIn == o.gaugeIn && material == o.material && wound == o.wound;
    }
    bool operator!= (const StringOverride& o) const noexcept { return ! (*this == o); }
};

struct GuitarFinish
{
    juce::String type = "solid", colourA = "#7A2E1B", colourB = "#F2C441", burstShape = "radial";
    double gloss = 0.8, aging = 0.0;
};

//==============================================================================
class WorkshopGuitar
{
public:
    static constexpr int kSchema = 1;
    static constexpr const char* kMagic = "luthier.guitar";
    static constexpr const char* kExtension = ".luthierguitar";

    juce::String name, family = "electric", bodyStyle, author;
    juce::StringArray tags;

    std::array<PartPtr, kNumGuitarSlots> parts {};
    std::array<PickupPlacement, 3> placements {};    ///< neck, middle, bridge
    GuitarSetup setup;
    GuitarFinish finish;
    juce::String hardwareColour = "nickel";
    juce::uint64 seed = 0;

    /** 3.3: the strings that differ from the set, at most one entry per string. */
    juce::Array<StringOverride> stringOverrides;

    PartPtr get (GuitarSlot slot) const noexcept { return parts[(size_t) slot]; }

    //==========================================================================
    // Per-string overrides (3.3).

    /** The override on a string, or nullptr when it plays the set's string. */
    const StringOverride* getStringOverride (int stringIndex) const noexcept;

    /** Replaces the string's override; an empty one removes it. */
    void setStringOverride (const StringOverride& o);
    bool clearStringOverride (int stringIndex);

    /** The gauge a string plays at: the set's, or the override's. */
    double getStringGaugeIn (int stringIndex) const;

    /** The winding material id a string plays with ("nickel_plated_steel", ...). */
    juce::String getStringMaterial (int stringIndex) const;

    /*  Wound or plain: the override says, else the set's rule (the renderer's,
        guitar-illustration.md 10): nylon trebles are plain, a bass is all wound,
        else 0.0195" and up is wound. */
    bool isStringWound (int stringIndex) const;

    /** The pickup slots in neck, middle, bridge order. */
    static GuitarSlot pickupSlot (int index) noexcept;

    /*  guitar-workshop.md 5.1: the string count is the smaller of what the
        neck and the bridge can take. `excess` reports what was dropped. */
    int getStringCount (int* excess = nullptr) const noexcept;

    /** Advisory warnings for parts that do not declare this family (5). */
    juce::StringArray getCompatibilityWarnings() const;

    /** Serialises by reference: each slot names its part, not its fields (6). */
    juce::var toVar() const;

    /*  The same with every part's fields embedded, for a preset's
        `guitar.override` - self-contained, loadable with no part files (8). */
    juce::var toEmbeddedVar() const;

    bool save (const juce::File& destination) const;

    bool operator== (const WorkshopGuitar&) const;
};

//==============================================================================
class PartLibrary
{
public:
    PartLibrary();

    /** Rescans both folders. Message thread. */
    void refresh();

    /** Scans explicit folders (tests, and a relocated install). */
    void refreshFrom (const juce::File& factoryParts, const juce::File& userParts);

    static juce::File getFactoryPartsFolder();
    static juce::File getUserPartsFolder();
    static juce::File getFactoryGuitarsFolder();
    static juce::File getUserGuitarsFolder();

    juce::Array<PartPtr> getParts (PartType type) const;
    int getNumParts() const noexcept;

    /** A part by type and name: the user's if there is one, else the factory's. */
    PartPtr find (PartType type, const juce::String& name) const;

    /*  A guitar file's reference, "<Folder>/<Name>.luthierpart" or with an
        origin prefix ("Factory/Bodies/..."); the origin is ignored because a
        user part of the same name wins wherever the reference was written. */
    PartPtr resolve (const juce::String& reference, PartType expected) const;

    /*  Factory parts and guitars renamed to drop trademarks (2026-09-23): the
        new name for an old one, or the name unchanged. Saved sessions and
        guitar files written before the rename keep resolving. */
    static juce::String renamedFactoryPart (const juce::String& name);
    static juce::String renamedFactoryGuitar (const juce::String& relativePath);

    /** ambiguity-resolutions.md 7: the shipped guitar a name from an older
        preset stands for (an old guitar-type name, a current one, or a renamed
        file), from Resources/Guitars/migration.json. Empty if it knows none. */
    static juce::String migratedGuitar (const juce::String& nameOrRelativePath);

    /** The migration table's version, or 0 when it is not installed. */
    static int getGuitarMigrationVersion();

    /** The factory default for a type (4.1); any factory part if none is flagged. */
    PartPtr getDefault (PartType type) const;

    //==========================================================================
    struct LoadReport
    {
        juce::StringArray missing;      ///< "Bridge X not found, using factory default" per gui-integration 15
        juce::StringArray errors;       ///< unreadable files
        juce::StringArray warnings;     ///< compatibility (advisory)
        int stringExcess = 0;
    };

    /*  Builds a guitar from a `.luthierguitar` (or a preset's embedded
        override). Never fails for a missing part: it fits the default and
        says so. Returns false only if the JSON is not a guitar at all. */
    bool buildGuitar (const juce::var& json, WorkshopGuitar& out, LoadReport& report) const;

    bool loadGuitar (const juce::File& file, WorkshopGuitar& out, LoadReport& report) const;

    /*  guitar-illustration.md 12.2: a family change. Starts from the family's
        default template, keeps every part of `from` that suits the new family
        (its compatibility list), and takes the template's part everywhere
        else, with the template's body style, setup, finish and the pickup
        placements of the pickups it fits. The character seed survives (12.4).
        `replaced` gets 12.1's banner line. False if the family has no
        template. */
    bool switchFamily (const WorkshopGuitar& from, const juce::String& family,
                       WorkshopGuitar& out, juce::String& replaced) const;

    /** The template a family switches to, relative to the factory guitars folder (12.2). */
    static juce::String getFamilyTemplate (const juce::String& family);

    /** Every guitar file in the factory and user folders, user last. */
    juce::Array<juce::File> getGuitarFiles() const;

    /** Load errors from the last refresh, for the error log. */
    const juce::StringArray& getScanErrors() const noexcept { return scanErrors; }

private:
    void scanFolder (const juce::File& root, bool factory);

    // Keyed by type then lower-cased name. A user part replaces a factory one.
    std::array<std::map<juce::String, PartPtr>, (size_t) PartType::numTypes> byType;
    juce::StringArray scanErrors;
};

} // namespace luthier
