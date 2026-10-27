# Original RC5 rhythm helper evidence

2026-10-24. This is a helper checkpoint, not a registered RC5 pedal or feature
acceptance. Contract: spec/reference-pedalboard/specs/RC5_RHYTHM.md. All57
patterns, both variations and seven kits are original authored grids and
sample-free synthesis; no Roland audio or proprietary pattern data is copied.

The isolated focused suite passes8 tests / 20,967 checks. Coverage:114 distinct
nonempty grids, seven distinct audio kits, deterministic partitioning at five
sample rates 44.1–192 kHz and blocks 1/16/37/64/481/2048, full-precision clock,
bar-boundary selection, tempo-phase retention, explicit positive/negative
transport seeks, hostile selectors/nonfinite controls, reset/stop, bounded
centered audio and no counted callback C++new. The counted hook does not
intercept Cmalloc. No callback locks, IO, resize or lazy initialization exist.

ASan+UBSan with leak detection pass the same8 tests / 20,967 checks. The helper,
tests and harness are instrumented; linked prebuilt JUCEcore/audio_basics
objects are not. No full-plugin sanitizer claim follows from this scope.

Independent DSP-worker read-only review found a real phrase transition bug:
a4-to2-bar selection retained bar2/3, outside the new phrase, which could omit
or misplace its fill. Two audio-comparison regressions for boundary and seek
failed before the fix and pass after reducing the bar modulo the new phrase
length on selection. Earlier selector-clamping regression also failed before
its fix. Independent follow-up review caught the reverse two-to-four seek: reducing the
absolute bar modulo the old phrase lost phase. The extra regression failed
before moving selection ahead of the absolute bar calculation, and now passes.
Initial missing-header and subsequent red logs are preserved.

The helper stress driver uses seed 0x725751a7 and 300 simulated seconds per rate,
140,490,000 total frames (25 minutes of simulated audio across five rates;
this is not 25 minutes of wall-clock execution). Each rate covers all six buffer sizes and randomized
pattern/kit/tempo/level/transport controls. After both phrase fixes, all rates passed with zero counted callback C++new
and invalid samples, maximum peak 0.849, and process CPU 0.077–0.353 percent
of audio duration. The five-rate JSONL records frame/block counts and timing. Concurrent native building
was active, so wall time and process CPU are separately logged. The RC5 total
0.8 percent budget includes its future looper; helper timing cannot certify it.

Evidence directory: /workspace/luthier-evidence/rc5-rhythm. Retain exact
commands, compiler flags, harness and JSONL in the next portable checkpoint.
Main application integration, wrapper transport/MIDI clock, memory persistence,
listening/hardware/DAW acceptance and full RC5 spec gates remain pending.
