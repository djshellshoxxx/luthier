# Testing

How to run the Luthier checks. Facts here were read from the source at commit `ca14934e`.
Results of any particular run are not claimed here; for the state of the tests at this
commit see [KNOWN_ISSUES.md](KNOWN_ISSUES.md), "Platform qualification".

## The unit-test binary

All unit tests are in one console program, `LuthierTests` (target `LuthierTests`, option
`LUTHIER_BUILD_TESTS`). It has its own tiny framework (`Source/Tests/TestFramework.h`);
tests are registered with `LUTHIER_TEST (Suite, name)` and the program exits non-zero if
anything fails.

Build and run (Linux, from the repository root, after `scripts/setup_linux.sh`):

```
ninja -C build LuthierTests
xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests
```

The tests open real editor windows, so a headless Linux machine needs `xvfb-run`.

### Filters, listing and skipping

Arguments are handled in `Source/Tests/TestMain.cpp` and `TestSelection.h`:

| Argument | Meaning |
|---|---|
| `Text` (any number of them) | Run tests whose suite name or test name contains `Text`, case-insensitive. A test runs if it matches any filter. |
| `--list` or `-l` | List the selected tests as `Suite :: name` and print the count. Nothing runs. |
| `--skip=Suite` | Exclude a suite by its exact name, case-insensitive. Can be repeated. |

Examples:

```
LuthierTests refpedal                 # every pedalboard suite
LuthierTests --list refpedal          # what that would run
LuthierTests Tuning --skip=Combo      # tuning tests, not the long Combo suite
LuthierTests panicStopsEveryPlayerAndLeavesTheSettings
```

CTest wraps some of this (`enable_testing` in `CMakeLists.txt`): `LuthierUnitTests` (the
whole binary), `RefPedalSuite`, `RefPedalLegacy`, `PackagingPaths`, `InstallCycleLinux`,
`UninstallMacOsLogic`, `PerfCompareSelfTest` and, when the renderer is built,
`LuthierRenderCli`.

The Combo suite is long; filter while iterating (`docs/HANDOFF.md`).

### Environment variables

| Variable | Effect |
|---|---|
| `LUTHIER_PERF=1` | Runs the machine-relative performance tests (CPU units, scaling curves, the long memory session); without it they skip. Also switches off the extra latency headroom the tests give shared macOS CI runners. Use on a quiet machine. (`TestFramework.h`) |
| `LUTHIER_SOAK` (any value) | The `RefPedalSoak` tests run for 1800 s instead of 30 s. (`Source/Tests/RefPedalSoakTests.cpp`) |
| `LUTHIER_SOAK_SECONDS=<n>` | Overrides the soak duration. (same file) |

### Timing tests

Tests that measure time or CPU can fail on a loaded or slow machine. Re-run once on a
quiet machine before treating such a failure as a regression, and report both results.
Never loosen a threshold to make a test pass.

### Sanitizer builds

`LuthierTests` can be built with sanitizers (not on MSVC):

```
cmake -S . -B build-asan -G Ninja -DLUTHIER_SANITIZE=address,undefined
cmake -S . -B build-tsan -G Ninja -DLUTHIER_SANITIZE=thread
```

`thread` cannot be combined with `address`. The option applies to `LuthierTests` only.
The test harness overrides the global allocation functions, and a whole-target thread
sanitizer build has been recorded as unable to link in the project's handoff notes
(`docs/handoff/PEDALBOARD-2026-10-28-CONTINUATION.md`); that has not been re-checked
here.

## Plugin validators

- **pluginval** on the VST3: `scripts/pluginval.sh [path/to/Luthier.vst3]` downloads
  pluginval (default `v1.0.3` in that script), runs it headless under xvfb at
  strictness 10, fails on any failure or warning, and keeps the log at
  `build/pluginval.log`.
- **CI scripts**: `scripts/ci_build.sh validate` runs pluginval on the VST3 (and the AU on
  macOS) and clap-validator on the CLAP. Strictness defaults to 5
  (`PLUGINVAL_STRICTNESS`); pluginval defaults to `v1.0.4` and clap-validator to `0.3.2`
  there. `SKIP_VALIDATORS=1` skips the step. On Windows use `scripts/ci_build.ps1 -Step
  validate`. `validate-diagnostic` is a VST3 and CLAP only variant.
- The CI workflow in `.github/workflows/build.yml` runs the validators on every push and
  pull request at strictness 5; manual dispatch can raise it to 10.

## Offline renderer

`LuthierRender` is a console program for rendering MIDI to audio and for notation
conversion. Its flags are in [../spec/cli-tools.md](../spec/cli-tools.md), section 7. The
`LuthierRenderCli` CTest (`scripts/render_cli_smoke.cmake`) exercises it end to end.

## Scripts with their own tests

The Python scripts under `scripts/` that have `test_*.py` files next to them are run
directly with `python3 scripts/test_<name>.py` (for example
`scripts/test_prepare_pro_beta_assets.py`). `scripts/perf_compare.py --selftest` is
covered by CTest.

## Benchmarks

There is no separate benchmark harness. CPU measurements are tests that run only with
`LUTHIER_PERF=1`, and an optimisation log is kept in `docs/perf/OPTIMISATION_LOG.md`.
`scripts/perf_compare.py` compares performance logs.
