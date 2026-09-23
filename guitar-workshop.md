# GUITAR WORKSHOP SPEC

The parts data model. A Luthier guitar stops being a preset with a
`guitar_type` enum and becomes a **bill of parts**: a body, a top, a neck,
a fretboard, frets, a nut, a bridge, a tailpiece, tuners, one to three
pickups, wiring, strings, a pickguard, hardware and a finish. Every one of
those is a file the user can swap, edit and save.

This is the largest change in the realism phase and the one that makes the
rest coherent. `volume-knob-interaction.md` needs the pickup's inductance;
`string-squeak.md` needs the string's winding; `fret-buzz.md` needs the
fret height and the nut slots. All of those are part fields, and without a
parts model each spec would invent its own table.

Per `CLAUDE_CODE_BRIEF.md`'s conflict list item 8, this file **supersedes
the hard-coded guitars in `engine.md`**.

`file-formats.md` 3 and 4 fix the `.luthierguitar` and `.luthierpart`
schemas. This file specifies the in-memory model, the slots, the library
and the rules. `part-acoustics.md` specifies what each field does to the
engine. `workshop-ui.md` specifies the bench.

## 0. Ground rules

1. **A guitar is its parts.** There is no property of a guitar that is not
   either a part field, a setup measurement or a finish. If the engine
   needs a number, some part owns it.
2. **Parts are files.** Factory parts ship read-only; user parts live in
   `~/Documents/Luthier/Parts/`. A part is portable between guitars.
3. **Every part swap is audible or it is not a part.** A field that
   changes nothing measurable does not belong in the model.
4. **Compatibility is advisory, not enforced.** A bass bridge on an
   electric guitar is a strange instrument, not an error. The UI warns; it
   does not refuse. Users who want a 7-string neck on a 6-string body are
   the users this feature is for.
5. **The committed spec is owned by the audio thread.** Edits arrive as
   commands and swap atomically (`ui-wiring.md` 6).
6. **Factory guitars become part files.** The 25 existing `GuitarType`
   enum entries ship as `.luthierguitar` files with factory parts. The
   enum survives only as a preset-browser shortcut.

## 1. Slots

| Slot id | Cardinality | Required | Notes |
|---|---|---|---|
| `body` | 1 | yes | Wood, chambering, dimensions |
| `top` | 0-1 | no | Carved or laminated cap |
| `neck` | 1 | yes | Wood, profile, scale length, joint |
| `fretboard` | 1 | yes | Wood, radius, thickness |
| `frets` | 1 | yes | Material, height, width, count |
| `nut` | 1 | yes | Material, width, slot depths |
| `bridge` | 1 | yes | Type, mass, coupling; whammy if any |
| `tailpiece` | 0-1 | no | Stopbar, trapeze, through-body |
| `tuners` | 1 | yes | Ratio, mass, stability |
| `pickups.neck` | 0-1 | no | Plus position and heights |
| `pickups.middle` | 0-1 | no | |
| `pickups.bridge` | 0-1 | no | |
| `wiring` | 1 | yes | Pot values, caps, switching, treble bleed |
| `strings` | 1 | yes | Gauge set, winding, core; per-string override |
| `pickguard` | 0-1 | no | Visual plus a small mass term |

Plus non-part fields on the guitar itself: `hardware_color`, `finish`,
`setup` and `character_seed` (`file-formats.md` 3).

A guitar with **zero pickups** is legal and is how an acoustic is
expressed; its output comes from the body model and any piezo in the
bridge part.

## 2. Part types and their field sets

Every part has `meta.part_type` and a `fields` object whose schema depends
on it. `part-acoustics.md` documents each field's engine effect; this is
the inventory.

| `part_type` | Key fields |
|---|---|
| `body` | `wood`, `density_kg_m3`, `chambering` (solid/chambered/semi/hollow), `thickness_mm`, `area_cm2`, `bracing` |
| `top` | `wood`, `density_kg_m3`, `thickness_mm`, `carve` |
| `neck` | `wood`, `density_kg_m3`, `profile`, `scale_length_mm`, `joint` (bolt/set/through), `truss` |
| `fretboard` | `wood`, `density_kg_m3`, `radius_mm`, `thickness_mm` |
| `frets` | `material`, `height_mm`, `width_mm`, `count`, `stainless` |
| `nut` | `material`, `width_mm`, `slot_depths_mm[]`, `friction` |
| `bridge` | `type`, `mass_g`, `coupling`, `has_tremolo`, `tremolo_type`, `spring_count`, `piezo` |
| `tailpiece` | `type`, `mass_g`, `break_angle_deg` |
| `tuners` | `ratio`, `mass_g`, `stability`, `locking` |
| `pickup` | `family`, `inductance_h`, `dc_resistance_k`, `capacitance_pf`, `magnet`, `coil_turns`, `pole_piece_material`, `cover`, `output_dbfs_reference` |
| `wiring` | `volume_pot_ohm`, `tone_pot_ohm`, `tone_cap_f`, `taper`, `treble_bleed`, `switching`, `active` |
| `strings` | `gauges_in[]`, `winding` (round/flat/half/coated), `winding_material`, `core` (round/hex), `winding_pitch_per_mm[]`, `tension_kg[]` |
| `pickguard` | `material`, `plies`, `mass_g` |
| `slide` | `material`, `mass_g`, `length_mm`, `diameter_mm` (`slide-guitar.md` 2) |
| `pick` | `material`, `thickness_mm`, `tip_radius_mm`, `bevel`, `wear` (`pick-noise.md` 2) |
| `capo` | `type` (full/partial), `mass_g`, `string_mask[]`, `pressure` |

The last three are not fitted to the guitar; they are the player's
accessories and live in their own library categories. `factory-content.md`
already ships three capo parts, which is what `B1`'s partial-capo string
mask needs.

## 3. `GuitarSpec` (the in-memory model)

Referenced by `ui-wiring.md` 6 as the thing the Workshop edits.

```
GuitarSpec {
    meta        { name, family, bodyStyle, author, tags }
    parts       PartRef[slot]        // resolved, not paths
    pickups     PickupPlacement[3]   // part + position_mm + heights
    strings     StringSet            // set + per-string overrides
    finish      Finish
    hardware    HardwareColour
    setup       Setup                // fret-buzz.md 1
    seed        uint64
    derived     DerivedAcoustics     // cached, see 3.2
}
```

- **POD and copyable.** A swap copies the spec, replaces one slot and
  publishes the copy (`ui-wiring.md` 6.2). It is small - a few hundred
  bytes plus part pointers - so copying is cheaper than locking.
- **Parts are resolved pointers**, not paths. Resolution happens on the
  message thread at load; the audio thread never touches the filesystem.
- **Owned by the audio thread**, swapped atomically.

### 3.1 The existing `GuitarSpec`

The build already has a `GuitarSpec` in `GuitarLibrary.h` populated from a
compiled-in table. It keeps its name and gains the fields above; the
compiled table becomes the fallback used when a part file is missing
(`error-recovery.md`). This is a widening, not a replacement, so every
existing call site keeps working while the parts arrive.

### 3.2 `DerivedAcoustics`

The part fields the engine actually consumes, computed once per swap by
`part-acoustics.md`'s mapping:

```
DerivedAcoustics {
    stringCoefficients[]     // per string: tension, mass/length, damping
    bodyModes[]              // frequency, Q, gain
    couplingMatrix
    pickupResponse[3]        // inductance, resonance, aperture
    circuitComponents        // volume-knob-interaction.md
    fretGeometry             // fret-buzz.md clearance inputs
    windingPitch[]           // string-squeak.md
}
```

Caching this is what keeps a part swap from being a per-block cost. It is
recomputed on the audio thread in `SwapPartCommand` step 2 and crossfaded
over 5 ms.

## 4. The parts library

```
Factory (read-only, installed):
  Resources/Parts/<Category>/<Name>.luthierpart
  Resources/Guitars/<Family>/<Name>.luthierguitar

User (read-write):
  ~/Documents/Luthier/Parts/<Category>/<Name>.luthierpart
  ~/Documents/Luthier/Guitars/<Name>.luthierguitar
```

- Scanned at startup and on folder change, into a `PartLibrary` that
  indexes by `part_type`, family compatibility and tags.
- A user part with the same name as a factory part **wins**, per
  `file-formats.md`'s rule 4 (the same rule `ControllerProfileLibrary`
  already follows).
- `factory-content.md` fixes what ships.

### 4.1 Resolution and missing parts

A `.luthierguitar` references parts by path. On load, each reference is
resolved against user then factory. A reference that resolves to nothing:

- Falls back to the **category default** (a factory part flagged
  `is_default` per category).
- Raises the `missing part` notification `gui-integration.md` 15 lists:
  "Bridge X not found, using factory default", with a jump-to-Workshop
  action.
- Is recorded in the error log (`error-recovery.md`).

The guitar loads. It does not fail. That is `error-recovery.md`'s rule and
this is the path `gui-integration.md` 15's trigger was written for.

## 5. Compatibility

Each part declares `meta.compatibility`, an array of families
(`electric`, `acoustic`, `bass`, `classical`, `resonator`, `any`).

- Fitting a part whose compatibility does not include the guitar's family
  shows a **warning**, not a refusal: "This is a bass bridge on an electric
  guitar. It will work; the spacing and mass are unusual."
- The part is fitted and the engine models it as specified. A heavy bass
  bridge on a light body produces the coupling that combination really
  produces.
- Ground rule 4. The Workshop's value is in letting people build things
  that do not exist.

### 5.1 String count mismatch

The one case that needs real handling. A 7-string neck on a guitar whose
bridge has 6 saddles:

- The guitar's string count is `min(neck.strings, bridge.strings)`.
- The excess is reported in the inspector, not silently dropped.
- Tuning and per-string state resize accordingly
  (`state-model.md`'s string-count change flow).

## 6. Save As Guitar

`gui-integration.md` 19 lists it; the shortcut is `Ctrl+G`
(`accessibility.md` 2, currently unregistered pending this file).

- Writes the current committed `GuitarSpec` to
  `~/Documents/Luthier/Guitars/<name>.luthierguitar`.
- Parts are written **by reference**. A guitar file is a bill of parts,
  not a bundle, so editing a part updates every guitar that uses it -
  which is the point.
- "Bundle parts" is an export option for sharing, writing the referenced
  parts alongside.
- The preset's `guitar.reference` is updated to the new file
  (`file-formats.md` 2).

## 7. Save As Part

Any slot's current fields can be saved as a new user part from the
inspector. This is how a user makes "my PAF with 300 fewer turns": edit
the field, save under a new name, and it becomes available to every
guitar.

Editing a **factory** part is not possible; editing its fields creates an
unsaved modification that the inspector marks and offers to save as a user
part. This is the same shape as a factory preset.

## 8. Interaction with presets

`file-formats.md` 2 gives a preset `guitar.reference` and
`guitar.override`:

- `reference` names a `.luthierguitar`.
- `override` is null normally. If the user edited the guitar without
  saving it as a file, the whole `GuitarSpec` is embedded here so the
  preset is self-contained.
- On load, `override` wins if present.

This is what stops "I tweaked the pickup height and now my preset sounds
wrong on another machine".

## 9. Parameters

**The Workshop adds no parameters.** Part fields are structural state
(`ui-wiring.md` 0.6) and flow through the command queue. Pickup position
and height are part *placement*, stored in the `GuitarSpec`, not
automatable - a user who wants to automate pickup height is asking for a
thing real guitars do not do during a performance.

The existing `pickupPosition`, `pickupHeight` parameters are **retired**
into the `GuitarSpec` when this lands, which is a parameter-count
*decrease* and therefore a schema migration: `file-formats.md`'s migration
rules apply, and old presets map those values into the spec's placement
fields.

Net parameter change: **-9** (three slots × position, height treble, height
bass), balanced against the additions from the other realism specs. The
final count is fixed by the migration note in `file-formats.md` and
asserted by `Parameters::everyParameterHasAUniqueIdAndSaneDefault`.

## 10. Tests

- **Round trip.** Every factory `.luthierguitar` loads, serialises and
  reloads to an identical `GuitarSpec`.
- **Part swap changes audio.** For each slot, swap between two factory
  parts and assert the rendered spectrum differs measurably; assert the
  same swap back restores the original render to within -80 dBFS.
- **Swap is click-free.** A part swap during a sounding note produces no
  sample discontinuity above -60 dBFS, per the 5 ms crossfade.
- **Missing part falls back and reports.** Load a guitar referencing a
  non-existent bridge; assert the category default is fitted, the guitar
  loads, the notification fires and the error log has an entry.
- **User part beats factory.** A user part with a factory part's name is
  the one resolved.
- **Incompatible parts fit.** A bass bridge on an electric produces a
  warning and a working instrument, not a refusal.
- **String-count mismatch clamps.** A 7-string neck with a 6-saddle bridge
  produces a 6-string instrument and reports the mismatch.
- **No filesystem access on the audio thread** during any swap.
- **Derived acoustics are cached.** Profile a swap and assert the mapping
  runs once, not per block.
- **Preset override is self-contained.** A preset with `guitar.override`
  loads identically on a machine with no part files at all.
