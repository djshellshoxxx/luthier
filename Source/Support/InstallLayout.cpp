#include "InstallLayout.h"

namespace luthier
{

const juce::StringArray& InstallLayout::subfolders()
{
    static const juce::StringArray list {
        "Presets/User", "Presets/Factory", "Guitars/User", "Guitars/Factory",
        "Parts/User", "Parts/Factory", "Tunes/User", "Tunes/Examples",
        "IRs", "Loops", "Setlists", "Sessions", "Renders", "Captures",
        "Practice", "Diagnostics", "config", "ContentUpdates"
    };

    return list;
}

juce::File InstallLayout::defaultRoot()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Luthier");
}

InstallLayout::Result InstallLayout::ensure (const juce::File& root, const juce::String& runningVersion)
{
    Result result;

    if (root == juce::File())
    {
        result.failures.add ("no root folder");
        return result;
    }

    for (const auto& sub : subfolders())
    {
        const auto folder = root.getChildFile (sub);

        if (! folder.isDirectory() && ! folder.createDirectory().wasOk())
            result.failures.add (folder.getFullPathName());
    }

    const auto config = configFile (root);

    if (! config.existsAsFile())
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("format", 1);
        obj->setProperty ("createdBy", runningVersion);
        obj->setProperty ("settings", juce::var (new juce::DynamicObject()));

        if (! config.replaceWithText (juce::JSON::toString (juce::var (obj))))
            result.failures.add (config.getFullPathName());
    }

    const auto marker = root.getChildFile (markerName);

    if (! marker.existsAsFile())
    {
        result.firstRun = true;
    }
    else
    {
        const auto previous = marker.loadFileAsString().trim();

        if (previous != runningVersion)
            result.upgradedFrom = previous.isEmpty() ? juce::String ("unknown") : previous;
    }

    if (result.firstRun || result.isUpgrade())
        if (! marker.replaceWithText (runningVersion))
            result.failures.add (marker.getFullPathName());

    return result;
}

} // namespace luthier
