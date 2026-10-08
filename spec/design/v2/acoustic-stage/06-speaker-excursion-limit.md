# 06 Speaker excursion limit (cone breakup at volume)

STG-07, STG-08. Source roadmap: 3.4.

**Summary.** A cone-excursion limiter: at high low-frequency drive, the speaker
compresses as the cone reaches its mechanical limit. Sag is unchanged (separate).

**Status.** Partial.
- Power sag exists: `Source/DSP/Amp/AmpEngine.h` line 139
  `getSagAmount() = 1 - supplyVoltage`, `supplyVoltage` (line 209).
- Cone breakup exists as a procedural fallback bandpass with a breakup peak
  (`Source/DSP/Amp/CabinetEngine.h` 218-236).
- Missing: excursion limit and the `speaker_breakup` control.

## User-facing behaviour
- Amp card: "Speaker breakup" slider `speaker_breakup` (0-1).
- 0: current speaker model, exactly. 1: full excursion compression.
- Sag is still its own control; breakup does not change the supply.

## Engine / DSP
Cone excursion amplitude, with the drive voltage `V` at the speaker terminals:
```
x_pk(f) = Bl * V / ( Re * Mms * max(w, w_s)^2 )       w = 2 pi f,  w_s = 2 pi f_s
```
- Mass-controlled above resonance (1/f^2), bounded below `f_s`. The roadmap's
  `V/(2 pi f)` is replaced: it is not the driver's behaviour above resonance and
  diverges below it.
- Per-cabinet constants `Bl`, `Re`, `Mms`, `f_s` (D). Default 12 in driver:
  Bl 15 T m, Re 6 ohm, Mms 0.05 kg, f_s 80 Hz, X_max 4 mm (D; source a datasheet
  before shipping, roadmap open question 3).
- Worked: at 40 Hz, `x_pk = 50 V / (2 pi 80)^2 ... = ~2.0e-4 V m`, so 4 mm needs
  about 20 V peak (about 50 W into 8 ohm, plausible at full drive).
- **Limiter.** Detector on the low band (2nd-order split at 200 Hz) of the drive,
  envelope `x_env = x_pk(f_c)` with 5 ms attack, 80 ms release.
  Knee at `0.85 X_max`; above the knee, gain follows ratio `R`:
  `R = 1 + 2 * speaker_breakup` (1:1 at 0, 3:1 at 1) (D).
  Gain applied to the low band only.
- **Bypass.** At `speaker_breakup` = 0 the limiter branch is not executed, so output
  is bit-identical (STG-07, STG-08).
- RT: coefficients computed in `prepareToPlay`/parameter change; envelope state in
  double; no allocation; `reset()` zeroes envelope.

## Data model and parameters
In `// ==== BEGIN V2-STAGE params ====`:
- `speaker_breakup`: 0..1, automatable, visible on Amp card.
- **Default issue (fix in this spec).** The roadmap says default 0.3 on amp presets
  and 0 on clean, and also says migrated presets get 0. The APVTS default must be
  **0**. The factory amp *template* used for new presets sets 0.3. Loading any
  preset without the key sets 0.

## State / file format / migration
- Stored in the `stage` block. Missing key on load: 0.
- Every factory amp preset is migrated to 0 (STG-08).

## Edition gating
- Free (roadmap 6).

## Performance budget
- 0.03 units: one limiter per cabinet channel (roadmap 7).

## Test plan
- **STG-07.** At `speaker_breakup` = 1, a 40 Hz sine at full drive: peak output is
  compressed 2-4 dB versus `speaker_breakup` = 0 (measured on the cone band).
  At 0, output is bit-identical to the current model.
- **STG-08.** Every factory amp preset loaded without the key renders bit-identically
  to the pre-change render (regression vs a golden render). Sag results unchanged.
- **STG-14 (partial).** No allocation during a 60 s stress run at breakup 1.

## Effort and dependencies
- **ED 6.**
- Depends on: `spec/engine.md` 10-13 (amp, cabinet); `spec/performance-budget.md`;
  `spec/factory-content.md` (migration of factory presets).
- Open question 3: `X_max` and the 85% knee are D until sourced.
