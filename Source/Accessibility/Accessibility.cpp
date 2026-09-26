#include "Accessibility.h"
#include "Localisation.h"

namespace luthier
{

const std::array<double, AccessibilitySettings::kNumScales> AccessibilitySettings::kScales =
    { 0.75, 1.0, 1.25, 1.5, 1.75, 2.0 };

//==============================================================================
const char* getPaletteName (PaletteId id) noexcept
{
    switch (id)
    {
        case PaletteId::defaultDark:  return "Default";
        case PaletteId::deuteranopia: return "Deuteranopia-safe";
        case PaletteId::protanopia:   return "Protanopia-safe";
        case PaletteId::tritanopia:   return "Tritanopia-safe";
        case PaletteId::highContrast: return "High contrast";
        case PaletteId::light:        return "Light";
        case PaletteId::numPalettes:
        default:                      return "Default";
    }
}

const char* AccessibilitySettings::getVerbosityName (Verbosity v) noexcept
{
    switch (v)
    {
        case Verbosity::minimal:  return "Minimal";
        case Verbosity::standard: return "Standard";
        case Verbosity::verbose:  return "Verbose";
        case Verbosity::numLevels:
        default:                  return "Standard";
    }
}

//==============================================================================
double PaletteColours::contrastRatio (juce::Colour a, juce::Colour b) noexcept
{
    /*  WCAG relative luminance.

        Not the same as perceived brightness: each channel is linearised out of
        sRGB's gamma first, then weighted by the eye's sensitivity to it. Using
        JUCE's getBrightness() here would give numbers that look plausible and
        fail a real audit.
    */
    auto luminance = [] (juce::Colour colour)
    {
        auto channel = [] (double value)
        {
            return (value <= 0.03928) ? (value / 12.92)
                                      : std::pow ((value + 0.055) / 1.055, 2.4);
        };

        const double r = channel ((double) colour.getFloatRed());
        const double g = channel ((double) colour.getFloatGreen());
        const double bl = channel ((double) colour.getFloatBlue());

        return 0.2126 * r + 0.7152 * g + 0.0722 * bl;
    };

    const double la = luminance (a);
    const double lb = luminance (b);

    const double lighter = juce::jmax (la, lb);
    const double darker = juce::jmin (la, lb);

    return (lighter + 0.05) / (darker + 0.05);
}

double PaletteColours::getWorstTextContrast() const noexcept
{
    // The pairs a user actually has to read. Muted and disabled text are
    // deliberately excluded: they are secondary information by design, and
    // holding them to body-text contrast would flatten the whole hierarchy.
    const double pairs[] =
    {
        contrastRatio (textPrimary, background),
        contrastRatio (textPrimary, panel),
        contrastRatio (textPrimary, panelRaised),
        contrastRatio (textPrimary, panelSunken),
        contrastRatio (textPrimary, backgroundDeep)
    };

    double worst = 21.0;

    for (double ratio : pairs)
        worst = juce::jmin (worst, ratio);

    return worst;
}

//==============================================================================
juce::var PaletteColours::toVar() const
{
    auto* object = new juce::DynamicObject();

    auto put = [object] (const char* name, juce::Colour colour)
    {
        object->setProperty (name, colour.toDisplayString (true));
    };

    put ("backgroundDeep", backgroundDeep);
    put ("background",     background);
    put ("panel",          panel);
    put ("panelRaised",    panelRaised);
    put ("panelSunken",    panelSunken);
    put ("edge",           edge);
    put ("edgeBright",     edgeBright);
    put ("accent",         accent);
    put ("accentBright",   accentBright);
    put ("accentDim",      accentDim);
    put ("secondary",      secondary);
    put ("secondaryDim",   secondaryDim);
    put ("textPrimary",    textPrimary);
    put ("textMuted",      textMuted);
    put ("textDisabled",   textDisabled);
    put ("success",        success);
    put ("warning",        warning);
    put ("clip",           clip);
    put ("dataStream",     dataStream);
    put ("shadow",         shadow);

    return { object };
}

PaletteColours PaletteColours::fromVar (const juce::var& state)
{
    PaletteColours palette;

    auto* object = state.getDynamicObject();

    if (object == nullptr)
        return palette;

    auto read = [object] (const char* name, juce::Colour& destination)
    {
        if (object->hasProperty (name))
            destination = juce::Colour::fromString (object->getProperty (name).toString());
    };

    read ("backgroundDeep", palette.backgroundDeep);
    read ("background",     palette.background);
    read ("panel",          palette.panel);
    read ("panelRaised",    palette.panelRaised);
    read ("panelSunken",    palette.panelSunken);
    read ("edge",           palette.edge);
    read ("edgeBright",     palette.edgeBright);
    read ("accent",         palette.accent);
    read ("accentBright",   palette.accentBright);
    read ("accentDim",      palette.accentDim);
    read ("secondary",      palette.secondary);
    read ("secondaryDim",   palette.secondaryDim);
    read ("textPrimary",    palette.textPrimary);
    read ("textMuted",      palette.textMuted);
    read ("textDisabled",   palette.textDisabled);
    read ("success",        palette.success);
    read ("warning",        palette.warning);
    read ("clip",           palette.clip);
    read ("dataStream",     palette.dataStream);
    read ("shadow",         palette.shadow);

    return palette;
}

bool PaletteColours::loadFrom (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());

    if (parsed.getDynamicObject() == nullptr)
        return false;

    *this = fromVar (parsed);
    return true;
}

bool PaletteColours::saveTo (const juce::File& file) const
{
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (juce::JSON::toString (toVar(), true));
}

//==============================================================================
AccessibilitySettings& AccessibilitySettings::get()
{
    static AccessibilitySettings instance;
    return instance;
}

AccessibilitySettings::AccessibilitySettings()
{
    colours = buildPalette (PaletteId::defaultDark);
    baseColours = colours;
    buildDefaultShortcuts();
}

//==============================================================================
PaletteColours AccessibilitySettings::buildPalette (PaletteId id)
{
    PaletteColours palette;

    switch (id)
    {
        case PaletteId::defaultDark:
            // The guitar-shop palette, which is the struct's defaults.
            break;

        case PaletteId::deuteranopia:
            /*  Green-blind.

                The problem to solve is that the default's green "success" and
                amber "warning" are the same colour to a deuteranope. The success
                indicator moves to a blue that no red-green deficiency confuses
                with amber, and the meter gradient runs blue-teal-orange-red so
                that every step along it differs in more than hue.
            */
            palette.success     = juce::Colour (0xff4f9ee8);
            palette.warning     = juce::Colour (0xffe8b33f);
            palette.clip        = juce::Colour (0xffe85c3f);
            palette.secondary   = juce::Colour (0xff5fb6c9);
            palette.secondaryDim = juce::Colour (0xff36707d);
            palette.dataStream  = juce::Colour (0xff6fb6e8);
            break;

        case PaletteId::protanopia:
            // Red-blind: the clip indicator becomes magenta, which a protanope
            // sees as clearly distinct from the amber warning.
            palette.success     = juce::Colour (0xff4f9ee8);
            palette.warning     = juce::Colour (0xffe8c44f);
            palette.clip        = juce::Colour (0xffe84fc4);
            palette.accent      = juce::Colour (0xffd9a441);
            palette.accentBright = juce::Colour (0xfff2c368);
            palette.accentDim   = juce::Colour (0xff8a6a29);
            palette.dataStream  = juce::Colour (0xff6fb6e8);
            break;

        case PaletteId::tritanopia:
            // Blue-blind: the accents move away from teal, which a tritanope
            // confuses with grey.
            palette.secondary   = juce::Colour (0xffc96f9e);
            palette.secondaryDim = juce::Colour (0xff7d3f5f);
            palette.success     = juce::Colour (0xff6fc97a);
            palette.warning     = juce::Colour (0xffe07f3c);
            palette.clip        = juce::Colour (0xffd9452f);
            palette.dataStream  = juce::Colour (0xff8fd18a);
            break;

        case PaletteId::highContrast:
            /*  Pure black, white text, one accent, no gradients.

                Every surface is black so that text contrast is the same
                everywhere: a panel that is slightly lighter than the background
                would give body text a different ratio depending on which panel
                it sat on, and the worst of those is what an audit measures.
            */
            palette.backgroundDeep = juce::Colours::black;
            palette.background     = juce::Colours::black;
            palette.panel          = juce::Colours::black;
            palette.panelRaised    = juce::Colours::black;
            palette.panelSunken    = juce::Colours::black;
            palette.edge           = juce::Colour (0xff808080);
            palette.edgeBright     = juce::Colours::white;
            palette.accent         = juce::Colour (0xffffd400);
            palette.accentBright   = juce::Colour (0xffffe866);
            palette.accentDim      = juce::Colour (0xff806a00);
            palette.secondary      = juce::Colour (0xff00d4ff);
            palette.secondaryDim   = juce::Colour (0xff00697f);
            palette.textPrimary    = juce::Colours::white;
            palette.textMuted      = juce::Colour (0xffcccccc);
            palette.textDisabled   = juce::Colour (0xff999999);
            palette.success        = juce::Colour (0xff00ff66);
            palette.warning        = juce::Colour (0xffffd400);
            palette.clip           = juce::Colour (0xffff3333);
            palette.dataStream     = juce::Colour (0xff00ff66);
            palette.shadow         = juce::Colour (0x00000000);
            break;

        case PaletteId::light:
            // Maple and cream: a blonde guitar and a tweed amp (visual-polish.md 6.1).
            palette.backgroundDeep = juce::Colour (0xffe6d6b4);
            palette.background     = juce::Colour (0xffefe3c8);
            palette.panel          = juce::Colour (0xfff6edda);
            palette.panelRaised    = juce::Colour (0xfffbf5e8);
            palette.panelSunken    = juce::Colour (0xffe4d4b3);
            palette.edge           = juce::Colour (0xffc2a878);
            palette.edgeBright     = juce::Colour (0xffa88a58);
            palette.accent         = juce::Colour (0xff7a3a0c);
            palette.accentBright   = juce::Colour (0xff9a5418);
            palette.accentDim      = juce::Colour (0xffb98c52);
            palette.secondary      = juce::Colour (0xff2a5c48);
            palette.secondaryDim   = juce::Colour (0xff7fa28f);
            palette.textPrimary    = juce::Colour (0xff2a1e14);
            palette.textMuted      = juce::Colour (0xff5e4a33);
            palette.textDisabled   = juce::Colour (0xff9a8466);
            palette.success        = juce::Colour (0xff1f5a17);
            palette.warning        = juce::Colour (0xff8a3e0e);
            palette.clip           = juce::Colour (0xffa32213);
            palette.dataStream     = juce::Colour (0xff2f6b3a);
            palette.shadow         = juce::Colour (0x33000000);
            break;

        case PaletteId::numPalettes:
        default:
            break;
    }

    return palette;
}

void AccessibilitySettings::setPalette (PaletteId id)
{
    palette = (PaletteId) juce::jlimit (0, (int) PaletteId::numPalettes - 1, (int) id);

    // A theme file of the same name overrides the built-in one, so a user can
    // edit a palette without a rebuild (accessibility 3).
    const auto file = getThemeDirectory()
                        .getChildFile (juce::String (getPaletteName (palette)) + ".json");

    if (! colours.loadFrom (file))
        colours = buildPalette (palette);

    baseColours = colours;
    applyAccent();
    sendChangeMessage();
}

bool AccessibilitySettings::loadPaletteFromFile (const juce::File& file)
{
    if (! colours.loadFrom (file))
        return false;

    baseColours = colours;
    applyAccent();
    sendChangeMessage();
    return true;
}

//==============================================================================
juce::StringArray AccessibilitySettings::getAccentNames()
{
    return { "Aged brass", "Tube amber", "Jewel red", "Seafoam green", "Sonic blue", "Pearl ivory" };
}

double AccessibilitySettings::accentContrast (juce::Colour c, const PaletteColours& p) noexcept
{
    double worst = 21.0;

    for (auto bg : { p.background, p.panel, p.panelRaised, p.panelSunken })
        worst = juce::jmin (worst, PaletteColours::contrastRatio (c, bg));

    return worst;
}

juce::Colour AccessibilitySettings::accentFor (int choice, const PaletteColours& p, juce::Colour guitarFinish)
{
    static const juce::uint32 bases[kNumAccents] = { 0, 0xffe8913a, 0xffd2574a, 0xff5fb39a, 0xff6ca6d9, 0xffdcd0b4 };

    juce::Colour c = choice == kFollowGuitar ? guitarFinish.withAlpha (1.0f)
                   : juce::isPositiveAndBelow (choice, kNumAccents) && choice > 0 ? juce::Colour (bases[choice])
                                                                                  : p.accent;

    // Toward the text colour until it reads on every background: lighter on a
    // dark palette, darker on the Light one (visual-polish.md 5, accessibility 10).
    for (int i = 0; i < 40 && accentContrast (c, p) < 4.5; ++i)
        c = c.interpolatedWith (p.textPrimary, 0.1f);

    return c;
}

void AccessibilitySettings::applyAccent()
{
    colours = baseColours;

    if (accentChoice == 0)
        return;

    const auto a = accentFor (accentChoice, baseColours, guitarAccent);
    colours.accent = a;
    colours.accentBright = a.interpolatedWith (baseColours.textPrimary, 0.3f);
    colours.accentDim = a.interpolatedWith (baseColours.background, 0.45f);
}

void AccessibilitySettings::setAccent (int choice)
{
    choice = choice == kFollowGuitar ? kFollowGuitar : juce::jlimit (0, kNumAccents - 1, choice);

    if (choice == accentChoice)
        return;

    accentChoice = choice;
    applyAccent();
    sendChangeMessage();
}

void AccessibilitySettings::setGuitarAccentSource (juce::Colour finish)
{
    if (finish == guitarAccent)
        return;

    guitarAccent = finish;

    if (accentChoice == kFollowGuitar)
    {
        applyAccent();
        sendChangeMessage();
    }
}

juce::File AccessibilitySettings::getThemeDirectory()
{
    return juce::File::getSpecialLocation (juce::File::currentApplicationFile)
             .getParentDirectory()
             .getChildFile ("Resources")
             .getChildFile ("Themes");
}

bool AccessibilitySettings::writeBuiltInPalettes (const juce::File& directory)
{
    directory.createDirectory();

    bool wroteAll = true;

    for (int i = 0; i < (int) PaletteId::numPalettes; ++i)
    {
        const auto id = (PaletteId) i;
        const auto file = directory.getChildFile (juce::String (getPaletteName (id)) + ".json");

        if (! buildPalette (id).saveTo (file))
            wroteAll = false;
    }

    return wroteAll;
}

//==============================================================================
void AccessibilitySettings::setUiScale (double scale)
{
    // Snap to the offered steps, so a scale the layout was never checked at
    // cannot be set by a stray value in a config file.
    double nearest = kScales[1];
    double bestDistance = 1.0e9;

    for (double candidate : kScales)
    {
        const double distance = std::abs (candidate - scale);

        if (distance < bestDistance)
        {
            bestDistance = distance;
            nearest = candidate;
        }
    }

    if (uiScale == nearest)
        return;

    uiScale = nearest;
    sendChangeMessage();
}

bool AccessibilitySettings::stepScaleDown()
{
    for (int i = 1; i < kNumScales; ++i)
    {
        if (std::abs (kScales[(size_t) i] - uiScale) < 1.0e-6)
        {
            setUiScale (kScales[(size_t) (i - 1)]);
            return true;
        }
    }

    return false;
}

void AccessibilitySettings::setReducedMotion (bool shouldReduce)
{
    if (reducedMotion == shouldReduce)
        return;

    reducedMotion = shouldReduce;
    sendChangeMessage();
}

void AccessibilitySettings::setVerbosity (Verbosity v)
{
    verbosity = (Verbosity) juce::jlimit (0, (int) Verbosity::numLevels - 1, (int) v);
    sendChangeMessage();
}

void AccessibilitySettings::setFontOverride (const juce::String& fontName)
{
    fontOverride = fontName;
    sendChangeMessage();
}

juce::Font AccessibilitySettings::getFont (float points, bool monospaced) const
{
    const float size = scaledFont (points);

    if (fontOverride.isNotEmpty())
        return juce::Font (juce::FontOptions (fontOverride, size, juce::Font::plain));

    /*  accessibility 8: complex scripts need a fallback stack.

        A CJK locale rendered in a Latin display face gives a box for every
        glyph. Naming the platform's own CJK face first is the only thing that
        reliably avoids it, and it is per-platform because the faces are.
    */
    if (Localisation::get().needsCjkFallbackFont())
    {
       #if JUCE_WINDOWS
        const juce::String cjk ("Yu Gothic UI");
       #elif JUCE_MAC
        const juce::String cjk ("Hiragino Sans");
       #else
        const juce::String cjk ("Noto Sans CJK JP");
       #endif

        return juce::Font (juce::FontOptions (cjk, size, juce::Font::plain));
    }

    if (monospaced)
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(),
                                              size, juce::Font::plain));

    return juce::Font (juce::FontOptions (size));
}

//==============================================================================
void AccessibilitySettings::buildDefaultShortcuts()
{
    shortcuts.clear();

    auto add = [this] (const char* id, const char* descriptionKey, juce::KeyPress key)
    {
        ShortcutBinding binding;
        binding.id = id;
        binding.descriptionKey = descriptionKey;
        binding.key = key;
        binding.defaultKey = key;

        shortcuts.push_back (binding);
    };

    using KP = juce::KeyPress;

    /*  These are gui-integration.md section 17's canonical defaults.

        That table is the single source of truth for what a key does, and this
        registry is the single place the editor reads it from - PluginEditor asks
        here rather than comparing key codes itself, which is what makes section
        17's "all rebindable" true rather than decorative.

        Where an action's feature does not exist yet the binding is simply absent,
        rather than present and dead: Workshop (W), Save As Guitar
        (Ctrl+G) and New Tune (Ctrl+T) all wait on specs that are not written.
        GAPS.md tracks them. The Column 4 tab steps used to be on that list and
        are not any more - the Advanced workspace has a tab strip now, so there
        is something for them to step.
    */

    const auto cmd   = juce::ModifierKeys::commandModifier;
    const auto shift = juce::ModifierKeys::shiftModifier;
    const auto alt   = juce::ModifierKeys::altModifier;

    add ("help",             "accessibility.shortcut.help",             KP (KP::F1Key));
    add ("showShortcuts",    "accessibility.shortcut.showShortcuts",    KP ('/', cmd | shift, 0));

    add ("toggleAdvanced",   "accessibility.shortcut.toggleAdvanced",   KP (KP::tabKey));
    add ("toggleLiveMode",   "accessibility.shortcut.toggleLiveMode",   KP ('l', 0, 0));
    add ("toggleSlideMode",  "accessibility.shortcut.toggleSlideMode",  KP ('s', 0, 0));
    add ("toggleWorkshop",   "accessibility.shortcut.toggleWorkshop",   KP ('w', 0, 0));   // gui-integration 17 (VISUAL-WORKSHOP-QA)
    add ("togglePractice",   "accessibility.shortcut.togglePractice",   KP ('d', 0, 0));
    add ("toggleAssist",     "accessibility.shortcut.toggleAssist",     KP ('a', 0, 0));   // auto-articulation.md 7.5

    // output-normalization.md 9: rebindable, unbound by default.
    add ("toggleNormalization", "accessibility.shortcut.toggleNormalization", KP());

    add ("panic",            "accessibility.shortcut.panic",            KP ('p', 0, 0));
    add ("tapTempo",         "accessibility.shortcut.tapTempo",         KP ('t', 0, 0));
    add ("killSwitch",       "accessibility.shortcut.killSwitch",       KP ('\\', 0, 0));

    // FEAT-JAM (jam-mode 8.2): the band - start/stop, fill, arm.
    add ("jamStartStop",     "accessibility.shortcut.jamStartStop",     KP ('j', 0, 0));
    add ("jamFill",          "accessibility.shortcut.jamFill",          KP ('j', shift, 0));
    add ("jamArm",           "accessibility.shortcut.jamArm",           KP ('j', alt, 0));

    add ("previousItem",     "accessibility.shortcut.previousItem",     KP ('[', 0, 0));
    add ("nextItem",         "accessibility.shortcut.nextItem",         KP (']', 0, 0));

    /*  Section 17's Column 4 tab steps. The modified pair sits deliberately beside
        the unmodified one above: unmodified steps what the whole plugin is playing,
        modified steps which workspace tab is looking at it. */
    add ("previousWorkspaceTab", "accessibility.shortcut.previousWorkspaceTab", KP ('[', cmd, 0));
    add ("nextWorkspaceTab",     "accessibility.shortcut.nextWorkspaceTab",     KP (']', cmd, 0));

    add ("setlistPrevious",  "accessibility.shortcut.setlistPrevious",  KP (KP::pageUpKey));
    add ("setlistNext",      "accessibility.shortcut.setlistNext",      KP (KP::pageDownKey));

    add ("undo",             "accessibility.shortcut.undo",             KP ('z', cmd, 0));
    add ("redo",             "accessibility.shortcut.redo",             KP ('z', cmd | shift, 0));
    add ("undoAcrossBoundary", "accessibility.shortcut.undoAcrossBoundary", KP ('z', cmd | alt, 0));   // action-and-undo.md 9
   #if ! JUCE_MAC
    add ("redoAlt",          "accessibility.shortcut.redoAlt",          KP ('y', cmd, 0));   // action-and-undo.md 9: Ctrl-Y
   #endif

    add ("save",             "accessibility.shortcut.save",             KP ('s', cmd, 0));
    add ("saveAs",           "accessibility.shortcut.saveAs",           KP ('s', cmd | shift, 0));

    /*  Section 17's "New preset". It loads the Init factory preset, which is what
        a new preset means here. GAPS.md A5 said no such action existed and that
        it "needs an init-preset concept first" - Init has been in the factory set
        the whole time, described in its own blurb as the place to start when
        building your own. It is not Reset All: that clears the session, this
        loads a preset, and the difference shows in the preset name afterwards. */
    add ("newPreset",        "accessibility.shortcut.newPreset",        KP ('n', cmd, 0));

    add ("presetBrowser",    "accessibility.shortcut.presetBrowser",    KP ('o', cmd, 0));

    /*  Section 17's "Reveal preset file", which needed somewhere to reveal.
        PresetManager::getCurrentPresetFile is that. */
    add ("revealPreset",     "accessibility.shortcut.revealPreset",     KP ('e', cmd | alt, 0));

    // guitar-workshop.md 6 and section 17's guitar-file pair.
    add ("saveGuitarAs",     "accessibility.shortcut.saveGuitarAs",     KP ('g', cmd, 0));
    add ("revealGuitar",     "accessibility.shortcut.revealGuitar",     KP ('e', cmd | shift, 0));

    add ("abCompare",        "accessibility.shortcut.abCompare",        KP ('/', cmd, 0));

    add ("randomise",        "accessibility.shortcut.randomise",        KP ('r', cmd, 0));
    add ("resetAll",         "accessibility.shortcut.resetAll",         KP ('r', cmd | shift, 0));

    add ("midiLearnArm",     "accessibility.shortcut.midiLearnArm",     KP ('l', cmd, 0));

    add ("export",           "accessibility.shortcut.export",           KP ('e', cmd, 0));

    // Section 17's "New tune" (tune-builder 2; TUNE-HELP-ONBOARDING).
    add ("newTune",          "accessibility.shortcut.newTune",          KP ('t', cmd, 0));
    add ("options",          "accessibility.shortcut.options",          KP (',', cmd, 0));

    /*  Not in section 17, kept because the debug panel is otherwise only reachable
        through Help and a diagnostics session is exactly when a user cannot
        navigate. Ctrl+D is free: section 17's D is unmodified. */
    add ("debugPanel",       "accessibility.shortcut.debugPanel",       KP ('d', cmd, 0));

    /*  Space auditions. Section 17 gives Space to the tune transport, which does
        not exist yet; when tune-builder lands, that binding takes it and audition
        moves. */
    add ("audition",         "accessibility.shortcut.audition",         KP (KP::spaceKey));

    // animated-strings.md 8: rebindable, unbound by default.
    add ("toggleStringAnimation", "accessibility.shortcut.toggleStringAnimation", KP());
    // cpu-quality-modes 5: rebindable, unbound by default.
    add ("cycleCpuQuality",  "quality.shortcut.cycle",                  KP());
    // global-search.md 6.1 (FEAT-SEARCH): Ctrl/Cmd+K opens the search palette.
    add ("search",           "accessibility.shortcut.search",           KP ('k', cmd, 0));
}

bool AccessibilitySettings::rebind (const juce::String& actionId, const juce::KeyPress& key)
{
    // A key already bound elsewhere is refused rather than silently stolen: the
    // rebind table shows the clash, which is what accessibility 2's "full rebind
    // table" needs to be usable.
    for (const auto& binding : shortcuts)
        if (binding.id != actionId && key.isValid() && binding.key == key)   // unbound never clashes
            return false;

    for (auto& binding : shortcuts)
    {
        if (binding.id == actionId)
        {
            binding.key = key;
            sendChangeMessage();
            return true;
        }
    }

    return false;
}

void AccessibilitySettings::resetShortcut (const juce::String& actionId)
{
    for (auto& binding : shortcuts)
    {
        if (binding.id == actionId)
        {
            binding.key = binding.defaultKey;
            sendChangeMessage();
            return;
        }
    }
}

void AccessibilitySettings::resetAllShortcuts()
{
    for (auto& binding : shortcuts)
        binding.key = binding.defaultKey;

    sendChangeMessage();
}

juce::String AccessibilitySettings::findAction (const juce::KeyPress& key) const
{
    for (const auto& binding : shortcuts)
        if (binding.key.isValid() && binding.key == key)
            return binding.id;

    return {};
}

const ShortcutBinding* AccessibilitySettings::findShortcut (const juce::String& actionId) const
{
    for (const auto& binding : shortcuts)
        if (binding.id == actionId)
            return &binding;

    return nullptr;
}

juce::StringArray AccessibilitySettings::getPrintableShortcuts() const
{
    juce::StringArray lines;

    for (const auto& binding : shortcuts)
        lines.add (binding.key.getTextDescription().paddedRight (' ', 20)
                     + tr (binding.descriptionKey));

    return lines;
}

//==============================================================================
juce::var AccessibilitySettings::toVar() const
{
    auto* root = new juce::DynamicObject();

    root->setProperty ("palette", (int) palette);
    root->setProperty ("accent", accentChoice);   // visual-polish.md 5
    root->setProperty ("uiScale", uiScale);
    root->setProperty ("reducedMotion", reducedMotion);
    root->setProperty ("verbosity", (int) verbosity);
    root->setProperty ("fontOverride", fontOverride);
    root->setProperty ("locale", Localisation::get().getLocale());
    root->setProperty ("fallbackLocale", Localisation::get().getFallbackLocale());

    // Only the rebound ones, so a default that changes in a later version is
    // picked up rather than frozen by an old config file.
    auto* bindings = new juce::DynamicObject();

    for (const auto& binding : shortcuts)
        if (binding.isRebound())
            bindings->setProperty (binding.id, binding.key.getTextDescription());

    root->setProperty ("shortcuts", juce::var (bindings));

    return { root };
}

void AccessibilitySettings::fromVar (const juce::var& state)
{
    auto* root = state.getDynamicObject();

    if (root == nullptr)
        return;

    setPalette ((PaletteId) juce::jlimit (0, (int) PaletteId::numPalettes - 1,
                                          (int) root->getProperty ("palette")));

    setAccent (root->hasProperty ("accent") ? (int) root->getProperty ("accent") : 0);
    setUiScale (root->hasProperty ("uiScale") ? (double) root->getProperty ("uiScale") : 1.0);
    setReducedMotion ((bool) root->getProperty ("reducedMotion"));

    setVerbosity ((Verbosity) juce::jlimit (0, (int) Verbosity::numLevels - 1,
                                            (int) root->getProperty ("verbosity")));

    setFontOverride (root->getProperty ("fontOverride").toString());

    if (root->hasProperty ("fallbackLocale"))
        Localisation::get().setFallbackLocale (root->getProperty ("fallbackLocale").toString());

    if (root->hasProperty ("locale"))
        Localisation::get().setLocale (root->getProperty ("locale").toString());

    resetAllShortcuts();

    if (auto* bindings = root->getProperty ("shortcuts").getDynamicObject())
        for (const auto& property : bindings->getProperties())
            rebind (property.name.toString(),
                    juce::KeyPress::createFromDescription (property.value.toString()));
}

juce::File AccessibilitySettings::getConfigFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("config")
             .getChildFile ("accessibility.json");
}

bool AccessibilitySettings::save() const
{
    const auto file = getConfigFile();

    file.getParentDirectory().createDirectory();

    return file.replaceWithText (juce::JSON::toString (toVar(), true));
}

bool AccessibilitySettings::load()
{
    const auto file = getConfigFile();

    if (! file.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());

    if (parsed.getDynamicObject() == nullptr)
        return false;

    fromVar (parsed);
    return true;
}

//==============================================================================
namespace AccessibleSetup
{
    void configureSlider (juce::Slider& slider, const juce::String& label,
                          const juce::String& unitSuffix)
    {
        slider.setTitle (label);
        slider.setWantsKeyboardFocus (true);

        // accessibility 1: the accessible value is the value in real units, not
        // a normalised number. "Seventy-two percent" is useful; "0.72" is not.
        slider.textFromValueFunction = [unitSuffix] (double value)
        {
            return juce::String (value, 2) + unitSuffix;
        };

        slider.setHelpText (label);
    }

    void configureButton (juce::Button& button, const juce::String& label,
                          const juce::String& description)
    {
        button.setTitle (label);
        button.setWantsKeyboardFocus (true);
        button.setHelpText (description.isNotEmpty() ? description : label);
    }

    void configureComboBox (juce::ComboBox& box, const juce::String& label)
    {
        box.setTitle (label);
        box.setWantsKeyboardFocus (true);
        box.setHelpText (label);
    }

    void configureMeter (juce::Component& meter, const juce::String& label,
                         std::function<double()> levelDbProvider)
    {
        meter.setTitle (label);

        /*  accessibility 1: a meter reports its peak in dBFS.

            JUCE has no ready-made accessible value interface for a component
            that is not a Slider, so the level is put into the help text and
            refreshed by whoever owns the meter. A screen reader reads the help
            text on focus, which is what makes the meter readable at all. */
        if (levelDbProvider != nullptr)
            meter.setHelpText (tr ("a11y.meter.description",
                                   { { "db", juce::String (levelDbProvider(), 1) } }));
        else
            meter.setHelpText (label);
    }

    void configureDescriptive (juce::Component& component, const juce::String& label,
                               const juce::String& description)
    {
        component.setTitle (label);
        component.setHelpText (description);
    }

    void announceOverlayOpened (juce::Component& overlay, const juce::String& name)
    {
        overlay.setTitle (name);

        // accessibility 1: announce, then move focus to the first thing the user
        // can act on, so tabbing starts inside the dialog rather than behind it.
        const auto announcement = tr ("a11y.dialog.opened", { { "name", name } });

        juce::AccessibilityHandler::postAnnouncement (
            announcement, juce::AccessibilityHandler::AnnouncementPriority::high);

        for (auto* child : overlay.getChildren())
        {
            if (child != nullptr && child->isVisible() && child->getWantsKeyboardFocus())
            {
                child->grabKeyboardFocus();
                break;
            }
        }
    }
}

} // namespace luthier
