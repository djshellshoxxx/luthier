---
name: reference-pedal-modeler
description: Researches specific physical guitar pedals and loopers, creates traceable revision-specific audio-model specs, tests an independently added model, and integrates it into Luthier's separate reference pedalboard without changing the existing effects racks.
---

# Luthier Reference Pedal Modeling Skill

## Trigger
Use when asked to research, recreate, model, validate, add or revise a named guitar pedal, analog circuit, modulation unit, compressor, rack effect or phrase looper for Luthier. Follow `spec/reference-pedalboard/ARCHITECTURE.md` and a separate `models/<id>.md`.

## Mandatory research order
1. Determine manufacturer, **exact revision, year and variant**, mono/stereo, power circuit and bypass type. Find original manual or manufacturer support page for control labels, gestures, mode matrix, impedance, signal level and routing.
2. For analog pedal, find independently documented schematic, BOM, PCB revision and datasheets. Derive frequency response and nonlinear topology from **the actual circuit**; use simulations (SPICE/Ngspice/LTspice or documented symbolic/nodal methods) with testbench values. Document alternatives and unknowns. For DSP/digital pedal, seek original manual and public algorithms; if proprietary algorithms are hidden, label `behavioral`, never `circuit accurate`.
3. Obtain a model-matched official product **photo reference**. Confirm visible hardware controls/model identifier; do NOT use a visually similar reissue as an unqualified original photo. Store official manufacturer image page, attribution, verification date, rights status; ship photos only with permission, otherwise draw independent generic face.
4. Write a new per-model spec following the template below before adding code. Do not merge multiple variations into one identity.
5. Check current Luthier source and recent branches: existing `EffectsChain` and `PedalRack` are protected. Do not modify or duplicate existing generic effect algorithms unless extraction is proven behavior-preserving. Use new namespace and source directory.
6. **RED**: add standalone DSP and lifecycle tests that fail on missing module (input/output, controls, footswitch, rate independence, no alloc in processing, buffers <=4, stereo, bypass, initial state). Run targeted local tests.
7. **GREEN**: implement exact topology or documented behavioral state machine; all runtime memory preallocated in `prepare`, model switches prepared off-thread. Smooth parameter and bypass changes, handle reset and latency.
8. **REFACTOR**: assess saturation frequency response/harmonic spectra, gain matching, physical knob calibration and CPU load. Compare to licensed hardware captures; list measured error and unknowns.
9. Integrate per-instance state and GUI face into **the new slide-out board**; test full chain order/controls, Free/Pro, host recall, undo/redo, MIDI mapping, offline rendering and cross-platform CI. No changes to existing rack on/off unless owner explicitly approves.
10. Merge only after green checks, documentation and reference-capture provenance. For visual correctness verify the pedal face manually against the linked **exact** product photo. Do not claim fidelity from a generic distortion or simple sine-based chorus.

## Per-model spec required fields
- `model_id`, model+revision, evidence class, feature category, UI photo link and reuse license, manufacturer manual URL, schematic/analysis URLs, datasheet/capture list, unknowns.
- Full knob/switch/button table: label, physical direction, pot taper, unit/range, default, permitted foot gestures, bypass semantics and MIDI.
- Stage-by-stage graph incl. nonlinear equations/component BOM, input/output impedance, headroom, voltage-to-digital calibration, stereo behavior, DC/AC coupling, noise, latency, level response.
- Captured audio test matrix, acoustic numerical criteria and pass/fail data, nominal resources/CPU, deterministic/state/reset tests, footswitch timing and host recall.
- Explicit build stage and status (`research`, `prototype`, `audio-measured`, `validated`). Never label `validated` before all required measurements and host tests are green.

## Reusable C++ contract
Use one reusable base with independent `ModelDescriptor` and `InstanceState`; processor implementing `prepare`, `reset`, `processStereo`, `setControls`, `getLatency`. Freeze immutable descriptor/pointer prior to process. Keep each pedal in `Source/ReferencePedals/Models/<Model>.{h,cpp}` and tests in `Source/Tests/ReferencePedal*.cpp`. `CMakeLists.txt` already uses source glob; confirm addition is included in test and plugin targets. No allocation, lock, I/O or reference cache refresh in callbacks. Registered API must preserve project state and plugin parameter order.

## Accuracy truth table
- Circuit-known + captured/hardware-calibrated: eligible for **hardware-matched** after passing tests.
- Circuit-known, no captures: **circuit-derived prototype**.
- Manufacturer gestures known, algorithms secret: **behaviorally implemented** once gesture/state tests pass.
- Controls/theme only: **UI mock**, no authentic effect claim.

## Stop conditions
If BOM/revision is ambiguous, photographed model differs, audio capture cannot be licensed, sample-rate/latency contract fails, required switch state unimplemented, or CI red: record blocker and keep model experimental/hidden, never silently fall back to a generic pedal and call it accurate.

## Mandatory references
`spec/reference-pedalboard/CATALOG.md`, `spec/reference-pedalboard/ARCHITECTURE.md`, per-model spec, `spec/roadmap/v2/pedalboard-v2.md`, `spec/performance-budget.md`, `skills/test-driven-development/SKILL.md`, `docs/RELEASING.md`.
