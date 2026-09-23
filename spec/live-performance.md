# LIVE PERFORMANCE SPEC: snapshots, morph, program change, tap, monitor mix

Extends spec.md's "Preferences" and header controls. Makes Luthier usable as
a live rig running on a laptop or in a live-host plugin without a mouse.

## 0. Ground rules

1. Every live control must be reachable **without a mouse**: keyboard
   shortcut, MIDI CC, MIDI PC, or external controller assignment.
2. Snapshot switching must be **glitch-free**: no clicks, no dropped notes,
   no silence gap. Held notes continue on their old string state until
   released; new notes take the new state.
3. Tap tempo is **plugin-global** and available even when the host provides
   no clock.
4. The kill switch must interrupt output within **one block** of activation,
   and restore within one block of release.
5. Nothing on the live surface may open a modal dialog. Confirmations happen
   in-line, dismissable by the same button.

## 1. Snapshot banks

A snapshot is a full parameter state, saved in-preset. A preset holds up to
128 snapshots, addressable by index.

Snapshot data:
- All parameter values (same schema as a preset, minus preset metadata).
- Modulation-matrix routing state (see `modulation-matrix.md`).
- Effect bypass states.
- Rhythm engine state (see `rhythm-engine.md`).
- A user label (up to 32 chars).
- A colour tag (16 colours, matches theme accents).

Snapshot recall behaviour:
- Held MIDI notes are not retriggered.
- Amp/preamp state is crossfaded over `snapshot_xfade_ms` (default 30 ms,
  range 0-500 ms).
- Effect bypass changes take effect on the crossfade midpoint.
- Delay and reverb tails are preserved through the crossfade (double-buffer
  the tail modules).
- Sympathetic coupling matrix is recomputed on a worker thread; if it isn't
  ready by the crossfade midpoint, the old matrix is held one extra block.

Storage: snapshots live inside the `.luthierpreset` file under a `snapshots`
array. A preset with no snapshots implicitly has one "Default" snapshot equal
to its top-level parameters.

## 2. Snapshot addressing

Snapshots can be recalled by:
- MIDI Program Change (PC): PC value = snapshot index. Bank Select (CC 0)
  selects the preset.
- MIDI CC: user assigns any CC to "next snapshot", "previous snapshot",
  "snapshot number by CC value".
- Keyboard: `[` and `]` step through, digits 1-9 recall snapshot 1-9,
  `Shift+digit` recalls 10-18, etc.
- On-screen: snapshot strip in the header shows the current bank of 8 with
  the active one lit.
- Foot controller: any MIDI foot controller can be learned via the standard
  MIDI Learn flow.

## 3. Snapshot morphing

Morphing is a continuous interpolation between two snapshots, driven by a
source (expression pedal CC, LFO, mod wheel, external CV via
sidechain-envelope in `routing-io.md`).

Rules:
- Only continuous parameters interpolate. Discrete parameters (amp type,
  pickup selection, cabinet model) hard-switch at 0.5 morph position.
- Discrete parameters can be excluded from the morph on a per-parameter
  basis, in which case they take the value from snapshot A regardless.
- Bypass states are discrete and follow the 0.5 rule unless excluded.
- Morph curve is user-selectable: linear, S-curve, exponential, custom
  4-point Bezier.

Morphing is exposed as a dedicated `Morph` control in the header:
- Two snapshot slots (A and B).
- A large morph knob or 0-1 pedal input assignment.
- A morph enable button.

## 4. Setlist

A setlist is an ordered list of `.luthierpreset` references. It lives outside
the preset system so it survives preset edits.

- Location: `~/Documents/Luthier/Setlists/*.luthierset`.
- File format: JSON with `name`, `notes`, `bpm_default`, and an ordered array
  of `{ preset_path, snapshot_index, notes }`.
- Navigation: `PageUp`/`PageDown` on keyboard, or user-assigned CCs.
- Header shows current setlist entry, previous, and next in a triptych.
- Pre-loads the next entry into a background buffer so switching is
  gap-free.

## 5. Tap tempo

Plugin-global tap tempo drives:
- Rhythm engine when host is not playing.
- Any delay pedal with the "sync" toggle on.
- Any LFO with the "sync" toggle on.

Behaviour:
- Detects tempo from the last 4 taps within a 3 s window.
- Median-averages tap intervals, discards outliers > 30% from the median.
- Range: 20-300 bpm.
- Snap to nearest integer bpm if within 0.4 bpm.
- Assignable to any CC, footswitch, or key.
- Sends visual feedback: the header LED (see theme.md) blinks on each beat
  in the tap accent colour.

When the host is playing, host tempo wins and tap is ignored, unless the
user has forced "internal tempo" in Options.

## 6. Kill switch

A momentary control that hard-mutes the output for as long as it is held.

- Fades to silence over 3 ms on press.
- Fades back to full over 3 ms on release.
- Does not affect internal DSP state; the string engine keeps decaying
  underneath.
- Assignable to any footswitch, CC, or key (default: `\`).
- Visible as a large red pill in the header when active.

## 7. Monitor mix

For guitarists monitoring themselves through the plugin, a dedicated monitor
signal path independent of the main out:

- Sums the main out with a user-provided sidechain input (backing track,
  drummer, band mix).
- Independent monitor level, monitor pan, and monitor EQ (three-band).
- Optional "click" bus that only routes to monitor, driven by rhythm
  engine's metronome (see `practice-tools.md`).
- Routes to a second stereo output pair when the host supports it (see
  `routing-io.md`).

Monitor path is bypassed and consumes no CPU when no sidechain input is
present and monitor level is at -inf.

## 8. Expression pedal calibration

Expression pedals send CC over MIDI but their ranges vary. A calibration
routine solves this:

- User opens Options -> Expression -> Calibrate.
- UI prompts: "heel down", user rocks pedal fully back, confirms.
- UI prompts: "toe down", user rocks pedal fully forward, confirms.
- Store min and max raw CC values; map to 0.0-1.0 internally.
- Also stores a dead-zone at each end (default 3% at heel, 5% at toe).
- Applies a user-selectable curve: linear, log, exp, S-curve.

Calibrations are per-CC-number, stored globally, not per-preset. A single
pedal calibration follows the user across all presets.

## 9. Panic behaviour

Panic (in header, or CC assignment, or key `P`) does:
1. All-notes-off on every string.
2. Reset all effect tails.
3. Reset amp DC and any oscillating feedback loops.
4. Reset the coupling matrix state.
5. Does **not** change the current snapshot or preset.
6. Does **not** reset user-editable parameters (that is the Reset function).

## 10. UI surface

A collapsible "Live" strip attaches to the bottom of the header when Live
Mode is on:
- Snapshot strip (8 buttons + prev/next).
- Setlist triptych.
- Tap tempo pad.
- Morph knob with A/B slots.
- Kill switch pill.
- Monitor level knob.

Live Mode is on/off in the header. When on, the plugin also:
- Increases minimum control hit-target to 44 px for touch use.
- Suppresses non-critical tooltips.
- Locks the "Advanced" mode toggle to prevent accidental switching.

## 11. Persistence and portability

- Snapshots travel inside the preset.
- Setlists travel as `.luthierset` files.
- Pedal calibrations are user-global, in
  `~/Documents/Luthier/config/expression.json`.
- Live-mode preference (on/off) is per-preset.
- MIDI learn assignments for live controls are per-preset by default but can
  be marked "global" to persist across presets.

## 12. Tests

- Snapshot recall: automated fuzz that recalls 1000 random snapshot pairs
  during playback, verifies zero clicks (peak sample-to-sample delta <
  threshold).
- Program Change routing: send PC across all 128 values, verify snapshot
  index maps correctly.
- Morph interpolation: for each parameter type, verify interpolation curve
  matches the selected shape within 0.001 tolerance at 100 sample points.
- Tap tempo: feed 10 tap sequences with known intervals + noise, verify
  detected bpm within 0.5 bpm.
- Kill switch: verify output drops below -80 dBFS within 5 ms of activation
  and recovers within 5 ms of release, over 1000 activations.
- Setlist: build a 50-preset setlist, walk it forwards and backwards, verify
  no memory growth per pass.
