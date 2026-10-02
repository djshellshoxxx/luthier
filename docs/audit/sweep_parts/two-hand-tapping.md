## two-hand-tapping.md

Re-verified against the current checkout: none of the tapping feature is present. There is no TapEngine, no `tap_*` parameter, no tap trigger, TAP page, pill, mirror, markers or presets, and no `Tap.*` test; the techniques-branch work never landed here. Only the legato hammer-on/pull-off promotion in `Model/Playing/TechniqueEngine` (`legato_window` 40 ms, velocity threshold 0.63), the `Technique::Tap` event kind and the tap-preempts-scrape/slap hooks in `LuthierEngine::triggerNote` exist, so TH-2, TH-8 and TH-15 are PARTIAL and the rest MISSING.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| TH-1 (§0, §1) | TapGesture (string, fret, strength, hand, pull-off-after, target, duration) | none (no TapEngine/TapGesture; only the `Technique::Tap` event kind in `Model/Playing/PlayingEvents.h`) | n/a | - | MISSING |
| TH-2 (§2) | Tap-on impulse + contact; tap-hold = movable capo; tap-off back to fretted pitch | `Technique::Tap` -> `Excitation::Kind::Tap` (`LuthierEngine.cpp` ~1435), `TechniqueEngine::setTapTrigger`; no movable-capo hold or tap-off | n/a | `Technique.controllersTakePriorityOverInference` (tap trigger only) | PARTIAL |
| TH-3 (§2) | Pull-off lateral flick excitation on release | none: no TapEngine/tap_* params on this checkout | n/a | - | MISSING |
| TH-4 (§2) | Multi-finger: concurrent taps same/different strings | none: no TapEngine/tap_* params on this checkout | n/a | - | MISSING |
| TH-5 (§3) | Trigger source MIDI ch 2 (default) / keyswitch / fretboard tap layer | none (`TechniqueEngine::setTapTrigger` only; no `TapSettings`, no ch-2/keyswitch 19 tap trigger) | none (no TAP page, no fretboard tap layer in `TechniqueOverlay`) | - | MISSING |
| TH-6 (§3) | Tap strength curve (default linear) | none: no TapEngine/tap_* params on this checkout | n/a | - | MISSING |
| TH-7 (§3) | Auto pull-off, default on | none: no TapEngine/tap_* params on this checkout | n/a | - | MISSING |
| TH-8 (§3, §5) | LH hammer-on threshold (40) and 150 ms promotion window; pull-off on revealing note-off | `TechniqueEngine::setLegatoWindowMs` (default 40 ms) / `setLegatoVelocityThreshold` (0.63): legato hammer-on/pull-off exists but not the spec 150 ms window / user threshold; no `tap_hammer_threshold` | n/a | - | PARTIAL |
| TH-9 (§3) | Lateral flick 0.5, duration 200 ms, max concurrent 2 (adv 8), fret snap on | none: no TapEngine/tap_* params on this checkout | n/a | - | MISSING |
| TH-10 (§4) | TapEngine trigger/release/processBlock/reset, inserted after TechniqueEngine before StringEngine | none: no TapEngine/tap_* params on this checkout | n/a | - | MISSING |
| TH-11 (§6) | Techniques > Tapping sub-tab, all controls | none: no TapEngine/tap_* params on this checkout | none (no Techniques tab or Tapping sub-tab in Source/UI) | - | MISSING |
| TH-12 (§6) | Easy Playing strip Tap pill arms next note as tap | none (no `tap_armed`) | none (no Tap pill in Easy strip) | - | MISSING |
| TH-13 (§6) | CHARACTER Right Hand group "Tapping" section (curve, flick, auto pull-off) — mirror sits at CHARACTER foot, not in a Right Hand group | none: no TapEngine/tap_* params on this checkout | none (no Tapping section in CHARACTER) | - | MISSING |
| TH-14 (§6) | Fretboard square tap markers, released fade 100 ms | none: no TapEngine/tap_* params on this checkout | none (no tap markers in `FretboardComponent`) | - | MISSING |
| TH-15 (§7) | Cascade: mute/bend compatible; slide conflicts on contacted strings; slap alternate; scrape conflicts | `LuthierEngine::triggerNote` (Technique::Tap preempts scrape and slap on its string), `SlapEngine::classify` refuses Tap; no CascadeResolver, slide-conflict or mute/bend compat | n/a | `Scrape.aPreemptedScrapeFadesOutInTenMilliseconds` | PARTIAL |
| TH-16 (§8) | Five presets (Standard Two-Hand, Legato Runs, Eight-Finger, Microtonal, Percussive) — Eight-Finger at 4 not 8 | none: no TapEngine/tap_* params on this checkout | n/a | - | MISSING |
| TH-17 (§9) | MIDI export of taps (SysEx hand/strength/pull-off; generic ch 2) — deferred on owner branch | none: no TapEngine/tap_* params on this checkout | n/a | - | MISSING |
| TH-T1 (§10) | Test: tap 12 over fret 5, returns with small transient | none (feature absent) | n/a | - | MISSING |
| TH-T2 (§10) | Test: flick 1.0 transient; auto pull-off off none | none (feature absent) | n/a | - | MISSING |
| TH-T3 (§10) | Test: hammer-on without pick transient | none (feature absent) | n/a | - | MISSING |
| TH-T4 (§10) | Test: two taps = capos in series | none (feature absent) | n/a | - | MISSING |
| TH-T5 (§10) | Test: fret snap off 12.3 sharp | none (feature absent) | n/a | - | MISSING |
| TH-T6 (§10) | Test: CPU idle < 0.05 %, busy < 0.8 % | none (feature absent) | n/a | - | MISSING |
| TH-T7 (§10) | Test: preset round-trips every field | none (feature absent) | n/a | - | MISSING |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=3 MISSING=21 OWNED=0 -->
