# Mono and stereo routing

**IDs:** PB-03. **ED:** 3.0. **Status:** missing (v1 chain is mono; `processStereo` exists for output).

**Summary.** Define how mono and stereo signals meet at pedal, merge and AMP IN jacks, and make the rules testable.

## User-facing behaviour
- Mono into a stereo input: duplicated, L = R.
- Stereo into a mono input (Merge input, AMP IN): `(L + R) x 0.7071`. A notice shows when stereo reaches AMP IN.
- Stereo through the amp: `acoustic-electric-and-stage-v2.md` section 5 (out of scope here; the amp input is mono, open question 5).

## Engine and data model
- Each buffer slot in the pool (file 16) carries a channel count (1 or 2). The compiler inserts a `Copy` op (mono to stereo) or a `Sum` op with the 0.7071 gain (stereo to mono) when a jack's type differs from its cable's.
- Stereo pedals process natively; mono pedals on a stereo path run twice (per channel, same parameters) or as a mono pedal fed by the downmix, decided by the pedal's declared width (file 08).
- No `dynamic_cast` or type check in `process`; the op already encodes the conversion.

## Parameters and data
None.

## State and migration
Stereo state is derived from the graph. Migrated boards are mono throughout (AMP IN is mono; the post-amp stereo output is the cabinet/room path, unchanged).

## Edition
Stereo pedals gated like other pedals (file 05). Mono and stereo are not gated separately.

## Performance budget
One `Copy` or `Sum` op = 0.005 units (file 14 routing estimate). Mono-on-stereo pedal running twice counts as two pedal costs, so the budget notes this.

## Tests
- **PB-03.** Mono to stereo and back. **Conflict to resolve:** with the spec's `0.7071` downmix, a coherent mono signal (L = R) round-trips at +3.0 dB, not within 0.1 dB. Recommended: keep 0.7071 (equal-power for uncorrelated stereo) and change the test to assert the documented +3.0 dB; or set the downmix to 0.5 for coherent input. Owner to decide before the test is written.
- Stereo into AMP IN is summed once and shows the notice.

## Effort and dependencies
ED 3.0. Depends on 14 (graph, slots), 08 (pedal width declaration).
