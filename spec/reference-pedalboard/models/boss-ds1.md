# BOSS DS-1 (modern op-amp revision) — model specification

model_id: `boss-ds1` | category: Distortion | evidence: A circuit documented; not audio validated | status: **RESEARCH ONLY** (no sound model verified, do not advertise as equivalent).

## Sources, identity, and verified photo page
1. https://www.boss.info/global/products/ds-1/
2. https://www.electrosmash.com/boss-ds1-analysis
- **Official/reference product photo page:** https://www.boss.info/us/support/by_product/ds-1/product_images/
- Photo file acquired: **NO**; original image rights not established. Image match note: 1978 TA7136 version differs; do not label it the same profile.
- Next evidence: exact production circuit revision/BOM or converter DSP, pot curves and licensed hardware measurement grid.

## Physical controls and switch behavior
- **Hardware controls:** DIST; TONE; LEVEL; stomp on/off.
- **Documented mapping:** DIST governs op-amp gain; TONE changes relative low/high bands and produces a mid dip; LEVEL scales output.
- Map every documented physical control to a single visible custom face widget; advanced internal trim may only appear under a distinct Advanced overlay and is NOT a physical knob. Implement functional bypass footswitch and preserve timing semantics.
- Physical control ranges and tapers not disclosed by sources must be measured; UI 0-1 is **not** a calibrated hardware scale.

## Audio-model signal path
Input buffer -> shunt-feedback transistor booster -> variable non-inverting op-amp gain -> antiparallel hard-clipping diodes to reference -> passive two-branch tone network -> level -> output buffer; original buffered JFET bypass

Implementation approach: Component-value-derived linear sections; diode nonlinear solve with antialiasing; transistor booster bias/asymmetry; buffered bypass not dry-wire.

- Analog hardware levels are volts with nominal source/loading impedance; set an explicit DI-to-volts calibration. 0dBFS does NOT mean 0dBu.
- No undocumented digital filters, converter bit depth, or loop limits may be asserted as reproduced.
- In the separate board, prepare all state off-audio-thread, chain left-to-right; process callback never allocates or locks. Bypass topology is per-model.
- Keep external legacy effects intact and old preset audio unchanged with new board disabled.

## Acceptance tests, measurements and failure cases
Primary response tests: Sweep DIST for harmonic spectrum, TONE for low/high transfer, nominal sensitivity, input-level cleanup; compare captures with identified revision.
Common suite: impulse, silence/no DC, sine sweep FRF at 5 knob positions, 4 gain levels, two-tone IMD, pluck decay, step response, bypass/null, on/off click, cross-sample-rate, block sizes 1/4/16/64/512, UI knob reachability, CPU budget, save/restore, deterministic output with noise disabled, stereo semantics, 30min randomized action soak.
- Compare against a licensed physical unit of the **same revision**; record measurement and uncertainty. Pass target thresholds are provisional until real reference measurements exist.
- No 'authentic', 'identical', or 'real hardware matched' claims before verified capture and tolerance report.

## Modular code placement and work sequencing
`Source/ReferencePedals/Models/boss_ds1.h/.cpp`; DSP unit test `Source/Tests/ReferencePedals_boss_ds1Tests.cpp`; isolated ModelRegistry entry; optional drawer face keyed by `boss-ds1`; no changes to `PedalType`.
1. Acquire revision evidence/photo rights; 2. write failing tests; 3. implement circuit/behavior processor; 4. compare measurements; 5. integrate UI/serialization; 6. full host+CI QA; 7. publish fidelity score.
Unknowns/constraints: 1978 TA7136 version differs; do not label it the same profile.
