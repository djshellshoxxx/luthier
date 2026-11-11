# Contributing to Luthier

Luthier is a physically modelled guitar instrument plugin (JUCE 8.0.10, C++17, CMake).
This page is the short version of the rules the code base already enforces. Longer
statements live in the files named in each section.

Before you start, read the status of the work you plan to touch: `docs/KNOWN_ISSUES.md`
and, for the pedalboard, `docs/PEDALBOARD.md`. Repository text, including this file, is a
description of conventions at commit `ca14934e`; check the code when in doubt.

## Build

- Linux: run `scripts/setup_linux.sh`. It installs the build dependencies, clones JUCE
  into `ThirdParty/JUCE` and configures a Release Ninja tree. Then, for example:
  `ninja -C build LuthierTests Luthier_VST3 Luthier_Standalone`.
- All platforms: `scripts/ci_build.sh` (Linux and macOS) and `scripts/ci_build.ps1`
  (Windows) run the same steps CI runs (`deps`, `configure`, `build`, `test`,
  `validate`, `stage`). Read the header of `scripts/ci_build.sh` for the environment
  variables.
- Targets: `Luthier_VST3`, `Luthier_Standalone` always; the AU target on macOS only;
  `Luthier_CLAP` when `ThirdParty/clap-juce-extensions` is present; `LuthierTests` (option
  `LUTHIER_BUILD_TESTS`, default on); `LuthierRender`, the offline renderer (option
  `LUTHIER_BUILD_CLI`, default on). Source: `CMakeLists.txt`.
- Editions: Pro is the default; the Free edition is selected with `LUTHIER_EDITION=FREE`
  in `scripts/ci_build.sh` (see `cmake/Editions.cmake`).
- `LUTHIER_LTO` (default ON) lengthens links; CI turns it off on ordinary pushes.

How to run the tests is in [docs/TESTING.md](docs/TESTING.md).

## Rules that are not negotiable

These come from `docs/HANDOFF.md` ("Non-negotiable code rules"), `spec/engine.md`
section 0 and `spec/JUCE_CLAUDE_GUIDELINES.md`.

### Host parameters are append-only

Host automation is indexed by parameter position. Never reorder, remove or rename an
existing parameter.

1. Add new parameters at the end of the layout, inside a pair of markers
   `// ==== BEGIN <name> params ====` and `// ==== END <name> params ====`
   (`Source/Parameters.cpp`), with matching IDs in `Source/Parameters.h` and the bridge
   code.
2. Update the expected total in `Source/Tests/IntegrationTests.cpp` (the `Parameters`
   suite): append ` + <n>  // <name>` to the sum.
3. Every automatable parameter needs a visible control. The Combo test
   `everyAutomatableParameterHasAVisibleControl` fails otherwise.
4. The pedalboard's per-slot parameters (`ref_s{nn}_*`) and lane order are append-only too
   (`spec/reference-pedalboard/specs/BOARD_CORE.md`, section on host automation).

### Real-time rules

Code that runs on the audio thread (`processBlock` and everything it calls):

- no heap allocation (`new`, `malloc`, container growth, `juce::String` building), no
  locks, no file or network I/O, no logging, no unbounded loops, no exceptions;
- all buffers are allocated in `prepareToPlay`;
- DSP in `double`, converted to `float` only at the output;
- every recursive stage has a DC blocker and a NaN/Inf guard;
- DSP-affecting parameters are smoothed (20 ms linear unless a spec says otherwise);
- nothing depends on the sample rate or block size beyond `prepareToPlay`: store times in
  seconds or Hz;
- denormals are disabled at the top of `processBlock` (`juce::ScopedNoDenormals`);
- every DSP object has `reset()`;
- nonlinear stages are oversampled.

When these conflict with clever code, the rule wins (`spec/engine.md`, section 0).

Before using a JUCE API you are not sure about, check the JUCE 8.0.10 documentation
(`spec/JUCE_CLAUDE_GUIDELINES.md`).

### No third-party brand or product names

Shipped strings (names, labels, descriptor text, presets) and new identifiers use generic
names. The test `Trademarks.sourceTreeHasNoUnmarkedBrandNames` scans string literals in
`Source/` (not tests, comments, or lines marked `legacy name`). `Tools/trademark_scan.py`
holds the same list. A reference to an inspiration belongs in a spec or a comment, not in
anything a user sees.

### Other rules

- Put new code in new files where possible; keep edits to hub files
  (`LuthierEngine.*`, `PluginProcessor.*`, `Parameters.*`, `PluginEditor.*`, large panels)
  small and local.
- Validate external input where it enters (files, host state, MIDI).
- Do not weaken, skip or delete a test, loosen a threshold, or regenerate golden data to
  make a test pass. A failing gate stays visible until it is fixed or the specification is
  changed on purpose and recorded.
- Docs describe verified behaviour only. Where something has not been checked, write
  "not verified".

## Line endings

The repository mixes CRLF and LF files on purpose. Editors and scripts can rewrite them.
Before every commit run `scripts/fixeol.sh` from the repository root; it restores each
modified tracked file's original line endings. Check `git diff --stat` is the size you
expect.

## Commits

- Small, logical commits. Imperative subject line, in the style
  `type(scope): subject`, for example `fix(looper): ...`, `feat(board): ...`,
  `test(ui): ...`, `docs: ...`, `ci: ...`.
- The body says why, and names the tests you ran.
- Do not hide a failing test in a commit message; report it.

## Adding a test

1. Create or extend a file `Source/Tests/<Name>Tests.cpp`. The CMake glob picks up new
   files on the next configure (`CONFIGURE_DEPENDS` makes ninja re-configure by itself).
2. Include `TestFramework.h` and register with the macro:

   ```cpp
   LUTHIER_TEST (MySuite, whatItChecks)
   {
       CHECK (condition);
       CHECK_MSG (condition, "what went wrong: " + juce::String (value));
       CHECK_NEAR (actual, expected, tolerance);
   }
   ```

3. Suite names are what filters match. Pedalboard suites start with `RefPedal`.
4. A bug fix needs a regression test that fails before the fix and passes after it.
5. Measurements that depend on the machine (CPU, timing) should honour `LUTHIER_PERF`
   (see `Source/Tests/TestFramework.h`) rather than assume a quiet machine.

## Adding a pedalboard model

Background: `spec/reference-pedalboard/ARCHITECTURE.md`, `.../specs/BOARD_CORE.md`
(section 11, descriptors) and `.../specs/GENERIC_PEDALS.md`. The interfaces in
`Source/ReferencePedals/PedalDesc.h`, `IPedal.h`, `PedalBase.h`, `PedalRegistry.h` and
`Registration.h` are marked frozen: change them only through the errata process described
in those headers.

1. Write the descriptor (`PedalDesc` with a `ParamDesc` array): a unique `modelId`, a
   unique `paramPrefix` ending in `_`, parameter ids starting with that prefix, ranges,
   defaults and tapers, the channel and bypass modes, declared CPU units. Use generic
   display names.
2. Implement the model by deriving from `PedalBase` (or the helpers in
   `Source/ReferencePedals/Common/` and `Generic/`). Follow the real-time rules above.
3. Provide a prepared-memory estimate (`PreparedMemory.h`) so the board can admit the
   model before constructing it.
4. Register it with `PedalRegistry::add` in the register function for its package (see
   the list in `Registration.h`). Registering makes a model visible to users, so the
   project's release policy applies: do not register a named model before its recorded
   acceptance gates pass (`docs/coverage/PEDALBOARD-ACCEPTANCE.md`).
5. Add a face (`Source/UI/ReferencePedals/Faces/`) if the model needs more than the
   generic face.
6. Add tests under `Source/Tests/` in a `RefPedal<Model>` suite: descriptor sanity, state
   round trip, bypass and latency, block-size independence and a real-time allocation
   check. Look at the existing `RefPedalGeneric*` and `ReferencePedals_*Tests.cpp` files.
7. Add the model to `docs/EFFECTS_REFERENCE.md`.

## Adding a legacy rack pedal

Legacy rack pedals are `PedalType` values in `Source/DSP/Effects/Pedal.h`. New types are
appended at the end of the enum (before `NumTypes`) so that saved slot indices keep their
meaning. Each pedal declares its parameters as `PedalParam` descriptors.

## Pull requests

Describe what changed and why, which tests you ran (exact command and result), and
anything you did not verify. Do not claim a platform works unless you ran it there.
