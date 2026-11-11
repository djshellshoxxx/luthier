# CE-2W interrupted mode-transition correction — 2026-10-27

Code/test commit `30d0474b28dbbf871db72880749f6d121dbbee04`, starting from
`c4044491563ea2af5e9251176ec574c3f2b4d872`, same executor/branch and integrator.
One OpenAI read-only reviewer; no Claude/external model API.

## Original failure and minimal fix

CE-2W§9 requires mode changes without a discontinuity exceeding−30dBFS.
The existing implementation fades between modes over10ms. A second selection
within that interval set `oldMode` to the abandoned target, discarding the
currently audible mixture. This creates a real step rather than a new fade
from current output.

The corrected original-source test fails30of60checks across15cells (five rates
44.1–192k, partitions1/64/2048), exit1. Event-window left-channel jumps range
0.146293741923324–0.147962893610151 versus the unchanged0.0316227766limit.
Deviation from independently rendered fixed-mode voice composition reaches
0.090359173911343. Input is0.1peak/100Hz, noise/depth off, with rapid switches
3/5/6.5ms after the first request.

The initial fixture also measured signal startup from zero. Review identified
that unrelated contribution. Its raw source/results remain separately retained;
the fixture was corrected to the warmed mode-event window and rerun against the
saved original model/header. The same failures/jumps remain. The corrected RED
checks left steps and both-channel composition; final coverage extends the
unchanged step threshold to both stereo outputs. No threshold was relaxed.

The model now retains three convex voice weights. On retarget it saves the
current weights and follows the existing linear10ms transition toward the new
one-hot selection. Settled output selects that exact voice. All three existing
BBD/filter histories continue advancing once per sample; no extra BBD rendering,
allocation, bypass/parameter/state/registration or fidelity-law change occurs.
Reset initializes the selected one-hot weights. The inline state adds48bytes
on this build; the new `sizeof(model)` memory bounds/reports account for it.
CE-2W storage at192k now reports208,328bytes, separate from the prior checkpoint's
208,280measurement. The64,000,000-byte cap remains unchanged.

## Verified results

| Configuration | Result | Time |
|---|---|---:|
| Linux Pro Release |14tests /1,407checks, exit0|0.788s|
| Linux Free Release |14tests /1,407checks, exit0|0.883s|
| Linux Pro ASan/LSan/UBSan |14tests /1,407checks, exit0|4.510s|

These include two new mode tests, all eight previous CE-2/CE-2W tests and four
memory/pool tests. The fixed-mode oracle renders real models with identical
histories independently of transition control. It establishes low-level,
noise-off composition; no clipping/hardware-fidelity/CPU inference is made.
The reset/reprepare coverage-only test confirms exact equality with a fresh
selected-mode model at five rates. Both new render paths show no allocation-count
change; previous distortion/impulse/buffer/partition/stale-history gates pass.

Evidence: `/workspace/luthier-evidence/ce2w-mode-reversal-20261027/`.
Build `python3 <evidence>/build.py pro` (also `free`, `asan`); run
`python3 <evidence>/run.py <edition>`. Strict checks enabled:
`ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, `LSAN_OPTIONS` unset.
No sanitizer finding through shutdown; stderr only JUCE v8.0.10.
All project TUs are fresh; only matching unchanged JUCE modules are reused.
Original/corrected RED source and command provenance, GREEN source/module/binary
hashes, exits and logs remain. The first interim GREEN build was not executed;
only the final source/test cohort above is claimed passing.

## Open gates and recovery

CE-2W stays unregistered/experimental. CPU, original buffer/null interpretation,
remaining fidelity/soak/host/UI/native/hardware qualification remain open.
Phase90 CPU<0.4% is still RED and its24-bit criterion unresolved/unapproved.
All selected six/deferred twelve models remain preserved.

Next bounded integration candidate: the existing selected Dyna Comp source/tests
at retained DSP tip `40ed1621b52a6b9110e15d87747924cf59b96e3d`, selectively reviewed
against current dependencies, without wholesale common-DSP/drive imports or
registration. Existing thresholds and original candidate failures must follow it.

The guarded same-identity Library checkpoint retains the complete predecessor
chain and verified Git bundle requiring
`f1efeded706b9dc57da7e941ca856b17eaad5183`. Verify manifest/checksums and import
into an isolated branch preserving local work. Notices/authorship and fixture
provenance are retained. No binaries/caches/private intake archive/vendor copies
or credentials packaged. No push/CI/merge/release/deployment, new dependency,
environment or security change, nor denied Actions/UI/process retry occurred.
