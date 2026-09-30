## slide-technique-controls.md

Re-verified against the current checkout: only the base slide from slide-guitar.md exists (`DSP/Slide/SlideEngine`, Slide Mode toggle in Advanced PERFORMANCE, `UI/SlideGroup` in CHARACTER with `slide_pressure`/`slide_slant`, `SlideEngine::startMove`, `Slide.*`/`SlideUi.*` tests). There is no SlideControlSettings, position source stack, speed limit, contact mask, auto-vibrato params, gesture keyswitch, SLIDE sub-tab, popover, presets or `SlideControls.*` test; the techniques-branch work never landed here. SL-3, SL-4, SL-7, SL-8 and SL-14 are PARTIAL and the rest MISSING (SL-T rows have no tests).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| SL-0 (§0.1, §3, §4) | slide-guitar.md physics and Slide Mode toggle unchanged when new fields untouched | `DSP/Slide/SlideEngine` | ADVANCED > PERFORMANCE "Slide guitar", `AdvancedPanel::slideGuitarToggle`; CHARACTER `SlideGroup` | `Slide.switchingModeMidNoteIsClean`, `SlideUi.theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | DONE |
| SL-1 (§0.3-0.4, §1) | Position source mod wheel / PB / MPE Y / expression / CC / fretboard drag | none (slide follows note pitch only; no position source stack) | none (no fretboard bar drag, no SLIDE sub-tab) | - | MISSING |
| SL-2 (§1) | Position mode absolute (0-1 -> fret 0-24) / relative | none: no SlideControlSettings/slide_pos_* params on this checkout | n/a | - | MISSING |
| SL-3 (§1) | Slant control (degrees), source selectable | `slide_slant` (`Parameters.cpp`, `SlideSettings::slantDegrees`); no source selector | `UI/SlideGroup` slant control (CHARACTER) | `Slide.slantGivesEachStringItsOwnInterval` | PARTIAL |
| SL-4 (§1) | Pressure 0-1, source selectable | `slide_pressure` (`SlideSettings::pressure`); no source selector | `UI/SlideGroup` pressure control | `SlideUi.pressureSaysWhatItMeans` | PARTIAL |
| SL-5 (§1) | Contact string mask all / bass 3 / treble 3 | none: no SlideControlSettings/slide_pos_* params on this checkout | n/a | - | MISSING |
| SL-6 (§1) | Speed limit 4800 c/s, advanced higher | none: no SlideControlSettings/slide_pos_* params on this checkout | n/a | - | MISSING |
| SL-7 (§1) | Auto-vibrato after 300 ms hold, depth/rate, off by default | `SlideEngine::vibratoCents` (bar vibrato exists); no auto-vibrato after a 300 ms hold or `slide_auto_vibrato` params | n/a | - | PARTIAL |
| SL-8 (§1-2) | Gesture trigger keyswitch/CC; SlideGesture (from, to, duration, curve, slant start/end, pressure) | `SlideEngine::startMove` (scripted from/to/duration move); no SlideGesture struct, curve, keyswitch/CC trigger | n/a | - | PARTIAL |
| SL-9 (§2) | Gesture triggered by MIDI meta or tune-builder — absent on owner branch (keyswitch/CC only) | none: no SlideControlSettings/slide_pos_* params on this checkout | n/a | - | MISSING |
| SL-10 (§3) | setPositionSource / triggerGesture / setSpeedLimit on SlideEngine | none: no SlideControlSettings/slide_pos_* params on this checkout | n/a | - | MISSING |
| SL-11 (§4) | Techniques > Slide sub-tab | none: no SlideControlSettings/slide_pos_* params on this checkout | none (no Techniques tab or SLIDE sub-tab) | - | MISSING |
| SL-12 (§4) | Easy slide glyph "…" popover (source + mode) — SLIDE pill hold popover | none: no SlideControlSettings/slide_pos_* params on this checkout | none (no slide popover) | - | MISSING |
| SL-13 (§4) | Advanced Col 3 SLIDE group expandable section — mirror at CHARACTER foot | none: no SlideControlSettings/slide_pos_* params on this checkout | `UI/SlideGroup` only (base controls); no expandable control-source section | - | MISSING |
| SL-14 (§5) | Cascade: compatible with mute, bends, scrape (bar lifted); not tap on contacted strings, not slap | `LuthierEngine.cpp` ~2596 (slide holds its strings), `SlapEngine::classify(e, underBar)` refuses slap under the bar; no CascadeResolver | n/a | `SlapWiring.slapAndScrapeTakeTheStringFromEachOther` (scrape only) | PARTIAL |
| SL-15 (§6) | Presets Standard (Mod Wheel), Pitch-Bend, Lap Steel Full Control, Auto-Vibrato Hold | none: no SlideControlSettings/slide_pos_* params on this checkout | n/a | - | MISSING |
| SL-T1 (§7) | Test: mod wheel drives position | none (feature absent) | n/a | - | MISSING |
| SL-T2 (§7) | Test: speed limit clamps a jump | none (feature absent) | n/a | - | MISSING |
| SL-T3 (§7) | Test: scripted gesture within ±5 ms | none (feature absent) | n/a | - | MISSING |
| SL-T4 (§7) | Test: auto-vibrato after 300 ms | none (feature absent) | n/a | - | MISSING |
| SL-T5 (§7) | Test: source swap crossfades 10 ms | none (feature absent) | n/a | - | MISSING |
| SL-T6 (§7) | Test: bass-only mask leaves treble free | none (feature absent) | n/a | - | MISSING |
| SL-T7 (§7) | Test: preset round-trips every control | none (feature absent) | n/a | - | MISSING |

<!-- counts DONE=1 NO-GUI=0 NO-TEST=0 PARTIAL=5 MISSING=17 OWNED=0 -->
