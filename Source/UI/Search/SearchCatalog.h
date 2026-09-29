#pragma once

/*  global-search.md 2 and 14: the palette's strings, titles and synonyms.

    Every string the palette shows comes from a catalog key under `search.*`
    with named placeholders (accessibility 6). The loaded locale's catalog
    (Resources/i18n/<locale>.json, or a custom catalog directory) wins; the
    built-in English table here is the fallback, the same arrangement as
    Localisation's own compiled-in English. It lives here rather than in
    Localisation.cpp so the search workstream does not edit that shared file.

    Keys:
      search.<ui string>          the palette's own text
      search.title.<itemId>       a title that differs from the item's own name
                                  ("Amp gain", where the parameter is "Gain")
      search.syn.<itemId>         '|'-separated synonyms (2)
*/

#include <juce_core/juce_core.h>

#include <map>

namespace luthier::search::SearchCatalog
{
    /** The current locale's text for a key, else the built-in English, else
        empty (never the key: a missing synonym list is simply none). */
    juce::String text (const juce::String& key);
    juce::String text (const juce::String& key, const std::map<juce::String, juce::String>& values);

    /** The built-in English only (the English title in every locale, 2). */
    juce::String english (const juce::String& key);

    /** True when the current locale's catalog has this key itself. */
    bool isLocalised (const juce::String& key);

    /** search.syn.<itemId>, localized then English (13: "Locale has no
        synonyms -> English synonyms apply"). */
    juce::StringArray synonyms (const juce::String& itemId);

    /** 2: "The unit, spelled out: dB -> decibel...". */
    juce::StringArray unitWords (const juce::String& unitLabel);

    const std::map<juce::String, juce::String>& builtInEnglish();
}
