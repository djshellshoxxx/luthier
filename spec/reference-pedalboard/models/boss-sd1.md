# BOSS SD-1 Super OverDrive — model specification

model_id: `boss-sd1` | category: Overdrive | evidence: B behavior documented, full revision schema pending | status: **RESEARCH ONLY** (no sound model verified, do not advertise as equivalent).

## Sources, identity, and verified photo page
1. https://www.boss.info/global/products/sd-1/support/
- **Official/reference product photo page:** https://www.boss.info/global/products/sd-1/
- Photo file acquired: **NO**; original image rights not established. Image match note: Do not reuse TS808 transfer function without explicit differences.
- Next evidence: exact production circuit revision/BOM or converter DSP, pot curves and licensed hardware measurement grid.

## Physical controls and switch behavior
- **Hardware controls:** DRIVE; TONE; LEVEL; stomp.
- **Documented mapping:** DRIVE clipping/gain; TONE response; LEVEL output.
- Map every documented physical control to a single visible custom face widget; advanced internal trim may only appear under a distinct Advanced overlay and is NOT a physical knob. Implement functional bypass footswitch and preserve timing semantics.
- Physical control ranges and tapers not disclosed by sources must be measured; UI 0-1 is **not** a calibrated hardware scale.

## Audio-model signal path
Mid-emphasis filtering -> asymmetric diode feedback clipping -> tone filter -> output; buffered bypass

Implementation approach: Asymmetric diode threshold topology informed by published manufacturer design; precise circuit component analysis required for fidelity level A.

- Analog hardware levels are volts with nominal source/loading impedance; set an explicit DI-to-volts calibration. 0dBFS does NOT mean 0dBu.
- No undocumented digital filters, converter bit depth, or loop limits may be asserted as reproduced.
- In the separate board, prepare all state off-audio-thread, chain left-to-right; process callback never allocates or locks. Bypass topology is per-model.
- Keep external legacy effects intact and old preset audio unchanged with new board disabled.

## Acceptance tests, measurements and failure cases
Primary response tests: Even and odd harmonics from asymmetric clipping; low-end tightening; input-level cleanup.
Common suite: impulse, silence/no DC, sine sweep FRF at 5 knob positions, 4 gain levels, two-tone IMD, pluck decay, step response, bypass/null, on/off click, cross-sample-rate, block sizes 1/4/16/64/512, UI knob reachability, CPU budget, save/restore, deterministic output with noise disabled, stereo semantics, 30min randomized action soak.
- Compare against a licensed physical unit of the **same revision**; record measurement and uncertainty. Pass target thresholds are provisional until real reference measurements exist.
- No 'authentic', 'identical', or 'real hardware matched' claims before verified capture and tolerance report.

## Modular code placement and work sequencing
`Source/ReferencePedals/Models/boss_sd1.h/.cpp`; DSP unit test `Source/Tests/ReferencePedals_boss_sd1Tests.cpp`; isolated ModelRegistry entry; optional drawer face keyed by `boss-sd1`; no changes to `PedalType`.
1. Acquire revision evidence/photo rights; 2. write failing tests; 3. implement circuit/behavior processor; 4. compare measurements; 5. integrate UI/serialization; 6. full host+CI QA; 7. publish fidelity score.
Unknowns/constraints: Do not reuse TS808 transfer function without explicit differences.
