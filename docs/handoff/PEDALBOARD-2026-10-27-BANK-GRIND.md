# RC-5 bank and audio grind checkpoint

Local source HEAD `1feb106d2db13b039f1b26b5176b1e5fe36e9330`. The 99-memory
metadata index is implemented and independently reviewed. Pro, Free and strict
ASan/LSan/UBSan each passed 19 tests and 2,331 checks; the additional 62-check
case preserves an unknown `looper.runtime` subtree through a real Ready save and
clear without treating it as resume authority. Exact results and source hashes:
`/workspace/luthier-evidence/rc5-memory-bank-20261027/`. This is metadata only:
asset retention, file persistence, playback selection, wrapper and registration
remain open. The stopped/empty reverse core at `36f16887` separately passed
71 tests and 56,661 checks in each of those three configurations.

The audio-only paced grind at `/workspace/luthier-evidence/paged-reverse-grind-20261027/`
ran once for 35.737 seconds, 3,278,100 frames and 7,348 callbacks across five
rates with forward and reverse wraps. **Exit 1:** 81 full-window readiness
observations were false. Audio/state matched the oracle; callback allocation,
asset-store calls, storage faults and nonfinite samples were zero. The run did
not identify which pages were missing, so a moving-window expectation versus
actual two-second lookahead defect remains under investigation. Preserve this
failure and its original predicate. This is not hardware, DAW or UI acceptance.

CE-2 BBD-on THD remains RED at all five rates. Independent DSP reviews found
insufficient evidence for a justified additional nonlinear transfer and voltage
normalization; no CE-2 calibration patch was applied. CE-2 raw bypass jump remains
RED in 12/60 diagnostic cases. Phase90 and DD-3T CPU gates remain RED; the Dyna
candidate remains private because maximum-sensitivity THD and attack failed.
See `PEDALBOARD-2026-10-27-REVERSE-DSP-GATES.md` for exact measurements.

Latest confirmed private Library checkpoint is version 29 of
`libfile_1047f71682648191b5c2e265aed98eaf`, source `1f7bfeed`, SHA-256
`3ec7474d7e5ce51bbda0dc1b8e8460b01e16008b5c7b2556c1da67ad7b854b42`.
The two separate native packet writes failed at Library helper discovery with a
network error and have no confirmed creation; do not retry or switch routes.
No push, CI, merge, release or deployment was made. Native Windows/macOS/Linux
formats, hardware/DAW acceptance, remaining model gates and RC-5 wrapper/99-memory
integration remain open. Six named models remain unregistered; twelve are retained
for later. Preserve all predecessor bundles and the `22d6b18d` recovery prerequisite.
