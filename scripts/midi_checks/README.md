# Focused MIDI continuity checks

Build the production MIDI router and rhythm writer with actual JUCE 8.0.10:

```sh
git clone --depth 1 --branch 8.0.10 https://github.com/juce-framework/JUCE.git ThirdParty/JUCE
cmake -S scripts/midi_checks -B build-midi -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=address,undefined
cmake --build build-midi --parallel 1
build-midi/midi_checks
```

This Linux target needs C++17 and CMake, without GUI/audio-device development
packages. It checks exact event times, note pairing, stop/source/global disable,
fixed queue overflow, small destination buffers, release retry, and deferred
engine-bound input across a missed callback. It does not
replace the full processor, plugin wrappers, or native platform tests. The
separate `RhythmMidiOut` suite exercises the processor at block sizes 1, 37,
512, and 2048 through the full native test executable.

As with the other focused targets, a sanitizer startup failure is not a pass.
If LeakSanitizer cannot run in a traced local environment, record any use of
`ASAN_OPTIONS=detect_leaks=0` separately from address/undefined-behavior results.
