## auto-articulation.md

Performance Assist does not exist in the compiled tree: no `AutoArticulator`, style table, feed, `aa_*` parameters, PLAYING group, AUTO pill, labels or AA tests. What exists is only the substrate it would plug into: `RubricVoicer::setPreferredPosition/voiceSingleNote`, `TechniqueEngine::decide` (with its own legato/slide inference) and `slideDurationFor`, `StrumRequest`/`StrumGesture`, `MidiInterpreter::flushChordGroup/emitVoicedNote/StringSlot::releaseDueAt`, `LuthierEngine::updatePerBlockModulation/ScheduledEvent`, `PerformanceCapture`. All rows MISSING (re-verified).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| AA-0 (§0) | Ground rules: off = bit-identical, own hash (no `rng` draws), explicit wins, zero added latency, deterministic/sample-accurate, never harmonics/tap/slide-guitar/muted-pick, RT-safe, visible+captured | - (no `AutoArticulator`; substrate `TechniqueEngine::decide` has its own legato/slide inference) | - | - | MISSING |
| AA-2 (§2) | 8 styles x constants table, Amount scaling of windows/depths/probabilities, legato chain caps, `constexpr` rows in `AutoArticulationStyles.cpp`, family cross-use | - | - | - | MISSING |
| AA-3.1 (§3.1) | Position: hand-tracker box cost function, tie rules, `setPreferredPosition(H)` before chord voicing, late-join roll | - (`RubricVoicer::setPreferredPosition` exists as substrate) | n/a | - | MISSING |
| AA-3.2 (§3.2) | Legato hammer-on/pull-off from stamped history, velocity guard, chain cap, deferred note-off hand-over, fretless -> slide | - | n/a | - | MISSING |
| AA-3.3 (§3.3) | Slide rule (overlap, interval, position shift 4-7 st, precedence) | - | n/a | - | MISSING |
| AA-3.4 (§3.4) | Delayed vibrato on lead notes, 300 ms ramp, ±4% hashed rate, `max(ccTarget, autoVibratoCents)` in `updatePerBlockModulation`, 150 ms ramp-out | - | n/a | - | MISSING |
| AA-3.5 (§3.5) | Attack accent/soft excitation scaling, velocity unchanged | - | n/a | - | MISSING |
| AA-3.6 (§3.6) | Palm-mute rule (chug, repeated pitch, Metal first note) + scheduled mute lift | - | n/a | - | MISSING |
| AA-3.7 (§3.7) | Alternate picking (grid/toggle), up-stroke scaling; chord strum direction/speed/force via `StrumRequest`, Fingerstyle/Bass exceptions | - (`StrumRequest`/`StrumGesture` substrate) | n/a | - | MISSING |
| AA-3.8 (§3.8) | Ornaments: bend-into, slide-in, fall via `releaseDueAt` | - | n/a | - | MISSING |
| AA-4.1 (§4.1) | New files `AutoArticulator`, `AutoArticulationStyles`, `AutoArticulationFeed` (SPSC) and API | - | n/a | - | MISSING |
| AA-4.2 (§4.2) | `NoteOnEvent` new fields; `TechniqueEngine::setLegatoInferenceEnabled` + `explicitOut`; interpreter hooks; engine `ExplicitContext`, `dampingLift`, excitation fields; `ParameterBridge` settings + Free resolution | - (`Model/Playing/PlayingEvents.h` NoteOnEvent unchanged) | n/a | - | MISSING |
| AA-5 (§5) | Explicit-context table (Guitar Controller/MPE bypass, rhythm driving, controller techniques, slap/scrape/tap, mute grid, CC vibrato/bend, strum CC, `aa=pre` imports) | - | - | - | MISSING |
| AA-6 (§6) | Params `aa_enabled`/`aa_style`/`aa_amount`/`aa_rules` appended last, automatable, morph rules | - (no `aa_*` in Parameters.cpp/h) | - | - | MISSING |
| AA-7.1 (§7.1) | Easy Playing strip: AUTO pill (flash dot, hold popover with Amount) + style combo | - | - (`UI/EasyPanel.h` has only playingModeSelector) | - | MISSING |
| AA-7.2 (§7.2) | Advanced RHYTHM tab first group PLAYING (`UI/PerformanceAssistGroup`), mode mirror, rule checkboxes via bitmask adapter, recent list, notice line, collapsible; CASCADE "Auto" row | - | - | - | MISSING |
| AA-7.3 (§7.3) | "Show what it did" fretboard/illustration labels (H, P, /, \, ~, PM, ↑, >, b½, ↘, strum arrow), 600 ms fade, reduced-motion rule, feed drained at 30 Hz | - | - | - | MISSING |
| AA-7.4 (§7.4) | Options Visual aids "Show Performance Assist labels" (UiPreferences, default on) | - | - | - | MISSING |
| AA-7.5 (§7.5) | `A` rebindable shortcut, HelpContent "performance-assist" topic + `?`, empty list text, Free upsell | - | - | - | MISSING |
| AA-7.6 (§7.6) | gui-integration 19 feature-index row | - | n/a | - | MISSING |
| AA-8 (§8) | State (params in presets/snapshots/host, old presets load off), `UiState::playingGroupCollapsed`, undo classes 3.3/3.2/3.1, accessibility names/list/tab order | - | - | - | MISSING |
| AA-9 (§9) | Capture marks (palm mute, vibrato, accent, bend-into, slideIn/Out, new `pickStrokeUp/Down` ScoreTechnique types -> MusicXML up/down-bow, GP pickstroke), `CapturedNote::autoRules`, secondary-accent drawing, Luthier-profile `aa=` field | - | - | - | MISSING |
| AA-10 (§10) | Budget 0.02 units / 3 µs per note-on, <4 KB, +0 latency, overlay within 2 ms | - | n/a | - | MISSING |
| AA-11 (§11) | Editions: Free styles Clean/Rock/Fingerstyle/Bass, `aa_rules` non-automatable "(Pro)" effective 511, Pro style fallback mapping + write-back | - | - | - | MISSING |
| AA-12 (§12) | Interactions (humanize after, strum dynamics, rhythm engine, Tune direct notes, piano roll ghost dots, cascade priority, Slide Mode, preset `reset()`, capo/tuning/12-string) | - | n/a | - | MISSING |
| AA-13 (§13) | Failure modes (unplayable -> voicer fallback, no extra events, contradictions -> Pluck, lift, fall cancel) | - | n/a | - | MISSING |
| AA-T1 (§14 AA-01..03) | Off bit-identical, no rng draws, latency unchanged | - | n/a | - | MISSING |
| AA-T2 (§14 AA-04..23) | Rule unit tests (legato, slide, re-pick, hand-over, chain cap, position, vibrato, attack, palm mute + lift, alternate/strum, late join, bend-into, fall, no forbidden techniques) | - | n/a | - | MISSING |
| AA-T3 (§14 AA-24..31) | Explicit-wins, controller/MPE, rhythm/tune, slap/tap, block-size independence, RT/offline identity, no alloc, cost | - | n/a | - | MISSING |
| AA-T4 (§14 AA-32..36) | Capture/MusicXML/`aa=`, round-trip null, param layout/order, state round trip, Free fallback | - | n/a | - | MISSING |
| AA-T5 (§14 AA-37..42) | GUI (pill, PLAYING group, labels, a11y), undo, combination sweep | - | n/a | - | MISSING |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=31 DEFERRED=0 -->
