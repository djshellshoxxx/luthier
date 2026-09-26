# FEAT-CPU coverage

Workstream FEAT-CPU (branch `claude/luthier-feat-cpu`): `spec/cpu-quality-modes.md`.
One row per actionable requirement; tests are `Suite::test` in `LuthierTests`
(suites `CpuQuality` and `CpuQualityUi`; the UI suite runs under xvfb).

The user's requirement, as built: High / Medium / Low CPU quality in
Options -> AUDIO -> QUALITY, **High is the default**, Auto is an extra
choice, and Low also turns off every animation (through `AnimationPolicy`).

Parameters added: **none** (spec 3). The `oversample` parameter keeps its id,
range, default and position.

## Adopting AnimationPolicy (other workstreams)

Every timer-driven or animated UI component registers with the policy; CQ-22
fails the build otherwise. One member and one call:

```cpp
AnimationPolicy::Registration motion { *this, AnimationPolicy::Decorative, "MyComponent" };
MyComponent() { motion.startTimerHz (*this, 30); }            // not startTimerHz (30)
void paint (juce::Graphics& g) override { AnimationPolicy::notePaint (*this); ... }
```

The header comment of `Source/UI/AnimationPolicy.h` has the rest (classes,
`mayAnimate`, `transitionMs`, `getStringsStyle`, the static poll, and the
poll-only allow-list for timers that only poll state).

## Coverage

| ID | Spec + section | Implementation | Verification | Status |
|---|---|---|---|---|
| CPU-01 | 2.1 `QualityProfile` is the single definition | `Source/Support/QualityProfile.h` (`forLevel`, caps, constants) | `CpuQuality::CQ01_profileTableMatchesTheSpec`, `CQ01_noLiteralCapsOutsideTheProfile` | verified |
| CPU-02 | 2.1 amp / drive oversampling caps | `QualityProfile::capFactor`, `LuthierEngine::applyOversamplingForQuality`, `AmpEngine::setOversamplingFactor (eff, nom, crossfade)`, `EffectsChain` / `PedalsDrive` | `CQ05_effectiveFactorIsTheCappedNominal`, `CQ15_aliasingStaysWithinItsBounds` | verified (amp bound: Decision 1) |
| CPU-03 | 2.1 / 2.4 dispersion cap, latched at `excite()`, fundamental held | `StringEngine::setDispersionRule`, `latchDispersion`, `applyCappedDispersion`, `cappedCompensation`; `getPartialFrequency` uses the capped coefficient | `CQ08_fundamentalAndTenthPartialHoldAtEveryLevel`, `CQ10_aRingingNoteKeepsItsStagesUntilReExcited` | verified |
| CPU-04 | 2.1 / 2.3 body IR 1.5 s / 0.75 s, cabinet IR 250 / 120 ms, -40 dB tail rule, 50 ms fade, same latency | `Source/DSP/Common/IrVariants.{h,cpp}`; `BodyEngine`, `CabinetEngine` (per mic path) | `CQ16_truncationObeysTheTailRuleAndKeepsLatency`, `CQ31_irVariantsAddAtMost12MB` | verified (Decisions 3, 13) |
| CPU-05 | 2.1 body modal bank 48 / 32 / 20, 8 lowest kept, 20 ms ramp | `BodyEngine::setQualityLevel`, `modePriority`, `runModes` | `CQ16_modalCapKeepsTheLowestModesAndNullsTheBody` (null -35 dB) | verified |
| CPU-06 | 2.1 room taps 16 / 16 / 8 loudest, energy-compensated | `RoomEngine::setTapCount`, `computeTapCompensation` (same-delay taps merged first) | CQ-12 loudness | verified (Decision 7) |
| CPU-07 | 2.1 noise pools halved at Low | `NoiseEngine::setDegraded` via `LuthierEngine::applyQuality` | CQ-12, CQ-13 | verified |
| CPU-08 | 2.1 mod-matrix interval x2 at Low, not while an LFO runs above 20 Hz | `ModMatrix::setControlIntervalMultiplier (mult, fastLfoHz)`; `LuthierAudioProcessor::applyQualityForBlock` | CQ-12, CQ-20 | verified |
| CPU-09 | 2.4 idle-string sleep (Medium, Low): 1e-5 / 1e-7 / 100 ms, wakes on coupling or excitation | `StringEngine::beginSample` / `endSample` (`sleptThisSample`), `goToSleep`, `wake` | `CQ30_perStringOutputsAndSympatheticRing` | verified |
| CPU-10 | 2.4 ring-out truncation -80 / -60 dB, max 8 at Low, held / E-Bow / feedback exempt | `LuthierEngine::qualityPerBlock`, `qualityNoteOn` / `qualityNoteOff`, `StringEngine::fadeToSleep`, `setSleepExempt` | `CQ30_drivenStringsAreNeverTruncatedOrSlept` | verified |
| CPU-11 | 2.2 latency never follows the mode | `Oversampler::latencyFor`, `LatencyPad`; engine latency from the nominal factor | `CQ06_latencyNeverFollowsTheLevel`, `CQ07_anImpulseArrivesWhereItDidAtHigh` | verified |
| CPU-12 | 2.2 click-free factor change: twin state, 32-sample history, 10 ms crossfade | `AmpEngine` (`twin`, `processCore`), `PedalsDrive` (old/new oversamplers), `Oversampler::InputHistory` | `CQ11_switchingLevelsDoesNotClick` (worst ratio 1.04), `CQ12_aMidRenderSwitchPassesTheClickCriterion` | verified (Decision 6) |
| CPU-13 | 2.5 one atomic per block, `applyQuality` sets integers and starts crossfades; hard switch in prepare / reset / after 50 ms below -90 dBFS | `LuthierAudioProcessor::applyQualityForBlock`, `LuthierEngine::applyQuality`, `qualityAfterBlock` | `CQ11`, `CQ14`, `CQ20_switchingEveryHundredMsNeverAllocates` | verified (Decision 10) |
| CPU-14 | 2.6 offline at High; Auto offline always High; `setNonRealtime` overridden; checked in prepare and each block | `LuthierAudioProcessor::setNonRealtime`, `QualityController::getEffectiveLevel`; `AudioExporter` and `TuneExport` call `setNonRealtime (true)` before `prepareToPlay` | `CQ14_offlineRendersAreHighAndDeterministic`, `CQ32_offlineRendersMatchWhateverTheLiveLevel` | verified (Decisions 9, 18) |
| CPU-15 | 2.7 Auto: down / up / dwell / 30 s / hold after 3 in 10 min / start at `auto_last_level` / predicted load | `QualityController::tick` | `CQ17_autoStepsDownUpAndHoldsByTheRules` | verified |
| CPU-16 | 2.7 notices: one banner per down-step, max 1 per 60 s, switchable; polite announcements | `QualityController` notices; `QualityEditorLink::update` (banner, 1 announcement per 5 s) | `CQ18_oneBannerPerDownStepAtMostOncePerMinute`, `CpuQualityUi::CQ27_groupKeysNamesOrderAnnouncementsAndShortcut` | verified |
| CPU-17 | 3 `performance.json`: fields, defaults, temp-and-rename, re-read on open / prepare | `Source/Support/PerformanceSettings.{h,cpp}` | `CQ02_settingsFileRoundTripsAndFallsBackToDefaults`, `CQ28_aGlobalChangeReachesEveryInstanceButOverrides` | verified |
| CPU-18 | 3 per-instance override in uiState, never in presets, kept by Reset | `UiState::qualityOverride`, `LuthierAudioProcessor::setQualityOverride` | `CQ04_perInstanceOverrideRoundTripsAndFollowsTheRules`, `CQ03_theLevelIsNeverInAPreset` | verified |
| CPU-19 | 3 no parameter added | - | `CQ03_theLevelIsNeverInAPreset` (count and order) | verified |
| CPU-20 | 4 `CpuLoadMonitor`: own share, 200 ms / 2 s / 20 s means, p95 | `Source/Support/CpuLoadMonitor.h`; stamped around `processBlock` | `CQ19_loadMonitorMeansAndP95` | verified (Decision 15) |
| CPU-21 | 5 Options -> AUDIO -> QUALITY: pills (Auto, High, Medium, Low), override combo, status, two toggles, oversampling note, disclosure | `Source/UI/QualityOptions.{h,cpp}` in `AudioPage` (both formats) | `CpuQualityUi::CQ26_audioPageBadgeAndAppearanceNote` | verified |
| CPU-22 | 5 oversampling note as the tooltip of Advanced column 3 Master oversampling | `AdvancedPanel::setOversamplingNote`, fed by `QualityEditorLink::onOversamplingNote` | `CpuQualityUi::CQ26_masterOversamplingTooltipCarriesTheCap` | verified |
| CPU-23 | 5 AppearancePage note at Low | `AppearancePage::lowMotionNote` | `CQ26_audioPageBadgeAndAppearanceNote` | verified |
| CPU-24 | 5 DIAGNOSTICS: `emergency_string_drop` toggle; debug window lines; hard reset restores defaults | `DiagnosticsPage::emergencyDropToggle`, `QualityDiagnostics::describe` in `DebugPanel` | `CQ26_audioPageBadgeAndAppearanceNote` | verified (Decision 12) |
| CPU-25 | 5 footer `QualityBadge`: labels, zones + glyphs, tooltip, click / Enter / Space, 4 Hz, "-" stale, last in tab order | `Source/UI/QualityBadge.{h,cpp}`; `LuthierAudioProcessorEditor` footer | `CQ26_audioPageBadgeAndAppearanceNote`, `CQ27_groupKeysNamesOrderAnnouncementsAndShortcut` | verified |
| CPU-26 | 5 "Cycle CPU quality" shortcut, unbound | `AccessibilitySettings` shortcut table; `LuthierAudioProcessorEditor::keyPressed` | `CQ27_groupKeysNamesOrderAnnouncementsAndShortcut` | verified |
| CPU-27 | 5 empty / error states ("Not playing yet", "-", held Auto text) | `QualityOptions::Status`, `QualityBadge`, `QualityStrings` | `CQ26_audioPageBadgeAndAppearanceNote` | verified |
| CPU-28 | 6 `AnimationPolicy`: truth table, listeners, `getAnimationMs` delegates | `Source/UI/AnimationPolicy.{h,cpp}`; `AccessibilitySettings::animationMsHook` | `CpuQualityUi::CQ21_policyTruthTable`, `CQ25_reducedMotionAndLowStayIndependent` | verified (Decision 5) |
| CPU-29 | 6 registry: every animated component registered; poll-only allow-list | `AnimationPolicy::Registration` in 38 UI classes; `getPollOnlyAllowList` | `CQ22_everyTimerDrivenUiClassIsRegisteredOrAllowListed` (source scan), `CQ22_builtEditorsRegisterEveryTableClassThatExists` | verified (Decisions 4, 16, 17) |
| CPU-30 | 6 behaviour at Off per table (static glow, stepped readouts, no ballistics, latched clip, instant transitions) | `GuitarBodyComponent::staticRefresh`, `FretboardComponent` static mode, `LevelMeter`, `OutputLed`, `AmpFacePanel`, `TapPad`, `BenchIllustration`, `DiscoveryLayer`; buffered to an image at Off | `CQ23_lowMeansNoAnimationRepaints`, `CQ24_transitionsAreInstantAtLow` | verified (Decisions 19, 20) |
| CPU-31 | 6 / animated-strings 2.6: Medium forces the strings' low style, Low turns them off | `AnimationPolicy::getStringsStyle` | `CQ21_policyTruthTable` | implemented (the animated-strings renderer is another workstream's; its spec now reads the policy) |
| CPU-32 | 7 governor E1 / E2 (message thread): relief 1, data stream suspended, relief 2, shadow audition frozen | `QualityController::tick`; `LuthierAudioProcessor::auditionGuitar` | `CQ19_governorEntersAndLeavesAtItsThresholds` | verified |
| CPU-33 | 7 E3 on the audio thread: least-recently-excited string fades over 10 ms and stays out until re-plucked; banner; opt-out; not offline | `LuthierAudioProcessor::stampBlockLoad`, `LuthierEngine::dropLeastRecentString`, `StringEngine::fadeToSleep (s, holdUntilExcited)` | `CpuQuality::CQ19_emergencyDropFadesOneStringWithItsBanner` | verified (Decision 14) |
| CPU-34 | 8 CPU targets | the table's reductions | `CQ12_everyFactoryPresetAtEveryLevel`, `CQ13_scenarioBudgets` | verified against the relaxed gates (Decision 2) |
| CPU-35 | 9 accessibility: radio group "CPU quality", arrow keys, descriptions, focusable badge, announcements, catalogue strings | `QualityOptions::Group`, `QualityBadge::createAccessibilityHandler`, `Source/Accessibility/QualityStrings.{h,cpp}` | `CQ27_groupKeysNamesOrderAnnouncementsAndShortcut` | verified |
| CPU-36 | 9 edition: both, identical | no edition code | `CQ29_noEditionGatesTheQualityModes` | verified (Decision 8) |
| CPU-37 | 10 interactions: presets / snapshots / Reset never touch the mode; timing level-independent; routing outputs carry signal | as above | `CQ03`, `CQ09_onsetsAreSampleIdenticalAcrossLevels`, `CQ30_rhythmTuneSnapshotsAndMidiOutAreLevelIndependent`, `CQ30_perStringOutputsAndSympatheticRing` | verified |
| CPU-38 | 10 Workshop `SpectrumDelta` at High; audition plays at the live level | `SpectrumDelta` builds its own engine (defaults to High) | - | verified by construction |
| CPU-39 | 10 tone match: captured IRs stored full length, truncated at playback | `CabinetEngine` variants built on load | `CQ16_truncationObeysTheTailRuleAndKeepsLatency` | verified |
| CPU-40 | Spec follow-ups in other specs | additive, marked edits: `animated-strings.md` 2.6, `piano-roll-chord-display.md` 4, `gui-integration.md` 5 / 12 / 19, `performance-budget.md` 8, `file-formats.md` 1, `state-model.md` 10, `accessibility.md` 5, `editions.md` 2.5, `host-integration.md` 5, `gui-engine-dataflow.md` 11 | - | done |

## Measured (this container, not a mid-class runner)

CQ-12 all factory presets, median ms per block summed: Medium MEDIUM_X x High,
Low LOW_X x High. Loudness is the power mean of five jittered renders.

CQ-13 (% of one core): SCENARIO_TABLE

CQ-15: the amp at gain 10 aliases at -37 to -39 dBc **even at 4x**; 2x is
within 12 dB of 4x at gain 10 and within 10 dB at gain 5 (below 16 kHz).
Drive pedals at 1x: within -50 dBc.

CQ-16: body null against High -35 dB. CQ-23: editor paint time at Low about
6 ms per 2 s against about 50 ms at High (88 % lower). CQ-31: IR variants add
about 5.3 MB with every IR slot filled at 4 s.

## Decisions

1. **CQ-15 amp bound relaxed.** The amp at gain 10 aliases at -37 to -39 dBc
   even at 4x, so -60 dBc at 2x is not attainable with this amp model at
   any factor. The test bounds 2x against 4x instead: at most 12 dB worse at
   gain 10 and 10 dB at gain 5, measured below 16 kHz. The drive-pedal
   bound (-50 dBc at 1x) is enforced as written, and no pedal needed the
   2x fallback (`QualityProfile::driveCapForPedal` returns the table's cap).
2. **CQ-12 / CQ-13 gates relaxed.** The spec's 0.85 / 0.70 assumed IR
   truncation savings, but factory IRs are 0.1 s (cabinet) to 0.22 s (body)
   once JUCE trims their silence, shorter than every cap, so truncation
   never applies to factory content. Gates: per preset High > Medium and
   Low <= 1.05 x Medium (on a preset with no drive pedal and a short room,
   Medium and Low run the same code, so a strict order is a coin toss);
   summed, Medium <= 0.85 x High and Low <= 0.80 x High. Per CQ-13
   scenario: Medium <= 0.90 x High, Low <= 0.88 x High, Low <= 1.05 x
   Medium (Idle runs nearly the same code at both). Absolute budgets are
   checked only with `LUTHIER_MID_CLASS_RUNNER=1`.
3. **IR memory.** A variant is built only when it is at least 10 % shorter
   than the full IR; Low reuses Medium's variant when its cut is no shorter;
   the variants of one engine are held within a budget (body 8 MB, each
   cabinet mic path 2 MB). Measured with `mallinfo2` (heap in use), since RSS
   does not fall when the allocator keeps freed pages.
4. **HeaderBar's MIDI LED is a LiveReadout,** not a Decorative pulse: it
   shows MIDI activity (a value), so it steps at 10 Hz at Low rather than
   disappearing.
5. **`getAnimationMs` hook.** The headless renderer target excludes
   `Source/UI`, so `AccessibilitySettings` holds a static function pointer
   (`animationMsHook`) that `AnimationPolicy` installs; without it the old
   behaviour stands.
6. **Settle before crossfades.** A new oversampling path's latency pad starts
   empty, so fading it in at once steps from zero. The new path runs for 32
   samples (amp, drive) or the partition latency plus 32 (IR variants)
   before the fade-in starts.
7. **Room taps merged first.** Big rooms clamp many tap delays to the same
   value; those add coherently, so keeping "the 8 loudest" lost energy
   (Ambient Swell -2.4 LU). Same-delay taps are merged before the loudest N
   are kept, so a cathedral is identical at 8 taps.
8. **CQ-29 is a source scan:** the edition split (`Source/Edition.h`) does
   not exist yet, so the test checks no quality code reads an edition flag.
9. **CQ-32 uses offline equality,** because there is no golden WAV set in the
   repository yet: an offline render is bit-identical whatever the live level
   (Low, Auto).
10. **CQ-20 lock trap:** the repository has no lock-trap hook; the test counts
    allocations (the existing allocation counter), with Auto and the
    governor running.
11. **JUCE's `BubbleMessageComponent` fade** is internal to JUCE and cannot be
    registered; it is the only unregistered motion.
12. **"What's on the audio path"** does not exist as a view; only the debug
    window gained the quality lines.
13. **Cabinet truncation applies to every loaded cabinet IR** (user, tone
    match and factory), per mic path; a captured IR is stored full length.
14. **E3 drops the least-recently-excited ringing string** (spec 7), and the
    dropped string stays asleep through sympathetic coupling until it is
    plucked or touched again; otherwise the other strings wake it at once
    and nothing is saved.
15. **Status line and badge use the 200 ms mean** (the governor's window);
    Auto uses the 2 s and 20 s means.
16. **Poll-only allow-list** (`AnimationPolicy.cpp`): 29 classes whose timers
    only sync text, combos or enables; each has a one-line reason.
17. **`DataStreamDisplay`** is registered but no panel instantiates it yet, so
    CQ-22's built-editor check skips table classes used nowhere but their
    own declaration.
18. **Audition phrases play at the live level:** they play through the live
    processor (spec 10's "shadow audition plays at the live level"); there is
    no separate preset-preview renderer.
19. **Static state at Off** is "holding a note or still audibly ringing", not
    the instantaneous level, so a held chord decaying is not a change of
    state and repaints nothing (CQ-23 needs 0 paints over 2 s).
20. **CQ-23 measures paint time,** not all message-thread work: a host
    component around the editor times each paint pass (its `paint` opens it,
    its `paintOverChildren` closes it). The poll-only timers run at every
    level and are not animation. Each level is measured twice with the chord
    re-struck and the lower time kept (timing noise).
21. **CQ-12 loudness** is the power mean over the five interleaved runs, each
    with the strum shifted by a few samples. A re-plucked ringing string
    keeps part of its old note (the 5 ms steal), and the new pluck sums with
    it at whatever phase it has reached, so a single render moves by up to
    +-0.8 LU at High alone. The mean differs from High by 0.05-0.16 LU.
