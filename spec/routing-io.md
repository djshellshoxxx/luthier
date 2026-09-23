# ROUTING AND IO SPEC: multi-out, sidechain, MIDI out, re-amp

Extends spec.md's "Effects / signal chain" section. Turns Luthier from a
stereo-in-stereo-out plugin into a routable studio instrument.

## 0. Ground rules

1. Bus layout is negotiated at plugin instantiation via
   `BusesProperties`. The plugin advertises multiple layout options; the
   host picks one.
2. The plugin must **run correctly at every advertised layout**, including
   the minimal stereo-out case.
3. All extra outputs are **post-master-limiter** unless labelled as tap
   points. Tap points are pre-effects and are documented as such.
4. Sidechain input is **optional and gain-neutral by default**. Its
   presence changes the modulation matrix's `EnvFollower/Sidechain` source
   from silent to live.
5. MIDI out is **sample-accurate to input MIDI** where the plugin is
   pass-through, and sample-accurate to the internal event stream where the
   plugin is generating events (rhythm engine, arpeggiator).

## 1. Advertised bus layouts

Layout A (default, always supported):
- Input: stereo (unused unless sidechain is not otherwise available).
- Output: main stereo.

Layout B (multi-out studio):
- Input: stereo (main sidechain).
- Output: main stereo + 7 auxiliary stereo pairs (see section 2).

Layout C (per-string):
- Input: stereo.
- Output: main stereo + up to 12 mono outputs, one per string.

Layout D (studio + per-string):
- Input: stereo.
- Output: main stereo + 7 aux stereo + up to 12 mono per-string.

Hosts that only support stereo I/O get Layout A. Hosts with flexible
routing get D.

## 2. Auxiliary output assignments (Layout B, C, D)

| Aux bus | Signal | Tap point |
|---|---|---|
| Aux 1 | DI (raw pickup output, post-cable-sim) | pre-amp |
| Aux 2 | Amp output pre-cabinet | post-amp, pre-cab |
| Aux 3 | Cabinet mic 1 only | post-cab |
| Aux 4 | Cabinet mic 2 only | post-cab |
| Aux 5 | Room mic only | post-room |
| Aux 6 | Wet effects only (reverb/delay tails) | post-post-fx |
| Aux 7 | Monitor bus (see live-performance.md) | monitor path |

Each aux bus has its own gain trim in the routing panel. Muted aux buses
do not process their tap point (skip the render).

## 3. Per-string outputs (Layout C, D)

- Twelve mono buses, `String 1` (typically high E) through `String 12`.
- Signal is the raw string engine output, post-body, pre-pickup.
- Useful for external per-string processing (per-string amp, per-string
  effects, MIDI-guitar-style workflows).
- If the current guitar has fewer than 12 strings, unused buses output
  silence.
- Latency of per-string buses is reported to the host as the guitar's own
  engine latency (no additional delay).

## 4. Sidechain input

- Any signal routed to the sidechain input becomes available as:
  - Modulation source `SidechainEnvFollower` (see `modulation-matrix.md`).
  - Ducking source for a "sidechain compressor" pedal, if instantiated in
    the effects chain.
  - Monitor input for the practice tools' backing-track summing.
- Sidechain never sums into the guitar output unless the user places a
  pedal or module that specifically mixes it (e.g., a stereo blend).
- Sidechain metering is shown in the routing panel.

## 5. Re-amp workflow

Two ways to re-amp:

**A. External re-amp**: user renders Aux 1 (DI) to a file, sends through
their own outboard, records the return.

**B. Internal re-amp**: user loads a rendered DI clip as sidechain input;
Luthier processes it through its own amp/cab as if it were live pickup
output. Requires the "Sidechain-to-amp" toggle in the routing panel, which
patches the sidechain input directly into the amp input, replacing the
string engine's contribution.

Internal re-amp is silent by default; the toggle exists precisely because
some users will want it and most won't.

## 6. MIDI output

Sends MIDI on:
- **Note pass-through**: everything the plugin receives on MIDI in, echoed
  to MIDI out with the same timestamps.
- **Rhythm engine output**: the transformed event stream from
  `rhythm-engine.md`, so users can bounce a strum performance to MIDI.
- **String activity**: per-string NoteOn/Off events reflecting the actual
  strings that are ringing, useful for driving a second Luthier instance,
  a light rig, or a tab renderer.
- **CC broadcast**: any macro can be echoed as a CC. User assigns the CC
  number in the routing panel.

MIDI out uses standard host mechanisms (`MidiBuffer` on the process call).
On hosts that do not support MIDI out from an instrument plugin, this
silently no-ops.

## 7. Latency reporting

Report the correct latency for each output separately when the host
supports per-output latency:
- Main out: sum of oversampling group delay + partitioned convolution
  latency + any lookahead limiter.
- Aux 1 (DI): oversampling group delay only.
- Aux 2 (pre-cab amp): oversampling group delay + pre-cab modules.
- Per-string outs: minimum, engine-only latency.

On hosts that only accept a single latency value, report the main-out
latency (largest). Users doing DI-based workflows can compensate manually.

## 8. UI

New panel, `ROUTING`, in Advanced mode's Column 4 tab strip.

Contents:
- Layout selector (shows which layouts the host advertises as supported).
- Aux bus strip: 7 channel strips, each with mute, solo, gain, and a
  meter.
- Per-string strip (compact, only shown when Layout C or D is active).
- Sidechain: meter, "sidechain to amp" toggle.
- MIDI out: enable, source checkboxes (pass-through, rhythm, string
  activity, CC), CC-mapping table.
- Small latency-report readout showing the plugin's reported latency on
  each active output.

## 9. Preset and snapshot integration

- Layout is not stored in the preset (host owns bus layout). The plugin
  restores mute/solo/gain per aux bus from the preset.
- MIDI out assignments are per-preset.
- Sidechain-to-amp state is per-preset.

## 10. Tests

- Each layout (A, B, C, D) instantiates cleanly, produces expected sample
  count on each output for a known input.
- Aux 1 DI vs. main out: null test after external re-application of the
  amp module should agree within -60 dBFS (accounting for oversampling
  differences).
- Per-string outputs: for a chord voicing, sum of per-string outputs
  equals main out pre-body within -80 dBFS.
- MIDI out pass-through: for a fuzz of 10 000 random MIDI events, echoed
  events match input timestamps within 0 samples.
- Sidechain-to-amp: routes correctly, string engine outputs silence on the
  aux buses tied to string activity.
- Latency: reported latency for main matches measured latency (delta impulse
  in, delta out) within 1 sample.
