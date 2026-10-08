# NOISE FLOOR SPEC

A real rig is never silent: pickup hum, preamp hiss, whatever the room
and the cable add. A model that forces it on players is wrong. A model
that cannot make it at all is also wrong, because a quiet passage on a
single coil through a cranked amp over a black background is a tell.
This file adds the rig's steady noise sources, each at the point where
it enters a real chain, so the volume knob, the pickup type and the amp
gain act on it the way they do on a real rig.

**Every source added here defaults to zero.** The one that already
exists (single-coil hum, `noise_amp_buzz`) keeps its ID, range, default
and constants. Every factory and user preset renders bit-identically
until a user turns something up.

## 0. Ground rules

1. **Injected where it enters** (section 2). There is no noise layer at
   the output. So a humbucker cancels hum, rolling back the volume
   quietens pickup hum but not amp hiss, and amp gain raises hiss. None
   of that is special-cased.
2. **A drone, not an event.** These sources run continuously, so they do
   not use the `NoiseEngine` pool (`pick-noise.md` 1), whose rule is
   "nothing here drones". They live in one new module, `NoiseFloor`.
3. **Deterministic.** Every random value comes from `noiseHash` /
   `RtRandom` seeded from the character seed (`character-wear.md` 1) and
   a fixed per-source index. `reset()` reseeds, so two offline renders of
   one preset are sample-identical.
4. **Honest magnitudes**, stated against the **reference pluck**: open
   low E, velocity 100, bridge single coil, volume and tone at 10, 3 m
   standard cable, peak at Aux 1 (DI). Hum you strain to hear on a clean
   amp is 50-60 dB below that.
5. **Zero is free.** With every new source at 0, `NoiseFloor::isIdle()`
   is true and the module is skipped.

## 1. What exists today (the delta starts here)

- `PickupEngine::processStrings` makes mains hum: fundamental + 0.35 ×
  3rd + 0.15 × 5th, at `noise_amp_buzz` × 0.0022 × the single-coil share
  of the active slots. An untapped humbucker gives exactly 0. The hum is
  added **before** `GuitarCircuit::process`, so the knobs already act on
  it. That stays.
- `PickupEngine::setMainsFrequency` exists but is only ever called with
  60 Hz, from `prepare()`.
- `noise_amp_buzz` is labelled "Amp Buzz" but is the pickups' hum. The
  ID stays, because hosts key automation on it. The display name becomes
  **"Single-coil Hum"**.
- `CharacterEngine`'s intermittent jack (`character-wear.md` 5) is a
  *contact* fault and stays there. Cable movement here is
  *triboelectric*. They are separate, and neither ducks the other.
- Aux 8 carries the `NoiseEngine` pool and the scrape, pre-body
  (`routing-io.md`, `pick-noise.md` 1.3). The existing hum is not on it.

## 2. Sources and where they enter

```
strings -> pickups (+hum, +fluorescent) -> [+passive hiss] -> GuitarCircuit
        -> [+cable movement] -> DI (Aux 1) -> pre-FX
        -> [+ground loop, +radio, +amp hiss, +microphonics] -> AmpEngine
```

The mains sources share one phase accumulator, `mainsPhase += f_mains /
sr`, reset to 0 in `reset()`. That is the same arithmetic as
`PickupEngine::humPhase`, so the existing hum and the new sources stay
phase-locked and can reinforce or partly cancel each other, as real hum
and a ground loop do.

### 2.1 Single-coil hum (existing, plus position and region)

The waveform and the constant are unchanged. A new position gain
multiplies them:

```
g_pos   = g_angle × g_dist
g_angle = 0.12 + 0.88 × |cos θ|          // θ = noise_player_angle
g_dist  = 0.4 + 0.6 × (1 m / d)^2        // d = noise_player_distance
```

- A single coil is a loop antenna. Turning 90° gives the familiar null,
  limited to about -18 dB because a room's field is never uniform.
- 40% of the field is room wiring (roughly uniform). 60% is the amp's
  transformer (near field). So 0.3 m from the amp is +17 dB and 5 m is
  -7.8 dB.
- At the defaults (0°, 1 m), `g_pos` = 1.0 exactly, and the code only
  multiplies when `g_pos != 1`. That keeps legacy renders bit-identical.
- `noise_mains_hz` calls the existing `setMainsFrequency(50 | 60)`.
- Target: `noise_amp_buzz` 1.0 at neutral = **-40 ±6 dB re the reference
  pluck** (a noisy vintage single coil in a bad room). NF-02 measures it.
  If the shipped constant falls outside that window, that goes in
  `DECISIONS.md` and the constant does **not** change: changing it would
  re-voice every preset. The new sources are calibrated to targets, not
  to it.
- **As built:** the reference pluck peaks at 1.08 engine units at Aux 1,
  and the shipped constant (0.0022) gives **-58 dB** at 1.0 (-76 dB at
  its 0.12 default). It is outside the window and, per the rule above, is
  kept; NF-02 logs both and pins -57.7 ±2 dB so either moving is noticed
  (recorded in `docs/coverage/REALISM-C.md`, as helpers do not edit
  `DECISIONS.md`). The hum's own phase accumulator only advances while
  the hum sounds, so the new mains sources run their own accumulator from
  `reset()`; they are not phase-locked to the legacy hum.

### 2.2 Fluorescent / dimmer buzz

Magnetic ballasts and phase-cut dimmers re-strike an arc each half
cycle. The result is an impulse train at 2 × `f_mains`, each impulse
ringing through a bandpass (3.5 kHz, Q 2.5), plus 0.25 × a 200 Hz
lowpassed copy for the 100/120 Hz body. Impulse heights jitter ±15% from
the hash. It is magnetic, so it is added with the hum and scaled by the
same single-coil share and `g_pos`. Target at 1.0: **-45 ±6 dB** re the
reference pluck (RMS).

### 2.3 Passive hiss (Johnson noise)

`e_n = sqrt(4 k T R B)`, with R = `coilResistance` + the volume pot's
wiper-to-ground resistance from the live `CircuitComponents`. 6 kΩ at
300 K over 20 kHz gives about 1.4 µV RMS. Against a real pickup's ~0.3 V
pluck (`kEmfVoltsPerUnit` = 0.3 V per reference-pluck peak) that is
**about -107 dB**: inaudible except through extreme gain, and the spec
keeps it so.

Gaussian white noise (sum of 4 seeded uniforms, rescaled to unit
variance) is added to the EMF before `circuit.process`, so the pickup
resonance gives it its real 3-5 kHz colour. 1.0 = physical: the white
noise carries the physical density 4kTR over the whole 0..sr/2 band, so
its 20 kHz band holds exactly sqrt(4kTRB). `kEmfVoltsPerUnit` = 0.3 V /
1.08 units. The "wiper-to-ground" term is the pot section in parallel
with the coil as the output sees it, which at volume 10 leaves the coil's
~6 k, as this section's own figure says. Advanced goes to 100 (+40 dB).

### 2.4 Cable movement (triboelectric)

- **Rolls**, deterministic from `noiseHash(seed, rollIndex)`:
  - each note-on rolls `p = amount × 0.02 × v²`;
  - while any string rings, one roll per second at `p = amount × 0.01`
    (the player shifting their weight).
- **Event**: a thump (noise through a 2-pole 80 Hz lowpass, 30-150 ms
  decay) plus a crackle (5-40 sparse impulses, 1 kHz highpass). Duration
  and count come from the hash. 4 preallocated voices; the oldest is
  stolen.
- **Level** = `amount × (cableLength / 3 m) × q`. q is 0.5 studio, 1
  standard, 2 cheap, 1.5 vintage (`CableQuality`). With `cableOn` false
  there are no events.
- It is added **after** `circuit.process` (the cable is after the pots),
  so it shows on Aux 1. Target at 1.0 with 3 m standard: peaks at
  **-35 to -50 dB** re the reference pluck.

### 2.5 Ground loop

Separate earths put hum on signal ground. The waveform is harmonics
k = 1..20 of `f_mains` at amplitude `k^-0.8`, with the 2nd raised +6 dB
(rectifier charging pulses). Phases are fixed from the seed. It is
generated from a 2048-point wavetable built in `prepare()`. The table is
one mains cycle, so a region change only changes its read rate and
nothing is rebuilt; a new character seed rebuilds it in place, without
allocating.

It enters at the **amp input**, so it does not depend on the pickups,
the volume knob or the player's position. That independence is the
diagnostic: a humbucker does not fix a ground loop. Target at 1.0:
**-45 ±6 dB** re reference (RMS at the amp input).

### 2.6 Radio pickup

A long cable is an antenna, and the first grid rectifies AM into faint,
garbled programme. No real audio ships. Instead the source makes:

- speech-like noise: a 300-3000 Hz bandpass on white noise,
  amplitude-modulated by a seeded syllabic envelope (3-6 Hz, 60% depth,
  0.2-1.5 s gaps);
- a heterodyne whistle drifting 4-8 kHz at 0.05 Hz, 12 dB below the
  programme.

Level = `amount × (cableLength / 3 m) × kRadio`, entering at the amp
input. With `cableOn` false there is no antenna and no radio. Target at 1.0: **-55 ±6 dB** (RMS). Most players never hear it on
a clean amp.

### 2.7 Amp hiss

This is the input-referred noise of V1: white noise plus 1/f flicker
below about 1 kHz (a 3-pole Kellet pinking filter blended at 0.5),
added at the amp input. `amp_gain`, the voicing's stage count and the
tone stack then shape it, with no gain special case. 0.5 = a typical
valve preamp, **-100 ±2 dB** re reference input-referred. That comes out
about 70 dB below the note from a clean amp and about 45 dB below it
from a dimed high-gain amp, where players reach for a gate. 1.0 is +6 dB
(a tired tube). The amp's warm-up gain mutes it in standby.

### 2.8 Tube microphonics

In a combo, speaker vibration shakes V1. Its electrodes ring at a
mechanical resonance and turn it into signal: a "ping" after loud notes,
or a howl in a bad tube.

```
x_mic = resonator(f_m, Q_m)(previous block's amp output) × G_m
f_m in 2.5-7 kHz, Q_m in 20-50 (per instance, from the seed)
G_m = noise_microphonics × 0.5                    // loop gain
```

- The tap is the same one-block amp-output buffer
  `FeedbackLoop::pushAmpOutput` uses. The result goes back in at the amp
  input.
- **Bounded by construction.** The resonator is normalised to unity
  peak. `G_m` is the loop gain: the injection divides out the amp's own
  gain at the resonance, measured each block on the signal that went in
  and came out (rising at once, falling slowly). `G_m` is clamped below
  0.95 in every mode, and the stock max of 0.5 cannot build up. It has the engine rule 3 DC blocker and NaN guard.
  The stock max makes a ping 30-40 dB below a hard note, decaying in
  50-200 ms.
- A cabinet that implies a separate head (4x12) scales `G_m` by 0.2.
- This is air to tube. It is not the air-to-string feedback path
  (`ambiguity-resolutions.md` 1). Both can run.

## 3. Parameters

Families per `advanced-ranges.md` 2: the guitar side is `circuit`, the
rig side is `amp`. Choices and routing are non-physical.

| ID | Name | Stock | Advanced | Default | Unit | Family |
|---|---|---|---|---|---|---|
| `noise_amp_buzz` (exists) | Single-coil Hum | 0 – 1 | 0 – 4 | 0.12 | ratio | circuit |
| `noise_mains_hz` | Mains Region | 60 / 50 Hz | – | 60 Hz | choice | – |
| `noise_player_angle` | Facing Angle | 0 – 90 | 0 – 90 | 0 | deg | circuit |
| `noise_player_distance` | Distance to Amp | 0.3 – 5 | 0.1 – 20 | 1.0 | m | circuit |
| `noise_fluorescent` | Fluorescent Buzz | 0 – 1 | 0 – 4 | 0 | ratio | circuit |
| `noise_passive_hiss` | Passive Hiss | 0 – 1 | 0 – 100 | 0 | × physical | circuit |
| `noise_cable_movement` | Cable Movement | 0 – 1 | 0 – 4 | 0 | ratio | circuit |
| `noise_radio` | Radio Pickup | 0 – 1 | 0 – 4 | 0 | ratio | circuit |
| `noise_ground_loop` | Ground Loop | 0 – 1 | 0 – 4 | 0 | ratio | amp |
| `noise_amp_hiss` | Amp Hiss | 0 – 1 | 0 – 4 | 0 | ratio | amp |
| `noise_microphonics` | Microphonics | 0 – 1 | 0 – 1.9 | 0 | ratio | amp |
| `noise_floor_to_aux8` | Noise Floor on Aux 8 | bool | – | false | – | – |
| `noise_floor_style` | Noise Floor Style | choice | – | Off | – | – |

- `noise_amp_buzz` gains a `PhysicalRange` row whose stock pair is its
  declared 0-1 (`advanced-ranges.md` 1.0), so no preset re-maps.
- The angle has the same pair twice, because past 90° is symmetrical.
  Microphonics stops at 1.9 (loop gain 0.95) per `advanced-ranges.md`
  3.3.
- **Net new parameters: +12**, appended after the existing noise block.
  Nothing is renumbered.
- Options gains a user-global **"Default mains region"** preference
  (Auto from OS locale / 50 / 60), on the Options AUDIO page. It seeds
  `noise_mains_hz` for Init presets only. (A new instance keeps the
  parameter's 60 Hz default: a preset saved before this parameter existed
  loads the default, and seeding it from the machine would make such a
  preset's hum differ between machines.) A loaded preset keeps its own value, so a render is
  the same on every machine.

`noise_floor_style` writes values when picked, and reads "(modified)"
after any edit, as `squeak_style` does:

| Style | Hum | Fluor. | Passive | Cable | Radio | Ground | Hiss | Mic |
|---|---|---|---|---|---|---|---|---|
| **Off (default)** | unchanged | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Studio, treated | 0.08 | 0 | 1 | 0 | 0 | 0 | 0.3 | 0 |
| Home desk | 0.25 | 0.15 | 1 | 0.2 | 0 | 0.15 | 0.5 | 0.1 |
| Club stage | 0.4 | 0.35 | 1 | 0.5 | 0.1 | 0.3 | 0.6 | 0.3 |
| Vintage combo | 0.5 | 0 | 1 | 0.3 | 0.25 | 0.1 | 0.8 | 0.5 |

## 4. Engine insertion

New class `NoiseFloor` (`Source/DSP/Noise/NoiseFloor.h/.cpp`), owned by
`LuthierEngine` next to `playingNoise`. It has `prepare / reset /
setSeed / setSettings / isIdle`, a `beginBlock (numSamples,
singleCoilShare, const CircuitComponents&, anyRinging)`, four per-sample
taps (`pickupSample`, `circuitInSample`, `diSample`, `ampInSample`),
`onNoteOn (velocity)` and `pushAmpOutput (const double*, int)`.

Hooks in `LuthierEngine::processSubBlock`:

1. Before the string loop, call `noiseFloor.beginBlock(...)`. The
   single-coil share comes from a new `PickupEngine::getSingleCoilShare()`,
   which factors out what `processStrings` already computes. Call
   `pickups.setHumPositionGain (g_pos)` as well. It is a new setter,
   applied inside the existing hum branch only when not 1.
2. In "4. combine the transducer paths" (electric):
   `magneticBuffer[i] += pickupSample(i)` and
   `instrument += circuitInSample(i)` before `circuit.process`, then
   `instrument += diSample(i)` after it, before the input gain. An
   acoustic (piezo and mic) gets the passive hiss and the cable only,
   because nothing there is magnetic.
3. In "6. amp", call `amp.processSample (mono + ampInSample(i))`, then
   `noiseFloor.pushAmpOutput(dl, numSamples)` after the loop.
   `AmpEngine` is unchanged.
4. `triggerNote` calls `noiseFloor.onNoteOn (e.velocity)`.
5. `ParameterBridge::applyToEngine` builds `NoiseFloorSettings` once per
   block. `noise_mains_hz` drives `pickups.setMainsFrequency` and
   `noiseFloor` together.
6. Aux 8: when `noise_floor_to_aux8` is on, `noiseBuffer[i]` also gets
   the dry sum of the new sources plus the hum × `g_pos`. That is an
   identification stem, not a mix stem. The default is off, so Aux 8
   stays unchanged for existing sessions.

**State** is per instance, not per string, because the noise floor
belongs to the rig. It holds the mains phase, the fluorescent jitter
index, 4 cable voices, the radio envelope, the microphonic resonator (2
doubles), the pinking filter (3 poles) and one `RtRandom` per source.

## 5. UI

`gui-integration.md` 4.4, CHARACTER tab: a new **NOISE FLOOR** group
after aged electronics.

- The style dropdown and a 50/60 switch.
- A **Guitar** knob row (hum, fluorescent, passive hiss, cable, radio)
  and a **Rig** knob row (ground loop, amp hiss, microphonics).
- A **position pad**: a 120 × 90 px top-down sketch of the player and
  the amp. Drag sets distance, the wheel sets angle, and the hum gain in
  dB is shown beneath.
- A **noise meter**: summed noise-floor RMS in dB re the reference
  pluck. It drains at 10 Hz and greys after 2 s stale
  (`gui-engine-dataflow.md`).
- The "on Aux 8" checkbox, mirrored on ROUTING's Aux 8 strip.

`gui-integration.md` 19 gains the row: Noise floor | NoiseFloor | Adv Col
4 CHARACTER NOISE FLOOR. The existing Amp Buzz knob stays as a
relabelled mirror. Undo: knob-move (`action-and-undo.md` 3.1). The style
and the region are discrete switches (3.2).

## 6. State and serialization

Everything is an APVTS parameter in the preset's `parameters` block. The
seed is the existing `character_seed`. Older presets lack the new IDs,
so those load at their defaults (0 / Off / false) and sound unchanged.
MIDI export sends nothing: every source is deterministic from the seed
and the performance, so a Luthier-profile re-import reproduces it.

## 7. Performance and realtime safety

- Budget: **0.15 units** with every source on (`performance-budget.md` 1
  gains a `NoiseFloor` row), and 0 when idle. The worst case per sample
  is a phase step, a wavetable read, 3 biquads, 3 pinking poles and one
  resonator.
- Nothing allocates in `processBlock`. A region change rebuilds the
  ground-loop table on the message thread and swaps it in through an
  atomic pointer.
- The rule 3 DC blocker and NaN guard are on every recursive filter.
  Frequencies are in Hz and times in seconds, recomputed in `prepare()`.

## 8. Tests

Group `NoiseFloor`, `Source/Tests/NoiseFloorTests.cpp`. Unless stated:
48 kHz, 128-sample blocks, no strings played.

- **NF-01 Defaults are bit-identical.** Three factory presets and a 10 s
  reference performance, rendered at defaults and with `NoiseFloor`
  bypassed by test hook: every output sample is equal.
- **NF-02 Hum calibration.** Hum 1.0 at neutral: RMS at Aux 1 is -40 ±6
  dB re the reference-pluck peak. The value at 0.12 is logged.
- **NF-03 Region.** At 50 Hz, the largest FFT bin of Aux 1 is within
  ±0.5 Hz of 50, and the 150 Hz bin is above -20 dB re the fundamental.
  The same at 60 / 180.
- **NF-04 Humbucker.** With hum and fluorescent at 1, bridge humbucker
  versus single coil: DI noise at least 40 dB lower. With ground loop at
  1, amp-input noise differs by less than 0.1 dB between the two.
- **NF-05 Volume knob.** Volume 1 to 0: hum at the amp input falls at
  least 40 dB. Ground loop and amp hiss move less than 0.5 dB.
- **NF-06 Position.** 90° versus 0°: -15 to -21 dB. 5 m versus 1 m: -6.5
  to -9 dB. 0.3 m: +15 to +19 dB. At (0°, 1 m) the hum samples are
  bit-identical to the legacy path.
- **NF-07 Amp hiss.** On a high-gain model, gain 0.9 versus 0.2 raises
  hiss at the cab output by at least 15 dB. At 0.5, input-referred RMS
  is -100 ±2 dB re reference.
- **NF-08 Microphonics is bounded.** Impulse at stock max, into the
  loop around a stand-in amp (x4 and x200): the microphonic component
  decays 60 dB within 500 ms. (In the full engine a choked note keeps
  feeding the resonator through the body's tail, so there the component
  tracks its source rather than decaying on its own.) At advanced max, the
  RMS of each second over 10 s never rises and every sample is finite.
- **NF-09 Determinism.** With the same seed and all sources at 1, two
  renders are sample-identical. A different seed gives cable and radio
  output correlation below 0.5.
- **NF-10 Cable.** Cable off: 0 events in 1000 note-ons at v=1. On at
  1.0: an event count in [10, 32]. 6 m versus 3 m: mean peak +6 ±1 dB.
  Cheap versus studio: +12 ±1 dB.
- **NF-11 Passive hiss is physical.** At 1.0: DI RMS within ±2 dB of
  the density `4kTR / kEmfVoltsPerUnit²` integrated through
  `GuitarCircuit::response` at the bilinear-warped frequency over
  0..sr/2 (the DI is the discrete circuit), with the spectral peak within
  ±15% of `findResonantPeakHz`.
- **NF-12 Fluorescent spectrum.** A line at 2 × mains, and 2-6 kHz energy
  at least 6 dB above 300-1000 Hz energy.
- **NF-13 Aux 8 opt-in.** With the flag off, Aux 8 is sample-identical to
  the pre-spec build under a picked note. With it on and ground loop at
  1, Aux 8 RMS is above 0 with no notes.
- **NF-14 Style Off is inert.** "Home desk" then "Off": every new source
  reads 0 and `noise_amp_buzz` is unchanged.
- **NF-15 Idle is free.** All new sources at 0: `isIdle()` is true, and a
  20 s render costs less than 0.02 units over the bypassed build (the
  figure is logged; timing noise on a shared machine exceeds 0.005).
- **NF-16 No allocation** over 60 s with every source on, including a
  region change and a style change (heap hook).
- **NF-17 Sample-rate independence.** NF-03, NF-06 and NF-07 hold at 44.1
  and 96 kHz with the same thresholds.
