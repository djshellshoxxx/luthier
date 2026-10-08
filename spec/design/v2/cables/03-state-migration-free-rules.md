# CB-E01, CB-M01: State, migration and Free rules

**Items:** CB-E01, CB-M01 (shared with PB-M01)
**ED:** 3
**Source:** `cables-and-brands.md` 6, 7, 8, 10 (CB-M01, CB-E01), 11; `pedalboard-v2.md` 5.1

## Summary

Define the board-block schema for cable and pedal tiers, migrate v1 presets so they render bit-identically, and enforce Free's Standard-only rule on writes.

## Status

Missing. v1 presets have no board block. v1 guitar-cable parameters exist (`Source/Parameters.h`: `cable_quality`, `cable_length`, `cable_on`, `noise_*`) and are unchanged.

## User-facing behaviour

- Old presets open unchanged. They gain an empty board with Daisy power and no patch cables.
- New patch cables and pedals default to Standard.
- Free: the tier popovers show Standard only. Other tiers are greyed with "Pro". Free can set Standard on any cable or pedal. Free cannot select Unmodelled for a new object.
- Free can open a Pro board. See Design for the rules.

## Engine / DSP design

No DSP. This file defines the data that the other files read.

- **Compile step.** `BoardCompiler` (message thread, `pedalboard-v2.md` 3.3) reads the board block and produces an immutable `CompiledBoard` snapshot. All tier-derived values (gains, biquads, impedances, pop buffers, hum levels) are computed there. File 04 defines the swap.
- **Migration rule.** A v1 preset (no board key) compiles to: zero patch cables, `powerMode = daisy`, and the v1 input cable. The v1 path is then the same code path as an all-Unmodelled board, so CB-M01 holds by construction.

## Data model and parameters

Board block (JSON in preset state). Stable string IDs, never array indices. All keys are append-only.

```
board:
  schema: 2                       // int, bumped once for this spec
  powerMode: "daisy" | "isolated" | "mixed"     // default "daisy"
  patchCables: [
    { id, tier, lengthM, balanced, coiled, label }   // see below
  ]
  pedals: [
    { id, type, ..., tier, bypassMode, popEnabled, label,
      lift?, isolate?  }          // lift and isolate for type 24 only
  ]
```

Field ranges:

| Field | Type | Range | Default (new) | Default (migrated) |
|---|---|---|---|---|
| `tier` | string enum | unmodelled, economy, standard, pro, boutique | standard | unmodelled |
| `lengthM` | float (m) | 0.3-10.0 | 1.0 (D) | 1.0 |
| `balanced` | bool | - | false | false |
| `coiled` | bool | - | false | false |
| `label` | string | 0-32 chars, user-typed | empty | empty |
| `bypassMode` | string enum | soft, buffered, true | soft (v1) | soft |
| `popEnabled` | bool | - | true | true |
| `powerMode` | string enum | daisy, isolated, mixed | daisy | daisy |
| `lift` | string enum | off, lift, isolate | off | - |

- Tier names are the generic strings in ground rule 1. The app writes no brand name. `label` is user text and is never supplied by the app.
- Tier is stored as the string, not an index, so a reordered enum cannot change meaning.
- `schema` is an integer on the board block. A board with `schema` absent is version 1 and migrates on load.

## State, file format and migration

- **Load.** Missing `board` key: migrate as above. Missing per-object keys: use the migrated default in the table. Unknown tier string: treat as Standard and mark the object for re-save; never fail the load.
- **Save.** Always write `schema: 2` and all keys. Writing a v2 preset with only v1 content still writes the board block (so the file is self-describing).
- **Round trip.** Save, load, save must produce byte-identical JSON (test below).
- **Forward compatibility.** Unknown keys are preserved on save.
- **v1 presets.** `cable_quality` and `cable_length` keep their values and meaning. The microphonic mapping has an open point (file 01) that must be settled before the migration is final.

## Edition gating

| Rule | Free | Pro |
|---|---|---|
| Write tier | Standard only | all |
| Unmodelled on a migrated object | Read-only; may be changed to Standard | all |
| Buffer, Ground Isolator (type 23, 24) | not selectable | yes |
| Power mode | Daisy only | all |
| Load a Pro board in Free | renders stored tiers; edits limited to Standard; Buffer and Isolator render as transparent and show locked | full |

The Free load rule is a design decision: the spec does not say what Free does with a Pro board. Rendering the stored tier avoids silent data loss. The transparent-Buffer rule is the only way to honour "no Buffer or Isolator" without changing the signal path of a saved board. Confirm with the spec owner.

## Performance budget

No per-sample cost. Compile cost is part of `pedalboard-v2.md` 3.3.

## Test plan

- **CB-M01 (migration).** An all-Unmodelled v1 preset (fixtures from the existing preset set) renders bit-identically to the pre-v2 build. Shared with PB-M01.
- **CB-E01 (Free).** In a Free build, writing any tier other than Standard is refused, and no Buffer or Isolator can be added. Asserted through the state API, not only the UI.
- Added: JSON round trip is byte-identical for a board with every field set.
- Added: a v1 preset without a `board` key loads with `powerMode = daisy`, zero patch cables, and all pedals Unmodelled.
- Added: an unknown tier string loads as Standard and does not throw.
- Added: a Pro board loaded in Free renders bit-identically to Pro for the stored tiers (only edit limits differ).

## Effort and dependencies

- ED 3.
- Depends on: `pedalboard-v2.md` 5.1 (board state) and 3.3 (compile); `cables-and-brands.md` 6.1 and 7; files 01, 02, 05, 06, 07, 08, 09 (fields they read).
