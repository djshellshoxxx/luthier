## factory-content.md

Guitars (27 files, which cover all 15 named ones), parts (148, with the spec's trademark names replaced by neutral ones) and IRs (720) ship and meet or exceed the spec. Pattern and kit content is compiled in (39 patterns, 27 guitar kits and 4 bass kits), not shipped as files. The factory preset bank is still the older 36-entry set, not the spec's 36 named presets: "Modern Overdrive" (the first-run default) does not exist, there is no difficulty ladder, and reggae, latin, indie and punk have no presets. The 6 factory tunes and the 12 example MIDI clips are on the tune-help branch. None of these exist: example setlists, backing tracks, the spectrum-delta fixtures and the legal/size gates.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| FC-1 (§0.1) | No trademarks in shipped names — "Fuzz Face Lead" is now "Germanium Fuzz Lead", "Modern LP Wiring" is "Modern Single-Cut Wiring" (old names aliased); legal sign-off is FC-28 | `Presets/FactoryPresets.cpp` (`renamedPreset`, retired list), `PartLibrary::renamedFactoryPart`, `Resources/Parts/Wiring`, `Tools/trademark_scan.py` | n/a | `Trademarks::noFactoryPresetPartOrGuitarNamesABrand`, `Trademarks::oldNamesStillLoad`, `Trademarks::sourceTreeHasNoUnmarkedBrandNames` | DONE |
| FC-2 (§0.2, §1) | Tonal spread: 36 presets covering the map — 36 entries including Init, Dry Instrument and Physics Showcase, not the spec list | `FactoryPresets.cpp` recipes | preset browser | `Presets::everyFactoryPresetLoadsAndPlays` | PARTIAL |
| FC-3 (§0.3) | Difficulty ladder: easy, medium and showcase per category | - | - | - | MISSING |
| FC-4 (§0.4) | At least 2 presets per genre (12 guitar + bass genres) — no reggae, latin, indie or punk; bass funk/reggae/punk/jazz missing | `FactoryPresets.cpp` | preset browser | - | MISSING |
| FC-5 (§0.5) | Every factory guitar playable at every factory preset; a preset with no guitar picks a suitable one — fallback by type exists; no cross-product test | guitar-block fallback in `PluginProcessor` | n/a | `WorkshopPresets::aMissingGuitarFileFallsBackToItsType` | PARTIAL |
| FC-6 (§0.6, §4) | 6 factory tunes that loop and sound finished (Fingerstyle Etude … Funk Slap Groove) | on tune-help: `Resources/Tunes/Examples/01-06*.luthiertune` | TUNE tab | on tune-help | OWNED |
| FC-7 (§0.7) | No copyrighted third-party audio, MIDI or images — IRs synthesised; `THIRD_PARTY_LICENCES` on visual | `scripts/make_irs.py` | n/a | n/a | DONE |
| FC-8 (§0.8, §13) | Factory content ≤ 200 MB compressed, as a gate — Resources is 31 MB; no automated gate | - | n/a | - | NO-TEST |
| FC-9 (§1) | The 36 named presets (Fresh Strings Clean … Jazz Walking Bass) — only "Modern Metal Chug" and "Flamenco Rasgueado" match | `FactoryPresets.cpp` | preset browser | - | MISSING |
| FC-10 (§1) | "Modern Overdrive" is the default first-run preset | - | - | - | MISSING |
| FC-11 (§2) | 15 named factory guitars with designed finishes and distinct parts ("Selmer-Style" ships as "Gypsy Jazz"; 12 extras) | `Resources/Guitars/*` (27) | WORKSHOP / guitar selector | `Workshop::everyFactoryGuitarLoadsAndRoundTrips` | DONE |
| FC-12 (§3) | ~90 parts across the named categories, names per list (brand names replaced: Kluson -> Vintage Keystone, Floyd -> Locking Double Tremolo, EMG -> Active …) | `Resources/Parts` (148), `Tools/generate_factory_parts.py` | WORKSHOP bench | `Workshop::theFactoryLibraryIsThere` | DONE |
| FC-13 (§5) | ~28 rhythm patterns incl. bass (Walking, Tumbao, Root-Fifth, Dub, Motown, One-Drop, Punk Pick) — 39 guitar patterns compiled in; 4 bass step grids; Walking, Latin Tumbao, Reggae One-Drop, Punk Pick and Dub Bass missing; no shipped `.luthierpattern` files | `Rhythm/Patterns.cpp` factory list, `Rhythm/BassStepGrid.cpp` | RHYTHM tab | `RhythmPatterns::factoryPatternsAreWellFormed` | PARTIAL |
| FC-14 (§6) | ~28 `.luthierkit` files with voicer, humanize, pick style, setup style and noise style — compiled in; no pick/setup/noise style fields | `Rhythm/GenreKit.cpp` | RHYTHM tab kit selector | `GenreKits::factoryKitsAreWellFormed`, `GenreKits::everyKitResolvesEveryPatternItNames` | PARTIAL |
| FC-15 (§7) | 10 example setlists that reference only factory presets | - | - | - | MISSING |
| FC-16 (§8) | 6 royalty-free backing tracks in `Resources/Practice/BackingTracks/` | - | - | - | MISSING |
| FC-17 (§9) | 12 example MIDI clips, one per genre kit | on tune-help: `Resources/Examples/01-12*.mid` (not in `MIDI/`) | - | on tune-help | OWNED |
| FC-18 (§10) | 720 IRs (216 body + 504 cab), deterministic generator | `Resources/BodyIRs`, `Resources/CabIRs`, `scripts/make_irs.py` | TONE MATCH tab | - (no test counts or loads the shipped IRs) | NO-TEST |
| FC-19 (§11) | Post-release `.luthiercontent` packs — format on visual; packs are post-release | on visual: `Updates/ContentPackage` | - | on visual: `ContentPackage.*` | OWNED |
| FC-20 (§12) | Every user-facing name in the locale catalog — preset/guitar/part names not catalogued | `Accessibility/Localisation.cpp` | n/a | - | PARTIAL |
| FC-21 (§12) | Preset names are noun phrases about tone or use — "Fretless Mwah" and "Octave Fuzz Stoner" borderline; not checked | `FactoryPresets.cpp` | n/a | - | NO-TEST |
| FC-22 (§12) | Guitar names `[Style] [Family]`; part names state the physical fact; tune names < 32 chars — no test | `Resources/Guitars`, `Resources/Parts`, `Resources/Tunes/Templates` | n/a | - | NO-TEST |
| FC-23 (§13) | Load every factory preset in every host: no crash, no missing-reference banner — in-process only; banner not asserted | - | n/a | `Presets::everyFactoryPresetLoadsWithoutAMissingReference` | DONE |
| FC-24 (§13) | Every factory guitar matches its spectrum-delta fixture within 0.2 dB — `SpectrumDelta` exists; no fixtures | `Workshop/SpectrumDelta.cpp` | n/a | on visual: `Workshop::everyFactoryGuitarRoundTripsInAudio` (null test, not fixtures) | MISSING |
| FC-25 (§13) | Every factory tune plays end to end without dropouts | on tune-help (tunes) | - | - | OWNED |
| FC-26 (§13) | Every factory setlist loads and every step resolves | - | - | - | MISSING |
| FC-27 (§13) | Every backing track streams at 48 kHz without dropouts | - | - | - | MISSING |
| FC-28 (§13) | Legal review sign-off recorded for every named entry | - | n/a | - | MISSING |

<!-- counts DONE=5 NO-GUI=0 NO-TEST=4 PARTIAL=5 MISSING=10 OWNED=4 -->
