#include "PerformanceSettings.h"

namespace luthier
{

namespace
{
    constexpr int kSchema = 1;
    const char* const kMagic = "luthier.performance";

    bool choiceFromKey (const juce::String& s, QualityChoice& out)
    {
        if (s == "high")   { out = QualityChoice::High;   return true; }
        if (s == "medium") { out = QualityChoice::Medium; return true; }
        if (s == "low")    { out = QualityChoice::Low;    return true; }
        if (s == "auto")   { out = QualityChoice::Auto;   return true; }
        return false;
    }

    bool levelFromKey (const juce::String& s, QualityLevel& out)
    {
        if (s == "high")   { out = QualityLevel::High;   return true; }
        if (s == "medium") { out = QualityLevel::Medium; return true; }
        if (s == "low")    { out = QualityLevel::Low;    return true; }
        return false;
    }
}

//==============================================================================
PerformanceSettings& PerformanceSettings::get()
{
    static PerformanceSettings instance;
    return instance;
}

PerformanceSettings::PerformanceSettings()
{
    load();
}

juce::File PerformanceSettings::getDefaultFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("config")
             .getChildFile ("performance.json");
}

juce::File PerformanceSettings::getFile() const
{
    return fileOverride != juce::File() ? fileOverride : getDefaultFile();
}

void PerformanceSettings::setFileForTesting (const juce::File& f)
{
    fileOverride = f;
    load();
}

PerformanceSettings::Values PerformanceSettings::getValues() const noexcept
{
    Values v;
    v.quality = getQuality();
    v.offlineAtHigh = isOfflineAtHigh();
    v.autoNotify = isAutoNotify();
    v.autoLastLevel = getAutoLastLevel();
    v.emergencyStringDrop = isEmergencyStringDrop();
    return v;
}

void PerformanceSettings::store (const Values& v) noexcept
{
    quality.store ((int) v.quality, std::memory_order_relaxed);
    offlineAtHigh.store (v.offlineAtHigh, std::memory_order_relaxed);
    autoNotify.store (v.autoNotify, std::memory_order_relaxed);
    autoLastLevel.store ((int) v.autoLastLevel, std::memory_order_relaxed);
    emergencyStringDrop.store (v.emergencyStringDrop, std::memory_order_relaxed);
}

//==============================================================================
bool PerformanceSettings::parse (const juce::String& json, Values& out)
{
    out = Values {};

    const auto parsed = juce::JSON::parse (json);
    auto* object = parsed.getDynamicObject();

    if (object == nullptr || object->getProperty ("magic").toString() != kMagic)
        return false;

    // Each field falls back on its own: one bad value costs one preference.
    QualityChoice q;
    if (choiceFromKey (object->getProperty ("quality").toString(), q))
        out.quality = q;

    QualityLevel l;
    if (levelFromKey (object->getProperty ("auto_last_level").toString(), l))
        out.autoLastLevel = l;

    if (object->getProperty ("offline_at_high").isBool())
        out.offlineAtHigh = (bool) object->getProperty ("offline_at_high");

    if (object->getProperty ("auto_notify").isBool())
        out.autoNotify = (bool) object->getProperty ("auto_notify");

    if (object->getProperty ("emergency_string_drop").isBool())
        out.emergencyStringDrop = (bool) object->getProperty ("emergency_string_drop");

    return true;
}

juce::String PerformanceSettings::toJson (const Values& v)
{
    auto* object = new juce::DynamicObject();
    object->setProperty ("schema", kSchema);
    object->setProperty ("magic", kMagic);
    object->setProperty ("quality", qualityChoiceKey (v.quality));
    object->setProperty ("offline_at_high", v.offlineAtHigh);
    object->setProperty ("auto_notify", v.autoNotify);
    object->setProperty ("auto_last_level", qualityLevelKey (v.autoLastLevel));
    object->setProperty ("emergency_string_drop", v.emergencyStringDrop);
    return juce::JSON::toString (juce::var (object), false);
}

bool PerformanceSettings::load()
{
    const auto file = getFile();
    const auto before = getValues();

    Values v;
    bool ok = false;

    if (file.existsAsFile())
    {
        ok = parse (file.loadFileAsString(), v);
        lastModification = file.getLastModificationTime();

        if (! ok)
            DBG ("performance.json unreadable; using the defaults");
    }

    store (v);

    if (before != v)
        sendChangeMessage();

    return ok;
}

void PerformanceSettings::reloadIfChanged()
{
    const auto file = getFile();

    if (file.existsAsFile() && file.getLastModificationTime() != lastModification)
        load();
}

bool PerformanceSettings::save() const
{
    const auto file = getFile();
    file.getParentDirectory().createDirectory();

    // file-formats 13: write beside the target, then rename over it.
    juce::TemporaryFile temp (file, juce::TemporaryFile::useHiddenFile);

    if (! temp.getFile().replaceWithText (toJson (getValues())))
        return false;

    const bool ok = temp.overwriteTargetFileWithTemporary();

    if (ok)
    {
        ++saveCount;
        const_cast<PerformanceSettings*> (this)->lastModification = file.getLastModificationTime();
    }

    return ok;
}

void PerformanceSettings::changed()
{
    save();
    sendChangeMessage();
}

//==============================================================================
void PerformanceSettings::setValues (const Values& v)
{
    if (v == getValues())
        return;

    store (v);
    changed();
}

void PerformanceSettings::setQuality (QualityChoice q)            { auto v = getValues(); v.quality = q; setValues (v); }
void PerformanceSettings::setOfflineAtHigh (bool b)               { auto v = getValues(); v.offlineAtHigh = b; setValues (v); }
void PerformanceSettings::setAutoNotify (bool b)                  { auto v = getValues(); v.autoNotify = b; setValues (v); }
void PerformanceSettings::setAutoLastLevel (QualityLevel l)       { auto v = getValues(); v.autoLastLevel = l; setValues (v); }
void PerformanceSettings::setEmergencyStringDrop (bool b)         { auto v = getValues(); v.emergencyStringDrop = b; setValues (v); }

void PerformanceSettings::resetToDefaults()
{
    store (Values {});
    changed();
}

} // namespace luthier
