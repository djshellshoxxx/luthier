# Code review findings

Reviewer branch: `claude/luthier-review`. One entry per finding; IDs are stable.

Severity: **critical** (crash, heap corruption, data loss), **high** (audible glitch,
wrong sound, feature broken in normal use), **medium** (wrong in some cases,
realtime-safety violation), **low**.

Status: `fixed in <commit>`; `reported-for-<helper-branch>` (the code is inside a
helper branch's active diff and is not edited here); `open` (not yet fixed:
needs a design decision or a larger change than a review fix); `wontfix` with a
reason.

Regression tests for the fixes are in `Source/Tests/ReviewRegressionTests.cpp`
unless stated otherwise.

## Fixed

| ID | Sev | Location | Description and failure scenario | Status |
|----|-----|----------|----------------------------------|--------|
| R-001 | critical | Source/PluginProcessor.cpp processBlock | Host blocks larger than the size promised in prepareToPlay were written whole into `clickBuffer`, `tuneClickBuffer`, `backingBuffer` and `monitorBuffer`, all sized from the promise. FL Studio / Reaper render sends 4096 after preparing 512 with the metronome or tune count-in running: heap overflow. processBlock now renders such a block in slices of the prepared size. Test `aBlockBiggerThanPreparedIsRenderedWhole`. | fixed in f580082 |
| R-002 | medium | Source/PluginProcessor.cpp (feedModulationSources, MIDI out) | `apvts.getRawParameterValue (macroByIndex (m))` every block builds a juce::String (heap allocation) and hashes it, 14 times a block. Cached in the constructor. | fixed in f580082 |
| R-003 | medium | Source/PluginProcessor.h `practicePanelOpen` | Plain bool written by the UI and read by the audio thread (data race). Now atomic. | fixed in f580082 |
| R-004 | critical | Source/UI/Overlays.cpp:236,1145; Source/UI/PracticeSetupPanel.cpp:998,1397,1873 | `NativeMessageBox::showAsync` returns the plain button index (JUCE 8 `ResultCodeMappingMode::plainIndex`): the destructive first button is 0 and Cancel is 1. The dialogs tested `result == 1`, so **Cancel (and Escape) reset everything / deleted the preset, routine, loop or practice history**, and the confirm button did nothing. | fixed in 871a798 |
| R-005 | high | Source/UI/Overlays.cpp:916 (export), :236, :1145 | Async completions captured a raw `this`; closing the window during a render or with the dialog open was a use-after-free. Now SafePointer. | fixed in 871a798 |
| R-006 | high | Source/DSP/Amp/ToneStack.cpp:113 | b3 term `t*C1C2C3R1R2R4` should be `t*l*C1C2C3R1R2R4` (Yeh). With bass at 0 and treble up the passive stack gave +27 dB at 5 kHz. Test `theToneStackIsPassiveAtEverySetting`. | fixed in f736723 |
| R-007 | high | Source/DSP/Effects/PedalsDrive.cpp:478 | Pitch Shifter read delay grew by (ratio-1), so it played at 2-ratio: +7 st came out -7 st, +12 froze. Test `thePitchShifterShiftsTheWayItSays`. | fixed in 23495f6 |
| R-008 | critical | Source/DSP/Effects/PedalsMod.cpp:792-828 | Reverb pedal rebuilt (zeroed) its FDN lines whenever Size/Character was *sent*; the bridge sends every parameter every block, so with blocks shorter than the shortest line the pedal produced no reverb at all, and it allocated on the audio thread. Test `theReverbPedalTailSurvivesRepeatedParameterSends`. | fixed in 7f32812 |
| R-009 | high | Source/DSP/Amp/RoomEngine.cpp:125,184 | `setDecayScale` rebuilt the room every block (bridge), wiping the late reverb; `rebuild` reallocated lines that the audio thread indexes while room size/material change on the message thread (use-after-free). Lines are now sized in prepare and decay only recomputes the loop gain. Test `theRoomTailSurvivesRepeatedDecaySends`. | fixed in 7f32812 |
| R-010 | high | Source/UI/RoutingPanel.cpp:364,392; Source/Routing/RoutingMatrix.h:57 | `MidiOutConfig::macroCc` had 6 entries, the routing panel reads and writes 8 (`kNumMacros`): **stack buffer overflow** (found by ASan) on opening the panel, and a stack write past the config on every control change. Arrays now sized to 8. | fixed in 333a1c8 |
| R-011 | high | Source/Modulation/ModMatrix.cpp:832,896 | Modulated values and per-route offsets went through `sanitise()` (audio NaN guard, clamps to ±4). Any modulated destination above 4 in its own units (concert A, Hz, ms, choice index > 4) was pinned to 4 as soon as a route existed. Test `Modulation::aRouteAtZeroLeavesAContinuousValueWhereItIs`. | fixed in 87086cb |
| R-012 | high | Source/Modulation/ModMatrix.cpp:888 | Discrete snap `floor (v/(N-1)*N + 0.5)` moved choice 1 of 3 to 2, 6 of 12 to 7 with the source at zero: adding a route to amp model/cab/pedal type switched it. Test `Modulation::aRouteAtZeroLeavesEveryChoiceWhereItIs`. | fixed in 87086cb |
| R-013 | high | Source/Model/Playing/MidiInterpreter.cpp:678 | Poly mode (default): a note-off for a note still waiting in the chord window was dropped; the note then sounded and was **never released** (short stabs near a block end). Test `aNoteReleasedInsideTheChordWindowIsReleased`. | fixed in d4f5962 |
| R-014 | medium | Source/Model/Playing/MidiInterpreter.cpp:681 | Note-off released the first string holding that note whatever its channel; on a hex controller a unison released the wrong string. Test `aControllerNoteOffReleasesItsOwnString`. | fixed in d4f5962 |
| R-015 | high | Source/Export/MidiProfiles.cpp:97 | `CharPointer_UTF8::isValidString` accepts a lead byte followed by NUL; the String built from it walks past its terminator (**ASan heap-buffer-overflow** in `LuthierEvents::decodePayload` on a corrupted .mid). Strict UTF-8 check added. Covered by `MidiExport::everyFlippedByteIsRefusedGracefully` under ASan. | fixed in 661c214 |
| R-016 | critical | Source/Practice/BackingTrack.cpp:139-151 | `unload()` (load / next / previous) destroyed the transport and sources on the message thread while the audio thread was inside `getNextAudioBlock`: crash when changing tracks during playback. Swapped under a try-locked SpinLock. | fixed in d1514ae |
| R-017 | critical | Source/Practice/Looper.cpp:838 (SessionRecorder::prepare) | PRACTICE setup resizes the session recorder ring while it records; the audio thread kept writing through the old pointer/capacity. Guarded by a try-locked SpinLock. | fixed in efabeaa |
| R-018 | high | Source/Practice/Looper.cpp:474-486 | Overdub played the live input twice (+6 dB; both branches of the if identical) and replace mode overwrote the take before it was heard. Test `anOverdubIsHeardOnceAndWrapsWithTheLoop`. | fixed in 5fb3dda |
| R-019 | high | Source/Practice/Looper.cpp:91-126 | Overdub record did not wrap at the loop length: a hole at the loop start after each wrap. Same test. | fixed in 5fb3dda |
| R-020 | high | Source/ToneMatch/ToneMatch.cpp:624 (was R-207) | `Capture::processBlock` had no caller; every tone-match wizard hung at "Recording...". Now fed from the main output or the sidechain. Test `aCaptureRecordsTheMainOutput`. | fixed in 9577b47 |
| R-021 | high | Source/Notation/NotationExport.cpp:403,1483,1509 (was R-208/209) | MusicXML `<string>` mirrored (1 must be the highest string); imported `<chord/>` notes spread out in time. Test `musicXmlStringsCountFromTheHighestAndChordsStayTogether`. | fixed in a6b40e0 |
| R-022 | high | Source/PluginProcessor.cpp processBlock (was R-212) | Tap tempo overwritten by the host tempo every block. | fixed in 7229620 |
| R-023 | high | Source/PluginProcessor.cpp handleLiveMidi (was R-206) | Local MidiBuffer allocated on the audio thread every block with MIDI. | fixed in 9383a34 |
| R-024 | high | Source/Presets/PresetManager.cpp:672-711 (was R-204); PresetMorph.cpp:76; PluginProcessor.cpp setlist | Factory presets (no `strings`/`midiMap` block) kept the previous preset's detune, gauges, temperament and CC map; morph and setlist loads skipped applyExtraState. Test `aPresetWithoutAStringsBlockClearsThePreviousDetune`. | fixed in 4033976 |
| R-025 | high | Source/Modulation/ModSources.cpp (was R-205) | prepareToPlay wiped sequencer steps and custom LFO shapes. Test `prepareKeepsTheSequencerSteps`. (Fixed the same way on the integration branch in parallel; merged.) | fixed in d23f115 |
| R-026 | medium | Source/Practice/Metronome.cpp (was R-216) | Beat one never sounded on start; UI thread wrote the click position. Test `theMetronomeStartsOnBeatOne`. | fixed in 6726255 |
| R-027 | medium | Source/DSP/Whammy/WhammyEngine.cpp (was R-222) | Fixed bridge still responded to the arm. Test `aHardtailIgnoresTheWhammyRanges`. | fixed in 1963be8 |
| R-028 | critical | Source/Support/MidiLearn.cpp (was R-201) | Audio thread indexed `mappings` without the lock while the UI cleared/reallocated it; String lookups and raw-`this` callAsync on the audio thread. Now a try-locked POD table and an AsyncUpdater. Test `midiLearnLearnsAppliesAndSurvivesAClear`. | fixed in 569b042 |
| R-029 | high | Source/Model/Playing/MidiInterpreter.cpp:509 (was R-203) | Chord-name String assigned on the audio thread while the UI copied it. Notes kept instead, named on the reading thread. | fixed in 7bdc4d5 |
| R-030 | critical | Source/Live/Snapshots.cpp:288-371 (was R-200) | Recall crossfade (String-keyed parameter reads, module fromVar at the midpoint, change message) ran on the audio thread and raced recall()/state restore. Audio thread now only accumulates time; the processor's timer applies it. | fixed in b6d6688 |
| R-031 | medium | Source/Routing/RoutingMatrix.cpp:225 (part of R-219) | Blocking CriticalSection on the audio thread every block for the MIDI-out config; now a SpinLock around a struct copy. | fixed in 605264d |
| R-032 | critical | Source/DSP/Coupling/CouplingMatrix.cpp:193-221 (was R-210) | Two strings within a few cents (12-string unison courses, a note on two strings) fed each other through the coupling matrix up to the ±4 guard, permanently. Measured with real StringEngines; octaves/fifths decay. Coupling now fades for near-unison pairs. Test `aUnisonPairDecays`. | fixed in 0a2f5f4 |
| R-033 | high | Source/DSP/Common/Oversampler.h:147-153 (was R-211) | Half-band branches used a two-sample memory at the base rate (A(z^4)): 11-14 dB less image rejection, passband droop, and twice the reported latency. Test `theOversamplerDelaysWhatItReports`. | fixed in ed0ae3d |
| R-034 | medium | Source/DSP/Feedback/FeedbackLoop.cpp:12 | Consequence of R-006 and R-033: `kInjectionGain` was calibrated against the buggy amp. Recalibrated (window now 0.020-0.0209, very narrow — the feedback test is fragile to any future amp voicing change). Also: **every amp preset was voiced against R-006's treble boost and is now darker above 1 kHz**; revoicing is a product decision. | fixed in 61b8005, ed0ae3d |

## Reported for a helper branch (not edited here)

| ID | Sev | Location | Description and failure scenario | Status |
|----|-----|----------|----------------------------------|--------|
| R-100 | critical | Source/UI/OptionsPages.cpp:1994 (DiagnosticsPage "Reset everything") | Same as R-004: acts on `result == 1`, i.e. Cancel resets everything. Fix: `result == 0` (NativeMessageBox plain index). | reported-for-claude/luthier-tune-help |
| R-101 | high | Source/UI/OptionsPages.cpp:1607-1640 | Update "Check now" thread and its callAsync capture raw `this`; closing Options during the check is a use-after-free. Capture `Telemetry&` and a SafePointer. | reported-for-claude/luthier-tune-help |
| R-102 | high | Source/PluginEditor.cpp:875-893 | Same as R-101 for the editor's startup update check. | reported-for-claude/luthier-visual |
| R-103 | high | Source/Parameters.cpp:883-898,987,1029,1251-1259 | `ParameterBridge::applyToEngine` (audio thread, every block) looks parameters up by `const juce::String&` built from `const char*` / runtime concatenation (`slotParam`, `pickupVolume`, ...): hundreds of heap allocations and hash lookups per block. | fixed on the integration branch in 6a0d737 |
| R-104 | critical | Source/Parameters.cpp:1254-1259 vs Source/DSP/Effects/EffectsChain.cpp:52-86 | The bridge calls `fx.getPedal(slot)->setParameterNormalised` on the audio thread without `swapLock`; `setSlotType` on the message thread moves the old pedal to `retired` and clears it immediately: use-after-free when the user changes a pedal type during playback. Push pedal parameters under the chain's try-lock, or defer `retired.clear()` until the audio thread has passed a block. | fixed on the integration branch in 6a0d737 |
| R-105 | medium | Source/Parameters.cpp:1371-1560 | `applyStructural` / `applyAllNow` run on the message thread (preset load, state restore, bank select, range change) without parking the engine: `setNumStrings`, `refreshStringPhysics`, room rebuild, `amp.setOversamplingFactor` race the audio thread. Wrap in `ScopedStructuralChange` (and park `bridge.applyToEngine`). | reported-for-claude/luthier-realism-c |
| R-106 | medium | Source/LuthierEngine.cpp:634-635 vs 936-941 | Slide-guitar friction noise set per note in `triggerNote` is reset to 0 by `setNoiseAmounts` on the next block. | reported-for-claude/luthier-realism-c |
| R-107 | medium | Source/LuthierEngine.cpp:1349-1351, 1912-1914, 2005-2007 | Oversized-block path builds a local `MidiBuffer` + `ensureSize(2048)` (allocation) per call; `static thread_local std::vector` scratch resized on the audio thread. (The oversized path is no longer reached from the processor after R-001.) Make them members sized in prepare. | reported-for-claude/luthier-realism-c |
| R-108 | low | Source/LuthierEngine.cpp:2027-2031 | Mono main output is L + 0.5R (≈ +3.5 dB, lopsided). Should be 0.5(L+R). | reported-for-claude/luthier-realism-c |
| R-109 | medium | Source/LuthierEngine.cpp:1583-1587 (with 1352-1386) | Oversized blocks: every slice passes the same host ppq to the rhythm engine, so steps repeat. Mostly moot after R-001. | reported-for-claude/luthier-realism-c |
| R-110 | medium | Source/UI/AdvancedPanel.cpp:126 (and FretboardComponent.cpp:103) | UI writes `engine.getString(i).setDamping(...)` directly on the message thread: data race with the audio thread on plain members. | reported-for-claude/luthier-visual (AdvancedPanel); FretboardComponent open |
| R-111 | low | Source/PhysicalRange.cpp:371 | `RangeState::applyTo` reassigns `AudioParameterFloat::range` (NormalisableRange with std::function members) on the message thread while the host may convert on the audio thread. | reported-for-claude/luthier-realism-c |

## Open (to be fixed on this branch)

| ID | Sev | Location | Description and failure scenario | Status |
|----|-----|----------|----------------------------------|--------|
| R-202 | high | Source/Rhythm/RhythmEngine.cpp:721,625 vs 128-135 | Pattern copied by value on the audio thread (String/StringArray: allocation) and the two-slot double buffer can be overwritten mid-copy by two quick `setPattern` calls. Use `const auto&` and a triple buffer / ack. | reported-for-claude/luthier-techniques, claude/luthier-model-gaps |
| R-213 | high | Source/Support/AudioExporter.cpp:334-352 | Offline export instance created/destroyed on the worker thread starts its own 30 Hz timer (message thread) that races the render, and writes factory preset files concurrently. | open |
| R-214 | medium | Source/UI/GuitarBodyComponent.cpp:306-312 | Clicking the pickup selector treats the 7-way choice as 5/3 evenly spaced steps: never reaches Neck or Bridge+Middle. Map switch positions to explicit PickupSelector values. | reported-for-claude/luthier-visual |
| R-215 | medium | Source/UI/ModMatrixPanel.cpp:451,556-620 | Route table cache goes stale; clicks act on the wrong route after routes change elsewhere. | reported-for-claude/luthier-visual |
| R-217 | medium | Source/Practice/Looper.cpp:334-351 | Any prepareToPlay (buffer size change, offline bounce) erases the recorded loops and reallocates ~276 MB. Only reallocate when the rate/capacity changes. | reported-for-claude/luthier-model-gaps |
| R-218 | medium | Source/Practice/Looper.cpp:542-560 | `Looper::captureMidi` adds to a MidiMessageSequence on the audio thread (allocation) racing clear/save on the message thread. (Note: R-018/R-019 were fixed in Looper.cpp before model-gaps touched it; expect a merge there.) | reported-for-claude/luthier-model-gaps |
| R-219 | medium | Source/Rhythm/RhythmEngine.cpp:149 | `getHumanise` takes a blocking CriticalSection per strum / step on the audio thread. (The RoutingMatrix half is R-031.) | reported-for-claude/luthier-techniques, claude/luthier-model-gaps |
| R-220 | medium | Source/DSP/Amp/RoomEngine.cpp:154-158 | Early-reflection taps beyond the 0.5 s buffer are all clamped to its end: 11 taps pile into one loud echo in a concert hall. Needs a design decision (buffer size vs tap spread). | open |
| R-221 | medium | Source/DSP/String/StringEngine.cpp:354-358 | Loop coefficients recomputed every sample while the frequency smoother glides (compares target vs last current). CPU spikes on bends/slides. Compare `getCurrentFrequency()` against `lastCoefficientHz`. | reported-for-claude/luthier-techniques, claude/luthier-model-gaps |
| R-223 | medium | UI async lambdas: HeaderBar.cpp:347/377/401/453, Overlays.cpp:683/772/1621, WorkshopPanel.cpp:739, LivePanel.cpp:460, LiveStrip.cpp:215, ModMatrixPanel.cpp:584; Widgets.cpp:350-369 | File choosers / inline editors / popup menus capture raw `this`; use-after-free if the window closes first. | open |
| R-224 | low | Source/UI/CharacterPanel.cpp:199 | Dead-spot drag re-subtracts the whole drag distance on each event. Store the depth at mouse-down. | reported-for-claude/luthier-techniques, claude/luthier-model-gaps |
| R-225 | low | Source/UI/Overlays.cpp:384-386 | Debug stream "trim" empties the view (`fromLastOccurrenceOf("\n")` of text ending in a newline). Keep the tail instead. | reported-for-claude/luthier-tune-help, -visual, -techniques |

| R-226 | medium | ASan full run (asan-tests3) | One intermittent SEGV in `juce::String` release inside `setPracticeStatsFile` on a freshly constructed processor (PracticeDrawerTests), not reproduced alone or in the next full run: points at a background thread or timer from an earlier test writing into freed/reused memory. Candidates: R-213 (export instance timer), R-101/R-102 update-check threads. | open (investigating) |

## Third-party / sanitizer notes

| ID | Where | Note | Status |
|----|-------|------|--------|
| R-900 | libstdc++ 13 `std::stable_sort` temporary buffer (e.g. PerformanceCapture.cpp:673) | ASan `alloc-dealloc-mismatch (operator new vs free)`: libstdc++'s `get_temporary_buffer` / `__return_temporary_buffer` pair under clang ASan; not project code. Suppressed with `ASAN_OPTIONS=alloc_dealloc_mismatch=0`. | wontfix (toolchain false positive) |
| R-901 | JUCE juce_MemoryBlock.cpp:89,122 | UBSan: `memcpy (nullptr, nullptr, 0)` when copying an empty MemoryBlock. Benign in practice; JUCE code. | wontfix (third party) |
| R-902 | JUCE juce_CharacterFunctions.h:377 | UBSan: signed overflow parsing a huge JSON exponent (mutated preset test). JUCE code. | wontfix (third party) |
