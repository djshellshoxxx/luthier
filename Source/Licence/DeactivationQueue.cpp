#include "DeactivationQueue.h"

namespace luthier
{

DeactivationQueue::DeactivationQueue (juce::File file_)
    : file (file_ == juce::File() ? defaultFile() : std::move (file_))
{
}

juce::File DeactivationQueue::defaultFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Luthier")
        .getChildFile ("licensing")
        .getChildFile ("deactivation-queue.json");
}

juce::Array<juce::var> DeactivationQueue::load() const
{
    if (! file.existsAsFile())
        return {};

    const auto parsed = juce::JSON::parse (file.loadFileAsString());
    if (const auto* a = parsed.getArray())
        return *a;

    return {};
}

bool DeactivationQueue::save (const juce::Array<juce::var>& items) const
{
    if (! file.getParentDirectory().createDirectory())
        return false;

    if (items.isEmpty())
        return ! file.exists() || file.deleteFile();

    juce::TemporaryFile temp (file);
    if (! temp.getFile().replaceWithText (juce::JSON::toString (juce::var (items), false)))
        return false;

    return temp.overwriteTargetFileWithTemporary();
}

bool DeactivationQueue::enqueue (const juce::String& licenseId,
                                 const juce::StringArray& fingerprints)
{
    if (licenseId.isEmpty())
        return false;

    auto items = load();
    auto* item = new juce::DynamicObject();
    item->setProperty ("licenseId", licenseId);

    juce::Array<juce::var> fp;
    for (const auto& value : fingerprints)
        fp.add (value);
    item->setProperty ("fp", fp);
    items.add (juce::var (item));

    return save (items);
}

int DeactivationQueue::drain (Transport& transport, const juce::String& endpoint)
{
    auto items = load();
    int removed = 0;

    while (! items.isEmpty())
    {
        const auto body = juce::JSON::toString (items.getFirst(), false);
        const auto result = transport.post (endpoint, body, 15000);

        if (! result.succeeded)
            break;

        items.remove (0);
        ++removed;
    }

    save (items);
    return removed;
}

int DeactivationQueue::size() const
{
    return load().size();
}

} // namespace luthier
