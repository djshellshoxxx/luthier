# Cabinet/body IR handoff — 2026-11-16

This addresses the F-C2 discontinuity and latency finding. It is an additional
candidate change, not a claim that Windows/macOS release qualification passed.

## Behavior

`LiveConvolution` builds each complete response and its quality variants on the
calling control thread. Audio retains its old bank until the new bank is ready,
primes the replacement history, then crossfades for 20 ms. Loads do not put the
running cabinet/body onto a fallback or expose a partially installed response.
Failed loads preserve the previous response. A reset adopts the newest prepared
bank immediately for deterministic restart/offline rendering.

A three-slot ownership handoff separates pending, current and fading banks.
Only the control thread allocates, replaces or destroys a bank. Audio returns
retired ownership through atomics. One serialized producer may also hold the
bank it is currently building: memory is bounded, but overlapping replacement
uses additional memory and temporarily renders two banks. Variant diagnostics
include all retained banks; their per-bank quality budgets remain enforced.

Cabinet bypass keeps a delayed dry path and the same reported latency as its
loaded response. Both bypass edges crossfade. Re-enabling primes the reset
partition before raising the wet gain, and settled bypass skips the expensive
wet rendering. Raw-sample IR sources are retained across host block-size or
sample-rate preparation instead of being lost when prepared banks are rebuilt.
UI variant diagnostics are value snapshots rather than references to a mutable
bank. The core callback no longer takes the convolution replacement spinlocks.

## Reproductions and verification

- The original production cabinet/body replacement tests failed 3 of 8 checks:
  cabinet latency changed and both paths stepped by far more than twice their
  steady-state sample delta. The repair also verifies that the opposite-polarity
  replacement really becomes audible, rather than merely keeping the old bank.
- Before the cabinet bypass repair, all three block sizes failed physical-delay
  and reported-latency checks (6 failures). A separate worst-phase 512-sample
  re-enable test then exposed an onset step; priming before the fade fixes it.
- The new prepare lifecycle initially discarded raw IRs. A regression caught
  four wrong impulse onsets across body/cabinet and block sizes 1 and 512.
  Retaining the raw source fixes those reprepare cases.
- The integrated cabinet/body component runner passes 7 tests / 70 checks under
  both GCC 14 ThreadSanitizer and Clang 19 ASan+UBSan with actual JUCE 8.0.10. It covers 1-, 64- and 512-sample
  callbacks, polarity-changing replacements, measured bypass impulse timing,
  both bypass edges, reprepare at 44.1/48/96 kHz, and repeated concurrent publication with quality
  changes and reset.
- The complete native rebuild and an earlier candidate's full-suite run were
  interrupted by the shared container memory limit (exit 137). The final
  standalone ASan build succeeded on retry with one compiler job. Native full
  product/GUI qualification is still pending and is not inferred from this run.

The reproducible target is `scripts/ir_checks`, using `SANITIZE=thread` or
`SANITIZE=address,undefined`. Local ASan runs disable LeakSanitizer because the
traced execution environment cannot run it; CI retains the default. The native
runner additionally enables the actual callback allocation counter. The small
sanitizer harness does not establish allocation-count or shipping-performance
acceptance. Native integration, full suites and platform validators are still
required; no release is qualified solely by these checks.
