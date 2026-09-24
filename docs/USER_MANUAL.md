# Luthier - user manual

---

## What this is

Luthier synthesises a guitar from physics. There are no samples anywhere in it.
Every note is a vibrating string model, coupled to the other strings through a
bridge, coloured by a body, sensed by a pickup, and pushed through an amp, a
speaker and a room.

That matters for one practical reason: **techniques you never set up still work**.
Bend into a slide into a vibrato into a harmonic and it behaves correctly, because
the engine knows what a string is rather than looking for a recording of that
combination.

---

## Getting started

1. Pick an instrument from the selector at the top left.
2. Pick a **Style** from the dropdown at the bottom of Easy mode.
3. Press **AUDITION** to hear it without touching a keyboard.
4. Play.

Six macro knobs cover most of what you will want to change. When you want more,
press **Advanced** at the top right - nothing is hidden there that is not also
reachable from Easy mode; Advanced just stops summarising.

---

## The header

Always visible, in both modes.

| | |
|---|---|
| **Output LED** | top-left corner. Dark when silent, brightening to white as the level approaches 0 dBFS, red while the signal is over. |
| **Instrument** | loads a complete guitar: body, woods, pickups, strings, tuning and its usual amp and cabinet. |
| **Tuning** | open-string tuning. Per-string tunings live in Advanced. |
| **Preset** | name, with prev/next arrows. Click the name to browse. |
| **Padlock** | Advanced mode only, and only when this preset has advanced ranges unlocked. Opens Options, RANGES. |
| **File** | save, save as, open, import and export a preset; **New Tune...** and **Import MIDI...** for the Tune Builder; export audio, save the last MIDI take, export notation; open the preset and render folders; options; randomise; reset. |
| **A / B** | two comparison slots. `A>B` copies the current one across. |
| **Undo / Redo** | 64 steps. |
| **Panic** | stops every string immediately and clears every tail - room, delays, freeze, feedback, the rhythm engine's pending strums. Your settings are untouched. `P`. |
| **RESET & STOP** | beside Panic, in the same warning colour: the heavy version. Stops the tune player, the looper, the backing track, the metronome, the rhythm engine, the practice runner and the kill switch, returns every setting to its default, then panics and resets the engine. `Ctrl + Shift + P`. It is one undo step, so `Ctrl + Z` brings the settings back. Reach for it when a loop keeps re-feeding the strings and Panic alone will not end it. |
| **Learn** | arms MIDI Learn: the next control you click is assigned to the next CC you move. |
| **?** | help. In Advanced mode it opens the HELP tab on the panel you were using; in Easy mode, the same help over the window. |
| **Advanced** | switches modes. Locked while Live Mode is on. |
| **Live** | Live Mode: the live strip along the bottom - snapshots, setlist, tap tempo, morph, the kill switch. |
| **Slide** | Slide Mode: play with a bar instead of frets. |
| **Workshop** | the Workshop bench: the WORKSHOP tab in Advanced mode, over the window in Easy mode. |

A small dot beside the logo lights up when MIDI arrives.

---

## Easy mode

### Top band - the instrument

The guitar illustration is **generated from the settings actually in use**, so it
is always correct. Move a pickup in Advanced and it moves in the picture. The pole
pieces glow with what each string is doing.

- Click a pickup to select it.
- Click the selector switch to advance its position.
- Drag the volume and tone knobs on the body.

Easy mode shows the whole guitar. The Advanced strip crops to the body, which is
why the picture looks different there - see below.

The **fretboard** below shows every string and fret. Notes light up as they sound -
and what is lit is what is actually ringing, including notes the chord voicer put
somewhere you did not expect, which is exactly when you want to see it.

- Click a fret to hear that note. How high in the string's lane you click sets how
  hard it is picked.
- Right-click for mute, capo, string selection and scale overlays.

Fret spacing follows the real rule, so the frets crowd together up the neck exactly
as they do on the instrument.

To the right, a scrolling readout shows what is happening inside the plugin - MIDI,
parameter changes, engine events. It stops when nothing is happening.

### The rig strip - pedals

The **Pre-effects** and **Post-effects** cards are the two pedal racks, before
and after the amp, eight slots each, shown as numbered pills. A rack is empty
until you choose a pedal: click a slot and the same pedal editor Advanced mode
uses opens as a popover over it. Pick a pedal from its menu and the popover
grows to show the pedal's whole face.

The footswitch on every pedal's face is its **bypass**, labelled ON or BYPASS
so you can see which way it is. A pedal you have just picked and cannot hear is
usually one whose footswitch is on BYPASS - click it.

### Middle band - the macros

| Macro | What it does |
|---|---|
| **Attack** | pick attack, soft and round to sharp and bright. Moves the contact bandwidth of whatever is touching the string. |
| **Body** | how much of the guitar's body you hear. Never fully off - even a solid body colours the sound. |
| **Drive** | amp gain. Adds to the amp's own control rather than replacing it. |
| **Tone** | global tone, dark to bright. Moves the guitar's tone control and the amp's treble together. |
| **Space** | room and ambience. |
| **Humanize** | timing, velocity, tuning and attack variation. At zero the plugin is machine-perfect. |

Under each: a **dice** (randomise just this one) and a **padlock** (exclude it from
Randomise).

Macros multiply the detailed controls rather than replacing them, so Advanced edits
survive a macro move.

### Bottom band - style and play

- **Style** - the factory bank, grouped by category.
- **Fingers** - play with the fingers instead of a pick: a rounder, softer attack
  with no pick click, measurably darker at the string. Acoustic and classical
  guitars start with it on. The full playing-hand controls are in Advanced,
  column 2.
- **Mode** - Mono, Poly or Guitar Controller.
- **Chord readout** - names what you are playing.
- **Audition** - plays a phrase through the current sound. Pick which phrase beside
  it.
- **Export**, **Randomise**, **Reset**.

---

## Advanced mode

A strip across the top - the guitar, and beside it either the fretboard or the
**string roll** - then four columns laid out the way the signal flows: the
instrument, what picks it up, what amplifies it, and a workspace for everything
that is not a knob. Columns 1 to 3 scroll on their own.

**The strip.** The guitar is drawn at the size the strip allows, and when the
whole instrument would be too small to read it is cropped to the body and the
last five frets, with the neck running off the left edge. Nothing has moved:
pickups, switch and knobs still work, and when the headstock is out of frame
the visible stub of the neck opens the tuning. Easy mode always shows the whole
guitar. The **FRETS | ROLL** toggle at the strip's edge swaps the fretboard for
the string roll (described under NOTATION below); the choice is remembered.

**Scrolling a column.** The mouse wheel scrolls the column, even when the
pointer is over a knob. Hold `Ctrl` (`Cmd` on macOS) and the wheel nudges the
knob under the pointer instead. A chevron at the top or bottom of a column
means more controls lie that way - hover it for the hint, click it to page
there.

Advanced mode needs a window at least 1000 points wide. Narrower than that, the
switch is disabled and says why. Below 1280 points, columns 2 and 3 share one
slot, one above the other.

### Column 1 - Instrument

**Temperament.** The temperament, the concert pitch (A4) and the capo.

**Body.** Convolution or modal synthesis, the amount, and then the dimensions -
size, depth, top thickness, sound hole, bracing, woods, age. In Modal mode these
genuinely move the resonances, because the plate and Helmholtz equations are
evaluated live.

**Strings.** One row per string: its pitch, its computed tension, and a mute
square. Click a row to select the string.

**The tension readout turns amber when it leaves the playable range.** That is the
engine telling you the tuning you have asked for would need a string no
manufacturer makes. It will still play - the value is nudged into range - but the
warning is there.

**String set.** Material, gauge and age for the set, and the sustain scaling.

**Tuning realism.** Realism detune, intonation, and tuning drift - which lets the
guitar slowly go out of tune while you play.

**Selected string.** Everything about the selected string, computed rather than
stored: gauge in inches and millimetres, whether it is wound, its core diameter,
its mass per metre, its tension, its scale length, its inharmonicity coefficient,
its T60, its brightness and how much it squeaks.

**Neck.** Fretless and slide guitar toggles, and **Contact** - how much a string
loses when it slaps a fret. Where and when it buzzes is the setup's job, in the
CHARACTER tab's SETUP group.

**Sympathetic.** How strongly the strings ring each other through the bridge.

**Bridge.** The bridge type, then the whammy: bar position, down and up range,
spring ring, and the transposing-tremolo detent.

### Column 2 - Signal capture

**Pickups.** The selector, then per slot: type, magnet and volume; coil tap; and
the piezo / mic balance for acoustic instruments. The slots follow the guitar
that is fitted: a slot the guitar has no pickup in is greyed out and labelled
**not fitted**, the fitted ones say Bridge, Middle or Neck, and the selector
offers only the positions the guitar can realise. To fit more pickups, or move
one, use the Workshop bench - where a pickup sits and how high is set there, by
dragging it.

**Circuit.** The guitar's own electronics, and a curve that shows what they are
doing to the top end: the volume and tone controls, pot values and taper, the tone
capacitor, treble bleed, an active buffer, and the cable - its length and quality -
into the amp's input. Turning the guitar's volume down cleans the amp up, as it
does on the real thing.

**Pedalboard (before the amp).** The pre-amp pedal slots. A slot is empty until
you choose a pedal from its menu, and a picked pedal is built and audible at
once. The footswitch on the pedal's face is its bypass, labelled ON or BYPASS;
hover it and the tooltip says which way a click will take it. Drag a slot onto
another to reorder; right-click to clear it or reset the pedal.

**Playing hand.** Pick or fingers, material, thickness, angle, position, nail
versus flesh.

**String noise.** Slide squeak, fret click, release thump, body knock, pick attack,
amp buzz - each with its own control.

### Column 3 - Amplification

**Amplifier.** The model and its face: gain, bass, mid, treble, presence and
master, with the bright, mid boost and standby switches.

**Effects loop (after the amp).** The post-amp pedal slots. The doubler lives here,
as a pedal.

**Cabinet and mic.** The cabinet and speaker, one or two microphones with position
and distance, their blend, width and phase.

**Room.** Size, material, blend, decay and width.

**Sustain** holds a note after you have stopped playing it, two different ways.
**Freeze** captures a window of what is sounding and loops it underneath whatever
you play next - switch it on again to capture a new one. **E-Bow** drives the
strings that are still ringing at their own resonance, so it sustains notes you
are still holding rather than ones you have let go. The **feedback** row makes the
amp sing back into the strings, with a light that shows when it is taking hold.

**Performance.** Vibrato, strum speed and direction, bend range, the legato and
chord windows, and MPE.

**Humanise.** Timing, velocity, detune, attack, noise and strum variation.

**Master.** Output level, the safety limiter, and oversampling.

### Column 4 - The workspace

A tab strip across the top, one panel behind each tab, in this order:

| Tab | What it is for |
|---|---|
| **WORKSHOP** | the bench: swap any part of the guitar, drag pickups, compare builds, Save As Guitar. It takes over columns 3 and 4 while it is open. |
| **MOD** | the modulation matrix - LFOs, envelopes, step sequencers, envelope followers, macros, a random source, and the route table. |
| **RHYTHM** | the chord voicer, the strum and fingerpick pattern editors, feel, and genre kits. A bass step grid when the guitar is a bass. |
| **TUNE** | write a whole tune: sections, a chord progression typed in shorthand, a melody piano roll, and a transport. NEW starts from a template, LOAD opens a `.luthiertune`, IMPORT reads a `.mid` (see Import, below). |
| **LIVE** | set up for the stage: the 128-snapshot bank as an 8 x 16 grid, the setlist, crossfade and morph. |
| **ROUTING** | bus layout, aux buses 1 to 8 (Aux 8 is the playing noise on its own), per-string outputs, the sidechain and MIDI out. |
| **TONE MATCH** | impulse-response slots, the cab and EQ match wizards, capture, and the IR library. |
| **CHARACTER** | the character seed, dead spots, fret wear, tuner drift, aged electronics, body age, environment, and the string noise, pick, setup and slide groups. |
| **PRACTICE** | progress, routines, per-tool defaults, the library and the session recorder. The practice drawer is where you actually practise. |
| **NOTATION** | capture what you play, the string roll, a live tab view with chord symbols, and export as MusicXML, Guitar Pro, ASCII tab or MIDI. |
| **MIDI OUT** | the MIDI export profile (Luthier or Generic), exporting or dragging out the capture, and live MIDI out. |
| **CONTROLLERS** | the controller profile, the latency wizard, dead zone and minimum note length. |
| **HELP** | this manual in the plugin, pinned to whichever panel you were on, with a live list of the keyboard shortcuts. |

`Ctrl + [` and `Ctrl + ]` step through the tabs, wrapping at the ends. The tab you
had open last is the one that opens next time.

#### The string roll

On the NOTATION tab, above the live tab view (collapsible, and it remembers),
and in the Advanced strip when FRETS | ROLL is set to ROLL. It is a small piano
roll whose lanes are the strings, in the fretboard's top-to-bottom order, with
time running left to right and *now* at the right edge. The notes the engine
actually played - voiced, not the incoming MIDI - scroll past as bars labelled
with their fret, four bars at the host's tempo or eight seconds without one,
and the right edge of each lane glows with that string's live level.

It plays as well as shows. Click a lane to pluck that string: how high in the
lane you click sets the fret (higher is further up the neck), how far right
sets how hard. Lanes take keyboard focus, so `Enter` or `Space` plucks the
focused lane, and each lane names its string to a screen reader.

#### The LIVE snapshot grid

Eight rows of sixteen cells, each with its number, label and colour tag. The
gestures on a cell:

| Input | Action |
|---|---|
| Click | Select the slot (the CAPTURE and RECALL buttons act on it) |
| `Shift` + click | Store the current sound in that slot |
| Double-click | Recall it, crossfading over the time set below |
| Right-click | Recall, capture here / capture over, rename, colour tag, clear |

An empty bank says so, and tells you how to fill it.

---

## Controls

Every control behaves the same way.

| Input | Action |
|---|---|
| Left-drag | Adjust |
| `Shift` + drag | Coarse |
| `Ctrl` / `Cmd` + drag | Ultra-fine |
| Double-click | Reset to default |
| Mouse wheel | Scrolls the column the control sits in |
| `Ctrl` / `Cmd` + wheel | Nudge the value |
| Hover | The value replaces the label; a tooltip follows after 400 ms |
| Right-click | Enter value, Reset, Copy, Paste, MIDI Learn, Lock, Randomise, Modulate |

### MIDI Learn

Two ways in, and they end in the same place.

**From the header**: press **Learn** (or `Ctrl + L`). The window tints and the
next control you click becomes the target - then move a knob or pedal on your
controller and it is mapped. Clicking anywhere that is not a control cancels, and
so does `Escape`.

**From the control**: right-click it and choose **MIDI Learn**, then move your
knob or pedal.

A small teal dot appears on any control that has a CC mapped. Right-click again
to clear it.

Sustain and sostenuto are skipped while learning, so an accidental pedal press
cannot steal the mapping.

Mappings are stored with the plugin state, not the preset, so your controller setup
survives changing sounds.

### Locks and randomise

A locked control is left alone by Randomise. Each press of Randomise starts from
the defaults, so repeated presses give genuinely new sounds rather than drifting
further from anything usable.

---

## Playing modes

### Mono / lead

Every note goes to one string, chosen to keep the hand near where it already is.
Overlapping notes become hammer-ons, pull-offs or slides. Use this for solos, and
for tapping.

### Poly / chord

Chords are voiced across the strings by a search that only returns fingerings a
hand could make. If a voicing is impossible, the nearest playable one is used
rather than something absurd, and any note the chord search cannot finger is
placed on its own on a free string rather than dropped.

One note per string, as on the instrument. A note that arrives while others are
still ringing is voiced around them: the strings that are sounding are handed
to the voicer as occupied, so the new note takes a free string instead of
cutting off one that is ringing.

Chords are **strummed**, not triggered simultaneously. The strum speed, direction
and variation are yours to set.

This mode carries a small latency - the chord window, 15 ms by default - so that
the fingers of a keyboard chord, which land a few milliseconds apart, and a chord
split across a buffer boundary both voice as one chord. It is reported to your
host. The window is in Options > MIDI, and in Advanced under Performance; set it
to zero if you would rather have the notes placed one at a time with no delay.

### Guitar controller

MIDI channel 1 is the high E, channel 2 the B, and so on, which is the convention
hex pickups use. Per-string bend and pressure work natively.

Turn **MPE** on for expressive controllers - Seaboard, LinnStrument, Osmose.

---

## Techniques

See [PLAYING_TECHNIQUES.md](PLAYING_TECHNIQUES.md) for the full list and the
controller map. The short version:

- Play two notes on the same string quickly and you get a slide.
- Play the second one softly and you get a hammer-on or a pull-off.
- Hold CC 67 for palm mute, CC 73 for natural harmonics, CC 72 for pinch harmonics.
- Pitch bend bends the string; the mod wheel adds vibrato.
- Sustain lets everything ring; sostenuto holds only what is already down.

---

## Presets

Plain JSON, extension `.luthierpreset`. See
[PRESET_FORMAT.md](PRESET_FORMAT.md).

| | Path |
|---|---|
| User presets | `Documents/Luthier/Presets/User` |
| Renders | `Documents/Luthier/Renders` |
| Diagnostics | `Documents/Luthier/Diagnostics` |
| Factory | beside the plugin, or `Documents/Luthier/Presets/Factory` when the plugin's own folder cannot be written |

**Options > FILE LOCATIONS** has a button that opens each one. The HELP tab's
Presets topic shows the actual paths on your machine.

A factory preset is never overwritten - saving one makes a user copy, so you cannot
lose the original.

**If a preset does not appear**: press **Rescan presets** in Options > FILE
LOCATIONS; check it is in one of the folders listed there; check its extension is
exactly `.luthierpreset`. The folder it sits in becomes its category.

---

## Export

### Audio

**File > Export audio**, or the Export button in Easy mode.

Render the audition phrase or a MIDI file you choose. Format, bit depth, sample
rate, tail length and normalisation are all yours to set.

The render runs on its own thread through a second, offline copy of the plugin
loaded with your exact settings, so what you get is what you heard - including the
reverb tail, which is why there is a tail-length control. When it finishes you are
told where the file went, how long it is and at what quality.

### MIDI

Luthier keeps the last sixty seconds of MIDI you played. **File > Save last MIDI
take** writes it out, even though you never pressed record - useful exactly when
you play something good by accident.

### Command line

`luthier-render` does the same thing without a DAW:

```
luthier-render --midi riff.mid --preset "Modern Metal Chug" --out riff.wav
luthier-render --list-presets
luthier-render --help
```

---

## Import

### MIDI files into the Tune Builder

Three ways in, all ending in the TUNE tab:

- **File > Import MIDI...** and choose a `.mid`.
- **Drop a `.mid` (or `.midi`) file** anywhere on the window. One tune at a
  time: the first MIDI file in the drop is the one that is read.
- **IMPORT** on the TUNE tab, beside LOAD.

**File > New Tune...** opens the same template picker as the TUNE tab's NEW
button. Either way the plugin switches to Advanced mode and the TUNE tab on the
way, so the window has to be wide enough for Advanced.

What the importer does with the file: tempo, meter, key and title come from
the metas; markers become sections (repeated markers fold into the setlist);
a file Luthier exported itself is recognised by its track names and read back
by channel, so chords come back as chord cells and layers as layers. Any other
file is sorted by what its tracks do: a track that mostly plays three or more
notes at once is the chord track, a low or "bass"-named one is the bass, the
first monophonic one is the melody, channel 10 is drums and is skipped, and
the rest come in as verbatim layers. Chords are recovered beat by beat with the
same chord detector the rhythm engine uses, so every chord the importer writes
is one the engine can play, and the Tune Builder then **strums** them with each
section's pattern - an import is an arrangement of the file, not a playback of
it. The melody and bass play as written. The file's own chord track is kept as
a muted layer so nothing is lost, and a note that crosses a section boundary
is split at it.

The banner afterwards says how many sections and what tempo; **Details** lists
which track became what and everything that was guessed or defaulted. A file
that is corrupt, empty, or in SMPTE time is refused and the tune you had is
left untouched.

---

## Options

Options (`Ctrl + ,`, or **File > Options**) has eleven tabs along the top:
AUDIO, MIDI, APPEARANCE, ACCESSIBILITY, LOCALIZATION, EXPRESSION, RANGES, UPDATES,
PRIVACY, DIAGNOSTICS and FILE LOCATIONS.

### Audio

- **Oversampling** - 4x by default. 2x sounds very close and costs noticeably less.
  The same control is in Advanced mode, column 3, under Master.
- **Devices** - handled by your host when running as a plugin, and by the
  wrapper's own settings in the standalone. The page says where to find them.

### MIDI

- **Chord window** - how long Poly mode waits to collect a chord, 15 ms by
  default.
- **Clear all MIDI mappings** - forgets every MIDI Learn assignment at once.

### Appearance

- **Palette** - the default, three colourblind-safe palettes, high contrast and
  light.
- **UI scale**, **reduced motion** (stops the animated meters and the data stream)
  and **tooltips** on or off.

### Accessibility

- **Screen reader verbosity** and **font**.
- **Shortcuts** - every keyboard shortcut, searchable and rebindable. Click a row
  and press the key you want. **Reset all shortcuts** puts them back.

### Localization

- **Language** and a fallback, plus a custom string catalog if you want to supply
  your own translation.

### Expression

- **Calibrate** an expression pedal: it asks for heel and toe, then records the
  range. Pedals that do not reach 0 or 127 are handled by the dead zones.
- **Curve** - linear, logarithmic or exponential response.
- The list shows every CC that has been calibrated.

### Ranges

- **Advanced ranges for this preset** - lets controls go past their stock range.
  Turning it off says how many values it will pull back before it does.
- Whether marked values always show in the warning colour, and whether Randomise
  stays inside the stock range.
- A list of anything in the current preset that is outside its stock range.

### Updates

- Automatic checks on or off, beta releases on or off, a **Check now** button and
  the release notes.

### Privacy

- **Telemetry** - usage, diagnostics and crash reports are three separate opt-ins,
  all off unless you turn them on, each with a plain description of what it sends.
- **View last upload** shows exactly what was sent, and **Clear all local logs**
  removes what is stored here.
- The last button turns everything off and deletes every diagnostic file in one
  action, for when you would rather not think about it again.
- The update, telemetry and crash-report addresses, for a network that goes through
  a proxy.

### Diagnostics

- **Open the debug window**, **Create a log file if Luthier crashes**, **Export
  troubleshooting file**, **Open diagnostics folder** and **Reset all settings and
  clear caches** - the same tools as Help's debug window, described below.
- Whether the session recorder keeps the last hour of audio.

### File locations

- A button that opens each folder Luthier uses: user presets, factory presets,
  renders and diagnostics.
- The folders it scans for presets, **Add a preset folder...** and **Rescan
  presets**.

Controller setup is not in Options any more: it is the **CONTROLLERS** tab in
Advanced mode's workspace.

---

## Help and debug

**?** in the header, or `F1`, opens Help: a full description of every part of the
interface, the technique map, the preset system, troubleshooting, the licence and
the links, beside a list of every keyboard shortcut as it is bound right now - a
shortcut you rebind shows up there straight away.

In Advanced mode Help is the **HELP** tab, and it opens on the topic for the panel
you were working in: press `F1` on the TONE MATCH tab and you get TONE MATCH's page.
In Easy mode it opens over the window.

Inside it, **Open Debug Tools** (or `Ctrl + D`) gives you:

- a **live state view** - every string's pitch, level and tension, the signal
  levels, the validator's corrections, and a self-test;
- a **live event stream**;
- **Create log file on crash** - off on every load, on purpose. Tick it, reproduce
  the crash, and a timestamped log is written with a copy of the troubleshooting
  report at the top;
- **Export troubleshooting file** - a one-off snapshot of your settings, your audio
  and MIDI configuration, your host, the version, and a short self-test;
- **Reset all settings and clear caches** - the destructive reset, for when nothing
  else works. It asks first, and it does not delete your saved presets.

If Luthier is hard-crashing, send **both** files to support with a description of
what you were doing.

---

## Performance

| Setting | Effect |
|---|---|
| Oversampling 2x instead of 4x | the largest single saving |
| Body mode Modal instead of Convolution | cheaper on some machines, dearer on others - try both |
| Fewer pedal slots | empty slots cost nothing, so clearing unused ones helps |
| Cabinet off | if you are using your own IR loader |
| Dual mic off | halves the cabinet convolution |

The footer shows this instance's CPU share and the reported latency.

---

## If something is wrong

See [TROUBLESHOOTING.md](TROUBLESHOOTING.md). The short list:

- **No sound?** Check MIDI is arriving (the dot beside the logo), that the amp is
  not on Standby, that Master is up, and that a pickup is selected. Press Panic in
  case a note is stuck.
- **A pedal you just picked is silent?** Its footswitch is on BYPASS. Click it.
- **Something keeps playing and Panic does not end it?** Press **RESET & STOP**
  (`Ctrl + Shift + P`). It stops the tune, the loops and the rhythm engine as
  well as the strings.
- **Crackles?** Lower the oversampling, or raise your host's buffer size.
- **Out of tune?** Realism Detune is deliberate. Turn it to zero in Advanced for
  machine-perfect tuning.
- **A preset will not load?** Rescan in Options > FILE LOCATIONS.
