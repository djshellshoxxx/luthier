#include "FirstRun.h"
#include "UiPreferences.h"
#include "../Accessibility/Localisation.h"

#include <atomic>

namespace luthier
{

// FirstRunOs.cpp: the platform reads, in a file with no JUCE header in it.
namespace FirstRunOs
{
    bool isHighContrastOn() noexcept;
    bool isReducedMotionOn() noexcept;
}

namespace
{
    /** Whether applyIfFirstRun has looked yet in this process; a second editor
        (a second instance in the same host) must not apply anything. */
    std::atomic<bool> checked { false };

    /** onboarding 8 and 9: this process is the first session. */
    std::atomic<bool> firstSession { false };
}

//==============================================================================
FirstRun::OsPreferences FirstRun::readOsPreferences()
{
    OsPreferences os;

    os.highContrast = FirstRunOs::isHighContrastOn();
    os.reducedMotion = FirstRunOs::isReducedMotionOn();

    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        os.displayScale = display->scale;

    os.language = juce::SystemStats::getUserLanguage();
    os.region = juce::SystemStats::getUserRegion();

    return os;
}

juce::String FirstRun::matchShipLocale (const juce::String& language, const juce::String& region)
{
    const auto lang = language.trim().toLowerCase();
    const auto reg = region.trim().toUpperCase();

    if (lang.isEmpty())
        return "en";

    // An exact tag: en-GB, pt-BR, zh-CN, zh-TW.
    if (reg.isNotEmpty())
        if (const auto* locale = Localisation::findLocale (lang + "-" + reg))
            return locale->code;

    // The tags that cover a region rather than name one.
    if (lang == "zh")
        return (reg == "TW" || reg == "HK" || reg == "MO") ? "zh-TW" : "zh-CN";

    // Spanish outside Spain is the Latin American catalog, the US included.
    if (lang == "es" && reg.isNotEmpty() && reg != "ES")
        return "es-419";

    // The bare language: fr-CA reads French, de-AT German. pt-PT is not
    // shipped and has no bare "pt", so it falls through to English, as
    // onboarding 5 says ("otherwise en").
    if (const auto* locale = Localisation::findLocale (lang))
        return locale->code;

    return "en";
}

FirstRun::Defaults FirstRun::defaultsFor (const OsPreferences& os)
{
    Defaults d;

    // onboarding 5, line by line.
    d.palette = os.highContrast ? PaletteId::highContrast : PaletteId::defaultDark;
    d.reducedMotion = os.reducedMotion;
    d.uiScale = os.displayScale > 1.5 ? 1.25 : 1.0;
    d.locale = matchShipLocale (os.language, os.region);

    return d;
}

//==============================================================================
bool FirstRun::isFirstRun()
{
    auto& preferences = UiPreferences::get();

    if (preferences.getBool (kPendingKey, false))
        return true;

    if (preferences.getBool (kCompletedKey, false))
        return false;

    return ! UiPreferences::getConfigFile().existsAsFile()
        && ! AccessibilitySettings::getConfigFile().existsAsFile();
}

bool FirstRun::applyIfFirstRun()
{
    if (checked.load())
        return false;

    return applyIfFirstRun (readOsPreferences());
}

bool FirstRun::applyIfFirstRun (const OsPreferences& os)
{
    if (checked.exchange (true))
        return false;

    auto& preferences = UiPreferences::get();

    if (! isFirstRun())
    {
        // An upgrade from a build without this: settings exist and are the
        // person's. Recorded so the question is settled from now on.
        preferences.setBool (kCompletedKey, true);
        return false;
    }

    const auto defaults = defaultsFor (os);
    auto& settings = AccessibilitySettings::get();

    settings.setPalette (defaults.palette);
    settings.setReducedMotion (defaults.reducedMotion);
    settings.setUiScale (defaults.uiScale);
    Localisation::get().setLocale (defaults.locale);
    settings.save();

    preferences.setBool (kPendingKey, false);
    preferences.setBool (kCompletedKey, true);

    firstSession = true;
    return true;
}

bool FirstRun::isFirstSession() noexcept
{
    return firstSession.load();
}

void FirstRun::setStateForTesting (bool isChecked, bool isFirstSession) noexcept
{
    checked = isChecked;
    firstSession = isFirstSession;
}

//==============================================================================
void FirstRun::restoreFirstRunExperience()
{
    /*  UiPreferences first. Emptying it is what re-arms every one-time thing
        kept there - onboarding 7's range explainer (TODO 14c), the TUNE and
        Workshop hints - and the pending flag is what makes the next launch a
        first run even though the file now exists. */
    auto& preferences = UiPreferences::get();
    preferences.reset();
    preferences.setBool (kPendingKey, true);

    /*  Then the person's settings, back to what a fresh install has, now and on
        disk. Set explicitly rather than from an empty object: fromVar reads a
        missing verbosity as 0, which is Minimal, and a fresh install is
        Standard. No "shortcuts" entry resets every binding. */
    auto* root = new juce::DynamicObject();
    root->setProperty ("palette", (int) PaletteId::defaultDark);
    root->setProperty ("uiScale", 1.0);
    root->setProperty ("reducedMotion", false);
    root->setProperty ("verbosity", (int) AccessibilitySettings::Verbosity::standard);
    root->setProperty ("fontOverride", juce::String());
    root->setProperty ("fallbackLocale", "en");
    root->setProperty ("locale", "en");

    auto& settings = AccessibilitySettings::get();
    settings.fromVar (juce::var (root));
    settings.save();
}

} // namespace luthier
