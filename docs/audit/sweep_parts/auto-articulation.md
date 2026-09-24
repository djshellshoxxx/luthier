## auto-articulation.md

Performance Assist does not exist on this checkout: no `AutoArticulator`, style table, feed, `aa_*` parameters, PLAYING group, AUTO pill, labels or AA tests. What exists is the substrate it plugs into: `RubricVoicer::setPreferredPosition/voiceSingleNote`, `TechniqueEngine::decide` (with its own legato/slide inference) and `slideDurationFor`, `StrumRequest`/`StrumGesture`, `MidiInterpreter::flushChordGroup/emitVoicedNote/StringSlot::releaseDueAt`, `LuthierEngine::updatePerBlockModulation/ScheduledEvent`, `PerformanceCapture`. No FEAT branch pushed yet; all rows OWNED.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| AA-0 (§0) | Ground rules: off = bit-identical, own hash (no `rng` draws), explicit wins, zero added latency, deterministic/sample-accurate, never harmonics/tap/slide-guitar/muted-pick, RT-safe, visible+captured — not built | - | - | - | OWNED |
| AA-2 (§2) | 8 styles x constants table, Amount scaling of windows/depths/probabilities, legato chain caps, `constexpr` rows in `AutoArticulationStyles.cpp`, family cross-use — not built | - | - | - | OWNED |
| AA-3.1 (§3.1) | Position: hand-tracker box cost function, tie rules, `setPreferredPosition(H)` before chord voicing, late-join roll — not built (voicer API exists `Model/Playing/RubricVoicer`) | - | n/a | - | OWNED |
| AA-3.2 (§3.2) | Legato hammer-on/pull-off from stamped history, velocity guard, chain cap, deferred note-off hand-over, fretless -> slide — not built | - | n/a | - | OWNED |
| AA-3.3 (§3.3) | Slide rule (overlap, interval, position shift 4-7 st, precedence) — not built | - | n/a | - | OWNED |
| AA-3.4 (§3.4) | Delayed vibrato on lead notes, 300 ms ramp, ±4% hashed rate, `max(ccTarget, autoVibratoCents)` in `updatePerBlockModulation`, 150 ms ramp-out — not built | - | n/a | - | OWNED |
| AA-3.5 (§3.5) | Attack accent/soft excitation scaling, velocity unchanged — not built | - | n/a | - | OWNED |
| AA-3.6 (§3.6) | Palm-mute rule (chug, repeated pitch, Metal first note) + scheduled mute lift — not built | - | n/a | - | OWNED |
| AA-3.7 (§3.7) | Alternate picking (grid/toggle), up-stroke scaling; chord strum direction/speed/force via `StrumRequest`, Fingerstyle/Bass exceptions — not built | - | n/a | - | OWNED |
| AA-3.8 (§3.8) | Ornaments: bend-into, slide-in, fall via `releaseDueAt` — not built | - | n/a | - | OWNED |
| AA-4.1 (§4.1) | New files `AutoArticulator`, `AutoArticulationStyles`, `AutoArticulationFeed` (SPSC) and API — absent | - | n/a | - | OWNED |
| AA-4.2 (§4.2) | `NoteOnEvent` new fields; `TechniqueEngine::setLegatoInferenceEnabled` + `explicitOut`; interpreter hooks; engine `ExplicitContext`, `dampingLift`, excitation fields; `ParameterBridge` settings + Free resolution — none present (`PlayingEvents.h:48` unchanged) | - | n/a | - | OWNED |
| AA-5 (§5) | Explicit-context table (Guitar Controller/MPE bypass, rhythm driving, controller techniques, slap/scrape/tap, mute grid, CC vibrato/bend, strum CC, `aa=pre` imports) — not built | - | - | - | OWNED |
| AA-6 (§6) | Params `aa_enabled`/`aa_style`/`aa_amount`/`aa_rules` appended last, automatable, morph rules — absent (last param today `aux1_pre_circuit`, `Parameters.cpp` ~791) | - | - | - | OWNED |
| AA-7.1 (§7.1) | Easy Playing strip: AUTO pill (flash dot, hold popover with Amount) + style combo — absent (`UI/EasyPanel.h:118 playingModeSelector` only) | - | - | - | OWNED |
| AA-7.2 (§7.2) | Advanced RHYTHM tab first group PLAYING (`UI/PerformanceAssistGroup`), mode mirror, rule checkboxes via bitmask adapter, recent list, notice line, collapsible; CASCADE "Auto" row — absent | - | - | - | OWNED |
| AA-7.3 (§7.3) | "Show what it did" fretboard/illustration labels (H, P, /, \, ~, PM, ↑, >, b½, ↘, strum arrow), 600 ms fade, reduced-motion rule, feed drained at 30 Hz — not built | - | - | - | OWNED |
| AA-7.4 (§7.4) | Options Visual aids "Show Performance Assist labels" (UiPreferences, default on) — absent | - | - | - | OWNED |
| AA-7.5 (§7.5) | `A` rebindable shortcut, HelpContent "performance-assist" topic + `?`, empty list text, Free upsell — absent | - | - | - | OWNED |
| AA-7.6 (§7.6) | gui-integration 19 feature-index row — not added | - | n/a | - | OWNED |
| AA-8 (§8) | State (params in presets/snapshots/host, old presets load off), `UiState::playingGroupCollapsed`, undo classes 3.3/3.2/3.1, accessibility names/list/tab order — not built | - | - | - | OWNED |
| AA-9 (§9) | Capture marks (palm mute, vibrato, accent, bend-into, slideIn/Out, new `pickStrokeUp/Down` ScoreTechnique types -> MusicXML up/down-bow, GP pickstroke), `CapturedNote::autoRules`, secondary-accent drawing, Luthier-profile `aa=` field — not built | - | - | - | OWNED |
| AA-10 (§10) | Budget 0.02 units / 3 µs per note-on, <4 KB, +0 latency, overlay within 2 ms — not built | - | n/a | - | OWNED |
| AA-11 (§11) | Editions: Free styles Clean/Rock/Fingerstyle/Bass, `aa_rules` non-automatable "(Pro)" effective 511, Pro style fallback mapping + write-back — not built | - | - | - | OWNED |
| AA-12 (§12) | Interactions (humanize after, strum dynamics, rhythm engine, Tune direct notes, piano roll ghost dots, cascade priority, Slide Mode, preset `reset()`, capo/tuning/12-string) — not built | - | n/a | - | OWNED |
| AA-13 (§13) | Failure modes (unplayable -> voicer fallback, no extra events, contradictions -> Pluck, lift, fall cancel) — not built | - | n/a | - | OWNED |
| AA-T1 (§14 AA-01..03) | Off bit-identical, no rng draws, latency unchanged — none | - | - | - | OWNED |
| AA-T2 (§14 AA-04..23) | Rule unit tests (legato, slide, re-pick, hand-over, chain cap, position, vibrato, attack, palm mute + lift, alternate/strum, late join, bend-into, fall, no forbidden techniques) — none | - | - | - | OWNED |
| AA-T3 (§14 AA-24..31) | Explicit-wins, controller/MPE, rhythm/tune, slap/tap, block-size independence, RT/offline identity, no alloc, cost — none | - | - | - | OWNED |
| AA-T4 (§14 AA-32..36) | Capture/MusicXML/`aa=`, round-trip null, param layout/order, state round trip, Free fallback — none | - | - | - | OWNED |
| AA-T5 (§14 AA-37..42) | GUI (pill, PLAYING group, labels, a11y), undo, combination sweep — none | - | - | - | OWNED |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=31 -->
