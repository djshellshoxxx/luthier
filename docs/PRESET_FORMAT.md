# The preset format

Presets are plain JSON with the extension `.luthierpreset`. Readable, diffable, and
safe to keep in version control.

---

## Where they live

| | Path |
|---|---|
| Factory | inside the plugin bundle, `Resources/Presets/Factory/<Category>/` |
| User (Windows) | `%USERPROFILE%\Documents\Luthier\Presets\User\<Category>\` |
| User (macOS / Linux) | `~/Documents/Luthier/Presets/User/<Category>/` |

Any other folder can be registered in Options. **The folder a preset sits in
becomes its category** - a preset loose in the root is filed under "User".

A factory preset is never overwritten. Saving one makes a copy in your user folder.

---

## Safety

**Saves are atomic.** The file is written to a temporary name, flushed, and then
renamed over the target. A crash mid-save leaves either the old file or the new
one, never half of either.

**The version you replace is kept.** Before a save lands, the file it is about to
replace is copied to `Presets/Backup/<yyyy-mm-dd>/` under its nearest `Presets`
ancestor. For a preset outside a `Presets` tree, `Backup/<yyyy-mm-dd>/` is beside
the file. Several saves in one day
keep several versions. A sweep at startup deletes backups older than 30 days,
dated by the folder name rather than the filesystem timestamp - copying a backup
folder around should not resurrect it or expire it early.

**Unknown fields survive.** A preset written by a newer version of Luthier keeps
whatever that version added, even after this one loads and re-saves it. Opening
someone else's preset does not quietly strip it.

**Files that are not presets are refused.** The `magic` marker is checked before
anything is applied, so a JSON file that happens to parse is not half-loaded.
Older presets written before the marker was named carry `"format": "luthierpreset"`
instead and are still accepted for backward compatibility.

---

## Structure

```json
{
  "magic": "luthier.preset",
  "schema": 1,
  "schemaVersion": 1,
  "pluginVersion": "1.0.0",

  "name": "Clean Strat Funk",
  "category": "Electric",
  "author": "Luthier Audio",
  "description": "Position 4 Strat into a blackface Twin.",
  "tags": ["clean", "funk", "strat"],

  "parameters": {
    "macro_attack": 0.74,
    "macro_body": 0.28,
    "guitar_type": 0.0,
    "amp_model": 0.0,
    "pre0_type": 0.047619
  },

  "strings": {
    "numStrings": 6,
    "useCustomTuning": false,
    "openFrequencyHz":    [329.628, 246.942, 195.998, 146.832, 110.0, 82.407],
    "detuneCents":        [0, 0, 0, 0, 0, 0],
    "realismDetuneCents": [1.2, -0.8, 2.1, -1.4, 0.6, -2.3],
    "fineTuneCents":      [0, 0, 0, 0, 0, 0],
    "customGaugeInches":  [0, 0, 0, 0, 0, 0],
    "muted":              [false, false, false, false, false, false],
    "customTemperament":  [1.0, 1.0595, 1.1225, 1.1892, 1.2599, 1.3348,
                           1.4142, 1.4983, 1.5874, 1.6818, 1.7818, 1.8877]
  },

  "midiMap": {
    "1": 1,
    "67": 6
  },

  "modulation":    { "lfos": [], "envelopes": [], "sequencers": [], "followers": [], "routes": [] },
  "snapshots":     { "snapshots": [ { "label": "Verse", "colourTag": 3, "parameters": {} } ] },
  "midi_mappings": [ { "parameter": "macro_drive", "cc": 21, "channel": 0,
                       "min": 0.0, "max": 1.0, "inverted": false } ],
  "rhythm_engine": { "enabled": false },
  "routing":       { "aux": [], "perString": [], "sidechainToAmp": false, "midiOut": {} },
  "character":     { "seed": "1592639710", "amount": 0.3 },
  "tone_match":    { "body": { "file": "" }, "cab1": { "file": "" }, "cab2": { "file": "" } }
}
```

(The processor blocks are abbreviated here; each is whatever that module
writes, and each is optional.)

---

## Fields

### Metadata

| Field | Type | Notes |
|---|---|---|
| `magic` | string | always `"luthier.preset"`. Files written before it was named carry `"format": "luthierpreset"` instead, which is still accepted on load |
| `schema` | int | currently 1. `schemaVersion` is written beside it for older builds, and read when `schema` is absent; a file with neither is schema 1. A file with a **higher** number is refused with "X was made by a newer Luthier version. Update to open." (it could not be re-saved without losing what the newer version wrote); one below 1 is refused as a format Luthier no longer supports |
| `pluginVersion` | string | which build wrote it, for support |
| `name` | string | shown in the header and the browser |
| `category` | string | overrides the folder name if present |
| `author` | string | free text |
| `description` | string | shown in the browser |
| `tags` | array of strings | searchable |

### `parameters`

Every automatable parameter, keyed by its ID, as a **normalised** value from 0 to 1.

Normalised rather than plain because it is unambiguous: a parameter's range can be
adjusted in a later version without silently reinterpreting every preset ever
saved. A missing key falls back to that parameter's default - not to whatever the
previous preset set - so a preset written by an older build still loads, and loads
the same way whatever was loaded before it. The exceptions are the preset-morph
slider and Slide Mode (`slide_guitar`, `slide_mode`), which belong to the player
rather than the sound and keep their value when a preset leaves them out.

The IDs follow a consistent pattern:

| Prefix | Covers |
|---|---|
| `macro_*` | the Easy-mode macros (attack, body, drive, tone, space, humanize, character) and the two spare modulation macros `macro_assign_a/b` |
| `string_*`, `fret*`, `noise_*` | strings, neck and mechanical noise |
| `body_*` | body dimensions, woods, bracing |
| `pickup<N>_*` | per-pickup, N = 0, 1, 2 |
| `amp_*`, `cab_*`, `mic_*`, `room_*` | the rig |
| `pre<N>_*`, `post<N>_*` | effect slots, N = 0 to 7 |
| `hum_*` | humanisation |
| `secret_*` | the hidden effect |

Each slot has `_type`, `_bypass`, `_mix` and `_p0` to `_p9`. What `_p0` onwards
mean depends on the pedal in that slot - they are read from the pedal's own
descriptors, which is why adding a pedal to the engine needs no preset-format
change.

### `strings`

Per-string state that is not automatable, because it is structural rather than
performable.

| Field | Notes |
|---|---|
| `numStrings` | 1 to 12 |
| `useCustomTuning` | if false, `openFrequencyHz` is informational only |
| `openFrequencyHz` | per-string open pitch |
| `detuneCents` | deliberate offset, -100 to +100 |
| `realismDetuneCents` | the randomised imperfection, **stored so it persists** |
| `fineTuneCents` | per-string fine tuner |
| `customGaugeInches` | 0 means use the gauge set |
| `muted` | per-string mute |
| `customTemperament` | twelve ratios, used when the temperament is Custom |

`realismDetuneCents` is saved rather than regenerated because a preset should sound
the same every time you open it. A guitar that was slightly out of tune yesterday
is out of tune the same way today.

### `midiMap`

CC number to performance target, for the technique controllers. Every mapped CC
is written, the defaults included; a CC absent from the map is unmapped, and a
preset with no `midiMap` at all gets the default map. Targets are the `MidiTarget`
enum, stored as integers (so these values never change):

| Value | Target | Value | Target |
|---|---|---|---|
| 0 | None | 11 | Pinch harmonic |
| 1 | Vibrato depth | 12 | Natural harmonic |
| 2 | Vibrato rate | 13 | Tap |
| 3 | Whammy bar | 14 | Strum speed |
| 4 | Expression | 15 | Strum direction |
| 5 | Master level | 16 | Humanize |
| 6 | Palm mute | 17 | Drive |
| 7 | Muted pick | 18 | Tone |
| 8 | Pick position | 19 | Space |
| 9 | Slide toggle | 20 | Body |
| 10 | Slide guitar toggle | 21 | Attack |

### The processor blocks

A preset also carries the state that does not live in parameters
(state-model.md 1, file-formats.md 2):

| Block | What |
|---|---|
| `modulation` | the modulation matrix: LFOs, envelopes, sequencers, followers and routes |
| `snapshots` | the snapshot bank - up to 128 labelled states, recalled from the LIVE tab, a program change or a setlist entry |
| `midi_mappings` | MIDI Learn: parameter, CC, channel, range, inverted |
| `rhythm_engine` | the rhythm engine: on/off, pattern, voicing, genre kit |
| `routing` | aux-strip mute, solo and gain, per-string buses, sidechain-to-amp and the MIDI-out assignments. Never the bus layout, which the host owns |
| `character` | the instrument's character seed and wear |
| `tone_match` | the body and cabinet IR slots: file (relative to the IR folder when it is inside it) and settings |

A block the file leaves out goes back to its default when the preset loads, so
nothing from the previous preset lingers - with one exception: **MIDI Learn**.
`midi_mappings` is written only when there are mappings, and a preset without it
leaves your current mappings alone, so a controller setup survives browsing the
factory presets. A preset that carries mappings replaces them.

Older builds spelled three of these `midiMappings`, `rhythmEngine` and
`toneMatch`; those spellings are still read.

---

## Host state

What a DAW saves in its project is a superset of a preset:

```json
{
  "preset":     { "...": "the object above, processor blocks included" },
  "ui":         { "advancedMode": false, "tooltipsEnabled": true,
                  "selectedString": 0, "easterEggFound": false,
                  "editorWidth": 1200, "editorHeight": 720 },
  "lockedParameters": ["amp_model"],
  "slotBActive": false,
  "liveMode": false,
  "metronome": { },
  "tune": { }
}
```

Sessions saved by older builds kept `midiLearn`, `modulation`, `snapshots`,
`rhythm`, `routing`, `character` and `toneMatch` at the top level instead; they
are still read.

---

## Writing presets by hand

Perfectly reasonable - that is why the format is JSON.

1. Save a preset from the plugin to get a complete file.
2. Edit the values you care about.
3. Drop it in your user preset folder.
4. Press **Rescan presets** in Options.

Only the keys you change need to be present; everything else falls back to its
default. A minimal preset is valid:

```json
{
  "magic": "luthier.preset",
  "schema": 1,
  "name": "Just A Les Paul",
  "category": "User",
  "parameters": { "guitar_type": 0.0833 }
}
```

To work out a normalised value for a choice parameter, divide its index by the
number of choices minus one. `guitar_type` has 25 entries, so a Les Paul at index 2
is 2 / 24 = 0.0833.

---

## Compatibility

- **Sample rates.** Every time is stored in seconds or Hz, never in samples, so a
  preset written at 44.1 kHz sounds identical at 192 kHz.
- **Older presets.** Missing keys fall back to defaults.
- **Newer presets.** A file with a higher schema number is refused with a message
  saying to update. Within one schema, keys this build does not know are kept and
  written back on save.
- **Impulse responses.** A preset stores the cabinet and body *configuration*, not
  a path. If the matching IR is missing, the engine falls back to modal synthesis
  for the body and to the procedural speaker model for the cabinet. A preset never
  fails to load because a file moved.
