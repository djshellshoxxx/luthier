#include "ConfigRecovery.h"
#include "ErrorLog.h"

namespace luthier
{
namespace ConfigRecovery
{
namespace
{
    juce::CriticalSection& lock()
    {
        static juce::CriticalSection cs;
        return cs;
    }

    juce::StringArray& recovered()
    {
        static juce::StringArray files;
        return files;
    }
}

juce::File setAside (const juce::File& file, const juce::String& module)
{
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
    auto aside = file.getSiblingFile (file.getFileName() + ".corrupted-" + stamp);

    for (int n = 2; aside.exists() && n < 100; ++n)
        aside = file.getSiblingFile (file.getFileName() + ".corrupted-" + stamp + "-" + juce::String (n));

    const bool moved = file.moveFileTo (aside);

    ErrorLog::write (ErrorLog::Severity::warn, module, "CONFIG_CORRUPTED",
                     "A settings file could not be read; defaults are in use and the file was kept aside",
                     [&]
                     {
                         auto* context = new juce::DynamicObject();
                         context->setProperty ("path", file.getFullPathName());
                         context->setProperty ("backup", moved ? aside.getFullPathName() : juce::String());
                         return juce::var (context);
                     }());

    const juce::ScopedLock sl (lock());
    recovered().addIfNotAlreadyThere (file.getFileName());
    return moved ? aside : juce::File();
}

juce::var loadObject (const juce::File& file, const juce::String& module, int maxSchema)
{
    if (! file.existsAsFile())
        return {};

    const auto parsed = juce::JSON::parse (file.loadFileAsString());

    const bool unreadable = parsed.getDynamicObject() == nullptr;
    const bool newer = ! unreadable && maxSchema >= 0 && parsed.hasProperty ("schema")
                         && (int) parsed.getProperty ("schema", 0) > maxSchema;

    if (unreadable || newer)
    {
        setAside (file, module);
        return {};
    }

    return parsed;
}

juce::StringArray takeRecoveredFiles()
{
    const juce::ScopedLock sl (lock());
    return std::exchange (recovered(), {});
}

} // namespace ConfigRecovery
} // namespace luthier
