## tuner-and-tuning-reference.md

Tuning reference (A4) parameter plus a tuner utility (reference + live mode). Codex-owned lane (no dedicated branch found; `origin/codex/verify-factory-tunings` is adjacent). This checkout has a `concertA` parameter (415-466 Hz, `ParamIDs::concertA`) with a knob on the guitar body panel, but no `tuning_reference_hz` name, tuner popover or live pitch tracker.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| TN-0 (§0) | Ground rules | - | - | - | OWNED |
| TN-1.1 (§1.1) | tuning_reference_hz parameter | `Source/Parameters.cpp:ParamIDs::concertA` | `GuitarBodyComponent::concertA` knob | - | PARTIAL |
| TN-1.2 (§1.2) | Where it applies (TuningEngine, capture) | `PerformanceCapture.cpp` uses `tuning.getConcertA()` | - | - | PARTIAL |
| TN-1.3 (§1.3) | What does not move | - | - | - | OWNED |
| TN-2.1 (§2.1) | Reference-mode tuner (needles at 0, Play tone) | - | - | - | OWNED |
| TN-2.2 (§2.2) | Live mode pitch tracking | - | - | - | OWNED |
| TN-2.3 (§2.3) | Accuracy and latency | - | - | - | OWNED |
| TN-3.1 (§3.1) | Canonical location (popover, headstock, Options mirror) | - | - | - | OWNED |
| TN-3.2 (§3.2) | Behaviour | - | - | - | OWNED |
| TN-3.3 (§3.3) | Empty and error states | - | - | - | OWNED |
| TN-4 (§4) | Serialization, undo, accessibility | concertA is an APVTS param (saved) | - | - | PARTIAL |
| TN-5 (§5) | Interactions with other specs | - | - | - | OWNED |
| TN-T1 (§6 TUNE-01..03) | Reference scaling, intervals, temperament/capo | - | - | - | NO-TEST |
| TN-T2 (§6 TUNE-04..10) | Tuner readout, play, tracking, low confidence, smoothing, zero cost | - | - | - | OWNED |
| TN-T3 (§6 TUNE-11..12) | GUI reachability, accessibility | - | - | - | OWNED |
