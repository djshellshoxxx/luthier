# CB-08: True-bypass pop

**Items:** CB-08
**ED:** 2
**Source:** `cables-and-brands.md` 5.1, 5.4, 10 (CB-08), 11; `pedalboard-v2.md` 3.5

## Summary

When a pedal in true bypass is switched in or out, add a seeded 4 ms half-Hann transient at the tier's peak level at the pedal output.

## Status

Missing. No pedal bypass modes or pop exist in `Source/` outside tests. The v1 soft bypass has no pop by definition.

## User-facing behaviour

- A pedal with `bypassMode = True` and `popEnabled = true` makes a short click on each footswitch change.
- Peak per tier (table 5.1): Economy -34 dBFS, Standard -40, Pro -44, Boutique -46. Unmodelled: none.
- `popEnabled = false` gives a silent switch.
- No pop on load, on preset change, on `reset()`, or when the host moves the bypass state (see Design).

## Engine / DSP design

**Pop waveform.** Precomputed once per pedal when the board compiles.

```
N     = round(0.004 * fs)                       // 4 ms
w[n]  = sin^2( pi * n / N ),  n = 0 .. N-1      // half-Hann: one lobe, peak 1 at N/2
pop[n]= sign * A_tier * w[n]
A_tier= 10^(popPeakDb(tier) / 20)               // table 5.1
sign  = +1 or -1 from seed ^ kSaltPop ^ instanceIndex
```

- Width (support above -20 dB of peak): about 3.2 ms, inside the 2-6 ms test.
- Storage: `std::vector<double>` sized N at compile. At 192 kHz that is 768 doubles per pedal.
- `kSaltPop` is a new salt, distinct from `kSaltCable` (`NoiseFloor.cpp:12`).

**Playback.** A per-pedal `popPos` (integer index, `-1` means idle).

- Trigger: the message thread writes a `std::atomic<uint32_t> popSerial` when a footswitch change is accepted. `process` compares it with its last-seen serial; on a change it sets `popPos = 0`.
- Per sample, while `popPos >= 0 && popPos < N`: `out += pop[popPos++]`. When `popPos == N`, set to idle.
- The pop is added at the pedal output after voicing (file 09) and after the pedal's noise sum, so it is the last term at the output.

**Design decisions (not in the spec, flagged):**
- The pop is triggered only by a user footswitch change. `reset()` sets `popPos = -1`, so a reset never clicks.
- A bypass change made while a pop is playing restarts the pop from 0. It does not sum two pops.
- Host-automated bypass (if any) follows the same serial path. Set `popEnabled` to control it.

### RT safety

- No allocation in `process`. The vector is built on the message thread at compile.
- The trigger is one atomic load per block. No locks.
- Double precision for the waveform; the sum to the `float` output buffer follows the existing pipeline.
- `reset()` clears `popPos` only; it does not touch the waveform.

## Data model and parameters

- No host parameter. Pedal state: `bypassMode` (`soft`, `buffered`, `true`; shared with `pedalboard-v2.md` 3.5), `popEnabled` (bool, default true, as `pedalboard-v2.md` 2.4 states).
- No new numeric parameters. Levels are the constants in table 5.1.

## State, file format and migration

- Board block per pedal: `popEnabled`. Missing key reads as `true`, which matches the default for new pedals. Migrated pedals are Unmodelled, so the key has no effect on them.
- No state for `popPos` or the waveform; both are derived.

## Edition gating

- The pop level follows the pedal tier. Free allows Standard only, so Free pops are always -40 dBFS.
- `popEnabled` writable in Free: not specified by section 8. Proposed: writable. Confirm with `pedalboard-v2.md` edition table.

## Performance budget

- Idle: one integer compare per pedal per block.
- Active: one load, one add, one index increment per sample for 4 ms (384 samples at 96 kHz). Negligible.
- Within the per-pedal line of section 9 (0.01 units).

## Test plan

- **CB-08 (pop).**
  - Peak per tier within 1 dB of table 5.1 (measured on a render with a bypass toggle at a known sample).
  - Width (support above -20 dB) between 2 and 6 ms.
  - No pop with `popEnabled` off (sample-exact zero output difference from a no-toggle render).
- Added: `reset()` during a pop leaves `popPos = -1` and emits no transient.
- Added: two toggles within 4 ms do not produce a summed peak above one pop's peak plus 1 dB.

## Effort and dependencies

- ED 2.
- Depends on: `pedalboard-v2.md` 3.5 (bypass modes, footswitch path) and 2.4 (`popEnabled`); `character-wear.md` 1 (seed); file 09 (voicing and noise order at output); file 04 (RT check).
