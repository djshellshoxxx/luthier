# Intermittent-jack timing repair — 2026-11-16

## Failure evidence

Native release qualification exposed a reproducible failure in
`CharacterWiring.theIntermittentJackDropsOutAndRecovers`: the measured dropout
ratio was `0.617`, above the unchanged `< 0.4` acceptance threshold. The test
window began six milliseconds after the character event but did not include the
engine's reported output latency. After the latency-preserving routing repair,
that window sampled the output before the dropout had reached it.

The same investigation found a product timing defect. Intermittent-jack state
was advanced once at the start of each host block, so a nominal 20–100 ms
dropout could be shortened or quantized by the host block size.

## Repair

- Slow character state remains block-rate.
- Intermittent-jack time advances once per rendered sample, making its duration
  independent of host block size without allocating or locking on the audio
  thread.
- The wiring test measures its dropout and recovery windows after the engine's
  reported latency. Its signal-depth and recovery thresholds are unchanged.
- A direct regression proves that a large slow-state block does not consume
  jack duration and that the dropout remains within 20–100 ms.
- Both focused regressions run in quick-check and in the early native release
  gates on Linux, Windows, and macOS.

## Local native evidence

The actual native Pro product runner passed the two intermittent-jack unit
tests and the processor wiring test: 3 tests and 6,011 checks. This focused
result verifies the repaired path only. Complete exact-source Windows and macOS
release qualification remains required before publication.
