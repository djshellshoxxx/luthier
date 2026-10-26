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

The 2026-10-26 corrected-HOME unfiltered suite did **not** run those crash sizes:
`Fuzz.presetLoadAtTrulyExtremeBufferSizes` intentionally excluded them and
recorded the permanent finding in `docs/review/QA_ROBUSTNESS.md`. Its green
safe-zone checks do not clear this issue. The exact suite log is preserved at
`/workspace/luthier-evidence/engine-retirement-source8/full-suite-writable-home.log`.

## Candidate fix and validation status

The candidate prepares cabinet, body, and IR-variant convolutions with an
internal minimum block capacity of 8, while continuing to process the host's
actual 1–4-sample callback sizes. The focused regression loads `Clean
Double-Cut Funk` and `T-Style Country Twang`, processes 20,000 blocks for each
at every host size, checks both IRs are installed and reloaded, verifies finite
nonzero output, destroys the processor, and churns 256 small allocations.

On the current dirty source tree derived from `51df65e9258c96dfdd3ec5047cd15e475b82f5b1`,
the corrected regression passed: 1 test, 2,102 checks, 14.6 seconds. The
13-test fuzz filter containing it also passed: 1,144,712 checks, 180.7 seconds.
This is evidence the candidate survives that runtime workload; it does not
prove heap integrity or establish the byte-level root cause. The pre-fix
failure remains preserved above. The earlier ASan rebuild did not complete,
and no ASan or Valgrind run has verified this candidate.
