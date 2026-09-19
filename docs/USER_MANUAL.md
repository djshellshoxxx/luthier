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
| **File** | save, open, import, export, options, randomise, reset. |
| **A / B** | two comparison slots. `A>B` copies the current one across. |
| **Undo / Redo** | 64 steps. |
| **Panic** | stops every string immediately. |
| **?** | help, troubleshooting and the debug tools. |
| **Advanced** | switches modes. |

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
- **Mode** - Mono, Poly or Guitar Controller.
- **Chord readout** - names what you are playing.
- **Audition** - plays a phrase through the current sound. Pick which phrase beside
  it.
- **Export**, **Randomise**, **Reset**.

---

## Advanced mode

A compressed guitar and fretboard strip across the top, then four scrollable
columns.

### Column 1 - Strings

One row per string: its pitch, its computed tension, its gauge, and a mute square.

**The tension readout turns amber when it leaves the playable range.** That is the
engine telling you the tuning you have asked for would need a string no
manufacturer makes. It will still play - the value is nudged into range - but the
warning is there.

Below: the string set (material, gauge, age), the sustain scaling, and the tuning
realism controls.

### Column 2 - String detail

Everything about the selected string, computed rather than stored: gauge in inches
and millimetres, whether it is wound, its core diameter, its mass per metre, its
tension, its inharmonicity coefficient, its T60 and its brightness.

Below: fretless, slide guitar and freeze toggles, the action and buzz controls, the
temperament, and the sympathetic coupling amount.

### Column 3 - Body, pickups and hand

**Body.** Convolution or modal synthesis, the amount, and then the dimensions -
size, depth, top thickness, sound hole, bracing, woods, age. In Modal mode these
genuinely move the resonances, because the plate and Helmholtz equations are
evaluated live.

**Pickups.** The selector, then per slot: type, position, height, magnet, volume.
Position runs from 0 at the bridge to 0.5 at the midpoint, and it is the same
number the comb filter uses - a pickup at 1/N of the string nulls the Nth harmonic.

**Playing hand.** Pick or fingers, material, thickness, angle, position, nail
versus flesh.

**String noise.** Slide squeak, fret click, release thump, body knock, pick attack,
amp buzz - each with its own control.

### Column 4 - The rig

Bridge type and whammy, cable, the pre-amp pedalboard, the amplifier, the cabinet
and microphones, the room, the effects loop, performance settings, humanisation,
feedback, doubler and master.

---

## Controls

Every control behaves the same way.

| Input | Action |
|---|---|
| Left-drag | Adjust |
| `Shift` + drag | Coarse |
| `Ctrl` / `Cmd` + drag | Ultra-fine |
| Double-click | Reset to default |
| Hover | The value replaces the label; a tooltip follows after 400 ms |
| Right-click | Enter value, Reset, Copy, Paste, MIDI Learn, Lock, Randomise |

### MIDI Learn

Right-click a control, choose **MIDI Learn**, then move a knob or pedal on your
controller. A small teal dot appears on any control that has a CC mapped.
Right-click again to clear it.

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
| Factory | inside the plugin bundle |

Options has a button for each.

A factory preset is never overwritten - saving one makes a user copy, so you cannot
lose the original.

**If a preset does not appear**: press **Rescan presets** in Options; check it is in
one of the folders listed there; check its extension is exactly
`.luthierpreset`. The folder it sits in becomes its category.

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

Options has five tabs along the top.

### General

- **Tooltips** on or off.
- **Tuning drift** - lets the guitar slowly go out of tune while you play.
- **Oversampling** - 4x by default. 2x sounds very close and costs noticeably less.
- **Chord window** - how long Poly mode waits to collect a chord.
- **Folders** - open the preset, render and diagnostics folders; add another folder
  to scan; rescan.
- **Audio and MIDI** - handled by your host when running as a plugin, and by the
  wrapper's own toolbar in the standalone.

### Controllers

- **Profile** - pick your controller and the channel map, pitch-bend range and
  latency budget come with it. The notes underneath say what the profile assumes.
- **Measure latency** - plays a short test and reports the round trip it actually
  measured, with the scatter, so you can see whether the number is trustworthy. A
  measured value overrides the profile's budget.
- **Pitch dead zone** - how far a string has to move before it counts as a bend
  rather than tracking noise.
- **Minimum note length** - keeps a note alive long enough to be heard when a
  controller sends an immediate note-off.
- **Save as my profile** - stores your edits as a profile of your own.

### Expression

- **Calibrate** an expression pedal: it asks for heel and toe, then records the
  range. Pedals that do not reach 0 or 127 are handled by the dead zones.
- **Curve** - linear, logarithmic or exponential response.
- The list shows every CC that has been calibrated.

### Accessibility

- **Screen reader verbosity**, **colourblind palette**, **UI scale** and
  **font** - see the accessibility notes for what each palette changes.
- **Reduced motion** - stops the animated meters and the data stream.
- **Language** and a fallback, plus a custom string catalog if you want to supply
  your own translation.
- **Shortcuts** - every keyboard shortcut, searchable and rebindable. Click a row
  and press the key you want.

### Privacy

- **Updates** - automatic checks on or off, beta releases on or off, and a
  **Check now** button.
- **Telemetry** - usage, diagnostics and crash reports are three separate opt-ins,
  all off unless you turn them on, each with a plain description of what it sends.
- **View last upload** shows exactly what was sent, and **Clear all local logs**
  removes what is stored here.
- The last button turns everything off and deletes every diagnostic file in one
  action, for when you would rather not think about it again.

---

## Help and debug

**?** in the header opens Help: a full description of every part of the interface,
the technique map, the preset system, the shortcuts, troubleshooting, the licence
and the links.

Inside it, **Open Debug Tools** gives you:

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
- **A preset will not load?** Rescan in Options.
