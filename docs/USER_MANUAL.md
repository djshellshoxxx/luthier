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

Seven macro knobs cover most of what you will want to change. When you want more,
press **Advanced** at the top right - nothing is hidden there that is not also
reachable from Easy mode; Advanced just stops summarising.

### The first launch

A fresh install opens in Easy mode on **Single-Cut Crunch**, a finished rock sound,
so the first note you play already sounds like a guitar. It follows your system's
high-contrast, reduced-motion, display-scale and language settings, and makes no
network connection.

A banner under the header offers a **two-minute tour**: twelve stops, one callout
each, pointing at the control it describes. **Next**, **Back** and **Skip** move
through it and **Escape** ends it. **Maybe later** brings the offer back next time
(three times at most); **Don't ask again** means it. The HELP tab's **Take the
tour** button starts it at any time.

For your first week (seven launches or seven days) new things are marked: every
panel's `?` pulses the first time you see it, each workspace tab you have not
opened carries a small dot, and the Workshop wrench, the TUNE tab and the Slide
switch pulse once.

Three ways in:

- **30 seconds** - play. The default sound is enough.
- **2 minutes** - take the tour, then press **Randomise** a few times.
- **5 minutes** - take the tour, load an example tune in the TUNE tab, press play,
  and swap the guitar in the Workshop while it loops.

Every panel with more than one row of controls has a `?` in its corner: it opens
Help on that panel. Right-clicking an empty part of an Advanced column offers the
same, as **Docs**.

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
| **File** | save, save as, open, import and export a preset; export audio, save the last MIDI take, export notation; open the preset and render folders; options; randomise; reset. |
| **A / B** | two comparison slots. `A>B` copies the current one across. |
| **Undo / Redo** | 200 steps. |
| **Panic** | stops every string immediately. |
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

The fretboard on the illustration shows the notes as they sound - and what is lit
is what is actually ringing, including notes the chord voicer put somewhere you did
not expect, which is exactly when you want to see it.

The **playable fretboard** is the strip across the top of Advanced mode:

- Click a fret to hear that note. How high in the string's lane you click sets how
  hard it is picked.
- Right-click for mute, capo, string selection and scale overlays.

Fret spacing follows the real rule, so the frets crowd together up the neck exactly
as they do on the instrument.

To the right, a scrolling readout shows what is happening inside the plugin - MIDI,
parameter changes, engine events. It stops when nothing is happening.

### Middle band - the macros

| Macro | What it does |
|---|---|
| **Attack** | pick attack, soft and round to sharp and bright. Moves the contact bandwidth of whatever is touching the string. |
| **Body** | how much of the guitar's body you hear. Never fully off - even a solid body colours the sound. |
| **Drive** | amp gain. Adds to the amp's own control rather than replacing it. |
| **Tone** | global tone, dark to bright. Moves the guitar's tone control and the amp's treble together. |
| **Space** | room and ambience. |
| **Humanize** | timing, velocity, tuning and attack variation. At zero the plugin is machine-perfect. |
| **Character** | dead spots, tuner drift, fret wear, body age and string noise together - the amount on the CHARACTER tab, which has each one on its own. |

Under each: a **dice** (randomise just this one) and a **padlock** (exclude it from
Randomise).

Macros multiply the detailed controls rather than replacing them, so Advanced edits
survive a macro move.

### Bottom band - style and play

- **Style** - the factory bank, grouped by category.
- **Mode** - Mono, Poly or Guitar Controller.
- **Chord readout** - names what you are playing.
- **Audition** - plays a phrase through the current sound. Pick which phrase beside
  it.
- **Export**, **Randomise**, **Reset**.

---

## Advanced mode

A compressed guitar and fretboard strip across the top, then four columns laid
out the way the signal flows: the instrument, what picks it up, what amplifies it,
and a workspace for everything that is not a knob. Columns 1 to 3 scroll on their
own.

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
the piezo / mic balance for acoustic instruments. Where a pickup sits and how high
is set on the Workshop bench, by dragging it.

**Circuit.** The guitar's own electronics, and a curve that shows what they are
doing to the top end: the volume and tone controls, pot values and taper, the tone
capacitor, treble bleed, an active buffer, and the cable - its length and quality -
into the amp's input. Turning the guitar's volume down cleans the amp up, as it
does on the real thing.

**Pedalboard (before the amp).** The pre-amp pedal slots.

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
| **TUNE** | write a whole tune: a setlist timeline and sections (drag, Vary), a chord progression typed in shorthand or edited as pills (popover, drag, substitutions, chord tools), a piano roll for the melody, bass line and countermelody (select, nudge, copy, note menu), Sing into the audio input, bass and layers, a transport with TO LOOPER, example tunes, and one-screen export of audio (with stems), MIDI, notation or the project. |
| **LIVE** | set up for the stage: the 128-snapshot bank, the setlist, crossfade and morph. |
| **ROUTING** | bus layout, aux buses 1 to 8 (Aux 8 is the playing noise on its own), per-string outputs, the sidechain and MIDI out. |
| **TONE MATCH** | impulse-response slots, the cab and EQ match wizards, capture, and the IR library. |
| **CHARACTER** | the character seed, dead spots, fret wear, tuner drift, aged electronics, body age, environment, and the string noise, pick, setup and slide groups. |
| **PRACTICE** | progress, routines, per-tool defaults, the library and the session recorder. The practice drawer is where you actually practise. |
| **NOTATION** | capture what you play, a live tab view with chord symbols, and export as MusicXML, Guitar Pro, ASCII tab or MIDI. |
| **MIDI OUT** | the MIDI export profile (Luthier or Generic), exporting or dragging out the capture, and live MIDI out. |
| **CONTROLLERS** | the controller profile, the latency wizard, dead zone and minimum note length. |
| **HELP** | this manual in the plugin, pinned to whichever panel you were on, with a live list of the keyboard shortcuts. |

`Ctrl + [` and `Ctrl + ]` step through the tabs, wrapping at the ends. The tab you
had open last is the one that opens next time.

---

## Controls

Every control behaves the same way.

| Input | Action |
|---|---|
| Left-drag | Adjust |
| `Shift` + drag | Coarse |
| `Ctrl` / `Cmd` + drag | Ultra-fine |
| Double-click | Reset to default |
| Hover | The value appears above the control (the label stays); a tooltip follows after 400 ms |
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
rather than something absurd.

Chords are **strummed**, not triggered simultaneously. The strum speed, direction
and variation are yours to set.

This mode carries a small latency - the chord window, 2 ms by default - so that a
chord split across a buffer boundary still voices as one chord. It is reported to
your host.

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

- **Chord window** - how long Poly mode waits to collect a chord.
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
- **Crackles?** Lower the oversampling, or raise your host's buffer size.
- **Out of tune?** Realism Detune is deliberate. Turn it to zero in Advanced for
  machine-perfect tuning.
- **A preset will not load?** Rescan in Options > FILE LOCATIONS.
