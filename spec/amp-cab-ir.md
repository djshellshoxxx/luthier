# AMP AND CABINET SECTION: BYPASS, QUICK IR, CPU-QUALITY SPEC

`engine.md` 11 and 13 fully specify the `AmpEngine` and `CabinetEngine`
DSP. `tone-match.md` fully specifies loading a user impulse response into
the cabinet's mic slots, the IR library, cab match and EQ match.
`mic-placement.md` fully specifies the mic geometry within a loaded IR.
`cpu-quality-modes.md` 2.1/2.3 already specifies exactly how a cabinet IR
degrades under Medium/Low. None of that is restated here.

What none of those files gives the AMP and CAB section of the rig is a
way to take either stage out of the signal path entirely, or a fast way to
pick and manage a cabinet IR without leaving the Advanced Column 3 panel
for the Column 4 TONE MATCH tab. Those are this file's two gaps: **whole-
stage bypass** for the amp and the cabinet, and a **quick IR picker**
mirrored onto the CAB panel itself, both CPU-quality aware.

Test ID prefix: **RIG-**.

## 0. Ground rules

1. **Bypass is a signal-path decision, not a DSP change.** Bypassing the
   amp or the cabinet removes that stage from the chain; it does not zero
   its parameters, mute the output, or change what a re-enabled stage
   would sound like.
2. **Bypass is click-free**, using the same crossfade class every pedal
   slot already has (`engine.md` 10, 10 ms).
3. **The quick IR picker is a mirror, not a second source of truth.**
   `tone-match.md`'s IR slot editors remain the canonical edit surface
   (`gui-integration.md` 0.1); the CAB panel's picker reads and writes the
   same slot state.
4. **Nothing here changes what plays when both stages are enabled.** A
   preset saved before this spec, loaded after it, with both stages
   enabled (the implicit prior behaviour), sounds identical.
5. **CPU-quality behaviour is inherited, not re-specified.** A bypassed
   stage costs its bypass crossfade only, at any quality level; an active
   stage's cost still follows `cpu-quality-modes.md` 2.1's table exactly.

## 1. Amp bypass

New switch `amp_bypass` (bool, default off). When on:

- `AmpEngine::processBlock` is skipped; the pre-effects output passes
  directly to the post-effects chain input, matching the actual physical
  case of running pedals into a cabinet with no amp in between (a common
  "cab as reactive load simulator" or DI-and-reamp workflow).
- The crossfade uses the same 10 ms class `PreEffectsChain`/
  `PostEffectsChain` already apply per pedal (`engine.md` 10), applied at
  the amp's insertion point.
- `AmpEngine`'s internal state (tube stage smoothers, sag envelope,
  standby fade) is frozen, not reset, while bypassed, so re-enabling
  resumes from where it left off rather than re-warming up
  (`engine.md` 11.7's standby fade is a *different* control and is
  unaffected by this switch).
- CPU cost while bypassed is the crossfade only; the amp's own DSP does
  not run.

## 2. Cabinet bypass

New switch `cab_bypass` (bool, default off). When on:

- `CabinetEngine::processBlock` (convolution, mic placement stage, ToF)
  is skipped; the amp's (or, with `amp_bypass` also on, the pre-effects
  chain's) output passes directly to `RoomEngine`.
- Same 10 ms crossfade class as section 1.
- The acoustic-guitar `AcousticMicModel` path (`mic-placement.md` 3) is
  independent of this switch: on an acoustic guitar, `cab_bypass` only
  affects the `AcousticDI` convolution stage if one is loaded, not the
  external-mic model, matching `mic-placement.md` 9's existing statement
  that placement lives upstream of the cabinet for acoustics.
- CPU cost while bypassed is the crossfade only.

## 3. Where the switches live

Per `gui-integration.md` 0.1's one-canonical-control rule:

- **`amp_bypass`**: a small bypass toggle on the **Advanced Column 3 AMP**
  panel header, in the same position a pedal slot's own bypass sits
  (`gui-integration.md` 4.3). Mirrored on the **Easy rig strip**'s Amp card
  (`gui-integration.md` 3.2 item 3) as a small LED-style switch beside the
  model name.
- **`cab_bypass`**: the equivalent control on the **Advanced Column 3 CAB**
  panel header, mirrored on the **Easy rig strip**'s Cabinet card
  (`gui-integration.md` 3.2 item 5).
- Both appear in the right-click menu's standard set
  (`gui-integration.md` 16) like any switch, and are valid MIDI Learn and
  mod-matrix targets (`midi-learn.md`, `ui-wiring.md` 12) - a footswitch
  toggling `amp_bypass` for a clean DI section of a song is a realistic
  use.
- Neither switch is a `PhysicalRange` (it is a boolean, not a physical
  quantity) and neither belongs to any `advanced-ranges.md` family.

## 4. Quick IR picker on the CAB panel

The Advanced Column 3 CAB panel (`gui-integration.md` 4.3) gains a compact
IR row, sized to fit beside the existing model/mic-1/mic-2/blend controls:

```
+-- CAB ---------------------------------------- [Bypass o] [v] --+
| Cabinet [4x12          v]   Mic 1 [...] Mic 2 [...] Blend (o)   |
| IR  [ Factory: default speaker IR      v ]  Mix (o)  [Manage->] |
+--------------------------------------------------------------------+
```

- The **IR** combo lists, in order: "Factory: default speaker IR" (no
  user IR loaded - the procedural/factory convolution `engine.md` 13
  already describes), then the tone-match.md 4 "Recent IRs" (last 20),
  then a "Browse..." entry that opens the full `tone-match.md` 6 TONE
  MATCH panel with the browser focused. Selecting a recent IR here writes
  the same cab-slot state `tone-match.md` 1 defines; it does not duplicate
  that state.
- The **Mix** knob is a mirror of the cab IR slot's Mix parameter
  (`tone-match.md` 1's per-slot "Mix (0-100%) between the built-in model
  and the user IR"). It is not a second parameter - it is the same
  automatable ID, exposed on both panels per `gui-integration.md` 0.1.
  Mix at 0% behaves exactly as `tone-match.md` already specifies (fully
  the built-in/factory response); nothing new is defined for the
  endpoints.
- **Manage ->** opens the full TONE MATCH tab (`gui-integration.md` 4.4)
  for gain trim, length trim, predelay, reverse, and the IR library
  browser - every control `tone-match.md` 1 and 6 already specify. This
  file adds no new IR-editing control; it only makes the two most common
  actions (pick a recently used IR, adjust the blend) reachable without
  leaving Column 3, per `gui-integration.md` 0.2's three-interaction rule.
- On an acoustic guitar with `AcousticDI` selected, the row's label
  changes to "Body IR" and reads/writes the body IR slot instead
  (`tone-match.md` 1's Body IR slot), matching `mic-placement.md` 6.1's
  precedent for retitling the same section by category.

## 5. CPU-quality awareness

This section states how the existing `cpu-quality-modes.md` behaviour
surfaces here; it changes no thresholds or budgets from that spec.

- The IR row's Mix knob tooltip appends the same truncation note the
  cabinet's oversampling control already carries under Options -> AUDIO
  (`cpu-quality-modes.md` 5): "Cabinet IR running at 120 ms while quality
  is Low." This reuses `cpu-quality-modes.md` 2.3's truncation variants -
  no new variant is computed for this row.
- Bypassing the amp or the cabinet (sections 1-2) reduces the block's
  measured load exactly as any other stage skip would; `CpuLoadMonitor`
  needs no special case, since it already measures wall time around
  whatever `processBlock` actually does.
- Neither bypass switch interacts with CPU quality's Auto stepping
  (`cpu-quality-modes.md` 2.7): Auto never sets or clears `amp_bypass` or
  `cab_bypass` - those remain entirely the player's own signal-routing
  choice, not a load-shedding mechanism. (Load-shedding already has its
  own named mechanisms in `cpu-quality-modes.md` 7; conflating them with a
  user-audible routing change would violate that spec's ground rule that
  quality changes are never "a tone EQ with a made-up curve" - here,
  never a made-up routing change either.)
- A bypassed cabinet still reports the same latency
  (`engine.md` 18, `cpu-quality-modes.md` 2.2's constant-latency rule):
  the cabinet's convolution partition latency is a pipeline constant the
  host has already compensated for, and flipping bypass mid-session must
  not move it. `CabinetEngine::setBypassed` therefore keeps the
  convolution engine warm (processing silence) rather than tearing it
  down, matching the "latency never follows the mode" principle
  `cpu-quality-modes.md` 2.2 already establishes for quality levels.

## 6. Serialization, undo, accessibility

**State.**
- `amp_bypass` and `cab_bypass` are ordinary boolean parameters, appended
  at the end of the parameter list per the existing convention
  (`DECISIONS.md`'s "new parameters are appended at the end"). They
  round-trip in presets, host state, snapshots and morph endpoints like
  any switch.
- The IR row's combo selection is not new state: it reads and writes
  `tone-match.md`'s existing cab-slot IR reference and Mix parameter.

**Undo** (`action-and-undo.md` 3.3, toggles):
- `amp_bypass` / `cab_bypass`: entry class `toggle`, not grouped,
  description "Turn on/off Amp Bypass" / "Turn on/off Cab Bypass."
- Picking an IR from the quick picker pushes the same undo entry
  `tone-match.md`'s own IR-load action would (this file adds no new entry
  class; it is the same action through a second door).

**Accessibility.**
- Both bypass switches are standard `AttachedSwitch` instances
  (`ui-wiring.md` 2) with accessible labels "Amp Bypass" / "Cabinet
  Bypass" and announce their on/off state like any toggle.
- The IR combo's accessible value reads the selected IR's display name;
  "Manage" is a plain focusable button.

## 7. Interactions with other specs

- **Tone Match**: canonical IR editing, library, cab match and EQ match
  are entirely `tone-match.md`'s; this file only adds the two shortcuts in
  section 4.
- **Mic placement**: unaffected by either bypass switch; a bypassed
  cabinet simply never reaches the mic placement stage, which is already
  `mic-placement.md`'s documented behaviour for "cabinet off"
  (`mic-placement.md` 6.4's empty-state row, restated here as "bypass"
  rather than "off" - the panel and its handles remain visible per that
  same row, since the cabinet can be re-enabled at any moment).
- **CPU quality**: covered in section 5; no budget or threshold changes.
- **Effects racks**: pedal-slot bypass (`engine.md` 10, 12) is unchanged
  and independent - a pedal's own bypass toggle is unaffected by
  `amp_bypass` or `cab_bypass`, and vice versa.
- **Routing** (`routing-io.md`): Aux taps that sit after the amp or after
  the cabinet still receive whatever signal is present at that point in
  the chain - silence-shaped-like-a-skip is not injected; the signal is
  simply the upstream stage's output, per Ground rule 1.
- **Randomize** (`randomize-and-ab.md`): `amp_bypass` and `cab_bypass` are
  discrete identity-level switches and are excluded from Randomize by
  that spec's section 1.3 default (discrete parameters excluded unless a
  category says otherwise; no category here does).

## 8. Failure modes

| Case | Response |
|---|---|
| Both `amp_bypass` and `cab_bypass` on | Legal; pre-effects (or raw pickup/circuit signal) reaches the room and master directly. No warning - this is a valid DI/reamp routing choice. |
| Quick-picker IR file missing on load | Same fallback `tone-match.md` 7 already specifies (built-in model, warning banner); the combo shows "Missing: \<name\>" until reassigned. |
| Bypass toggled at extreme automation rate (host stress test) | Each toggle re-triggers the 10 ms crossfade; overlapping toggles queue rather than click, matching the existing pedal-bypass behaviour under the same stress. |

## 9. Tests

1. **RIG-01** Amp bypass signal path: with `amp_bypass` on, the pre-
   effects output reaches the post-effects chain input bit-identical
   (modulo the crossfade window) to a build with `AmpEngine` removed from
   the chain entirely.
2. **RIG-02** Cab bypass signal path: with `cab_bypass` on, the upstream
   stage's output reaches `RoomEngine` unchanged, equivalently.
3. **RIG-03** Click-free toggling: toggling either switch 100 times during
   a sustained chord keeps the max sample-to-sample delta within the
   existing pedal-bypass click bound.
4. **RIG-04** State freeze, not reset: bypassing the amp for 5 seconds
   during a sustained sag/standby transition and re-enabling it resumes
   the transition rather than restarting it.
5. **RIG-05** Latency constancy: `getLatencySamples()` is unchanged across
   1000 bypass toggles of either switch, at every CPU quality level.
6. **RIG-06** CPU cost: with a stage bypassed, its module's own processing
   time drops to the crossfade-only cost measured by `CpuLoadMonitor`,
   at High, Medium and Low.
7. **RIG-07** Quick picker mirrors the canonical slot: selecting an IR
   from the CAB panel combo updates the same state `tone-match.md`'s IR
   slot editor shows, and vice versa, within one editor tick.
8. **RIG-08** Mix knob identity: the CAB panel's Mix knob and the TONE
   MATCH cab-slot Mix knob resolve to the same parameter ID and move
   together.
9. **RIG-09** Acoustic retitle: loading an acoustic guitar with
   `AcousticDI` selected relabels the row "Body IR" and reads/writes the
   body IR slot; switching to an electric guitar relabels it back.
10. **RIG-10** Auto quality independence: 60 seconds of Auto quality
    stepping under load never changes `amp_bypass` or `cab_bypass`.
11. **RIG-11** Serialization: both switches round-trip in presets, host
    state, snapshots and morph endpoints; a pre-existing preset with
    neither field loads with both defaulted to off and sounds unchanged.
12. **RIG-12** Undo: each toggle pushes one `toggle` entry with the
    correct description; Ctrl-Z restores the prior routing with no click
    beyond the crossfade bound.
13. **RIG-13** Accessibility: both switches announce label and state;
    the IR combo announces the selected IR's display name.
