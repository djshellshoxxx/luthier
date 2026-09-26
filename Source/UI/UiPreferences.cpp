#include "UiPreferences.h"

namespace luthier
{

UiPreferences& UiPreferences::get()
{
    static UiPreferences instance;
    return instance;
}

UiPreferences::UiPreferences()
{
    values = juce::var (new juce::DynamicObject());
    load();
}

juce::File UiPreferences::getConfigFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("config")
             .getChildFile ("ui.json");
}

//==============================================================================
int UiPreferences::getInt (const juce::String& key, int fallback) const
{
    if (auto* object = values.getDynamicObject())
        if (object->hasProperty (key))
            return (int) object->getProperty (key);

    return fallback;
}

void UiPreferences::setInt (const juce::String& key, int value)
{
    if (getInt (key, value - 1) == value)
        return;

    if (auto* object = values.getDynamicObject())
    {
        object->setProperty (key, value);
        save();
    }
}

bool UiPreferences::getBool (const juce::String& key, bool fallback) const
{
    if (auto* object = values.getDynamicObject())
        if (object->hasProperty (key))
            return (bool) object->getProperty (key);

    return fallback;
}

void UiPreferences::setBool (const juce::String& key, bool value)
{
    if (getBool (key, ! value) == value)
        return;

    if (auto* object = values.getDynamicObject())
    {
        object->setProperty (key, value);
        save();
    }
}

bool UiPreferences::has (const juce::String& key) const
{
    auto* object = values.getDynamicObject();
    return object != nullptr && object->hasProperty (key);
}

void UiPreferences::remove (const juce::String& key)
{
    if (auto* object = values.getDynamicObject(); object != nullptr && object->hasProperty (key))
    {
        object->removeProperty (key);
        save();
    }
}

juce::String UiPreferences::getString (const juce::String& key, const juce::String& fallback) const
{
    if (auto* object = values.getDynamicObject())
        if (object->hasProperty (key))
            return object->getProperty (key).toString();

    return fallback;
}

void UiPreferences::setString (const juce::String& key, const juce::String& value)
{
    if (getString (key, value + "x") == value)
        return;

    if (auto* object = values.getDynamicObject())
    {
        object->setProperty (key, value);
        save();
    }
}

//==============================================================================
void UiPreferences::reset()
{
    values = juce::var (new juce::DynamicObject());
}

bool UiPreferences::save() const
{
    const auto file = getConfigFile();

    file.getParentDirectory().createDirectory();

    return file.replaceWithText (juce::JSON::toString (values, true));
}

bool UiPreferences::load()
{
    const auto file = getConfigFile();

    if (! file.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());

    /*  A file that is there but unreadable is treated as no file. Refusing to
        start because a preference file was corrupted would be the worse failure,
        and every getter here has a default at the call site. */
    if (parsed.getDynamicObject() == nullptr)
        return false;

    values = parsed;
    return true;
}

} // namespace luthier
