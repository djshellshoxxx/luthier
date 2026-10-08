# 04 Piezo, internal mic and magnetic soundhole blend (three-source mixer)

STG-02, STG-03. Source roadmap: 3.1.

**Summary.** A three-source acoustic mixer: level, alignment and tone per source,
with a Blend card. It replaces the two-way blends only for acoustic presets that
opt in.

**Status.** Partial. Existing pieces:
- `PickupEngine.h` line 114 `setBlend` (outer active pickups); line 194
  `setPiezoMicBlend` (piezo/mic blend, already present); `processPiezo` /
  `processInternalMic` (lines 190-191).
- `PickupType::Piezo`, `MagneticSoundhole`, `InternalMic` (line 39).
- Missing: a three-source sum with per-source level, alignment and tilt, and the
  Blend card.

## User-facing behaviour
- Acoustic guitars show a pickup card with three faders (Piezo, Mic, Magnetic) and
  an alignment slider (0 to 1.0 ms).
- Easy Mode: one "Blend" slider `b` in [0,1] sets piezo and mic together.
  Piezo dB = 20 log10(cos(b pi/2)), mic dB = 20 log10(sin(b pi/2)), floored at -60
  (D: equal-power).

## Engine / DSP
Sum of active sources, each with level and tilt:
```
y(t) = g_p * T_p{ D_align(x_p) } + g_m * T_m{ x_m } + g_g * T_g{ x_g }
```
- `g_* = 10^(dB/20)`; a dB value at or below -60 means off (`g = 0`).
- `D_align`: fractional delay on the **piezo path** by `align_ms` (4-point Lagrange,
  max 1 ms buffer, allocated in `prepareToPlay`). The piezo hears the string
  first, so it is delayed to meet the mic. (The roadmap STG-03 wording says
  "shifts the mic response"; this file corrects it to the piezo path.)
- `T_p`: peaking EQ, +4 dB at 3 kHz, Q 0.7 (D). `T_m`: high shelf, -2 dB above
  6 kHz (D). `T_g`: existing `MagneticSoundhole` voicing (no change).
  Biquads per the RBJ cookbook, double precision.
- Gains are smoothed (one-pole, 20 ms) in double.
- **Compatibility rule.** If the preset has no `stage.ac_*` values, the legacy
  `setBlend` / `setPiezoMicBlend` path runs unchanged. The new mixer runs only when
  the block is present.
- RT: delay buffer and filter state allocated in `prepareToPlay`; `reset()` clears
  them; no allocation on the audio thread.

## Data model and parameters
In `// ==== BEGIN V2-STAGE params ====` (new block; none exists yet):

| ID | Range | Default | Automatable |
|---|---|---|---|
| `ac_piezo_level` | -60..0 dB | 0 | yes |
| `ac_mic_level` | -60..0 dB | -6 | yes |
| `ac_mag_level` | -60..0 dB | -60 (off) | yes |
| `ac_piezo_mic_align_ms` | 0..1.0 ms | 0.3 | yes |

Control: each is a visible fader or slider on the pickup card.

## State / file format / migration
- Stored in the preset's `stage` block. Legacy presets: block absent, legacy path.
- A new acoustic preset writes the block with the defaults.

## Edition gating
- Free (roadmap 6).

## Performance budget
- 0.04 units with all three active (roadmap 7).

## Test plan
- **STG-02.** Piezo 0 dB, mic -6 dB, magnetic off. Output equals `piezo + 0.5*mic`
  (the -6 dB mic) within 0.1 dB, with the tilts bypassed in the test build.
  Also: magnetic at -60 dB equals magnetic off within 0.1 dB.
- **STG-03.** Set `align_ms` = 0.3. Measure group delay of the piezo path at 1 kHz
  with a swept sine; assert 0.3 ms within 0.02 ms. At 0 ms, the two paths are
  coherent (phase difference < 5 deg) at 1 kHz.
- **STG-14 (partial).** No audio-thread allocation when the sources are toggled.

## Effort and dependencies
- **ED 6.**
- Depends on: `spec/engine.md` 7.5-7.6 (pickups); `spec/mic-placement.md` (mic
  position); `spec/instruments-v2.md` 0 (M/I/D tags: alignment is D until measured).
- Open question 1 (roadmap 10): slider stays, tagged D.
