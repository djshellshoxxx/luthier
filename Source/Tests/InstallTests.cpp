/*  installer.md and qa-polish.md 11: the install layout, the content search
    paths and the shipped licence file. */

#include "TestFramework.h"

#include "../Support/InstallLayout.h"
#include "../Support/IrLibrary.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    struct TempRoot
    {
        juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("LuthierInstallTest-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
        ~TempRoot() { dir.deleteRecursively(); }
    };
}

//==============================================================================
LUTHIER_TEST (Legal, thirdPartyLicencesNameEveryBundledDependency)
{
    const auto resources = IrLibrary::getResourcesFolder();
    CHECK_MSG (resources.isDirectory(), "no Resources folder found");

    const auto file = resources.getChildFile ("THIRD_PARTY_LICENCES.txt");
    CHECK_MSG (file.existsAsFile(), "THIRD_PARTY_LICENCES.txt does not ship in Resources/");

    const auto text = file.loadFileAsString();
    CHECK (text.contains ("JUCE"));
    CHECK (text.contains ("VST3"));

    int fonts = 0;

    for (const auto& entry : juce::RangedDirectoryIterator (resources.getChildFile ("Fonts"), false, "*-OFL.txt"))
    {
        const auto family = entry.getFile().getFileName().upToFirstOccurrenceOf ("-OFL", false, false);
        // "BebasNeue" is written "Bebas Neue" in prose; accept either.
        const auto spaced = family.replace ("Neue", " Neue");
        CHECK_MSG (text.contains (family) || text.contains (spaced), family + " is not named");
        ++fonts;
    }

    CHECK_MSG (fonts >= 2, "expected the bundled OFL fonts");
}

//==============================================================================
LUTHIER_TEST (IrLibrary, candidatesIncludeTheInstallerLayout)
{
    const auto candidates = IrLibrary::getCandidateFolders();

   #if JUCE_LINUX || JUCE_BSD
    CHECK (candidates.contains (juce::File ("/usr/share/luthier")));
    CHECK (candidates.contains (juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                                  .getChildFile (".local/share/luthier")));
   #elif JUCE_MAC
    CHECK (candidates.contains (juce::File ("/Library/Application Support/Luthier")));
   #elif JUCE_WINDOWS
    CHECK (candidates.contains (juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
                                  .getChildFile ("Luthier")));
   #endif

    // The development layout still wins: the first candidate that looks right
    // is the build tree's copy.
    IrLibrary::forgetResourcesFolder();
    const auto found = IrLibrary::getResourcesFolder();
    CHECK (candidates.indexOf (found) >= 0);
}

//==============================================================================
LUTHIER_TEST (InstallLayout, createsTheTreeAndMarker)
{
    TempRoot t;
    const auto r = InstallLayout::ensure (t.dir, "1.2.3");

    CHECK (r.firstRun);
    CHECK (! r.isUpgrade());
    CHECK_MSG (r.failures.isEmpty(), r.failures.joinIntoString (", "));
    CHECK (InstallLayout::subfolders().size() == 18);

    for (const auto& sub : InstallLayout::subfolders())
        CHECK_MSG (t.dir.getChildFile (sub).isDirectory(), sub + " missing");

    const auto config = juce::JSON::parse (InstallLayout::configFile (t.dir));
    CHECK (config.isObject());
    CHECK (t.dir.getChildFile (InstallLayout::markerName).loadFileAsString().trim() == "1.2.3");
}

LUTHIER_TEST (InstallLayout, sameVersionIsQuiet)
{
    TempRoot t;
    InstallLayout::ensure (t.dir, "1.2.3");

    // A user edit to plugin.json survives a second load.
    InstallLayout::configFile (t.dir).replaceWithText ("{\"mine\": 1}");

    const auto r = InstallLayout::ensure (t.dir, "1.2.3");
    CHECK (! r.firstRun);
    CHECK (! r.isUpgrade());
    CHECK (InstallLayout::configFile (t.dir).loadFileAsString().contains ("mine"));
}

LUTHIER_TEST (InstallLayout, differentVersionReportsUpgrade)
{
    TempRoot t;
    InstallLayout::ensure (t.dir, "1.0.0");

    const auto r = InstallLayout::ensure (t.dir, "1.1.0");
    CHECK (! r.firstRun);
    CHECK (r.upgradedFrom == "1.0.0");
    CHECK (t.dir.getChildFile (InstallLayout::markerName).loadFileAsString().trim() == "1.1.0");

    // ... and only once.
    CHECK (! InstallLayout::ensure (t.dir, "1.1.0").isUpgrade());
}
