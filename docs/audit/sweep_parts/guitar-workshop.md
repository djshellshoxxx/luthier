## guitar-workshop.md

The parts model is essentially DONE on this checkout: `Part`, `PartLibrary` and `WorkshopGuitar`, resolution and fallback, compatibility warnings, string-count clamping, Save As Guitar/Part, the preset override and parameter retirement are all implemented, reachable from the WORKSHOP bench and tested. The remaining rows are OWNED by `claude/luthier-visual`, which has workshop work (per-string overrides and the finish-free bench items) but does not touch these gaps. Four slots have no drawer category (top, fretboard, tailpiece, pickguard). There is no finish or hardware editor and no folder rescan. The missing-part banner has no jump action, the inspector does not show the string-count excess, and the per-slot "swap changes the rendered spectrum" test is missing.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| GW-1 (§0.1, §3) | A guitar is its parts; in-memory spec model | `Model/Workshop/PartLibrary.h:WorkshopGuitar`, `PartAcoustics.cpp:mapSpec` | Adv WORKSHOP tab / Easy wrench overlay, `UI/WorkshopPanel` | `Workshop::everyFactoryGuitarLoadsAndRoundTrips` | DONE |
| GW-2 (§0.2, §4) | Parts are files; factory read-only, user in Documents/Luthier/Parts; layout Parts/<Cat>, Guitars/<Family> | `PartLibrary::getFactory*/getUser*Folder` | WORKSHOP drawer | `Workshop::theFactoryLibraryIsThere` | DONE |
| GW-3 (§0.4, §5) | Compatibility advisory: warning text, part still fitted | `PartLibrary::getCompatibilityWarnings` | drawer card "Unusual here", inspector line | `Workshop::incompatiblePartsFitWithAWarning` | DONE |
| GW-4 (§0.5, §3) | Committed spec owned by the audio thread, atomic block-boundary swap | `PluginProcessor::applyWorkshopGuitar`, `LuthierEngine` live swap | n/a | `WorkshopSwap::aPartSwapDuringANoteIsClickFree`, `PartSwap::aSwapKeepingTheStructureIsTakenAtABlockBoundaryWithoutSilence` | DONE |
| GW-5 (§0.6) | The factory `GuitarType` guitars ship as `.luthierguitar`; enum kept as a shortcut | `Resources/Guitars`, `migration.json` | header guitar-type combo | `GuitarMigration::everyPreM49NameResolvesToItsShippedGuitar`, `WorkshopPresets::choosingAGuitarTypeFitsItsParts` | DONE |
| GW-6 (§1) | Slots body, neck, frets, nut, bridge, tuners, strings, wiring, pickups n/m/b fittable | `GuitarSlot`, `PartLibrary::resolve` | WORKSHOP drawer categories (`WorkshopPanel.cpp` `kCategories`) | `WorkshopPanel::clickingACardFitsItAsOneUndoEntry`, `WorkshopPanel::everyFittedPartIsReachableAndNothingElseIs` | DONE |
| GW-7 (§1) | Slots top, fretboard, tailpiece, pickguard fittable — parts ship, but no drawer category on base or visual | `GuitarSlot::top/fretboard/tailpiece/pickguard` | - | - | OWNED |
| GW-8 (§1) | Non-part fields `hardware_color`, `finish`, `setup`, `character_seed` — setup strip only; no finish/hardware UI anywhere | `WorkshopGuitar::finish/hardwareColour/setup/seed` | WORKSHOP setup strip (setup only) | `PartAcoustics::hardwareColourIsSilent` | OWNED |
| GW-9 (§1) | Zero-pickup guitar is legal (acoustic) | `mapSpec` | n/a | `Workshop::everyFactoryGuitarLoadsAndRoundTrips` (acoustic factory guitars) | DONE |
| GW-10 (§2) | Part types and field sets (16 types incl. slide/pick/capo accessories) | `Part::fields`, `PartType` | WORKSHOP inspector; drawer Pick/Slide/Capo | `WorkshopPanel::theInspectorShowsTheSelectedPart`, `WorkshopPanel::aSlideNeedsSlideMode`, `WorkshopCapo::aPartialCapoClampsOnlyItsStrings` | DONE |
| GW-11 (§3.1) | Existing GuitarSpec widened; compiled table is the missing-file fallback | `mapSpec` `baseTypeFor` | n/a | `WorkshopPresets::aMissingGuitarFileFallsBackToItsType` | DONE |
| GW-12 (§3.2) | DerivedAcoustics cached, mapped once per swap | `mapSpec` once per swap | n/a | `WorkshopSwap::aSwapMapsOnceNotPerBlock` | DONE |
| GW-13 (§4) | Library scanned at startup and on folder change — no folder watcher on any branch | `PartLibrary::refresh` (ctor, `savePartAs`) | - | - | OWNED |
| GW-14 (§4) | A user part with a factory part's name wins | `PartLibrary::find` | n/a | `Workshop::aUserPartBeatsTheFactoryOne` | DONE |
| GW-15 (§4.1) | Missing reference falls back to the category default, loads, logs `PART_MISSING` | `PartLibrary::resolve/getDefault`, `ErrorLog` | n/a | `Workshop::aMissingPartFallsBackAndSaysSo` | DONE |
| GW-16 (§4.1) | Missing-part notification with a jump-to-Workshop action — banner posts, no jump action on base or visual | `PluginProcessor::takeGuitarNotices` | header banner (`PluginEditor.cpp` "missing-part") | - | OWNED |
| GW-17 (§5.1) | String count = min(neck, bridge); tuning resizes | `getStringCount`, `writeGuitarParameters` | n/a | `Workshop::aStringCountMismatchClamps`, `WorkshopPresets::choosingATypeGivesItsStringCount` | DONE |
| GW-18 (§5.1) | Excess strings reported in the inspector — ErrorLog only | - | - | - | OWNED |
| GW-19 (§6) | Save As Guitar (Ctrl+G) to Documents/Luthier/Guitars, by reference, preset reference updated | `LuthierAudioProcessor::saveGuitarAs` | WORKSHOP `saveAsButton`; Ctrl+G (`Accessibility.cpp` `saveGuitarAs`) | `WorkshopPresets::saveAsGuitarWritesAFileAndPointsThePresetAtIt` | DONE |
| GW-20 (§6) | "Bundle parts" export option — no test | `saveGuitarAs (bundleParts)` | Save-guitar dialog button | - | OWNED |
| GW-21 (§7) | Save As Part from the inspector; editing a factory part makes an unsaved user copy | `LuthierAudioProcessor::savePartAs`, `WorkshopBench::editField` | WORKSHOP "Save as user part" | `WorkshopPresets::saveAsPartMakesAUserPartAndFitsIt`, `WorkshopPanel::editingAFieldMakesAUserCopy` | DONE |
| GW-22 (§8) | Preset `guitar.reference` + `override`; override wins, self-contained | `PluginProcessor` guitar override state | n/a | `WorkshopPresets::anEditedGuitarTravelsWholeInTheState`, `Workshop::anEmbeddedGuitarNeedsNoPartFiles` | DONE |
| GW-23 (§9) | Workshop adds no parameters; pickup position/height retired and migrated | `PresetManager` migration | n/a | `WorkshopPresets::oldPickupPlacementParametersBecomeTheGuitars` | DONE |
| GW-T1 (§10) | Every factory guitar round-trips | - | n/a | `Workshop::everyFactoryGuitarLoadsAndRoundTrips` | DONE |
| GW-T2 (§10) | Per slot, a swap changes the rendered spectrum and swapping back restores it to -80 dBFS — derived values only | - | n/a | `PartAcoustics::everyMappedFieldMovesSomething` (struct, not render) | OWNED |
| GW-T3 (§10) | Swap is click-free | - | n/a | `WorkshopSwap::aPartSwapDuringANoteIsClickFree` | DONE |
| GW-T4 (§10) | Missing part falls back and reports (notification + log) | - | n/a | `Workshop::aMissingPartFallsBackAndSaysSo` | DONE |
| GW-T5 (§10) | User part beats factory; incompatible parts fit; string-count clamps | - | n/a | `Workshop::aUserPartBeatsTheFactoryOne`, `Workshop::incompatiblePartsFitWithAWarning`, `Workshop::aStringCountMismatchClamps` | DONE |
| GW-T6 (§10) | No filesystem access on the audio thread during a swap; mapping runs once | - | n/a | `WorkshopSwap::noFileIsTouchedFromTheAudioThreadDuringASwap`, `WorkshopSwap::aSwapMapsOnceNotPerBlock` | DONE |
| GW-T7 (§10) | Preset override loads with no part files | - | n/a | `Workshop::anEmbeddedGuitarNeedsNoPartFiles` | DONE |

<!-- counts -->
