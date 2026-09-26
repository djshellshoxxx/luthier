#include "Localisation.h"
#include "QualityStrings.h"   // cpu-quality-modes

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
    // cpu-quality-modes: the CPU-quality strings live in QualityStrings.cpp.
    static const std::map<juce::String, juce::String> catalog = QualityStrings::mergeInto (
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

        // ---- live -----------------------------------------------------------------------
        { "live.snapshot",            "Snapshot {number}" },
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
        { "accessibility.shortcut.toggleWorkshop", "Toggle Workshop" },
        { "accessibility.shortcut.togglePractice",  "Open the practice drawer" },
        { "accessibility.shortcut.panic",           "Panic: silence all notes" },
        { "accessibility.shortcut.tapTempo",        "Tap tempo" },
        { "accessibility.shortcut.killSwitch",      "Kill switch" },
        { "accessibility.shortcut.jamStartStop",    "Jam band: start or stop" },   // FEAT-JAM
        { "accessibility.shortcut.jamFill",         "Jam band: fill" },
        { "accessibility.shortcut.jamArm",          "Jam band: arm or disarm" },
        { "accessibility.shortcut.previousItem",    "Previous preset or snapshot" },
        { "accessibility.shortcut.nextItem",        "Next preset or snapshot" },
        { "accessibility.shortcut.previousWorkspaceTab", "Previous workspace tab" },
        { "accessibility.shortcut.nextWorkspaceTab", "Next workspace tab" },
        { "accessibility.shortcut.setlistPrevious", "Previous setlist entry" },
        { "accessibility.shortcut.setlistNext",     "Next setlist entry" },
        { "accessibility.shortcut.undo",            "Undo" },
        { "accessibility.shortcut.redo",            "Redo" },
        { "accessibility.shortcut.redoAlt",         "Redo (Ctrl+Y)" },
        { "accessibility.shortcut.undoAcrossBoundary", "Undo past a preset or guitar load" },
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
        { "accessibility.shortcut.abCompare",       "A/B compare" },
        { "accessibility.shortcut.randomise",       "Randomise" },
        { "accessibility.shortcut.resetAll",        "Reset all" },
        { "accessibility.shortcut.midiLearnArm",    "Arm MIDI Learn" },
        { "accessibility.shortcut.export",          "Open export" },
        { "accessibility.shortcut.newTune",         "New tune" },
        { "accessibility.shortcut.options",         "Open options" },
        { "accessibility.shortcut.debugPanel",      "Open the debug panel" },
        { "accessibility.shortcut.audition",        "Audition" },
        { "accessibility.shortcut.toggleStringAnimation", "Toggle string animation" },

        // ---- animated-strings.md 5 and 8: Options -> Appearance -> Visual aids ------------------
        { "options.appearance.visualAids.heading",        "VISUAL AIDS" },
        { "options.appearance.visualAids.animateStrings", "Animate strings" },
        { "options.appearance.visualAids.quality",        "Quality" },
        { "options.appearance.visualAids.qualityName",    "String animation quality" },
        { "options.appearance.visualAids.qualityLow",     "Low" },
        { "options.appearance.visualAids.qualityHigh",    "High" },
        { "options.appearance.visualAids.help",           "Strings vibrate on the guitar and fretboard while they sound. Display only: no effect on the sound." },
        { "options.appearance.visualAids.paused",         "Paused while Reduced motion is on." },

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
        { "message.nullResult",       "Null test: {db} dB" },

        // ---- output normalization (output-normalization.md 5) ----------------------
        { "options.audio.normalization.heading",   "OUTPUT NORMALIZATION" },
        { "options.audio.normalization.switch",    "Normalize output loudness" },
        { "options.audio.normalization.target",    "Target loudness" },
        { "options.audio.normalization.targetOff", "Turn normalization on to choose a target." },
        { "options.audio.normalization.caption",   "Evens out the natural level differences between guitars and settings, so relative levels are no longer realistic: a nylon-string or a clean single-coil will sound louder than it really is next to a high-gain humbucker." },
        { "options.audio.normalization.note",      "Applies to the main output only. Per-string and aux outputs keep their natural level. The safety limiter stays on while normalization is on." },
        { "options.audio.normalization.readout.off",          "Off. Each sound plays at its natural level." },
        { "options.audio.normalization.readout.measuring",    "Measuring this sound..." },
        { "options.audio.normalization.readout.applied",      "Normalization: {gain} dB (this sound measures {lufs} LUFS)" },
        { "options.audio.normalization.readout.clampedQuiet", "Normalization: {gain} dB (at the limit; this sound is very quiet)" },
        { "options.audio.normalization.readout.clampedLoud",  "Normalization: {gain} dB (at the limit; this sound is very loud)" },
        { "options.audio.normalization.readout.estimate",     "Normalization: about {gain} dB (estimated; measuring failed)" },
        { "options.audio.normalization.readout.unmeasurable", "This sound is silent on the test phrase; level unchanged." },
        { "options.audio.normalization.readout.morphing",     "Normalization: {gain} dB (between the two morph presets)" },
        { "options.audio.normalization.announce",  "Normalization {sign} {value} decibels" },
        { "banner.normalization.on",       "Output normalization is on. Every sound is brought to the same loudness, so the natural level differences between guitars and settings are gone. Turn it off for realistic relative levels." },
        { "banner.normalization.options",  "Options" },
        { "banner.normalization.dontShow", "Don't show again" },
        { "banner.normalization.failed",   "Normalization could not measure this sound." },
        { "badge.normalization.tooltip",   "Output normalization {gain} dB, target {target} LUFS. Click for options." },
        { "badge.normalization.name",      "Output normalization, {sign} {value} decibels" },
        { "routing.normalization.caption", "Output normalization applies to the main output only." },
        { "workshop.normalization.note",   "Normalization is on: level differences between parts are evened out. Shadow audition (Alt-hover) still plays at the real level." },
        { "accessibility.shortcut.toggleNormalization", "Toggle output normalization" },
    });

    return catalog;
}

} // namespace luthier
