## jam-mode.md

Jam mode does not exist on this checkout: no `Source/Jam/`, `Source/DSP/Jam/`, `jam_*` parameters, JAM tab, Easy/Live pills, Aux 9/10 or JM tests. The pieces it reads from are present and working: `ChordDetector` (`kConfidenceFloor`, templates with `intervalMask`, `ChordSymbol::isSlash`), `RhythmEngine::isDriving/getCurrentChord/isBassFamily`, `Metronome`, `Looper`, `BackingTrackPlayer`, `TapTempo`/`getEffectiveTempo`, `KillSwitch`, `TuneTimeline::build`, `RtRandom`, `MidiOutConfig`, `buildBusesProperties` (Aux 8 last). No FEAT branch pushed yet; all rows OWNED.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| JM-0 (§0) | Ground rules: no samples, band in processor (not guitar path), musical bass changes, RT-safe atomic swaps, deterministic seeded `RtRandom`, `L`-sample alignment, free when off — not built | - | - | - | OWNED |
| JM-2.1 (§2.1) | Five-state transport; start modes Auto/Host/First Note/Count-In/Tap In; START semantics — not built | - | - | - | OWNED |
| JM-2.2 (§2.2) | Stop: host stop, STOP + ending, stop-on-silence, ending, panic — not built | - | - | - | OWNED |
| JM-2.3 (§2.3) | Tempo priority, meter source, style meters/generic bar, double/half time, own clock drives `setTransportPosition/setTempoBpm` — not built | - | n/a | - | OWNED |
| JM-3.1 (§3.1) | Chord sources Auto/Live/Tune via own `ChordDetector` (fed after learn/tune merge + looper MIDI), rhythm-engine override, >=2 pc rule, slash bass, token resolution — not built (`Rhythm/ChordDetector.h:87` exists) | - | n/a | - | OWNED |
| JM-3.2 (§3.2) | Follow quanta Tight/Natural/Relaxed/Bar + grace window, damp/pluck at change — not built | - | n/a | - | OWNED |
| JM-3.3 (§3.3) | Anticipation: `JamChordMap` from `TuneTimeline::build`, section fills/hints; repeat-cycle prediction — not built | - | - | - | OWNED |
| JM-4.1 (§4.1) | 10 factory styles table, A/B, fills, ending, dbl/half bars, genre-kit via `GenreKitLibrary::apply` when linked — not built | - | - | - | OWNED |
| JM-4.2 (§4.2) | Intensity 1-5 semantics + dynamics follow with hysteresis — not built | - | - | - | OWNED |
| JM-4.3 (§4.3) | Fill period, Fill Now, humanise (lane push/pull), swing — not built | - | - | - | OWNED |
| JM-4.4 (§4.4) | Change-quantisation table — not built | - | n/a | - | OWNED |
| JM-5 (§5) | `JamDrumKit`: `ModalResonatorBank` pieces (kick, snare wires/brush, toms, hat choke, ride, crash bloom, rim, PhISEM shaker), voice pool/stealing, 5 kits, tuning/damping/room/perspective/width — not built | - | - | - | OWNED |
| JM-6 (§6.1, §6.2) | `JamBassVoice`: 2 ping-ponged `StringEngine`s, bassist string choice, excitations, `JamBassTone`; bass line tokens, register rules, voice choice — not built | - | - | - | OWNED |
| JM-7 (§7) | Mixer (volume, balance, pans, mutes), bass rests when guitar is bass, `jam_output` Aux 9/10 appended after Aux 8 + ROUTING strips, mix point after looper/before sessionRecorder, `KillSwitch::applyBlockRamp` — not built | - | - | - | OWNED |
| JM-8.1 (§8.1) | Col 4 JAM tab after TUNE with status, STYLE/FEEL/FOLLOW/START-STOP/KIT/BASS/MIXER groups, read-only `JamLaneView`, drag/export — absent | - | - | - | OWNED |
| JM-8.2 (§8.2) | Easy rhythm-strip JAM group (pill, style, intensity dots, Band knob), Live Strip JAM pill, `J`/`Shift+J`/`Alt+J` shortcuts — absent | - | - | - | OWNED |
| JM-8.3 (§8.3) | `JamStatus` double-buffered snapshot, 30 Hz drain, 250 ms stale — absent | - | n/a | - | OWNED |
| JM-8.4 (§8.4) | Empty-state / error messages — absent | - | - | - | OWNED |
| JM-9 (§9) | `MidiOutConfig::jamParts` GM ch10/bass ch11, `JamCapture` ring, drag-out Type 1, Export MIDI range, Tune export "Include Jam band" — absent | - | - | - | OWNED |
| JM-10 (§10) | 34 `jam_*` params appended in order; `RangeFamily::jam` for tuning/damping; transient `jam_play`/`jam_fill_now` excluded from presets/snapshots/morph, restore off — absent | - | - | - | OWNED |
| JM-11 (§11) | Interactions: rhythm engine, looper bar-quantise + `Looper::renderPlaybackMidi`, metronome ducking pref, tune percussion replaced / tune bass through JamBassVoice, `.luthiertune` jam hint, PROG, backing-track hint, snapshots/setlist, host sync, multi-instance — not built | - | - | - | OWNED |
| JM-12 (§12) | Preset `jam` block (style_ref, link_rhythm_kit, seed); `.luthierjam` format; `JamStyleLibrary`, user styles folder; undo classes; accessibility (announcements, lane description, reduced motion) — not built | - | - | - | OWNED |
| JM-13 (§13) | Failure modes (bad style file, map >1024, no playhead, re-prepare, CPU relief step, NaN guard) — not built | - | n/a | - | OWNED |
| JM-14 (§14) | Budget: total <=1.7 units, "Jam" scenario <=10, 2 MB, 0 latency — not built | - | n/a | - | OWNED |
| JM-15 (§15) | Edition split Free/Pro (styles, kits, voices, tuning/damping, separate outs, MIDI, anticipation from edited tunes) — not built (no edition framework here) | - | - | - | OWNED |
| JM-16 (§16) | New classes and six `processBlock` insertion points — absent | - | n/a | - | OWNED |
| JM-T1 (§17 JM-01..23) | Engine/timing tests (styles, grid, tempo automation, determinism, block size, follow/grace/quanta, chord rules, slash, anticipation, prediction, start/stop modes, intensity, fills, meters, cycle jump) — none | - | - | - | OWNED |
| JM-T2 (§17 JM-24..32) | DSP tests (kick modes, snare wires, hat choke, ride, kit tuning, bass pitch/position, no file reads, long-run stability) — none | - | - | - | OWNED |
| JM-T3 (§17 JM-33..34) | RT alloc/lock and budget tests — none | - | - | - | OWNED |
| JM-T4 (§17 JM-35..41) | State, param order, routing, looper/kill switch, MIDI out, drag-out, bad style file — none | - | - | - | OWNED |
| JM-T5 (§17 JM-42..46) | Combination (tune percussion/bass, metronome, snapshot, rhythm grid) — none | - | - | - | OWNED |
| JM-T6 (§17 JM-47..51) | GUI xvfb (tab placement, pills, shortcuts, messages/a11y, Free locks) — none | - | - | - | OWNED |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=32 -->
