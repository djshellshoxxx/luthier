# Narrow release/amp causal analysis — 280e

This is a diagnostic continuation of the unchanged failing35dB whole-output gate. Both original obligations remain: raw-string SUS08>40dB and whole-rig35dB at250ms in the existing30ms RMS window. They are not logically contradictory. No thresholds, source note, time windows, production gain/tuning/damping or fixture code were changed.

## New finding: actual effective drive

The prior settings export records the `amp_gain` parameter0.35, not the final AmpEngine gain. `Source/Parameters.cpp:525` gives Drive macro default0.3; the bridge at1919 applies `amp_gain + macro_drive*0.55`. Thus the actual default effective gain is **0.51500000059604645**. AmpEngine's46dB/unit mapping makes the macro contribute **7.59dB preamp drive**. Master is0.69999998807907104. This routing is explicit existing behavior; it is not a newly found erroneous double application.

A replay using only knob0.35 failed to reproduce the trace (retained `knob-only-results.txt`). Correcting the replay setup to include the macro reproduces the actual retained PStyle amp window RMS:
- Before note-off: measured0.74186919521921313; replay0.74186920314305027.
-250ms after: measured0.007888285390436503; replay0.007888285396029168.
- Attenuation: **39.46689456dB** through the amp, versus **54.30914360dB** at its DI input.
- Maximum sample errors in the two relevant windows:7.21e-8 and9.57e-10. The input/reference capture is float; replay is double. The initial attack has larger differences because this isolated replay begins with settled amp controls, whereas the real host prepares/smooths them. This limitation is retained; no whole-trace identity claim.

The archived AmpEngine/ToneStack/common sources are exact280e and unchanged from b5; no mutable native object files were linked. Only the independent prebuilt JUCE core helper object and exact source files are used. The fresh full280e binary independently retains the34.7dB output failure, as recorded in `/workspace/luthier-evidence/native-280-dsp-qualification`.

## Causal diagnostic interventions, not candidate product changes

All comparisons use the original amp windows. Each intervention exists only in the standalone probe:

| Diagnostic | Amp attenuation | Interpretation |
|---|---:|---|
| Original effective gain and input |39.466895dB|Reproduces measured amp stage|
| Replace amp input with zero after note-off |143.343348dB|Stored amp/filter state alone does not produce the observed tail|
| Reset all amp state at note-off, retain actual input |39.464433dB|Clearing stored state does not improve the original-window attenuation|
| Scale entire DI by0.001 |55.362680dB|Small-signal operation retains the input's decay; strong-signal compression explains lost relative attenuation|
| Disable sag only in the probe |39.809110dB|Sag contributes0.342215dB to the relative attenuation difference; removing intended sag is not an authorized fix|

The amp's sag supply voltage recovers from0.835704 before release to0.863633 at250ms; the model's existing slow envelope is operating, not stuck. Source `AmpEngine::processCore` has static input-dependent tube curves and its envelope-driven power-stage headroom. It receives audio, not note-off events; a note-off should not silently clear cabinet/amp states. Original `Amp::gainSweepIsMonotonicAt1kHz` and `Amp::coldStartHasNoTransient` also pass on the fresh native binary (182checks). These finite results do not certify all amp modeling behavior, but rule out an amp reset/held-state explanation for this measured tail.

The downstream cabinet/DC/lookahead contribution is already independently replayed from measured kernels in3715's preserved release survey. No second full survey or CPU benchmark was performed here.

## Release coefficient contract audit

`StringEngine::release` with neutral sustain shaping sets Released damping at1 immediately; `setDamping` marks coefficients dirty and the next `beginSample` recalculates them before processing the feedback loop. No missed release, stale coefficient, unit-conversion or sample-clock error was found on this path.

At full release the implementation sets `T60<=0.3s`, then:
`g = exp(-ln(1000)*(sr/f0)/(T60*sr)) = exp(-ln(1000)/(f0*T60))`.
Ignoring additional passive losses, this is50dB attenuation in250ms, not60dB. The current coefficient implements that estimate correctly. The earlier all-rate raw survey's minimum52.794dB is consistent with the cap plus filtering; it does not violate the advertised50dB estimate.

The0.3s cap was introduced in316aa3d4 to replace an inadequate0.13-times-open-sustain-only release. Its source/audit rationale is that a lifted fingertip damps a string in0.2–0.4s. The normative SUS08 test fixes the40dB raw threshold and40ms ramp comparison, but does not fix0.3s as a hardware parameter. The original whole-output35dB criterion was introduced in the same historical change and remains an independent obligation.

Consequently this is not evidence that the35dB test is invalid, and not a numerical defect in the current cap equation. A stronger universal release-only curve could satisfy both obligations while leaving held notes alone. A possible bounded engineering candidate is a0.25s cap (a full60dB per normative250ms, within the existing physical interval), preserving the geometric ramp and all other states. That would be an explicit release-target revision with audible tail consequences, not an arithmetic correction or a per-instrument exception. The parent subsequently authorized this release-only candidate. Its implementation and finite qualification are recorded separately in LEGACY-RELEASE-CAP-QUALIFICATION.md; this causal probe itself changes no production code.

Exact source excerpts, standalone probe, retained controls/interventions, output arrays, input/source provenance and original amp-test log are adjacent to this report.
