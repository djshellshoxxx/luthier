# Focused recorder checks

This Linux target compiles the production recorder, MIDI export code, native
regression tests, and JUCE 8.0.10 sources. It needs a C++17 compiler and CMake,
but no GUI or audio-device development packages. Fetch the pinned JUCE source
into `ThirdParty/JUCE` using `scripts/ci_build.sh deps` or:

```sh
git clone --depth 1 --branch 8.0.10 https://github.com/juce-framework/JUCE.git ThirdParty/JUCE
cmake -S scripts/recorder_checks -B build-recorder -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=thread
cmake --build build-recorder --parallel 2
TSAN_OPTIONS=halt_on_error=1 build-recorder/recorder_checks
```

Use a separate build directory and `-DSANITIZE=address,undefined` for memory and
undefined-behavior checks. Sanitizer execution must complete successfully; a
startup failure is not a passing run. Full native JUCE builds also discover the
same test files. This target does not qualify the plugin, GUI, audio-device
callbacks, Windows, macOS, or the full test suite. Optional codecs are disabled;
WAV and MIDI serialization use real JUCE implementations. The colour translation
unit includes JUCE's own implementation because score headers define colours;
there are no recorder, synchronization, FIFO, or file-format mocks.
