# Board history publication allocation failures

Private base:dbfa90b284d48501007355950cb5a06f67178871. Focused native and sanitizer qualification passed; coherent full-product integration remains pending. No production header/ABI changes.

Original real-Board apply/undo/redo ordinal allocation sweeps reproduced14 mismatches in3tests/1138checks: new audio/topology version had published while stored mix remained old. The exact steady-state audio oracle uses the original1e-9 tolerance after settling the existing20ms mix slew. Expanded7tests on the original BoardModel produced81failures/2105checks, including transaction setup and loss of the oldest100-entry history item.

Change: stage the needed history entry, runtime-extra overlays and destination vector capacity before invoking the compiler. On successful publication, move/erase/clear only; History, BoardSettings and TopologyInfo moves are statically checked noexcept. Only vector capacity can change on failed staging; history contents, coalescing gesture and transaction flags remain unchanged. Undo/redo similarly stage the outgoing snapshot and destination slot first. replaceAll clones settings before publication. beginTransaction copies its baseline and both saved stacks before incrementing depth. Existing100-entry bound, gesture coalescing and redo semantics are retained.

Test-only fault injection extends the disabled-by-default standard new/new[] hook with ordinal and size-matched ordinal modes; legacy size-selector and paired nothrow behavior remain. Faults disarm before reporting. All standard new/new[] allocations in the mix-edit fixture’s apply/undo/redo are swept; real rendered audio, topology versions and complete undo/redo sequences are checked. Separate cases cover transaction setup, nested cancellation,100-entry capacity and same-gesture continuation. Snapshot/whole-restore coverage targets recoverable PedalInstance-vector allocations.

Explicit limitation: pinned JUCE var::clone() is declared noexcept yet allocates DynamicObject. An arbitrary injected allocation failure there calls terminate before compiler publication (snapshot ordinal2,size32; captured stack in external evidence). This candidate does not modify JUCE, suppress termination, or claim general JSON-OOM recovery. Settings clones now precede publication, but dependency termination remains a separate issue.

Standard allocation exceptions during pre-publication staging may propagate; there is no catch-and-ignore or dropped undo. Listener notifications remain after commit and outside the model lock; a listener exception may propagate after a fully coherent commit. This is publication/history consistency for the tested paths, not universal product bad_alloc atomicity or recovery.

## Actual qualification

Native full focused:140tests/208787checks,0failures,8.68s,stderr0. Binary SHA-256:ddcf03200eed4a515aec5366d58698ed69f2db0aaf56ac0ed2141e7b9281b699.
ASan+UBSan+leak full focused:140tests/210595checks,0failures,31.38s,stderr0. Binary SHA-256:7884b0b9dffcca69d810f7c9f5af2fbdc58d060840092516130cc1ad0e7c81b9.
The focused history corpus passes7tests/2104checks. Allocation sites swept:apply33,undo31,redo30,transaction setup9,100-entry edit5; targeted snapshot history vectors7 and whole-restore vectors2. Different full-suite totals reflect existing concurrent schedules.

All private model/engine/test TUs were instrumented; linked shared JUCE core/audio-basics/crypto objects were prebuilt and uninstrumented. No mutable main build objects used. ASAN_OPTIONS=detect_leaks=1:halt_on_error=1; UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1, no suppressions. Exact1374-source-file manifest and original-red binary/model identities are preserved in external evidence.

Independent timefx source review found no blocker under the stated single-writer/compiler-atomicity assumptions. This is a source review, not an independent rerun. Parent main remained frozen for build7; this patch has not been included in that binary.

Minimal dependency-termination reproducer, intentionally not an always-crashing suite test: create juce::var(new juce::DynamicObject()), arm failNextAllocationOfSizeOnThisThread(sizeof(juce::DynamicObject)), then call source.clone(). The pinned JUCE noexcept boundary terminates. No production recovery claim is made for that case.
