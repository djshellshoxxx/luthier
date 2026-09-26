# MIDI LEARN SPEC

`ui-wiring.md` 8 already specs the wiring for one mapping: arm, right-click
a control, catch the next CC. `gui-integration.md` 16's right-click menu
already lists "MIDI Learn" as item 5 on every control, and its
feature-to-location index already has a row for it
(`Support::MidiLearn`, header button, right-click, Ctrl+L). What is
missing is everything a working MIDI Learn feature needs once more than
one mapping exists: a second, direct entry point that does not require
pre-arming the header first; MPE-aware mapping so a member-channel
dimension can drive a parameter, not only a CC number; a place to see and
manage every mapping at once; and the exact rules for conflicts,
per-preset versus global mappings, and how ranges and modulation interact
with a learned CC. This file specifies all of that on top of the existing
wiring, without changing it.

Test ID prefix: **LEARN-**.

## 0. Ground rules

1. **Every automatable parameter is a valid learn target.** If a control
   has a canonical UI location (`gui-integration.md` 0.1), it can be
   MIDI-learned there, and nowhere else, matching the single-canonical-
   surface rule for right-click, mod-matrix drag and accessibility focus.
2. **Learning is never the only way to see a mapping.** The right-click
   menu is a shortcut (`gui-integration.md` 0.4); the mappings list
   (section 3) is the visible, non-right-click surface every mapping is
   also reachable from.
3. **A mapping never touches the audio thread's parameter path directly.**
   Learned CC values still flow to `ParameterBridge` exactly like host
   automation (`ui-wiring.md` 4.1); MIDI Learn only decides *which*
   incoming CC or MPE dimension writes to *which* parameter ID.
4. **One control, one mapping.** Learning a second source onto an
   already-mapped control replaces the mapping (with confirmation if the
   old one has a custom label); the same source can drive multiple
   controls at once (a single CC broadcasting to several parameters is
   legitimate and common - a "brightness" CC feeding both `amp_treble`
   and `mic_angle`).
5. **Never fights automation.** A learned CC and host automation on the
   same parameter follow `action-and-undo.md` 11's existing last-writer
   rule; MIDI Learn adds no special case.

## 1. Sources a mapping can bind to

Extending `ui-wiring.md` 8's "next non-note MIDI event":

| Source class | Identified by | Typical use |
|---|---|---|
| CC (0-127) | CC number + channel (or "any channel" / "any MPE member") | Standard controller knob or pedal |
| Pitch bend | Channel (or "any MPE member") | Whammy-style continuous control from a non-guitar controller |
| Channel pressure (aftertouch) | Channel (or "any MPE member") | Vibrato depth, sustain-style controls |
| **MPE per-note dimensions** (`controllers.md` 1) | Zone + dimension (Y = CC 74 / timbre, per-note pitch, per-note pressure), **aggregated** across the zone's active notes | A macro that should respond to "how hard is the player pressing right now", not one member channel |
| Note on/off (a specific key or a range) | Note number, velocity ignored or used as depth | A footswitch-style controller sending a fixed note; drives a toggle-class parameter (bypass, snapshot-like behaviour is out of scope - snapshots already have their own MIDI mapping in `live-performance.md`) |

**MPE aggregation** (new, since a knob has one value and MPE has one value
per active note): the learn dialog offers **Highest note**, **Lowest
note**, **Most recent note**, or **Mean of active notes**. Default is Most
recent, matching the "sticky" bias `controllers.md` 4 already uses for
string assignment. With no notes active, an aggregated source holds its
last value rather than dropping to zero, so a sustained macro does not
jump when the player releases.

**Note-class mappings** target only parameters with a natural on/off
reading: switches (`AttachedSwitch`), and continuous parameters via a
"jump to value" (a note maps a control to one specific value, e.g. "note
36 sets `amp_bright` to 1.0") - the same pattern a footswitch preset-morph
trigger would use.

## 2. Two entry points, one flow after arming

### 2.1 Header arm, then click (existing, `ui-wiring.md` 8)

1. Header MIDI Learn button or Ctrl+L arms global learn mode.
2. The next control clicked (not right-clicked - a plain click while armed)
   receives the arm flag, matching `ui-wiring.md` 8 exactly.
3. Proceeds as section 2.3.

### 2.2 Direct, per-control (`gui-integration.md` 16 item 5)

1. Right-click any control -> "MIDI Learn".
2. That control receives the arm flag immediately, with no header state
   change (the header button does not light).
3. Proceeds as section 2.3.

Both paths converge on the same command, so nothing downstream
distinguishes how a control was armed.

### 2.3 Catching the source (shared)

1. `ArmMidiLearnCommand { on: true, param_id: X }` sent (`ui-wiring.md` 8).
2. The armed control shows a pulsing outline (`AnimationPolicy`
   `Decorative`; a steady outline at Low / reduced motion,
   `cpu-quality-modes.md` 6) and the status line reads "Move a control on
   your MIDI device...".
3. The audio thread listens for the next qualifying MIDI event from
   section 1. Two or more different CCs arriving within the first 150 ms
   of arming (a pedal or fader that sends a burst) are treated as one
   gesture; the one with the largest value swing wins, so bumping a
   multi-turn encoder does not learn a stray adjacent controller.
4. `MidiLearnedResult { param_id, source }` posted, where `source`
   captures whichever row of section 1's table applies.
5. Arming times out after 20 seconds with no qualifying event: the outline
   stops pulsing, the status line reads "MIDI Learn cancelled - no input
   received," and no mapping is created. Escape cancels immediately.
6. On success, a small transient confirmation replaces the status line
   ("Learned: CC 74 -> Amp Treble") for 3 seconds.

## 3. Mapping list and management

New popover, **MIDI Learn mappings**, opened from a chevron on the header
MIDI Learn button (`gui-integration.md` 2's Utility region) - satisfying
`gui-integration.md` 0.4 without adding a Column 4 tab, whose order is
fixed:

```
+-- MIDI LEARN MAPPINGS --------------------------- [x] --+
| CC 74  ch 1        -> Amp Treble             [x]        |
| CC 11  any ch       -> Master Volume          [x]        |
| Pitch bend  MPE any -> Whammy Amount          [x]        |
| Note 36  ch 10      -> Bypass: Doubler        [x]        |
| MPE Y (mean)  zone 1 -> Character Amount      [x]        |
+------------------------------------------------------------+
| [ Clear all ]                    [ Save all as global ]  |
+------------------------------------------------------------+
```

- Each row: source description, target's display name, and a remove
  button. Clicking the row's target name focuses that control
  (`gui-integration.md` 0.1's canonical surface) and briefly highlights it,
  the same "focus source" behaviour `ui-wiring.md` 22's test already
  expects from right-click.
- **Clear all** removes every mapping in the current preset (with
  confirmation above 3 mappings).
- **Save all as global** is the bulk form of `ui-wiring.md` 8's per-mapping
  "Save as global" flag (section 4).
- Empty state: "No MIDI mappings yet. Right-click any control and choose
  MIDI Learn, or press Ctrl+L and click a control."
- The popover is read-only for source/target pairing - to change a target,
  remove the mapping and re-learn; this avoids inventing a second editing
  affordance for something the learn gesture already does well.

## 4. Global versus preset mappings

`ui-wiring.md` 8 already establishes that mappings live in the preset by
default, with a per-mapping "Save as global" flag that moves one to the
user-global settings file (`ui.json`-family store, alongside
`ui-scaling.md`'s preference). This file fixes the resolution order:

1. On preset load, the preset's own `midi_mappings`
   (`file-formats.md` 2) are installed.
2. Global mappings are then layered on top for any source not already
   claimed by a preset mapping. A source claimed by both uses the
   preset's mapping while that preset is loaded, so a preset author's
   intent is never silently overridden by a standing global mapping.
3. Global mappings persist across preset loads; a fresh preset with no
   `midi_mappings` still responds to them, which is the point of making a
   mapping global (a volume pedal on Master Volume that should work no
   matter what preset is loaded).
4. The mappings popover (section 3) shows both kinds in one list, global
   ones marked with a small globe glyph, so a player is never confused
   about why an "unmapped" preset already responds to their pedal.

## 5. Ranges and modulation

- A learned CC always maps across the target's **live** range
  (`advanced-ranges.md` 1.4's rule for modulation applies identically
  here): widening a family in advanced mode re-scales what the mapping
  reaches, exactly as it re-scales a mod route.
- A control that already has a mod-matrix route keeps it; the learned CC
  and the mod route both write toward the parameter through the normal
  automation-atomics path (`ui-wiring.md` 4.1), and the last write wins
  per block, same as a CC racing host automation.
- MIDI Learn cannot target a mod-matrix source's own settings (LFO rate,
  envelope times) - those are structural state (`ui-wiring.md` 0.6), not
  parameters, and are out of scope for a spec whose whole mechanism is
  "which CC writes which parameter ID."
- MIDI Learn cannot target the CPU quality mode
  (`cpu-quality-modes.md` 3, "MIDI Learn cannot target the mode" is
  already that spec's own rule) or any non-parameter `uiState` field (tab
  selection, Live Mode, palette).

## 6. Serialization, undo, accessibility

**State.**
- Preset mappings round-trip via `.luthierpreset`'s existing
  `midi_mappings` array (`file-formats.md` 2); each entry gains an
  optional `mpe_dimension` and `mpe_aggregation` field for section 1's new
  source classes, and an optional `note_target_value` for note-class
  mappings. Absent fields mean "ordinary CC," so every mapping written
  before this spec loads unchanged.
- Global mappings live in a new array in the same store `ui-scaling.md`
  and `cpu-quality-modes.md` use for machine/user preferences
  (`Documents/Luthier/config/`), as `midi_mappings.json`, schema 1, magic
  `luthier.midi-mappings`, same shape as the preset array.
- Snapshots, A/B slots and morph endpoints do not carry mappings
  separately - mappings belong to the preset as a whole, matching
  `advanced-ranges.md` 5's rule for the `ranges` block.

**Undo** (`action-and-undo.md` 3.12, already reserved for this feature):
- Learning a mapping: entry class `midi-learn`, not grouped.
- Removing a mapping (single or via Clear all, which is one multi-target
  entry): entry class `midi-mapping-delete`.
- Saving as global (per-mapping or bulk) is a preference move, not a
  sound change, and is **not undoable** - matching the CPU quality
  preference's own "not undoable, it is an option" rule
  (`cpu-quality-modes.md` 9); the mapping itself remains undoable via the
  entries above.

**Accessibility.**
- The mappings popover is a list with the standard role and each row's
  accessible label is its full description ("C C 74, channel 1, maps to
  Amp Treble").
- Arming is announced ("MIDI Learn armed for Amp Treble; move a control on
  your MIDI device"); a successful catch and a timeout are both announced.
- The header MIDI Learn button and its chevron are both in the header's
  focus order (`gui-integration.md` 17); the popover follows the Options
  overlay's Escape/outside-click convention.

## 7. Failure modes

| Case | Response |
|---|---|
| Arm with no MIDI input connected (Standalone) | Timeout message names the missing input: "No MIDI input selected. Options -> MIDI." |
| Two CCs arrive in the arming burst window from different controllers | Larger swing wins (section 2.3); the other is ignored for this arm |
| A global mapping's target parameter does not exist in the loaded preset's parameter set (should not happen - the parameter list is fixed, but a corrupt file could claim otherwise) | Mapping skipped, logged, surfaced in Diagnostics, not applied |
| `midi_mappings.json` missing or corrupt | No global mappings; rewritten cleanly on the next "Save as global" |
| Preset and global both claim the same source | Preset wins while that preset is loaded (section 4.2) |

## 8. Tests

1. **LEARN-01** Header arm-then-click and direct right-click both produce
   an identical mapping for the same control and source.
2. **LEARN-02** Every automatable parameter in the build can be learned
   and resolves to its one canonical control when focused from the
   mappings list, matching `ui-wiring.md` 22's existing focus-source test.
3. **LEARN-03** CC catch: arm, send each of 128 CCs on channel 1, verify
   each maps to the intended parameter within one block
   (`ui-wiring.md` 23's existing bound, re-asserted here for both entry
   points).
4. **LEARN-04** Burst resolution: two CCs within 150 ms of arming, the
   larger-swing one is learned.
5. **LEARN-05** Timeout: arming with no input for 20 s cancels cleanly
   with no mapping created and the correct status text.
6. **LEARN-06** MPE aggregation: for each of Highest / Lowest / Most
   recent / Mean, a fixture with three overlapping MPE notes produces the
   documented aggregate value, and releasing all notes holds the last
   value rather than resetting to 0.
7. **LEARN-07** Note-class mapping: learning note 36 onto a switch toggles
   it on note-on and leaves it alone on note-off (or on a second press, if
   the control is latching per `ui-wiring.md`'s `AttachedSwitch`
   momentary/latching distinction).
8. **LEARN-08** One control, one mapping: learning a second source onto an
   already-mapped control replaces it; the same source mapped to two
   different controls drives both.
9. **LEARN-09** Global versus preset resolution: a global mapping and a
   preset mapping on the same source, with the preset loaded, only the
   preset's mapping is active; unloading that preset for one with no
   conflicting mapping activates the global one.
10. **LEARN-10** Serialization: preset `midi_mappings` (old shape and new
    shape with `mpe_dimension`) both load correctly;
    `midi_mappings.json` round-trips; a legacy preset with the old shape
    is unaffected.
11. **LEARN-11** Range interaction: widening a mapped parameter's family
    to advanced re-scales what the same CC value reaches, matching
    `advanced-ranges.md` 1.4's modulation test.
12. **LEARN-12** Undo: learning and deleting mappings push the documented
    entries; Clear all is one multi-target entry; Save-as-global is not on
    the undo stack.
13. **LEARN-13** Mappings popover: lists every mapping with correct
    source/target text, global glyph on global entries, Clear all and
    Save-all-as-global both work, empty state text is correct.
14. **LEARN-14** Accessibility: arming, catching and timing out each
    produce the documented announcement; every mapping row's accessible
    label matches its display text.
15. **LEARN-15** No fight with automation: a host automation lane and a
    learned CC on the same parameter each write correctly when the other
    is silent, and the last writer wins when both write in the same
    block, per `action-and-undo.md` 11.
