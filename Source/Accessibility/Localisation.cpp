#include "Localisation.h"

namespace luthier
{

//==============================================================================
Localisation& Localisation::get()
{
    static Localisation instance;
    return instance;
}

Localisation::Localisation()
{
    fallback = getBuiltInEnglish();
    current = fallback;
}

//==============================================================================
const std::vector<LocaleInfo>& Localisation::getShipLocales()
{
    // accessibility.md 6 names exactly these fifteen.
    static const std::vector<LocaleInfo> locales =
    {
        { "en",    "English",              "English",              false, false },
        { "en-GB", "English (UK)",         "English (UK)",         false, false },
        { "es",    "Spanish",              "Espanol",              false, false },
        { "es-419","Spanish (Latin America)", "Espanol (Latinoamerica)", false, false },
        { "fr",    "French",               "Francais",             false, false },
        { "de",    "German",               "Deutsch",              false, false },
        { "pt-BR", "Portuguese (Brazil)",  "Portugues (Brasil)",   false, false },
        { "ja",    "Japanese",             "Nihongo",              false, true  },
        { "zh-CN", "Chinese (Simplified)", "Zhongwen (Jianti)",    false, true  },
        { "zh-TW", "Chinese (Traditional)","Zhongwen (Fanti)",     false, true  },
        { "ko",    "Korean",               "Hangugeo",             false, true  },
        { "ru",    "Russian",              "Russkiy",              false, false },
        { "pl",    "Polish",               "Polski",               false, false },
        { "nl",    "Dutch",                "Nederlands",           false, false },
        { "it",    "Italian",              "Italiano",             false, false }
    };

    return locales;
}

const LocaleInfo* Localisation::findLocale (const juce::String& code)
{
    for (const auto& locale : getShipLocales())
        if (locale.code.equalsIgnoreCase (code))
            return &locale;

    return nullptr;
}

//==============================================================================
juce::File Localisation::getCatalogDirectory()
{
    return juce::File::getSpecialLocation (juce::File::currentApplicationFile)
             .getParentDirectory()
             .getChildFile ("Resources")
             .getChildFile ("i18n");
}

bool Localisation::loadCatalog (const juce::String& code,
                                std::map<juce::String, juce::String>& destination) const
{
    destination.clear();

    // English is compiled in, so it is always available even with no resources.
    if (code.equalsIgnoreCase ("en"))
    {
        destination = getBuiltInEnglish();
        return true;
    }

    const auto directory = customCatalogDirectory.isDirectory()
                             ? customCatalogDirectory
                             : getCatalogDirectory();

    const auto file = directory.getChildFile (code + ".json");

    if (! file.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());
    auto* object = parsed.getDynamicObject();

    if (object == nullptr)
        return false;

    for (const auto& property : object->getProperties())
        destination[property.name.toString()] = property.value.toString();

    return ! destination.empty();
}

bool Localisation::setLocale (const juce::String& code)
{
    // Load the fallback first, so a partial catalog still has somewhere to fall
    // back to rather than showing keys.
    if (! loadCatalog (fallbackLocale, fallback))
        fallback = getBuiltInEnglish();

    if (loadCatalog (code, current))
    {
        currentLocale = code;
        loggedMissingKeys.clear();
        return true;
    }

    // A locale with no catalog is not an error the user should see a broken UI
    // for: the fallback is used and the request is reported as unmet.
    current = fallback;
    currentLocale = fallbackLocale;

    return false;
}

void Localisation::setFallbackLocale (const juce::String& code)
{
    fallbackLocale = code.isNotEmpty() ? code : "en";

    if (! loadCatalog (fallbackLocale, fallback))
        fallback = getBuiltInEnglish();
}

void Localisation::setCustomCatalogDirectory (const juce::File& directory)
{
    customCatalogDirectory = directory;

    // Re-read, so the change takes effect without a locale switch.
    setLocale (currentLocale);
}

//==============================================================================
juce::String Localisation::substitute (const juce::String& text,
                                       const std::map<juce::String, juce::String>& values)
{
    if (values.empty() || ! text.containsChar ('{'))
        return text;

    auto result = text;

    // Named, not positional: a translator may need "{n} presets loaded" in one
    // language and "Loaded: {n}" in another, and both have to work.
    for (const auto& [name, value] : values)
        result = result.replace ("{" + name + "}", value);

    return result;
}

juce::String Localisation::translate (const juce::String& key) const
{
    if (const auto entry = current.find (key); entry != current.end())
        return entry->second;

    if (const auto entry = fallback.find (key); entry != fallback.end())
        return entry->second;

    // accessibility 6: a key missing from English is a bug in the plugin rather
    // than in a translation, so it is logged - once - at debug level.
    if (! loggedMissingKeys.contains (key))
    {
        loggedMissingKeys.add (key);
        DBG ("Localisation: no string for key '" << key << "'");
    }

    return key;
}

juce::String Localisation::translate (const juce::String& key,
                                      const std::map<juce::String, juce::String>& values) const
{
    return substitute (translate (key), values);
}

bool Localisation::hasKey (const juce::String& key) const
{
    return current.find (key) != current.end() || fallback.find (key) != fallback.end();
}

//==============================================================================
juce::StringArray Localisation::getAllKeys() const
{
    juce::StringArray keys;

    for (const auto& [key, value] : getBuiltInEnglish())
    {
        juce::ignoreUnused (value);
        keys.add (key);
    }

    keys.sort (true);

    return keys;
}

juce::StringArray Localisation::getMissingKeys() const
{
    juce::StringArray missing;

    for (const auto& [key, value] : getBuiltInEnglish())
    {
        juce::ignoreUnused (value);

        if (current.find (key) == current.end())
            missing.add (key);
    }

    missing.sort (true);

    return missing;
}

bool Localisation::exportCatalog (const juce::File& file) const
{
    auto* object = new juce::DynamicObject();

    for (const auto& [key, value] : getBuiltInEnglish())
        object->setProperty (key, value);

    file.getParentDirectory().createDirectory();

    return file.replaceWithText (juce::JSON::toString (juce::var (object), true));
}

//==============================================================================
bool Localisation::isRightToLeft() const
{
    const auto* locale = findLocale (currentLocale);

    return locale != nullptr && locale->rightToLeft;
}

bool Localisation::needsCjkFallbackFont() const
{
    const auto* locale = findLocale (currentLocale);

    return locale != nullptr && locale->needsCjkFont;
}

//==============================================================================
const std::map<juce::String, juce::String>& Localisation::getBuiltInEnglish()
{
    /*  The English catalog.

        Compiled in rather than loaded from a file, so the plugin can always
        draw itself: a missing resource folder should cost the user their IRs,
        not their labels.

        Keys are dotted and stable. A key is never reused for a different
        meaning, because a translator works from the key's context as much as
        from its English text.
    */
    static const std::map<juce::String, juce::String> catalog =
    {
        // ---- application ------------------------------------------------------------
        { "app.name",                 "Luthier" },
        { "app.easy",                 "Easy" },
        { "app.advanced",             "Advanced" },
        { "app.live",                 "Live" },

        // ---- header -----------------------------------------------------------------
        { "header.file",              "File" },
        { "header.help",              "Help" },
        { "header.panic",             "Panic" },
        { "header.resetStop",         "RESET & STOP" },
        { "header.resetStop.title",   "Reset and stop" },
        { "header.resetStop.tooltip", "Stop everything - tune, loops, rhythm, effects - and return every setting to default (Ctrl+Shift+P)" },
        { "header.undo",              "Undo" },
        { "header.redo",              "Redo" },
        { "header.compareA",          "A" },
        { "header.compareB",          "B" },
        { "header.copyAtoB",          "Copy A to B" },
        { "header.preset.previous",   "Previous preset" },
        { "header.preset.next",       "Next preset" },

        // ---- macros -----------------------------------------------------------------
        { "macro.attack",             "Attack" },
        { "macro.body",               "Body" },
        { "macro.drive",              "Drive" },
        { "macro.tone",               "Tone" },
        { "macro.space",              "Space" },
        { "macro.humanize",           "Humanize" },

        // ---- easy mode ----------------------------------------------------------------
        { "easy.style",               "Style" },
        { "easy.mode",                "Mode" },
        { "easy.audition",            "Audition" },
        { "easy.stop",                "Stop" },
        { "easy.export",              "Export" },
        { "easy.randomise",           "Randomise" },
        { "easy.reset",               "Reset" },

        // ---- advanced columns -----------------------------------------------------------
        { "advanced.strings",         "Strings" },
        { "advanced.string",          "String" },
        { "advanced.body",            "Body" },
        { "advanced.pickups",         "Pickups" },
        { "advanced.rig",             "Rig" },
        { "advanced.amplifier",       "Amplifier" },
        { "advanced.cabinet",         "Cabinet and Mic" },
        { "advanced.room",            "Room" },
        { "advanced.effects",         "Effects" },
        { "advanced.master",          "Master" },
        { "advanced.humanise",        "Humanise" },
        { "advanced.playingHand",     "Playing Hand" },
        { "advanced.bridge",          "Bridge" },
        { "advanced.cable",           "Cable" },
        { "advanced.routing",         "Routing" },
        { "advanced.modMatrix",       "Mod Matrix" },
        { "advanced.rhythm",          "Rhythm" },
        { "advanced.toneMatch",       "Tone Match" },
        { "advanced.character",       "Character" },

        // ---- rhythm ---------------------------------------------------------------------
        { "rhythm.engine",            "Rhythm Engine" },
        { "rhythm.freeRun",           "Free-run" },
        { "rhythm.genreKit",          "Genre Kit" },
        { "rhythm.voicing",           "Voicing" },
        { "rhythm.density",           "Density" },
        { "rhythm.handPosition",      "Hand position" },
        { "rhythm.capo",              "Capo" },
        { "rhythm.capo.off",          "Capo: off" },
        { "rhythm.capo.at",           "Capo: fret {fret}" },
        { "rhythm.strumPattern",      "Strum Pattern" },
        { "rhythm.fingerpickPattern", "Fingerpick Pattern" },
        { "rhythm.feel",              "Feel" },
        { "rhythm.swing",             "Swing" },
        { "rhythm.patternBrowser",    "Pattern Browser" },
        { "rhythm.chord",             "Chord" },
        { "rhythm.next",              "Next" },
        { "rhythm.needsPoly",         "Switch to Poly mode to hear the rhythm engine." },

        // ---- advanced: the scrolling columns ------------------------------------------------
        { "advanced.scroll.moreAbove",      "More controls above - scroll with the mouse wheel, or click this arrow." },
        { "advanced.scroll.moreBelow",      "More controls below - scroll with the mouse wheel, or click this arrow." },
        { "advanced.scroll.moreAbove.name", "More controls above" },
        { "advanced.scroll.moreBelow.name", "More controls below" },
        { "advanced.strip.frets",           "FRETS" },
        { "advanced.strip.frets.tooltip",   "Show the fretboard in the strip." },
        { "advanced.strip.roll",            "ROLL" },
        { "advanced.strip.roll.tooltip",    "Show the string roll in the strip: what you play scrolling by, one lane per string. Click a lane to pluck it." },

        // ---- shared widgets -----------------------------------------------------------------
        { "widgets.knob.wheelHint",   "{tip}  Ctrl+wheel nudges the value; the wheel alone scrolls the column." },

        // ---- mod matrix ---------------------------------------------------------------------
        { "mod.sourceSelector.tooltip", "Which modulation source the card below edits. Every source is always running; this only chooses which one to look at." },
        { "mod.scope.tooltip",        "The source's output over the last few seconds, with its current value at the top right." },
        { "mod.shape.tooltip",        "The LFO's waveform." },
        { "mod.division.tooltip",     "The note length one cycle or one step lasts when synced to the host tempo." },
        { "mod.retrigger.tooltip",    "When the LFO restarts its cycle: never, on each note, when the transport starts, or locked to the bar grid." },
        { "mod.direction.tooltip",    "The order the sequencer walks its steps in." },
        { "mod.detection.tooltip",    "How the follower measures level: peak, RMS, or true peak." },
        { "mod.followerSource.tooltip", "Which signal the follower listens to." },
        { "mod.sync.tooltip",         "Lock the rate to the host tempo, using the division below." },
        { "mod.bipolar.tooltip",      "Swing both ways around the centre instead of only upward from it." },
        { "mod.rate",                 "Rate" },
        { "mod.rate.tooltip",         "How fast the LFO cycles, in hertz, when it is not synced. Ctrl+wheel nudges." },
        { "mod.depth",                "Depth" },
        { "mod.depth.tooltip",        "How far the LFO travels, before each route's own depth. Ctrl+wheel nudges." },
        { "mod.symmetry",             "Symmetry" },
        { "mod.symmetry.tooltip",     "Where the turning point falls in the cycle: 0.5 is even, lower is a fast rise and slow fall. Ctrl+wheel nudges." },
        { "mod.smoothing",            "Smooth" },
        { "mod.smoothing.tooltip",    "Rounds off the LFO's corners, in milliseconds, so a square or a step does not click. Ctrl+wheel nudges." },
        { "mod.delay",                "Delay" },
        { "mod.delay.tooltip",        "How long after the note the envelope waits before it starts. Ctrl+wheel nudges." },
        { "mod.attack",               "Attack" },
        { "mod.attack.tooltip",       "How long the envelope takes to reach its peak. Ctrl+wheel nudges." },
        { "mod.hold",                 "Hold" },
        { "mod.hold.tooltip",         "How long the envelope sits at its peak before decaying. Ctrl+wheel nudges." },
        { "mod.decay",                "Decay" },
        { "mod.decay.tooltip",        "How long the envelope takes to fall from its peak to the sustain level. Ctrl+wheel nudges." },
        { "mod.sustain",              "Sustain" },
        { "mod.sustain.tooltip",      "The level the envelope holds while the note is held. Ctrl+wheel nudges." },
        { "mod.release",              "Release" },
        { "mod.release.tooltip",      "How long the envelope takes to fall to nothing after the note ends. Ctrl+wheel nudges." },
        { "mod.length",               "Length" },
        { "mod.length.tooltip",       "How many steps the sequencer plays before it repeats. Ctrl+wheel nudges." },
        { "mod.swing",                "Swing" },
        { "mod.swing.tooltip",        "Delays every second step for a shuffled feel. Ctrl+wheel nudges." },
        { "mod.followerAttack",       "Attack" },
        { "mod.followerAttack.tooltip", "How quickly the follower rises when the signal gets louder. Ctrl+wheel nudges." },
        { "mod.followerRelease",      "Release" },
        { "mod.followerRelease.tooltip", "How quickly the follower falls when the signal gets quieter. Ctrl+wheel nudges." },
        { "mod.threshold",            "Threshold" },
        { "mod.threshold.tooltip",    "The level below which the follower outputs nothing. Ctrl+wheel nudges." },
        { "mod.table.tooltip",        "Every modulation route. Click Depth to type a value, Curve to cycle it, On to toggle the route, x to remove it." },
        { "mod.table.route.tooltip",  "This route's source and the control it moves." },
        { "mod.table.depth.tooltip",  "How far this route moves the control, from -1 to 1. Click to type a value." },
        { "mod.table.curve.tooltip",  "The shape between the source and the control. Click to cycle through the curves." },
        { "mod.table.enabled.tooltip", "Click to switch this route on or off without removing it." },
        { "mod.table.remove.tooltip", "Click to remove this route." },
        { "mod.emptyRoutes",          "No mod routes: drag a source onto any control, or right-click a control to modulate." },
        { "mod.routeCount",           "{n} route(s)" },
        { "mod.add.tooltip",          "Add a route from the source shown above to a control chosen from a menu." },
        { "mod.clear.tooltip",        "Remove every route. There is no undo for this." },
        { "mod.summary.tooltip",      "How many routes the matrix holds." },

        // ---- rhythm: tooltips -----------------------------------------------------------------
        { "rhythm.modeHint.tooltip",  "Why the rhythm engine is not playing right now." },
        { "rhythm.genreKit.tooltip",  "A style: picks a pattern, a voicing, a feel and a rig in one go. The dice randomises within it." },
        { "rhythm.rigHint.tooltip",   "What the chosen style set the rig to." },
        { "rhythm.voicing.tooltip",   "How the chord voicer lays chords on the neck: open, barre, jazz, and so on." },
        { "rhythm.density.tooltip",   "How many strings the voicer uses, from sparse to full. Ctrl+wheel nudges." },
        { "rhythm.handPosition.tooltip", "The fret the voicer keeps the hand near. Ctrl+wheel nudges." },
        { "rhythm.capo.tooltip",      "Where the capo sits. The arrows either side move it." },
        { "rhythm.tagFilter.tooltip", "Show only the patterns carrying this tag." },
        { "rhythm.patternList.tooltip", "The pattern library. Select one and press Load to play it." },

        // ---- live -----------------------------------------------------------------------
        { "live.snapshot",            "Snapshot {number}" },
        { "live.grid.name",           "Snapshot bank" },
        { "live.grid.tooltip",        "The snapshot bank: 128 pads. Click a pad to select it, double-click to recall it, Shift-click to store the current sound in it, right-click for more." },
        { "live.grid.emptyCell",      "empty" },
        { "live.grid.cell.empty",     "Snapshot {number} (empty). Click to select, Shift-click to store the current sound here." },
        { "live.grid.cell.filled",    "Snapshot {number}: {label}. Click to select, double-click to recall, right-click for more." },
        { "live.bank.emptyHint",      "Shift-click to save current state here." },
        { "live.menu.recall",         "Recall" },
        { "live.menu.captureHere",    "Capture here" },
        { "live.menu.captureOver",    "Capture over this snapshot" },
        { "live.colour",              "Colour {number}" },
        { "live.slot.selected",       "Slot {number}: {label}" },
        { "live.slot.selectedEmpty",  "Slot {number} - empty" },
        { "live.slotLabel.tooltip",   "The selected pad. The buttons below act on it." },
        { "live.setlist.tooltip",     "The order the set runs in. Select a row to remove it or move it." },
        { "live.setlist.empty",       "No setlist loaded. Add a snapshot to start one." },
        { "live.setlist.empty.tooltip", "Select a pad in the bank and press Add snapshot to start a setlist." },
        { "live.crossfade",           "Crossfade" },
        { "live.crossfade.tooltip",   "How long a recall takes to arrive. Zero is an instant switch. Ctrl+wheel nudges." },
        { "live.morphSlots.tooltip",  "Which two snapshots the morph runs between. The live strip's morph control sets the position." },
        { "live.snapshots",           "Snapshots" },
        { "live.capture",             "Capture" },
        { "live.rename",              "Rename" },
        { "live.clear",               "Clear" },
        { "live.colourTag",           "Colour tag" },
        { "live.setlist",             "Setlist" },
        { "live.setlist.open",        "Open a setlist file..." },
        { "live.setlist.previous",    "Previous entry" },
        { "live.setlist.next",        "Next entry" },
        { "live.tap",                 "Tap" },
        { "live.morph",               "Morph" },
        { "live.kill",                "Kill" },
        { "live.monitor",             "Monitor" },

        // ---- practice -------------------------------------------------------------------
        { "practice.metronome",       "Metronome" },
        { "practice.looper",          "Looper" },
        { "practice.track",           "Backing Track" },
        { "practice.scale",           "Scale Trainer" },
        { "practice.ear",             "Ear Training" },
        { "practice.tab",             "Tab Reader" },
        { "practice.progression",     "Progression" },
        { "practice.session",         "Session Recorder" },
        { "practice.tempo",           "Tempo" },
        { "practice.timeSignature",   "Time signature" },
        { "practice.accent",          "Accent" },
        { "practice.silentBars",      "Silent bars" },
        { "practice.record",          "Record" },
        { "practice.play",            "Play" },
        { "practice.stop",            "Stop" },
        { "practice.undo",            "Undo" },
        { "practice.layer",           "Layer {number}" },

        // ---- tone match -------------------------------------------------------------------
        { "tonematch.bodyIr",         "Body IR" },
        { "tonematch.cabIr1",         "Cabinet IR 1" },
        { "tonematch.cabIr2",         "Cabinet IR 2" },
        { "tonematch.load",           "Load..." },
        { "tonematch.clear",          "Clear" },
        { "tonematch.gainTrim",       "Gain trim" },
        { "tonematch.predelay",       "Predelay" },
        { "tonematch.reverse",        "Reverse" },
        { "tonematch.mix",            "Mix" },
        { "tonematch.cabMatch",       "Cab Match" },
        { "tonematch.eqMatch",        "EQ Match" },
        { "tonematch.capture",        "Capture" },
        { "tonematch.irMissing",      "The preset's IR is missing. The built-in model is being used." },

        // ---- character ---------------------------------------------------------------------
        { "character.seed",           "Character seed" },
        { "character.newCharacter",   "New Character" },
        { "character.deadSpots",      "Dead spots" },
        { "character.fretWear",       "Fret wear" },
        { "character.refret",         "Refret" },
        { "character.tunerLooseness", "Tuner looseness" },
        { "character.agedElectronics","Aged electronics" },
        { "character.bodyAge",        "Body age" },
        { "character.environment",    "Environment" },
        { "character.temperature",    "Temperature" },
        { "character.humidity",       "Humidity" },
        { "character.retune",         "Retune" },
        { "character.allFresh",       "All fresh" },
        { "character.allOld",         "All old" },

        // ---- accessibility -------------------------------------------------------------------
        { "accessibility.title",      "Accessibility" },
        { "accessibility.verbosity",  "Screen reader verbosity" },
        { "accessibility.shortcuts",  "Keyboard shortcuts" },
        { "accessibility.uiScale",    "UI scale" },
        { "accessibility.palette",    "Colour palette" },
        { "accessibility.reducedMotion", "Reduced motion" },
        { "accessibility.fontOverride", "Font" },
        { "accessibility.localisation", "Localisation" },
        { "accessibility.locale",     "Language" },
        { "accessibility.fallback",   "Fallback language" },
        { "accessibility.catalogPath","Custom string catalog" },

        /*  ---- shortcut descriptions -----------------------------------------------------
            One per binding in AccessibilitySettings::buildDefaultShortcuts, keyed by
            that binding's descriptionKey. These are the labels in the Options >
            ACCESSIBILITY rebind table (accessibility 2) and in getPrintableShortcuts.
            Without them the table rendered the key itself, so every row read
            "accessibility.shortcut.undo" rather than "Undo". A binding added to the
            registry needs its string added here, and shortcutsAllHaveDescriptions
            fails if one does not.
        */
        { "accessibility.shortcut.help",            "Open help" },
        { "accessibility.shortcut.showShortcuts",   "Show all shortcuts" },
        { "accessibility.shortcut.toggleAdvanced",  "Switch Easy / Advanced mode" },
        { "accessibility.shortcut.toggleLiveMode",  "Toggle Live Mode" },
        { "accessibility.shortcut.toggleSlideMode", "Toggle Slide Mode" },
        { "accessibility.shortcut.togglePractice",  "Open the practice drawer" },
        { "accessibility.shortcut.panic",           "Panic: silence all notes" },
        { "accessibility.shortcut.resetAndStop",    "Reset and stop: stop everything and return every setting to default" },
        { "accessibility.shortcut.tapTempo",        "Tap tempo" },
        { "accessibility.shortcut.killSwitch",      "Kill switch" },
        { "accessibility.shortcut.previousItem",    "Previous preset or snapshot" },
        { "accessibility.shortcut.nextItem",        "Next preset or snapshot" },
        { "accessibility.shortcut.previousWorkspaceTab", "Previous workspace tab" },
        { "accessibility.shortcut.nextWorkspaceTab", "Next workspace tab" },
        { "accessibility.shortcut.setlistPrevious", "Previous setlist entry" },
        { "accessibility.shortcut.setlistNext",     "Next setlist entry" },
        { "accessibility.shortcut.undo",            "Undo" },
        { "accessibility.shortcut.redo",            "Redo" },
        { "accessibility.shortcut.save",            "Save preset" },
        { "accessibility.shortcut.saveAs",          "Save preset as..." },
        { "accessibility.shortcut.newPreset",       "New preset (loads Init)" },
        { "accessibility.shortcut.presetBrowser",   "Open the preset browser" },
        { "accessibility.shortcut.revealPreset",    "Show the preset file on disk" },
        { "accessibility.shortcut.saveGuitarAs",    "Save the guitar as..." },
        { "accessibility.shortcut.revealGuitar",    "Show the guitar file on disk" },
        { "workshop.saveGuitar.title",              "Save As Guitar" },
        { "workshop.saveGuitar.prompt",             "Name this guitar. It is saved by reference to its parts, so editing a part later updates every guitar that uses it." },
        { "workshop.saveGuitar.name",               "Name" },
        { "workshop.saveGuitar.bundle",             "Save with parts (for sharing)" },
        { "workshop.saveGuitar.save",               "Save" },
        { "workshop.saveGuitar.saved",              "Guitar saved as {name}." },
        { "workshop.saveGuitar.failed",             "The guitar could not be saved. The error log has the details." },
        { "workshop.revealGuitar.none",             "This guitar has not been saved as a file yet. Save As Guitar (Ctrl+G) makes one." },
        { "tune.import.failed",                     "Could not import {name}: {error}" },
        { "tune.import.details",                    "Details" },
        { "tune.import.noPanel",                    "The Tune Builder is not available in this window." },
        { "accessibility.shortcut.abCompare",       "A/B compare" },
        { "accessibility.shortcut.randomise",       "Randomise" },
        { "accessibility.shortcut.resetAll",        "Reset all" },
        { "accessibility.shortcut.midiLearnArm",    "Arm MIDI Learn" },
        { "accessibility.shortcut.export",          "Open export" },
        { "accessibility.shortcut.options",         "Open options" },
        { "accessibility.shortcut.debugPanel",      "Open the debug panel" },
        { "accessibility.shortcut.audition",        "Audition" },

        // ---- accessible descriptions -----------------------------------------------------------
        { "a11y.knob.role",           "Rotary control" },
        { "a11y.fret.description",    "String {string}, fret {fret}, note {note}" },
        { "a11y.fret.open",           "String {string}, open, note {note}" },
        { "a11y.meter.description",   "Output level, {db} decibels" },
        { "a11y.snapshot.description","Snapshot {number}, {name}" },
        { "a11y.snapshot.empty",      "Snapshot {number}, empty" },
        { "a11y.dialog.opened",       "{name} dialog opened" },
        { "a11y.dialog.closed",       "{name} dialog closed" },
        { "a11y.value.percent",       "{value} percent" },
        { "a11y.value.decibels",      "{value} decibels" },
        { "a11y.value.hertz",         "{value} hertz" },
        { "a11y.value.semitones",     "{value} semitones" },
        { "a11y.value.milliseconds",  "{value} milliseconds" },

        // ---- routing ---------------------------------------------------------------------------
        { "routing.mute",             "Mute" },
        { "routing.solo",             "Solo" },
        { "routing.gain",             "Gain" },
        { "routing.midiOut",          "MIDI out" },
        { "routing.sidechain",        "Sidechain" },
        { "routing.perString",        "Per-string outputs" },

        // ---- updates and privacy -------------------------------------------------------------
        { "updates.title",            "Updates" },
        { "updates.checkNow",         "Check now" },
        { "updates.automatic",        "Check for updates automatically" },
        { "updates.beta",             "Include beta releases" },
        { "updates.available",        "Version {version} is available" },
        { "updates.upToDate",         "Luthier is up to date" },
        { "updates.whatsNew",         "What's new" },
        { "updates.download",         "Download" },
        { "privacy.title",            "Privacy" },
        { "privacy.usage",            "Usage telemetry" },
        { "privacy.diagnostics",      "Diagnostics telemetry" },
        { "privacy.crashReports",     "Crash reports" },
        { "privacy.viewLog",          "View last upload" },
        { "privacy.clearLogs",        "Clear all local logs" },
        { "privacy.turnOffEverything","Turn everything off and delete all diagnostic files" },
        { "privacy.managedByPolicy",  "Managed by policy" },
        { "privacy.endpoint",         "Endpoint" },

        // ---- common -----------------------------------------------------------------------------
        { "common.ok",                "OK" },
        { "common.cancel",            "Cancel" },
        { "common.close",             "Close" },
        { "common.save",              "Save" },
        { "common.load",              "Load" },
        { "common.export",            "Export" },
        { "common.on",                "On" },
        { "common.off",               "Off" },
        { "common.none",              "None" },
        { "common.default",           "Default" },
        { "common.browse",            "Browse..." },

        // ---- messages with placeholders -----------------------------------------------------------
        { "message.presetsLoaded",    "Loaded {n} presets" },
        { "message.patternsLoaded",   "Loaded {n} patterns" },
        { "message.irLoaded",         "Loaded {name}, {ms} milliseconds" },
        { "message.exportComplete",   "Exported to {path}" },
        { "message.exportFailed",     "Could not export: {reason}" },
        { "message.snapshotRecalled", "Recalled snapshot {number}" },
        { "message.tempoDetected",    "Detected {bpm} bpm" },
        { "message.nullResult",       "Null test: {db} dB" }
    };

    return catalog;
}

} // namespace luthier
