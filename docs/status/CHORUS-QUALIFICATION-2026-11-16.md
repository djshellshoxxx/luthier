# Chorus model qualification — 2026-11-16

Parent: recorder continuation `19e6102c762af0b61ef2ab5eb2e0d3bf9b9200ec` (PR #103).
This work adapts the parked PR #100 production proposal and qualifies it locally.
It does not publish a release or claim measurement against a physical pedal.

## Model and limits

The BBD's existing symmetric headroom law lacks low-level even-order curvature.
The opt-in clocked transfer adds a quadratic deviation through the existing
oversampled limiter, then AC-couples that deviation at an estimated 15 Hz.
The coefficient defaults to zero; only the CE2's BBD-on path enables it.
CE2W and CE2 BBD-off retain their prior transfer. Reset clears coupling history.

The [Panasonic MN3007 datasheet](https://datasheets.pl/elementy_czynne/IC/MN/MN3007.pdf)
was read in full, including its figures. Printed page 35 (PDF page 2) specifies
0.5% typical and 2.5% maximum THD at 0.78 Vrms, 1 kHz, 40 kHz clock, and -15 V.
For a quadratic sine transfer, HD2 = k*A/2. The proposed coefficient uses the
typical value and scales the level by 9/15 for the modeled 9 V supply. That
scaling is an estimate: the supply curves on printed page 37 show a trend but
do not establish this exact law, nor a minimum distortion guarantee at 9 V.
The 15 Hz coupling and attribution of all typical THD to HD2 are also modeling
assumptions. Preserve ESTIMATED labeling and the unregistered prototype status.

## Local evidence

GCC 14, actual JUCE 8.0.10, production cores, ASan+UBSan:

- Before the change: the 11 original tests ran 405 checks. Five checks failed,
  all in `RefPedalCe2.bbdOnOriginalThdGate`, across its five sample rates.
- After the change: those unchanged 11 tests and four added regression tests
  pass: 15 tests, 439 checks, zero failures. Leak detection was disabled because
  the traced local environment cannot run LeakSanitizer; address and undefined
  behavior checks remained active.
- Added regressions cover disabled/unclocked behavior, reset determinism,
  silent DC output, block partition independence, finite output, and AC coupling.
  Their wet-path distortion range describes this estimated software model;
  the existing pedal-output acceptance thresholds are unchanged.
- A separate 100-case probe compared output hashes before and after the change:
  all 80 unaffected cases match exactly (20 CE2 BBD-off and 60 CE2W cases across
  five rates, stereo settings, noise/stair options, and CE2W modes). The 20
  CE2 BBD-on cases change as intended. The probe is supplementary measurement,
  not an independent hardware validation or a substitute for the native suites.

Measured CE2 BBD-on pedal-output THD at the original -12 dBFS gate:

| Sample rate | THD |
|---|---:|
| 44.1 kHz | 0.802452% |
| 48 kHz | 0.777473% |
| 88.2 kHz | 0.694015% |
| 96 kHz | 0.685728% |
| 192 kHz | 0.666941% |

The high-level headroom/third-harmonic gate also passes unchanged. Sanitized
timings do not establish a release CPU budget. Reproduce the focused tests with
`scripts/chorus_checks/README.md`; the complete native test runner includes the
same regression source through its existing test glob.

## Pending release qualification

Free/Pro quick-check selections now include `RefPedalCe2`, `RefPedalBbdCore`,
`RefPedalChorusMemory`, `RefPedalCe2w`, `RefPedalCe2NodeBypass`, `RefPedalRouting`,
`RefPedalBufferedTimeFx`, and `RefPedalBbdCurvature`. Full Windows/macOS native
tests, plugin validators, release optimization, packaging, and the remaining
processor/audit blockers still require qualification on the final source.
