# MODULATION MATRIX SPEC

Introduces a user-facing modulation system: N sources, M destinations, a
routing matrix, and per-route depth, offset, curve and MIDI Learn.

## 0. Ground rules

1. Modulation runs at **control rate**, not audio rate. Control rate is
   1/32nd of the block size, minimum 128 samples, computed in
   `prepareToPlay`. This is the granularity of all mod source updates.
2. Between control-rate updates, destination parameters interpolate
   linearly. This eliminates zipper noise without paying for audio-rate
   modulation on every parameter.
3. Modulation is **additive over the base parameter value**, clamped to the
   parameter's range after summation.
4. A destination can be driven by up to 8 sources simultaneously. Each
   route has its own depth (-100% to +100%) and offset (-100% to +100%).
5. Sources are **stateful and reset on preset load, snapshot recall, and
   transport start**. Free-running LFOs are a per-source option.
6. The matrix is **serialized in the preset** as a compact array of route
   records, not as a full N x M matrix.

## 1. Sources

### 1.1 LFO (x 8 instances)

Per instance:
- Shape: sine, triangle, ramp up, ramp down, square, sample-and-hold, random
  smooth, custom (8-point breakpoint editor).
- Rate: 0.01 Hz to 40 Hz, or synced to host tempo (1/32 T to 8 bars, dotted
  and triplet variants).
- Phase offset: 0 to 360 deg.
- Depth: 0 to 100%.
- Symmetry (for triangle/ramp): 0 to 100%.
- Retrigger mode: free-run, retrigger on NoteOn, retrigger on transport
  start, retrigger on sync boundary.
- Smoothing: 0 to 500 ms on the output for S+H and square.
- Unipolar/bipolar output toggle.

### 1.2 Envelope generator (x 4 instances)

Per instance:
- Stages: DAHDSR (delay, attack, hold, decay, sustain, release).
- Each time stage: 0 ms to 30 s, log-scaled slider.
- Sustain level: 0-100%.
- Curve per stage: linear, exp, log, custom.
- Retrigger mode: legato, retrigger on every NoteOn, one-shot.
- Loop mode: off, D->S loop, D->R loop for evolving pads.

### 1.3 Step sequencer (x 2 instances)

- Length: 4 to 64 steps.
- Grid: 1/4, 1/8, 1/16, 1/32, and dotted/triplet.
- Per step: value (-100 to +100), gate (on/off), slide (glide to next
  step's value), probability (0-100%).
- Sync to host, tap, or internal.
- Direction: forward, reverse, ping-pong, random, brownian.
- Swing: 0-75%.

### 1.4 Envelope followers (x 2 instances)

- Source: main output, sidechain input, per-string output (see
  `routing-io.md`), pickup output.
- Attack: 0.1 ms to 500 ms.
- Release: 1 ms to 5 s.
- Detection: peak, RMS, or true-peak.
- Threshold and gate.
- Log/linear output curve.

### 1.5 Note-derived sources

- Note pitch (normalized 0-1 across MIDI range).
- Note velocity.
- Note-on trigger (single-sample impulse for use with EGs, if EG isn't
  self-triggered).
- Notes-held count (0-N, useful for chord dynamics).
- Aftertouch, poly aftertouch (per-note in MPE mode).

### 1.6 MIDI CC sources

- Any incoming CC number, including 14-bit CC pairs (MSB/LSB).
- Pitch bend, mod wheel, channel pressure.
- Program change is not a mod source, it recalls snapshots (see
  `live-performance.md`).

### 1.7 Macros (x 8)

- Named 32-char slots.
- Assignable to any control, any depth.
- Themselves modulatable (an LFO can drive a macro).
- Exposed to the host as automatable parameters `Macro 1..8`.

### 1.8 Random source

- Per NoteOn: new random 0-1 held for that note's lifetime.
- Per bar: new random 0-1 held for the bar.
- Continuous smoothed random (like an S+H LFO but with independent smoothing).
- Seeded, so a preset re-renders the same "random" values in the offline
  renderer.

## 2. Destinations

Every automatable parameter is a legal destination. That includes but is not
limited to:

- All string parameters (per string): tuning cents, damping, sustain,
  inharmonicity, coupling amount, pluck position, pluck strength, pluck
  material selector.
- All body parameters: size, depth, top thickness, air resonance, bracing.
- All pickup parameters: position, height, coil specs, magnet selection,
  blend.
- All amp parameters: gain, EQ, presence, bright, master, sag.
- All effect parameters, including bypass (bypass is a discrete destination,
  see section 4).
- Rhythm-engine parameters: voicing density, hand position, humanize amounts.
- Whammy position.
- Master volume, output pan.
- Snapshot morph position (allows LFOs to auto-morph).

Discrete destinations (amp model, pickup selection, cabinet model, snapshot
index) accept modulation but the source must cross an integer boundary to
change the value. Crossfades apply as defined in the module's spec.

## 3. Routing

A route is `{ source_id, source_output_channel, destination_id, depth,
offset, curve, enabled }`.

- `source_id` is an interned string like `"lfo1"`, `"env2"`, `"macro5"`,
  `"cc74"`.
- `destination_id` is the automation ID string of the destination parameter.
- `depth` is -100% to +100%.
- `offset` is -100% to +100%, added after depth scaling, before summing.
- `curve` is one of `Linear, Exp, Log, S, Custom(name)`.
- `enabled` is a bool, so users can quickly A/B routes.

Sum-and-clamp per destination per control-rate tick:
```
value = base_value
for each enabled route routed to this destination:
    v = source.output_for(route.source_output_channel)
    v = apply_curve(v, route.curve)
    value += (v * route.depth + route.offset) * parameter_range
value = clamp(value, param_min, param_max)
```

Base value is whatever the user set on the control. Automation from the host
is applied to the base value before modulation.

## 4. Discrete destinations

Bypasses, selectors and switches accept modulation via a threshold rule:
```
active_index = floor(mod_position * option_count + 0.5)
```
Bypass toggles at mod position 0.5.

For selectors, the module's crossfade rule (already specified per module in
engine.md) governs the audible transition.

## 5. UI

A dedicated Mod Matrix panel accessible from Advanced mode, Column 4, tab
labelled MOD.

Layout:
- Left third: source pool. Each source is a card with its own compact editor
  (LFO shape, envelope stages, step grid).
- Right two thirds: routing table. Columns: source, destination, depth,
  offset, curve, enabled, delete.
- Add-route button. Dragging a source card onto a destination control in
  the plugin also creates a route (source and destination auto-populate).
- Right-clicking any control in the plugin opens a "Modulate" submenu
  listing all sources, which creates a route on selection.
- Depth is shown as a coloured arc around modulated controls in the main
  UI, matching the source's colour tag.

Colour coding: each source has a user-assignable colour from the theme
palette. Routes and mod arcs inherit the source colour. Default assignments
alternate through the accent palette.

## 6. Preset and snapshot integration

- Full modulation state is stored in every preset under `modulation`.
- Snapshots (see `live-performance.md`) can be marked "snapshot includes
  modulation state" or "snapshot uses preset-level modulation only".
- Preset export includes the modulation section; import validates route
  destination IDs and warns on unknown ones (parameters removed in a later
  version).

## 7. Automation vs. modulation

Distinguish clearly to the user:
- **Host automation** writes to the base value. It moves the on-screen
  control.
- **Modulation** adds on top of the base value. It does not move the on-
  screen control; it shows as a coloured arc.
- Both stack.

## 8. Tests

- Every source produces the mathematically expected output for a defined
  input signal (LFO frequency accuracy, EG stage times, S+H hold-time).
- 1000-route stress test: verify per-block CPU cost is under budget
  (control-rate updates keep this well below 1% at 8 sources * 1000 routes
  at 128-sample blocks).
- Preset round trip: save a preset with a full matrix, load, verify every
  route's depth/offset/curve is byte-identical.
- Discrete destination boundary test: modulate a 5-option selector with a
  ramp, verify it changes value at 1/5, 2/5, 3/5, 4/5 positions.
- Determinism: with the random source seeded, offline renders are byte-
  identical across runs.
