# Reference pedals: build plan

Reconciles spec/reference-pedalboard/* (ChatGPT/Codex) with the repo. Where they disagree, ARCHITECTURE.md wins on structure; PROGRESS.md decisions win on naming and evidence.

## Scope
Pedals (first release, all Evidence A, Fidelity CircuitDerivedPrototype): overdrive (TS808-type), distortion (DS-1-type, Distortion+-type, RAT-type), fuzz (Big Muff-type, Fuzz Face Ge-type), phaser (Phase 90-type), compressor (Dyna Comp-type). Evidence-B pedals (loopers, DD-3, CE-2W, Deluxe Muff...) are deferred: hidden circuitry, behavioural only; skipped per the accuracy rule.

## Modules (one responsibility each)
| Module | Path | Owner |
|---|---|---|
| Contract | Source/ReferencePedals/ReferencePedalModel.h | done |
| Circuit primitives | .../CircuitPrimitives.h | worker P |
| Models | .../Models/<Id>.{h,cpp}, each exposes descriptor + factory | model workers |
| Registry | .../ModelRegistry.{h,cpp} id -> descriptor + factory | integration |
| Board | .../ReferenceBoard.{h,cpp}: instances (UUID), order, placement, bypass crossfade, RT-safe swap, up to 16 | integration |
| State | .../ReferenceBoardState.{h,cpp}: versioned ValueTree, unknown-model preservation | integration |
| Engine hook | LuthierEngine: Pre stage before existing preEffects, Loop stage after postEffects | integration |
| UI | Source/UI/ReferencePedals/: Drawer, ModelPicker, PedalFaceView (custom paint per FaceSpec), keyboard + a11y | UI worker |
| Tests | Source/Tests/ReferencePedal*Tests.cpp | each owner |

## Naming / brands (decision D1)
Display names are generic and original. Real names only in docs/research and spec/reference-pedalboard. The catalogue holds one `brandLabel` hook, empty by default, so the owner can decide on licensed naming without touching DSP.

## Acceptance (per pedal)
Finite output at all knob corners x levels x {44.1,48,96 kHz} x block {1,64,512}; deterministic; no alloc in process; documented test vectors from the model spec (gain at 1 kHz, corner frequencies, clip level); old presets bit-identical with board off; A->B differs from B->A.

## Milestones
M1 contract + primitives + scout. M2 five model workers in parallel + board + state + engine hook. M3 UI. M4 audit/QA/fixes. M5 Windows Pro CI build.
