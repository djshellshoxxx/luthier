# GAPS-GUI coverage

Workstream GAPS-GUI (branch `claude/luthier-gaps-gui`): the MISSING / PARTIAL /
NO-TEST / NO-GUI rows in `gui-integration.md`, `ui-wiring.md`, `theme.md`,
`accessibility.md` and `gui-engine-dataflow.md` from `docs/audit/SPEC_SWEEP.md`
(the `claude/luthier-spec-sweep` audit). OWNED and DONE rows are other
workstreams' and are not repeated here. One row per requirement touched; tests
are `Suite::test` in `LuthierTests` unless noted.

No new parameters were added in this batch (`Source/Tests/IntegrationTests.cpp`'s
parameter count is unchanged).

## Batch 1: meter/LED staleness, theme.md metrics

Shared infrastructure: `MasterBus::getProcessedBlockCount()`
(`Source/DSP/Master/MasterBus.h/.cpp`) - a counter incremented once per
`processBlock` call regardless of path, so a UI element polling the bus can
tell whether the engine is still rendering and go stale rather than holding
whatever the last real reading was.

| Row | What was done | Test | Status |
|---|---|---|---|
| GD-5 | `LevelMeter` goes to -inf 200 ms after the last processed block (`kStaleAfterMs`), instead of decaying from a stale peak forever | `Theme::levelMeterHoldsDecaysAndGoesStale` | PARTIAL -> the meter's own stale rule is DONE; the header still has no output `LevelMeter` instance (that is GI-16, not done this batch) |
| GD-8 | `OutputLed`: 60 Hz refresh (was 30), dark colour `#5A5F66` (spec, was `#4A4640`), red held 400 ms before re-evaluating (was 1000 ms and Low-quality-only), unlit 100 ms after the last processed block | `Theme::outputLedTracksLevelHoldsRedAndGoesStale` | DONE |
| TH-4 | Knob body and slider-thumb drop shadow to the spec's 8 px blur (was 6 / 5) | covered indirectly by the existing `Theme::controlsRenderInEveryPaletteAndRepeatExactly` (still green); no new pixel-probe test added | DONE (rendering only; not independently verified by a dedicated shadow-radius test) |
| TH-6 | `Fonts::mono` fallback behaviour test | `Theme::monoFontIsAPreferredFaceOrFallback` | still PARTIAL: no JetBrains Mono / IBM Plex Mono binary is bundled in `Resources/Fonts` (this environment has no network access to fetch a redistributable font file); the test asserts the resolved face is one of the preferred names or JUCE's own monospaced fallback, never a proportional face standing in by accident. Bundling the font file is left for a session that can add binary assets |
| TH-7 | Knob value readout raised from 12 px to 13 px mono (spec: 13-14 px); label tracking was already the spec's 0.08em default | (covered by the render probes in the tests below) | DONE |
| TH-8 | Knob size test | `Theme::knobSizesAreTheSpecsThree` | DONE |
| TH-10 / TH-12 | Render probe for the indicator line / value arc / centre dot (muted at default, accent away from it) | `Theme::knobRenderShowsIndicatorArcAndDot` | DONE |
| TH-13 | Regression test that the `SliderAttachment` still wires `setDoubleClickReturnValue`, and that the right-click menu's "Reset to default" (result id 2) resets the parameter | `Theme::knobDoubleClickReturnsToDefaultAndMenuResets` | DONE (the "Enter value..." `CallOutBox` path (result id 1) is UI-async and not covered by an automated test) |
| TH-14 | Render probe: the value row is blank while idle and shows text once `mouseEnter` fires | `Theme::knobShowsValueOnlyWhileHovered` | DONE |
| TH-15 | `LuthierLookAndFeel::drawLinearSlider` now draws 1 px muted ticks every 10% of the track, just outside it (approximates the spec's "every 6 dB for dB params, else 10%" - no per-parameter unit metadata exists yet to pick the dB variant; see UW-10) | `Theme::sliderTicksAreDrawnOutsideTheTrack` | PARTIAL -> DONE for the generic 10% case; the dB-aware 6 dB spacing is deferred until UW-10 lands |
| TH-16 / TH-23 | Render probe for the 28 px height, 4 px corner radius, and the accent border that only the "on" state carries | `Theme::buttonStatesMatchTheSpec` | DONE |
| TH-17 | 100 ms timed accent flash on a momentary press | - | DEFERRED - the current flash is a tint while the mouse is down (`isDown`), not a timed flash that persists after a fast click releases. A real fix needs per-button timer state (a last-click timestamp plus a repaint-driving timer), which is riskier to retrofit across every `TextButton` than the other Theme rows in this batch; left for the next session with a clean run at it |
| TH-18 / TH-19 | (Meter gradient/peak-hold: already implemented) peak-hold + stale test | `Theme::levelMeterHoldsDecaysAndGoesStale` (see GD-5 / UW-30) | DONE |
| TH-27 | Hover brighten reduced from 12% to the spec's ~8%; `UpDownResizeCursor` set on `LuthierKnob`'s and `LuthierSlider`'s slider | `Theme::knobsAndSlidersShowAVerticalResizeCursor` | DONE |
| TH-31 | `HeaderBar` gained test-only accessors (`getPresetNameButton`/`getCompareAButton`/`getCompareBButton`); test that they are visible and that A/B click flips `LuthierAudioProcessor::isSlotBActive` | `Theme::theHeaderCarriesPresetSelectorAndAB` | DONE (visibility and behaviour only; exact left-to-right pixel ordering is not asserted) |
| TH-34 | Covered by the GD-8 fix and test above | `Theme::outputLedTracksLevelHoldsRedAndGoesStale` | DONE |
| UW-30 | `LevelMeter` gained `getPeakHoldLeft/Right`, `getDisplayPeakDb`, `isStale`, and a public `refresh()` (as `FeedbackLed::refresh`) for deterministic testing; peak-hold-then-decay-then-stale test | `Theme::levelMeterHoldsDecaysAndGoesStale` | PARTIAL -> DONE for the meter itself; header meters (GI-16) are still not wired up |

### Verified

- `xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests Theme` - 27/27 tests, 226 checks, green.
- `Master Editor RangesUi QualityModeUi GuiReach ModelGapsUi Live` - 81/83 tests green; the two failures
  (`GuiReach::everyAutomatableParameterHasAVisibleControl` on `scrape_*`/`slap_*`/`macro_assign_*`/`pickup_blend`,
  and `QualityModeUi::CQ22_builtEditorsRegisterEveryTableClassThatExists` on `StringAnimator`) are pre-existing,
  documented elsewhere (GI-1: techniques workstream) or outside this batch's files, and untouched by this branch.
- `Accessibility FacesIntegration Faces Widgets HeaderBar Undo MidiLearn` - 77/80 tests green; the three failures
  (a shortcut-key-clash pair unrelated to any file this branch touched, and an `AmpFacePanel` sizing check) are
  likewise pre-existing.
- Full CI pipeline (VST3/Standalone build, complete `LuthierTests` run) not yet re-run this batch; scheduled before
  this workstream calls itself done.

## Remaining rows in scope (not started this batch)

Left for the next batch, roughly in the order the SPEC_SWEEP work lists put them (effort S/M first):

- **gui-integration.md**: GI-7/GI-35 (hide WHAMMY group on a hardtail), GI-13 (header Save button), GI-16/GI-17/GI-18/GI-19/GI-20
  (header input/output meters, tap/kill/gear/dice/reset, collapse-below-1280, tooltips+shortcuts), GI-21 (A/B transient test),
  GI-33 (Body -> Character link), GI-70 (slide glide-target in the tuning popover), GI-72/GI-73 (snapshot shift-click write +
  label test), GI-90 (locked-range notice test), GI-93 (DECISIONS note only), GI-95 (Assign-to-macro; note: macros are already
  reachable via the existing "Modulate -> Macro" submenu, so this may turn out to be a DECISIONS-documented equivalent rather
  than new code - needs a product call on whether a separate top-level entry is still wanted), GI-104 (DECISIONS note: 200 not
  64), GI-106 (per-instance undo test), GI-107 (Ctrl+O binding), GI-112 (Advanced-header playing-mode control), GI-113
  (Options > Audio "Export audio..." button), GI-130 (first-unlock explainer test).
- **gui-engine-dataflow.md**: GD-2 (fretboard/aux/circuit timer rates to spec), GD-9 (chord dims instead of clearing), GD-13
  (heatmap 500 ms stale + fade), GD-14 (slide bar freeze), GD-25 (header tap LED), GD-26 (kill-switch parameter), GD-29 (A/B
  highlight test), GD-30 (MIDI Learn 1 Hz pulses), GD-31 (looper LED), GD-33 (session buffer bar).
- **theme.md**: TH-17 (real timed flash - see DEFERRED note above), TH-28/TH-29/TH-33 (drag-modifier ordering, tooltip delay,
  footer version text - all plausible but need either simulated `MouseEvent` drag sequences or a text-capture test hook that
  does not exist yet in this codebase).
- **ui-wiring.md**: UW-14 (momentary `LuthierToggle`), UW-15 (`LuthierChoice::rebuildItems`), UW-41 (circuit update-cost test),
  UW-47 (header undo/redo state test), UW-T5 (MIDI Learn 128-CC test).
- **accessibility.md**: effort-S items not yet started - A11Y-7 (meter accessible value), A11Y-9 (snapshot strip accessible
  children), A11Y-10/A11Y-11 (overlay announce + focus return), A11Y-14 (arrow-key stepping), A11Y-15 (Enter opens
  dropdown/dialog test), A11Y-24 (write `Resources/Themes/*.json`), A11Y-25 (meter shape, not just colour), A11Y-27 (10 pt
  font floor), A11Y-40 (font override reaching `Fonts::ui`), A11Y-41 (mono-readout-in-every-locale test), A11Y-46
  (Localization page test).
- Larger, higher-risk items intentionally left for a dedicated pass rather than rushed into this batch: GD-35 (the
  `Tests/Ui/Dataflow/` suite), GD-36 (diagnostics data-flow overlay), UW-6 (the SPSC command/result queue), UW-20 (the
  per-subsystem display FIFO), UW-49 (locale-change broadcaster + `refreshStrings()`), GI-15/GI-79 (header snapshot strip;
  per-source segmented mod arcs), A11Y-4/A11Y-32/A11Y-33 (string-catalog migration and the 14 non-English catalogs).
