#include "LicenceStore.h"

namespace luthier
{

LicenceStore::LicenceStore (juce::File root_)
    : root (root_ == juce::File() ? defaultRoot() : std::move (root_))
{
}

juce::File LicenceStore::defaultRoot()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Luthier")
        .getChildFile ("licensing");
}

bool LicenceStore::atomicWrite (const juce::File& target, const juce::String& text) const
{
    if (! target.getParentDirectory().createDirectory())
        return false;

    juce::TemporaryFile temp (target);

    if (! temp.getFile().replaceWithText (text))
        return false;

    return temp.overwriteTargetFileWithTemporary();
}

bool LicenceStore::saveEnvelope (const juce::String& json) const
{
    // The persisted licence is exactly the signed envelope. Monotonic clock
    // metadata lives separately so it can never be confused with signed data.
    const auto parsed = juce::JSON::parse (json);

    if (parsed.getDynamicObject() == nullptr)
        return false;

    return atomicWrite (getLicenceFile(), json);
}

std::optional<juce::String> LicenceStore::loadEnvelope() const
{
    const auto file = getLicenceFile();

    if (! file.existsAsFile())
        return std::nullopt;

    const auto text = file.loadFileAsString();

    if (juce::JSON::parse (text).getDynamicObject() == nullptr)
        return std::nullopt;

    return text;
}

bool LicenceStore::removeEnvelope() const
{
    const auto file = getLicenceFile();
    return ! file.exists() || file.deleteFile();
}

juce::Time LicenceStore::loadLastSeen() const
{
    const auto state = juce::JSON::parse (getStateFile().loadFileAsString());
    const auto* obj = state.getDynamicObject();

    if (obj == nullptr)
        return {};

    return juce::Time ((juce::int64) obj->getProperty ("lastSeen"));
}

bool LicenceStore::updateLastSeen (juce::Time now) const
{
    const auto previous = loadLastSeen();

    // Never move the stored monotonic wall-clock marker backwards.
    const auto chosen = previous > now ? previous : now;
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("lastSeen", chosen.toMilliseconds());

    return atomicWrite (getStateFile(), juce::JSON::toString (juce::var (obj), false));
}

} // namespace luthier
