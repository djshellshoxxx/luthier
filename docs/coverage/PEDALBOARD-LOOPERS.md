# Pedalboard loopers coverage (WP15/WP16)

Transferred baseline: a3ceccaa (bounded layered core) and 30eeb3f7 (bounded reset
queue drain), imported into the state worker as 47a8513d and 6a49a0e8.

## Unlimited overdub core increment

The core now separates a speculative scratch layer from the configured 1..8
retained undo layers. Committing beyond that count retires the oldest accepted
layer; each audio frame folds retired samples into its base in chronological
order before reuse. A preallocated epoch pool tracks accepted/retired/discarded
samples by reference count. This matters for partial/canceled overdubs: canceled
scratch samples never become base audio, and an aborted overdub at the undo limit
does not remove the previous undo layer.

Every buffer and epoch record is included in `memoryBytes()` and the supplied
byte budget. Zero-byte preparation fails without retained storage. The tradeoff
is more bookkeeping RAM and a shorter RAM recording capacity for the same budget.
Gestures and clear/reset have cost bounded by the fixed layer/queue counts,
independent of loop duration. Per-frame folding scans at most nine storage layers;
there is no audio callback allocation or full-loop copy.

Focused verification: **6 cases, 23,050 checks, 0 failures**, in normal and ASan
runs. Existing timestamp/undo/budget tests remain green. Added 300 partial accepted
or canceled overdubs, 50 cancel-at-capacity/undo cycles, 1,000 zero-frame canceled
gestures, stale-audio reset, zero-byte budget, and an allocation-counter test with
100 overdub/commit/cancel/undo/redo cycles. The allocation counter reports zero.
`-Wall -Wextra -Wpedantic` syntax checking is clean.

Reproduce and inspect evidence:

```sh
bash /workspace/luthier-evidence/loopers/run.sh
bash /workspace/luthier-evidence/loopers/run-asan.sh
```

The ASan harness instruments core and tests; reused JUCE core is not instrumented.
This is core acceptance only. Named wrappers/registration, 11/99 memories,
concurrent audio snapshots, worker spill/persistence, reverse/half speed, rhythm,
MIDI clock master, and full crossfade seam modeling remain subsequent increments.
The existing seam algorithm attenuates loop edges rather than overlapping two
unmodified regions; it must not be represented as a completed full crossfade.
The original PhraseLooper prototype remains intact until integration is verified.
