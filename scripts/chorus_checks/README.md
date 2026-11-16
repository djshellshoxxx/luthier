# Focused chorus checks

Build the real BBD, CE2, and CE2W production sources with JUCE 8.0.10 and the
native regression tests, without Linux GUI or audio-device dependencies:

```sh
git clone --depth 1 --branch 8.0.10 https://github.com/juce-framework/JUCE.git ThirdParty/JUCE
cmake -S scripts/chorus_checks -B build-chorus -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSANITIZE=address,undefined
cmake --build build-chorus --parallel 2
build-chorus/chorus_checks
```

The target covers 15 tests in `RefPedalCe2`, `RefPedalBbdCore`,
`RefPedalBufferedTimeFx`, and `RefPedalBbdCurvature`. It includes the existing
distortion acceptance tests without changing their thresholds. It does not
qualify GUI integration, processor allocation behavior, native plugins, or
hardware fidelity. Free and Pro quick-check jobs also run the broader chorus,
board-routing, bypass, and memory suites in the full native test executable.

Sanitizer startup failure is not a passing result. Local traced environments
that cannot run LeakSanitizer may use `ASAN_OPTIONS=detect_leaks=0`; record that
limitation separately from address/undefined-behavior results.
