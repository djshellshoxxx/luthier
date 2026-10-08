# Optimisation log (claude/luthier-optimize)

Goal: no lag in code, audio playback or GUI controls, with the rendered audio
unchanged. Each entry: what changed, how it was measured, what was run.

Measurement: a local harness (`Source/Tests/ZZGuiProfileHarness.cpp`, untracked)
opens the editor on an Xvfb desktop while a thread strums chords at 48 kHz / 256
for 10 s and reports the message thread's CPU time (`CLOCK_THREAD_CPUTIME_ID`,
"GUI %" below = share of one core) and the paint count per component. perf
(`-F 299..999 --call-graph dwarf`) attributes the time. A second local harness
times `PresetManager::refresh()`, every preset load and every workspace tab
switch on the message thread, with a 1 ms timer recording the longest gap.
The machine is shared, so figures carry about +-0.5 % of noise.

## GUI while playing

| Commit | Change | Easy GUI % | Advanced GUI % |
|---|---|---|---|
| (start) | after 7aac98a / b2bbd63 | 13.4 | 10.8 |
| 9412763 | rest lines and dashed winding as rect runs (stroke + edge table per ringing string per frame was ~25 % of the thread) | 10.4 | 9.4 |
| c36f6d4 | guitar / fretboard caches bake the opaque parent's background: RGB cache, plain blit, child opaque, parent spared (drawImage 12 % + EasyPanel::paint 4 % before); name plate into the cache | 8.7 | 8.8 |
| 6268433 | `AnimationPolicy::isShowingFast`: `isShowing()` was an X round trip per call (StringAnimator::poll 60 Hz, sweep): ~5 % of the thread | - | - |
| aa44d64 | HeaderBar from a PaintCache, MIDI blink repaints the 6 px dot; VuMeter face cached; LevelMeter / MicPad / AssistPill / QualityBadge repaint only on change; PianoRollStrip idle = no repaint | 7.0 | 7.4 |
| (end) | all of the above, re-measured at the end; process total incl. audio 25-27 % -> 21 % (Easy), 24 % -> 22 % (Advanced); idle GUI thread 2.2 % | 6.7 | 7.5 |

Idle (editor open, no notes): HeaderBar, OutputLed, AssistPill, MicPad went from
150-490 paints / 10 s to 1; PianoRollStrip from 300 to 0. The LevelMeter still
repaints ~14 Hz at idle because its -inf/-9x.x dB readout text jitters with the
noise floor (left: the text really changes).

Tests run after the GUI changes (all green): AnimatedStrings, Editor, GuiReach,
CpuQualityUi, Accent, ContextMenu, DataStream, Diagnostics, DragToModulate,
LiveDisplays, NewDots, NoiseStrip, RangeMarking, Reflow, ScreenReader,
StageTouches, PianoRoll, MicPlacementUi, Widgets, Circuit, Fretboard,
GuitarIllustration, EasyLayout, FacesIntegration, Faces, ReducedMotion,
Accessibility, AutoArticulationUi, Onboarding, Theme, NoteDots, VisualAids,
Screenshots. (CQ01/CQ22/CQ29 scan the source tree through a ccache-relative
`__FILE__` and only pass when run from the build directory; CQ11/CQ12 are
CPU-timing tests that fail on this loaded shared machine; both unrelated.)

## Message-thread stalls (preset switch, tab switch, startup)

| Commit | Change | Before | After |
|---|---|---|---|
| 4c7b0bc | `refresh()` (first list read and after every save): writeAll no longer JSON-parses all 70 factory files to read `factoryRevision`; scanFolder reuses a file's parsed metadata while its stamp and size are unchanged | 54.8 ms sync | 8-10 ms |
| a080e39 | `AdvancedPanel::Column::paint` headings from a PaintCache (8 % of the thread across a run of preset loads); tolex flecks as two filled paths instead of 3000 fillEllipse calls | Column::paint 8.0 %, fillTolex 4.5 % | 0.8 %, 2.9 % |

Startup here: processor 230-250 ms, editor 150 ms (unchanged; dominated by
prepareToPlay / Looper::prepare and building the Advanced workspace). Preset
load: 2-7 ms synchronous, worst 1 ms-timer gap in the 400 ms after a load
22-25 ms -> 14-18 ms (what remains: AmpFacePanel::renderFace when the amp model
changes, RangesUi::resyncControls, PedalSlotComponent::rebuildControls).
Tab switches: 3-13 ms sync; CHARACTER ~19 ms (CharacterPanel is a 4900 px tall
column laid out on show).

## Audio thread: profiled, not changed

perf on the harness's audio thread, 12 s each, chords vs. silence:
AmpEngine::processCore 8.5 % / 9.6 % plus `expm1`+`tanh` 7 % / 6 % (the
waveshaper), StringEngine::beginSample 4.3 / 4.7, PickupEngine::processStrings
2.9 / 3.2 (its mains hum: three `sin` per sample), ParameterBridge::applyToEngine
2.2 / 2.5 (per block), IrVariants FFT 3.7 / 4.0, RoomEngine 1.7 / 1.7,
CouplingMatrix 1.5 / 2.2, FretBuzz 1.5 (per block, 24 frets x strings).
No `malloc`/`free` samples on the audio thread in either run; the
CODEX_RTSAFETY P0/P1 items read as fixed (CodexRtSafety suite green).

Idle costs as much as playing (B-13 confirmed). Nothing here could be made
cheaper bit-exactly: the amp, pickups (hum), strings and room all carry
non-zero state and noise while silent, string sleep is Medium/Low-only for
the reason Task C found, and applyToEngine's ~200 setters are not proven
idempotent, so skipping it on "no change" could move a smoother. Candidate
hoists that are bit-exact (FretBuzz::displacementMm's pluck weights, the
per-fret saddle maths in clearanceMm) are under 1 % of the thread, below the
harness's noise, and were left out rather than claimed.

Normalization ON02 golden hashes, ON27, Combo determinism and CodexRtSafety
were run at the end as a sanity check (no DSP file changed): green.
