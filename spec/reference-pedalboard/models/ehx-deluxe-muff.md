# Electro-Harmonix Deluxe Big Muff Pi — model specification

model_id: `ehx-deluxe-muff` | category: Fuzz / gate / parametric mid | evidence: B manual specified, internal gating envelope unknown | status: **RESEARCH ONLY** (no sound model verified, do not advertise as equivalent).

## Sources, identity, and verified photo page
1. https://www.ehx.com/products/deluxe-big-muff-pi/
- **Official/reference product photo page:** https://www.ehx.com/products/deluxe-big-muff-pi/
- Photo file acquired: **NO**; original image rights not established. Image match note: Do not label identical to classic Muff when extra circuits active.
- Next evidence: exact production circuit revision/BOM or converter DSP, pot curves and licensed hardware measurement grid.

## Physical controls and switch behavior
- **Hardware controls:** VOLUME; TONE; SUSTAIN; ATTACK; GATE; MIDS LEVEL; MIDS FREQ; MIDS Q; BASS BOOST; MIDS stomp; EXP.
- **Documented mapping:** MIDS LEVEL up to +/-10dB, MIDS FREQ 310Hz-5kHz, Q switch, GATE threshold and ATTACK contribution.
- Map every documented physical control to a single visible custom face widget; advanced internal trim may only appear under a distinct Advanced overlay and is NOT a physical knob. Implement functional bypass footswitch and preserve timing semantics.
- Physical control ranges and tapers not disclosed by sources must be measured; UI 0-1 is **not** a calibrated hardware scale.

## Audio-model signal path
Classic Big Muff core + separately switchable mids parametric EQ, gate and bass boost, attack dry component

Implementation approach: Reuse validated Big Muff core then add independent stateful noise gate and parametric EQ with expression modulation.

- Analog hardware levels are volts with nominal source/loading impedance; set an explicit DI-to-volts calibration. 0dBFS does NOT mean 0dBu.
- No undocumented digital filters, converter bit depth, or loop limits may be asserted as reproduced.
- In the separate board, prepare all state off-audio-thread, chain left-to-right; process callback never allocates or locks. Bypass topology is per-model.
- Keep external legacy effects intact and old preset audio unchanged with new board disabled.

## Acceptance tests, measurements and failure cases
Primary response tests: EQ peak frequency and gain extrema, gate hysteresis, effect on bypass, expression range, order of attack branch.
Common suite: impulse, silence/no DC, sine sweep FRF at 5 knob positions, 4 gain levels, two-tone IMD, pluck decay, step response, bypass/null, on/off click, cross-sample-rate, block sizes 1/4/16/64/512, UI knob reachability, CPU budget, save/restore, deterministic output with noise disabled, stereo semantics, 30min randomized action soak.
- Compare against a licensed physical unit of the **same revision**; record measurement and uncertainty. Pass target thresholds are provisional until real reference measurements exist.
- No 'authentic', 'identical', or 'real hardware matched' claims before verified capture and tolerance report.

## Modular code placement and work sequencing
`Source/ReferencePedals/Models/ehx_deluxe_muff.h/.cpp`; DSP unit test `Source/Tests/ReferencePedals_ehx_deluxe_muffTests.cpp`; isolated ModelRegistry entry; optional drawer face keyed by `ehx-deluxe-muff`; no changes to `PedalType`.
1. Acquire revision evidence/photo rights; 2. write failing tests; 3. implement circuit/behavior processor; 4. compare measurements; 5. integrate UI/serialization; 6. full host+CI QA; 7. publish fidelity score.
Unknowns/constraints: Do not label identical to classic Muff when extra circuits active.
