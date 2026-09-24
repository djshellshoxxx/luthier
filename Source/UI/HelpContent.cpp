#include "HelpContent.h"
#include "../Accessibility/Accessibility.h"
#include "../Accessibility/Localisation.h"

#include <algorithm>
#include <cstring>

namespace luthier
{

namespace
{
    /*  include.md "Help File": each feature, the workflow, the GUI and what the
        controls do, the version, the licence, the links, installation and preset
        troubleshooting, and the debug button (which is the view's, not text).

        The order is the order a newcomer reads in: start, the window, the
        controls, the four columns left to right, column 4's tabs in 4.4's order,
        then reference material. Paragraphs are not hard-wrapped: the HELP tab
        is 480 points at its narrowest and the overlay 860, and the body wraps to
        whichever it is in. */
    const HelpTopic kTopics[] =
    {
        { "getting-started", "Getting Started", "HELP|welcome|workflow|manual",
          "Luthier synthesises a guitar from physics. There are no samples anywhere in it: every note is a "
          "vibrating string model, coupled through a bridge, coloured by a body, sensed by a pickup and pushed "
          "through an amp, a cabinet and a room.\n\n"
          "THE QUICKEST WAY IN\n"
          "1. Pick an instrument from the selector in the header.\n"
          "2. Pick a style from the list at the bottom of Easy mode, or a preset from the browser ({key:presetBrowser}).\n"
          "3. Press AUDITION ({key:audition}) to hear it without touching a keyboard.\n"
          "4. Play. The six macro knobs cover most of what you will want to change.\n\n"
          "THE WORKFLOW\n"
          "Choose the instrument, shape it, put it through the rig, then play or write with it. "
          "Easy mode is the first two steps on one screen. Advanced mode ({key:toggleAdvanced}) lays the whole "
          "signal path out left to right - instrument, pickups and pedals, amplifier and room - with a workspace "
          "on the right for everything that is not a knob: the Workshop bench, modulation, rhythm, tunes, live "
          "setup, routing, tone matching, character, practice, notation, MIDI out and controllers. Nothing in "
          "Advanced is unreachable from Easy; Advanced just stops summarising.\n\n"
          "Because the model is physical, techniques you never set up still work. Bend into a slide into a "
          "vibrato into a harmonic and it behaves correctly, because the engine knows what a string is rather "
          "than looking for a recording of that combination.\n\n"
          "Every topic in this list is also reachable from the panel it describes: {key:help} opens Help on the "
          "panel you are working in." },

        { "interface", "The Interface", "GUI|window|header|EASY|Easy mode|ADVANCED|Advanced mode|Tone",
          "HEADER (always visible)\n"
          "- Output LED, top left: dark when silent, brightening as the level approaches 0 dBFS, red while the "
          "signal is over.\n"
          "- Instrument: loads a complete guitar - body, woods, pickups, strings, tuning - and its usual amp and "
          "cabinet.\n"
          "- Tuning: the open-string tuning. Per-string tunings are in Advanced.\n"
          "- Preset: the name, with previous and next arrows. Click the name to browse.\n"
          "- File: save, open, import, export, options, randomise, reset.\n"
          "- A / B: two comparison slots. A>B copies the current one across.\n"
          "- Undo / Redo: 64 steps.\n"
          "- Panic: stops every string immediately.\n"
          "- Learn: arms MIDI Learn; the next control you click takes the next CC you move.\n"
          "- ?: this help.\n"
          "- Advanced, Live, Slide, Workshop: the mode switches.\n\n"
          "EASY MODE\n"
          "- Top band: the instrument, drawn from the settings actually in use, and an interactive fretboard. "
          "Click a fret to hear that note; how high in the lane you click sets how hard it is picked. "
          "Right-click a string for mute, capo and scale overlays.\n"
          "- Middle band: six macros - Attack, Body, Drive, Tone, Space, Humanize. Each has a dice (randomise "
          "just this one) and a padlock (leave it alone when randomising).\n"
          "- Bottom band: style, playing mode, the chord readout, AUDITION and export.\n\n"
          "ADVANCED MODE\n"
          "A compressed guitar and fretboard strip across the top, then four columns:\n"
          "- Column 1, Instrument: the strings, the neck, the body and the bridge.\n"
          "- Column 2, Signal capture: pickups, the guitar's circuit, the pedalboard before the amp, the "
          "playing hand and string noise.\n"
          "- Column 3, Amplification: the amp, the effects loop, cabinet and mics, room, sustain, performance, "
          "humanise and master.\n"
          "- Column 4, the workspace: a tab strip, one panel at a time. {key:previousWorkspaceTab} and "
          "{key:nextWorkspaceTab} step the tabs.\n\n"
          "Below 1000 points wide the window cannot fit four columns, so Advanced is unavailable and says so. "
          "The practice drawer ({key:togglePractice}) opens along the bottom in either mode." },

        { "controls", "Controls", "control|knob|right-click|MIDI LEARN|lock|locks|Playing|macros strip",
          "Every control behaves the same way.\n\n"
          "- Drag: adjust, vertically or horizontally, whichever you start with.\n"
          "- Shift-drag: coarse. Ctrl-drag (Cmd on macOS): ultra-fine.\n"
          "- Double-click: reset to default.\n"
          "- Hover: the value replaces the label; a tooltip follows after a moment. Tooltips can be turned off "
          "in Options, APPEARANCE.\n"
          "- Right-click: enter a value, reset, copy, paste, MIDI Learn, lock, randomise, and Modulate to send "
          "any modulation source to it.\n"
          "- Keyboard: the arrow keys adjust the focused control, finer with Shift.\n\n"
          "MIDI LEARN\n"
          "Right-click a control and choose MIDI Learn, or press Learn in the header ({key:midiLearnArm}) and "
          "click the control, then move a knob or pedal on your controller. A small dot marks any control that "
          "has a CC mapped; right-click again to clear it. Sustain and sostenuto are ignored while learning, so "
          "an accidental pedal press cannot steal the mapping.\n\n"
          "LOCKS AND RANDOMISE\n"
          "Randomise ({key:randomise}) gives a new sound every press; each press after the first starts again "
          "from the defaults rather than piling changes on changes. A locked control is left alone. Reset All "
          "({key:resetAll}) puts every setting back to its default." },

        { "instrument", "Column 1: Instrument",
          "Instrument|GUITAR|STRINGS|String Set|Strings|Tuning Realism|Temperament|Selected String|Neck|"
          "Sympathetic|BODY|Body|Bridge|WHAMMY",
          "One row per string at the top: its pitch, its computed tension and a mute square. Click a row to "
          "select the string the detail below describes. The tension turns amber when it leaves the playable "
          "range - the tuning you have asked for would need a string nobody makes. It still plays, nudged into "
          "range.\n\n"
          "- Temperament: the temperament, the concert pitch and the capo.\n"
          "- Body: convolution or modal synthesis, then size, depth, top thickness, sound hole, bracing, woods "
          "and age. In modal mode these move the resonances, because the plate and air-cavity equations run "
          "live.\n"
          "- Strings and string set: material, gauge and age for the set.\n"
          "- Tuning realism: the small detune and intonation error of a real instrument, and tuning drift. At "
          "zero it is machine-perfect.\n"
          "- Selected string: everything about the string chosen above, computed rather than stored.\n"
          "- Neck: the fretless and slide guitar switches, and fret buzz.\n"
          "- Sympathetic: how much undamped strings ring along.\n"
          "- Bridge: the bridge type and the whammy - its position, travel down and up, spring count and "
          "whether the other strings stay in tune." },

        { "signal-capture", "Column 2: Signal Capture",
          "Signal capture|PICKUPS|Pickups|CIRCUIT|Circuit|PRE-FX|PRE-EFFECTS RACK|Pedalboard (before the amp)|"
          "Pedalboard|Playing Hand|String Noise",
          "- Pickups: the selector, then per slot the type, the magnet and the volume, and coil tap. "
          "Position is drawn on the instrument; move it in the Workshop.\n"
          "- Circuit: the guitar's own electronics - volume and tone pots and their taper, the tone capacitor, "
          "the treble bleed, the cable and the amp's input. Turning the guitar's volume down cleans the amp up, "
          "exactly as it does with the real thing. The response curve above the controls shows what the "
          "circuit is doing to the top end.\n"
          "- Pedalboard (before the amp): the pedal slots. Drag to reorder; an empty slot costs nothing.\n"
          "- Playing hand: pick or fingers, pick material, thickness and angle, where along the string you "
          "pluck, and nail against flesh.\n"
          "- String noise: slide squeak, fret noise, release noise, body knock, pick noise and amp buzz, each "
          "with its own amount." },

        { "amplification", "Column 3: Amplification",
          "Amplification|Rig|AMP|Amplifier|POST-FX|POST-EFFECTS RACK|Effects Loop (after the amp)|Effects Loop|CAB|"
          "Cabinet and Mic|Cabinet|ROOM|Room|SUSTAIN|Sustain|Performance|Humanise|Humanize|Master",
          "- Amplifier: the model and its face - gain, bass, mid, treble, presence, master - with the bright, "
          "mid boost and standby switches. Standby mutes the amp.\n"
          "- Effects loop (after the amp): the post-amp pedal slots.\n"
          "- Cabinet and mic: the cabinet and speaker, one or two microphones with position and distance, "
          "their blend and phase.\n"
          "- Room: size, material, blend, decay and width.\n"
          "- Sustain: Freeze captures a window of what is sounding and holds it under what you play next; "
          "E-Bow drives the strings that are still ringing, so it sustains notes you are holding rather than "
          "ones you let go. The feedback row makes the amp sing back into the strings, with a light that shows "
          "when it is taking hold.\n"
          "- Performance: vibrato, strum speed and direction, bend range, the legato and chord windows, MPE.\n"
          "- Humanise: timing, velocity, detune, attack, noise and strum variation.\n"
          "- Master: the output level, the safety limiter and oversampling." },

        { "workspace", "Column 4: The Workspace", "workspace|Column 4|tabs",
          "Column 4 is a tab strip with one panel behind each tab. The tabs, in order:\n\n"
          "WORKSHOP, MOD, RHYTHM, TUNE, LIVE, ROUTING, TONE MATCH, CHARACTER, PRACTICE, NOTATION, MIDI OUT, "
          "CONTROLLERS, HELP.\n\n"
          "Each has its own topic in this list. {key:previousWorkspaceTab} and {key:nextWorkspaceTab} step "
          "through them, wrapping at the ends, and the tab you had open last is the one that opens next time. "
          "WORKSHOP is different from the others: it takes over columns 3 and 4, and the tab strip stays so "
          "you can leave it again." },

        { "workshop", "WORKSHOP", "WORKSHOP|bench|Workshop bench|parts",
          "The bench. Every part of the guitar is a real part you can swap, and the illustration is the "
          "guitar you are playing.\n\n"
          "- Click a part on the illustration, or pick it from the parts drawer below it: body, neck, frets, "
          "nut, bridge, tuners, strings, pickups, wiring, preamp, pick, slide, capo.\n"
          "- Drag pickups along their rail and set their height; the ruler reads millimetres from the saddle.\n"
          "- Hold Alt over a card in the drawer to audition it without committing; click to fit it.\n"
          "- The inspector on the right shows the chosen part's real values. The spectrum delta beneath shows "
          "what the last change did to the sound - small changes look small, because they are.\n"
          "- A to H are eight bench slots for comparing builds.\n"
          "- Save As Guitar ({key:saveGuitarAs}) keeps the result as a guitar of your own.\n\n"
          "Every change is one undo step. In Easy mode the header's Workshop button opens the same bench over "
          "the window." },

        { "mod", "MOD", "MOD|modulation|mod matrix|modulation matrix|LFO|macros",
          "The modulation matrix: a card for the source you are editing, and below it the route table of "
          "what every source drives.\n\n"
          "- Sources: eight LFOs, four envelopes, two step sequencers, two envelope followers, eight macros "
          "and a random source. Pick one from the selector to edit it.\n"
          "- Routes: ADD ROUTE makes one, from a source to a destination with a depth. CLEAR ALL removes "
          "every route.\n"
          "- Two quicker ways to make a route: right-click any control and choose Modulate, or drag a source "
          "card onto the control." },

        { "rhythm", "RHYTHM", "RHYTHM|strum|strumming|fingerpicking|genre kit|chord voicer|bass step grid",
          "Turns held chords into guitar parts.\n\n"
          "- The chord detector names what you hold; the voicer picks a fingering a hand could make, with a "
          "style, a density and a hand position.\n"
          "- Strum and fingerpick editors: one cell per step. Left-click cycles a strum cell's direction; "
          "right-click sets its dynamic, its string mask or deletes it.\n"
          "- Feel: swing, timing, velocity, missed and ghost strings, and how long a strum takes.\n"
          "- Genre kits load a matched pattern, feel and voicing in one go; the dice picks one for you.\n"
          "- Patterns can be loaded, saved and exported, and filtered by tag.\n"
          "- When the guitar is a bass, the step grid is a bass line instead.\n\n"
          "The pattern is part of the preset, not a host parameter, so it travels with the sound." },

        { "tune", "TUNE", "TUNE|tune builder|composition|song|melody|progression",
          "A sketchpad for a whole tune in about three minutes, top to bottom:\n\n"
          "- Header: title, new, load, save, export, tempo, key and mode.\n"
          "- Sections: one tab per section, coloured by its role. Right-click to rename, duplicate, delete, "
          "repeat, tag the role or link its rhythm.\n"
          "- Progression: type chords in shorthand, such as [Verse] Am F C G. It is read as you type, and "
          "anything it cannot read is underlined where it sits.\n"
          "- Rhythm: the genre kit, feel and strum each section plays with.\n"
          "- Melody: a piano roll. Draw notes with the mouse, snapped to the grid and the key; right-click a "
          "note to delete or lock it. Auto, Record and Improvise write one for you, and never touch a locked "
          "note.\n"
          "- Transport: back, play and pause, forward, loop, count-in and metronome.\n\n"
          "With the tab focused, save ({key:save}) and export ({key:export}) act on the tune rather than the "
          "preset, and undo steps through the tune's own history. Playback keeps going when the window is "
          "closed." },

        { "live", "LIVE", "LIVE|live mode|snapshots|snapshot|setlist|morph",
          "The setup surface for playing live. The live strip along the bottom of the window ({key:toggleLiveMode}) "
          "is where you use it on stage; this is where you prepare it.\n\n"
          "- Snapshot bank: 128 pads in a grid, so you can see at a glance which are filled. Select a pad to "
          "capture, recall, rename or clear it.\n"
          "- Setlist: the order you will play them in - add, remove and move entries. {key:setlistPrevious} "
          "and {key:setlistNext} step it.\n"
          "- Crossfade and morph: how one snapshot becomes the next.\n\n"
          "In Live Mode, {key:previousItem} and {key:nextItem} step snapshots rather than presets, and the "
          "digits 1 to 9 (Shift for 10 to 18) recall them directly. Expression pedals are calibrated in "
          "Options, EXPRESSION." },

        { "routing", "ROUTING", "ROUTING|routing|outputs|aux|bus layout|sidechain|per-string outputs",
          "Where the sound goes.\n\n"
          "- Bus layout: stereo or multi-out. Your host chooses it when it loads the plugin; this shows which "
          "you have, and the latency it reports.\n"
          "- Aux buses 1 to 8, each with mute, solo, a gain trim and a meter. Aux 8 carries only the playing "
          "noise - squeak, pick and buzz - so it can be mixed on its own.\n"
          "- Per-string outputs, and the sidechain input, which can feed the amp.\n"
          "- MIDI out: what the plugin sends back to the host - the notes, the rhythm engine, string "
          "activity, macro CCs, tune playback and Luthier's own event classes - and on which channel.\n\n"
          "These are mixing decisions for the session, not controls for a host to automate, so they are not "
          "host parameters." },

        { "tone-match", "TONE MATCH", "TONE MATCH|impulse response|IR|cab match|EQ match|capture",
          "Bring in the sound of something real.\n\n"
          "- IR slots: body, cabinet 1 and cabinet 2. Drop an impulse-response file onto a slot or load one; "
          "clear or reverse it, and choose its channel.\n"
          "- Cab match and EQ match: step-by-step wizards that run in place while you play, one step at a "
          "time with one button to go on.\n"
          "- Capture: records a reference and the plugin's own output in turn, to match one to the other.\n"
          "- IR library: every impulse response Luthier has found, filtered by tag. Rescan after adding "
          "files." },

        /*  Not "STRING NOISE": column 2 has a section by that name as well, and a
            name can pin only one topic. Column 2's is the one on screen in both
            Advanced layouts, and this topic describes the group anyway. */
        { "character", "CHARACTER", "CHARACTER|character|wear|dead spots|fret wear|SETUP|PICK|SLIDE|"
          "environment|aging|ageing",
          "The imperfections that make an instrument this one rather than any one.\n\n"
          "- Character seed: rolls the whole set of imperfections; a new seed is saved with the preset.\n"
          "- Dead spots: a fretboard map of where each string goes dull. Drag a spot to move it or change its "
          "depth.\n"
          "- Fret wear: a strip showing how worn each fret is, also editable.\n"
          "- Tuner drift, aged electronics, body age and environment.\n"
          "- String noise: squeak amount and probability, finger moisture and pressure, per-material profiles "
          "and style presets.\n"
          "- Pick: material, thickness, tip, bevel, wear, angle, and the click, chirp and scrape amounts.\n"
          "- Setup: action, relief, nut depth, fret height and the buzz threshold, with a live buzz map.\n"
          "- Slide: shown only in Slide Mode ({key:toggleSlideMode}).\n\n"
          "A padlock on the tab lights when any of these have been unlocked beyond their stock range." },

        { "practice", "PRACTICE", "PRACTICE|practice|practice drawer|drawer|metronome|looper|routines",
          "The setup side of practice. The drawer along the bottom ({key:togglePractice}) is where you "
          "practise; this tab is for before and after.\n\n"
          "- Progress: practice time per day, per tool, trainer accuracy and your best clean tempo.\n"
          "- Routines: the factory routines and your own. START hands a routine to the drawer.\n"
          "- Defaults: what each tool starts with.\n"
          "- Library: saved loops, sessions, backing tracks and tab files.\n"
          "- Session recorder: how much it keeps, audio or MIDI, and auto-save.\n\n"
          "Nothing starts or stops here: the metronome, looper, track and recorder are run from the drawer." },

        { "notation", "NOTATION", "NOTATION|notation|tab view|tablature|MusicXML|chord history",
          "What you played, written down.\n\n"
          "- Capture: Off, Rolling (always keeps the last minutes, so the take you did not know you wanted is "
          "there) or Armed (clears and records from the next note).\n"
          "- Live tab: the last one to eight bars as tablature, with the chord symbols as they changed. Freeze "
          "holds it still.\n"
          "- Export: MusicXML, Guitar Pro, ASCII tab or MIDI, for the whole take or its last seconds, with "
          "optional quantise. The take itself is never quantised." },

        { "midi-out", "MIDI OUT", "MIDI OUT|midi out|MIDI export|export profile|drag out",
          "MIDI that carries what the guitar did, not just which notes.\n\n"
          "- Export profile: Luthier (every event class - squeak, pick, buzz, slide, strum, character - so "
          "the file plays back identically here) or Generic (plain MIDI any host reads). PPQ, track split and "
          "which classes to include.\n"
          "- Export: the capture, or its last seconds, as a file, or dragged straight out of the plugin onto a "
          "track. Hold Alt while dragging for Generic.\n"
          "- Live MIDI out: the same switches as ROUTING's; change them on either tab." },

        { "controllers", "CONTROLLERS", "CONTROLLERS|controller|controllers|MPE|guitar controller|latency",
          "Setting up the thing you play Luthier with.\n\n"
          "- Profile: presets for MPE keyboards, hex-pickup guitar systems and MIDI guitars. Its notes say "
          "what it expects.\n"
          "- Latency: Measure latency runs a short wizard; the result, the dead zone and the minimum note "
          "length can then be fine-tuned.\n"
          "- Save as my profile keeps your adjustments." },

        { "techniques", "Playing Techniques", "TECHNIQUES|techniques|technique|articulations|CC map",
          "Technique is inferred from what you play, in this order:\n\n"
          "- Palm mute: CC 67.\n"
          "- Pinch harmonic: CC 72.\n"
          "- Natural harmonic: CC 73. Land on a node - the 12th, 7th or 5th fret - and it chimes.\n"
          "- Tap: CC 74, when MPE is off.\n"
          "- Slide guitar: CC 75, or the Slide switch in the header ({key:toggleSlideMode}).\n"
          "- Muted picking: CC 71.\n"
          "- Slide: two notes on one string closer together than the legato window, or CC 65 held.\n"
          "- Hammer-on: a higher note on a ringing string, below the legato velocity. Pull-off: the same, "
          "going down.\n"
          "- Pluck: everything else.\n\n"
          "- Pitch bend bends the string by changing its tension; the mod wheel and aftertouch set vibrato "
          "depth; breath or CC 2 is the whammy bar.\n"
          "- Sustain lets every string ring; sostenuto holds only what was already down.\n\n"
          "A legato move never re-picks the string: the vibration carries through and only the pitch changes, "
          "which is what makes a slide sound like one note rather than two." },

        { "modes", "Playing Modes", "playing mode|mode|mono|poly|guitar controller mode",
          "MONO / LEAD\n"
          "Every note goes to one string, chosen to keep the hand near where it already is. Overlapping notes "
          "become hammer-ons, pull-offs or slides. Best for solos.\n\n"
          "POLY / CHORD\n"
          "Chords are voiced across the strings by a search that only returns fingerings a hand could make: one "
          "note per string, a reachable fret span, and pitch order following string order. They are strummed, "
          "not triggered at once, and the strum varies a little every time. The mode carries a small latency - "
          "the chord window, 2 ms by default - so a chord split across a buffer boundary still voices as a "
          "chord. It is reported to your host.\n\n"
          "GUITAR CONTROLLER\n"
          "MIDI channel 1 is the high E, channel 2 the B, and so on, for hex-pickup systems. Per-string bend and "
          "pressure work natively. Turn MPE on in Advanced for expressive keyboards." },

        { "presets", "Presets", "PRESET BROWSER|preset|presets|preset browser|save|A / B",
          "Presets are plain JSON files with the extension .luthierpreset, so they are readable, diffable and "
          "safe to keep in version control.\n\n"
          "- Browse: {key:presetBrowser}, or click the preset name. Categories, search and tags.\n"
          "- Save: {key:save}. Save as: {key:saveAs}. New preset (loads Init): {key:newPreset}.\n"
          "- Show the file on disk: {key:revealPreset}.\n"
          "- Compare: {key:abCompare} flips between the A and B slots.\n\n"
          "A factory preset is never overwritten. Saving one makes a copy in your user folder, so you cannot "
          "lose the original.\n\n"
          "IF PRESETS DO NOT APPEAR\n"
          "1. Open Options, FILE LOCATIONS, and press Rescan presets.\n"
          "2. Check the file is in one of the folders listed there, and that its extension is exactly "
          ".luthierpreset. Windows hides extensions by default, so a file that looks right can end in .txt.\n"
          "3. Sub-folders become categories: a preset in User/Metal is filed under Metal, one loose in User under "
          "User.\n"
          "4. Add any other folder with Add a preset folder.\n"
          "5. Open the file in a text editor. It should start with { and name its format as luthierpreset; a "
          "truncated file is skipped.\n\n"
          "If the factory presets themselves are missing, Debug Tools' Reset all settings and clear caches "
          "writes them again. It does not delete your own presets." },

        { "export", "Export", "export|export audio|render|bounce|wav",
          "AUDIO\n"
          "File, Export audio ({key:export}), or the Export button in Easy mode. Render the audition phrase or a "
          "MIDI file you choose. Format, bit depth, sample rate, tail length and normalisation are yours to set. "
          "The render runs on its own thread through a second, offline copy of the plugin with your exact "
          "settings, so what you get is what you heard, reverb tail included. When it finishes you are told "
          "where the file went, its name, how long it is and at what quality.\n\n"
          "MIDI\n"
          "Luthier keeps the last minutes of what you played. File, Save last MIDI take writes it out even "
          "though you never pressed record. NOTATION and MIDI OUT export the same capture as notation or as "
          "MIDI with every event class." },

        { "options", "Options", "OPTIONS|options|AUDIO|MIDI|APPEARANCE|ACCESSIBILITY|LOCALIZATION|EXPRESSION|"
          "RANGES|UPDATES|PRIVACY|FILE LOCATIONS|settings|palette|ui scale|tooltips",
          "Options ({key:options}) is one overlay with a tab per page:\n\n"
          "- AUDIO and MIDI: devices, in the standalone application.\n"
          "- APPEARANCE: palette, UI scale, reduced motion, tooltips on or off.\n"
          "- ACCESSIBILITY: screen-reader verbosity, the font, and the full shortcut table where every "
          "shortcut can be rebound ({key:showShortcuts} opens it).\n"
          "- LOCALIZATION: the language.\n"
          "- EXPRESSION: expression-pedal calibration.\n"
          "- RANGES: whether this preset may go beyond stock ranges.\n"
          "- UPDATES and PRIVACY: update checks and what, if anything, is ever sent.\n"
          "- DIAGNOSTICS: the debug window, crash logging and the troubleshooting file.\n"
          "- FILE LOCATIONS: every folder Luthier reads or writes, with a button to open each one in your "
          "file browser, and Rescan presets." },

        { "shortcuts", "Keyboard Shortcuts", "shortcuts|shortcut|keys|keyboard|hotkeys|Show all shortcuts",
          "The list beside this topic is live: it is read from the shortcut table every time a binding "
          "changes, so what it shows is what the keys do now.\n\n"
          "Every shortcut can be rebound in Options, ACCESSIBILITY - Rebind... opens it there. A rebound "
          "shortcut is marked in the list. Escape always closes the overlay that is open and cannot be "
          "rebound, so a clumsy rebind can never trap you in a dialog. The digit keys recall snapshots and "
          "are fixed too.\n\n"
          "On any control, mouse modifiers do not change: Shift-drag is coarse, Ctrl-drag (Cmd on macOS) is "
          "ultra-fine, double-click resets and right-click opens the control's menu." },

        { "troubleshooting", "Troubleshooting", "troubleshooting|install|installation|uninstall|no sound|crash|"
          "crackles|cpu",
          "THE PLUGIN DOES NOT APPEAR IN MY HOST\n"
          "Check the plugin is where your host looks for it:\n"
          "- Windows VST3: C:\\Program Files\\Common Files\\VST3\\Luthier.vst3\n"
          "- macOS VST3: ~/Library/Audio/Plug-Ins/VST3/Luthier.vst3\n"
          "- macOS AU: ~/Library/Audio/Plug-Ins/Components/Luthier.component\n"
          "- Linux VST3: ~/.vst3/Luthier.vst3\n"
          "Luthier.vst3 is a folder, not a file: copy the whole thing, not its contents. If the installer "
          "could not put it there, copy it there by hand. Then rescan in your host; most hosts cache their "
          "plugin list, and one that crashed while scanning keeps a list of plugins it will not try again, "
          "which needs clearing too. Luthier is 64-bit only.\n\n"
          "TO UNINSTALL BY HAND\n"
          "Delete Luthier.vst3 (and Luthier.component on macOS) from the folder above, and the standalone "
          "application from its folder - on Windows C:\\Program Files\\Luthier. Your presets, guitars, tunes, "
          "renders and diagnostics live in Documents/Luthier and are left alone, so reinstalling loses nothing; "
          "delete that folder too if you want everything gone.\n\n"
          "NO SOUND\n"
          "- Check the track is receiving MIDI: the dot beside the logo lights up.\n"
          "- Check the amp is not on Standby, and that Master is up.\n"
          "- If every pickup is switched off, an electric guitar is silent by design. The switch position is "
          "drawn on the instrument.\n"
          "- Press Panic ({key:panic}) in case a note is stuck.\n\n"
          "CRACKLES OR HIGH CPU\n"
          "- Lower the oversampling, in column 3's Master section. 2x is usually indistinguishable from 4x.\n"
          "- Raise your host's buffer size.\n"
          "- Turn off the second microphone, or empty pedal slots you do not use.\n"
          "- Try the other body mode; which is cheaper depends on your machine.\n\n"
          "PRESETS\n"
          "See the Presets topic: where they live, and what to check when they do not show up.\n\n"
          "IT CRASHES\n"
          "See the Debug Tools topic." },

        { "debug", "Debug Tools", "DIAGNOSTICS|debug|debug tools|crash log|troubleshooting file|hard reset",
          "Open Debug Tools below, or press {key:debugPanel}.\n\n"
          "- The live state view: every string's pitch, level and tension, the signal levels and a self-test, "
          "updating as you play and change settings, with a stream of events beside it.\n"
          "- Create log file on crash: off every time the plugin loads, on purpose. Tick it, reproduce the "
          "crash, and a log named with the date and time is written, with a copy of the troubleshooting file "
          "at the top.\n"
          "- Export troubleshooting file: a one-off snapshot of your settings, your audio and MIDI setup, your "
          "host, the licence, the version and a short self-test. It is safe to share and contains no audio. "
          "Send it first for anything that is merely not working as expected.\n"
          "- Reset all settings and clear caches: the destructive reset, for when nothing else works. It asks "
          "first. It is much stronger than Reset All in the header, and it does not delete your saved "
          "presets.\n\n"
          "If Luthier is hard-crashing, send BOTH the crash log and the troubleshooting file to support, with a "
          "description of what you were doing when it happened." },

        // ==== BEGIN TUNE-HELP-ONBOARDING topics ====
        { "first-steps", "First Steps and the Tour", "tour|take the tour|first run|first-run|onboarding|"
          "welcome banner|restore first-run experience|hints",
          "THREE WAYS IN (onboarding)\n"
          "- 30 seconds: play. The default preset is a finished sound; nothing needs setting up.\n"
          "- 2 minutes: take the tour, then press Randomise a few times for fresh sounds.\n"
          "- 5 minutes: take the tour, open one of the example tunes in the TUNE tab (Load, then the Examples "
          "folder), press play, and swap the guitar in the Workshop while it loops.\n\n"
          "THE TOUR\n"
          "Twelve stops, one callout each, pointing at the control it describes: playing, presets, Advanced "
          "mode, the guitar, the rig, the workspace, the Workshop, the TUNE tab, snapshots, the practice drawer, "
          "Slide Mode and Options. Next and Back move between stops, Skip or Escape ends it, and the Take the "
          "tour button at the top of this page starts it again at any time.\n\n"
          "THE FIRST WEEK\n"
          "For your first seven launches (or seven days, whichever comes first) new things are marked: a ? "
          "pulses the first time you see it, every workspace tab you have not opened yet carries a small dot, "
          "and the Workshop wrench, the TUNE tab and the Slide switch pulse once. After that the hints go "
          "quiet.\n\n"
          "STARTING OVER\n"
          "Options -> Diagnostics -> Restore first-run experience puts your settings back to a fresh install's "
          "and brings back the welcome banner, the tour offer and every one-time hint. Your presets, guitars, "
          "tunes and parts are kept." },

        { "whats-new", "What's New", "what's new|whats new|changelog|release notes|NEW|upgrade",
          "Each release lists what it added in CHANGELOG.md, installed beside this manual. Anything a new "
          "version adds is marked NEW where you reach it for a week after you first run that version.\n\n"
          "RECENT ADDITIONS\n"
          "- The real guitar on screen, drawn from its parts.\n"
          "- The Workshop: swap bodies, necks, pickups, bridges and strings, and hear what changed.\n"
          "- MIDI export with realism events, and live MIDI out.\n"
          "- Feedback that behaves like feedback, and a real E-Bow.\n"
          "- The TUNE tab: sketch a whole tune, with a melody, bass and layers, and export it as audio, MIDI or "
          "notation from one dialog.\n"
          "- The welcome tour, and first-week hints." },
        // ==== END TUNE-HELP-ONBOARDING topics ====

        /*  ==== BEGIN TUNE-HELP-ONBOARDING topics for other workstreams ====
            Features other workstreams add (the TECHNIQUES tab's sub-tabs,
            the CHARACTER groups of the phase 2b specs), each as its own
            topic so the panel's ? or Docs can pin to it by name. Written from
            their specs; kept apart so a workstream can edit its own entry. */
        { "technique-slap", "Techniques: Slap", "Slap|SLAP|Slap sub-tab|slap technique|thumb slap|pop|finger pop|palm slap|body tap",
          "Slap is a strike, not a pluck: the thumb, a popping finger, the palm or a hand on the body hits the "
          "string (or the top) and the string rattles against the frets.\n\n"
          "- Slap type: Thumb Slap, Finger Pop, Palm Slap or Body Tap.\n"
          "- Trigger: a velocity zone, a keyswitch, a CC, an MPE zone or the Playing strip button.\n"
          "- Contact position and force: where along the string it lands and how hard.\n"
          "- String mask: which strings a slap hits.\n"
          "- Ghost mode: the fretting hand mutes first, for a pitchless thump.\n"
          "- Rebound and snap-back: the thumb's double hit, and how hard a bass string clacks back on the "
          "fretboard.\n"
          "- Body tap: which part of the body the hand meets, and so which resonances answer." },

        { "technique-scrape", "Techniques: Scrape", "Scrape|SCRAPE|Scrape sub-tab|string scrape|scraping|pick scrape|zipper",
          "A pick or a nail drawn along a wound string: the tip catches each winding in turn, so the pitch of the "
          "zipper follows how fast you move and how tightly the string is wound. It is a real event on the "
          "string, so it shows up in the string's ring, in the pickups and through the amp exactly as the rest "
          "of the note does. The Scrape controls set how it is triggered, its direction, speed and pressure, "
          "and which strings it crosses." },

        { "technique-muting", "Techniques: Muting", "Muting|MUTING|Muting sub-tab|mute|palm mute|chuka|mute pattern|mute grid",
          "Muting as rhythm: metal chugs, funk chukas, reggae skanks.\n\n"
          "- A 16-step grid: paint each step open, palm-muted, fretting-hand muted or dead.\n"
          "- Master mute mode overrides the grid with one kind of mute for every step.\n"
          "- Palm position and pressure: where the palm rests and how hard.\n"
          "- Fretting-hand style: a rock spread across the strings, or a classical fingertip.\n"
          "- Chuka source: what makes a strum a chuka (by default, a very soft one).\n"
          "- Humanise: the chance a step slips between open and muted, and the ghost-note level." },

        { "technique-tapping", "Techniques: Tapping", "Tapping|TAPPING|Tapping sub-tab|tap|two-hand tapping|taps|hammer-on|pull-off",
          "A tap is a fret event, not a pluck: a right-hand finger presses the string onto a fret sharply and "
          "the string between that fret and the bridge sounds.\n\n"
          "- Trigger: right-hand notes on MIDI channel 2 by default, a keyswitch, or the fretboard's tap layer.\n"
          "- Strength curve: how velocity becomes tap strength.\n"
          "- Auto pull-off: lifting a tap with a fretted note held plays the pull-off.\n"
          "- Hammer-on threshold: how soft a legato note can be and still count as a hammer-on.\n"
          "- Flick, default length, how many taps a string can hold, and snapping taps to frets (off for "
          "microtonal taps)." },

        { "technique-bends", "Techniques: Microtonal Bends", "Microtonal Bends|MICROTONAL BENDS|Microtonal Bends sub-tab|"
          "microtonal|bend quantise|pre-bend|quarter tone|scala",
          "Pitch on a string is continuous, so bends and vibrato are not limited to semitones.\n\n"
          "- Bend source and range, for all strings and per string (MPE Y by default): 200 cents unless you "
          "widen it.\n"
          "- Vibrato: its source, rate, depth and how long after the note it starts.\n"
          "- Bend quantise: none, quarter tones, semitones, 24-EDO or a loaded .scl / .tun scale.\n"
          "- Pre-bend: start a note bent and release into pitch.\n"
          "- Bend and release curves: linear, exponential (as a finger does it) or drawn." },

        { "technique-cascade", "Techniques: Combining Them", "technique cascade|cascade|combining techniques|"
          "technique conflicts|compatibility",
          "Techniques combine the way they do on a real guitar. Each belongs to a class - what controls the "
          "pitch, what excites the string, what damps it - and two that would fight over the same string at "
          "the same moment cannot both be armed on it: the tab greys out the one that conflicts and says why. "
          "Techniques on different strings, or in different classes, always combine." },

        { "string-aging", "Character: String Aging", "STRING AGING|String Aging|string age|fresh strings|dead strings",
          "Fresh strings are bright, zingy and long-sustaining; dead ones are dull, short and play slightly "
          "out of tune up the neck. The STRING AGING group in the CHARACTER tab sets how old the set is and "
          "how it has been played, and the brightness, sustain and intonation follow." },

        { "environment-group", "Character: Environment", "Environment group|ENVIRONMENT group|temperature|humidity|"
          "climate|acclimatise",
          "Steel and wood respond to the room. The ENVIRONMENT group in the CHARACTER tab sets temperature and "
          "humidity: cold strings go flat and drift back as they warm, a humid room lifts an acoustic's top "
          "and its action, a dry one back-bows the neck and wakes fret buzz." },

        { "body-coupling", "Character: Body Coupling", "BODY COUPLING|Body Coupling|wolf note|wolf notes|body feedback",
          "The strings drive the body and the body pushes back. The BODY COUPLING group in the CHARACTER tab "
          "sets how strongly, which is what gives an acoustic its wolf notes - a note on the top's main "
          "resonance that blooms, warbles or dies early - and lets the body's own resonances ring on." },

        { "harmonics-group", "Character: Harmonics", "HARMONICS|Harmonics|harmonic|natural harmonic|touch harmonic",
          "A harmonic is a string touched at a node: every partial with a node there survives and the rest die "
          "within a few periods. The HARMONICS row in the CHARACTER tab's PICK group sets the touch - its "
          "pressure, the finger's width, how long it rests and how it leaves - which decides how pure the "
          "harmonic is and how much of the fundamental leaks through." },

        { "right-hand", "Character: Right Hand", "RIGHT HAND|Right Hand|fingerstyle|rest stroke|free stroke|nail|thumb",
          "What touches the string and how it lets go. The RIGHT HAND group in the CHARACTER tab sets nail "
          "against flesh, rest or free stroke, the thumb, Travis-style muting, hybrid picking snap and how "
          "much each finger varies - the difference between a classical player, a Travis picker and a "
          "flamenco player." },

        { "noise-floor", "Character: Noise Floor", "NOISE FLOOR|Noise Floor|hum|hiss|buzz|60 cycle|noise gate",
          "A real rig is never silent. The NOISE FLOOR group in the CHARACTER tab adds pickup hum, preamp hiss "
          "and the cable and room, each where it enters a real chain, so the guitar's volume, the pickup type "
          "and the amp's gain act on it as they do on a real rig. All of it can be turned off." },

        { "sustain-shape", "Character: Sustain Shape", "SUSTAIN SHAPE|Sustain Shape|decay|two-stage decay|"
          "pitch sag|release",
          "A plucked note falls fast for the first half second and then settles into a long tail, starts a "
          "touch sharp, and sags as the finger lifts at the end. The SUSTAIN SHAPE group in the CHARACTER tab "
          "sets each of those stages." },

        { "tuning-stability", "Character: Tuning Stability", "TUNING STABILITY|Tuning Stability|string stretch|"
          "nut binding|tuner slip|floating bridge detune",
          "Guitars go out of tune for reasons: new strings stretch flat, a hard bend sticks in the nut and "
          "comes back sharp, a loose tuner lets go, a floating bridge detunes the other strings when one is "
          "bent, a capo pulls everything sharp. The tuning-stability controls in the CHARACTER tab set how "
          "much of each your guitar does." },
        // ==== END TUNE-HELP-ONBOARDING topics for other workstreams ====

        { "about", "About and Licence", "about|licence|license|version|links|support|homepage|github|source",
          "Luthier - a physically-modelled guitar. No samples.\n\n"
          "(c) Luthier Audio. All rights reserved.\n\n"
          "Built with JUCE, used under the terms of the JUCE licence that applies to this build.\n\n"
          "LICENCE\n"
          "This copy of Luthier is licensed, not sold. You may install and use it on the machines you "
          "personally work on. You may not redistribute the plugin, nor reverse engineer it, except where that "
          "right cannot be excluded by law. Presets you create are yours, and audio you render with it is "
          "yours, with no further obligation. The plugin is provided as-is, without warranty of any kind.\n\n"
          "Full third-party notices are in THIRD_PARTY_LICENCES.txt beside the plugin.\n\n"
          "The buttons below open the homepage, the source and a support email. If a link does not open, the "
          "addresses here can be copied by hand." }
    };

    constexpr int kNumTopics = (int) (sizeof (kTopics) / sizeof (kTopics[0]));

    /** "Tone-Match  tab" -> "tone match". */
    juce::String normalise (juce::String name)
    {
        name = name.replaceCharacter ('-', ' ').replaceCharacter ('_', ' ').toLowerCase().trim();

        for (const char* suffix : { " tab", " panel" })
            if (name.endsWith (suffix))
                name = name.dropLastCharacters ((int) std::strlen (suffix)).trim();

        juce::StringArray words;
        words.addTokens (name, " ", {});
        words.removeEmptyStrings();

        return words.joinIntoString (" ");
    }

    /*  The cheat sheet's groups. An action is listed under the first group that
        names it; an action no group names is still listed, under "Other" - the
        rows come from the registry, and this only decides where they sit. */
    struct Group { const char* name; const char* actions; };

    const Group kGroups[] =
    {
        { "Help and navigation", "help|showShortcuts|options|toggleAdvanced|previousWorkspaceTab|"
                                 "nextWorkspaceTab|debugPanel" },
        { "Playing",             "panic|killSwitch|tapTempo|audition|toggleLiveMode|toggleSlideMode|"
                                 "togglePractice|midiLearnArm" },
        { "Presets, snapshots and setlists", "previousItem|nextItem|setlistPrevious|setlistNext|abCompare|"
                                 "randomise|resetAll|newPreset|presetBrowser" },
        { "Files and editing",   "undo|redo|save|saveAs|revealPreset|saveGuitarAs|revealGuitar|export" }
    };

    constexpr const char* kOtherGroup = "Other";

    juce::String groupFor (const juce::String& actionId)
    {
        for (const auto& group : kGroups)
        {
            juce::StringArray ids;
            ids.addTokens (group.actions, "|", {});

            if (ids.contains (actionId))
                return group.name;
        }

        return kOtherGroup;
    }

    int groupIndex (const juce::String& groupName)
    {
        int index = 0;

        for (const auto& group : kGroups)
        {
            if (groupName == group.name)
                return index;

            ++index;
        }

        return index;   // "Other" last
    }
}

//==============================================================================
int HelpContent::getNumTopics() noexcept
{
    return kNumTopics;
}

const HelpTopic& HelpContent::getTopic (int index) noexcept
{
    return kTopics[juce::jlimit (0, kNumTopics - 1, index)];
}

juce::StringArray HelpContent::getAliases (const HelpTopic& topic)
{
    juce::StringArray aliases;
    aliases.addTokens (topic.aliases, "|", {});
    aliases.trim();
    aliases.removeEmptyStrings();
    return aliases;
}

int HelpContent::findTopic (const juce::String& nameOrAlias)
{
    const auto wanted = normalise (nameOrAlias);

    if (wanted.isEmpty())
        return -1;

    // Ids and titles first, so an alias can never shadow a topic's own name.
    for (int i = 0; i < kNumTopics; ++i)
        if (normalise (kTopics[i].id) == wanted || normalise (kTopics[i].title) == wanted)
            return i;

    for (int i = 0; i < kNumTopics; ++i)
        for (const auto& alias : getAliases (kTopics[i]))
            if (normalise (alias) == wanted)
                return i;

    return -1;
}

juce::String HelpContent::resolveKeys (const juce::String& text)
{
    const juce::String open ("{key:");
    juce::String result;
    int from = 0;

    for (int at = text.indexOf (open); at >= 0; at = text.indexOf (from, open))
    {
        const int close = text.indexOfChar (at, '}');

        if (close < 0)
            break;

        result << text.substring (from, at);

        const auto actionId = text.substring (at + open.length(), close);
        const auto* binding = AccessibilitySettings::get().findShortcut (actionId);

        result << (binding != nullptr && binding->key.isValid() ? binding->key.getTextDescription()
                                                                : juce::String ("(not bound)"));
        from = close + 1;
    }

    result << text.substring (from);
    return result;
}

//==============================================================================
std::vector<ShortcutRow> HelpContent::getShortcutRows (const juce::String& filter)
{
    std::vector<ShortcutRow> rows;

    for (const auto& binding : AccessibilitySettings::get().getShortcuts())
    {
        ShortcutRow row;
        row.group = groupFor (binding.id);
        row.actionId = binding.id;
        row.keyText = binding.key.isValid() ? binding.key.getTextDescription() : juce::String ("(not bound)");
        row.description = tr (binding.descriptionKey);
        row.rebound = binding.isRebound();
        rows.push_back (row);
    }

    /*  The keys PluginEditor::keyPressed answers without the registry. Escape
        (accessibility 2: the way out of any dialog, so never rebindable) and the
        snapshot digits (live-performance 2; positional, so GAPS.md keeps them out
        of the table). Listed so the sheet is every key, not every rebindable
        key. */
    auto fixed = [&rows] (const char* group, const char* key, const char* description)
    {
        ShortcutRow row;
        row.group = group;
        row.keyText = key;
        row.description = description;
        row.fixed = true;
        rows.push_back (row);
    };

    fixed ("Help and navigation", "escape", "Close the open overlay (fixed)");
    fixed ("Presets, snapshots and setlists", "1 - 9", "Recall snapshot 1 to 9 (fixed)");
    fixed ("Presets, snapshots and setlists", "shift + 1 - 9", "Recall snapshot 10 to 18 (fixed)");

    std::stable_sort (rows.begin(), rows.end(), [] (const ShortcutRow& a, const ShortcutRow& b)
    {
        return groupIndex (a.group) < groupIndex (b.group);
    });

    const auto query = filter.trim();

    if (query.isNotEmpty())
    {
        rows.erase (std::remove_if (rows.begin(), rows.end(), [&query] (const ShortcutRow& row)
        {
            return ! (row.description.containsIgnoreCase (query)
                        || row.keyText.containsIgnoreCase (query)
                        || row.actionId.containsIgnoreCase (query));
        }), rows.end());
    }

    return rows;
}

juce::String HelpContent::formatShortcutRows (const std::vector<ShortcutRow>& rows)
{
    juce::String text;
    juce::String group;

    for (const auto& row : rows)
    {
        if (row.group != group)
        {
            group = row.group;
            text << (text.isEmpty() ? "" : "\n") << group.toUpperCase() << "\n";
        }

        text << "  " << row.keyText.paddedRight (' ', 22) << "  " << row.description
             << (row.rebound ? "  (rebound)" : "") << "\n";
    }

    return text;
}

juce::URL HelpContent::getSupportMailUrl()
{
    return juce::URL (juce::String ("mailto:") + supportEmail + "?subject=Luthier%20" JucePlugin_VersionString);
}

} // namespace luthier
