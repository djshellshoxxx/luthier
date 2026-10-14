# Electro-Harmonix Nano Looper 360 — model specification

model_id: `ehx-nano-360` | category: Mono phrase looper | evidence: B functionality specified, hardware noise/converter unknown | status: **RESEARCH ONLY** (no sound model verified, do not advertise as equivalent).

## Sources, identity, and verified photo page
1. https://www.ehx.com/products/nano-looper-360/
- **Official/reference product photo page:** https://www.ehx.com/products/nano-looper-360/
- Photo file acquired: **NO**; original image rights not established. Image match note: Preserve memory switching safety while playing.
- Next evidence: exact production circuit revision/BOM or converter DSP, pot curves and licensed hardware measurement grid.

## Physical controls and switch behavior
- **Hardware controls:** LEVEL; LOOP selector 1–11; stomp.
- **Documented mapping:** LEVEL loop playback only; LOOP changes selected memory.
- Map every documented physical control to a single visible custom face widget; advanced internal trim may only appear under a distinct Advanced overlay and is NOT a physical knob. Implement functional bypass footswitch and preserve timing semantics.
- Physical control ranges and tapers not disclosed by sources must be measured; UI 0-1 is **not** a calibrated hardware scale.

## Audio-model signal path
Uncompressed 24-bit A/D/A at 44.1kHz; 360 seconds total across 11 persistent loop memories; unlimited overdub and undo/redo

Implementation approach: Bounded persistent phrase storage, buffered bypass, durable files; implement memory state/undo off RT.

- Analog hardware levels are volts with nominal source/loading impedance; set an explicit DI-to-volts calibration. 0dBFS does NOT mean 0dBu.
- No undocumented digital filters, converter bit depth, or loop limits may be asserted as reproduced.
- In the separate board, prepare all state off-audio-thread, chain left-to-right; process callback never allocates or locks. Bypass topology is per-model.
- Keep external legacy effects intact and old preset audio unchanged with new board disabled.

## Acceptance tests, measurements and failure cases
Primary response tests: 11 memory isolation, 360s shared budget, independent loop lengths, undo/redo and source rate migration.
Common suite: impulse, silence/no DC, sine sweep FRF at 5 knob positions, 4 gain levels, two-tone IMD, pluck decay, step response, bypass/null, on/off click, cross-sample-rate, block sizes 1/4/16/64/512, UI knob reachability, CPU budget, save/restore, deterministic output with noise disabled, stereo semantics, 30min randomized action soak.
- Compare against a licensed physical unit of the **same revision**; record measurement and uncertainty. Pass target thresholds are provisional until real reference measurements exist.
- No 'authentic', 'identical', or 'real hardware matched' claims before verified capture and tolerance report.

## Modular code placement and work sequencing
`Source/ReferencePedals/Models/ehx_nano_360.h/.cpp`; DSP unit test `Source/Tests/ReferencePedals_ehx_nano_360Tests.cpp`; isolated ModelRegistry entry; optional drawer face keyed by `ehx-nano-360`; no changes to `PedalType`.
1. Acquire revision evidence/photo rights; 2. write failing tests; 3. implement circuit/behavior processor; 4. compare measurements; 5. integrate UI/serialization; 6. full host+CI QA; 7. publish fidelity score.
Unknowns/constraints: Preserve memory switching safety while playing.
