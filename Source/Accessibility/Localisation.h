#pragma once

/*  The string catalog and locale handling (accessibility.md section 6).

    Rule 4 of section 0: every UI string lives in a catalog, not in source. The
    catalog is a flat map of stable key to display string, loaded from
    `Resources/i18n/<locale>.json`.

    Rule "no string concatenation in code" is the one that shapes the API. A
    sentence assembled from fragments cannot be translated: word order differs
    between languages, and a fragment has no grammatical context. So the only way
    to build a string with a value in it is `format`, with named placeholders:

        tr ("presets.loaded", { { "n", juce::String (count) } })

    which a translator sees as "Loaded {n} presets" and can reorder freely.

    A missing key falls back to the fallback locale and then to English, and a
    key missing from English is logged at debug level and returned as itself, so
    a missing translation shows a key rather than an empty label.
*/

#include <juce_core/juce_core.h>

#include <map>
#include <vector>

namespace luthier
{

//==============================================================================
/** One locale the plugin ships with (accessibility.md 6). */
struct LocaleInfo
{
    juce::String code;         ///< "en", "pt-BR", "zh-CN".
    juce::String englishName;
    juce::String nativeName;

    /** True for right-to-left scripts. None ship, but the layout must not
        assume otherwise (accessibility 6). */
    bool rightToLeft = false;

    /** True when the script needs a fallback font the theme does not carry. */
    bool needsCjkFont = false;
};

//==============================================================================
class Localisation
{
public:
    /** accessibility.md 6 names fifteen locales at ship. */
    static constexpr int kNumShipLocales = 15;

    static Localisation& get();

    //==========================================================================
    /** Every locale the plugin ships with. */
    static const std::vector<LocaleInfo>& getShipLocales();

    static const LocaleInfo* findLocale (const juce::String& code);

    //==========================================================================
    /** Switches locale. accessibility 6: no restart; the caller repaints. */
    bool setLocale (const juce::String& code);
    juce::String getLocale() const { return currentLocale; }

    void setFallbackLocale (const juce::String& code);
    juce::String getFallbackLocale() const { return fallbackLocale; }

    /** accessibility 9: an advanced option, for a catalog outside Resources. */
    void setCustomCatalogDirectory (const juce::File& directory);

    //==========================================================================
    /** Looks a key up. Returns the key itself when nothing has it, so a missing
        string is visible rather than blank. */
    juce::String translate (const juce::String& key) const;

    /** Looks a key up and substitutes named placeholders. */
    juce::String translate (const juce::String& key,
                            const std::map<juce::String, juce::String>& values) const;

    /** True when the key exists in the current or fallback catalog. */
    bool hasKey (const juce::String& key) const;

    //==========================================================================
    /** Every key the built-in English catalog defines, for the completeness
        test and for the translator tooling. */
    juce::StringArray getAllKeys() const;

    /** Keys the current locale is missing relative to English. */
    juce::StringArray getMissingKeys() const;

    /** Writes the current catalog out as JSON, for a translator to work from. */
    bool exportCatalog (const juce::File& file) const;

    static juce::File getCatalogDirectory();

    //==========================================================================
    /** True when the current locale is right to left. */
    bool isRightToLeft() const;

    /** True when the current locale needs the CJK fallback font stack
        (accessibility 8). */
    bool needsCjkFallbackFont() const;

private:
    Localisation();

    /** The built-in English catalog. Compiled in rather than loaded, so the
        plugin can always draw its own UI even with no resources installed. */
    static const std::map<juce::String, juce::String>& getBuiltInEnglish();

    bool loadCatalog (const juce::String& code,
                      std::map<juce::String, juce::String>& destination) const;

    /** Substitutes `{name}` placeholders. */
    static juce::String substitute (const juce::String& text,
                                    const std::map<juce::String, juce::String>& values);

    juce::String currentLocale { "en" };
    juce::String fallbackLocale { "en" };

    std::map<juce::String, juce::String> current;
    std::map<juce::String, juce::String> fallback;

    juce::File customCatalogDirectory;

    mutable juce::StringArray loggedMissingKeys;
};

//==============================================================================
/** The short form used at every call site. */
inline juce::String tr (const juce::String& key)
{
    return Localisation::get().translate (key);
}

inline juce::String tr (const juce::String& key,
                        const std::map<juce::String, juce::String>& values)
{
    return Localisation::get().translate (key, values);
}

} // namespace luthier
