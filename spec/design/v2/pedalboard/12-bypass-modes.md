# Bypass modes and pop (True, Buffered)

**IDs:** PB-08 (pop), PB-09 (buffer). **ED:** 5.0. **Status:** partial (Soft exists in `Pedal.cpp` ~81; True and Buffered missing).

**Summary.** Add True and Buffered bypass beside Soft, a modelled pop on True, and latency-neutral padding of every bypassed leg.

## User-facing behaviour
- **Soft (v1).** Pedal runs; wet crossfades with dry over 10 ms. Kept for migrated presets.
- **True.** Pedal is removed from the path. With `popEnabled`, a modelled pop is added (`cables-and-brands.md` 5.4). The cable is not isolated: the cable and the next input both load the source.
- **Buffered.** Pedal is off, but a unity buffer (1 MOhm in, 1 kOhm out, design values, `cables-and-brands.md` 4.2) sits at its input, so the cable before it no longer loads the source. Processing is skipped.
- Pop default -40 dBFS, width 4 ms (design values; PB-08 range -44 to -36 dBFS, 2-6 ms).

## Engine and data model
- Bypass mode selects the op: Soft keeps `RunPedal` with a crossfade; True emits a `Copy` (in to out) instead of `RunPedal`; Buffered emits a buffer `RunPedal` (Buffer type, file 08) feeding the copy. Mode changes are structural edits (file 16) and swap with a 50 ms fade (see 16).
- **Latency pad (from file 01).** Every bypassed leg (True, Buffered) is padded by the pedal's nominal latency L, so the path latency is constant. Pad delay is preallocated per instance.
- Pop: a short, shaped impulse added at the switch point, generated in `process` from a precomputed table (no allocation); the table is built in `prepare`.
- Tier and bypass filter coefficients are computed at compile time, never in `process`.

## Parameters and data
- State: `bypassMode` (soft, true, buffered), `popEnabled` (default on for new pedals).
- No host parameter for bypass mode. Pedal bypass remains the existing `preN_bypass`/`postN_bypass` toggle, plus footswitch (file 10).

## State and migration
- Migrated pedals: Soft, pop irrelevant (Soft has no pop). New pedals: True.
- A v1 preset never has True or Buffered, so it renders as before (PB-M01).

## Edition
Soft only on Free. True and Buffered Pro (file 05).

## Performance budget
Buffer 0.01 units when used; pops are negligible. Pad delays are memory, not CPU.

## Tests
- **PB-08.** Peak -44 to -36 dBFS, 2-6 ms wide; off when `popEnabled` is off.
- **PB-09.** A 10 m Economy cable before a Buffer matches 0 m at 5 kHz within 0.5 dB (same as CB-07 in `cables-and-brands.md`).
- **PB-07 sub-assert (file 01).** Latency constant across all three modes.

## Effort and dependencies
ED 5.0. Depends on 01 (latency), 08 (Buffer), 16 (ops).
