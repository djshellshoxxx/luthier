# ADVANCED RANGES SPEC

Every physical parameter in Luthier has two ranges: the **stock** range,
which is what the real object can do, and the **advanced** range, which is
what the model can do. A pot that a real guitar builds at 250 kΩ or 500 kΩ
has a stock range of 100 k to 1 M and an advanced range of 1 k to 10 M.
Both are useful. Only one of them is honest.

This file exists because those two goals fight. A $200 modelled guitar has
to sound like a guitar when a player opens it and turns things, and it also
has to let someone who wants a 40-inch scale length with a 10 MΩ tone pot
go and get one. Hiding the second is timid. Defaulting to it is why modelled
instruments sound like synths.

The resolution is that **the stock range is the default and the advanced
range is opt-in per preset**, and that leaving stock is always visible and
never silent.

`PhysicalRange` is the mechanism. It is specified here and every later
realism spec depends on it, which is why this file is first in phase 2.

## 0. Ground rules

1. **Stock is the default.** A new preset, a factory preset and a
   first-run install are all stock. Advanced is something a user turns on.
2. **Advanced is marked, never hidden.** Leaving stock changes what the
   control looks like (`gui-integration.md` 9 and 21). A user can always
   see that a value is outside what a real instrument does.
3. **Range mode never changes audio by itself.** Switching a family to
   advanced widens the limits and leaves every current value alone.
   Switching back to stock clamps, which does change audio, and is
   therefore undoable and announced.
4. **Range mode belongs to the preset, not to the user.** It travels in
   the preset file so a preset sounds the same on someone else's machine.
   The *preferences* about how ranges are displayed belong to the user.
5. **A parameter is physical or it is not.** Physical parameters model a
   real object and get a `PhysicalRange`. Everything else - mix amounts,
   enables, indices, macro assignments - keeps a single range. Inventing a
   "stock range" for a wet/dry control is noise.
6. **No parameter count changes.** `PhysicalRange` replaces how a
   parameter's limits are computed. It does not add, remove or renumber
   parameters, because hosts index saved automation against that list.

## 1. `PhysicalRange`

```
PhysicalRange {
    stockMin, stockMax        double
    advancedMin, advancedMax  double
    defaultValue              double
    unit                      Unit          // ui-wiring.md 2's unit enum
    family                    RangeFamily
    skew                      double = 1.0  // applies to whichever range is live
}
```

Invariants, asserted at construction:

- `advancedMin <= stockMin < stockMax <= advancedMax`. The advanced range
  contains the stock range. A parameter whose advanced range is not a
  superset is a specification error, not a runtime case.
- `stockMin <= defaultValue <= stockMax`. A default outside stock would
  make a fresh preset start out marked.
- Both ranges are non-degenerate.

A parameter is **live** in one of two modes. In stock mode its host-visible
range is `[stockMin, stockMax]`; in advanced mode it is
`[advancedMin, advancedMax]`.

### 1.0 Stock is the range the parameter already shipped with

**Presets store normalised values, not plain ones.** `PresetManager`
writes `parameter->getValue()`, which is 0-1 against whatever range was
live when it was saved, and reads it back the same way.

That has a consequence which governs this whole file: **changing a shipped
parameter's range silently re-maps every preset ever saved.** A preset
storing 0.8 against a 0.5-15 m cable range means 12.1 m; re-declare that
range as 1-10 m and the same 0.8 now means 8.2 m. Nothing errors. The
preset just sounds different.

So the rule is:

> For a parameter that already exists, the **stock range is exactly its
> current declared range**. The advanced range extends beyond it. New
> parameters, arriving with the later realism specs, declare both fresh.

This is principled rather than merely convenient. The ranges in the build
were chosen as the sensible full travel of each control, which is what
"stock" means. Honouring them means no preset ever re-maps, no schema
migration is needed, and the legacy derivation in 4.1 becomes trivially
correct: a preset saved before advanced ranges existed used only stock
ranges, because there was nothing else.

It also fixes a constraint going forward: **once a parameter ships, its
stock range cannot change without a preset migration.** Widening into
advanced is always safe; moving `stockMin` or `stockMax` is not.

### 1.1 Normalisation and host automation

This is the part that has to be got right, because hosts store automation
as a normalised 0..1 value and a range change silently re-maps every one of
them.

**The normalised value is always taken against the live range.** A host
automation lane at 1.0 means "this parameter's maximum", and when the range
widens the maximum moves. That is the behaviour a user expects from the
DAW's point of view and it is the behaviour JUCE's `NormalisableRange`
gives for free.

The consequence is stated plainly rather than worked around: **changing a
family's range mode while automation is written against it will move the
plain values that automation produces.** Because of that:

- Range mode is a **structural** change (`ui-wiring.md` 5 and 9), applied
  through the command queue, not a parameter.
- Switching mode writes the current *plain* values back through the new
  normalisation, so the sound at the instant of the switch is unchanged.
  Only future automation is re-mapped.
- `qa-polish.md`'s automation test matrix gains one case per family.

### 1.2 Switching to advanced

For each parameter in the family:

1. Read the current plain value `v`.
2. Widen the live range to the advanced pair.
3. Write `v` back as plain, which produces a new normalised value.

No clamping happens, because the advanced range contains the stock range.
Audio is bit-identical across the switch.

### 1.3 Switching to stock

1. Read the current plain value `v`.
2. Narrow the live range to the stock pair.
3. Write `clamp(v, stockMin, stockMax)` back as plain.

This **can** change audio, and every parameter it moved is reported: the
switch produces a notification banner listing how many values were clamped,
and the undo entry restores them. A silent clamp here is the exact failure
ground rule 0.2 exists to prevent.

### 1.4 Modulation and the live range

Modulation is applied in `ParameterBridge::value` and is expressed in
normalised units against the live range. A mod route at depth 1.0 sweeps
the whole live range, so **the same route sweeps further in advanced mode**.

That is correct and deliberate: the route says "sweep this control fully",
and what "fully" means is what the range says. `state-model.md` 8.6 already
commits to automation behaving this way; modulation follows the same rule so
there is one story rather than two.

## 2. Range families

A family is the unit a user locks and unlocks. Families are coarse on
purpose: per-parameter locking is available through the right-click menu,
and a list of forty families would be a worse RANGES tab than seven.

`file-formats.md` fixes the seven family keys. This file fixes what is in
each.

| Family | Covers |
|---|---|
| `amp` | Amp gain, EQ, presence, master, bias, sag, output-stage terms |
| `circuit` | `GuitarCircuit` pots, caps, treble bleed, cable capacitance, input impedance (`volume-knob-interaction.md`) |
| `squeak` | Finger-slide squeak amount, probability, moisture, pressure (`string-squeak.md`) |
| `buzz` | Action, relief, nut depth, fret height, buzz threshold (`fret-buzz.md`) |
| `pick` | Pick thickness, tip radius, bevel, angle, wear, click and chirp amounts (`pick-noise.md`) |
| `slide` | Slide mass, pressure, slant, noise and clank amounts (`slide-guitar.md`) |
| `modulation` | LFO rate, envelope times, sequencer rate, follower attack/release |
| `mic` | (mic-placement.md, FEAT-MIC) Cabinet mic distance and angle, acoustic mic distance and angle (`mic_dist`, `mic_angle`, `ac_mic_dist`, `ac_mic_angle` and their `_2`): stock 0-100 cm / 0-90 deg (acoustic 0-100 cm), advanced 0-200 cm / 0-180 deg (acoustic 0-300 cm) |

Parameters not in a family are non-physical and have a single range. The
instrument's own geometry - scale length, string gauge, pickup position -
belongs to the `GuitarSpec` and the Workshop
(`guitar-workshop.md`), not to the parameter list, and is ranged by
`part-acoustics.md` rather than here.

### 2.1 The `modulation` family is the odd one, twice over

First, an LFO rate is not a physical object. It is in the list because the
same argument applies: 0.01-20 Hz is what a player uses, 0.001-200 Hz is
what a sound designer occasionally wants. Its stock range is a taste
judgement rather than a measurement, and this file says so.

Second, and more importantly for the implementation: **the modulation
sources are not parameters.** LFO rate, envelope times and sequencer rate
live in the `ModMatrix` as structural state and are serialised in the
preset's `modulation` block (`file-formats.md` 2), not in `parameters`.

So the `modulation` family cannot work by swapping a parameter's
`NormalisableRange`. It works by **clamping the mod-source setters**:
`ModMatrix::setLfoRate` and its siblings clamp to the stock pair unless
the family is advanced. The observable behaviour is the same - a value
outside stock is refused while locked, and preserved while unlocked - but
the mechanism is a clamp in a setter rather than a range swap.

`file-formats.md` fixes the seven family keys, so the family stays; this
section records that one of the seven is implemented differently from the
other six.

## 3. Stock and advanced values

The numbers below are the ones the implementation uses. Where a real
instrument has a measurable range, stock is that range; where it does not,
stock is the range within which the control behaves like the thing it is
named after.

### 3.1 `amp`

The amp controls are normalised 0-1 in the build, not 0-10 as a front
panel would be printed. That is deliberate and it stays: re-scaling them
would change the meaning of every saved automation lane and every stored
preset value for a cosmetic gain. The stock range is therefore the panel's
full travel, and advanced is past the end of the knob.

| Parameter | Stock | Advanced | Unit |
|---|---|---|---|
| `amp_gain` | 0 – 1 | 0 – 2 | normalised |
| `amp_bass`, `amp_mid`, `amp_treble` | 0 – 1 | -0.5 – 1.5 | normalised |
| `amp_presence` | 0 – 1 | 0 – 2 | normalised |
| `amp_master` | 0 – 1 | 0 – 2 | normalised |

Negative EQ is cut beyond what the tone stack can do; gain and master
above 1 drive the stage past its modelled maximum.

### 3.2 `circuit` (see `volume-knob-interaction.md`)

| Parameter | Stock | Advanced | Unit |
|---|---|---|---|
| `circuit_volume_pot` | 100k – 1M | 1k – 10M | ohm |
| `circuit_tone_pot` | 100k – 1M | 1k – 10M | ohm |
| `circuit_tone_cap` | 10n – 100n | 1n – 1µ | F |
| `circuit_treble_bleed` | 0 – 1 | 0 – 1 | ratio (non-physical within family) |
| `cable_length` | 0.5 – 15 | 0 – 100 | m |
| `amp_input_impedance` | 220k – 1M | 10k – 10M | ohm |

### 3.3 `squeak`, `pick`, `buzz`, `slide`

Those four families' parameters are defined by their own specs, which name
the stock and advanced pair for each in their parameter tables. This file
fixes only the rule they follow: **stock is the range a real player and a
real object produce, advanced is the range the model will still compute
without instability.** A parameter whose advanced range would make the
model blow up does not get a wider advanced range; it gets the same pair
twice and a note saying why.

### 3.4 `modulation`

Applied as setter clamps, not range swaps — see 2.1.

| Field | Stock | Advanced | Unit |
|---|---|---|---|
| LFO rate | 0.01 – 20 | 0.001 – 200 | Hz |
| Envelope attack / decay / release | 0.1 – 5000 | 0.01 – 60000 | ms |
| Sequencer rate | 0.1 – 40 | 0.01 – 400 | Hz |
| Follower attack / release | 0.1 – 1000 | 0.01 – 10000 | ms |

### 3.5 What exists today

Of the seven families, only `amp` and `circuit` have parameters in the
current build, and `circuit` has only `cable_length` until
`volume-knob-interaction.md` lands. `squeak`, `buzz`, `pick` and `slide`
acquire their parameters with their own specs' modules.

The registry is therefore built to be **sparse and additive**: a parameter
with no `PhysicalRange` entry is non-physical and keeps its single range,
and a family with no members is legal and reads as stock. Each later
realism spec adds its rows without touching this mechanism.

## 4. The preset's `ranges` block

Schema is fixed by `file-formats.md`:

```json
"ranges": {
  "families": {
    "amp": "stock", "circuit": "stock", "squeak": "stock",
    "buzz": "stock", "pick": "stock", "slide": "stock",
    "modulation": "stock"
  },
  "per_control_unlocks": []
}
```

- `families` maps each of the seven keys to `"stock"` or `"advanced"`.
  An absent key reads as `"stock"`. (mic-placement.md, FEAT-MIC): an
  eighth key, `"mic"`, follows the same rule.
- `per_control_unlocks` is an array of parameter IDs that are advanced
  regardless of their family's mode. A control listed here in a family that
  is already advanced is redundant and is dropped on save.
- There is no per-control *lock* list. A control cannot be stock inside an
  advanced family, because the reason to unlock a family is that its
  members interact and locking one of them back produces a combination the
  model was not asked about. The right-click "Restrict to stock range for
  this control" item, which `gui-integration.md` 16 lists, therefore
  removes the control from `per_control_unlocks` and is only offered when
  the control is in that list.

### 4.1 Loading a preset that has no `ranges` block

Every preset written before this spec has no block, and its stored values
were written against the old single ranges - which, for most parameters,
are what this file now calls advanced.

Treating those presets as stock would clamp them and **change the sound of
every preset already saved**. Treating them all as advanced would preserve
the sound and put a padlock on presets that are entirely ordinary.

Neither is acceptable, so the rule is per family and derived from the file:

> On loading a preset with no `ranges` block, a family is set to
> `"advanced"` if and only if at least one stored value in that family
> falls outside its stock range. Every other family is `"stock"`.

The preset sounds exactly as it did, and the padlock tells the truth. The
block is written on the next save, so the derivation happens once.

Because of 1.0, a preset written before this spec existed can only contain
values inside stock - its ranges *were* the stock ranges - so in practice
the derivation returns all-stock for every legacy file. The rule is
written generally anyway, because it is also what should happen to a
preset hand-edited to hold an out-of-stock value, and because a later
parameter whose stock range is genuinely narrower than something already
shipped would need it.

**The derivation reads plain values, not the stored normalised ones.** A
normalised value carries no information about which range it was written
against; it is always in 0-1 and therefore always "inside" whatever range
is live. So the derivation runs *after* the parameters have been set, on
what they actually hold.

`error-recovery.md`'s load path gains no new failure mode: a malformed
`ranges` block is treated as absent and the derivation above runs.

## 5. Interaction with other state

- **Snapshots never carry range mode.** `ui-wiring.md` 5 already says this.
  A snapshot recalled inside a preset uses the preset's ranges. A snapshot
  captured while advanced and recalled after the family was locked has its
  out-of-range values clamped on recall, like any other value.
- **A/B slots** each carry their own `ranges` block, because each slot is a
  preset.
- **Randomise** respects stock range by default. The preference in Options
  is `randomise_respects_stock`, default on. With it off, randomise uses the
  live range, which in an advanced family means it can produce values a real
  guitar cannot - which is the point of turning it off.
- **Reset to default** always writes the parameter's `defaultValue`, which
  is inside stock by invariant, and never changes range mode.
- **MIDI Learn** maps a CC across the live range. Widening a family
  re-scales what an existing mapping reaches, by the same argument as
  automation in 1.1.

## 6. UI

### 6.1 Marking (`gui-integration.md` 9, 21)

- The portion of a knob's value arc past `stockMax` (or below `stockMin`)
  is drawn in the warning colour.
- A readout showing a value outside stock gains a `*` suffix.
- A tab whose panel holds any parameter currently outside stock shows a
  padlock in its header: secondary accent when the family is unlocked,
  muted when locked.
- The marking is driven by **the value**, not by the mode. A family that is
  unlocked but whose values all sit inside stock shows no warning arcs -
  there is nothing unusual about the sound, so nothing is marked. The
  padlock still shows, because the *preset* is unlocked.

### 6.2 Options → RANGES

Four things, per `gui-integration.md` 5:

1. **Master per-preset toggle.** Unlocks or locks all seven families at
   once. Locking shows the clamp count before it commits.
2. **"Always show marked values as warning colour"** preference. Default
   on. Off draws the arc in the normal accent and keeps only the `*`, for
   users who find the warning colour loud.
3. **"Randomise respects stock range"** preference. Default on.
4. **Out-of-stock summary.** A list of every parameter in the loaded preset
   whose value is outside stock, with its value, its stock range and a
   button that clamps that one parameter. Empty state: "Every value in this
   preset is inside its stock range."

Preferences 2 and 3 are user-global (`UiPreferences`), not preset state.

### 6.3 Trying to leave stock while locked

`gui-integration.md` 14 fixes the message:

> This preset uses stock ranges. Options -> Ranges to unlock, or
> right-click to unlock this control only for this preset.

Shown as an inline notice at the control, not a banner - it answers
something the user just did, at the place they did it.

The control does not move. A drag that would leave stock stops at
`stockMax` rather than refusing the whole gesture.

### 6.4 First unlock explainer

`onboarding.md` 7 fixes the wording and the once-only behaviour. It fires
on the first transition of any family or control to advanced, not on the
first *attempt* while locked.

## 7. Undo

Per `action-and-undo.md` 3.11: entry class `ranges-toggle`, grouped by
target within 200 ms, not a state boundary.

Undoing a **lock** must restore the clamped values, so the entry carries
them. Undoing an **unlock** only narrows the range back; no value moved, so
there is nothing to restore beyond the mode.

## 8. Telemetry

`updates-telemetry.md`'s usage category already reports booleans for
Workshop and Slide. It gains `advanced_ranges_used`, true when any family
or control in the loaded preset is advanced. No parameter values are sent -
the point is to learn whether the feature is found at all, not what people
set.

Options → Diagnostics mirrors this boolean (`gui-integration.md` 5).

## 9. Performance

Range mode is read once per parameter when it changes, not per block.
`PhysicalRange` adds two doubles and an enum to each physical parameter's
description and nothing to the audio path: the live `NormalisableRange` is
the same object the parameter already had.

`performance-budget.md` needs no new allowance.

## 10. Tests

- **Invariants.** Every `PhysicalRange` in the build satisfies
  `advancedMin <= stockMin < stockMax <= advancedMax` and
  `stockMin <= default <= stockMax`. This is a sweep over the whole
  parameter list, so a later spec that adds a parameter with a bad pair
  fails here rather than in a listening test.
- **Widening is silent.** For each family: set values at stock min, middle
  and max; switch to advanced; assert every plain value is unchanged to
  within 1e-9 and that a rendered block is bit-identical.
- **Narrowing clamps and reports.** Set a value outside stock in advanced
  mode, switch to stock, assert the value equals `stockMax` (or `stockMin`),
  and that the reported clamp count is 1.
- **Undo restores a clamp.** After the above, undo and assert the original
  value returns.
- **Normalisation follows the live range.** In stock mode, normalised 1.0
  reads back as `stockMax`; after widening, normalised 1.0 reads back as
  `advancedMax`, and the *plain* value written before the switch is
  unchanged.
- **Legacy load derives per family.** Construct a preset var with no
  `ranges` block and one value outside stock in `amp` only; load it; assert
  `amp` is advanced, the other six are stock, and every value is unchanged.
- **Legacy load of an ordinary preset stays stock.** Same with every value
  inside stock; assert all seven families are stock.
- **Marking follows the value.** A parameter inside stock in an unlocked
  family reports no marking; the same parameter moved past `stockMax`
  reports marking.
- **Randomise respects stock.** With the preference on, a thousand
  randomise passes over an advanced family produce no value outside stock.
  With it off, at least one does.
- **Snapshots do not carry mode.** Capture in advanced, lock the family,
  recall, assert the recalled values are clamped and the family is still
  locked.
