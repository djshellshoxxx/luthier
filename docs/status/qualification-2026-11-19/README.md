# Supplemental qualification evidence

Candidate: `51fd4de677fd0d4a056ded5ee2e8845b9e81584d`.
Read the [qualification report](../QUALIFICATION-2026-11-19.md) for the decision,
scope and remaining work. This directory contains source identities, commands,
test infrastructure and raw execution results. No shipping binary is replaced.

- `state-qualification.log`: original 23-test board-state suite.
- `phaser-correctness*`: original 13 phaser tests and four prepared-memory tests.
- `phaser-offline-soak*`: the original numerical-soak template, selected only for
  the phaser, and its four-rate 1,800-second-per-rate run. The template matches the
  original source bytes. Its 7,200 seconds of simulated audio took about 102 real
  seconds; this is not a 30-minute live DAW session.
- `cpu-probe*` and `cpu-source-identity.json`: unchanged existing CPU/response
  probe, environment, source identities and raw measurements. A zero process
  exit is not a pass for the 0.4% CPU target.
- `cpu-wrap-trial*`: isolated optimization measurements. The target still failed
  and this trial was not applied to the candidate.
- `board-source-blobs.json` and `board-build-commands.json`: exact production
  input identities and compiler/linker arguments for the native component runner.
- `component-runner.cpp`, `allocation-counter.cpp`, `phaser-board-soak.cpp` and
  `soak-monitor.py`: supplemental harnesses. Allocation hooks are extracted from
  the candidate's `CircuitTests.cpp`. They observe C++ allocation calls; the
  existing ThreadProbe counter observes explicitly instrumented locks, not every
  possible operating-system allocation or synchronization operation.

For the existing full project runner, the matching invocations are:

```sh
LuthierTests RefPedalState
LuthierTests RefPedalPhase90
LUTHIER_SOAK=1 LuthierTests RefPedalSoak
```

To reproduce the additional phaser-board case, copy `phaser-board-soak.cpp` into
`Source/Tests/PhaserBoardQualificationSoakTests.cpp` in a separate checkout of the
candidate, rebuild the test runner, and run:

```sh
LUTHIER_SOAK=1 LuthierTests PhaserBoardQualificationSoak
```

The additional test retains the original board-soak checks and 100 edits/second
schedule. It selects the real named phaser, adds speed/depth/mix/stereo-phase
parameter changes, and uses blocks of 1, 4, 16, 37, 64, 128, 256, 481, 512, 1024
and 2048 samples. The concurrent runs reported here use the component runner,
actual production board/registry/DSP and patched JUCE, without a GUI, DAW or
audio device. Each long test must finish before its log can establish a pass.

Absolute paths in command manifests identify the actual execution workspace.
They are evidence of what ran; adapt the checkout/build locations when repeating
the tests. Never infer Windows/macOS host compatibility or certificate trust
from these Linux component results.
