# Buffer-size 1–4 heap-corruption finding (2026-10-26)

This handoff preserves the pre-fix evidence before the regression is broadened.

## Reported failure

The robustness work on the integrated pedalboard source recorded a reproducible
heap-corruption failure when `LuthierAudioProcessor::prepareToPlay` receives a
maximum block size of 1–4 samples and a preset changes the compiled-in default
cabinet or body. The recorded reproducer was `Clean Double-Cut Funk` at 8 kHz,
with prepare/load tested in both orders. The process aborted with varying glibc
messages (`corrupted size vs. prev_size`, `munmap_chunk(): invalid pointer`, or
`free(): invalid pointer`); some runs detected damage only after thousands of
render blocks. The report records 5 samples and above as clean, including a
20,000-block run at 5.

The saved source comment in `Source/Tests/QA_Robustness.cpp` identifies the
cabinet/body `juce::dsp::Convolution` setup as a hypothesis based on the
`nextPowerOfTwo(maxBlockSize)` boundary. It explicitly does not claim a
byte-level root cause. That prior session reported an incomplete ASan rebuild
(under 40% built) and no completed ASan/Valgrind diagnosis.

The broad test `Fuzz.presetLoadAtTrulyExtremeBufferSizes` intentionally excludes
those crash sizes and records the finding in `docs/review/QA_ROBUSTNESS.md`.
The separate `Fuzz.convolutionPreparesSafelyForTinyHostBlocks` regression does
exercise host blocks 1–4 and passed 2,102 checks in the 13-test fuzz selection
and again in the corrected-HOME full suite on exact source
`f1efeded706b9dc57da7e941ca856b17eaad5183`. The full-suite log is
`/workspace/luthier-evidence/engine-retirement-source8/full-suite-clean-home-f1ef-final.log`
(SHA-256 `d5a42e592ca30740824c789e083d7a1da6fdb687bc7de8a2a1f9374080dbaeac`).
This is runtime survival evidence, not proof of heap integrity or byte-level
root cause. The additional focused GCC 14.2 ASan/UBSan run on exact source
`f1efeded706b9dc57da7e941ca856b17eaad5183` now passes the same regression:
1 test, 2,102 checks, 152.4 seconds, exit 0, with no sanitizer diagnostic.
Its log SHA-256 is
`553a9c28ccf77ef73caebe6c59c2001cbf1bc02fe973a2507ee1b5283d5a37d0` and the
instrumented test binary SHA-256 is
`198d528b5ad544f9346bf946cd5adb63222c5ad0062e565408b54cba0b2c793d`.
Leak detection and allocator-mismatch detection were disabled for that focused
run; the entire suite was not instrumented. The original failure and exact
allocator overwrite/root cause remain unproven, and Valgrind remains
unavailable.

The subsequent bounded qualification review ran the identical binary/test
with `detect_leaks=1` and `alloc_dealloc_mismatch=1`, no LSan suppressions,
and halt/abort on sanitizer errors. That stricter run passed 1 test / 2,102
checks / 157.1 seconds / exit 0, with no sanitizer diagnostic at execution or
process shutdown. Log SHA-256:
`a4f72a279dd7a52ddc91e928719f07f9f29f80a7db881b5a9ca39fe125073a16`.
The earlier suppressions were not required for this workload. This still
does not establish the original byte-level overwrite cause, all tiny-buffer
sample-rate combinations, or full-suite sanitizer acceptance.

The subsequent test-only extension now covers all 21 cells of
`{8,44.1,192 kHz} x {1,2,3,4,5,7,8 samples}` under strict ASan/LSan/UBSan.
The original 20,000 blocks/preset remain unchanged; the 17 added cells use
2,000 blocks/preset. Result: 2 tests / 11,029 checks / 407.5 seconds / exit 0,
with no sanitizer diagnostic. Source/test/binary identity and limits are in
`PEDALBOARD-2026-10-26-TINY-BOUNDARY-FREE.md`; log SHA-256
`61a270294fafdc81c60ebdb00b46843a6e64063f849a3177b515d2ed7e737349`.

## Candidate fix and validation status

The candidate prepares cabinet, body, and IR-variant convolutions with an
internal minimum block capacity of 8, while continuing to process the host's
actual 1–4-sample callback sizes. The focused regression loads `Clean
Double-Cut Funk` and `T-Style Country Twang`, processes 20,000 blocks for each
at every host size, checks both IRs are installed and reloaded, verifies finite
nonzero output, destroys the processor, and churns 256 small allocations.

At the earlier candidate checkpoint derived from `51df65e9258c96dfdd3ec5047cd15e475b82f5b1`,
the corrected regression passed: 1 test, 2,102 checks, 14.6 seconds. The
13-test fuzz filter containing it also passed: 1,144,712 checks, 180.7 seconds.
This is evidence the candidate survives that runtime workload; it does not
prove heap integrity or establish the byte-level root cause. The pre-fix
failure remains preserved above. At that checkpoint the ASan rebuild had not
completed. The strict sanitizer results above now supersede that instrumentation
gap for the covered matrix; Valgrind remains unavailable and the original
byte-level root cause remains unproven.
