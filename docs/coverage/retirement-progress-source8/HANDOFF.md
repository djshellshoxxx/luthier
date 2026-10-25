# Retirement progress/control-guard source checkpoint

This engine-branch commit intentionally preserves an exact-root **source patch**, rather than mixing the divergent worker production tree with root8's older audited Generic A implementation. It does not change this branch's runtime Source. Do not treat an artifact-only cherry-pick as deployment of the fix.

Base: `6d9b5943217d6e0872f65aea6514ebf40fc789e1`, SourceTree `9f790ebe860c680cc989f461ad7355ea2a772892`. Source intent: reclaim eligible completed-reader owners while a successor reader is active, retain the original eight-owner bound and zero callback lock/allocation/wait, and serialize previously racing control-side publication/reclamation. No new model registration or BoardModel change.

`adapter.patch` contains all implementation and regression sources plus the proof/ledger. `MANIFEST.json` binds its SHA, exact changed-file hashes, test binary hashes and results. `evidence/` preserves the original failures and final logs. `DESIGN-AND-EVIDENCE.md` defines ordering, finite exhaustion, lock order and remaining boundaries.

## Recovery / next coordinator actions

1. Verify the manifest's adapter SHA. In an isolated checkout of the base (or reviewed Source-equivalent descendant), run `git apply --check /path/to/adapter.patch` then `git apply /path/to/adapter.patch`. This checkpoint was apply-checked against current root's Source-equivalent descendant; root was not edited.
2. Reconfigure and rebuild affected targets coherently. BoardEngine private layout and RenderPlan layout change; Board.cpp and all dependent TUs must be rebuilt. A new test TU is included. Do not mix old objects/vtables/class sizes. Public BoardModel/IPedal interfaces are unchanged.
3. Run actual host/API/memory/history/retirement/allocator suites, full-native selection, then final-source300s and original long-duration gates. The focused149-test results are not a full-host or cross-platform qualification. Do not attribute prior root8b300s results to this adapter.
4. Repeat platform/native diagnostics before integration/release decisions. The running Windows diagnostic/source6d9 is separate; do not cancel or replace it as part of this checkpoint.

The optional focused harness expects an isolated `source/` tree alongside a `qualification/` directory containing these three harness files. Its compiler/JUCE/shared-object paths reflect the selected Luthier environment and must be deliberately supplied when recovering elsewhere. No objects/build caches/assets are included. Standard CMake remains the coherent host/product verification path.

Lifecycle: one non-nested audio reader; control calls serialized by the new offaudio guard; prepare/release/destruction require stopped audio and quiescent model mutations. No arbitrary concurrent prepare/edit support is claimed. Extra-state callbacks run outside the guard with retained runtime owners. Synthetic phase tests exercise helper phases, not forced scheduler pauses in actual Board processing. Shared JUCE objects were uninstrumented in focused sanitizer runs. JUCE noexcept JSON-clone OOM and the project's broader release/CPU/model gates remain open.

No root merge, release, native-branch push, registration, or deployment is performed by this checkpoint. Only the engine source-artifact branch is published, after a secret/private-data scan and fast-forward verification.
