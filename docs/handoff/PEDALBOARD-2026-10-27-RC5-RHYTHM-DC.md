# RC-5 rhythm DC correction — 2026-10-27

Code/test commit `809f48bdc84e51f183709a9f010f275922336f7f`, starting from
`c37e66878b118b596d52371c1a83d21b58d55d9b`. Same executor/single integrator;
one completed OpenAI read-only reviewer. No Claude/external model API.

## Original requirement, RED and narrow correction

RC-5§9.11 requires rhythm DC below−80dBFS: `abs(mean)<1e-4`. The helper directly
summed decaying tonal/noise voices, leaving a nonzero mean. Original default
fixture:48k/120BPM/pattern1/variation0/kit1/seed0x51a7, zero input, two full
phrases8seconds warmup and eight full phrases32seconds measurement. It fails
2of5checks, exit1, at0.000268781076564FS per channel (about−71.4dBFS).
The window and threshold remain unchanged.

One existing1Hz `DcBlock` now processes the synthesized sum before the existing
20ms level smoother, then untouched dry is added. Prepare/reset initialize/clear
its fixed inline state. Selection/start/stop/locate retain state; triggers, grids,
voice generation and clock counters remain unchanged. Putting the blocker before
level preserves the stop/level envelope instead of adding an independent filter
tail. The local pi constant was renamed to avoid the newly included helper's
namespace constant; its numeric literal/value is unchanged.

The1Hz corner is an engineering choice, not a hardware-matched claim. Its
analytic attenuation at the lowest authored34.5Hz kick floor is about0.0036dB.
This uses the existing WP7 helper and preserves WP16/WP24 authorship and original
synthesized-pattern provenance; no manufacturer audio or new dependency copied.

## Measured results

| Configuration | Result | Time |
|---|---|---:|
| Linux Pro Release |11tests /21,594checks, exit0|12.533s|
| Linux Free Release |11tests /21,594checks, exit0|11.009s|
| Linux Pro ASan/LSan/UBSan |11tests /21,594checks, exit0|121.961s|

The original default fixture now measures about5.72453e-8FS. The expanded matrix
has149executions:114pattern/variation pairs at48k/kit1, plus all7kits at five
44.1–192k rates on pattern1/variation0. It renders457,524,000stereo frames including
warmup, at full rhythm level, using two warm/eight measured complete phrases per
case. Worst measured mean is approximately7.73394e-7FS. One configuration overlaps
the two sweeps. This is not the entire3,990-combination Cartesian product, nor
every tempo/seed. All cases use120BPM and the fixed documented seed.

New dry tests compare additive mixed versus wet-only helper output exactly through
stop/start, locate and reset at all five rates, without clipping hiding a mismatch.
Existing partition, bounded random processing, selection, fractional clock, reset
and seek-identity tests remain unchanged and pass. No callback allocation-counter
change or sanitizer finding through shutdown; stderr only JUCE v8.0.10.

Evidence: `/workspace/luthier-evidence/rc5-rhythm-dc-20261027/`. Build
`python3 <evidence>/build.py pro` (also `free`, `asan`), run
`python3 <evidence>/run.py <edition>`. The focused closure rebuilds only the actual
rhythm/test project TUs; matching unchanged JUCE module objects/caches are reused.
Original RED, first narrow GREEN and final expanded source/commands/module/binary
hashes/logs/exits remain. Strict options:
`ASAN_OPTIONS=detect_leaks=1:alloc_dealloc_mismatch=1:halt_on_error=1`,
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, `LSAN_OPTIONS` unset.
The121.961second sanitizer process stayed healthy; no duplicate/restart occurred.
The read-only reviewer ran no tests and found no scoped implementation blocker.

## Compact continuation state and ownership

| State | Work | Owner / evidence |
|---|---|---|
| Integrated |RC-5 rhythm default/matrix DC and dry-preservation correction |sole integrator;809f48bd, results above|
| Integrated |Five named DSP prototypes; Dyna policy/safety, chorus memory/mode, DD-3T timing/tails |prior handoffs/private checkpoint chain;0named registered|
| Active |Phase90 prepared-memory admission |sole integrator; `Models/mxr_phase90_script.{h,cpp}`, `Tests/RefPedalPhase90MemoryTests.cpp` working diff|
| Validating |Phase90 sizeof-only memory report/constructor-free bounds |original1test/3checks/2unknown-report failures retained; focused regressions pending|
| Ready |Finish Phase90 memory checks and test-only pool replacement/shared-pin accounting |same integrator; read-only review confirmed no owned heap|
| Open |RC-5 wrapper, full rhythm Cartesian/tempo/seed acceptance,99memory integration, host/storage/hardware |this component correction is not whole RC-5 acceptance|
| Blocked/open |Phase90 CPU<0.4%, unresolved24-bit metric; Dyna original numerical/CPU failures |unchanged original thresholds/evidence|
| Restricted |Source push/CI/merge/release/deployment/new dependencies/environments/security changes |no such action attempted; denied operations not retried|

All selected six and deferred twelve stay preserved. No named registration,
product/full-suite/native/plugin/release qualification is implied. The private
Library checkpoint preserves all predecessors and a verified bundle requiring
`f1efeded706b9dc57da7e941ca856b17eaad5183`. The active Phase90 working diff is
separately labeled, not part of the committed bundle source. Verify checksums
and recover into an isolated branch preserving all local work. No caches/binaries,
private intake archives/vendor trees/credentials are packaged.
