# Universal released-string cap: original and revised evidence

2026-10-25. Candidate production delta: only `StringEngine::updateLoopCoefficients`
changes the fully Released T60 cap from **0.3 s to 0.25 s**, with a comment recording
both targets. No parameters, note selection, tuning, gain, body routing, windows,
acceptance assertions, or reference-pedal registrations change. Independent review
supports integration; the coordinator must still verify the combined build and
remaining platforms. This is not a claim that every release gate is closed.

## Engineering choice, not an arithmetic or hardware claim

The original cap from `316aa3d4` implements its estimate correctly:
`loopGain = exp(-ln(1000)/(f0*T60))`. A 0.3 s cap supplies a nominal 50 dB loss in
250 ms before other loop losses. It was intended to replace the inadequate
0.13-times-open-sustain-only release and meet both raw-string SUS-08 and the whole-rig
release gate. Its recorded fingertip-damping interval was 0.2–0.4 s; 0.3 s is not a
fixed component value specified by SUS-08.

The parent authorized a stronger universal 0.25 s release target: one full nominal
60 dB loop decay in that 250 ms interval, within the existing physical estimate.
This deliberately changes released tails; it is not a claim that their sound is
unchanged. The geometric interpolation for partial damping/release ramps is retained.
Open/held notes, other damping modes and pitch calculations are unchanged. There
are no instrument-, note-, sample-rate- or threshold-specific exceptions.

The two original requirements remain **raw >40 dB** and **whole output ≥35 dB** at
250 ms, using the original 30 ms windows and idle-floor conditions. They are
compatible obligations; neither has been reclassified or relaxed. No original
assertion was edited. The Linux test uses its existing −35 dB branch; this evidence
makes no claim about the existing different macOS branch.

## Why the old whole-output margin failed

The earlier raw-to-output survey and the fresh combined `280e182a` native binary
both retain the original P-Style Bass note 42 failure. Raw damping passes, but
compression and the cabinet reduce the relative attenuation at the output.

A narrow replay found that the `amp_gain` knob is 0.35 while the default Drive macro
is 0.3. The existing bridge adds `0.55 * macro_drive`, so the actual amp gain is
0.515000000596, adding 7.59 dB preamp drive. Replaying the captured DI through the
unchanged amp reproduces the original amp-window RMS to within 8e-9. Feeding zero
after note-off leaves 143.34 dB attenuation by the original window; resetting the
amp at note-off does not improve the measured tail. Thus a stuck amp state is not
the cause. The retained small string signal passing through compression explains
it. No amp or macro change was made. The source/measurement reasoning is retained
in [amp-cause.md](pedalboard-dsp-evidence/release-cap-025/amp-cause.md).

## Pinned original-versus-candidate results

Exact source base: `280e182a8f033457f3d84384b9917a024b1d1ed9`.
Both private executables use the same 819 independently copied and hash-verified
native build6 link inputs; only the candidate StringEngine translation unit is
replaced. Headers are archived from that exact source. No mutable root objects
were linked. Full resource provenance is retained: all 1,253 original resources
and all 70 active factory presets match the qualified bank. The private copy also
contains 66 additional loader-generated Backup files, all ignored by the factory
scanner; no qualified resource was changed or removed.

All 25 original instrument choices were exercised at 44.1, 48, 88.2, 96 and 192 kHz,
with the original note selection, event times and acceptance formula.

| P-Style Bass rate | Original output drop | Candidate output drop | Candidate raw drop |
|---:|---:|---:|---:|
|44.1 kHz|34.4524 dB — fail|42.2985 dB|63.1391 dB|
|48 kHz|34.7390 dB — fail|43.0742 dB|63.1663 dB|
|88.2 kHz|36.4409 dB|44.8429 dB|63.0737 dB|
|96 kHz|36.5400 dB|45.0253 dB|63.1080 dB|
|192 kHz|36.2083 dB|44.5576 dB|63.0429 dB|

Across all 125 candidate cases: **zero original gate failures**, minimum output
42.2985 dB, minimum raw 59.9247 dB. All captured streams remain finite; event/tap
checks pass. The 60 dB target is the loop-coefficient estimate, not a replacement
for the original 40 dB raw-window assertion.

Unchanged native tests also pass:

| Invocation | Tests | Checks |
|---|---:|---:|
|Original Combo release and pedal release, sostenuto, legato, stagger, ramp, sag, ring|9|74|
|Full SustainDecay plus original four-combination ON03 normalization|16|71|
|StringEngine pitch/tone/bend/extreme/repluck tests, E-Bow held release, freeze release|12|39|

These are per-invocation counts with some overlap, not a unique-test total. The
normalization table, four combinations and 4 LU comparison remain unchanged.
No new CPU budget or hardware-faithfulness claim follows from these checks.

## Held-note and pedal identity

Pinned generation3 verifies all 125 full-host prefixes before note-off are
bit-identical across raw, amp, mic and final output: **23,415,000 float bit patterns**,
including signed zero. Direct StringEngine probes cover 120 combinations of five
rates, two strings, three frets and four release/hold scenarios: all pre-release
SHA-256 traces match, and all 60 no-release/letRing full traces match.

Pinned generation4 adds real CC64 sustain and CC66 sostenuto: 30 cases across five
rates and three instrument types. All four stream prefixes before pedal-up match.
The played string is Open while held and Released after pedal-up; both baseline
and candidate pass all 150 checks. This supplements, rather than substitutes for,
the original pedal-release regression.

The final accepted probe components are the generation3 original-release and
held/letRing tests (1,000 + 280 checks), plus generation4 pedal test (150 checks).
`final-comparison.json` records zero hash drift for the frozen inputs.

## Failed diagnostic evidence retained

The first, unpinned full sweep had one pre-release mismatch: Resonator at 88.2 kHz.
Two subsequent unchanged-baseline runs and a candidate repeat matched every
pre-release sample and the same serialized state. The final pinned full sweep
matches all 125 prefixes. The original outlier remains recorded as a baseline/setup
nondeterminism observation; this change does not claim to fix or universally rule
out that separate behavior.

Generation3's newly added pedal diagnostic made an incorrect assumption: its first
StringActivity Off would occur at pedal-up. Actual `applyNoteOff` publishes the
physical key-up before handling letRing, and clears the sounding-note identity.
That diagnostic assertion failed in all 30 cases in **both** versions. Generation4
corrects only that added diagnostic to check key-up timing plus the exposed held
and released damping states. Both runs then pass; all held-prefix hashes already
matched in the failed run. The complete generation3 RED logs are retained. Original
product acceptance assertions were never changed.

Generation1 did not have a pre-run binary hash; later hashes are not attributed to
its output. Generation3/4 sources, binaries, commands and manifests were frozen
before their runs. The committed evidence includes those manifests and exact probe
sources. Full private artifacts remain under
`/workspace/luthier-evidence/release-cap-025-280/`.

## Independent review and integration boundary

The state worker independently checked the one-literal production change,
per-loop T60 equation, the pinned 125-case prefixes and minima, all 30 pedal-prefix
hash sets and corrected state assertions, and resource/manifests. Its
[review](pedalboard-dsp-evidence/release-cap-025/independent-review.md) found no blocking
production-code defect and supports coordinator integration and fresh platform
qualification. It did not certify macOS, Windows, or unconditional host determinism.

The source change has no new state, callback allocation, locks, processing loops or
API surface. Two included stereo float WAVs preserve the 48 kHz P-Style **raw/final**
channels at their actual captured gain, without peak normalization. Other captured
audio remains in the private evidence directory with committed SHA-256 manifests.
