# Independent reference pedalboard: developer-ready specification

Version 0.1 | 2026-10-14 | Status: SPEC — NOT IMPLEMENTED

## Non-negotiable compatibility
- Existing `Source/DSP/Effects/EffectsChain.*`, `Source/UI/PedalRack.*`, `PedalType` stored IDs and current pre/post racks remain intact. No rename, slot reordering, change in default effect sound, or serialized parameter index changes.
- Implement as an **additional, optional, separately versioned** component `Source/ReferencePedals/` and `Source/UI/ReferencePedals/` with isolated model catalog. Default disabled, no resource allocation on the RT callback. Do not put brand models in the existing generic `PedalType` switch.
- Previous projects without the new state block must produce identical audio bitwise. Preserve unknown fields in all user/preset loads. Factory normalization goldens unchanged while board off.
- Product marks/images remain attributed to their manufacturers; release uses either commissioned independent artwork or licensed photos.

## User story and visible behavior
Header or Easy/Advanced opens **REFERENCE PEDALS** horizontally sliding drawer. Collapsed height 0, expanded responsive/pannable 220-380px; no forced editor resize beyond display work area. Selecting an empty slot opens a searchable model selector with thumbnails, manufacturer, family, evidence grade, controls and demo (only if implemented). Each selected physical-model profile appears as a visually accurate dedicated control face; footswitch toggles effect; drag handle reorders. At least 8 positions in a scrollable horizontal chain, scalable to 16 after performance tests. Each pedal has its **own physical controls**, not generic one-size-fits-all knobs. A collapsed summary indicates ON/OFF, pedal count and gain/CPU.

Controls: drawer visibility, board enable, pedal add/delete, model selection, footswitch, each physical knob/switch, move, duplicate, reset, dry/wet if hardware provides, preset save/load, undo/redo, MIDI learn, per-pedal info and provenance. Keyboard-only add/remove/reorder and screen-reader names. No unapproved hidden controls. On overflow, scroll horizontally; no controls offscreen without reachability. Footswitch feedback must reflect actual DSP state.

## Routing / chaining
Default **Input -> Reference pre-amp chain -> Existing preEffects -> Amp -> Existing postEffects -> Reference FX-loop chain -> Cabinet -> Room -> Master**, subject to actual graph inspection. Expose user-selectable placement PRE and LOOP and, when available, POST-CAB only with correct multi-output routing. Looper capture point is especially important: a looper before the amp records DI, after amp records amp signal; provide explicit placement selection. Avoid using a generic 'FX loop' that is actually after an amp/cab and mislabeling routing. No hidden insertion into Jam, backing track or mic buses. Bypass and dry route maintain correct delay/latency compensation. Internal order is user-defined series. Future split/mixer topology requires a separate graph compiler as in `spec/roadmap/v2/pedalboard-v2.md`; don't clone or break that roadmap.

## Data contract (JSON conceptual; production serializer use project conventions)
`referencePedals: {schema:1, enabled:false, drawerOpen:false, instances:[{instanceId:"uuid",modelId:"boss-ds1", revision:"DS1-NJM", placement:"pre", enabled:true, controls:{dist:0.5,tone:0.5,level:0.5}}], order:[...], assets:[]}`.
- Immutable model ID is not the display name. Keep per-instance UUID stable across drag/reorder.
- Unknown models preserve their state and render as disabled 'missing model'; never silently substitute unrelated audio.
- Normalize recorded knob values with per-control taper and preserve raw normalized host automation mapping. Add new APVTS parameters append-only at the end if fixed slot host automation is provided; do not create/destroy host parameters in live callbacks.
- Preset/host state restores full order and controls. Looper audio **must not** be serialized in an unbounded host chunk. Use a content-addressed, managed external asset with explicit missing-audio warnings, temp write+atomic rename, permissions and path redaction. Separate loop memory persistence from per-session transport.
- Version migrations, max instance count, missing model and concurrent host saves must be covered by tests.

## DSP interface
`ReferencePedalProcessor {prepare(sr, maxBlock); reset(); processStereo(double* L,double* R,int n) noexcept; setControlSnapshot(snapshot); getLatencySamples();}` where immutable model topologies and parameter descriptors are compiled/prepared away from audio thread. No memory allocation, filesystem I/O, mutex acquisition, logging, UI call, or branch rebuild inside processing. RT adopts new prepared graph via atomic handle + retired graph reclamation on message thread; if a swap pending keep processing the old graph, with a crossfade. Bound all blocks. Smooth gain/control updates and footswitch bypass at audio rate; implement model-specific buffered bypass vs true bypass; model input loading/impedance only when routing enables physical impedance emulation, since a plugin cannot directly alter analog pickup impedance. Stereo and mono behavior explicit. Input nominal reference level documented: -20dBu is analog hardware, not a fixed dBFS; calibrate digital analog-equivalent V per digital unit, expose optional trim.

### Model primitives (single-purpose reusable modules)
- `RCNetworks`: topology-specific bilinear/ZDF state-space filters from R/C and pot tapers, prewarping tests.
- `NonlinearCircuit`: diode Shockley I-V with bounded Newton iterations, op-amp saturation/rail/slew, transistor curves, WDF/MNA where needed, anti-aliasing oversampling + latency.
- `BBDDelay`: clock-modulated sample/hold, anti-alias/pre/de-emphasis, interpolation, analog noise and loss; only claim chip-equivalence after capture.
- `Modulators`: triangle/sine with phase and sample-rate continuity, stable all-pass network, FET resistance mapping.
- `Dynamics`: envelope, OTA response, hysteresis, gain smoothing and deterministic noise.
- `PhraseLooper`: explicit Empty/Recording/Playing/Overdubbing/Stopped/Undo/Redo/Clearing transitions, bounded loop memory, dry-through, worker storage; footswitch gesture decoding is a separate UI module with sample/timestamp boundaries.
- Reuse existing `Source/DSP/Common` and basic JUCE DSP helpers only when math and threading match; avoid changing original pedal semantics or factory rendering.

## Research fidelity and validation gate (mandatory PER pedal)
1. Record manufacturer, exact model/revision and photo provenance; manufacturer manual, actual control labels, switch/pot travel and ranges. For vintage variants mark photo difference.
2. Circuit evidence: independent schematic/BOM, R/C/semiconductor tolerances, revised PCB, transfer functions; determine what is unavailable. Digital model algorithms are proprietary unless documented; implement **behaviorally equivalent** version only and label as such.
3. Collect licensed hardware DI/capture at matched impedance, supply, reference levels, 44.1/48/96k; grid physical knob positions 0/25/50/75/100%, four input levels, single-tone sweeps, two-tone IMD, multisine, pluck attack, bypass and footswitch, variation noise. Save fixture checksums/provenance.
4. Validation: null/bypass and dry-through as documented, impulse/transfer/error across grid, harmonics vs frequency/level, attack/release, state transitions, switch clicks, under/overruns and CPU. Log RMS dB, THD spectrum, spectral envelope errors, latency, parameter monotonicity. Define threshold after observing a real unit; never invent a universal numerical 'exactness' threshold.
5. Runs on Windows/Linux/macOS with Debug ASan/UBSan where supported, 1/4/16/64/512/2048 samples and non-power-two blocks, sample rate changes, overload, 30 min random footswitch operations, project save/restore and offline render. All tests deterministic when noise disabled.
6. Performance: per-instance CPU budget measured in `spec/performance-budget.md` units, end-to-end chain ≤ existing audio budget. If over budget, block enabling additional instance with actionable message rather than degrade silently.
7. Legal review before release: independently created DSP only, no transcribed proprietary firmware, trademark in factual descriptive reference only, no unlicensed product face/art/photos inside distributed bundle.

## Conflict and integration notes
The existing `spec/roadmap/v2/pedalboard-v2.md` proposes a replacement/expanded board. This request instead requires an additional board, so implement a new namespace and have BOTH coexist until product-owner approves combining them. Required compatibility tests: classic rack + new board off bit-identical, classic on + new bypassed null, A->B differs from B->A, reorder playback follows instance ID, no old preset corruption, no RT allocation in chains.

## Increment plan
P0: Catalog + model specs + modeling skill (this PR); independent pure model-state and processing interfaces with tests. P1: additive processor stage initially OFF and host-state v1, regression. P2: slide-out UI and all gesture/control wiring. P3: first nonlinear reference pedal then circuit validation; P4: phase/chorus/compressor; P5: straightforward phrase looper before RC-5-like advanced model; P6: expanded series library/photo permissions; P7: long stress, release gating. Do not confuse a compiled stub or generic effect proxy with a finished faithful model.
