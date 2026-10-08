# 04 Flamenco nylon: golpe and rasgueado (roadmap 3.7)

Covers: **IN-10** (golpe), **IN-11** (rasgueado) (IN-12, IN-13, IN-14 shared; see INDEX.md).

## Summary

The flamenco nylon guitar exists. v2 adds two techniques: the golpe (a hand strike
on the top, excited into the body) and the rasgueado (a fast multi-string strum with
nail attack). Rest stroke is unchanged.

## Status

**partial.**
- Instrument: exists (`Resources/Guitars/Classical/Flamenca Blanca`; flamenco outline in
  `BodyOutlines.h`).
- Golpe sound: **missing**. The golpeador is drawn (`GuitarRenderer.cpp` ~1103) but
  not modelled. Nearest voice: `TechniqueKeyswitch::bodyTap` (17,
  `Source/Model/Playing/TechniqueTriggers.h` 51), a slap body tap.
- Rasgueado: `LuthierEventClass::rasgueado` (`Source/Export/LuthierMidiEvents.h` 43)
  exists. No playing model.
- Rest stroke: exists (`rest_stroke`, `rest_stroke_damping`).

## User-facing behaviour

- **Golpe:** a keyswitch (24, appended) fires a golpe. The hand hits the top; the
  body rings with a short thump and a dry click. `flam_golpe_amount` scales it.
- **Rasgueado:** one note event plays the chord as a fast stroke across the strings.
  `flam_rasgueado_speed` sets the total time from the first string to the last.
- Rest stroke and free stroke behave as today.

## Roadmap corrections

1. **Speed unit.** Section 2 and 4 say "20-120 ms per string"; section 3.7 says "total
   across the strings". Per string at 120 ms over six strings is 600 ms, which is not
   a rasgueado. **Resolved (D): total, first string to last, 20-120 ms.** Tooltip
   "Time from the first string to the last" matches.
2. **Golpe keyswitch.** Roadmap section 3.7 names `bodyTap` (17) or 24. Section 4 and
   open question 4 choose 24. **Resolved: 24.** A slap body tap and a golpe stay
   separate on the same instrument.

## Engine and model design

### Golpe (`BodyEngine`)

Excitation is a pre-body source. Let `A = flam_golpe_amount` (0-1), `fs` the sample rate.

```
u(n)      seeded white noise (xorshift32, seed from the strike's event id)
e_low(n)  = LP_{400 Hz, 2nd order}(u)(n) * exp(-n/fs / tau_low)     tau_low = 4.3 ms
e_clk(n)  = BP_{2-4 kHz}(impulse)(n)    * exp(-n/fs / tau_clk)     tau_clk = 0.5 ms
e(n)      = A * (e_low(n) + 0.3 * e_clk(n))                           (D: click gain 0.3)
```

- Decay to -20 dB (amplitude 0.1) is `tau * ln(10)`. `tau_low = 4.3 ms` gives 9.9 ms,
  inside the 5-15 ms band of IN-10.
- The excitation is summed into the body input, next to the string output, and
  passed to `BodyEngine::processMono` (double). The golpe shares the body's modes and
  adds no second body.
- The excitation buffer is at most 15 ms (720 samples at 48 kHz), allocated in
  `prepare()`.
- Peak normalisation (D): `A = 1` gives a peak of -12 dBFS at the body input.

### Rasgueado (`StrumGesture`)

Reuse the existing strum scheduler. A rasgueado is one `StrumGesture` with the
rasgueado flag, so it is one note event, as a strum is.

For N strings in the chord (N <= 6), stroke i (i = 0..N-1, in order of the stroke
direction) is at:

```
t_i = i * S / (N - 1)        S = flam_rasgueado_speed (ms, total span)
```

For N = 6 and S = 60 ms the offsets are 0, 12, 24, 36, 48, 60 ms. Each stroke is a
pluck through `StringEngine` with the nail attack: `nail_vs_flesh = 1`
(`fingerstyle-attack.md`, existing contact model).

**Trigger.** The imported MIDI path already reads `rasgueado`. A live trigger is
open (see INDEX open items): either the event class mapped through input routing, or a
keyswitch. This spec does not add a second live keyswitch without a decision.

**RT-safety.**
- The stroke queue is a fixed array of at most 6 strokes, allocated in `prepare()`.
- The golpe PRNG is a 32-bit state in the voice; `reset()` restores the seed.
- Stroke times are computed in samples as doubles, rounded to the nearest sample
  once. No allocation on the audio thread.
- Golpe and rasgueado are not in the body or string state path beyond the normal
  inputs, so `reset()` on `BodyEngine` and `StringEngine` covers them.

## Data model and parameters

| ID | Range | Default | Automatable | Unit | Control |
|---|---|---|---|---|---|
| `flam_golpe_amount` | 0-1 | 0 | yes | - | Flamenco card |
| `flam_rasgueado_speed` | 20-120 | 60 | yes | ms (total span) | Flamenco card |

Keyswitch: `TechniqueKeyswitch::golpe = 24`, appended. Confirm 24 is unused in
`TechniqueTriggers.h`.

## State, file format, migration

- Both parameters default to 0 / 60 and are read as no-ops when the golpe is zero
  and no rasgueado is triggered, so existing flamenco presets render unchanged.
- The PRNG seed comes from the event id, so an offline render is deterministic.

## Edition

| Item | Free | Pro |
|---|---|---|
| Flamenco nylon with golpe and rasgueado | Pro | Pro |

As roadmap 6.

## Performance budget

- Golpe: 0.02 units (one short excitation, one body input already running).
- Rasgueado: 0.01 units (scheduler only; strokes use the existing string path).

## Test plan

- **IN-10 (golpe).** Render the excitation alone at `A = 1` (before the body).
  - Band energy: most energy in 100-400 Hz, and a second peak in 2-4 kHz.
  - Duration: time from peak to -20 dB is 5-15 ms.
  - Deterministic: two renders with the same event id are bit-identical.
  - Then render through the body with the factory resonator reference: the
    100-400 Hz energy is still the largest band.
- **IN-11 (rasgueado).** Six strings, `flam_rasgueado_speed = 60`: stroke onsets
  within 2 ms of 0, 12, 24, 36, 48, 60 ms. Each voice has `nail_vs_flesh = 1`.
- **IN-10b (amount zero).** `flam_golpe_amount = 0` gives zero golpe energy.
- **IN-11b (speed bounds).** 20 and 120 ms give onset spans of 20 and 120 ms within 2 ms.
- Shared: IN-12 (existing flamenco presets bit-identical), IN-13 (visible controls for
  both automatable parameters), IN-14.

## Effort and dependencies

- **ED 10:** golpe excitation and body routing 4, rasgueado scheduling 3, keyswitch
  and card 1, tests 2.
- Dependencies: `fingerstyle-attack.md` (nail contact, `nail_vs_flesh`), rest stroke
  (`rest_stroke`, unchanged), `body-coupling.md` (body input), and the research file
  `docs/research/INSTR_flamenco.md` (missing; the 100-400 Hz and click values are D until
  it lands).
