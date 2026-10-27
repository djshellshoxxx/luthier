# Phase90 bounded CPU investigation — 2026-10-27

Starting HEAD `7afde71bfce1db91bc935c9d65bea15be6741e7e`, production code
`6e9129474255e097ddbe5f6eabd8fd7c8462cba9`, same executor/branch. One
coordinator, zero workers. **No runtime/test change is retained. CPU remains
RED and Phase90 unregistered.** No duplicate review or full rebuild occurred.

## Exact unchanged benchmark

Phase90 §3 requires <0.4% of one core at48kHz stereo,128-frame buffers;
§9 repeats <0.4%. The probe is `docs/coverage/probes/phase90-e58-actual.cpp`:

- Five fresh trials per stereo-phase setting0/180degrees. Default Script,
  speed0.5, depth/mix1, mismatch0.02, noise off, independent stereo histories;
  `prepare(48000,128,1)` precedes timing.
- 480,000 stereo frames:10seconds of audio,128-frame calls. Inputs are
  `0.1*sin(i*0.13)` and `0.1*cos(i*0.17)`. No host/transport updates occur in
  the timed loop. Neither channel count nor core count changes the denominator.
- Construction, prepare, allocation and waveform generation are outside timing.
  There is no explicit warm-up or first-block exclusion. Earlier FRF/THD probes
  execute first, so instruction code is not necessarily cold. None was changed.
- `CLOCK_THREAD_CPUTIME_ID` measures CPU seconds. `seconds*10` correctly equals
  `100*CPU_seconds/10_audio_seconds`. The limit is40ms CPU per10seconds of audio,
  or83.333ns per stereo frame. Probe exit0 prints measurements; it does not
  assert the CPU threshold.
- Existing GCC14.2.0 C++17 Release flags use `-O3`, NDEBUG, no fast-math/LTO/
  native-architecture change. Commands and matching JUCE object hashes are saved.
  No machine, clock, workload, denominator or toolchain substitution qualifies it.

The spec's rough25-flop estimate is not a replacement acceptance metric. It
does not name a reference processor; that portability ambiguity does not clear
the measured failures here or authorize qualification on a faster machine.

## Saved measurements and static hot-path evidence

Original `c49bffc8` baseline:0.496025180–0.552433890%.
Single-envelope corrected source:0.491946320–0.567572280%.
Original logs/SHAs remain intact. The latter is49.195–56.757ms per10audio
seconds,102.489–118.244ns per stereo frame. Approximately19–30% less CPU is
needed over that observed range, plus margin for reliable acceptance.

The current `render` body is byte-identical to that measured corrected source.
The sync fix guards callbacks not exercised inside this timed loop. Source and
installed `objdump` show eight stateful nonlinear stages per stereo sample,
retaining square roots/divisions and denormal guards. Every eighth sample,
E58 coefficient updates need two tangents and a discriminant square root per
distinct seeded stage frequency. Default mismatch prevents treating all four
stages as identical. Aligned-channel coefficient targets are already shared;
the preceding cutoff power calculation can still repeat. Fade trigonometry is
skipped at settled endpoints. Default Script/free mode avoids Block feedback
and enabled-sync work. Static call sites are **not dynamic time attribution**.
No installed `perf` was found; no download or privileged profiling was attempted.

## Exact-output experiment, not retained

A minimal candidate reused the left cutoff power result only when both
smoothed triangle operands were exactly equal. No coefficient cadence or DSP
law changed. A paired real-model comparator linked the baseline under a renamed
C++ class beside the candidate, preserving original source ownership notices.
It exercised five rates, blocks1/64/2048, both render entries, aligned/opposed/
changing stereo phase and deterministic parameter/bypass/sync/noise histories.
Result: **1,725,120 comparisons,0errors,max absolute difference0**, exit0.
This scoped comparison is not universal equivalence or sanitizer coverage.

The unchanged original benchmark produced:

| Source | Ten CPU trials | <0.4% |
|---|---|---|
| Candidate | 0.486868380–0.660431650% | All fail |
| Adjacent unchanged baseline control | 0.532187510–0.689867590% | All fail |

These supplement the earlier failures. Trial/run variation prevents claiming a
reliable sufficient improvement. The candidate is retained privately as source,
patch, commands, hashes and output; an exact inverse patch restored production.
The entire `Source` tree is byte-identical to the starting checkpoint. No further
unchanged probe loops were run. No new regression/sanitizer run was needed after
restoration: previous13-test/54,620-check Pro/Free/strict sanitizer results retain
their original source identity and are not claimed newly executed here.

## Blocker and next independent slice

The blocker is measured scalar processing cost versus the unchanged40ms/10audio-
second allowance. No sufficient safe optimization was demonstrated. Further CPU
work needs targeted profiling and a bounded cross-platform scheduling/vectorization
or exact-reuse design, with equivalence and unchanged numerical/safety gates.
No broad rewrite, approximate math, reduced cadence or quality change was made.

Next independent selected-pedal slice: **DD-3T tap boundaries/history reset**.
Current `ReferencePedals_boss_dd3tTests.cpp` covers500ms subdivisions/slow clock,
but lacks explicit133/800ms boundary/rejection and>2second averaging-history-reset
oracles. `BossDd3t::tap` contains these conditions; test quantized sample timing,
supported boundaries and a2.1second reset through real trigger/foot routes,
then diagnose only an observed defect. Preserve reconciled333.3/375/500ms mapping,
original tolerances, persistence and callback-allocation requirements. This
recommendation does not assert that tap implementation is defective.

The unresolved24-bit criterion, remaining soak/MIDI/fidelity/hardware acceptance,
coherent product rebuild/full-suite/validators and current Windows/macOS Intel+
ARM qualification remain open. All selected six and retained twelve are preserved.

## Recovery

Evidence: `/workspace/luthier-evidence/phase90-cpu-investigation-20261027/`.
The same-identity private Library checkpoint adds saved/new CPU records, rejected
candidate, comparator, complete flags/hashes/disassembly, verified Git bundle
and prior checkpoint. Manifest/receipt records final HEAD/SHA256/version. Bundle
requires `f1efeded706b9dc57da7e941ca856b17eaad5183`; verify/import separately,
preserving local work. No binary builds, caches, credentials, private intake
archive or vendor tree is packaged. Attribution and source provenance remain.

No denied operation occurred. No Claude/external API, push/CI/release/deployment,
installation/VM, credential/security change or denied Actions/UI/process retry
occurred. Current restrictions remain in force.
