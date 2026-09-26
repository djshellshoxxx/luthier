/*  Accessibility and localization tests (accessibility.md section 10).

    Two of the spec's tests need a running screen reader and a human, and are
    listed in the known-issues file rather than faked here: the NVDA smoke run
    and the missing-glyph check. What can be checked without either is checked,
    and it is the part that would actually regress: the contrast ratios, the
    completeness of the string catalog, the placeholder discipline, the scale
    steps and the shortcut table.
*/

#include "TestFramework.h"

#include "../Accessibility/Accessibility.h"
#include "../Accessibility/Localisation.h"

using namespace luthier;
using namespace luthier::tests;

//==============================================================================
/*  accessibility 10: text on background must reach WCAG AA - 4.5 to one - on
    Default, High contrast and Light. */
LUTHIER_TEST (Accessibility, palettesMeetContrastRequirements)
{
    auto& settings = AccessibilitySettings::get();

    const auto originalPalette = settings.getPalette();

    // The three the spec names explicitly.
    for (auto id : { PaletteId::defaultDark, PaletteId::highContrast, PaletteId::light })
    {
        settings.setPalette (id);

        const double worst = settings.getColours().getWorstTextContrast();

        CHECK_MSG (worst >= 4.5,
                   juce::String (getPaletteName (id)) + ": worst text contrast is "
                     + juce::String (worst, 2) + ", needs 4.5");
    }

    // The colourblind palettes are held to the same standard: a palette that is
    // distinguishable but unreadable helps nobody.
    for (auto id : { PaletteId::deuteranopia, PaletteId::protanopia, PaletteId::tritanopia })
    {
        settings.setPalette (id);

        const double worst = settings.getColours().getWorstTextContrast();

        CHECK_MSG (worst >= 4.5,
                   juce::String (getPaletteName (id)) + ": worst text contrast is "
                     + juce::String (worst, 2));
    }

    settings.setPalette (originalPalette);
}

//==============================================================================
/*  The contrast calculation itself, against values with known answers. A wrong
    formula here would make every palette check meaningless. */
LUTHIER_TEST (Accessibility, contrastRatioIsCorrect)
{
    // Black on white is the maximum: 21 to 1.
    CHECK_NEAR (PaletteColours::contrastRatio (juce::Colours::black, juce::Colours::white),
                21.0, 0.01);

    // A colour against itself is 1 to 1.
    CHECK_NEAR (PaletteColours::contrastRatio (juce::Colours::red, juce::Colours::red),
                1.0, 0.001);

    // It is symmetric.
    CHECK_NEAR (PaletteColours::contrastRatio (juce::Colours::black, juce::Colours::white),
                PaletteColours::contrastRatio (juce::Colours::white, juce::Colours::black),
                0.001);

    // Mid grey on white is a known value: about 3.95 to 1 for #808080.
    const auto midGrey = juce::Colour (0xff808080);

    CHECK_NEAR (PaletteColours::contrastRatio (midGrey, juce::Colours::white), 3.95, 0.1);
}

//==============================================================================
/*  accessibility 3: the colourblind palettes must actually separate the states
    that the default palette confuses. */
LUTHIER_TEST (Accessibility, colourblindPalettesSeparateTheStatesTheyTarget)
{
    auto& settings = AccessibilitySettings::get();
    const auto original = settings.getPalette();

    /*  A crude simulation of dichromacy.

        A deuteranope's red and green channels collapse toward each other. What
        matters for this test is not accuracy of the simulation but whether the
        palette's success and warning colours remain distinguishable once they
        have been collapsed, which a palette that relies on red-versus-green
        would fail.
    */
    auto simulateDeuteranopia = [] (juce::Colour c)
    {
        const float r = c.getFloatRed(), g = c.getFloatGreen(), b = c.getFloatBlue();
        const float merged = (r * 0.625f + g * 0.375f);

        return juce::Colour::fromFloatRGBA (merged, merged, b, 1.0f);
    };

    auto simulateProtanopia = [] (juce::Colour c)
    {
        const float r = c.getFloatRed(), g = c.getFloatGreen(), b = c.getFloatBlue();
        const float merged = (r * 0.4f + g * 0.6f);

        return juce::Colour::fromFloatRGBA (merged, merged, b, 1.0f);
    };

    // The default palette's success and warning are both warm, and a deuteranope
    // sees them as close. That is exactly why the alternate palettes exist.
    settings.setPalette (PaletteId::deuteranopia);

    {
        const auto& colours = settings.getColours();

        const double separation = PaletteColours::contrastRatio (
            simulateDeuteranopia (colours.success),
            simulateDeuteranopia (colours.warning));

        CHECK_MSG (separation > 1.6,
                   "under deuteranopia the success and warning colours separate by only "
                     + juce::String (separation, 2));
    }

    settings.setPalette (PaletteId::protanopia);

    {
        const auto& colours = settings.getColours();

        const double separation = PaletteColours::contrastRatio (
            simulateProtanopia (colours.clip),
            simulateProtanopia (colours.warning));

        CHECK_MSG (separation > 1.6,
                   "under protanopia the clip and warning colours separate by only "
                     + juce::String (separation, 2));
    }

    settings.setPalette (original);
}

//==============================================================================
/*  accessibility 3: the palettes round-trip through their theme files. */
LUTHIER_TEST (Accessibility, palettesRoundTripThroughJson)
{
    for (int i = 0; i < (int) PaletteId::numPalettes; ++i)
    {
        auto& settings = AccessibilitySettings::get();
        settings.setPalette ((PaletteId) i);

        const auto original = settings.getColours();

        const auto text = juce::JSON::toString (original.toVar(), false);
        const auto restored = PaletteColours::fromVar (juce::JSON::parse (text));

        CHECK_MSG (restored.background == original.background,
                   juce::String (getPaletteName ((PaletteId) i)) + ": background lost");

        CHECK (restored.textPrimary == original.textPrimary);
        CHECK (restored.accent == original.accent);
        CHECK (restored.success == original.success);
        CHECK (restored.warning == original.warning);
        CHECK (restored.clip == original.clip);

        CHECK_NEAR (restored.getWorstTextContrast(), original.getWorstTextContrast(), 0.001);
    }
}

//==============================================================================
/*  accessibility 4: the six scales, snapping, and the graceful step down. */
LUTHIER_TEST (Accessibility, uiScaleStepsAndFontFloor)
{
    auto& settings = AccessibilitySettings::get();

    const double originalScale = settings.getUiScale();

    CHECK (AccessibilitySettings::kNumScales == 6);

    // The spec's six: 75 to 200 percent.
    CHECK_NEAR (AccessibilitySettings::kScales[0], 0.75, 1.0e-9);
    CHECK_NEAR (AccessibilitySettings::kScales[5], 2.0, 1.0e-9);

    for (double scale : AccessibilitySettings::kScales)
    {
        settings.setUiScale (scale);
        CHECK_NEAR (settings.getUiScale(), scale, 1.0e-9);
    }

    // A value in between snaps to the nearest offered step, so a stray number in
    // a config file cannot put the layout somewhere it was never checked.
    settings.setUiScale (1.31);
    CHECK_NEAR (settings.getUiScale(), 1.25, 1.0e-9);

    settings.setUiScale (5.0);
    CHECK_NEAR (settings.getUiScale(), 2.0, 1.0e-9);

    // accessibility 4: text never drops below a readable size.
    settings.setUiScale (0.75);

    for (float points : { 8.0f, 9.0f, 10.0f, 12.0f, 20.0f })
        CHECK_MSG (settings.scaledFont (points) >= 9.0f,
                   "a " + juce::String (points) + "pt font scaled to "
                     + juce::String (settings.scaledFont (points)) + " at 75%");

    // And scaling a size is monotonic and never zero.
    settings.setUiScale (2.0);
    CHECK (settings.scaled (10) > settings.scaled (5));
    CHECK (settings.scaled (1) >= 1);

    // The step down, for a window that no longer fits.
    settings.setUiScale (1.5);
    CHECK (settings.stepScaleDown());
    CHECK_NEAR (settings.getUiScale(), 1.25, 1.0e-9);

    settings.setUiScale (0.75);
    CHECK_MSG (! settings.stepScaleDown(), "the smallest scale stepped down further");

    settings.setUiScale (originalScale);
}

//==============================================================================
/*  accessibility 5: reduced motion removes the animation rather than shortening
    it. */
LUTHIER_TEST (Accessibility, reducedMotionRemovesAnimation)
{
    auto& settings = AccessibilitySettings::get();

    settings.setReducedMotion (false);
    CHECK (settings.getAnimationMs (80) == 80);

    settings.setReducedMotion (true);
    CHECK_MSG (settings.getAnimationMs (80) == 0,
               "reduced motion left an animation of "
                 + juce::String (settings.getAnimationMs (80)) + " ms");

    CHECK (settings.getAnimationMs (1000) == 0);

    settings.setReducedMotion (false);
}

//==============================================================================
/*  accessibility 6: every locale the spec names is offered, and English is
    complete. */
LUTHIER_TEST (Localisation, everyShipLocaleIsOffered)
{
    const auto& locales = Localisation::getShipLocales();

    CHECK_MSG ((int) locales.size() == Localisation::kNumShipLocales,
               juce::String ((int) locales.size()) + " locales, the spec names "
                 + juce::String (Localisation::kNumShipLocales));

    // The exact set the spec lists.
    for (const char* code : { "en", "en-GB", "es", "es-419", "fr", "de", "pt-BR",
                              "ja", "zh-CN", "zh-TW", "ko", "ru", "pl", "nl", "it" })
        CHECK_MSG (Localisation::findLocale (code) != nullptr,
                   juce::String ("missing locale ") + code);

    for (const auto& locale : locales)
    {
        CHECK (locale.code.isNotEmpty());
        CHECK (locale.englishName.isNotEmpty());
        CHECK (locale.nativeName.isNotEmpty());
    }

    // The CJK locales are flagged, because they need a fallback font.
    for (const char* code : { "ja", "zh-CN", "zh-TW", "ko" })
    {
        const auto* locale = Localisation::findLocale (code);

        CHECK (locale != nullptr);

        if (locale != nullptr)
            CHECK_MSG (locale->needsCjkFont,
                       juce::String (code) + " is not flagged as needing a CJK font");
    }

    // And a Latin one is not.
    const auto* french = Localisation::findLocale ("fr");
    CHECK (french != nullptr && ! french->needsCjkFont);
}

//==============================================================================
/*  accessibility 6: no string concatenation in code, which means every string
    that takes a value uses a named placeholder. */
LUTHIER_TEST (Localisation, placeholdersAreNamedAndSubstitute)
{
    auto& localisation = Localisation::get();

    localisation.setLocale ("en");

    // A key with a placeholder substitutes it.
    const auto loaded = localisation.translate ("message.presetsLoaded", { { "n", "36" } });

    CHECK_MSG (loaded.contains ("36"),
               "the placeholder was not substituted: " + loaded);

    CHECK_MSG (! loaded.contains ("{n}"),
               "the placeholder was left in place: " + loaded);

    // Several placeholders in one string.
    const auto ir = localisation.translate ("message.irLoaded",
                                            { { "name", "Greenback" }, { "ms", "500" } });

    CHECK (ir.contains ("Greenback"));
    CHECK (ir.contains ("500"));
    CHECK (! ir.contains ("{"));

    // Every placeholder in the catalog is named rather than positional: a
    // positional one cannot be reordered by a translator, which is the whole
    // reason the rule exists.
    for (const auto& key : localisation.getAllKeys())
    {
        const auto text = localisation.translate (key);

        int position = text.indexOfChar ('{');

        while (position >= 0)
        {
            const int close = text.indexOfChar (position, '}');

            CHECK_MSG (close > position,
                       key + ": an unclosed placeholder in \"" + text + "\"");

            if (close <= position)
                break;

            const auto name = text.substring (position + 1, close);

            CHECK_MSG (name.isNotEmpty() && ! name.containsOnly ("0123456789"),
                       key + ": placeholder {" + name + "} is positional, not named");

            position = text.indexOfChar (close, '{');
        }
    }
}

//==============================================================================
/*  A missing key falls back rather than showing nothing. */
LUTHIER_TEST (Localisation, missingKeysFallBackRatherThanBlank)
{
    auto& localisation = Localisation::get();

    localisation.setLocale ("en");

    // A key nothing defines comes back as itself, which is visible in the UI
    // and therefore reportable, rather than as an empty label.
    const auto missing = localisation.translate ("this.key.does.not.exist");

    CHECK_MSG (missing == "this.key.does.not.exist",
               "a missing key returned \"" + missing + "\" rather than the key");

    CHECK (! localisation.hasKey ("this.key.does.not.exist"));

    // A locale with no catalog on disk falls back to English rather than to
    // nothing, and reports that it could not honour the request.
    const bool switched = localisation.setLocale ("zz");

    CHECK_MSG (! switched, "a locale with no catalog reported success");

    // The UI still has its strings.
    CHECK (localisation.translate ("app.name") == "Luthier");

    localisation.setLocale ("en");
}

//==============================================================================
/*  The catalog has to cover the UI: a key used at a call site but absent here
    would show as raw text. */
LUTHIER_TEST (Localisation, catalogCoversTheUi)
{
    auto& localisation = Localisation::get();
    localisation.setLocale ("en");

    const auto keys = localisation.getAllKeys();

    CHECK_MSG (keys.size() > 150,
               "the catalog has only " + juce::String (keys.size()) + " keys");

    // Every key resolves to something that is not the key itself.
    for (const auto& key : keys)
    {
        const auto text = localisation.translate (key);

        CHECK_MSG (text != key, key + " resolved to itself");
        CHECK_MSG (text.isNotEmpty(), key + " resolved to an empty string");
    }

    // The areas the plugin actually has are all represented.
    for (const char* prefix : { "app.", "header.", "macro.", "easy.", "advanced.",
                                "rhythm.", "live.", "practice.", "tonematch.",
                                "character.", "accessibility.", "a11y.", "routing.",
                                "updates.", "privacy.", "common.", "message." })
    {
        bool found = false;

        for (const auto& key : keys)
        {
            if (key.startsWith (prefix))
            {
                found = true;
                break;
            }
        }

        CHECK_MSG (found, juce::String ("no catalog keys for ") + prefix);
    }

    // Keys are stable identifiers, not English text.
    for (const auto& key : keys)
    {
        CHECK_MSG (! key.containsChar (' '), "key \"" + key + "\" contains a space");
        CHECK_MSG (key.containsChar ('.'), "key \"" + key + "\" is not dotted");
    }
}

//==============================================================================
/*  accessibility 2: every action is rebindable, clashes are refused, and the
    table can be printed. */
LUTHIER_TEST (Accessibility, shortcutsRebindAndRefuseClashes)
{
    auto& settings = AccessibilitySettings::get();

    settings.resetAllShortcuts();

    const auto& shortcuts = settings.getShortcuts();

    CHECK_MSG (shortcuts.size() >= 15,
               "only " + juce::String ((int) shortcuts.size()) + " shortcuts defined");

    // No two actions share a key to begin with.
    for (size_t i = 0; i < shortcuts.size(); ++i)
    {
        for (size_t j = i + 1; j < shortcuts.size(); ++j)
        {
            CHECK_MSG (! (shortcuts[i].key == shortcuts[j].key),
                       shortcuts[i].id + " and " + shortcuts[j].id + " share a key");
        }
    }

    // Every action has a description key, or the rebind table cannot label it.
    for (const auto& binding : shortcuts)
    {
        CHECK (binding.id.isNotEmpty());
        CHECK (binding.descriptionKey.isNotEmpty());
        CHECK (! binding.isRebound());
    }

    // ---- rebinding -----------------------------------------------------------------
    const auto freeKey = juce::KeyPress ('q', juce::ModifierKeys::commandModifier, 0);

    CHECK_MSG (settings.rebind ("panic", freeKey), "rebinding to a free key failed");

    const auto* panic = settings.findShortcut ("panic");
    CHECK (panic != nullptr && panic->key == freeKey);
    CHECK (panic != nullptr && panic->isRebound());

    CHECK (settings.findAction (freeKey) == "panic");

    // A key another action holds is refused rather than stolen.
    const auto* undo = settings.findShortcut ("undo");
    CHECK (undo != nullptr);

    if (undo != nullptr)
    {
        CHECK_MSG (! settings.rebind ("panic", undo->key),
                   "rebinding onto another action's key was allowed");

        // And the original binding is untouched.
        CHECK (settings.findAction (undo->key) == "undo");
    }

    // Rebinding an action that does not exist fails rather than adding one.
    CHECK (! settings.rebind ("nosuchaction", juce::KeyPress ('x', 0, 0)));

    // ---- reset ------------------------------------------------------------------------
    settings.resetShortcut ("panic");
    CHECK (settings.findShortcut ("panic") != nullptr
             && ! settings.findShortcut ("panic")->isRebound());

    settings.resetAllShortcuts();

    for (const auto& binding : settings.getShortcuts())
        CHECK (! binding.isRebound());

    // ---- printable ----------------------------------------------------------------------
    const auto printable = settings.getPrintableShortcuts();

    CHECK (printable.size() == (int) shortcuts.size());

    for (const auto& line : printable)
        CHECK (line.isNotEmpty());
}

//==============================================================================
/*  Every binding has a string in the catalog.

    This is the check that was missing when all twenty-five of them were not in
    it. `translate` returns the key itself when there is no string for it, so the
    rebind table in Options > ACCESSIBILITY rendered "accessibility.shortcut.undo"
    as the description of Ctrl+Z, and getPrintableShortcuts printed the same. Both
    surfaces looked built and worked: every row was there, every key was right,
    and every label was a key.

    Asserting `isNotEmpty` is what let that through - a key is not empty. So this
    asks for the one thing the fallback cannot fake: a description that is not the
    key it was looked up by.
*/
LUTHIER_TEST (Accessibility, everyShortcutHasADescriptionInTheCatalog)
{
    auto& settings = AccessibilitySettings::get();

    for (const auto& binding : settings.getShortcuts())
    {
        CHECK_MSG (binding.descriptionKey.isNotEmpty(),
                   "the shortcut \"" + binding.id + "\" has no description key");

        CHECK_MSG (Localisation::get().hasKey (binding.descriptionKey),
                   "no catalog string for \"" + binding.descriptionKey + "\", so the rebind "
                     "table shows the key");

        const auto description = tr (binding.descriptionKey);

        CHECK_MSG (description != binding.descriptionKey,
                   "the description of \"" + binding.id + "\" is its own key");
    }

    /*  And the printable list carries them, since that is what the manual and the
        shortcut overlay read. */
    for (const auto& line : settings.getPrintableShortcuts())
        CHECK_MSG (! line.contains ("accessibility.shortcut."),
                   "a printed shortcut is showing a raw key: " + line);
}

//==============================================================================
/*  The settings are user-global and have to survive a restart. */
LUTHIER_TEST (Accessibility, settingsRoundTrip)
{
    auto& settings = AccessibilitySettings::get();

    const auto originalPalette = settings.getPalette();
    const auto originalScale = settings.getUiScale();

    settings.setPalette (PaletteId::highContrast);
    settings.setUiScale (1.5);
    settings.setReducedMotion (true);
    settings.setVerbosity (AccessibilitySettings::Verbosity::verbose);
    settings.setFontOverride ("Arial");
    // Ctrl+J: free. (Ctrl+K was, until global-search.md made it Search.)
    settings.rebind ("panic", juce::KeyPress ('j', juce::ModifierKeys::commandModifier, 0));

    const auto text = juce::JSON::toString (settings.toVar(), false);

    // Put it back to something else, then restore.
    settings.setPalette (PaletteId::defaultDark);
    settings.setUiScale (1.0);
    settings.setReducedMotion (false);
    settings.setFontOverride ({});
    settings.resetAllShortcuts();

    settings.fromVar (juce::JSON::parse (text));

    CHECK (settings.getPalette() == PaletteId::highContrast);
    CHECK_NEAR (settings.getUiScale(), 1.5, 1.0e-9);
    CHECK (settings.isReducedMotion());
    CHECK (settings.getVerbosity() == AccessibilitySettings::Verbosity::verbose);
    CHECK (settings.getFontOverride() == "Arial");

    const auto* panic = settings.findShortcut ("panic");
    CHECK (panic != nullptr && panic->isRebound());

    // Clean up, so the singleton does not leak state into other tests.
    settings.setPalette (originalPalette);
    settings.setUiScale (originalScale);
    settings.setReducedMotion (false);
    settings.setVerbosity (AccessibilitySettings::Verbosity::standard);
    settings.setFontOverride ({});
    settings.resetAllShortcuts();
}

//==============================================================================
/*  gui-integration.md section 17 is the canonical binding table, and section 17
    says every one of them is rebindable. That is only true if the registry is
    where the plugin actually looks, so this pins the defaults to the document.

    The editor is not constructed here - Source/UI is not in this target - so what
    is checked is the contract the editor depends on: that each action exists under
    the id PluginEditor asks for, with section 17's default key. */
LUTHIER_TEST (Accessibility, shortcutDefaultsMatchTheCanonicalTable)
{
    auto& settings = AccessibilitySettings::get();
    settings.resetAllShortcuts();

    using KP = juce::KeyPress;

    const auto cmd   = juce::ModifierKeys::commandModifier;
    const auto shift = juce::ModifierKeys::shiftModifier;

    struct Expected { const char* id; juce::KeyPress key; };

    const Expected expected[] =
    {
        { "help",            KP (KP::F1Key) },
        { "showShortcuts",   KP ('/', cmd | shift, 0) },
        { "toggleAdvanced",  KP (KP::tabKey) },
        { "toggleLiveMode",  KP ('l', 0, 0) },
        { "togglePractice",  KP ('d', 0, 0) },
        { "panic",           KP ('p', 0, 0) },
        { "tapTempo",        KP ('t', 0, 0) },
        { "killSwitch",      KP ('\\', 0, 0) },
        { "previousItem",    KP ('[', 0, 0) },
        { "nextItem",        KP (']', 0, 0) },
        { "setlistPrevious", KP (KP::pageUpKey) },
        { "setlistNext",     KP (KP::pageDownKey) },
        { "undo",            KP ('z', cmd, 0) },
        { "redo",            KP ('z', cmd | shift, 0) },
        { "undoAcrossBoundary", KP ('z', cmd | juce::ModifierKeys::altModifier, 0) },   // action-and-undo.md 9
       #if ! JUCE_MAC
        { "redoAlt",         KP ('y', cmd, 0) },   // action-and-undo.md 9
       #endif
        { "save",            KP ('s', cmd, 0) },
        { "saveAs",          KP ('s', cmd | shift, 0) },
        { "presetBrowser",   KP ('o', cmd, 0) },
        { "abCompare",       KP ('/', cmd, 0) },
        { "randomise",       KP ('r', cmd, 0) },
        { "resetAll",        KP ('r', cmd | shift, 0) },
        { "midiLearnArm",    KP ('l', cmd, 0) },
        { "export",          KP ('e', cmd, 0) },
        { "saveGuitarAs",    KP ('g', cmd, 0) },
        { "revealGuitar",    KP ('e', cmd | shift, 0) },
        { "options",         KP (',', cmd, 0) }
    };

    for (const auto& row : expected)
    {
        const auto* binding = settings.findShortcut (row.id);

        CHECK_MSG (binding != nullptr,
                   juce::String ("section 17 lists an action the registry does not "
                                 "have: ") + row.id);

        if (binding == nullptr)
            continue;

        CHECK_MSG (binding->key == row.key,
                   juce::String (row.id) + " defaults to "
                     + binding->key.getTextDescription() + ", but section 17 says "
                     + row.key.getTextDescription());
    }
}

//==============================================================================
/*  A default table with a collision would leave one of the two actions
    unreachable, and rebind() refuses collisions, so a clash would also make that
    action impossible to rebind out of the way. */
LUTHIER_TEST (Accessibility, noTwoShortcutsShareADefaultKey)
{
    auto& settings = AccessibilitySettings::get();
    settings.resetAllShortcuts();

    const auto& shortcuts = settings.getShortcuts();

    for (size_t i = 0; i < shortcuts.size(); ++i)
        for (size_t j = i + 1; j < shortcuts.size(); ++j)
            CHECK_MSG (! (shortcuts[i].key == shortcuts[j].key),
                       shortcuts[i].id + " and " + shortcuts[j].id
                         + " both default to " + shortcuts[i].key.getTextDescription());
}
