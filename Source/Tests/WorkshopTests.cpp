/*  The parts model (guitar-workshop.md 10). */

#include "TestFramework.h"

#include "../Model/Workshop/PartLibrary.h"
#include "../Support/IrLibrary.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    /** A scratch folder removed when the test ends. */
    struct TempFolder
    {
        TempFolder() : dir (juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getNonexistentChildFile ("luthier-workshop-test", {}))
        {
            dir.createDirectory();
        }

        ~TempFolder() { dir.deleteRecursively(); }

        juce::File dir;
    };

    PartLibrary factoryOnly (const juce::File& emptyUser)
    {
        PartLibrary library;
        library.refreshFrom (PartLibrary::getFactoryPartsFolder(), emptyUser);
        return library;
    }

    juce::Array<juce::File> factoryGuitars()
    {
        juce::Array<juce::File> files;

        for (const auto& entry : juce::RangedDirectoryIterator (PartLibrary::getFactoryGuitarsFolder(), true,
                                                                "*.luthierguitar"))
            files.add (entry.getFile());

        return files;
    }
}

//==============================================================================
LUTHIER_TEST (Workshop, theFactoryLibraryIsThere)
{
    TempFolder user;
    auto library = factoryOnly (user.dir);

    CHECK_MSG (library.getScanErrors().isEmpty(), library.getScanErrors().joinIntoString ("; "));
    CHECK_MSG (library.getNumParts() >= 90,
               juce::String (library.getNumParts()) + " factory parts; factory-content.md 3 ships about 90");

    // Every type a guitar needs has a default to fall back to (4.1).
    for (int i = 0; i < kNumGuitarSlots; ++i)
        if (isSlotRequired ((GuitarSlot) i))
            CHECK_MSG (library.getDefault (getSlotPartType ((GuitarSlot) i)) != nullptr,
                       juce::String ("no default ") + getPartTypeId (getSlotPartType ((GuitarSlot) i)));

    CHECK (factoryGuitars().size() >= 15);
}

LUTHIER_TEST (Workshop, everyFactoryGuitarLoadsAndRoundTrips)
{
    TempFolder user;
    auto library = factoryOnly (user.dir);

    for (const auto& file : factoryGuitars())
    {
        WorkshopGuitar guitar;
        PartLibrary::LoadReport report;

        CHECK_MSG (library.loadGuitar (file, guitar, report), file.getFileName() + " did not load");
        CHECK_MSG (report.missing.isEmpty() && report.errors.isEmpty(),
                   file.getFileName() + ": " + report.missing.joinIntoString ("; ") + report.errors.joinIntoString ("; "));
        CHECK_MSG (report.stringExcess == 0, file.getFileName() + ": neck and bridge disagree on strings");

        // Serialise and reload: an identical guitar.
        WorkshopGuitar again;
        PartLibrary::LoadReport report2;
        CHECK (library.buildGuitar (juce::JSON::parse (juce::JSON::toString (guitar.toVar())), again, report2));
        CHECK_MSG (again == guitar, file.getFileName() + " changed on a round trip");
    }
}

LUTHIER_TEST (Workshop, aMissingPartFallsBackAndSaysSo)
{
    TempFolder user;
    auto library = factoryOnly (user.dir);

    WorkshopGuitar guitar;
    PartLibrary::LoadReport report;
    CHECK (library.loadGuitar (PartLibrary::getFactoryGuitarsFolder().getChildFile ("Electric/Vintage Single-Cut.luthierguitar"),
                               guitar, report));

    auto json = juce::JSON::parse (juce::JSON::toString (guitar.toVar()));
    json.getProperty ("parts", {}).getProperty ("bridge", {}).getDynamicObject()
        ->setProperty ("reference", "Factory/Bridges/Unobtainium Bridge.luthierpart");

    WorkshopGuitar fallen;
    PartLibrary::LoadReport fallReport;
    CHECK_MSG (library.buildGuitar (json, fallen, fallReport), "a missing part failed the whole guitar");

    CHECK (fallen.get (GuitarSlot::bridge) == library.getDefault (PartType::bridge));
    CHECK_MSG (fallReport.missing.size() == 1 && fallReport.missing[0].contains ("Unobtainium Bridge not found"),
               fallReport.missing.joinIntoString ("; "));
}

LUTHIER_TEST (Workshop, aUserPartBeatsTheFactoryOne)
{
    TempFolder user;

    Part mine;
    mine.name = "PAF 59 Alnico 5 8.1k";
    mine.type = PartType::pickup;
    mine.author = "Me";
    auto* fields = new juce::DynamicObject();
    fields->setProperty ("inductance_h", 3.9);
    mine.fields = juce::var (fields);
    CHECK (mine.save (user.dir.getChildFile ("Pickups").getChildFile ("PAF 59 Alnico 5 8.1k.luthierpart")));

    auto library = factoryOnly (user.dir);
    auto found = library.resolve ("Factory/Pickups/PAF 59 Alnico 5 8.1k.luthierpart", PartType::pickup);

    CHECK (found != nullptr);
    CHECK (found != nullptr && found->author == "Me" && ! found->isFactory);
    CHECK (found != nullptr && std::abs (found->number ("inductance_h", 0.0) - 3.9) < 1.0e-9);
}

LUTHIER_TEST (Workshop, incompatiblePartsFitWithAWarning)
{
    TempFolder user;
    auto library = factoryOnly (user.dir);

    WorkshopGuitar guitar;
    PartLibrary::LoadReport report;
    CHECK (library.loadGuitar (PartLibrary::getFactoryGuitarsFolder().getChildFile ("Electric/Vintage Double-Cut.luthierguitar"),
                               guitar, report));

    guitar.parts[(size_t) GuitarSlot::bridge] = library.find (PartType::bridge, "Bass High-Mass Bridge");
    CHECK (guitar.get (GuitarSlot::bridge) != nullptr);

    const auto warnings = guitar.getCompatibilityWarnings();
    CHECK_MSG (warnings.size() == 1 && warnings[0].contains ("bass"), warnings.joinIntoString ("; "));
}

LUTHIER_TEST (Workshop, aStringCountMismatchClamps)
{
    TempFolder user;
    auto library = factoryOnly (user.dir);

    WorkshopGuitar guitar;
    PartLibrary::LoadReport report;
    CHECK (library.loadGuitar (PartLibrary::getFactoryGuitarsFolder().getChildFile ("Electric/Vintage Double-Cut.luthierguitar"),
                               guitar, report));

    guitar.parts[(size_t) GuitarSlot::neck] = library.find (PartType::neck, "7-String Through-Neck");

    int excess = 0;
    CHECK (guitar.getStringCount (&excess) == 6);
    CHECK (excess == 1);
}

LUTHIER_TEST (Workshop, anEmbeddedGuitarNeedsNoPartFiles)
{
    TempFolder user;
    auto library = factoryOnly (user.dir);

    WorkshopGuitar guitar;
    PartLibrary::LoadReport report;
    CHECK (library.loadGuitar (PartLibrary::getFactoryGuitarsFolder().getChildFile ("Electric/Vintage Single-Cut.luthierguitar"),
                               guitar, report));

    // A machine with no parts at all.
    TempFolder emptyFactory, emptyUser;
    PartLibrary bare;
    bare.refreshFrom (emptyFactory.dir, emptyUser.dir);

    WorkshopGuitar restored;
    PartLibrary::LoadReport bareReport;
    CHECK (bare.buildGuitar (juce::JSON::parse (juce::JSON::toString (guitar.toEmbeddedVar())), restored, bareReport));

    CHECK_MSG (bareReport.missing.isEmpty(), bareReport.missing.joinIntoString ("; "));
    CHECK_MSG (restored == guitar, "the embedded guitar came back different");
}

//==============================================================================
LUTHIER_TEST (Workshop, aStringOverrideRoundTripsAndOlderFilesHaveNone)
{
    // guitar-workshop.md 3.3: a heavier third saved in the guitar file, under the
    // strings entry's per_string_override list (file-formats.md 3), numbered for
    // people; a file without the list is a guitar without overrides.
    TempFolder user;
    auto library = factoryOnly (user.dir);

    WorkshopGuitar guitar;
    PartLibrary::LoadReport report;
    CHECK (library.loadGuitar (PartLibrary::getFactoryGuitarsFolder().getChildFile ("Electric/Vintage Single-Cut.luthierguitar"), guitar, report));
    CHECK (guitar.stringOverrides.isEmpty());

    const double setGauge = guitar.getStringGaugeIn (2);
    CHECK (setGauge > 0.0);
    CHECK (! guitar.isStringWound (2));            // the set's plain G
    CHECK (guitar.isStringWound (5));

    StringOverride heavier;
    heavier.stringIndex = 2;
    heavier.gaugeIn = 0.020;
    heavier.wound = 1;
    heavier.material = "phosphor_bronze";
    guitar.setStringOverride (heavier);

    CHECK (guitar.getStringOverride (2) != nullptr);
    CHECK_NEAR (guitar.getStringGaugeIn (2), 0.020, 1.0e-12);
    CHECK (guitar.isStringWound (2));
    CHECK (guitar.getStringMaterial (2) == "phosphor_bronze");
    CHECK (guitar.getStringMaterial (1) != "phosphor_bronze");

    // Written under the strings entry, string 3 for people.
    const auto json = guitar.toVar();
    const auto list = json.getProperty ("parts", {}).getProperty ("strings", {}).getProperty ("per_string_override", {});
    CHECK (list.isArray() && list.size() == 1);
    CHECK ((int) list[0].getProperty ("string", 0) == 3);
    CHECK_NEAR ((double) list[0].getProperty ("gauge_in", 0.0), 0.020, 1.0e-12);
    CHECK ((bool) list[0].getProperty ("wound", false));

    // Round trip, by reference and embedded.
    for (const auto& serialised : { json, guitar.toEmbeddedVar() })
    {
        WorkshopGuitar again;
        PartLibrary::LoadReport report2;
        CHECK (library.buildGuitar (juce::JSON::parse (juce::JSON::toString (serialised)), again, report2));
        CHECK_MSG (again == guitar, "the override did not survive a round trip");
        CHECK (again.getStringOverride (2) != nullptr && again.getStringOverride (2)->material == "phosphor_bronze");
    }

    // The key removed - a file from before this - loads the same guitar minus the override.
    auto older = juce::JSON::parse (juce::JSON::toString (json));
    older.getProperty ("parts", {}).getProperty ("strings", {}).getDynamicObject()->removeProperty ("per_string_override");
    WorkshopGuitar plain;
    PartLibrary::LoadReport report3;
    CHECK (library.buildGuitar (older, plain, report3));
    CHECK (plain.stringOverrides.isEmpty());
    CHECK_NEAR (plain.getStringGaugeIn (2), setGauge, 1.0e-12);
    CHECK (! (plain == guitar));

    // An override with nothing in it is no override; setting the same string twice keeps one.
    guitar.setStringOverride (heavier);
    CHECK (guitar.stringOverrides.size() == 1);
    StringOverride empty;
    empty.stringIndex = 2;
    guitar.setStringOverride (empty);
    CHECK (guitar.stringOverrides.isEmpty());
}
