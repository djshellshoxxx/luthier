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

std::vector<LocaleInfo> Localisation::getAvailableLocales() const
{
    std::vector<LocaleInfo> available;
    std::map<juce::String, juce::String> probe;

    for (const auto& locale : getShipLocales())
        if (loadCatalog (locale.code, probe))
            available.push_back (locale);

    return available;
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

        // ---- advanced: the pickup slots, which follow the fitted guitar --------------------
        { "advanced.pickup.label",          "Pickup {n} ({position})" },
        { "advanced.pickup.magnetLabel",    "Magnet {n} ({position})" },
        { "advanced.pickup.bridge",         "Bridge" },
        { "advanced.pickup.middle",         "Middle" },
        { "advanced.pickup.neck",           "Neck" },
        { "advanced.pickup.notFitted",      "not fitted" },
        { "advanced.pickup.notFitted.tooltip", "This guitar has no pickup in this slot; fit one on the Workshop bench." },

        // ---- the string roll (StringRoll.h) -------------------------------------------------
        { "stringRoll.lane.title",          "String {n} ({note})" },
        { "stringRoll.lane.help",           "Press to pluck the string. Higher in the lane is higher up the neck." },
        { "stringRoll.lane.tooltip",        "String {n} ({note}), fret {fret} - click to pluck" },
        { "stringRoll.empty.captureOff",    "Capture is off - switch it on and the strings you pluck appear here" },
        { "stringRoll.empty.play",          "Play something - the strings you pluck appear here" },

        // ---- the piano keyboard (PianoKeyboard.h) --------------------------------------
        { "piano.title",                    "Piano keyboard" },
        { "piano.tooltip",                  "Click a key to play it on the guitar. Lit keys show what the strings are playing." },
        { "piano.help",                     "Left and Right choose a key, Up and Down an octave; Space or Enter plays it. Lit keys show what each string is playing, numbered by string." },
        { "piano.qwerty",                   "QWERTY" },
        { "piano.qwerty.tooltip",           "Play the keys from the computer keyboard (A W S E D F T G ...). While it is on and the piano has focus, those letters play notes instead of their shortcuts." },
        { "piano.zoomOut",                  "Zoom out" },
        { "piano.zoomOut.tooltip",          "Narrower keys (Ctrl or Cmd + wheel)" },
        { "piano.zoomIn",                   "Zoom in" },
        { "piano.zoomIn.tooltip",           "Wider keys - scroll with the wheel or the arrows at the ends (Ctrl or Cmd + wheel)" },
        { "piano.silent",                   "Nothing sounding" },
        { "piano.sounding",                 "Sounding: {notes}" },
        { "piano.soundingNote",             "{note} on string {n}" },
        { "advanced.strip.keys",            "KEYS" },
        { "advanced.strip.keys.tooltip",    "Show a piano keyboard in the strip: click a key to play it on the guitar; lit keys show what each string is playing." },
        { "easy.keys",                      "Keys" },
        { "easy.keys.tooltip",              "Show a piano keyboard under the guitar: click a key to play it on the guitar; lit keys show what the strings are playing." },

        // ---- shared widgets -----------------------------------------------------------------
        { "widgets.knob.wheelHint",   "{tip}  Ctrl+wheel nudges the value; the wheel alone scrolls the column." },
        { "widgets.menu.assignToMacro",     "Assign to macro" },
        { "widgets.menu.automationId",      "Automation ID: {id}" },
        { "widgets.menu.showShortcuts",     "Show in Options -> Shortcuts" },
        { "widgets.section.helpTip",        "Help for {section}" },
        { "widgets.section.resetPanel",     "Reset panel to default" },
        { "widgets.section.resetUndo",      "Reset {section}" },
        { "widgets.section.screenshot",     "Screenshot to Pictures/Luthier" },
        { "widgets.section.screenshotSaved", "Saved {path}" },
        { "widgets.section.screenshotFailed", "Could not save the screenshot." },
        { "widgets.section.docs",           "Docs" },

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
        // practice-tools 11.2: the session recorder's drawer transport and the
        // PRACTICE tab's setup, the looper's default length and the trainers'
        // session (range and question count).
        { "practice.session.enable",        "Session recorder" },
        { "practice.session.save",          "Save last take" },
        { "practice.session.drag",          "Drag the take out as files" },
        { "practice.session.openFolder",    "Open sessions folder" },
        { "practice.session.ringLength",    "Ring length" },
        { "practice.session.recordWhat",    "Record" },
        { "practice.session.audioAndMidi",  "Audio and MIDI" },
        { "practice.session.audioOnly",     "Audio only" },
        { "practice.session.midiOnly",      "MIDI only" },
        { "practice.session.autoSave",      "Auto-save on stop" },
        { "practice.session.held",          "{recorded} of {capacity} minutes held" },
        { "practice.session.saved",         "Saved {name} to your Sessions folder." },
        { "practice.session.autoSaved",     "Auto-saved {name} to your Sessions folder." },
        { "practice.session.nothing",       "There is nothing recorded to save." },
        { "practice.looper.defaultLength",  "Default loop length" },
        { "practice.looper.firstTake",      "first take" },
        { "practice.trainer.lowestNote",    "Lowest note" },
        { "practice.trainer.highestNote",   "Highest note" },
        { "practice.trainer.questions",     "Questions per session" },
        { "practice.trainer.ask",           "Ask" },
        { "practice.trainer.new",           "New" },
        { "practice.trainer.complete",      "Session complete: {correct} of {asked} right. Press New to start again." },
        { "practice.tab.open",              "Open tab file" },

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
        { "character.stringDetune",   "STRING DETUNE" },
        { "character.stringDetuneTip","Detunes this string by up to 25 cents - enough to hear, never a different note. Double-click to put it back in tune." },
        { "character.outOfTune",      "Out of tune" },
        { "character.outOfTuneTip",   "How out of tune the guitar is: turn it and every string moves together, keeping the pattern it has (or rolling one if it is in tune). Randomise rolls a new pattern within this range." },
        { "character.detuneRandomise","Randomise" },
        { "character.detuneRandomiseTip", "Puts every string a random amount out of tune, within the Out of tune range. Enough to hear, never a different note." },
        { "character.detuneReset",    "Reset" },
        { "character.detuneResetTip", "Puts every string back in tune." },

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
        { "accessibility.shortcut.redoAlt",         "Redo (alternate key)" },
        { "accessibility.shortcut.undoAcrossBoundary", "Undo across a preset, guitar or setlist boundary" },
        { "undo.boundary.stopped",                  "Undo stopped at \"{name}\". Ctrl+Alt+Z undoes across it." },
        { "undo.boundary.crossed",                  "Undo crossed \"{name}\": the state before it is back." },
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
        { "tune.import.title",                      "MIDI import" },
        { "tune.import.noPanel",                    "The Tune Builder is not available in this window." },
        { "open.file.unsupported",                  "Luthier cannot open {name}: it is not a preset, guitar, tune or MIDI file." },
        { "open.file.unreadable",                   "Could not open {name}: the file is missing or cannot be read." },
        { "open.guitar.failed",                     "Could not open the guitar {name}: {error}" },
        { "open.tune.failed",                       "Could not open the tune {name}: {error}" },
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
        { "message.nullResult",       "Null test: {db} dB" },

        // ---- workshop bench (workshop-ui.md) --------------------------------------------------------
        { "workshop.title",                   "Workshop" },
        { "workshop.heading",                 "WORKSHOP" },
        { "workshop.description",             "The bench: take the guitar apart, swap parts, move pickups, hear what changed." },
        { "workshop.illustration.title",      "Workshop guitar" },
        { "workshop.illustration.description","The guitar on the bench. Tab walks the parts and tools; arrow keys nudge the selected one." },
        { "workshop.guitarModified",          "{name}  (modified)" },
        { "workshop.saveAsGuitar",            "Save As Guitar" },
        { "workshop.saveAsGuitar.tip",        "Save this guitar as a .luthierguitar file (Ctrl+G)" },
        { "workshop.slot.name",               "Bench slot {slot}" },
        { "workshop.slot.filled",             "Bench slot {slot}: click to recall, Shift-click to clear" },
        { "workshop.slot.empty",              "Bench slot {slot}: click to store the guitar as it is now" },
        { "workshop.category.tip",            "Show the {category} parts" },
        { "workshop.category.guitar",         "Guitar" },
        { "workshop.category.body",           "Body" },
        { "workshop.category.neck",           "Neck" },
        { "workshop.category.frets",          "Frets" },
        { "workshop.category.nut",            "Nut" },
        { "workshop.category.bridge",         "Bridge" },
        { "workshop.category.tuners",         "Tuners" },
        { "workshop.category.strings",        "Strings" },
        { "workshop.category.pickups",        "Pickups" },
        { "workshop.category.wiring",         "Wiring" },
        { "workshop.category.preamp",         "Preamp" },
        { "workshop.category.pick",           "Pick" },
        { "workshop.category.slide",          "Slide" },
        { "workshop.category.capo",           "Capo" },
        { "workshop.swap",                    "Swap" },
        { "workshop.swap.tip",                "Show the parts that can go in this slot" },
        { "workshop.revert",                  "Revert" },
        { "workshop.revert.tip",              "Put this slot back to what the guitar file has" },
        { "workshop.savePart",                "Save as user part" },
        { "workshop.savePart.tip",            "Keep this edited part in your parts folder" },
        { "workshop.savePart.prompt",         "Name the part:" },
        { "workshop.savePart.defaultName",    "{name} (mine)" },
        { "workshop.autoZoom",                "Auto-zoom" },
        { "workshop.autoZoom.tip",            "Fit the spectrum's scale to the change instead of +-12 dB" },
        { "workshop.spectrum",                "Spectrum delta" },
        { "workshop.spectrum.empty",          "Swap or audition a part to see what it changes." },
        { "workshop.setup",                   "Setup" },
        { "workshop.setup.actionTreble",      "Action T" },
        { "workshop.setup.actionTreble.tip",  "String height at the 12th fret, treble side (mm)" },
        { "workshop.setup.actionBass",        "Action B" },
        { "workshop.setup.actionBass.tip",    "String height at the 12th fret, bass side (mm)" },
        { "workshop.setup.relief",            "Relief" },
        { "workshop.setup.relief.tip",        "Neck relief at the 7th fret (mm)" },
        { "workshop.setup.nut",               "Nut {n}" },
        { "workshop.setup.nut.tip",           "Nut slot depth, string {n} (mm)" },
        { "workshop.inspector",               "Inspector" },
        { "workshop.inspector.nothing",       "Nothing selected" },
        { "workshop.inspector.nothing.hint",  "Click a part of the guitar, or press Tab." },
        { "workshop.inspector.empty",         "(empty)" },
        { "workshop.inspector.auditioning",   "AUDITIONING - not fitted" },
        { "workshop.inspector.factory",       "Factory part" },
        { "workshop.inspector.user",          "User part (unsaved edits live in the preset)" },
        { "workshop.inspector.unusual",       "Unusual on a {family} guitar - it will work." },
        { "workshop.inspector.editHint",      "Double-click a value to edit it. A factory part becomes your own copy." },
        { "workshop.auditioningBanner",       "AUDITIONING - release Alt to go back" },
        { "workshop.ruler",                   "mm from saddle" },
        { "workshop.slideNeedsSlideMode",     "Turn on Slide Mode (S) to fit a slide." },
        { "workshop.drawer.noUserParts",      "Your saved parts appear here. Edit any factory part and Save As to start." },
        { "workshop.drawer.none",             "No {category} parts are installed." },
        { "workshop.card.unusual",            "Unusual here - {summary}" },
        { "workshop.card.yours",              "yours - {summary}" },
        { "workshop.card.tip",                "{name} - {summary}. Click to fit; hold Alt to hear it without fitting." },
        { "workshop.family.title",            "Change guitar family?" },
        { "workshop.family.message",          "This will replace incompatible parts with defaults for the new family." },
        { "workshop.family.change",           "Change" },
        { "workshop.string.wound",            "wound" },
        { "workshop.string.plain",            "plain" },
        { "workshop.string.overridden",       "string {n} is overridden - set its fields back to the set's to clear it" },
        { "workshop.string.fromSet",          "string {n} plays the set's string - edit a field above to override it" },
        { "workshop.tool.pick",               "Pick" },
        { "workshop.tool.slide",              "Slide" },
        { "workshop.tool.capo",               "Capo" },
        { "workshop.tool.default",            "(default)" },
        { "workshop.tool.currentPick",        "the current pick" },
        { "workshop.tool.defaultSlide",       "glass 22 mm" },
        { "workshop.tool.defaultCapo",        "full capo" },
        { "workshop.field.pickPosition",      "position: {mm} mm from the saddle" },
        { "workshop.field.pickAngle",         "angle: {deg} degrees" },
        { "workshop.field.slideFret",         "position: fret {fret}" },
        { "workshop.field.slideSlant",        "slant: {deg} degrees" },
        { "workshop.field.slidePositionNote", "The bar's position is where the bench draws it; playing moves it." },
        { "workshop.field.capoFret",          "fret: {fret}" },
        { "workshop.tip.pickup",              "Drag along the strings to move it; scroll to raise or lower it (Shift: treble side, Alt: bass side)." },
        { "workshop.tip.saddle",              "Drag a saddle along the string to set its intonation." },
        { "workshop.tip.nutSlot",             "Drag a slot down to cut it deeper (0.05 mm steps; Shift: 0.005 mm)." },
        { "workshop.tip.strings",             "Click a string to give it its own gauge or material." },
        { "workshop.tip.pick",                "Drag along the strings to move the pick; drag a back corner to angle it." },
        { "workshop.tip.slide",               "Drag the bar along the neck; drag an end to slant it." },
        { "workshop.tip.capo",                "Drag along the neck to a fret; past the nut takes it off." },
        { "workshop.a11y.string",             "String {n}: {string}, {material}." },
        { "workshop.a11y.stringOverridden",   "Overridden from the set." },
        { "workshop.a11y.nutSlot",            "String {n} slot {mm} mm deep." },
        { "workshop.a11y.pick",               "Pick: {name}, position {mm} mm from the saddle, angle {deg} degrees." },
        { "workshop.a11y.slide",              "Slide: {name}; position fret {fret}, slant {deg} degrees." },
        { "workshop.a11y.capo",               "Capo: {name} at fret {fret}." },
        { "workshop.a11y.capoOff",            "Capo: {name}, off, resting on the headstock." },
        { "workshop.card.otherFamily",        "Made for {made} - fits, unusual here - {summary}" },
        { "workshop.fit.otherFamily",         "{part} is made for {made} guitars. It is fitted and it will work on a {family} guitar; for a matched build, Guitar > {Made} rebuilds around it." },
        { "workshop.drawer.more",             "{n} more - scroll for the rest" },
        { "workshop.paint.body",              "Body colour" },
        { "workshop.paint.edge",              "Burst edge" },
        { "workshop.paint.centre",            "Burst centre" },
        { "workshop.paint.plastics",          "Plastics colour" },
        { "workshop.paint.tip",               "{what}: click for the colour picker. Paint only - the sound does not change." },
        { "workshop.paint.picker",            "Finish colour" },
        { "workshop.paint.plasticsReset",     "Part's own" },
        { "workshop.paint.plasticsReset.tip", "Put the plastics back to the pickguard part's own colour" },
        { "workshop.paint.preset.tip",        "{name} finish" },
        { "workshop.paint.preset.sunburst",   "Sunburst" },
        { "workshop.paint.preset.cherry",     "Cherry burst" },
        { "workshop.paint.preset.black",      "Black" },
        { "workshop.paint.preset.white",      "White" },
        { "workshop.paint.preset.seafoam",    "Seafoam green" },
        { "workshop.paint.preset.natural",    "Natural" },
        // ---- TUNE tab (tune-builder 3, 6, 7, 9) --------------------------------------------------
        { "accessibility.shortcut.newTune",         "New tune (template picker)" },
        { "tune.untitled",                          "Untitled Tune" },
        { "tune.sections",                          "Sections" },
        { "tune.sections.tooltip",                  "Click a section to edit it; drag to reorder; right-click to rename, duplicate, vary, delete, repeat or tag it" },
        { "tune.sections.menu.vary",                "Vary (a variation as a new section)" },
        { "tune.sections.edit.move",                "Move section" },
        { "tune.sections.edit.vary",                "Vary section" },
        { "tune.pills",                             "Chord pills" },
        { "tune.pills.tooltip",                     "The section's chords, coloured by their function in the key. Click one to edit it, drag to reorder, drag its right edge to change its length, right-click for more" },
        { "tune.pills.empty",                       "No chords yet: type a progression above" },
        { "tune.pills.menu.edit",                   "Edit..." },
        { "tune.pills.menu.insertBefore",           "Insert before" },
        { "tune.pills.menu.insertAfter",            "Insert after" },
        { "tune.pills.menu.duplicate",              "Duplicate" },
        { "tune.pills.menu.delete",                 "Delete" },
        { "tune.pills.menu.copy",                   "Copy" },
        { "tune.pills.menu.paste",                  "Paste" },
        { "tune.pills.menu.lock",                   "Lock (reharmonize leaves it alone)" },
        { "tune.pills.menu.substitute",             "Suggest substitution" },
        { "tune.pills.menu.suggestNext",            "Suggest next chord" },
        { "tune.pills.edit.insert",                 "Insert chord" },
        { "tune.pills.edit.duplicate",              "Duplicate chord" },
        { "tune.pills.edit.delete",                 "Delete chord" },
        { "tune.pills.edit.paste",                  "Paste chord" },
        { "tune.pills.edit.move",                   "Move chord" },
        { "tune.pills.edit.suggestNext",            "Add suggested chord" },
        { "tune.chord.editor",                      "Chord editor" },
        { "tune.chord.editor.tooltip",              "The chord's root, quality, bass, length, extensions, emphasis and lock." },
        { "tune.chord.root",                        "Root" },
        { "tune.chord.root.tooltip",                "The chord's root note" },
        { "tune.chord.quality",                     "Quality" },
        { "tune.chord.quality.tooltip",             "Major, minor, seventh and the rest: every one the rhythm engine can play" },
        { "tune.chord.quality.major",               "major" },
        { "tune.chord.bass",                        "Bass" },
        { "tune.chord.bass.tooltip",                "A slash chord's bass note, or the root" },
        { "tune.chord.bass.root",                   "root" },
        { "tune.chord.beats",                       "Beats" },
        { "tune.chord.beats.tooltip",               "How many beats the chord lasts" },
        { "tune.chord.fill",                        "to end" },
        { "tune.chord.fill.tooltip",                "Hold this chord to the end of the section" },
        { "tune.chord.extensions",                  "Add" },
        { "tune.chord.extensions.tooltip",          "Extensions, comma separated: 9, add9, #11, 13" },
        { "tune.chord.emphasis",                    "Play" },
        { "tune.chord.emphasis.tooltip",            "How hard the chord is played" },
        { "tune.chord.emphasis.normal",             "normal" },
        { "tune.chord.emphasis.accent",             "accent" },
        { "tune.chord.emphasis.ghost",              "ghost" },
        { "tune.chord.lock",                        "Locked: reharmonize and substitutions leave this chord alone" },
        { "tune.chord.lock.tooltip",                "A locked chord is kept as written by Reharmonize and the substitution offers" },
        { "tune.chord.edit.root",                   "Change chord root" },
        { "tune.chord.edit.quality",                "Change chord quality" },
        { "tune.chord.edit.bass",                   "Change chord bass" },
        { "tune.chord.edit.beats",                  "Change chord length" },
        { "tune.chord.edit.extensions",             "Change chord extensions" },
        { "tune.chord.edit.emphasis",               "Change chord emphasis" },
        { "tune.chord.edit.lock",                   "Lock chord" },
        { "tune.chord.edit.unlock",                 "Unlock chord" },
        { "tune.bass",                              "Bass line" },
        { "tune.bass.label",                        "BASS" },
        { "tune.bass.tooltip",                      "The section's bass line: off, a pattern that follows the chords, or a manual line drawn on the roll's BASS part" },
        { "tune.bass.edit",                         "Change bass mode" },
        { "tune.bass.mode.off",                     "Off" },
        { "tune.bass.mode.root",                    "Root" },
        { "tune.bass.mode.root_fifth",              "Root-fifth" },
        { "tune.bass.mode.walking",                 "Walking" },
        { "tune.bass.mode.genre",                   "Genre" },
        { "tune.bass.mode.manual",                  "Manual" },
        { "tune.layer.label",                       "LAYERS" },
        { "tune.layer.pad",                         "PAD" },
        { "tune.layer.pad.tooltip",                 "Sustained chord tones on a soft picked layer" },
        { "tune.layer.arpeggio",                    "ARP" },
        { "tune.layer.arpeggio.tooltip",            "A fingerpick pattern from the rhythm engine" },
        { "tune.layer.countermelody",               "COUNTER" },
        { "tune.layer.countermelody.tooltip",       "A second melody that complements the main one; edit it on the roll's LAYER part" },
        { "tune.layer.percussion",                  "PERC" },
        { "tune.layer.percussion.tooltip",          "The chuck and palm-mute noise as rhythmic percussion" },
        { "tune.layer.edit.on",                     "Layer on" },
        { "tune.layer.edit.off",                    "Layer off" },
        { "tune.part.label",                        "PART" },
        { "tune.part.melody",                       "MELODY" },
        { "tune.part.melody.tooltip",               "Edit the section's melody on the roll" },
        { "tune.part.bass",                         "BASS" },
        { "tune.part.bass.tooltip",                 "Edit the section's bass line on the roll; a first edit makes it a Manual line" },
        { "tune.part.countermelody",                "LAYER" },
        { "tune.part.countermelody.tooltip",        "Edit the countermelody layer on the roll" },
        { "tune.roll",                              "Piano roll" },
        { "tune.roll.tooltip",                      "Drag to draw a note; click to select, Shift-click to add, drag on empty space (or with Draw off) to box-select; arrows nudge (Shift: octave or bar); Ctrl+C/X/V; right-click a note for velocity, articulation, technique, lock and delete; C toggles chromatic; wheel scrolls, Ctrl+wheel zooms" },
        { "tune.roll.view",                         "VIEW" },
        { "tune.roll.zoomIn",                       "Zoom in" },
        { "tune.roll.zoomIn.tooltip",               "Show fewer bars (Ctrl+wheel on the roll)" },
        { "tune.roll.zoomOut",                      "Zoom out" },
        { "tune.roll.zoomOut.tooltip",              "Show more bars (Ctrl+wheel on the roll)" },
        { "tune.roll.noSection",                    "Add a section to write a melody" },
        { "tune.roll.improvising",                  "IMPROVISING: a new line every pass" },
        { "tune.roll.chromatic",                    "CHROMATIC" },
        { "tune.roll.bassDerived",                  "BASS: follows the chords; draw or move a note to make it a manual line" },
        { "tune.roll.bassOff",                      "BASS: off; draw a note to write a manual line" },
        { "tune.roll.bassManual",                   "BASS: manual" },
        { "tune.roll.menu.velocity",                "Velocity ({v})" },
        { "tune.roll.menu.articulation",            "Articulation" },
        { "tune.roll.menu.technique",               "Technique" },
        { "tune.roll.menu.lock",                    "Lock (regenerate leaves it alone)" },
        { "tune.roll.menu.unlock",                  "Unlock" },
        { "tune.roll.menu.delete",                  "Delete" },
        { "tune.roll.menu.paste",                   "Paste here" },
        { "tune.roll.menu.selectAll",               "Select all" },
        { "tune.roll.articulation.inherit",         "As the track" },
        { "tune.roll.articulation.natural",         "Natural" },
        { "tune.roll.articulation.legato",          "Legato" },
        { "tune.roll.articulation.staccato",        "Staccato" },
        { "tune.roll.articulation.palm_muted",      "Palm muted" },
        { "tune.roll.articulation.let_ring",        "Let ring" },
        { "tune.roll.technique.none",               "None" },
        { "tune.roll.technique.bend",               "Bend" },
        { "tune.roll.technique.slide",              "Slide" },
        { "tune.roll.technique.hammer_on",          "Hammer-on" },
        { "tune.roll.technique.pull_off",           "Pull-off" },
        { "tune.roll.technique.vibrato",            "Vibrato" },
        { "tune.roll.technique.harmonic",           "Harmonic" },
        { "tune.roll.edit.draw",                    "Draw note" },
        { "tune.roll.edit.delete",                  "Delete notes" },
        { "tune.roll.edit.lock",                    "Lock notes" },
        { "tune.roll.edit.unlock",                  "Unlock notes" },
        { "tune.roll.edit.nudge",                   "Nudge notes" },
        { "tune.roll.edit.velocity",                "Change velocity" },
        { "tune.roll.edit.articulation",            "Change articulation" },
        { "tune.roll.edit.technique",               "Change technique" },
        { "tune.roll.edit.paste",                   "Paste notes" },
        { "tune.transport.stop",                    "Stop" },
        { "tune.transport.stop.tooltip",            "Stop and go back to the start of the tune" },
        { "tune.export.button",                     "Export tune" },
        { "tune.export.button.tooltip",             "Export the tune as audio, MIDI, notation and a project file (Ctrl+E)" },
        { "tune.export.title",                      "Export Tune" },
        { "tune.export.name",                       "File name" },
        { "tune.export.name.tooltip",               "The name every exported file starts with" },
        { "tune.export.folder",                     "Folder" },
        { "tune.export.folder.tooltip",             "Where the files go" },
        { "tune.export.audio",                      "Audio" },
        { "tune.export.audio.tooltip",              "Render the tune through this instance's sound, exactly as it plays" },
        { "tune.export.format",                     "Format" },
        { "tune.export.format.tooltip",             "The audio file format" },
        { "tune.export.bitDepth",                   "Bit depth" },
        { "tune.export.bitDepth.tooltip",           "16 or 24-bit integer, or 32-bit float" },
        { "tune.export.bits",                       "{n}-bit" },
        { "tune.export.sampleRate",                 "Sample rate" },
        { "tune.export.sampleRate.tooltip",         "The render's sample rate; the current one by default" },
        { "tune.export.tail",                       "Tail" },
        { "tune.export.tail.tooltip",               "Seconds of decay after the final beat, 0 to 5" },
        { "tune.export.stems",                      "Stems" },
        { "tune.export.stems.tooltip",              "One stereo file, or every routing bus (main and Aux 1 to 8) as its own file" },
        { "tune.export.stems.main",                 "Main stereo" },
        { "tune.export.stems.every",                "Every bus as a stem" },
        { "tune.export.midi",                       "MIDI" },
        { "tune.export.midi.tooltip",               "Write the tune as a MIDI file" },
        { "tune.export.profile",                    "Profile" },
        { "tune.export.profile.tooltip",            "Luthier: sections and realism ride along and come back on import. Generic: plain MIDI any instrument can play" },
        { "tune.export.profile.luthier",            "Luthier" },
        { "tune.export.profile.generic",            "Generic" },
        { "tune.export.split",                      "Tracks" },
        { "tune.export.split.tooltip",              "One track, one per section, one per instrument (guitar and bass), or one per string" },
        { "tune.export.split.single",               "Single track" },
        { "tune.export.split.section",              "Per section" },
        { "tune.export.split.instrument",           "Per instrument" },
        { "tune.export.split.string",               "Per string" },
        { "tune.export.realism",                    "Realism" },
        { "tune.export.realism.tooltip",            "Bends, slides and vibratos as controllers, or plain note-on / note-off" },
        { "tune.export.notation",                   "Notation" },
        { "tune.export.notation.tooltip",           "Write the tune as notation: sections and chord symbols kept" },
        { "tune.export.notationFormat",             "Notation format" },
        { "tune.export.notationFormat.tooltip",     "MusicXML, Guitar Pro or ASCII tab" },
        { "tune.export.chordSymbols",               "Chord symbols" },
        { "tune.export.chordSymbols.tooltip",       "Chord symbols above the staff" },
        { "tune.export.project",                    "Project (.luthiertune)" },
        { "tune.export.project.tooltip",            "A copy of the tune file, which opens in any Luthier" },
        { "tune.export.go.tooltip",                 "Write every ticked destination" },
        { "tune.export.cancel.tooltip",             "Stop the audio render" },
        { "tune.export.cancelled",                  "Cancelled" },
        { "tune.export.busy",                       "Another render is still running" },
        { "tune.export.nothingTicked",              "Tick at least one destination." },
        { "tune.export.noSections",                 "The tune has no sections to export." },
        { "tune.export.noFolder",                   "Could not create {path}." },
        { "tune.export.rendering",                  "Rendering {name}..." },
        { "tune.export.estimate",                   "{seconds} s of audio, about {size}" },
        { "tune.export.done",                       "Exported {n} file(s) to {path}" },
        { "tune.export.failed",                     "Could not export: {errors}" },
        { "tune.export.notice.done",                "Tune exported: {n} file(s) written" },
        { "tune.export.notice.failed",              "Tune export: {errors}" },

        // ---- visual polish: faces, rig strip and the preset browser --------------------------
        { "amp.vuMeter",              "VU meter" },
        { "amp.vuMeter.tooltip",      "The master output on a VU scale: 0 VU is -18 dBFS" },
        { "amp.model",                "Amp model" },
        { "easy.roomSize",            "Room size" },
        { "easy.roomLight",           "The room's light: wider with the size, warmer with the wet level" },
        { "presets.thumbnail",        "The preset's guitar" },
        { "presets.thumbnailPending", "Drawing the guitar" },
        { "presets.list",             "Presets" }
    };

    return catalog;
}

} // namespace luthier
