# FIX-CROSS: cross-feature failures on the integration branch

Branch: `claude/luthier-fix-cross`, from `claude/luthier-cloud-session-5lzlix`.
The failures below appeared only where independently built features met
(FEAT-CPU, FEAT-STRINGS, FEAT-JAM, FEAT-NORMALIZE, the realism and
oversampler merges). Each is fixed at its root cause. Two tests were
changed; the reason is recorded against each.

| # | Test(s) | Root cause | Fix | Test changed? |
|---|---|---|---|---|
| 1 | `CpuQualityUi::CQ22_everyTimerDrivenUiClassIsRegisteredOrAllowListed`, `CQ22_builtEditorsRegisterEveryTableClassThatExists` | `StringAnimator` (FEAT-STRINGS) and `JamPanel` (FEAT-JAM) were written before `AnimationPolicy` existed and ran bare timers. `StringAnimator` could not simply register, because its owner (`GuitarBodyComponent`, `FretboardComponent`) already held a registration and the registry was a one-per-component map. | The registry is now a `std::multimap`: a component may hold several registrations, `notePaint` counts for all of them, and a registration removes only itself. `StringAnimator` holds `Registration { owner, Decorative, "StringAnimator", poll }` and runs its 30 Hz fallback clock through it. `JamPanel` holds a `LiveReadout` registration (its 30 Hz drain drives the lane playhead and status: 10 Hz, stepped, at Low). The full run then listed four more classes that fail on the integration branch too. `JamPill`, `NormalizationBadge` and `NormalizationOptionsGroup` show live state or values and are `LiveReadout`. `NormalizationCaption` only shows or hides with the switch, so it is on the poll-only allow-list. | No |
| 2 | `AnimatedStrings::cpuQualityMotionPolicy` | FEAT-STRINGS gated the strings on `StringMotionPolicy`, a stand-in adapter that only knew Reduced motion. FEAT-CPU moved `GuitarBodyComponent` onto `AnimationPolicy::mayAnimate`. The two disagreed: the test's override reached the animator but not the illustration's overlay, and CPU quality never reached the strings. | `StringAnimator` reads `AnimationPolicy::get().getStringsStyle()` (animated-strings 2.6): `Off` (CPU Low, Reduced motion, relief ≥ 2) shows the static overlay; `LowStyle` (Medium, relief 1) forces the Low style. `StringMotionPolicy` is kept only as a read-only bridge onto `AnimationPolicy`. Its test override now feeds `AnimationPolicy` a source (Limited = Medium, Off = Low), so every consumer sees the same state. | No |
| 3 | `AnimatedStrings::AS25_theOptionsRows` | Two layout rules collided in `AppearancePage`. animated-strings 5 puts VISUAL AIDS directly under the tooltips / Reduced motion row. cpu-quality-modes 5 adds a muted "Animations are off while CPU quality is Low" line under Reduced motion, and its 16 px row was reserved even while the line was hidden. | The note belongs to the row above and takes space only while it shows. `refresh()` re-lays out when its visibility changes. Both rules now hold: VISUAL AIDS is directly under that row, with the note between them only at Low. | No |
| 4 | `CpuQuality::CQ12_everyFactoryPresetAtEveryLevel` (Fingerstyle Folk −0.99 LU at Low) | Probing each Low setting on its own put the whole drop on the dispersion cap. Single notes lose ≤ 0.2 dB, and only re-plucked ringing strings in the humanised phrase differed. `StringEngine::excite` latched the new note's capped dispersion coefficient immediately, even when the pluck was deferred behind the 5 ms voice steal. The old note's partials were re-tuned while it faded, so the remnant the new pluck sums with came out at a different phase than at High. | `latchDispersion()` runs when the pluck actually triggers: at once for a free string, after the steal for a ringing one. Fingerstyle Folk Low − High: −0.99 → +0.01 LU. J-Style Fingerstyle and Nylon Classical are unchanged, within ±0.07. | **Yes**, `CQ10_aRingingNoteKeepsItsStagesUntilReExcited`, structurally: it checked the new stage count one sample after re-exciting a ringing string, which is a voice steal whose pluck comes 5 ms later. It now checks after 10 ms. The rule it tests is unchanged: stages hold mid-note and change at the next pluck. |
| 5 | `Accessibility::shortcutsRebindAndRefuseClashes`, `noTwoShortcutsShareADefaultKey` | `toggleNormalization`, `toggleStringAnimation` and `cycleCpuQuality` are unbound by default, as output-normalization 9, animated-strings 8 and cpu-quality-modes 5 each require. The tests compared `KeyPress()` with `KeyPress()`, which JUCE reports as equal, so three unbound actions looked like one shared key. | None in the product: unbound is right, and `AccessibilitySettings::rebind` already treats unbound as never clashing. | **Yes**: structurally wrong. An unbound action has no key to share. Both tests now skip pairs whose key is not valid, and still catch any two real keys that collide. |
| 6 | `Reflow::noControlHangsOutsideItsParentAtAnyWidthOrScale` (Easy 940 px: JamPill, LuthierChoice outside a 0-wide parent) | `EasyPanel` gave the JAM group `min(preferred, width − 290)`. That is 0 at 940 px, and `JamStripGroup` laid its children out centred on a 0-wide box. jam-mode 8.2 says the band's style, level and state are never hidden. | `JamStripGroup::minimumWidth` (pill 88 + style 56 + dots 76 + knob 40 + gaps). The rhythm strip gives way in order: the hint label, the readout (to 70), the genre box (to 110), then the JAM group down to its minimum. The group is hidden only if even that does not fit, which is narrower than the window's minimum. The style box's width is clamped to 56..118. | No |
| 7a | `Feedback::eachStringHearsItsOwnNote` (octave bias 1: 7.4 cents off; 18.6 before the oversampler fix) | See "Feedback" below: the loop's phase at the note was set by the block size and the processing latency. | Whole-period padding of the loop delay. Above the note, the peak is placed on the string's own partial 2^bias (`StringEngine::getPartialFrequency`), not on the plain multiple. Now within 5 cents at bias 0 and 1. | No |
| 7b | `Feedback::aLoudRigTakesOverAndACleanOneDoesNot` | Same cause. Shred Lead's E4 at 0.5 m sat near 174° and never took over. With the phase corrected the old gain, tuned on accidental phase, made the clean rig run away. | Padding, then `kInjectionGain` recalibrated (0.0205 → 0.0025) and the resonance threshold re-derived (8 % → 1.6 % of the ceiling). The feedback LED's glow scales to that threshold. | No |

## Feedback: root cause and reasoning

ambiguity-resolutions 1.1 defines the loop as

```
excitation_feedback[s] = k_couple(s) * H_s(f) * amp_out   (delayed one block)
```

H_s is a peak centred on the ringing note, so it has zero phase there. The
per-string path stands in for everything between the speaker and the string:
the direct sound and the room's reflections. At distance 0 the spec reduces
it to "a same-block peak filter", in phase with the note.

The code's delay had three parts:

1. **The block** that breaks the same-block loop (`maxBlock`, 256 in the tests).
2. **Processing latency**, which the loop never counted. The drive pedals'
   and the amp's oversamplers sit between the strings and the ring buffer.
   Since FEAT-CPU, `LatencyPad` holds that latency at the nominal factor's
   value at every quality level (266 samples here).
3. **The air**, `distance / c` (70 samples at 0.5 m).

To the narrow band each string hears, only the delay modulo one period
matters, and that remainder set the loop's phase. The first two parts are
artefacts: they change with the host's block size, the oversampling setting
and, before the pad existed, the quality level. That is why the octave-bias
lock moved from 18.6 to 7.4 cents between merges, and why "does a loud rig
feed back" came down to luck. A scan of an added phase offset at 0.5 m showed
Shred Lead going from full takeover (activity 0.28) to none (0.07) with
nothing else changed. The air alone put E4 at 0.48 of a period, 174°: anti-phase.

Real feedback locks inside the string partial's own bandwidth. A partial's
resonance supplies at most ±90° of phase, so a loop that arrives near 180°
cannot sustain that note at all. A single delay also cannot represent a real
room, where many paths arrive together. So the fix pads the whole delay (block
+ air + processing latency) up to the next whole period of the string's target
(`FeedbackLoop::updateReadDelay`, cubic-interpolated). The loop is in phase on
the partial, and a takeover locks on it at any block size, quality level or
distance. The air still sets how late the sound arrives (the delay is never
shorter than block + air) and, through `k_couple`'s 1/r, how strong it is.

For octave bias > 0 the target is the string's own partial 2^bias, which
dispersion stretches slightly sharp of the plain multiple. `LuthierEngine`
hands the loop the note scaled by `partial(n) / (n · partial(1))`.

**The FEAT-CPU and normalisation interactions:**
- **Latency pad:** now counted, through `FeedbackLoop::setProcessingLatency`
  (pre-effects + amp, per block), and folded into the padding.
- **Oversampling cap:** it changes the effective factor, not the reported
  latency (the pad), and the padding absorbs whatever latency remains. Level
  changes no longer move the lock.
- **Normalisation's master stage:** it sits after the cabinet and the post
  effects. The loop taps the amp's output before the cabinet, so the master
  stage is outside the loop and does not affect it (checked: the loop reads
  `dl` straight after `amp.processSample`).

**Gain:** `kInjectionGain` had been tuned (0.012, then 0.0205) while the phase
was accidental, so part of its value was making up for a loop out of phase.
With the loop in phase:
- "Clean Double-Cut Funk" at 20 % runs away above ~0.0042.
- "Shred Lead" at 100 % and 0.5 m takes over from ~0.0003. The amp saturates
  first, so the settled injection grows with the gain.

0.0025 sits inside that window, 1.7× below the clean rig's runaway. The loud
rig then settles at ~3.4 % of the ceiling, and the clean rig peaks at 0.75 % on
the pluck and decays. `isResonant()`'s threshold moves from 8 % to 1.6 %
(`kResonantShare`, about the geometric mean of the two). The feedback LED's
glow is scaled to that threshold, so it still builds up to the takeover.

## Golden hashes (ON-02)

"Shred Lead" (factory preset 6) ships with `feedback_amount` 45 %, so the
feedback fix changes its output. That is intended, not a regression.
`Source/Tests/Golden/NormalizationOffHashes.json` was regenerated with
`scripts/regen_normalization_hashes.sh`, full grid (`LUTHIER_SLOW_TESTS=1`),
on the toolchain the file records. Only preset 6's entries changed. No other
preset's audio moved: at High the dispersion latch is a no-op, and the loop
is skipped at amount 0.

## Failures on the integration branch that are not in scope

- `GuiReach::everyAutomatableParameterHasAVisibleControl` (BETA B-11: the
  scrape and slap controls, `macro_assign_a/b` and `pickup_blend` have no
  control yet).
- `Combo::everyFactoryPresetPlaysEveryPhrase` and `Combo::snapshotsAndPresetMorph`
  on "P-Bass Flatwound" (BETA B-15: legacy "Old" strings silence a bass above
  about A3).

Both are recorded as open in `docs/audit/BETA_TEST_REPORT.md`. GuiReach was
re-run on the unmodified integration sources and fails there identically.
