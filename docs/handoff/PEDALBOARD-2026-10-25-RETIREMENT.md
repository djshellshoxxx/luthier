# Pedalboard retirement and test integration checkpoint

2026-10-25 UTC. Production source immediately before this handoff is
`f3f468939478712adea7322d3aabe5ed72b4719f`. Query HEAD for the documentation
checkpoint. This remains unfinished development, not a release candidate.

Latest coherent product build remains280e182a (fresh build6, all five Linux Pro
targets, exit0). Its actual CLAP18pass/0fail/3unsupported skips and VST3 strictness5
all30rate/buffer pairs pass. Later source changes require a coherent rebuild;
do not assign those binary results to the new source.

## Completed stress and focused evidence

- Root280 engine/API/routing/admission/retirement/MIDI/allocation:119tests/197413
  checks; registered sweep2tests/6724checks, all pass.
- Root28030-second seeded concurrent edit/render grind:3000attempts,296rejected,
  124986blocks/55139657samples,callback_new0,invalid0,exit0.
- Root280300-second grind:30000attempts,3001rejected,1293149blocks/570492627samples,
  callback_new0,invalid0,exit0. This is board-engine coverage, not proof that the
  entire processor is lock-free. Historical longer soak logs remain separate.
- Scoped UI ASan/UBSan/LSan candidate:10000actual operations,21editor destructions,
  546native closes,271drag cancels,272accepted changed drops;3tests/393630checks,
  exit0,no suppressions.81UI/test TUs plus allocator helper instrumented;
  JUCE/backend/processor/DSP and other UI remain uninstrumented. Original nothrow
  allocator and53482byte/55allocation X11-helper leak failures are preserved.
- Fresh root280 UI selection8tests/114checks and MIDI/bus/E46 selection35/743
  pass. The actual standalone process stayed alive12seconds under Xvfb, then
  was deliberately terminated; this is no hardware/audio/listening acceptance.

## Newly integrated source and original failures

879be6be adapts the precise single-reader retirement mechanism from a6541899,
preserving c6 reserve-before-publication. The original owner-list limit is eight;
saturation rejects before compile/publication/model mutation. Six retained
current/pending pointers are protected by a stable even sequence; no callback
mutex, allocation or waiting was added. Exact root280 four-file candidate passes
native97/191417,ASan/UBSan/leak97/197733,TSan retirement6/9898 and MIDI publication
2/3012,all exit0/no diagnostics. Shared JUCE runtime objects are uninstrumented.
Coordinator and timefx reviewed the SC ordering/lifecycle argument. Rebuild
Board.cpp as well as BoardEngine.cpp because the private implementation grew.
One nonnested audio reader, serialized control writer and stopped prepare/release
are required. Universal post-publication history bad_alloc rollback remains open.

8d239a30 reuses only two additional capacity fixture files from Claude1c9ae70b;
original2failures become9tests/27checks pass with assertions/licensing unchanged.
cf2212fb corrects raw-control event attribution and lifetime-aware GUI deduplication;
original21failures/539checks become6tests/539checks pass, preserving exhaustive
actual Pedalboard traversal.041fbcb6 closes the test-only X11 connection/library
after GUI shutdown, matching the clean sanitizer candidate. These fix tests and
their environment; none is a blanket ref_* visibility/search/CPU exemption.

f3f46893 shares the existing diagnostics test override with state backups; the
production default path is unchanged. The original three backup failures are
reproduced against the read-only sandbox Documents mount. Correct-target private
candidate passes targeted4/58,HostState31/8623,StateModel18/130,migration6/68;
47new checks preserve default/reset path,exact bytes and20-file retention. An
invalid mixed-target overlay attempt(SIGSEGV) is retained and explicitly excluded
from product evidence; its compiler selector was corrected to LuthierTests.dir.

The failure-only macOS collector is now invoked from the ordinary build workflow;
17synthetic tests pass,with no actual Mac crash/backtrace claim. Integrated
create-only draft/frozen-SHA release guards pass8fixtures and independent hostile
early-match review. Packaging identity/checksum/signature fixes pass12fixtures,
using temporary synthetic payloads/keys,not native installers or distribution
signing. Release strictness10,LTO,full unit and platform gates remain intact.

## Preservation and next work

Previous source checkpoint0db477a5 is verified on
codex/luthier-checkpoint-20261025-build6. Portable archive1697854bytes,sha256
`a09f568be92808212e47cad49c89f0b2d087951d59efe83961351d293146079f`, saved Library
identity `libfile_d8c810d7eac48191a7b5ff322a2de3d0`. Its incremental verified bundle
requires public b5bec374 base and preserves ten separate source histories plus
tracked/untracked source WIP. Binaries/objects/caches/private intake are excluded.
Claude final preserved1f646a65 remains unchanged; no whole divergent merge.
New DSP data3715d9ff and host dd36ba63 were independently secret-checked/pushed
to their own branches and exact remote hashes verified. Do not merge all lanes.

Seven existing slots continue: coordinator integration; engine complete E69
memory closure; DSP release-cap qualification; host native-workflow review and
RT transport; state independent DSP review; timefx bounded native diagnostics;
UI actual search routing. No worker is waiting solely for the parent.

Remaining red/open gates: whole-processor20trylocks/callback; CQ03 exact-ID/spec
decision (original predicate unchanged); full64,000,000byte prepared-memory
closure; original Pro bass35dB output versus measured34.7dB; named model/looper
registration/acceptance; complete native/installer/manual/hardware/DAW/listening.
The universal0.25s release candidate is an explicit engineering change within
the documented0.2–0.4s damping intent,not an arithmetic or hardware-discovery claim;
unchanged35dB output/raw40dB gates and held/sustain behavior must qualify and get
independent review before integration. Global search's old deferred test passes
despite373reference IDs lacking actual search routing; UI owns a real fix and
regression,not an exclusion. Preview command transport passes module tests but
is unwired; no production callback lock has been removed or accepted.

Next: qualify these scoped candidates,coherently rebuild/test the corrected
batch,and push the reviewed bounded Windows/macOS native diagnostic workflow.
It uses immutable SHA,existing supported targets and explicitly unqualified
artifacts; no full known-bad matrix,release/tag/overwrite or guessed platform
fix. Mac exact-state/backtrace evidence must be newly collected,not described
as recovered. Required signing secrets/hardware have not been created or assumed.
