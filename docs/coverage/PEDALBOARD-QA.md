# Pedalboard QA evidence

2026-10-23 checkpoint. These focused runs do not certify feature completeness,
the full application, native plugins, hardware or DAWs.

## Coherent complete Linux Pro executable, 2026-10-24

A single source-frozen native CMake/Ninja build completed at source checkpoint
`bfc01448409a3294ae71cd78a081d763dc541a1a` (documentation-only differences
from `347f0ce5` plus the subsequent engine/E40 commits already integrated there).
The local build identity is deliberately pinned to `pedalboard-local-qa`; it is
not release provenance. No source was changed during this coherent build.

`LuthierTests RefPedal ReferencePedals ReferencePhraseLoop` executed under Xvfb:
**151 tests, 5,033,311 checks, 3 failed tests / 39 failed checks, 39.6 seconds**.
The failures are preserved in `native-pedalboard-checkpoint.log`: sparse legacy
slot move, missing required first-line ownership headers, and `test.event`
fixture pollution of the real registered-model hostile-input sweep. Ownership
and legacy/scope fixes were subsequently integrated without weakening checks;
the next source-frozen native rebuild is pending execution. Fixture factories
now use the existing scoped test registry instead of leaking into later suites.

The separate unchanged release-decay gate still failed after the prior NoBody
fixture isolation: P-Style Bass note42 measured 34.7 dB decay against the 35 dB
output requirement (1/25 checks, 5.3 seconds). This is an unresolved baseline
failure, not a passing gate. `native-release-fixture.log` preserves the result.

The 30-minute simultaneous-edit soak is running from an immutable standalone
core snapshot. Memory samples are recorded every ten seconds; its result must
be read after completion before claiming a pass. This snapshot does not cover
the next integrated engine/host/DSP/UI changes.

## Coordinator native focused run

Real `Board`, `BoardModel`, `BoardEngine`, `TopologyCompiler`, `PedalPool`,
registry and registration sources, MIDI/router, generic models and prototypes
were compiled with g++ C++17 -O2. JUCE8.0.10 core/audio_basics shared objects
were linked. The harness uses actual thread-local global-new counting;
`malloc` and other C allocator paths are not intercepted by this hook.

`RefPedalSweepTests.cpp` exercises registered models at 44.1/48/88.2/96/192kHz,
partitions1/16/37/64/481/2048, deterministic partition comparison to1e-9,
each control at0/25/50/75/100%, and NaN/Inf/denormal/±1e6 inputs.
`RefPedalSoakTests.cpp` runs audio concurrently with100 structural/control
edits per second. Rejections are counted and audio bounds remain checked.

Result before delay/reverb integration: **3 tests, 4,619 checks, 0 failures**.

```
RefPedalSweep.registeredModelsAreDeterministicAcrossRatesAndPartitions: 1201 checks, 0 failures
RefPedalSweep.allRegisteredControlsRecoverFromHostileInput: 3410 checks, 0 failures
seed=0x51a7 seconds=30 edits=3000 rejected=296 blocks=125904 samples=55544648 callback_new=0 invalid=0
RefPedalSoak.concurrentEditsAndProcessingRemainFiniteAndAllocationFree: 8 checks, 0 failures
```

The earlier isolated3-model sweep passed2 tests/1,714 checks; the combined
run above included seven generics plus three engine prototypes. Neither run
means the still-unregistered specified models passed.

Runtime:30seconds default. `LUTHIER_SOAK=1` requests1800seconds;
`LUTHIER_SOAK_SECONDS=N` permits30..7200seconds. Seed0x51a7 is fixed.
New source integration requires rerunning; no ASan/UBSan/TSan claim yet.
The full native Linux Pro application/test/plugin build is in progress.

## Durable checkpoint

The intake+initial DSP complete-history Git bundle was verified with
`git bundle verify` and saved in ChatGPT Library as `luthier-intake-and-dsp.bundle`.
SHA256: `39b8d85e0a5552e03df65abfd81ee027ac21d28c4b8862650f7822db0e3cb93a`.
It records commit `e733841e4e84e5cab9c99225386829c77c6c63cd`; later work is
on the coordinator branch and subsequent checkpoints, not in that early bundle.

## Unattended versus manual

Unattended: unit/seeded state/sweep/concurrent editing checks above.
Pending: full30-minute soak, flat-memory and CPU measurements, full sanitizer
and race runs, full regression suite, standalone smoke and plugin validators,
all native Windows/macOS verification and packaging.
Unavailable here: real audio-device latency/xruns, listening comparison,
physical pedal measurement, and actual Reaper/Live/Logic/Bitwig/Cubase/FL
operator acceptance. These must never be reported as unattended passes.

## Completed 30-minute wall-clock concurrent soak

The immutable core executable completed successfully (exit0,1810.19s driver wall
time including startup/collection) with seed0x51a7 and1800s requested workload:
180,000edits,18,318rejectededits,6,913,402blocks,3,049,960,869samples,
callback_new0,invalid0. Combined sweep/soak:3tests/6111checks/0failures.
Memory sampled every10s peaked at26,628KiB resident; kernel high-water was29,212KiB. The final
resident sample was11,932KiB. C++new is intercepted; Cmalloc/hostwrapper
allocation and locks are not certified by this hook. This source snapshot
predates E47 and the later routing/host/UI/paged-looper integrations.

The next integrated executable at05bbdb8e passed206tests/5,649,492checks in40.1s
underXvfb, including realprocessor no-message-pump automation(6checks),
1000writes/sec automation(1504checks), ownership and sparselegacy regressions.
This pass covers the frozen source, not later queued commits or fulllegacy suite.

A coordinator seeded1000-malformed-load test now runs the real Board/codec and
audio after each rejected load:7004checks/0failures, no countedcallbacknew,
finiteboundedoutput and unchanged serializedmodel. Grammar/type/range/order
corruptions use seed0x54a7ef01. Its coherent focused compiler/run commands and
logs are retained; fullapplication inclusion is the next native QA checkpoint.
