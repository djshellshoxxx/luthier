# Minimum page coverage and broker proposal outcomes — 2026-10-27

Committed code checkpoint **ed802201ffa2a5aa686efbdfc07c828a54723ea4**:
geometric lookahead e5e73ec92f3b973869b7c71cb24a045afc5da322, followed by an
explicitly RED CE-2 acceptance test. Base1973a7659e98ac064154314a03d6e4e26bb8810e.
No current full-suite, product, native-format or release qualification is claimed.
Same executor; root is sole Git integrator. Original failures and thresholds remain.

## Integrated: actual minimum forward-window coverage

`PagedLooperCore::minimumForwardWindowFrames` is a constructor-free geometric
query, not a residency/I/O deadline guarantee. Current page plus A ahead pages
cover at least 1+(A−1)P+R frames for A>=1, where R is final-page length. A0
covers at least one frame. Entire-loop residency returns UINT64_MAX; invalid
geometry returns0. Unsigned widened arithmetic avoids overflow, including
UINT64_MAX input. Independent review proved the formula and attainable minimum.
The new test independently enumerates every cursor in4,704 small geometries,
plus invalid/extreme inputs and the actual90minute/192k partial-tail example.

Original API-absent compile RED is retained. With the correct query and original
A12 policy, the original closure is RED:55tests/37,520checks/fourfailures.
The old nominal393,216frames assertion passes while actual minimum380,929frames
is only1.984s, below the unchanged2s requirement. Policy fixture now uses A13
with the SAME14read slots; minimum413,697frames for that loop and at least
393,218 even for a one-frame tail. No extra PCM buffers or generic reconfiguration.
Worst tested RAM55,328,256bytes remains below64,000,000bytes; the old nominal
assertion is retained and the stronger actual/worst-tail/decimal-cap checks added.

| Focused unchanged storage/core/state/resume + new geometry closure | Result | Wall time |
|---|---|---:|
| Pro Release |55tests /37,536checks, exit0|2.220s|
| Free Release |55tests /37,536checks, exit0|2.191s|
| Pro strict ASan/LSan/UBSan |55tests /37,536checks, exit0|4.231s|

Sanitizers: detect_leaks=1,alloc_dealloc_mismatch=1,halt_on_error=1;
UBSANhalt_on_error=1,print_stacktrace=1; LSANunset. No sanitizer findings.
Exact commands/hashes/logs/RED and root/independent reviews are in
`/workspace/luthier-evidence/looper-window-coverage-20261027/`.
Reproduce with its build.py<pro|free|asan>, then run.py<edition>; binaries/cache
were shared only after the preceding worker completed. Source hashes pin results.

## Integrated known RED: CE-2 original BBD-on THD gate

Sonnet proposal `libfile_741fc3ba42cc8191ab1fb557048eb451` was received through
supported native text read after ONE generic materialization failure. Original
download did not succeed and was not retried. Native text11102bytes lacked its
terminalLF versus reported11103; this distinction is retained. Exact normalized
patch/base/proposed-result hashes verified; only adaptation removes duplicate48k
from proposed rates, leaving ordered48/44.1/88.2/96/192k. Adapted test SHA256
d7c90a5ae3f2508f8c6bf5b70d4047caa9831c2726f758f9fd8afe86fabb44a6.

New `RefPedalCe2.bbdOnOriginalThdGate` preserves original0.5–3% ratio and original
coherent two-second/last-second1kHz harmonic2..9 fixture at−12dBFS, depth0,
BBDon,noiseoff. Existing four tests unchanged. Pro focused real-source binary:
**5tests/55checks/fivelower-boundfailures, exit1,0.477s**. BBD-on THD in rate order:
0.0017%,0.0097%,0.0075%,0.0008%,0.0003%. All upper-bound/finite/fundamental
checks pass. No CE-2 DSP correction, Free/sanitizer rerun or fidelity claim.
This commit exposes an existing unqualified requirement; do not skip or weaken it
to restore green. Source-path diagnosis is separate from an approved DSP change.

Attribution: Claude Sonnet5.5 proposal request req_011CgSCi6gRdEwjTLigR1M6L;
root verified transport/content, adapted unique rates and executed the test.
Existing source authors and notices retained. Exact proposal, failed download,
native recovery, scoped patch, build commands and exit evidence are preserved in
`ce2-broker-intake-20261027` and `ce2-bbd-gate-20261027` evidence directories.

## Rejected, private/unapplied: Phase90 Fable CPU candidate

Native artifact `libfile_d987922b6f688191a6990f39e38d5f2a` version0,
`Luthier-Phase90-CPU-Claude-proposal-073.md`,25922bytes, SHA256
4f7e32497c22e60385d6c71caea5e20e168ee6928092e7297552413cb4ce9882.
Complete native text plus terminalLF matches this exact artifact hash. Base645cc99e;
CPP-only target and14 source dependencies, patch/base/result hashes all verified.
Candidate result SHA a51c2037757c74daa058bc46077008de2e50347c345f540039cc9f09e9cb3982.
No product source application. Existing failed candidates remain unchanged.

Original E58 CPU workload,480000stereo48k frames/block128,5trials each0°/180°,
thread CPU time, original plain GCC Release flags (no fastmath/no-math-errno),
same installed JUCE objects. All10trials of BOTH baseline and candidate FAIL
unchanged<0.4% gate: baseline0.50028350–0.60042850%; candidate0.50122741–0.59128511%.
Harness exits1 for failed gate although each probe exits0. Printed FR/THD lines
match; this is not full binary output/state equivalence or an established speedup.

Fresh independent static review found no concrete positive-block semantic defect
under serialized calls/ordinary buffer aliasing, but n<=0 now evaluates arithmetic
and copied state instead of a literal no-op, and hoisted detune multiplication
can alter rounding/contraction. C++17 does not itself disable FMA. Neither concern
is silently accepted. No larger validation or candidate rescue rerun warranted
after CPU failure. No named registration,24bit-gate change or qualification.

Attribution: Claude Fable5.1 request req_011CgSCiC4dVUHhQ7bvPH1VE; root verified
and measured, independent Codex reviewer inspected semantics. Broker recorded
$2.445405; no separate paid requests here. The broker's5min cache was not reused;
do not issue requests to manufacture cache savings. Exact command/probe/logs/
binary/source hashes/trials in `phase90-broker-intake-20261027/cpu-validation`.

## Queue / recovery / restrictions

| State | Slice | Ownership / next action |
|---|---|---|
| Integrated GREEN |Minimum geometric forward coverage |e5e73ec9; actual disk deadlines remain open|
| Integrated RED |Original CE-2 BBD-on THD acceptance |ed802201; independent source diagnosis pending|
| Rejected |Phase90 block-local-state CPU candidate |private only, all10CPUtrialsfail; retain original gates|
| Active |Directional storage read-ahead prerequisite |storage worker owns only LooperStorage.h/.cpp and new direction test; no core/model edits|
| Ready later |Stopped reverse playback integration |must align core readiness/prime/read direction; keep forward recording and manual OD restriction|
| Blocked engineering |RC5midplay/activeOD reverse behavior |existingOD commit/cancel and phase-anchor policy unresolved; no destructive default|
| Open |Other original DSP/CPU/RT/full/native/manual/release gates |six selected, twelve retained deferred;0/6 registered/accepted|

Root owns CE2 test integration, all Git/docs/evidence and Phase90 candidate review;
no overlapping implementation. Ongoing worker changes are NOT part of this fixed
checkpoint. Prior Library same-identity version27 and every nested predecessor,
source bundles and verified recovery-base bridge are retained; next guarded save
uses expected27. Recover isolated against published22d6b18d + verified bridge +
current over-f1ef bundle; never reset another tree. Source remains local: no push,
CI, merge, release, deployment, new software/VM/credentials/security changes.
Denied Actions read and extra callback stress remain unexecuted/unretried.
Frozen completion report stays pinned4ceb1b5d; later prerequisites are not silently
folded into its effort ranges. No manual/hardware/native acceptance inferred.
