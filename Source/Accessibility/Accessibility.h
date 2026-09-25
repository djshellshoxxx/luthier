#pragma once

/*  Accessibility settings (accessibility.md sections 3, 4, 5 and 9).

    Four things live here: the alternate colour palettes, the UI scale, the
    reduced-motion switch, and the rebindable shortcut table.

    Rule 2 of section 0 is the one that reaches furthest into the rest of the UI:
    nothing important may be conveyed by colour alone. A palette can therefore
    only ever be half the answer - the other half is that every colour-coded
    state also has a shape, a position or a label, which is a property of the
    components rather than of this file. What this file guarantees is that the
    palettes themselves meet the contrast the spec asks for, which is checkable
    and is checked.
*/

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <map>
#include <vector>

namespace luthier
{

//==============================================================================
/** The palettes (accessibility.md 3). */
enum class PaletteId
{
    defaultDark = 0,
    deuteranopia,     ///< Green-blind.
    protanopia,       ///< Red-blind.
    tritanopia,       ///< Blue-blind.
    highContrast,
    light,
    /** A flat, modern dark mode: charcoal surfaces, off-white text, one blue
        accent, no guitar-shop materials. Appended last so saved palette
        numbers (settings files, first-run defaults) keep their meaning. */
    modernDark,
    numPalettes
};

const char* getPaletteName (PaletteId id) noexcept;

/** Whether a palette dresses the UI in the guitar-shop materials (wood grain,
    brass plates, corner screws, Tolex, lit knob caps). False for High
    contrast and Modern Dark, which draw the same controls flat. */
bool paletteUsesMaterials (PaletteId id) noexcept;

/** Whether the guitar illustration is drawn with its lighting and real
    finishes. Only High contrast flattens it: in Modern Dark the guitar is
    still a picture of a guitar, only the UI around it goes flat. */
bool paletteLightsIllustrations (PaletteId id) noexcept;

//==============================================================================
/** One palette's colours. The names match theme.md's, so a component asks for
    the same colour whichever palette is loaded. */
struct PaletteColours
{
    // The Luthier guitar-shop palette (proposals/visual-polish.md 6.1, approved
    // 2026-09-23): rosewood and walnut, Tolex black, ivory text, aged brass.
    juce::Colour backgroundDeep { 0xff17100c };
    juce::Colour background     { 0xff1e1511 };
    juce::Colour panel          { 0xff2a1e17 };
    juce::Colour panelRaised    { 0xff1d1a18 };
    juce::Colour panelSunken    { 0xff140e0b };
    juce::Colour edge           { 0xff4a3726 };
    juce::Colour edgeBright     { 0xff6b5033 };

    juce::Colour accent         { 0xffd4a24c };
    juce::Colour accentBright   { 0xffe9be6e };
    juce::Colour accentDim      { 0xff8c6a2e };

    juce::Colour secondary      { 0xff6fa58a };
    juce::Colour secondaryDim   { 0xff3e6450 };

    juce::Colour textPrimary    { 0xffefe3cc };
    juce::Colour textMuted      { 0xffb9a58a };
    juce::Colour textDisabled   { 0xff756650 };

    juce::Colour success        { 0xff8fbf6f };
    juce::Colour warning        { 0xfff0824a };
    juce::Colour clip           { 0xffe0503a };

    juce::Colour dataStream     { 0xff7fd18a };
    juce::Colour shadow         { 0x99000000 };

    /** The WCAG contrast ratio between two colours, 1 to 21. */
    static double contrastRatio (juce::Colour a, juce::Colour b) noexcept;

    /** The lowest contrast ratio among the text-on-background pairs that matter.
        accessibility 10 wants at least 4.5 on Default, High contrast and Light. */
    double getWorstTextContrast() const noexcept;

    juce::var toVar() const;
    static PaletteColours fromVar (const juce::var& state);

    bool loadFrom (const juce::File& file);
    bool saveTo (const juce::File& file) const;
};

//==============================================================================
/** A rebindable action (accessibility.md 2). */
struct ShortcutBinding
{
    juce::String id;              ///< Stable, e.g. "panic".
    juce::String descriptionKey;  ///< A localisation key.
    juce::KeyPress key;
    juce::KeyPress defaultKey;

    bool isRebound() const { return ! (key == defaultKey); }
};

//==============================================================================
class AccessibilitySettings : public juce::ChangeBroadcaster
{
public:
    /** accessibility.md 4: 75% to 200%. */
    static constexpr int kNumScales = 6;
    static const std::array<double, kNumScales> kScales;

    /** accessibility.md 9. */
    enum class Verbosity { minimal = 0, standard, verbose, numLevels };

    static const char* getVerbosityName (Verbosity v) noexcept;

    static AccessibilitySettings& get();

    //==========================================================================
    void setPalette (PaletteId id);
    PaletteId getPalette() const noexcept { return palette; }

    const PaletteColours& getColours() const noexcept { return colours; }

    /** accessibility 3: palettes ship as `Resources/Themes/*.json`, so one can
        be edited or added without a rebuild. */
    bool loadPaletteFromFile (const juce::File& file);

    static juce::File getThemeDirectory();

    /** Writes every built-in palette out as a theme file. */
    static bool writeBuiltInPalettes (const juce::File& directory);

    //==========================================================================
    void setUiScale (double scale);
    double getUiScale() const noexcept { return uiScale; }

    /** The next scale down, for the graceful recovery accessibility 4 asks for
        when a saved window size no longer fits. Returns false at the smallest. */
    bool stepScaleDown();

    /** Scales a size, rounded to a whole pixel. */
    int scaled (int size) const noexcept
    {
        return juce::jmax (1, juce::roundToInt ((double) size * uiScale));
    }

    /** Scales a font size, with a floor so text never becomes unreadable
        (accessibility 4). */
    float scaledFont (float points) const noexcept
    {
        return juce::jmax (9.0f, (float) ((double) points * uiScale));
    }

    //==========================================================================
    void setReducedMotion (bool shouldReduce);
    bool isReducedMotion() const noexcept { return reducedMotion; }

    /** The animation length to use, which is zero under reduced motion
        (accessibility 5). */
    int getAnimationMs (int normalMs) const noexcept
    {
        return reducedMotion ? 0 : normalMs;
    }

    //==========================================================================
    void setVerbosity (Verbosity v);
    Verbosity getVerbosity() const noexcept { return verbosity; }

    /** accessibility 4 and 8: the theme's font is a preference, not a lock. */
    void setFontOverride (const juce::String& fontName);
    juce::String getFontOverride() const { return fontOverride; }

    /** The font stack to use, honouring the override and the locale's script
        needs (accessibility 8). */
    juce::Font getFont (float points, bool monospaced = false) const;

    //==========================================================================
    // Shortcuts (accessibility.md 2).

    const std::vector<ShortcutBinding>& getShortcuts() const noexcept { return shortcuts; }

    /** Rebinds an action. Returns false if the key is already taken by another,
        which the table shows rather than silently overwriting. */
    bool rebind (const juce::String& actionId, const juce::KeyPress& key);

    void resetShortcut (const juce::String& actionId);
    void resetAllShortcuts();

    /** The action a key press triggers, or an empty string. */
    juce::String findAction (const juce::KeyPress& key) const;

    const ShortcutBinding* findShortcut (const juce::String& actionId) const;

    /** Every shortcut as printable text, for the "show all shortcuts" overlay
        and for the manual (accessibility 2 and 7). */
    juce::StringArray getPrintableShortcuts() const;

    //==========================================================================
    /** These are user-global rather than per-preset: they describe the person,
        not the sound. */
    juce::var toVar() const;
    void fromVar (const juce::var& state);

    bool save() const;
    bool load();

    static juce::File getConfigFile();

    /** A built-in palette, ignoring any theme file that overrides it. */
    static PaletteColours buildPalette (PaletteId id);

private:
    AccessibilitySettings();

    void buildDefaultShortcuts();

    PaletteId palette = PaletteId::defaultDark;
    PaletteColours colours;

    double uiScale = 1.0;
    bool reducedMotion = false;
    Verbosity verbosity = Verbosity::standard;
    juce::String fontOverride;

    std::vector<ShortcutBinding> shortcuts;
};

//==============================================================================
/** Helpers that set up a component's accessibility handler the way
    accessibility.md 1 requires: a label, a role and a value description.

    Every control in the plugin goes through one of these, so that adding a
    control cannot silently add an unlabelled one. */
namespace AccessibleSetup
{
    /** A slider or knob: role, label, and a value description in real units. */
    void configureSlider (juce::Slider& slider, const juce::String& label,
                          const juce::String& unitSuffix = {});

    void configureButton (juce::Button& button, const juce::String& label,
                          const juce::String& description = {});

    void configureComboBox (juce::ComboBox& box, const juce::String& label);

    /** A meter, whose accessible value is its level in dBFS (accessibility 1). */
    void configureMeter (juce::Component& meter, const juce::String& label,
                         std::function<double()> levelDbProvider);

    /** A component that is not itself interactive but needs a description, like
        the fretboard's individual frets. */
    void configureDescriptive (juce::Component& component, const juce::String& label,
                               const juce::String& description);

    /** accessibility 1: an overlay announces itself and moves focus to its first
        interactive child. Call it once the overlay is showing (OverlayHost::show,
        after addAndMakeVisible): focus can only land on a component with a peer. */
    void announceOverlayOpened (juce::Component& overlay, const juce::String& name);

    /** The first interactive element inside `root`, in Tab order - the one
        announceOverlayOpened gives focus to - or null when nothing in it wants
        focus. Walks the whole tree, not only the direct children, because an
        overlay's controls sit inside its pages. */
    juce::Component* findFirstInteractive (juce::Component& root);
}

} // namespace luthier
