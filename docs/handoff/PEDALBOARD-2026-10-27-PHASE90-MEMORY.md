# Phase90 inline-memory admission and portable recovery — 2026-10-27

Code/test commit `4fd574c653101b11512b298a4d99f0003ba2fd8a`, same executor,
branch `codex/luthier-pedalboard-completion`, one integrator with completed
OpenAI read-only reviews. No Claude or external model API.

## Correction and focused results

Phase90 inherited unknown prepared-memory reporting despite owning only inline
state: its2,049-double oscillator table, stages, coefficients, filters, noise,
TimeInfo and base controls. Original RED is1test/3checks/2failures, exit1.

The estimator now validates rate8000–1000000, block>0 and policy0–2, then returns
`sizeof(MxrPhase90Script)` with zero temporary bytes, without constructing it.
The getter reports that same complete footprint. Pool runtime and dry pads remain
separately accounted. No heap allocation policy, audio law, bypass/transport,
parameter/state format, descriptor or production registration changed.

| Configuration | Result | Time |
|---|---|---:|
| Linux Pro Release |17tests /55,056checks, exit0|4.177s|
| Linux Free Release |17tests /55,056checks, exit0|4.377s|
| Linux Pro ASan/LSan/UBSan |17tests /55,056checks, exit0|10.522s|

Four memory tests join the existing13Phase90 tests. Coverage includes81metadata
rate/policy/control configurations, no allocation across model construction,
prepare/reset/query, block-size-independent bounds, invalid inputs, and test-only
pool preflight/acquire/reuse/config replacement/shared-pin refunds. Both old and
new runtime reservations remain counted until their last pins release. The
64,000,000-byte cap is unchanged. Extended metadata rates do not qualify audio
behavior beyond the separately tested model domain.

All original Phase90 regression predicates remain. CPU<0.4% is still an open
last-measured RED gate; the unresolved24-bit criterion remains unapproved. No
unchanged CPU benchmark was repeated for this metadata-only correction.

Evidence: `/workspace/luthier-evidence/phase90-memory-20261027/`. Build
`python3 <evidence>/build.py pro` (also `free`, `asan`); run
`python3 <evidence>/run.py <edition>`. Actual project TUs are freshly compiled
against current headers; existing matching JUCE modules/caches are reused.
Strict `ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, `LSAN_OPTIONS` unset.
No findings through shutdown; stderr only JUCE v8.0.10. Source/commands/module/
binary hashes and original RED/final logs/exits are retained. Reviews executed
no builds/tests. Getter/accounting closure is not acoustic/native qualification.

## Verified portable recovery

The older incremental bundle chain correctly declared its dependency on local
`f1efeded706b9dc57da7e941ca856b17eaad5183`, but did not itself carry that base.
A58,432-byte bridge now restores it from the previously parent-verified published
checkpoint `22d6b18d5e6bb024efc5143eba9e01ce0836eb46`, branch
`codex/luthier-checkpoint-20261025-build8b`. This is historical publication
evidence, not a new GitHub read or current-remote claim. The bridge's ten commits,
123objects/55blobs/727,536bytes were scanned for secret patterns and cache paths.
No match was found. Its SHA256 is
`f560448b5b810236bf83e71e8ee2db9d8ce8296bbc7cf9193baa3ff8d500b09f`.

A local preservation ref `codex/luthier-recovery-base-f1efeded` was created at the
existing base without checkout, overwrite or push. Initial Git command errors
(raw unnamed bundle tip, absent-ref verification exit and partial ref pattern)
were corrected and logged; these were not access denials or tests, and no
uncertain operation was blindly repeated.

`verify-recovery.py` seeded an isolated local bare repository with only the
published-base object graph, obtained from existing local objects, not network.
It proved f1efeded absent, verified/imported the bridge, then proved the current
code target absent and verified/imported its incremental bundle. Recovered tree
`421ebaed8d409ca635f0fe289c1a6fd200d55848` equals the tested code tree exactly;
`git fsck --no-reflogs` exits0. No environment/VM replacement occurred.

A second2,582,259-byte bundle preserves the seven retained worker-lane tips and
recorded integration ref, including deferred/unqualified models and original
failure provenance. Its3,342unique objects/1,395blobs/18,639,189bytes were scanned
with no secret/cache match. SHA256:
`23bb06df98d14b87e2137d3eb1edba6e434b3f1d742ef006fd4e6cbf2670552b`.
All eight recorded tips restore in the isolated repository; fsck exits0.
`PRESERVED-WORKER-REFS.json` records exact branch SHAs. This is preservation,
not integration or acceptance of those branches.

Recovery order: obtain/verify the known published22d6b18d base; verify/import the
bridge to f1efeded; verify/import the current incremental bundle; optionally
restore the retained-lane bundle/refs into isolated names. Compare recorded tree
and checksums before checkout, preserving local/untracked work. Dependencies and
native binaries are not included; no install/download or current GitHub access
was used for this recovery test. The private package retains predecessor history
and reports exact final documentation HEAD in its manifest/receipt.

## Compact continuation states and ownership

| State | Work | Owner / next evidence |
|---|---|---|
| Integrated |Phase90 memory admission; RC-5 rhythm DC; Dyna import/policy/safety; chorus memory/mode; DD-3T timing/tails |sole integrator, source-pinned handoffs|
| Integrated |Portable base bridge and retained-lane recovery |same integrator; verified source trees/ref identities, private only|
| Ready |Bounded Phase90 CPU experiment: paired channel-stage scheduling, preserving math/noise/store order |read-only review supports offline comparison; no speedup or production change yet|
| Validating |No tests/builds left active at this checkpoint |completed results above|
| Open |RC-5 wrapper/99memories/storage/host and remaining selected-model fidelity/UI/soak |five named DSP prototypes;zero registered|
| Blocked/open |Original CPU budgets, unresolved Phase9024-bit criterion, native/manual/hardware qualification |no threshold or scope weakening|
| Restricted |Source push/CI/merge/release/deployment/new dependencies/environments/security changes |no such action; denied operations not retried|

The proposed next CPU experiment must preserve exact baseline/candidate sources,
per-TU flags and audio/state comparisons, including alias/noise/reset/bypass,
then the unchanged480,000frame/48k/128/five-trial0°/180°benchmark. A proposed
`-fno-math-errno` flag is not fast-math but relaxes errno side effects; any result
must distinguish audio/state equivalence from complete FP-trap/errno equivalence.
Nothing in this handoff approves registration, the unresolved24-bit criterion,
a claimed SIMD speedup or any release.

Selected six/deferred twelve and attribution remain preserved. No full-product,
platform/format/release pass is inferred from component checks. No binaries,
build caches, private intake archive, vendor trees or credentials are packaged;
three pre-existing untracked build/cache paths stay preserved/excluded.
