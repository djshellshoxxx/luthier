## slide-technique-controls.md

This checkout has only the base slide from slide-guitar.md (`DSP/Slide/SlideEngine`, Slide Mode toggle in Advanced PERFORMANCE, `UI/SlideGroup` in CHARACTER, `Slide.*`/`SlideUi.*` tests), so only the "base behaviour unchanged" row is DONE. The techniques branch has the work: `SlideControlSettings`, `SlideEngine::advanceControls/triggerGesture`, ~25 `slide_pos_*/slide_gesture_*/slide_auto_vib*` params, SLIDE sub-tab, SLIDE pill popover, CHARACTER mirror, four presets and 8 `SlideControls.*` tests (spot-checked `SlideEngine.h` SlideGesture, KS 21, gesture params and the preset names). Owner gaps: gestures can be triggered only by keyswitch/CC (no tune-builder or MIDI-meta trigger); the Advanced mirror is at the foot of CHARACTER rather than an expandable section of the SLIDE group; the Easy "…" is the SLIDE pill's hold popover (recorded decision).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| SL-0 (§0.1, §3, §4) | slide-guitar.md physics and Slide Mode toggle unchanged when new fields untouched | `DSP/Slide/SlideEngine` | ADVANCED > PERFORMANCE "Slide guitar", `AdvancedPanel::slideGuitarToggle`; CHARACTER `SlideGroup` | `Slide.switchingModeMidNoteIsClean`, `SlideUi.theSlideGroupAppearsWithSlideModeAndTheTabFitsIt` | DONE |
| SL-1 (§0.3-0.4, §1) | Position source mod wheel / PB / MPE Y / expression / CC / fretboard drag | (branch) `SlideControlSettings`, `SlideEngine::advanceControls`; `TechniqueOverlay::handleMouseDrag` | (branch) TECHNIQUES > SLIDE, `SlidePage` | (branch) `SlideControls.theModWheelDrivesThePosition`, `SlideControls.theStringFollowsTheControlledBar` | OWNED |
| SL-2 (§1) | Position mode absolute (0-1 -> fret 0-24) / relative | (branch) `SlideEngine::controlledBarFret`, `slide_pos_mode/_range` | (branch) SLIDE page | (branch) `SlideControls.theModWheelDrivesThePosition` | OWNED |
| SL-3 (§1) | Slant control (degrees), source selectable | (branch) `slide_slant_source/_cc` | (branch) SLIDE page | (branch) `SlideControls.everyControlRoundTrips` | OWNED |
| SL-4 (§1) | Pressure 0-1, source selectable | (branch) `slide_pressure_source/_cc` | (branch) SLIDE page | (branch) `SlideControls.everyControlRoundTrips` | OWNED |
| SL-5 (§1) | Contact string mask all / bass 3 / treble 3 | (branch) `SlideEngine::contactsString`, `slide_contact` | (branch) SLIDE page | (branch) `SlideControls.aBassOnlyBarLeavesTheTrebleFree` | OWNED |
| SL-6 (§1) | Speed limit 4800 c/s, advanced higher | (branch) `advanceTowards`, `slide_speed_limit`, `PhysicalRange` row | (branch) SLIDE page | (branch) `SlideControls.theSpeedLimitClampsAJump` | OWNED |
| SL-7 (§1) | Auto-vibrato after 300 ms hold, depth/rate, off by default | (branch) `advanceControls`, `slide_auto_vibrato/_vib_depth/_vib_rate` | (branch) SLIDE page | (branch) `SlideControls.autoVibratoEngagesAfterAHold` | OWNED |
| SL-8 (§1-2) | Gesture trigger keyswitch/CC; SlideGesture (from, to, duration, curve, slant start/end, pressure) | (branch) `SlideEngine::triggerGesture`, `SlideGesture`, KS 21 | (branch) SLIDE page | (branch) `SlideControls.aScriptedGestureArrivesOnTime` | OWNED |
| SL-9 (§2) | Gesture triggered by MIDI meta or tune-builder — absent on owner branch (keyswitch/CC only) | - | n/a | - | OWNED |
| SL-10 (§3) | setPositionSource / triggerGesture / setSpeedLimit on SlideEngine | (branch) `SlideEngine` | n/a | (branch) `SlideControls.*` | OWNED |
| SL-11 (§4) | Techniques > Slide sub-tab | (branch) - | (branch) `TechniquePages:SlidePage` | (branch) `TechniquesUi.everySubTabRendersItsControls` | OWNED |
| SL-12 (§4) | Easy slide glyph "…" popover (source + mode) — SLIDE pill hold popover | (branch) - | (branch) `TechniquePopover` | (branch) `TechniquesUi.holdOpensThePopoverAndEscapeClosesIt` | OWNED |
| SL-13 (§4) | Advanced Col 3 SLIDE group expandable section — mirror at CHARACTER foot | (branch) - | (branch) `TechniqueMirrors` | (branch) `TechniquesUi.theCharacterMirrorsAttachTheSameParameters` | OWNED |
| SL-14 (§5) | Cascade: compatible with mute, bends, scrape (bar lifted); not tap on contacted strings, not slap | (branch) `CascadeResolver` | n/a | (branch) `Tap.theSlideBarHoldsItsStrings`, `Cascade.theMatrixIsTheSpecs` | OWNED |
| SL-15 (§6) | Presets Standard (Mod Wheel), Pitch-Bend, Lap Steel Full Control, Auto-Vibrato Hold | (branch) `Presets/TechniquePresets.cpp` | (branch) browser | (branch) `TechniquesUi.thePresetChipFilters` | OWNED |
| SL-T1 (§7) | Test: mod wheel drives position | (branch) | n/a | (branch) `SlideControls.theModWheelDrivesThePosition` | OWNED |
| SL-T2 (§7) | Test: speed limit clamps a jump | (branch) | n/a | (branch) `SlideControls.theSpeedLimitClampsAJump` | OWNED |
| SL-T3 (§7) | Test: scripted gesture within ±5 ms | (branch) | n/a | (branch) `SlideControls.aScriptedGestureArrivesOnTime` | OWNED |
| SL-T4 (§7) | Test: auto-vibrato after 300 ms | (branch) | n/a | (branch) `SlideControls.autoVibratoEngagesAfterAHold` | OWNED |
| SL-T5 (§7) | Test: source swap crossfades 10 ms | (branch) | n/a | (branch) `SlideControls.swappingTheSourceGlides` | OWNED |
| SL-T6 (§7) | Test: bass-only mask leaves treble free | (branch) | n/a | (branch) `SlideControls.aBassOnlyBarLeavesTheTrebleFree` | OWNED |
| SL-T7 (§7) | Test: preset round-trips every control | (branch) | n/a | (branch) `SlideControls.everyControlRoundTrips` | OWNED |

<!-- counts DONE=1 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=22 -->
