# FILE FORMATS SPEC

Canonical schemas for every file Luthier reads or writes. If a spec
mentions a file, its exact schema lives here. Fragments in other specs
are reference; this file is the source of truth.

## 0. Ground rules

1. Every file is **UTF-8 JSON** unless noted (audio is WAV, MIDI is
   standard MIDI file).
2. Every file starts with a `schema` integer at the top level. Missing
   `schema` is treated as `schema: 1`.
3. **Forward compatibility**: unknown fields are preserved on load and
   written back on save. Older readers ignore what they don't know.
4. **Backward compatibility**: migrations documented per format below.
5. Every file has a **canonical file extension** and a **magic marker**
   (either a top-level `magic` string or, for MIDI, the LUTHIER chunk).
6. **Portable paths**: file references inside JSON use relative paths
   when the referent lives under a registered user folder, absolute
   otherwise. Registered folders are recorded in the plugin's
   user-global config.

## 1. File type table

| Extension | Type | Magic | Owner |
|---|---|---|---|
| `.luthierpreset` | Preset | `"magic": "luthier.preset"` | PresetSystem |
| `.luthierguitar` | Guitar (bill of parts) | `"magic": "luthier.guitar"` | Workshop |
| `.luthierpart` | Single part | `"magic": "luthier.part"` | Workshop |
| `.luthiertune` | Tune (composition) | `"magic": "luthier.tune"` | TuneBuilder |
| `.luthierpattern` | Rhythm pattern | `"magic": "luthier.pattern"` | RhythmEngine |
| `.luthierjam` | Jam band style (jam-mode.md 12) | `"magic": "luthier.jam"` | Jam |
| `.luthierset` | Setlist | `"magic": "luthier.setlist"` | LivePerf |
| `.luthierloop` | Looper save | `"magic": "luthier.loop"` | Practice |
| `.luthiercontent` | Content update bundle | Signed manifest | Installer |
| `.midprofile` | MIDI export profile | `"magic": "luthier.midprofile"` | MidiExport |
| `.mid` / `.midi` | Standard MIDI (Luthier or Generic profile) | See midi-export.md 2 | MidiExport |
| `.wav` / `.aiff` / `.flac` | Audio (renders, captures, IRs) | Standard | AudioExport / ToneMatch |
| `.mp3` | Audio in (backing tracks only) | Standard | Practice |
| `config/performance.json` | CPU quality preference (cpu-quality-modes 3) | `"magic": "luthier.performance"`, `"schema": 1` | PerformanceSettings; written temp-and-rename (13); missing or corrupt -> defaults, never an error |

## 2. `.luthierpreset`

Full plugin state minus per-instance UI state.

```json
{
  "schema": 3,
  "magic": "luthier.preset",
  "meta": {
    "name": "Modern Overdrive",
    "author": "Factory",
    "category": "Rock/Electric",
    "tags": ["overdrive", "crunch", "electric"],
    "created": "2026-01-14T09:32:00Z",
    "modified": "2026-03-02T11:14:00Z",
    "version_created": "1.0.0",
    "version_modified": "1.2.1",
    "notes": ""
  },
  "guitar": {
    "reference": "Factory/Electric/Vintage Single-Cut.luthierguitar",
    "override": null
  },
  "parameters": {
    "amp_gain": 0.72,
    "amp_bass": 0.55,
    "guitar_volume": 1.0,
    "squeak_amount": 0.35
  },
  "ranges": {
    "families": {
      "amp": "stock",
      "circuit": "stock",
      "squeak": "stock",
      "buzz": "stock",
      "pick": "stock",
      "slide": "stock",
      "modulation": "stock"
    },
    "per_control_unlocks": []
  },
  "modulation": {
    "sources": {
      "lfo1": { "shape": "sine", "rate_hz": 1.5, "depth": 0.4 }
    },
    "routes": [
      { "src": "lfo1", "dst": "amp_bright", "depth": 0.15, "offset": 0, "curve": "linear", "enabled": true }
    ]
  },
  "snapshots": [
    { "index": 0, "name": "Rhythm", "color": 3, "parameters": {}, "modulation": {} }
  ],
  "midi_mappings": [
    { "cc": 74, "channel": 0, "param": "amp_gain", "min": 0, "max": 1 }
  ],
  "rhythm_engine": { "enabled": false, "genre_kit": "Modern Rock", "pattern": "rock_8_1" },
  "effects_state": {
    "pre_rack": [ { "slot": 0, "type": "compressor", "params": {}, "bypass": false } ],
    "post_rack": []
  },
  "midi_out_profile": "Generic"
}
```

Migration: schema 1 (pre-M42) had no `ranges` block. Loader adds
`{"families": {...}: "stock"}` for every family.
Schema 2 (pre-M49) had `guitar.name` string instead of `guitar.reference`.
Loader consults `Resources/Guitars/migration.json` to convert.

Backup on migration: original file moved to
`~/Documents/Luthier/Presets/Backup/<yyyy-mm-dd>/<name>-v<schema>.luthierpreset`.

## 3. `.luthierguitar`

A bill of parts. Every field references a part (by embedded blob or
by `.luthierpart` reference). This is what makes a guitar a real,
editable thing rather than a hard-coded enum.

```json
{
  "schema": 1,
  "magic": "luthier.guitar",
  "meta": {
    "name": "'59 Vintage Single-Cut",
    "family": "electric",
    "body_style": "single_cutaway_arched",
    "author": "Factory",
    "tags": ["vintage", "humbucker", "mahogany"],
    "created": "2026-01-10T00:00:00Z",
    "modified": "2026-01-10T00:00:00Z"
  },
  "parts": {
    "body": { "reference": "Factory/Bodies/Mahogany Single-Cut.luthierpart" },
    "top": { "reference": "Factory/Tops/Flame Maple.luthierpart" },
    "neck": { "reference": "Factory/Necks/Mahogany Set-Neck.luthierpart" },
    "fretboard": { "reference": "Factory/Fretboards/Rosewood.luthierpart" },
    "frets": { "reference": "Factory/Frets/Medium-Jumbo Nickel-Silver.luthierpart" },
    "nut": { "reference": "Factory/Nuts/Bone 43mm.luthierpart" },
    "bridge": { "reference": "Factory/Bridges/ABR-1 Tune-o-matic.luthierpart" },
    "tailpiece": { "reference": "Factory/Tailpieces/Stopbar.luthierpart" },
    "tuners": { "reference": "Factory/Tuners/Kluson 15:1.luthierpart" },
    "pickups": {
      "neck": {
        "reference": "Factory/Pickups/PAF 57 Alnico 2 7.6k.luthierpart",
        "position_mm": 152,
        "height_treble_mm": 2.8,
        "height_bass_mm": 3.1
      },
      "middle": null,
      "bridge": {
        "reference": "Factory/Pickups/PAF 57 Alnico 2 8.1k.luthierpart",
        "position_mm": 38,
        "height_treble_mm": 2.4,
        "height_bass_mm": 2.6
      }
    },
    "wiring": { "reference": "Factory/Wiring/50s LP.luthierpart" },
    "strings": {
      "reference": "Factory/Strings/D'Addario NYXL 10-46.luthierpart",
      "per_string_override": []
    },
    "pickguard": { "reference": "Factory/Pickguards/Cream 3-Ply.luthierpart" },
    "hardware_color": "nickel",
    "finish": {
      "type": "burst",
      "color_a": "#7A2E1B",
      "color_b": "#F2C441",
      "burst_shape": "diagonal",
      "gloss": 0.9,
      "aging": 0.15
    }
  },
  "setup": {
    "action_treble_mm": 1.4,
    "action_bass_mm": 1.7,
    "relief_mm": 0.25,
    "nut_slot_depths_mm": [0.9, 0.9, 0.9, 0.9, 0.9, 0.9],
    "intonation_mm": [0, 0, 0, 0, 0, 0]
  },
  "character_seed": 428913
}
```

## 4. `.luthierpart`

A single swappable part. One schema, many `part_type` values.

```json
{
  "schema": 1,
  "magic": "luthier.part",
  "meta": {
    "name": "PAF 57 Alnico 2 7.6k",
    "part_type": "pickup",
    "author": "Factory",
    "tags": ["humbucker", "vintage", "alnico 2"],
    "compatibility": ["electric"]
  },
  "fields": {
    "family": "humbucker",
    "inductance_h": 4.5,
    "dc_resistance_k": 7.6,
    "capacitance_pf": 150,
    "magnet": "alnico2",
    "coil_turns": 5000,
    "pole_piece_material": "steel",
    "cover": "nickel",
    "output_dbfs_reference": -6.2
  },
  "illustration": {
    "shape": "humbucker_open",
    "primary_color": "#B9BEC4",
    "secondary_color": "#2A2D31"
  }
}
```

Part-type-specific `fields` schemas documented in `part-acoustics.md`.

## 5. `.luthiertune`

Per `tune-builder.md` 11. Sections, chord cells, melody, setlist,
variations. Referenced presets and guitars carried by reference, not
embedded (unless the user chose "bundle" on export).

## 6. `.luthierpattern`

Per `rhythm-engine.md` 6. Strum or fingerpick pattern with steps,
mask, dynamics.

## 7. `.luthierset`

Setlist per `live-performance.md` 4:

```json
{
  "schema": 1,
  "magic": "luthier.setlist",
  "meta": {
    "name": "Friday Night Set",
    "notes": "start with clean, warm up to overdrive by track 3",
    "bpm_default": 118
  },
  "entries": [
    { "preset": "Presets/User/Clean Ballad.luthierpreset", "snapshot": 0, "notes": "intro" },
    { "preset": "Presets/User/Clean Ballad.luthierpreset", "snapshot": 1, "notes": "chorus" }
  ]
}
```

## 8. `.luthierloop`

Per `practice-tools.md` 2. MIDI events (Luthier profile inline),
audio snapshots (WAV references relative to the loop file), per-layer
settings.

## 9. `.luthiercontent`

Content update bundle. Zip container with a signed manifest inside:

```json
{
  "schema": 1,
  "magic": "luthier.content",
  "manifest": {
    "name": "Blues Tone Pack 1",
    "version": "1.0.0",
    "requires_plugin_min": "1.2.0",
    "requires_plugin_max": null,
    "files": [
      { "path": "Presets/Factory/Blues Pack/Delta Slide.luthierpreset", "sha256": "...", "size": 12842 },
      { "path": "Guitars/Factory/Resonator - Steel Body.luthierguitar", "sha256": "...", "size": 8412 }
    ]
  },
  "signature": "base64 PGP signature over manifest"
}
```

Applied files land under `~/Documents/Luthier/ContentUpdates/<name>/`
per `installer.md` 11.

## 10. `.midprofile`

User's saved MIDI-export configuration per `midi-export.md` 7.

```json
{
  "schema": 1,
  "magic": "luthier.midprofile",
  "meta": { "name": "Bandmates - Simple", "notes": "no realism, per section" },
  "config": {
    "profile": "generic",
    "ppq": 480,
    "track_split": "per_section",
    "include_realism": false,
    "sysex_redundancy": false,
    "strip_identifiers": true,
    "included_event_classes": ["NOTE", "BEND", "SLIDE", "VIBRATO"]
  }
}
```

## 11. `.mid` / `.midi`

See `midi-export.md` for the Luthier and Generic profiles. Header
chunk disambiguates.

## 12. Common metadata rules

Every JSON file's `meta` block:
- `name`: display name; unique within its category is not required
  but recommended.
- `author`: "Factory" for shipped content, otherwise free text.
- `created` / `modified`: ISO 8601 UTC.
- `version_created` / `version_modified`: plugin version strings.
- `tags`: array of short strings, used for filtering in browsers.
- `notes`: user-facing free text.

## 13. Save-atomicity rules

Every write path uses a temp-file-then-rename to avoid partial files
on crash:
1. Write to `<name>.tmp` in the target folder.
2. `fsync`.
3. Rename over the target.
4. Rename the previous target (if any) into `.backup` folder if the
   file is a preset, guitar or tune.

Backups older than 30 days are pruned by a background sweep on
startup.

## 14. Load-error handling

Per `error-recovery.md`, every load path:
1. Reads the file into memory.
2. Verifies UTF-8, JSON well-formedness, `magic` field.
3. Verifies `schema` is a known integer for that file type.
4. Runs migration if needed.
5. Validates required fields are present.
6. Validates referenced files (guitar reference, part references, IR
   references) exist; missing references get factory fallback with a
   named notification.
7. Loads on success; refuses with a named banner on any failure.

Corrupt files are never silently overwritten. A refused load leaves
the file untouched and prompts the user.

## 15. Version discipline

- New fields only added at the tail of a schema, never renamed or
  moved.
- Removed fields deprecated for at least one major version before
  removal; loader tolerates their absence.
- Semantic changes to an existing field require a schema bump and a
  migration.

## 16. Tests

- Round trip: every factory file loads, saves, matches byte-identical
  (canonical JSON formatting: 2-space indent, key order preserved from
  schema).
- Fuzz: 10 000 mutated bytes across a sample of factory files; every
  load either succeeds or refuses cleanly with no crash.
- Migration: every documented migration (preset schema 1 -> 3, guitar
  reference from name to path) resolves correctly across a fixture set
  of 200 files.
- Missing reference: load a preset that names a nonexistent guitar,
  verify banner and factory fallback.
- Atomicity: kill the plugin mid-save 100 times, verify no target file
  is corrupted (either the pre-save version or the new version
  survives, never a partial).
- Backup pruning: place files with fake dates spanning 60 days, verify
  the 30-day sweep prunes correctly.
