# MXR Phase 90 (vintage Script circuit) — model specification

model_id: `mxr-phase90-script` | category: Phaser | evidence: A circuit documented; script vs block revision explicit | status: **RESEARCH ONLY** (no sound model verified, do not advertise as equivalent).

## Sources, identity, and verified photo page
1. https://www.electrosmash.com/mxr-phase-90/pedals/delay/mxr-phase-90.html
2. https://www.jimdunlop.com/content/manuals/M101.pdf
- **Official/reference product photo page:** https://www.jimdunlop.com/mxr-phase-90/
- Photo file acquired: **NO**; original image rights not established. Image match note: Vintage Script analyzed; modern M101 photo is a *reference* image only, not the same circuit revision.
- Next evidence: exact production circuit revision/BOM or converter DSP, pot curves and licensed hardware measurement grid.

## Physical controls and switch behavior
- **Hardware controls:** SPEED; stomp.
- **Documented mapping:** SPEED controls LFO frequency; depth/mix/feedback are circuit constants not visible knobs.
- Map every documented physical control to a single visible custom face widget; advanced internal trim may only appear under a distinct Advanced overlay and is NOT a physical knob. Implement functional bypass footswitch and preserve timing semantics.
- Physical control ranges and tapers not disclosed by sources must be measured; UI 0-1 is **not** a calibrated hardware scale.

## Audio-model signal path
Input buffer -> four cascaded FET-controlled all-pass stages -> wet/dry summing notch response with feedback path -> slow triangle/smoothed LFO

Implementation approach: Four first-order time-varying allpass stages; coordinated FET resistance and LFO; stable feedback.

- Analog hardware levels are volts with nominal source/loading impedance; set an explicit DI-to-volts calibration. 0dBFS does NOT mean 0dBu.
- No undocumented digital filters, converter bit depth, or loop limits may be asserted as reproduced.
- In the separate board, prepare all state off-audio-thread, chain left-to-right; process callback never allocates or locks. Bypass topology is per-model.
- Keep external legacy effects intact and old preset audio unchanged with new board disabled.

## Acceptance tests, measurements and failure cases
Primary response tests: Two moving notches under sweep, max/min speed cycle estimate, bypass gain, no zipper.
Common suite: impulse, silence/no DC, sine sweep FRF at 5 knob positions, 4 gain levels, two-tone IMD, pluck decay, step response, bypass/null, on/off click, cross-sample-rate, block sizes 1/4/16/64/512, UI knob reachability, CPU budget, save/restore, deterministic output with noise disabled, stereo semantics, 30min randomized action soak.
- Compare against a licensed physical unit of the **same revision**; record measurement and uncertainty. Pass target thresholds are provisional until real reference measurements exist.
- No 'authentic', 'identical', or 'real hardware matched' claims before verified capture and tolerance report.

## Modular code placement and work sequencing
`Source/ReferencePedals/Models/mxr_phase90_script.h/.cpp`; DSP unit test `Source/Tests/ReferencePedals_mxr_phase90_scriptTests.cpp`; isolated ModelRegistry entry; optional drawer face keyed by `mxr-phase90-script`; no changes to `PedalType`.
1. Acquire revision evidence/photo rights; 2. write failing tests; 3. implement circuit/behavior processor; 4. compare measurements; 5. integrate UI/serialization; 6. full host+CI QA; 7. publish fidelity score.
Unknowns/constraints: Vintage Script analyzed; modern M101 photo is a *reference* image only, not the same circuit revision.
