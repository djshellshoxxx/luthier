# Ibanez TS808 Tube Screamer (original topology) — model specification

model_id: `ibanez-ts808` | category: Overdrive | evidence: A circuit documented; not audio validated | status: **RESEARCH ONLY** (no sound model verified, do not advertise as equivalent).

## Sources, identity, and verified photo page
1. https://www.ibanez.com/eu/products/detail/ts808_99.html
2. https://www.electrosmash.com/tube-screamer-analysis/pedals/distortion/tube-screamer-analysis.html
- **Official/reference product photo page:** https://www.ibanez.com/eu/products/detail/ts808_99.html
- Photo file acquired: **NO**; original image rights not established. Image match note: Do not assume modern TS808HWV2's additional boost matches original.
- Next evidence: exact production circuit revision/BOM or converter DSP, pot curves and licensed hardware measurement grid.

## Physical controls and switch behavior
- **Hardware controls:** OVERDRIVE; TONE; LEVEL; stomp on/off.
- **Documented mapping:** OVERDRIVE varies feedback loop gain; TONE shapes highs; LEVEL output attenuation.
- Map every documented physical control to a single visible custom face widget; advanced internal trim may only appear under a distinct Advanced overlay and is NOT a physical knob. Implement functional bypass footswitch and preserve timing semantics.
- Physical control ranges and tapers not disclosed by sources must be measured; UI 0-1 is **not** a calibrated hardware scale.

## Audio-model signal path
Input buffer -> op-amp gain with diode feedback soft clipping -> characteristic clipping high-pass/mid emphasis -> active tone/level -> output buffer; JFET switching

Implementation approach: Nodal or wave-digital feedback diode topology with preserved frequency selective gain, optional op-amp slew/rail behavior.

- Analog hardware levels are volts with nominal source/loading impedance; set an explicit DI-to-volts calibration. 0dBFS does NOT mean 0dBu.
- No undocumented digital filters, converter bit depth, or loop limits may be asserted as reproduced.
- In the separate board, prepare all state off-audio-thread, chain left-to-right; process callback never allocates or locks. Bypass topology is per-model.
- Keep external legacy effects intact and old preset audio unchanged with new board disabled.

## Acceptance tests, measurements and failure cases
Primary response tests: Check soft vs hard clip knee, mid emphasis, dynamic cleanup vs guitar volume, drive-dependent frequency response.
Common suite: impulse, silence/no DC, sine sweep FRF at 5 knob positions, 4 gain levels, two-tone IMD, pluck decay, step response, bypass/null, on/off click, cross-sample-rate, block sizes 1/4/16/64/512, UI knob reachability, CPU budget, save/restore, deterministic output with noise disabled, stereo semantics, 30min randomized action soak.
- Compare against a licensed physical unit of the **same revision**; record measurement and uncertainty. Pass target thresholds are provisional until real reference measurements exist.
- No 'authentic', 'identical', or 'real hardware matched' claims before verified capture and tolerance report.

## Modular code placement and work sequencing
`Source/ReferencePedals/Models/ibanez_ts808.h/.cpp`; DSP unit test `Source/Tests/ReferencePedals_ibanez_ts808Tests.cpp`; isolated ModelRegistry entry; optional drawer face keyed by `ibanez-ts808`; no changes to `PedalType`.
1. Acquire revision evidence/photo rights; 2. write failing tests; 3. implement circuit/behavior processor; 4. compare measurements; 5. integrate UI/serialization; 6. full host+CI QA; 7. publish fidelity score.
Unknowns/constraints: Do not assume modern TS808HWV2's additional boost matches original.
