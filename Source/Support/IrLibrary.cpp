#include "IrLibrary.h"
#include "ThreadProbe.h"

namespace luthier
{

namespace
{
    juce::File cachedResources;
    bool hasSearched = false;

    /** A folder is the one we want if it holds either of the IR libraries or the
        preset bank; checking the contents rather than just the name stops an
        unrelated "Resources" folder higher up the tree from winning. */
    bool looksRight (const juce::File& folder)
    {
        if (! folder.isDirectory())
            return false;

        return folder.getChildFile ("BodyIRs").isDirectory()
               || folder.getChildFile ("CabIRs").isDirectory()
               || folder.getChildFile ("Presets").isDirectory();
    }

    /** Picks any .wav in a folder, preferring one whose name shares the most
        underscore-separated tokens with what was asked for. */
    juce::File bestMatchIn (const juce::File& folder, const juce::String& wanted)
    {
        if (! folder.isDirectory())
            return {};

        const auto wantedTokens = juce::StringArray::fromTokens (
            wanted.upToLastOccurrenceOf (".", false, false), "_", "");

        juce::File best;
        int bestScore = -1;

        for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*.wav",
                                                                juce::File::findFiles))
        {
            const auto file = entry.getFile();
            const auto tokens = juce::StringArray::fromTokens (
                file.getFileNameWithoutExtension(), "_", "");

            int score = 0;

            for (const auto& token : wantedTokens)
                if (tokens.contains (token))
                    ++score;

            if (score > bestScore)
            {
                bestScore = score;
                best = file;
            }
        }

        return best;
    }
}

//==============================================================================
juce::Array<juce::File> IrLibrary::getCandidateFolders()
{
    juce::Array<juce::File> candidates;

    auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                    .getParentDirectory();

    // Walk up from the binary. A VST3 bundle puts the DLL several levels below
    // Contents/Resources, so four levels covers every layout we produce.
    juce::File walk = exeDir;

    for (int i = 0; i < 5 && walk.exists(); ++i)
    {
        candidates.add (walk.getChildFile ("Resources"));
       #if JUCE_LINUX || JUCE_BSD
        // installer.md 3.1: a packaged VST3 at <prefix>/lib/vst3/Luthier.vst3
        // finds <prefix>/share/luthier four levels up.
        if (i == 4)
            candidates.add (walk.getChildFile ("share").getChildFile ("luthier"));
       #endif
        walk = walk.getParentDirectory();
    }

    // The module folder, for hosts that load from somewhere unusual.
    auto moduleDir = juce::File::getSpecialLocation (juce::File::currentApplicationFile)
                        .getParentDirectory();

    candidates.add (moduleDir.getChildFile ("Resources"));
    candidates.add (moduleDir.getParentDirectory().getChildFile ("Resources"));

    // A hand install, or a user who put their own IRs there.
    candidates.add (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("Luthier").getChildFile ("Resources"));

    // installer.md 1.1 / 2.1 / 3.1: where each platform's installer puts the
    // factory content. The folder itself is the content root (BodyIRs, CabIRs,
    // Presets ... directly inside it).
   #if JUCE_LINUX || JUCE_BSD
    // A package installs <prefix>/bin/luthier beside <prefix>/share/luthier, so
    // a tarball's user install (~/.local) and /usr/local are found as well.
    candidates.add (exeDir.getParentDirectory().getChildFile ("share").getChildFile ("luthier"));
    candidates.add (juce::File ("/usr/share/luthier"));
    candidates.add (juce::File ("/usr/local/share/luthier"));
    candidates.add (juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                      .getChildFile (".local/share/luthier"));
   #elif JUCE_MAC
    candidates.add (juce::File ("/Library/Application Support/Luthier"));
    candidates.add (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                      .getChildFile ("Application Support").getChildFile ("Luthier"));
   #elif JUCE_WINDOWS
    candidates.add (juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
                      .getChildFile ("Luthier"));
   #endif

    // Shared application data, where an older hand-made layout put it.
    candidates.add (juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
                      .getChildFile ("Luthier").getChildFile ("Resources"));

    // Where the release installers put the factory content (installer.md 1-3,
    // docs/RELEASING.md): the plugin bundles ship without it, so every format
    // shares one copy.
   #if JUCE_MAC
    candidates.add (juce::File ("/Library/Application Support/Luthier/Resources"));
    candidates.add (juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                      .getChildFile ("Library/Application Support/Luthier/Resources"));
   #elif JUCE_LINUX || JUCE_BSD
    candidates.add (juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                      .getChildFile (".local/share/luthier/Resources"));
    candidates.add (juce::File ("/usr/local/share/luthier/Resources"));
    candidates.add (juce::File ("/usr/share/luthier/Resources"));
   #endif

    return candidates;
}

juce::File IrLibrary::searchForResources()
{
    for (const auto& candidate : getCandidateFolders())
        if (looksRight (candidate))
            return candidate;

    return {};
}

juce::File IrLibrary::getResourcesFolder()
{
    if (! hasSearched)
    {
        cachedResources = searchForResources();
        hasSearched = true;
    }

    return cachedResources;
}

void IrLibrary::forgetResourcesFolder()
{
    hasSearched = false;
    cachedResources = juce::File();
}

juce::File IrLibrary::getBodyIrFolder()
{
    const auto resources = getResourcesFolder();
    return resources == juce::File() ? juce::File() : resources.getChildFile ("BodyIRs");
}

juce::File IrLibrary::getCabIrFolder()
{
    const auto resources = getResourcesFolder();
    return resources == juce::File() ? juce::File() : resources.getChildFile ("CabIRs");
}

//==============================================================================
juce::File IrLibrary::findBodyIr (const BodyConfig& config)
{
    ThreadProbe::noteFileAccess();
    const auto root = getBodyIrFolder();

    if (! root.isDirectory())
        return {};

    // The generator names files shape/size_topwood_age.wav, and BodyModels builds
    // the same string, so an exact hit is the common case.
    const auto relative = BodyModels::irFileNameFor (config);
    const auto exact = root.getChildFile (relative);

    if (exact.existsAsFile())
        return exact;

    const auto shapeFolder = exact.getParentDirectory();

    if (! shapeFolder.isDirectory())
        return {};

    // Same shape, nearest name. Getting the body shape right matters far more
    // than getting the wood or the age exactly right.
    return bestMatchIn (shapeFolder, exact.getFileName());
}

juce::File IrLibrary::findCabIr (const CabinetConfig& config)
{
    ThreadProbe::noteFileAccess();
    const auto root = getCabIrFolder();

    if (! root.isDirectory())
        return {};

    const auto relative = CabinetEngine::irFileNameFor (config);
    const auto exact = root.getChildFile (relative);

    if (exact.existsAsFile())
        return exact;

    const auto cabFolder = exact.getParentDirectory();

    if (cabFolder.isDirectory())
    {
        const auto match = bestMatchIn (cabFolder, exact.getFileName());

        if (match.existsAsFile())
            return match;
    }

    // No folder for that cabinet at all: fall back to any 4x12, which is the
    // least surprising thing to hear if the requested cab is missing.
    auto fallback = root.getChildFile ("4x12");

    if (fallback.isDirectory())
        return bestMatchIn (fallback, exact.getFileName());

    return {};
}

//==============================================================================
int IrLibrary::countBodyIrs()
{
    const auto folder = getBodyIrFolder();

    if (! folder.isDirectory())
        return 0;

    int count = 0;

    for (const auto& e : juce::RangedDirectoryIterator (folder, true, "*.wav", juce::File::findFiles))
    {
        juce::ignoreUnused (e);
        ++count;
    }

    return count;
}

int IrLibrary::countCabIrs()
{
    const auto folder = getCabIrFolder();

    if (! folder.isDirectory())
        return 0;

    int count = 0;

    for (const auto& e : juce::RangedDirectoryIterator (folder, true, "*.wav", juce::File::findFiles))
    {
        juce::ignoreUnused (e);
        ++count;
    }

    return count;
}

bool IrLibrary::isAvailable()
{
    return getResourcesFolder() != juce::File();
}

juce::String IrLibrary::describe()
{
    if (! isAvailable())
        return "No Resources folder found - the body and cabinet convolution paths "
               "will fall back to their modelled equivalents.";

    return getResourcesFolder().getFullPathName()
           + "  (" + juce::String (countBodyIrs()) + " body IRs, "
           + juce::String (countCabIrs()) + " cabinet IRs)";
}

} // namespace luthier
