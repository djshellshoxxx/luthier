# QA: pitch-tracker finite-output boundary

Baseline: `4e191393deff6f67aa9edda01b63843c6a4963c1` on
`claude/luthier-cloud-session-5lzlix`.

## Scope

This bounded review follows the pitch-tracker transition gap recorded in PR #4.
It inspected:

- `Source/DSP/Common/PitchTracker.{h,cpp}`
- `Source/Tune/TuneHumCapture.{h,cpp}`
- `Source/Tests/HumCaptureTests.cpp`
- test discovery in `CMakeLists.txt`

It does not overlap the active keyboard-shortcut audit, completeness ledger,
Easy amp layout, packaging work, or the real-time-safety findings in PR #6.

## Finding

**P2 robustness: a non-finite input frame returns a non-finite RMS.**

In `PitchTracker::analyse()`, the frame energy becomes non-finite when any
sample is NaN or infinity. The guard at `PitchTracker.cpp:29-30` returns the
partially populated estimate rather than a clean default:

```cpp
e.rms = std::sqrt (energy / frameSize);

if (! std::isfinite (e.rms) || e.rms < 1.0e-6)
    return e;
```

Frequency and confidence remain zero, but `e.rms` remains NaN or infinity.
`TuneHumCapture::process()` stores that value in every frame, and
`TuneHumCapture::segment()` feeds it into the take-wide loudness and gate
calculation. The result is not guaranteed to poison the final note list because
JUCE's max implementation and later comparisons may reject it, but the shared
tracker's public estimate violates the project's finite-output/NaN-guard rule.

## Smallest safe remedy

First add a regression case to `Source/Tests/HumCaptureTests.cpp` that prepares
a tracker, supplies one NaN in an otherwise silent frame, and requires all three
estimate fields to be finite and zero. Repeat with positive infinity. Confirm
that this test fails on the baseline specifically because `rms` is non-finite.

Then change the non-finite branch to `return {};` while retaining `return e;`
for ordinary sub-threshold finite silence. No other DSP or capture behavior
needs to change.

Suggested focused test name:
`HumCapture.pitchTrackerSanitisesNonFiniteFrames`.

## Verification status

**NOT RUN.** This workspace has repository access through the GitHub connector
but no authenticated local checkout or configured JUCE/CMake build tree.
Consequently no test or production change is included: the required test-first
red/green cycle could not be executed.

Linux verification gate:

```sh
ninja -C build -j$(nproc) LuthierTests
xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests HumCapture
```

The branch is documentation-only and remains draft for the coordinator.
