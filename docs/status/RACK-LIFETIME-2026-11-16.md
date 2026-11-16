# Rack pedal lifetime continuation — 2026-11-16

Base: PR #101 head `7560468bdb080b50bccfde0c80d241c29f659802`.
This change implements the retirement portion of the #98 handoff. It is not
whole-processor thread-safety or release qualification.

## Change and lifetime argument

Each slot publishes an atomic live pointer separately from its unique owner.
Previously unlocked latency, tempo, expression, quality, bypass and mix paths
enter a reader scope before loading that pointer. Their last use precedes the
scope's decrement. These paths add no locks, allocation or reclamation.

Writers retain their existing swap lock. They unpublish outgoing pointers and
park ownership in `retired`. A non-audio caller reclaims only after observing
zero readers under the writer lock. Reader entry/exit, pointer publication and
zero observation use sequentially consistent atomics. Therefore a reader that
could have loaded an outgoing pointer must have exited before its destruction;
a reader entering after the zero observation can only load a currently published
pointer. The existing locked processing paths are excluded by the writer lock.

Slot metadata and shared control defaults are atomic. Reordering transfers
ownership and republishes pointers without freeing pedals. The processor timer
services both racks so retirement does not depend on another preset change.
As before, destroying/preparing the chain requires the host lifecycle contract;
raw `getPedal` pointers remain message-thread-only between structural edits.
Concurrent mutation of a pedal's internal DSP state is outside this lifetime fix.
A continuous population of readers can defer reclamation; this is not a bounded
retirement-latency guarantee.

## Tests and scope

Four native regressions pause inside actual virtual latency/quality reads and
verify replacement/clear defer destruction, reader exit never destroys, message
reclamation destroys exactly once, and reordering preserves live publication.
They are selected in quick-check as `EffectsChainLifetime` and discovered by the
full native runner.

`python3 scripts/check_effects_chain_lifetime.py` compiles the unchanged production
EffectsChain sources and those same test cases against minimal boundary fixtures,
without requiring JUCE or system GUI headers. It is an isolated lifetime proof,
not DSP/JUCE/host validation. It is also run by quick-check with ASan/UBSan.

Local evidence:

- GCC 14, C++17, ASan+UBSan: 4 cases, 30 checks, zero failures.
- LeakSanitizer cannot inspect processes in this environment (ptrace limitation).
  The successful local invocation used `ASAN_OPTIONS=detect_leaks=0`; ASan memory
  error and UBSan checks remained enabled. CI uses the default sanitizer options.
- Controlled mutation `--unsafe-reclaim`: deliberately bypasses the zero-reader
  predicate. Assertions fail, then ASan detects heap-use-after-free in the paused
  latency reader (nonzero exit). This is mutation evidence, not an execution of
  the complete historical baseline.
- Native JUCE build, full-processor sanitizer/stress, and Windows/macOS validation
  of this new change are pending. Do not close #98 until these gates are reviewed.

## Earlier PR #101 native evidence

Run 43843489246: Windows and Linux Pro compiled, passed plugin validators, and
passed the new recorder, backend release and 10,000-operation UI lifecycle cases.
Linux Pro: 1/2929 tests failed; Windows Pro: 1/2928 tests failed. Both failures
are the pre-existing `RefPedalCe2.bbdOnOriginalThdGate`. Their Free jobs also
compiled and passed validators, but full suite failure details have not yet been
examined here. macOS Pro/Free compiled and were still running their unit tests.
Quick-check run 43843489169 passed for both editions.

The chorus gate, recorder reset concurrency, rhythm MIDI continuity, raw UI
pointer/writer serialization, other audit findings and real-host checks remain.
No release or tag is created by this work.
