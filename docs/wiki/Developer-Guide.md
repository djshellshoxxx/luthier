# Developer Guide

## Prerequisites

CMake 3.22+, a C++17 compiler (MSVC 2022, Xcode 14, GCC 11, Clang 14), and JUCE 8.0.10 cloned into
`ThirdParty/JUCE`. Python 3.9+ is needed only to regenerate icons and impulse responses.

## Build

```bash
git clone --depth 1 --branch 8.0.10 https://github.com/juce-framework/JUCE.git ThirdParty/JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2   # use 2 on low-RAM machines
```

Options defined in `CMakeLists.txt`:

| Option | Default | Purpose |
|---|---|---|
| `LUTHIER_EDITION` | `PAID` | `PAID` (Pro) or `FREE`; set in `cmake/Editions.cmake` |
| `LUTHIER_BUILD_TESTS` | ON | build `LuthierTests` |
| `LUTHIER_BUILD_CLI` | ON | build `LuthierRender` |
| `LUTHIER_LTO` | ON | link-time optimisation for plugin targets (CI turns it off on ordinary pushes) |
| `LUTHIER_ENABLE_SENTRY` | OFF | opt-in sentry-native crash reporting; DSN from `SENTRY_DSN` env |

## Test

`LuthierTests` takes an optional name fragment or suite name; `--list` prints the suites.
Run under `xvfb-run -a` on headless Linux. `ctest -R LuthierRenderCli` runs the renderer smoke test.
Plugin validation uses pluginval (`scripts/pluginval.sh`) and clap-validator.

## Conventions that matter

These come from `spec/README.md` ("Rules the DSP obeys"):

1. Internal DSP is `double`.
2. All buffers are allocated in `prepareToPlay`; `processBlock` never allocates, locks or touches files.
3. Recursive stages have DC blockers and NaN guards.
4. Continuous parameters are smoothed; discrete switches crossfade over 5 ms.
5. Sample-rate-dependent values are recomputed in `prepareToPlay`.
6. Times are stored in seconds and Hz, not samples, so presets port across sample rates.
7. `ScopedNoDenormals` at the top of `processBlock`.
8. Every module has `reset()` and is testable in isolation.
9. Nonlinear stages are 4x oversampled by default.

Process: specs first (`spec/`), record judgement calls in `spec/DECISIONS.md`.

## Where to start reading

Start at `Source/PluginProcessor.cpp` and `Source/LuthierEngine.*`, then follow the signal chain
described in the Architecture page. See the Codebase Map for the folder list.
