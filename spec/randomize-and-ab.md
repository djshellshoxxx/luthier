# RANDOMIZE AND A/B COMPARE SPEC

Both live in the header (`gui-integration.md` 2: the Overflow region's dice,
Ctrl+R; the Preset controls region's "A / B", Ctrl+/) and both already have
a one-line mention and a keyboard binding, but neither has a behavioural
spec. `gui-integration.md` 2 says only that "A / B compare holds two
transient parameter states separate from the snapshot bank... it does not
serialize; snapshots persist in the preset," and `advanced-ranges.md` 5
mentions a `randomise_respects_stock` preference in passing. This file is
the rest of both: what Randomize actually touches and how it stays
musical rather than chaotic, and exactly what A/B holds, how flipping
between the two stays click-free, and what happens to edits made in each.

Test ID prefix: **RAND-** for Randomize, **AB-** for A/B compare.

## 0. Ground rules

1. **Randomize is a starting point, not a slot machine.** A press should
   produce something a player might actually want to keep, not a
   parameter-space explosion. Every randomized value is biased toward
   where it already was, not drawn uniformly from its full range.
2. **Randomize never touches structure.** It moves continuous and
   discrete *parameter* values within the categories the player has
   enabled. It never swaps the guitar, changes tuning or capo, adds or
   removes a mod route or a pedal, or changes any file reference. Those
   are `Randomize`'s explicit non-goals, not omissions.
3. **Advanced ranges already decided the boundary.** `advanced-ranges.md`
   5's `randomise_respects_stock` preference (default on) is Randomize's
   only range rule; this file does not add a second one.
4. **A/B is two RAM-only preset buffers, not two separate features.**
   Flipping between them reuses every click-free mechanism the engine
   already has for the fields that actually differ (parameter smoothing,
   pedal-slot crossfade, guitar-swap park) - it introduces no new
   crossfade mechanism.
5. **Neither feature is undoable in the ordinary sense**, per
   `action-and-undo.md` 7's existing rule that A/B flipping is "a
   viewport, not a change." Section 4 states exactly what each does push.

## 1. Randomize: scope

### 1.1 Categories

The dice's chevron opens a small popover (the same affordance pattern as
`midi-learn.md` 3's mappings list) listing the categories Randomize can
touch, each with its own checkbox, default states shown:

| Category | Default | Covers |
|---|---|---|
| Tone | on | Amp EQ/gain/presence, `GuitarCircuit` volume/tone, pre/post effect *parameters* (not slot contents) |
| Character | on | `character-wear.md` amounts, `pick-noise.md`, `string-squeak.md`, `fret-buzz.md` amounts (not their file/material choices) |
| Feel | on | Strum/fingerpick feel, humanize, crossing velocity, evenness |
| Rig levels | off | Cabinet/room mix and blend levels only - never the model choice |
| Modulation depth | off | Existing mod route depths only - never adds, removes or retargets a route |

Off by default means "Rig levels" and "Modulation depth" opt in, because a
surprise cabinet-blend or mod-depth jump is more likely to be unwanted
than a tone/character/feel nudge. A category with no members in the
current build (e.g., Modulation depth when no routes exist) is simply a
no-op, not hidden - consistent with `gui-integration.md` 0.7's rule that a
panel a user cannot use is hidden, but a *checkbox* that currently has
nothing to do is still informative.

### 1.2 Amount

A single "Amount" slider, 0-100%, default 40%, in the same popover.
Amount scales how far a category's values can move from their current
position, not the odds of moving at all - every enabled, unlocked
parameter in an enabled category is touched on every press.

### 1.3 The distribution ("musical, not chaotic")

For each eligible parameter with current value `v`, live range
`[lo, hi]` (the stock range unless `randomise_respects_stock` is off, per
`advanced-ranges.md` 5):

```
spread = Amount * (hi - lo)
new_v  = clamp(v + spread * triangularSample(-1, 1), lo, hi)
```

`triangularSample(-1, 1)` peaks at 0 and falls linearly to the edges - the
same shape a fader "nudge" would produce, so most presses make a
noticeable but not disorienting change, and a repeated press at low Amount
tends to explore near the current sound rather than jump around it.
Discrete parameters (amp model, pedal type choices already in a slot, pot
value dropdowns) are **excluded by default** - Randomize moves knobs, not
identities - unless a category explicitly says otherwise (none do at
ship; a future category could).

### 1.4 Locks

Per `gui-integration.md` 0's spirit that nothing important is hidden, any
control can be excluded from Randomize individually via its right-click
menu (`gui-integration.md` 16 gains no new item; "Reset to default" is
already there - a parameter locked out of Randomize is simply one whose
current value the player wants kept, which is what dragging Amount to 0
for its whole category already achieves at a coarser grain). At ship,
locking is per-category via the popover checkboxes only; a per-control
lock is left as a documented future extension rather than added here,
since `gui-integration.md`'s right-click menu is fixed at thirteen items
and this file does not reopen it.

### 1.5 What Randomize never touches, regardless of category

Guitar reference and `GuitarSpec` overrides, tuning, capo, temperament,
the `ranges` block, MIDI mappings, mod-matrix route topology (source,
destination, curve - only depth, and only when enabled), snapshot
contents, effects-rack slot contents (adding/removing/reordering a pedal),
CPU quality, UI scale, and any `uiState` field. This list exists once,
here, rather than being repeated per category.

## 2. A/B compare: what the two slots hold

### 2.1 Contents

Each slot (A, B) holds an in-memory `PresetData` - the same structure a
saved `.luthierpreset` would serialize (`file-formats.md` 2): parameters,
modulation, effects-rack contents, MIDI mappings, the `ranges` block, and
the guitar reference/override. On a preset load, **both slots are set to
the freshly loaded preset**, so the first press of A/B does nothing
audible until one slot has been edited - matching the existing line in
`gui-integration.md` 2 that A/B is "separate from the snapshot bank," not
a way to jump between two different songs.

### 2.2 Active slot

Exactly one slot is **active** at any time (shown by a highlight on the
A or B letter in the header). Every edit the player makes - a knob move, a
pedal swap, a guitar change, a mod route edit - applies to the live engine
state as normal and is simultaneously the active slot's content, because
the active slot is not a separate copy: it is a label for "this is what
the engine currently holds." There is no independent edit buffer to keep
in sync.

### 2.3 Flipping

Pressing the header A/B control (or Ctrl+/) switches which slot is
active:

1. The current live state is captured into the slot that is *about to
   become inactive* (it is already that slot's content per 2.2, so this
   is bookkeeping, not a copy operation with a cost).
2. The target slot's stored `PresetData` is applied via the same
   diff-and-swap the preset loader already uses (`ui-wiring.md` 5), with
   one difference: **only fields that actually differ between the two
   slots move.** A parameter equal in both slots is left alone, so a
   flip's audible click risk is bounded by what actually changed, not by
   the whole parameter count.
3. Each differing field uses **its own existing click-free mechanism**:
   continuous parameters use their normal smoothing ramp; a differing
   pedal type or slot content crosses via the rack's existing 10 ms
   bypass/change crossfade (`engine.md` 10); a differing guitar reference
   uses the existing 5 ms park-and-rebuild (`ui-wiring.md` 6.1,
   `DECISIONS.md`'s guitar-swap entry); a differing `ranges` block applies
   per `advanced-ranges.md` 1.2/1.3's own widen/narrow rules, including
   its clamp notification if narrowing moved a value.
4. No new crossfade mechanism is introduced by this spec; A/B compare is
   entirely a matter of computing a diff and replaying each field through
   the mechanism that field already owns.

### 2.4 Copy and reset

The A/B popover (opened from the same chevron pattern as section 1.1,
`midi-learn.md` 3) offers:
- **Copy A -> B** / **Copy B -> A**: overwrites the inactive slot with the
  active slot's current content. Useful before making an exploratory
  change, so the "before" is preserved in the other letter.
- **Reset both to loaded preset**: restores both slots to what they held
  immediately after the last preset load, discarding edits in both.

### 2.5 What A/B does not do

- **It does not serialize.** Saving the preset (`Ctrl+S` /
  `Ctrl+Shift+S`) writes only the active slot's content, exactly as
  `gui-integration.md` 2 already states. The inactive slot's edits are
  lost if the player closes the instance without flipping back and
  saving from there too - the mappings popover and the header highlight
  both make clear, at all times, which slot is about to be the one that
  saves.
- **It is not a second snapshot bank.** The eight snapshots
  (`live-performance.md`) are a performance feature that persists in the
  preset; A/B is a two-slot scratchpad that does not.
- **It does not survive a plugin instance close** (like undo,
  `action-and-undo.md` 10) - a fresh instance always starts with both
  slots equal to whatever preset it loads.

## 3. UI

- Header **Preset controls** region (`gui-integration.md` 2): the existing
  "A / B" control becomes two small letter buttons, the active one
  highlighted with the accent outline (matching the snapshot strip's
  active-button convention, `gui-integration.md` 8). Clicking the inactive
  letter flips to it. A small chevron beside the pair opens the Copy/Reset
  popover (section 2.4).
- Header **Overflow** region's dice (`gui-integration.md` 2): a plain
  click runs Randomize immediately at the current category/Amount
  settings (so the common case - "surprise me" - is one click, matching
  `gui-integration.md` 0.2's three-interaction rule). A chevron beside the
  dice opens the categories/Amount popover (section 1.1-1.2); the popover
  remembers its settings across presses within the session
  (`uiState`, not the preset).
- Both popovers close on Escape or outside click, matching every other
  popover in the window.
- **Live Mode** (`gui-integration.md` 9): Randomize's dice and its popover
  are not shown on the Live Strip - a surprise tone change mid-performance
  is exactly what Live Mode's locked Advanced-Mode-toggle rule exists to
  prevent. A/B remains available on the Live Strip (it repeats in the
  header per `gui-integration.md` 8/9), since flipping between two known,
  prepared states is a legitimate live move.

## 4. Undo

- **Randomize** is a single multi-target undo entry, `randomize`,
  capturing the before-value of every parameter it touched, the same
  shape as a preset load's multi-target entry (`action-and-undo.md` 6).
  It is **not** a state boundary - Ctrl-Z reverses it in one step like any
  other multi-target entry, without requiring Shift.
- **A/B flip** pushes no undo entry (`action-and-undo.md` 7): it is a
  viewport change over state that already existed in both slots. Undoing
  an edit made while a given slot was active reverses that edit in the
  live engine state regardless of which slot is active when Ctrl-Z is
  pressed, because the undo stack tracks parameter values, not slots.
- **Copy A<->B and Reset both** are not undoable, matching A/B flipping's
  own rule (they move slot *bookkeeping*, and any resulting audible change
  happens through a subsequent flip, which is itself not undoable). A
  player who copies over a slot they wanted to keep can only recover it by
  not having flipped away - the popover's confirmation before "Reset both"
  is the safeguard, not undo.

## 5. Interactions with other specs

- **Advanced ranges**: Randomize's Amount respects `stockMin/stockMax`
  exactly as `advanced-ranges.md` 5 already specifies; A/B's diff-and-swap
  applies `ranges` block changes per that spec's own widen/narrow
  mechanism (section 2.3).
- **MIDI Learn**: a learned CC and Randomize can both write the same
  parameter; Randomize's write is a one-off undo-tracked event like a
  mouse drag, not a standing mapping, so there is no conflict to resolve
  beyond the ordinary last-writer rule.
- **CPU quality**: neither feature is stored in a preset field that CPU
  quality touches, and neither is scaled by quality level - both are
  message-thread, low-frequency operations.
- **Preset morph** (`ambiguity-resolutions.md` 5): a separate mechanism
  (continuous interpolation between two *saved* presets). A/B does not
  read or write `preset_morph_position`, and turning on preset morph does
  not disturb the A/B slots' contents, though it does change what the
  engine is currently playing - a flip immediately after would diff
  against whatever the morph left behind, which is exactly the documented
  "current live state" behaviour in 2.2.
- **Tuning reference** (`tuner-and-tuning-reference.md`):
  `tuning_reference_hz` is a normal parameter and is eligible for A/B's
  diff (it is instrument setup, not sound, but A/B does not special-case
  it - a player comparing two full states reasonably wants the reference
  compared too) and is excluded from Randomize under section 1.5's guitar-
  and-tuning exclusion.

## 6. Tests

1. **RAND-01** Category scope: with only Tone enabled, a press changes no
   parameter outside the Tone category's list.
2. **RAND-02** Distribution: over 10 000 presses at a fixed Amount, the
   resulting value distribution for a mid-range parameter is triangular
   around its starting value with the documented spread, and no value
   ever falls outside its live range.
3. **RAND-03** Respects stock range: with `randomise_respects_stock` on,
   1000 presses on an advanced-unlocked family never produce a value
   outside stock; with it off, some do (`advanced-ranges.md`'s own test,
   re-asserted here against the dice specifically).
4. **RAND-04** Never touches structure: 1000 presses across every
   category combination never change the guitar reference, tuning, capo,
   `ranges.families`, MIDI mappings, mod-route topology, effects-rack slot
   contents, or any `uiState` field.
5. **RAND-05** Undo: a press creates exactly one multi-target `randomize`
   entry; Ctrl-Z restores every touched parameter to its prior value in
   one step, with no boundary confirmation required.
6. **RAND-06** Popover state: category and Amount choices persist across
   presses within a session and reset to the documented defaults in a
   fresh instance.
7. **AB-01** Load resets both: loading a preset sets A and B to identical
   content; a flip immediately afterward produces no audible or parameter
   change.
8. **AB-02** Diff-only swap: editing three parameters while A is active,
   then flipping to B, changes only those three parameters (plus any
   field that already differed) and leaves every equal field's value
   untouched (verified by a per-parameter write-count instrument).
9. **AB-03** Click-free flip: a flip that changes a continuous parameter,
   a pedal-slot content, and the guitar reference simultaneously produces
   no click beyond each field's own existing crossfade bound (reusing the
   click thresholds already asserted in `ui-wiring.md` 23 and the
   guitar-swap tests).
10. **AB-04** Copy and reset: Copy A->B makes B's subsequent flip a no-op;
    Reset both restores both slots to the last-loaded preset's content.
11. **AB-05** Save writes only the active slot: with A active and edited
    and B holding different content, saving the preset and reloading it
    reproduces A's content exactly; B's content is not present anywhere in
    the saved file.
12. **AB-06** Undo independence from slot: an edit made while A is active,
    followed by a flip to B and back to A, still undoes with Ctrl-Z
    regardless of which slot was active when undo was pressed; flipping
    itself never appears in the undo history.
13. **AB-07** Session boundary: a fresh plugin instance loading the same
    preset file starts with A and B equal, with no memory of a previous
    instance's slot contents.
14. **AB-08** Live Mode: the Randomize dice and its popover are not
    reachable from the Live Strip; the A/B control is, and flipping during
    playback meets the AB-03 click bound.
