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

## Structure

```json
{
  "format": "luthierpreset",
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
  }
}
```

---

## Fields

### Metadata

| Field | Type | Notes |
|---|---|---|
| `format` | string | always `"luthierpreset"` |
| `schemaVersion` | int | currently 1; a file with a higher number still loads, with unknown keys ignored |
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
saved. A missing key falls back to that parameter's default, so a preset written by
an older build still loads.

The IDs follow a consistent pattern:

| Prefix | Covers |
|---|---|
| `macro_*` | the six Easy-mode macros |
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

CC number to performance target, for the technique controllers. Only non-default
entries are written. Targets are the `MidiTarget` enum:

| Value | Target | Value | Target |
|---|---|---|---|
| 0 | None | 11 | Slide guitar |
| 1 | Vibrato depth | 12 | Pinch harmonic |
| 2 | Vibrato rate | 13 | Natural harmonic |
| 3 | Whammy bar | 14 | Tap |
| 4 | Expression | 15 | Strum speed |
| 5 | Master level | 16 | Strum direction |
| 6 | Palm mute | 17 | Humanize |
| 7 | Muted pick | 18 | Drive |
| 8 | Pick position | 19 | Tone |
| 9 | Slide mode | 20 | Space |
| 10 | Slide guitar toggle | 21 | Body |

Generic MIDI Learn mappings are **not** stored here. They live with the plugin
state instead, so your controller setup survives changing presets.

---

## Host state

What a DAW saves in its project is a superset of a preset:

```json
{
  "preset":     { "...": "the object above" },
  "midiLearn":  [ { "parameter": "macro_drive", "cc": 21, "channel": 0,
                    "min": 0.0, "max": 1.0, "inverted": false } ],
  "ui":         { "advancedMode": false, "tooltipsEnabled": true,
                  "selectedString": 0, "easterEggFound": false,
                  "editorWidth": 1200, "editorHeight": 720 },
  "lockedParameters": ["amp_model"],
  "slotBActive": false
}
```

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
  "format": "luthierpreset",
  "schemaVersion": 1,
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
- **Newer presets.** Unknown keys are ignored, so a file from a later version loads
  as best it can rather than being refused.
- **Impulse responses.** A preset stores the cabinet and body *configuration*, not
  a path. If the matching IR is missing, the engine falls back to modal synthesis
  for the body and to the procedural speaker model for the cabinet. A preset never
  fails to load because a file moved.
