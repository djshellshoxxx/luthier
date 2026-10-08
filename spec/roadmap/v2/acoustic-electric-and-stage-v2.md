# ACOUSTIC-ELECTRIC AND STAGE V2 SPEC

Most players who gig with an acoustic plug it in, and most who record with an
electric want the room, the rig and the stage tools next to the sound. This
file is the v2 list for that world: piezo, internal microphone and magnetic
pickups blended in one guitar; feedback through the soundhole; body tapping as
percussion; speaker and power-amp behaviour at volume; stereo and
double-tracked rigs; DI capture and re-amp; and practice and stage tools
(metronome, drum practice, a real-input tuner).

It is a delta. Each item lists what exists, with file evidence, and specifies
only the gap.

## 0. Ground rules

1. **Evidence first.** Exists, partial or missing is decided by a search of
   `Source/` and `spec/`, with a file and symbol for each claim.
2. **Physics over presets.** Acoustic feedback, speaker breakup and sag are
   computed from the body, cabinet and supply models. A preset may set a
   coefficient; it may not stand in for the physics.
3. **Honest magnitudes.** Each number is tagged M, I or D (`instruments-v2.md`
   0). Helmholtz and sensitivity values are derived (I) from stated formulas;
   excursion and balance values are design defaults (D).
4. **Stage-safe.** Any tool that may be heard by an audience (tuner, metronome,
   drum practice) has a mute and a route that never reaches the main output
   unless the player sends it there (`live-performance.md`).
5. **Append-only IDs.** New automatable parameters go in
   `// ==== BEGIN V2-STAGE params ====`. Every automatable one has a visible
   control.

## 1. Status

| Item | Status | Evidence |
|---|---|---|
| Piezo, internal mic, magnetic soundhole pickups | exists | `PickupEngine.h` line 39 (`Piezo`, `MagneticSoundhole`, `InternalMic`); `engine.md` 7.5-7.6 |
| Blend between active pickups | exists | `PickupEngine.h` 111-114 (`setBlend`, outermost active pickups) |
| Piezo + mic + magnetic blend, with alignment | partial | Blend is between two outermost pickups only; no per-source alignment or balance |
| Soundhole feedback | partial | `FeedbackLoop.h` header (air path, `k_couple`, `H_s` peak at the note); no soundhole-specific coupling |
| Body-tap percussion | exists | `TechniqueKeyswitch::bodyTap` (17); `string-slap-technique.md` 30-35 |
| Power-supply sag at volume | exists | `AmpEngine.h` `getSagAmount()` (line 138), `supplyVoltage` |
| Cone breakup | partial | `CabinetEngine.h` 218-236 (procedural bandpass with breakup peak); no excursion-limited compression |
| Stereo L/R rig | partial | Stereo outs exist (`routing-io.md` 1-3); no stereo amp path (`pedalboard-v2.md` 3.4) |
| Double-tracking | partial | `Doubler` pedal (`PedalsMod.h` 74) is one guitar copied to a panned second; not two takes |
| DI capture | exists | Aux 1 DI (`routing-io.md` 2); `CaptureRanges` (`tone-match.md`) |
| Re-amp | exists | `routing-io.md` 5; re-amp symbols in `Parameters.cpp` and `PluginProcessor.cpp` |
| Metronome | exists | `Source/Practice/Metronome.h/.cpp`; `practice-tools.md` 1 |
| Drum practice | partial | Jam drum kit (`Source/Jam/JamEngine.h`); no drum-pattern trainer in `practice-tools.md` |
| Real-input tuner | missing (code) | `tuner-and-tuning-reference.md` specifies it; no `Tuner` class or panel in `Source/` |

## 2. User stories

- **U1.** I blend the piezo with the internal mic and add a little of the
  magnetic soundhole pickup, and each source sits where I want it.
- **U2.** I stand near my acoustic and it feeds back at the air resonance; I can
  set how much, and I can turn it off.
- **U3.** I tap the body for a percussive hit on the beat, with its own body
  tone.
- **U4.** At volume my amp sags and my speaker breaks up the way a real cone
  does, not a clean bandpass.
- **U5.** My rig is a stereo pair with a second take double-tracked on the
  other side.
- **U6.** I capture a clean DI and re-amp it later through another rig.
- **U7.** I tune on stage with the tuner on my input, muted from the audience.
- **U8.** I practise drum patterns with a count-in and subdivisions.

## 3. Engine and data model

### 3.1 Piezo, mic and magnetic blend (partial)

A blend of three sources needs each source to carry its own level, its own
alignment and its own tone, then a sum. The existing blend mixes two outermost
pickups (`PickupEngine.h` 111-114). Add a source mixer for acoustic guitars:

- **Sources.** Piezo (bridge), internal condenser (body), magnetic soundhole.
  Each is a `PickupEngine` coil model; only active sources are summed.
- **Level.** One fader per source, -60 to 0 dB, default piezo 0 dB, mic -6 dB,
  magnetic -inf (off).
- **Alignment.** The piezo sits at the bridge and hears the string earlier than
  the mic at the body. The mic sits about 0.1 m from the bridge, so sound takes
  about 0.3 ms to arrive (I: 0.1 m / 343 m/s). The control `ac_piezo_mic_align_ms`
  ranges 0-1.0 ms, default 0.3 ms (D).
  The control lets the player time-align them in the mix; it does not model
  propagation.
- **Tone.** A fixed tilt per source (piezo: +4 dB at 2-4 kHz, mic: -2 dB above
  6 kHz, magnetic soundhole: the existing `MagneticSoundhole` voicing) (D).
- **Blend card.** Three faders plus an alignment slider, in the pickup card of
  acoustic guitars. Easy Mode shows one "Blend" slider that sets the piezo and
  mic faders together.

### 3.2 Soundhole feedback (partial)

`FeedbackLoop` already models acoustic feedback as a loop: the amp output
reaches the strings through the air, weighted by `k_couple` and a peak `H_s`
near each string's note (`FeedbackLoop.h` header). The gap is the soundhole.
An acoustic body has a Helmholtz resonance: the air in the body vibrates
through the soundhole. That resonance is a strong peak at the soundhole's
output and sets where feedback is most likely on an acoustic.

Helmholtz frequency (I), for body volume `V`, soundhole radius `r`, top
thickness `t`:

```
f_H = (c / 2 pi) x sqrt( pi r^2 / (V x (t + 1.7 r)) ),   c = 343 m/s
```

Worked: `V = 0.0175 m^3`, `r = 0.05 m`, `t = 0.003 m` gives 123 Hz; with
`r = 0.04 m` it gives 110 Hz. Those are typical dreadnought-scale values.
The body engine already has the air mode in its modal or convolution model
(`BodyEngine`); the soundhole coupling uses that mode's frequency and Q, not
the formula, which is here to check the body model.

Changes (partial):

- `k_couple(s)` gains a soundhole factor: the coupling is multiplied by the
  body's air-mode response at the string's frequency, normalised to 1 at the
  air mode. Strings far from the air mode feed back less.
- Parameter `sh_feedback` (0-1, default 0), automatable, is the acoustic
  feedback amount, and it sits next to the existing `feedback_amount`
  (`ambiguity-resolutions.md` 1.2). It enters the same loop; it is not a
  second loop.
- Stage safety: feedback is off on any preset unless the player turns it on,
  and a clip guard limits the loop to -6 dBFS at the output.

### 3.3 Body-tap percussion (exists)

`TechniqueKeyswitch::bodyTap` (17) and the Body Tap slap type
(`string-slap-technique.md` 30-35) already produce a percussive hit on the
body. There is no delta. Its voice depends on the body model, which is correct:
a body tap on a dreadnought and on a parlor sound different. Only the stage
tooltip is added ("The body tap uses this guitar's top"). Test: STG-01 checks
that the tap's energy follows the body's top modes.

### 3.4 Speaker breakup and sag at volume (partial)

Power-supply sag exists: `AmpEngine` tracks `supplyVoltage` and reports
`getSagAmount()` (`AmpEngine.h` 136-138). Cone breakup exists in the
procedural cabinet fallback as a bandpass with a breakup peak
(`CabinetEngine.h` 218-236).

Missing: the cone's **excursion limit**. A real speaker compresses when the
cone reaches its mechanical limit (`X_max`), and the compression is most
audible on bass and on loud sustained chords. Add:

- Cone excursion per frequency: `x = V_drive / (2 pi f)` scaled by the
  cabinet's `Bl` and moving mass (D), clipped to `X_max` (D, 4 mm for a 12 in
  speaker).
- A soft limit above `X_max`: a knee at 85 % of `X_max` with a 3:1 slope
  (D), giving a few dB of compression on low notes at high drive.
- Parameter `speaker_breakup` (0-1, default 0.3 on amp presets, 0 on
  "clean" presets), automatable. At 0 the speaker model is the current one.

The sag and the breakup are separate: sag changes the supply and the power
stage's headroom; excursion changes the speaker's output. Both are at zero on
existing presets, where they are compatibility-tested (STG-08).

### 3.5 Stereo L/R rigs (partial)

The engine has stereo outputs (`routing-io.md` 1-3) and the board carries a
stereo signal to its last stage (`pedalboard-v2.md` 3.4). The amp is mono.
A stereo rig is two amp paths, L and R, each with its own cabinet and mic
position, fed from the board's stereo output.

- **Rig modes.** Mono (current), Stereo pair (one amp, stereo cab: the existing
  dual-IR option, `engine.md` 13.3), and Two-amp (L and R each with a full amp
  and cabinet, Pro).
- **Two-amp** doubles the amp cost (about 2 times the amp budget), and is
  gated by the board budget (`pedalboard-v2.md` 7).

### 3.6 Double-tracking, two takes (partial)

The `Doubler` pedal copies the guitar to a second, panned copy with a slight
timing and pitch offset (`PedalsMod.h` 74). That is one performance, duplicated.
A double-track is two performances: the player records a take and plays a
second take on the other side. The model here is a stored take plus a
replay:

- **Take store.** A recorded take (MIDI or the performance's note stream, not
  audio) replayed to the second side. Per take: a timing offset (10-30 ms, D),
  a level offset (-1 to -2 dB, D), and a pan (L or R).
- **Human variation.** Each replayed note gets the seeded timing jitter already
  in `humanize` (`rhythm-engine.md` 135), so the two takes differ the way two
  takes do.
- **Scope.** Two takes, not a multitrack editor. The second take replaces the
  `Doubler` on that side; `Doubler` remains the single-performance option.

### 3.7 DI capture and re-amp (exists, with a delta)

Aux 1 is the raw DI (`routing-io.md` 2). Re-amp (`routing-io.md` 5) plays a
DI file through the rig. The delta is a single action, **Capture DI**, which
writes the Aux 1 signal to a WAV alongside the preset's state, and a **Re-amp
from file** that loads it. This is partial: the routes exist, the one-button
capture file does not. The file goes in `~/Documents/Luthier/Captures/` (the
practice-tools data-location pattern, `practice-tools.md` 10).

### 3.8 Drum practice (partial; metronome exists in `practice-tools.md` 1)

The Jam drum kit (`Source/Jam/JamEngine.h`) plays patterns with the band. A
practice drum trainer is missing: a pattern (a bar of 8ths or 16ths with
accents), a count-in, a subdivision click, and a tempo ramp. It reuses the
metronome's timing and the rhythm engine's pattern type. It is a trainer mode
in `practice-tools.md` 4-style layout, not a new panel.

### 3.9 Real-input tuner (missing in code)

`tuner-and-tuning-reference.md` is the owner of the tuner: pitch tracking,
the source selector (Main In, per-string input), and the Live mode. This
section adds only what the stage needs:

- A **Mute to audience** switch on the tuner, which routes the tuner's input
  monitor to the headphone bus only. It is on by default when the tuner is open
  in performance mode.
- The tuner input is the same ring buffer the capture path uses
  (`notation-export.md` 6.2 pattern), never allocating on the audio thread.

## 4. Parameters

New, appended inside `// ==== BEGIN V2-STAGE params ====`:

| ID | Range | Default | Automatable | Control |
|---|---|---|---|---|
| `ac_piezo_level` | -60 to 0 dB | 0 | yes | Blend card |
| `ac_mic_level` | -60 to 0 dB | -6 | yes | Blend card |
| `ac_mag_level` | -60 to 0 dB | -60 (off) | yes | Blend card |
| `ac_piezo_mic_align_ms` | 0 to 1.0 ms | 0.3 | yes | Blend card |
| `sh_feedback` | 0-1 | 0 | yes | Feedback card |
| `speaker_breakup` | 0-1 | 0.3 (amp), 0 (clean) | yes | Amp card |
| `rig_mode` | choice: mono, stereo pair, two-amp | mono | no | Rig card |
| `dt_take_offset_ms` | 10-30 ms | 18 | yes | Double-track card |
| `tuner_mute_audience` | bool | on in performance | no | Tuner |

Count: 7 automatable, 2 structural (`rig_mode`, `tuner_mute_audience`), plus the
double-track take store, which is state. Keyswitch use: none new.

## 5. State, file format and migration

- The stereo rig mode, take store and tuner mute are saved with the preset
  (`file-formats`) in an optional `stage` block. A preset without it loads
  mono, no takes, tuner unmuted (v1 behaviour).
- Takes are stored as note events (MIDI-like), not audio, so a take is small.
- DI captures are files, referenced from the preset by name. Missing files
  load the preset with a notice, and re-amp is disabled for that preset.
- Existing presets have `sh_feedback` 0, `speaker_breakup` 0, mic and magnetic
  levels at their v1 values, and mono rig mode.

Every migrated preset gets `speaker_breakup` 0, so no amp preset changes sound
on load; the player opts in to breakup. This is the basis of STG-08.

## 6. Edition

Free: blend and alignment, soundhole feedback (capped at 0.3), body tap, speaker
breakup, stereo pair, DI capture and re-amp, metronome, tuner with mute to
audience. Pro: full soundhole feedback, two-amp rig, two-take double-track,
drum practice trainer. The stage tools stay in Free, since a plugin used on
stage needs them (`editions.md` 2). Production features are Pro.

## 7. Performance budget

Units per `performance-budget.md` 0.

- Blend card (three sources): 0.04 units when all three are active.
- Soundhole feedback: a per-string lookup (0.01); the loop's own cost is unchanged.
- Speaker excursion: 0.03 (one limiter per cabinet channel).
- Stereo pair: 0.1 (second cabinet and mic path).
- Two-amp rig: about 2 times the amp budget (`performance-budget.md` 1, amp
  entries), charged against the board budget.
- Double-track take playback: 0.02 (note events, plus one voice).
- Drum practice trainer: 0.01 (scheduler; no audio voices beyond the kit).
- Tuner: 0.1 on a worker thread, not on the audio thread.
- Real-input tuner and capture file writes run off the audio thread.

## 8. Tests

- **STG-01 (body tap).** The body tap's energy at the top's lowest mode is
  within 3 dB of the body's own response; two bodies give different taps.
- **STG-02 (blend levels).** With piezo 0 dB, mic -6 dB and magnetic off, the
  output equals the piezo-plus-mic sum within 0.1 dB.
- **STG-03 (alignment).** A 0.3 ms piezo-to-mic alignment shifts the mic
  response by 0.3 ms within 0.02 ms; at 0 ms the two are coherent at 1 kHz.
- **STG-04 (Helmholtz).** The body's air mode for the worked example sits at
  123 Hz within 5 Hz (I); a 0.04 m soundhole sits at 110 Hz within 5 Hz.
- **STG-05 (soundhole feedback).** With `sh_feedback` at 0, output is bit-
  identical to v1. At 1, a note at the air mode feeds back more than a note 1
  octave away, by at least 6 dB.
- **STG-06 (feedback limit).** With the clip guard, the loop output never
  exceeds -6 dBFS over 60 s at `sh_feedback` 1.
- **STG-07 (excursion).** At `speaker_breakup` 1, a 40 Hz sine at full drive is
  compressed by 2-4 dB at the peak; at 0, the speaker model is bit-identical
  to the current one.
- **STG-08 (migration).** Every factory amp preset renders bit-identically at
  `speaker_breakup` 0 (the migrated value), and the amp presets' existing sound
  is unchanged.
- **STG-09 (stereo pair).** Left and right outputs are correlated below 0.9
  for a mono source at a 20 cm mic spacing; mono sum equals the stereo pair's
  L + R at -3 dB within 0.5 dB.
- **STG-10 (double-track).** Two takes with a 18 ms offset: onset difference
  is 18 ms within 0.5 ms; levels differ by the take offset within 0.1 dB.
- **STG-11 (DI capture).** Capture DI then re-amp from file reproduces the
  rig output within 0.1 dB of a live DI pass with the same settings.
- **STG-12 (drum trainer).** A 16th-note pattern with a count-in of four bars
  at 120 BPM fires every 125 ms within 1 ms.
- **STG-13 (tuner mute).** With the tuner muted to audience, the main output
  contains no tuner signal (silence on the tuner's source channel) in 10 s of
  performance mode.
- **STG-14 (RT and controls).** No audio-thread allocation across blend,
  feedback, excursion and take playback in a 60 s stress run;
  `everyAutomatableParameterHasAVisibleControl` passes for the seven IDs.

## 9. Effort and dependencies

ED = engineer-days, one engineer with an AI pair.

| Work | ED |
|---|---|
| Blend mixer, alignment, tilt (3.1, STG-02, STG-03) | 6 |
| Soundhole feedback coupling (3.2, STG-04 to STG-06) | 6 |
| Body-tap tooltip and test (3.3, STG-01) | 1 |
| Speaker excursion limiter (3.4, STG-07, STG-08) | 6 |
| Stereo pair and two-amp rig (3.5, STG-09) | 10 |
| Double-track take store and playback (3.6, STG-10) | 6 |
| Capture DI and re-amp from file (3.7, STG-11) | 4 |
| Drum practice trainer (3.9, STG-12) | 6 |
| Tuner stage mute, integration with the owner spec (3.9, STG-13) | 3 |
| Parameters, UI cards, state block | 4 |
| Tests STG-14 and RT checks | 3 |
| **Total** | **55 ED, about 11 weeks** |

Dependencies: `pedalboard-v2.md` (stereo last stage, board budget);
`engine.md` 7 (pickups), 10-13 (chains, amp, cabinet); `routing-io.md` (stereo
outputs, Aux 1 DI, re-amp); `tuner-and-tuning-reference.md` (tuner owner);
`practice-tools.md` (metronome, trainer layout); `rhythm-engine.md` (humanize
jitter for takes); `live-performance.md` (stage safety); `ambiguity-resolutions.md`
1.2 (`feedback_amount`).

## 10. Open questions

1. **Alignment.** Is a 0-1 ms slider honest enough, or should the model derive
   alignment from body size? Recommend the slider, tagged D until measured.
2. **Air mode.** Is the `BodyEngine` air mode accurate enough for soundhole
   coupling, or is a dedicated Helmholtz term needed? Measure first.
3. **Excursion values.** `X_max` (4 mm) and the 85 % knee are D; source a
   datasheet before shipping the limiter.
4. **Two-amp rig.** Recommend the stereo pair ships in v2.0 and the two-amp
   rig follows if the board budget allows.
5. **Tuner owner.** Confirm `tuner-and-tuning-reference.md` owns the tuner
   build, so the stage mute is an addition to it, not a second tuner.
