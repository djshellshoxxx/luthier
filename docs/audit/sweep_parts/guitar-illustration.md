## guitar-illustration.md

The renderer on this checkout is substantial and unchanged since the earlier audit (`UI/Guitar/GuitarRenderer.*`, `BodyOutlines.h` 34 styles, `HeadstockOutlines.h`; 11 `GuitarIllustration.*` tests plus the `Workshop*` suites): procedural scene with spec-hash cache, z-order, string colours, family switch with banner and preserved state, 10 000-click hit test and the bench's pickup/height drags are DONE. The visual branch has work (e401027, 2026-09-24): thumbnails on a worker with a 200-entry cache, 60 ms note dots, 250 ms crossfade / reduced-motion outline, Ctrl-scroll 4x zoom test, reverse headstocks, per-string override rendering, nut-slot/pick/slide/capo drags, the 0.8 mm height stop, family-aware amp defaults and the auditioned-body tint (spot-checked `IllustrationRemainderTests.cpp`, `WorkshopRemainderTests.cpp`, `GuitarRenderer.h` diff). Owner gaps (no evidence on visual): body parts still carry no `illustration.body_style` so swapping a body never changes the outline (ground rule 5) and 14 outlines are unreachable; no finish / hardware-colour editor anywhere; no buzz-heatmap or pickup-pulse layer; drawer drag-and-drop, Shift+click-below, `pole_spacing_mm`, pinless / bass-mute / ferrule / pearloid drawing, and several untested drawing rows.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| GI-1 (§0.1) | Procedurally drawn from GuitarSpec; one renderer for Easy and Workshop | `UI/Guitar/GuitarRenderer::build` | Easy illustration `GuitarBodyComponent`; ADVANCED > WORKSHOP `WorkshopPanel` | `GuitarIllustration.everyFactoryGuitarHasItsParts` | DONE |
| GI-2 (§0.2) | Flat language — superseded by approved visual-polish lighting (DECISIONS); High contrast flat | `GuitarRenderer::buildLighting` | illustration | `GuitarIllustration.highContrastHasNoLighting` | DONE |
| GI-3 (§0.3, §0.5) | Every part a first-class visual; drawn = loaded — body swap does not change the outline (parts lack `illustration.body_style`), headstock/inlay follow body style not neck/fretboard part | `GuitarRenderer::resolveStyle` | WORKSHOP drawer | `GuitarIllustration.theKeyChangesWithEveryVisibleChange` (pickups/bridge) | OWNED |
| GI-4 (§0.4, §3) | Family switch fully rebuilds; five families | `PartLibrary::switchFamily`, `LuthierAudioProcessor::switchGuitarFamily` | WORKSHOP drawer "Guitar" category | `GuitarIllustration.aFamilySwitchGivesTheTargetFamilysGuitar`, `WorkshopPanel.theGuitarCategorySwitchesFamily` | DONE |
| GI-5 (§3) | `extended` family (7/8, fanned) — deliberately absent (DECISIONS "Family templates") | - | - | - | OWNED |
| GI-6 (§0.6) | Three feedbacks: visual, inspector, audible | `WorkshopBench`, `SpectrumDelta` | WORKSHOP inspector + spectrum | `WorkshopBench.aMovedPickupIsSeenReadAndHeard` | DONE |
| GI-7 (§0.7, §17) | Budgets: full repaint 8/40 ms, overlay 2 ms, family switch 120 ms, thumbnail 100 ms, 200 caches — only static render timed here | - | n/a | `GuitarIllustration.fullRenderIsFastEnough`; (visual) `Thumbnails.workerRendersCachesAndEvictsAt200` | OWNED |
| GI-8 (§1) | mm coords, origin at saddle; fit zoom shows whole guitar | `GuitarScene`, `GuitarRenderer::fitTransform` | illustration | `GuitarIllustration.everyFactoryGuitarRendersWithoutClipping` | DONE |
| GI-9 (§1) | Ctrl-scroll zoom to 4x with pan | `WorkshopPanel` BenchIllustration `mouseWheelMove` | WORKSHOP bench | (visual) `BenchZoom.ctrlScrollZoomsToFourTimesAboutThePointer` | OWNED |
| GI-10 (§2.1) | Static scene cached by spec hash; invalidates on swap/move/finish | `GuitarRenderer::keyFor`, `GuitarBodyComponent::rebuildScene` | n/a | `GuitarIllustration.theKeyChangesWithEveryVisibleChange` | DONE |
| GI-11 (§2.2) | Live overlays from the display FIFO per frame (polls engine here) | `GuitarBodyComponent::timerCallback` | illustration | (visual) `NoteDots.theDotAppearsWithin60msAndFadesOver60ms` | OWNED |
| GI-12 (§2.2, §5.31) | Hover / selection outlines | `GuitarRenderer::paintOverlay` | WORKSHOP bench | `WorkshopPanel.hoverDoesNotSelect` | DONE |
| GI-13 (§2.2, §5.32, §13.2) | Drag ghost; drag part cards onto the illustration (pickup, route, bridge, string, body, pick, slide, capo) — click-to-fit only | `WorkshopPanel::clickCard` | WORKSHOP drawer | `WorkshopPanel.clickingACardFitsItAsOneUndoEntry`; (visual) `WorkshopStrings.aCardOntoAStringOverridesItAndASetClearsOverrides` | OWNED |
| GI-14 (§2.3, §15) | Preset-browser thumbnails 128x256 on worker, hash cache, reduced detail | `GuitarRenderer` Options.thumbnail; (visual) `UI/Guitar/GuitarThumbnails` | (visual) preset browser | (visual) `Thumbnails.workerRendersCachesAndEvictsAt200`, `Thumbnails.everyFactoryPresetShowsItsGuitar` | OWNED |
| GI-15 (§3, §12.1) | Incompatible parts -> family defaults, banner lists replaced parts | `PartLibrary::switchFamily` (`replaced`) | WORKSHOP banner | `GuitarIllustration.aFamilySwitchGivesTheTargetFamilysGuitar` | DONE |
| GI-16 (§4.1-4.5) | Body catalogue (electric 12, acoustic 8, classical 3, bass 8, resonator 3) reachable — outlines exist, 14 unreachable (no part/guitar selects them) | `BodyOutlines.h` | WORKSHOP Body (by guitar only) | `GuitarIllustration.everyFactoryGuitarHasItsParts` | OWNED |
| GI-17 (§4, §18) | Body part carries outline + attachment points; new shapes as data | `resolveStyle` reads `illustration.body_style` (0 of 20 body parts set it) | n/a | - | OWNED |
| GI-18 (§5 1-25) | Fixed z-order layers shadow ... truss cover | `SceneBuilder::build*` | illustration | `GuitarIllustration.everyFactoryGuitarHasItsParts` | DONE |
| GI-19 (§5.6) | Grain per wood (flame, quilt, ash, mahogany, alder) — no test | `SceneBuilder::buildGrain` | illustration | - | OWNED |
| GI-20 (§5.7, §9) | Pickguard tortoise stipple, pearloid swirl (missing), 9 colours; pickguard slot has no drawer category | `buildPickguard`, `pickguardColour` | - (no drawer category) | - | OWNED |
| GI-21 (§5.8-5.9) | Soundhole/f-holes, rosette, bracing shadow — no test | `SceneBuilder` soundhole/bracing | illustration | - | OWNED |
| GI-22 (§5.26) | Played-notes layer | `paintOverlay` stringLevel; (visual) `IllustrationMotion.h` dots | illustration | (visual) `NoteDots.theDotAppearsWithin60msAndFadesOver60ms` | OWNED |
| GI-23 (§5.27) | Buzz heatmap layer when SETUP focused — not on any branch (heatmap only in CHARACTER `SetupGroup`) | - | - | - | OWNED |
| GI-24 (§5.28) | Slide bar layer (slant, material) | `paintOverlay` slideFret; (visual) `slidePath`, slant/colour | illustration | (visual) `AppearanceTests LiveDisplays.theFretboardDrawsTheSlideBarAndTheCircuitCurveFollowsTheVolume`, `WorkshopAccessories.theSlideTurnsOnTheBenchAndItsMaterialIsPlayed` | OWNED |
| GI-25 (§5.29) | Pick overlay | (visual) `GuitarRenderer::pickPath` | (visual) bench | (visual) `WorkshopAccessories.thePickIsDraggedAndTurnedOnTheBench` | OWNED |
| GI-26 (§5.30) | Pickup pulse layer during activity — not on any branch | - | - | - | OWNED |
| GI-27 (§6) | Bolt-on / set / through neck (pocket seam instead of bolts per DECISIONS) — no test | `buildNeck` joint | illustration | - | OWNED |
| GI-28 (§6) | Fretboard radius shading vintage/modern/compound (compound missing); woods + binding | `buildNeck`, `woodColour`, `buildBinding` | illustration | - | OWNED |
| GI-29 (§6) | Inlays dot/block/trapezoid/sharktooth/vine per fretboard part (chosen by body style; vine missing) | `buildNeck` inlay | illustration | - | OWNED |
| GI-30 (§6) | Headstocks 3+3, 6-in-line, reverse, 4-in-line, 2+2, slotted, 6+6; neck part selects layout | `HeadstockOutlines.h`; (visual) reverse layouts | illustration | (visual) `Headstocks.everyLayoutIsDrawableAndEveryGuitarHasItsPosts` | OWNED |
| GI-31 (§6) | Truss-rod cover + "L" mark at 4 % — no test | `buildHeadstock` | illustration | - | OWNED |
| GI-32 (§7) | Electric bridges TOM+stopbar, vintage/2-point trem, Floyd, wrap, Bigsby; hardtail ferrules (back not drawn) — no test | `buildBridge`, `buildTailpiece` | illustration | - | OWNED |
| GI-33 (§7) | Acoustic pin / pinless (missing) / floating+trapeze / moustache / tie-block; resonator biscuit/spider | `buildBridge` | illustration | - | OWNED |
| GI-34 (§7) | Bass vintage / high-mass / with mutes (missing) | `bassBridge` | illustration | - | OWNED |
| GI-35 (§8) | Pickup variants SC/HB/P90/mini/bar/split-P/J/MM; covers; mounting rings — no test | `SceneBuilder::pickup` | illustration | - | OWNED |
| GI-36 (§8) | Piezo shown as preamp/jack indicator; soundhole magnetic with cable | `SceneBuilder::pickup` | illustration | - | OWNED |
| GI-37 (§8) | Pole spacing from `pole_spacing_mm` — field never read | - | - | - | OWNED |
| GI-38 (§10) | String colours/styles per material table | `GuitarRenderer::stringColour` | illustration | `GuitarIllustration.stringColoursFollowSection10` | DONE |
| GI-39 (§10) | String counts 6/7/8/12-pairs/4/5/6 bass | `buildStrings` | illustration | `GuitarIllustration.everyFactoryGuitarHasItsParts` | DONE |
| GI-40 (§10, §13.1) | Per-string material override drawn in own colour; Ctrl+click string as drop target | (visual) `StringOverride`, `WorkshopBench` | (visual) bench | (visual) `WorkshopStrings.anOverriddenStringIsDrawnInItsOwnMaterial`, `aPerStringOverrideIsSeenReadHeardAndOneEntry` | OWNED |
| GI-41 (§11) | Finish block fields type/color_a/b/burst_shape/gloss/aging — no finish editor in any UI | `GuitarFinish`, file round-trip | - | `Workshop.everyFactoryGuitarLoadsAndRoundTrips` | OWNED |
| GI-42 (§11.1) | Named solid palette (18 colours) — none | - | - | - | OWNED |
| GI-43 (§11.2-11.6) | Bursts (named), transparent 60 %, natural, metallic stroke, sparkle 3 % — generic burst (+edge strokes per DECISIONS), metallic gradient, sparkle 30 % | renderer finish branches | - | - | OWNED |
| GI-44 (§11.7) | Aging: edge wear, fade, yellowing, seeded dings, checking (belt-buckle wear missing) | `buildAging` | - (no UI) | `GuitarIllustration.agingIsSeededAndStable` | OWNED |
| GI-45 (§11.8) | Hardware colour table applied to metal parts; per-part override — no UI, no per-part override | `GuitarRenderer::hardwareColour` | - | `PartAcoustics.hardwareColourIsSilent` | OWNED |
| GI-46 (§12.1) | Family selector first drawer category; one-time session confirmation | `WorkshopPanel::switchFamily` | WORKSHOP drawer | `WorkshopPanel.theGuitarCategorySwitchesFamily` | DONE |
| GI-47 (§12.1, §16) | 250 ms crossfade on family change; reduced motion = static outline | (visual) `UI/Guitar/IllustrationMotion.h` | illustration | (visual) `ReducedMotion.aGuitarChangeCrossfadesOrIsStaticWithAnOutline` | OWNED |
| GI-48 (§12.2) | Family change via template, keep compatible parts (templates = factory guitars per DECISIONS) | `PartLibrary::getFamilyTemplate/switchFamily` | WORKSHOP | `GuitarIllustration.aFamilySwitchGivesTheTargetFamilysGuitar` | DONE |
| GI-49 (§12.3) | Body, strings, scale, bridge, pickups, nut, frets, tuners, circuit change with family | template parts + `writeGuitarParameters` | WORKSHOP | `GuitarIllustration.aFamilySwitchGivesTheTargetFamilysGuitar` | DONE |
| GI-50 (§12.3) | Amp defaults follow family | (visual) `Workshop/FamilyDefaults` | n/a | (visual) `FamilySwitch.theAmpFollowsTheFamilyOnlyWhenItDoesNotSuit` | OWNED |
| GI-51 (§12.3) | Family -> bass mode in rhythm engine | `TunePlayer`, `retargetStrumDefaults` | n/a | `TunePlayer.theBassGoesToTheEngineOnlyForABass` | DONE |
| GI-52 (§12.3) | Slide-friendly hint on a high nut; MIDI-out profile default per family — none | - | - | - | OWNED |
| GI-53 (§12.4) | Preserve name, FX, amp, mod matrix, snapshots, seed | `switchGuitarFamily` | n/a | `WorkshopFamily.aFamilySwitchKeepsWhatSection12_4Keeps` | DONE |
| GI-54 (§13.1) | Hit regions from geometry, topmost wins; 10 000 random clicks | `GuitarRenderer::hitTest` | WORKSHOP bench | `GuitarIllustration.hitTestingFindsThePartOnTop` | DONE |
| GI-55 (§13.1) | Alt+click audition; Shift+click region below — Alt means bass-side, no Shift-below | `hoverCard(altDown)` | WORKSHOP | `WorkshopPanel.auditionFromTheDrawerNeverCommits` | OWNED |
| GI-56 (§13.3) | Drag pickup along axis with ruler snap | BenchIllustration `mouseDrag`, `WorkshopBench::movePickup` | WORKSHOP bench | `WorkshopPanel.aPickupDragIsOneEntryAndTheRulerValueFollows` | DONE |
| GI-57 (§13.3) | Scroll / screw handles set height per side | BenchIllustration `mouseWheelMove` | WORKSHOP bench | `WorkshopBench.heightsAndSetupEditsAreOneEntryEach` | DONE |
| GI-58 (§13.3) | Drag bridge saddle -> intonation — no test | `Drag::saddle` | WORKSHOP bench | - | OWNED |
| GI-59 (§13.3) | Drag nut slot -> depth | (visual) nut slot drag | (visual) bench | (visual) `WorkshopNut.aSlotDragIsOneEntryInRealUnitsAndReachesTheEngine` | OWNED |
| GI-60 (§13.2-13.3) | Capo card/drag along neck (headstock removes); pick drag+angle; slide drag+slant | (visual) `capoPath/pickPath/slidePath` | (visual) bench | (visual) `WorkshopAccessories.*` | OWNED |
| GI-61 (§13.3) | Snap modifiers fine default / coarse Shift / ultra-fine Ctrl — mapping differs (Shift fine, Alt free) | BenchIllustration `mouseDrag` | WORKSHOP | `WorkshopBench.snapIsOneMillimetreFineWithShiftFreeWithAlt` | OWNED |
| GI-62 (§14) | Warm/cool body tint while auditioning, fades 500 ms; static label under reduced motion | (visual) `WorkshopPanel` tint | (visual) bench | (visual) `WorkshopSpectrum.theBodyTintsWhileAuditioningAndFadesIn500ms`, `ReducedMotion.theBenchFadesCommittedChangesOnly` | OWNED |
| GI-63 (§16) | Accessible child per part with documented strings; announce on click | `Hit.description`; (visual) `stringDescriptions` | WORKSHOP bench (one component) | `GuitarIllustration.accessibleDescriptionsNameTheParts` | OWNED |
| GI-64 (§16) | Tab across parts, arrows navigate z-order, Enter opens inspector (arrows nudge here) | BenchIllustration `keyPressed` | WORKSHOP | `WorkshopPanel.keyboardNudgesMatchADrag` | OWNED |
| GI-T1 (§19) | Test: every factory guitar at 128-3072 px, no clipping | | | `GuitarIllustration.everyFactoryGuitarRendersWithoutClipping` | DONE |
| GI-T2 (§19) | Test: hash changes on swap and move > 0.5 mm | | | `GuitarIllustration.theKeyChangesWithEveryVisibleChange` | DONE |
| GI-T3 (§19) | Test: every family -> every family valid | | | `GuitarIllustration.aFamilySwitchGivesTheTargetFamilysGuitar` | DONE |
| GI-T4 (§19) | Test: drag bounds (route; height >= 0.8 mm without advanced) | `WorkshopBench::movePickup` | | `WorkshopBench.aPickupStopsBeforeItOverlapsAndSaysWhy`; (visual) `WorkshopRanges.theTabCarriesAPadlockAndHeightsStopAtStock` | OWNED |
| GI-T5 (§19) | Test: 3-tone sunburst vs reference SVG — none | | | - | OWNED |
| GI-T6 (§19) | Test: aging 0.7 dings stable | | | `GuitarIllustration.agingIsSeededAndStable` | DONE |
| GI-T7 (§19) | Test: family switch preserves name, FX, mod, seed | | | `WorkshopFamily.aFamilySwitchKeepsWhatSection12_4Keeps` | DONE |
| GI-T8 (§19) | Test: accessibility Tab reach + announcement strings for a fixture guitar | | | `GuitarIllustration.accessibleDescriptionsNameTheParts` (strings only) | OWNED |

<!-- counts DONE=24 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=48 -->
