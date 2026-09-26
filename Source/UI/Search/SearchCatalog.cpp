#include "SearchCatalog.h"

#include "../../Accessibility/Localisation.h"

namespace luthier::search::SearchCatalog
{

const std::map<juce::String, juce::String>& builtInEnglish()
{
    static const std::map<juce::String, juce::String> catalog =
    {
        // ---- the palette (6, 14) -------------------------------------------------------
        { "search.name",                "Search" },
        { "search.fieldLabel",          "Search everything" },
        { "search.tooltip",             "Search everything ({key})" },
        { "search.menuItem",            "Search... {key}" },
        { "search.announce.open",       "Search. Type to find controls, places, commands and presets." },
        { "search.announce.results",    "{n} results, first: {title}" },
        { "search.announce.none",       "No results" },
        { "search.announce.target",     "{name}, {breadcrumb}" },
        { "search.hint",                "Type a control, place, command or preset. > commands, ? help, # content" },
        { "search.footer",              "Enter go  |  Alt+Enter actions  |  type a value to set  |  Esc close" },
        { "search.section.recent",      "Recent" },
        { "search.section.recentQueries", "Recent searches" },
        { "search.section.suggestions", "Suggestions" },
        { "search.noResults",           "No matches for '{query}'." },
        { "search.didYouMean",          "Did you mean '{suggestion}'?" },
        { "search.searchHelp",          "Search Help for '{query}'" },
        { "search.opensInAdvanced",     "Opens in Advanced" },
        { "search.opensInAdvancedConfirm", "Opens in Advanced Mode. Enter again to switch." },
        { "search.needsSlideMode",      "Needs Slide Mode" },
        { "search.slideModeOff",        "Slide Mode is off. Enter turns it on and shows the control." },
        { "search.needsPart",           "Appears on guitars with {part}. Open Workshop -> {category}?" },
        { "search.part.bass",           "a bass body" },
        { "search.part.whammy",         "a whammy bridge" },
        { "search.needsEmptySlot",      "Every slot in that chain is full. Remove a pedal first." },
        { "search.advancedUnavailable", "Advanced Mode is unavailable at this width. Adjust here, or widen the window." },
        { "search.advancedLocked",      "Advanced Mode is locked by Live Mode. Adjust here." },
        { "search.proLocked",           "Available in Luthier Pro" },
        { "search.pro",                 "Pro" },
        { "search.switchedToAdvanced",  "Switched to Advanced Mode to show {name}. {key} returns." },
        { "search.switchedToEasy",      "Switched to Easy Mode to show {name}. {key} returns." },
        { "search.navigateMiss",        "Couldn't show {name}. Opened its panel instead." },
        { "search.noLongerExists",      "{name} no longer exists" },
        { "search.nothingHappened",     "Nothing to do for {name} here." },
        { "search.clamped",             "Clamped to {value} ({edge})" },
        { "search.setTo",               "Set to {value} (now {old})" },
        { "search.valueSet",            "{name} set to {value}" },
        { "search.noInlineSet",         "{name} can't be set from here." },
        { "search.clearRecent",         "Clear recent searches" },
        { "search.group",               "Search" },
        { "search.autoSwitchMode",      "Switch mode automatically to show a result" },
        { "search.rememberRecent",      "Remember recent searches" },
        { "search.helpField",           "Search everything..." },
        { "search.recentCleared",       "Recent searches cleared." },

        // Row actions (5).
        { "search.action.load",         "Load" },
        { "search.action.showInBrowser", "Show in browser" },
        { "search.action.reveal",       "Reveal file" },
        { "search.action.showInWorkshop", "Show in Workshop" },
        { "search.action.fit",          "Fit to this guitar" },
        { "search.action.run",          "Run" },
        { "search.action.showShortcut", "Show shortcut" },
        { "search.action.open",         "Open" },
        { "search.action.openPinned",   "Open pinned in HELP tab" },
        { "search.action.copyId",       "Copy id" },
        { "search.action.removeRecent", "Remove from recent" },
        { "search.action.goTo",         "Go to control" },

        // Scope chips (4.1).
        { "search.scope.All",           "All" },
        { "search.scope.Controls",      "Controls" },
        { "search.scope.Places",        "Places" },
        { "search.scope.Commands",      "Commands" },
        { "search.scope.Content",       "Content" },
        { "search.scope.Help",          "Help" },

        // Commands without a shortcut (4.3) and the new shortcut.
        { "accessibility.shortcut.search", "Search everything" },
        { "search.cmd.openWorkshop",    "Open Workshop" },
        { "search.cmd.exportMidi",      "Export MIDI" },
        { "search.cmd.exportAudio",     "Export audio" },
        { "search.cmd.importMidi",      "Import MIDI" },
        { "search.cmd.retuneAll",       "Retune all" },
        { "search.cmd.newTune",         "New tune" },
        { "search.cmd.recallSnapshot",  "Recall snapshot {n}" },
        { "search.cmd.saveSnapshot",    "Save snapshot to slot {n}" },
        { "search.cmd.armTechnique",    "Arm / disarm {technique}" },
        { "search.cmd.clearRecentSearches", "Clear recent searches" },
        { "search.cmd.openChords",      "Open chords and tab" },
        { "search.cmd.addPedal",        "Add {pedal} pedal" },

        // ---- titles that differ from the parameter's own short name ------------------
        // A knob's name is read beside its amp face; a search row stands alone.
        { "search.title.param:amp_gain",      "Amp gain" },
        { "search.title.param:amp_bass",      "Amp bass" },
        { "search.title.param:amp_mid",       "Amp mid" },
        { "search.title.param:amp_treble",    "Amp treble" },
        { "search.title.param:amp_presence",  "Amp presence" },
        { "search.title.param:amp_master",    "Amp master" },
        { "search.title.param:amp_bright",    "Amp bright switch" },
        { "search.title.param:amp_mid_boost", "Amp mid boost" },
        { "search.title.param:amp_standby",   "Amp standby" },
        { "search.title.param:amp_model",     "Amp model" },
        { "search.title.param:room_blend",    "Room" },   // the rig strip ROOM card's wet/dry (GS-20 "room 50%")
        { "search.title.param:output_mix",    "Output wet/dry" },
        { "search.title.param:macro_attack",  "Attack macro" },
        { "search.title.param:macro_body",    "Body macro" },
        { "search.title.param:macro_tone",    "Tone macro" },
        { "search.title.param:macro_space",   "Space macro" },
        { "search.title.param:macro_humanize", "Humanize macro" },
        { "search.title.param:macro_character", "Character macro" },
        { "search.title.param:cab_on",        "Cabinet on" },
        { "search.title.param:room_on",       "Room on" },
        { "search.title.param:strum_direction", "Strum direction" },

        // ---- synonyms (2): parameters ---------------------------------------------------
        { "search.syn.param:amp_gain",        "drive|distortion|overdrive|dirt|crunch|preamp" },
        { "search.syn.param:amp_bass",        "low|lows|bottom|eq" },
        { "search.syn.param:amp_mid",         "middle|mids|eq" },
        { "search.syn.param:amp_treble",      "high|highs|top end|bright|eq" },
        { "search.syn.param:amp_presence",    "air|sparkle|top end" },
        { "search.syn.param:amp_master",      "volume|loudness|level|power amp" },
        { "search.syn.param:amp_model",       "amplifier|head|plexi|combo" },
        { "search.syn.param:macro_drive",     "distortion|dirt|crunch" },
        { "search.syn.param:macro_space",     "reverb|ambience|room" },
        { "search.syn.param:room_blend",      "reverb|ambience|wet|dry|mix|space|echo" },
        { "search.syn.param:room_decay",      "reverb time|tail|rt60|length" },
        { "search.syn.param:room_size",       "reverb|space|hall|studio" },
        { "search.syn.param:room_on",         "reverb|ambience" },
        { "search.syn.param:output_mix",      "wet|dry|mix|blend" },
        { "search.syn.param:master_gain",     "output|volume|level|loudness" },
        { "search.syn.param:input_gain",      "trim|level|input level|sensitivity" },
        { "search.syn.param:stereo_width",    "stereo|wide|spread|image" },
        { "search.syn.param:limiter_on",      "clip|protect|ceiling|brickwall" },
        { "search.syn.param:oversampling",    "aliasing|quality|hq" },
        { "search.syn.param:circuit_treble_bleed", "bleed|volume knob|highs|top end|cap" },
        { "search.syn.param:circuit_bleed_r", "treble bleed|resistor" },
        { "search.syn.param:circuit_bleed_c", "treble bleed|capacitor" },
        { "search.syn.param:circuit_volume_pot", "pot|potentiometer|volume knob" },
        { "search.syn.param:circuit_tone_pot", "pot|potentiometer|tone knob" },
        { "search.syn.param:circuit_tone_cap", "capacitor|tone knob|cap" },
        { "search.syn.param:circuit_pot_taper", "audio taper|linear|log" },
        { "search.syn.param:circuit_active",  "active pickups|preamp|battery|emg" },
        { "search.syn.param:cable_length",    "lead|cord|capacitance" },
        { "search.syn.param:cable_quality",   "lead|cord|capacitance" },
        { "search.syn.param:cable_on",        "lead|cord" },
        { "search.syn.param:amp_input_impedance", "load|impedance|input" },
        { "search.syn.param:guitar_volume",   "volume knob|level" },
        { "search.syn.param:guitar_tone",     "tone knob|brightness" },
        { "search.syn.param:pickup_selector", "switch|toggle|neck|bridge|middle|position" },
        { "search.syn.param:pickup_blend",    "mix|pickups" },
        { "search.syn.param:coil_tap",        "split|single coil|humbucker" },
        { "search.syn.param:piezo_mic_blend", "acoustic|piezo|mic" },
        { "search.syn.param:concert_a",       "tuning reference|pitch|a440|reference pitch|hz" },
        { "search.syn.param:tuning_preset",   "drop d|open g|dadgad|standard|tuning" },
        { "search.syn.param:temperament",     "tuning system|just|equal|sweetened" },
        { "search.syn.param:capo_fret",       "capo|transpose" },
        { "search.syn.param:string_material", "strings|nickel|steel|nylon|bronze" },
        { "search.syn.param:string_gauge",    "strings|thickness|gauge|heavy|light" },
        { "search.syn.param:string_age",      "old strings|dead strings|new strings|brightness" },
        { "search.syn.param:realism_detune",  "detune|out of tune|imperfection" },
        { "search.syn.param:intonation_error", "intonation|sharp|flat" },
        { "search.syn.param:tuning_drift",    "drift|out of tune" },
        { "search.syn.param:fret_buzz",       "buzz|rattle|action" },
        { "search.syn.param:sustain_scale",   "sustain|decay|ring" },
        { "search.syn.param:coupling_amount", "sympathetic|resonance|coupling" },
        { "search.syn.param:use_fingers",     "fingerstyle|fingerpicking|pluck|no pick" },
        { "search.syn.param:pick_material",   "plectrum|pick" },
        { "search.syn.param:pick_thickness",  "plectrum|gauge|heavy pick|light pick" },
        { "search.syn.param:pick_angle",      "plectrum|attack angle" },
        { "search.syn.param:pluck_position",  "pick position|bridge|neck|plucking point" },
        { "search.syn.param:nail_vs_flesh",   "nail|flesh|fingers" },
        { "search.syn.param:noise_slide",     "finger noise|squeak" },
        { "search.syn.param:noise_fret",      "fret noise|clack" },
        { "search.syn.param:noise_release",   "release noise|lift" },
        { "search.syn.param:noise_body_knock", "knock|body tap" },
        { "search.syn.param:noise_pick",      "pick noise|attack" },
        { "search.syn.param:noise_amp_buzz",  "hum|buzz|mains|60 hz|50 hz" },
        { "search.syn.param:squeak_amount",   "finger squeak|string squeak|noise" },
        { "search.syn.param:body_mode",       "body type|acoustic|electric|resonance" },
        { "search.syn.param:body_amount",     "body resonance|woody|acoustic" },
        { "search.syn.param:body_width",      "body size|dreadnought|parlor" },
        { "search.syn.param:body_bracing",    "bracing|x brace|ladder" },
        { "search.syn.param:body_top_wood",   "spruce|cedar|tonewood|top" },
        { "search.syn.param:body_back_wood",  "mahogany|rosewood|maple|tonewood" },
        { "search.syn.param:body_air_gain",   "air|helmholtz|soundhole|boom" },
        { "search.syn.param:bridge_type",     "whammy|tremolo|vibrato bar|hardtail|floyd|bigsby" },
        { "search.syn.param:whammy_position", "whammy bar|tremolo arm|vibrato arm|dive" },
        { "search.syn.param:whammy_down_range", "dive|dive bomb|whammy" },
        { "search.syn.param:whammy_up_range", "pull up|whammy" },
        { "search.syn.param:transpose_lock",  "whammy|lock" },
        { "search.syn.param:playing_mode",    "mode|lead|rhythm|chords|strum" },
        { "search.syn.param:mpe_enabled",     "mpe|expression|per note" },
        { "search.syn.param:bend_range",      "pitch bend|whammy|range|semitones" },
        { "search.syn.param:strum_direction", "downstroke|upstroke|strum" },
        { "search.syn.param:chord_window",    "strum|chord detection" },
        { "search.syn.param:vibrato_rate",    "vibrato speed|lfo" },
        { "search.syn.param:vibrato_depth",   "vibrato amount|wobble" },
        { "search.syn.param:legato_window",   "hammer on|pull off|legato|slur" },
        { "search.syn.param:slide_guitar",    "bottleneck|slide|lap steel" },
        { "search.syn.param:ebow_enable",     "ebow|sustainer|infinite sustain" },
        { "search.syn.param:freeze_enable",   "freeze|hold|drone|sustain" },
        { "search.syn.param:feedback_amount", "feedback|howl|sustain|amp feedback" },
        { "search.syn.param:cab_type",        "cabinet|speaker cab|4x12|2x12|1x12" },
        { "search.syn.param:cab_speaker",     "speaker|driver|greenback|alnico" },
        { "search.syn.param:cab_on",          "cabinet|speaker" },
        { "search.syn.param:mic_type",        "microphone|sm57|condenser|ribbon" },
        { "search.syn.param:mic_position",    "microphone|placement|cone|edge" },
        { "search.syn.param:mic_distance",    "microphone|distance|close|far" },
        { "search.syn.param:dual_mic",        "two mics|second mic" },
        { "search.syn.param:mic_blend",       "microphone mix|mic mix" },
        { "search.syn.param:hum_timing",      "humanize|timing|groove|sloppy" },
        { "search.syn.param:hum_velocity",    "humanize|dynamics" },
        { "search.syn.param:preset_morph_position", "morph|crossfade|blend presets" },
        { "search.syn.param:slide_mode",      "bottleneck|slide" },
        { "search.syn.param:scrape_armed",    "pick scrape|pick slide|scrape" },
        { "search.syn.param:slap_armed",      "slap|pop|bass slap" },
        { "search.syn.param:strum_crossing_sps", "strum speed|strum time" },
        { "search.syn.param:setup_action_treble", "action|string height|setup" },
        { "search.syn.param:setup_action_bass", "action|string height|setup" },
        { "search.syn.param:setup_relief",    "truss rod|neck bow|setup" },

        // ---- synonyms: places ----------------------------------------------------------
        { "search.syn.place:tab:WORKSHOP",    "parts|build|luthier|swap parts|guitar builder" },
        { "search.syn.place:tab:MOD",         "modulation|lfo|envelope|matrix" },
        { "search.syn.place:tab:RHYTHM",      "strumming|groove|pattern|genre kit|drums" },
        { "search.syn.place:tab:TUNE",        "song|compose|arrange|sketch|tune builder" },
        { "search.syn.place:tab:JAM",         "jam mode|jam band|band|backing band|drums|drummer|bass player" },   // FEAT-JAM
        { "search.syn.place:tab:LIVE",        "snapshots|setlist|stage|performance" },
        { "search.syn.place:tab:ROUTING",     "sidechain|aux|outputs|signal flow" },
        { "search.syn.place:tab:TONE MATCH",  "ir|impulse response|capture|profile|match" },
        { "search.syn.place:tab:CHARACTER",   "pick|squeak|buzz|slide|techniques|feel" },
        { "search.syn.place:tab:PRACTICE",    "routine|practice|exercise" },
        { "search.syn.place:tab:NOTATION",    "sheet music|score|tab|tablature|export" },
        { "search.syn.place:tab:MIDI OUT",    "midi export|drag|capture" },
        { "search.syn.place:tab:CONTROLLERS", "controller|cc|footswitch|midi map|learn" },
        { "search.syn.place:tab:HELP",        "manual|docs|documentation|shortcuts" },
        { "search.syn.place:section:ROOM",    "reverb|ambience|space" },
        { "search.syn.place:section:CAB",     "cabinet|speaker|microphone|mic" },
        { "search.syn.place:section:AMP",     "amplifier|head|gain|eq" },
        { "search.syn.place:section:CIRCUIT", "wiring|pots|treble bleed|cable" },
        { "search.syn.place:section:SUSTAIN", "feedback|ebow|freeze" },
        { "search.syn.place:overlay:options", "settings|preferences|config" },
        { "search.syn.place:overlay:presetBrowser", "presets|sounds|patches|library" },
        { "search.syn.place:overlay:help",    "manual|docs" },
        { "search.syn.place:overlay:export",  "bounce|render|audio export|wav" },
        { "search.syn.place:overlay:chords",  "chord library|tab display|chords" },
        { "search.syn.place:overlay:debug",   "diagnostics|debug|internals" },
        { "search.syn.place:overlay:saveAs",  "save preset" },
        { "search.syn.place:overlay:workshop", "parts|build" },
        { "search.syn.place:options:AUDIO",   "sample rate|buffer|latency" },
        { "search.syn.place:options:APPEARANCE", "theme|colours|colors|palette|scale|zoom" },
        { "search.syn.place:options:ACCESSIBILITY", "screen reader|keyboard|shortcuts|rebind" },
        { "search.syn.place:options:LOCALIZATION", "language|locale|translation" },
        { "search.syn.place:options:RANGES",  "advanced ranges|unlock|stock range" },
        { "search.syn.place:options:PRIVACY", "telemetry|crash report|data" },

        // ---- synonyms: commands --------------------------------------------------------
        { "search.syn.cmd:panic",             "all notes off|silence|stop|kill" },
        { "search.syn.cmd:toggleSlideMode",   "slide|bottleneck" },
        { "search.syn.cmd:toggleAdvanced",    "easy|advanced|mode" },
        { "search.syn.cmd:newPreset",         "init|blank|start over" },
        { "search.syn.cmd:presetBrowser",     "presets|load preset|open preset" },
        { "search.syn.cmd:options",           "settings|preferences" },
        { "search.syn.cmd:tapTempo",          "bpm|tempo" },
        { "search.syn.cmd:retuneAll",         "tune|retune|character" },
        { "search.syn.cmd:exportMidi",        "midi|export|save midi" },
        { "search.syn.cmd:search",            "find|palette|command palette" },
    };

    return catalog;
}

juce::String english (const juce::String& key)
{
    const auto& c = builtInEnglish();

    if (auto it = c.find (key); it != c.end())
        return it->second;

    return {};
}

bool isLocalised (const juce::String& key)
{
    auto& loc = Localisation::get();
    return loc.hasKey (key) && loc.translate (key) != key;
}

juce::String text (const juce::String& key)
{
    if (isLocalised (key))
        return Localisation::get().translate (key);

    return english (key);
}

juce::String text (const juce::String& key, const std::map<juce::String, juce::String>& values)
{
    auto t = text (key);

    for (const auto& [name, value] : values)
        t = t.replace ("{" + name + "}", value);

    return t;
}

juce::StringArray synonyms (const juce::String& itemId)
{
    const auto key = "search.syn." + itemId;
    juce::StringArray result;

    auto add = [&result] (const juce::String& list)
    {
        juce::StringArray parts;
        parts.addTokens (list, "|", {});
        parts.trim();
        parts.removeEmptyStrings();

        for (const auto& p : parts)
            result.addIfNotAlreadyThere (p);
    };

    if (isLocalised (key))
        add (Localisation::get().translate (key));

    add (english (key));
    return result;
}

juce::StringArray unitWords (const juce::String& unitLabel)
{
    const auto u = unitLabel.trim().toLowerCase();

    if (u == "db")                   return { "decibel" };
    if (u == "hz")                   return { "hertz", "frequency" };
    if (u == "khz")                  return { "kilohertz", "frequency" };
    if (u == "ms")                   return { "time", "milliseconds" };
    if (u == "s" || u == "sec")      return { "time", "seconds" };
    if (u == "st" || u == "semi")    return { "semitone" };
    if (u == "cents" || u == "ct")   return { "cents", "pitch" };
    if (u == "mm")                   return { "millimetres", "distance" };
    if (u == "%")                    return { "percent" };
    if (u == "bpm")                  return { "tempo", "beats per minute" };

    return {};
}

} // namespace luthier::search::SearchCatalog
