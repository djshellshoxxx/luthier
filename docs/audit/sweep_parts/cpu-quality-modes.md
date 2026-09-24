## cpu-quality-modes.md

CPU quality modes do not exist on this checkout: no `QualityProfile`, `PerformanceSettings`/`performance.json`, `CpuLoadMonitor`, `QualityController`, `AnimationPolicy`, `QualityBadge`, `Oversampler::latencyFor`, latency pads, IR variants, string sleep or load governor, and no CQ tests. The existing hooks are `StringEngine::activeDispersionStages` + `filterDelayCompensation`, `NoiseEngine::setDegraded` (unit-tested in NoiseTests but never called by the engine), `LuthierEngine::cpuEstimate`, `AccessibilitySettings::getAnimationMs`, and the `oversampling` parameter with its Options AUDIO and Advanced controls. That parameter must keep its id, range and position, which it does today (the one DONE row). No FEAT branch pushed yet; everything else is OWNED.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| CQ-3c (§3) | `oversampling` parameter keeps id/range/default/position with visible controls | `Parameters.h:271 ParamIDs::oversample` | Options > AUDIO `OptionsPages.cpp:554`; ADVANCED Col 3 `AdvancedPanel.cpp:945` | `GuiReach::everyAutomatableParameterHasAVisibleControl`, `GuiReach::operatingEachControlWritesItsParameter` | DONE |
| CQ-0 (§0) | Ground rules: never in presets, no pitch/timing/latency change, bounded audible cost, offline at High, identity rules, RT-safe prepared alternatives — not built | - | - | - | OWNED |
| CQ-2.1 (§2.1) | `QualityProfile` table (amp/drive OS caps, dispersion stages, body IR/modal caps, cab IR caps, room taps, noise pools, mod interval, idle sleep, ring-out, UI motion, readout rate) — not built | - | n/a | - | OWNED |
| CQ-2.2 (§2.2) | Effective vs nominal oversampling, `setOversamplingFactor(eff, nom)` + `LatencyPad`, `Oversampler::latencyFor`, constant reported latency, dual-oversampler 10 ms crossfade (also fixes today's `setFactor`->`reset` click) — not built (`DSP/Common/Oversampler.h:117 setFactor` still resets) | - | n/a | - | OWNED |
| CQ-2.3 (§2.3) | Convolution truncation variants per level (-40 dB tail rule, 50 ms fade, same partition latency, 20 ms switch crossfade, ≤12 MB) — not built | - | n/a | - | OWNED |
| CQ-2.4 (§2.4) | String savings: dispersion cap latched at `excite()`, idle-string sleep keeping coupling, ring-out truncation with exemptions — not built | - | n/a | - | OWNED |
| CQ-2.5 (§2.5) | Switching via atomic + `LuthierEngine::applyQuality`, hard-switch conditions, 20 ms ramps — not built | - | n/a | - | OWNED |
| CQ-2.6 (§2.6) | Offline at High (Auto always High offline), `setNonRealtime` override, `AudioExporter` sets non-realtime, previews/audition/SpectrumDelta at High — not built | - | - | - | OWNED |
| CQ-2.7 (§2.7) | Auto: own-share load, step-down/up rules, dwell, anti-oscillation hold, start level, 10 Hz controller, banner/announcement — not built | - | - | - | OWNED |
| CQ-3 (§3) | `performance.json` global prefs (`PerformanceSettings` singleton broadcast), `UiState::qualityOverride`, not in presets, no parameter added — not built | - | - | - | OWNED |
| CQ-4 (§4) | New files and engine insertion points (processBlock stamping, `applyQuality` fan-out to amp/pedals/strings/body/cab/room/noise/mod matrix, `applyStructural` passes nominal, `getAnimationMs` delegates) — not built (`NoiseEngine::setDegraded` exists but is uncalled) | - | n/a | - | OWNED |
| CQ-5 (§5) | Options AUDIO QUALITY section (radio pills, override combo, live load, toggles, capped-oversampling note, disclosure); AppearancePage Low note; DIAGNOSTICS `emergency_string_drop` + path/debug additions; footer `QualityBadge` replacing CPU text; "Cycle CPU quality" shortcut; empty states — absent | - | - | - | OWNED |
| CQ-6 (§6) | `AnimationPolicy` singleton (motion table, API, `Registration` RAII, `notePaint`, poll-only allow-list) and per-component behaviour at Off — absent | - | - | - | OWNED |
| CQ-7 (§7) | Load governor E1/E2/E3 (display relief, emergency string drop on audio thread, banner, opt-out) superseding performance-budget 8 — not built | - | - | - | OWNED |
| CQ-8 (§8) | CPU targets and ratio gates (Medium ≤0.85x, Low ≤0.70x High), message-thread paint -60% at Low, switch cost — not built | - | n/a | - | OWNED |
| CQ-9 (§9) | Not undoable; a11y radio group, badge last in footer, rate-limited announcements, catalogue strings; both editions identical — not built | - | - | - | OWNED |
| CQ-10 (§10) | Interactions (presets/snapshots never touch mode, timing identical, automation below cap, SpectrumDelta High, tone-match full IRs, routing pad, multi-instance, golden renders) — not built | - | n/a | - | OWNED |
| CQ-11 (§11) | Failure modes — not built | - | n/a | - | OWNED |
| CQ-T1 (§12 CQ-01..10) | Profile, settings file, not-in-presets, override, OS cap, constant latency, latency accuracy, pitch, timing, dispersion latch — none | - | - | - | OWNED |
| CQ-T2 (§12 CQ-11..20) | Click-free switching, factory matrix, scenario budgets, offline at High, aliasing, body/IR, Auto, notification, governor, RT safety — none | - | - | - | OWNED |
| CQ-T3 (§12 CQ-21..27) | AnimationPolicy truth table, registry scan, Low no repaints, transitions, reduced-motion independence, UI, accessibility — none | - | - | - | OWNED |
| CQ-T4 (§12 CQ-28..32) | Multi-instance, edition, combinations, memory, golden renders at Low/Auto — none | - | - | - | OWNED |

<!-- counts DONE=1 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=21 -->
