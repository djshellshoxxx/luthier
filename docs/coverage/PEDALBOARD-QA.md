# Pedalboard QA evidence

2026-10-23 checkpoint. These focused runs do not certify feature completeness,
the full application, native plugins, hardware or DAWs.

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
