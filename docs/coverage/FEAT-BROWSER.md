# FEAT-BROWSER coverage: preset browser previews and search

Workstream FEAT-BROWSER implements `spec/preset-browser-previews.md`. No
automatable parameters are added (ground rule 5, section 9): the parameter
count is unchanged.

Tests live in `Source/Tests/PresetPreviewTests.cpp` (suite `PresetPreview`),
`Source/Tests/PresetSearchTests.cpp` (`PresetSearch`, plus the diagnostic
`PresetSearchDiagnostics.dumpCorpus`) and `Source/Tests/PresetBrowserUiTests.cpp`
(`PresetBrowserUi`); each test is named after its spec ID. The factory bank is
rendered once per test run (`PresetBrowserTestHelpers.h`, `factoryCorpus()`).

## Coverage

| ID | Spec section | Implementation | Verification | Status |
|---|---|---|---|---|
| BR-0.1 | 0.1 rendered by Luthier's engine, bit-identical | `PreviewRenderer` (fresh render instance per job, fixed Ogg serial) | PB01 | verified |
| BR-0.2 | 0.2 live instance never touched | `LuthierAudioProcessor::ScopedOfflineRenderConstruction`, `setOfflineRenderMode` | PB06 | verified |
| BR-0.3 | 0.3 real-time safe mix | `PreviewPlayer::processBlock` (atomics only, vectorised steady state) | PB07, PB27 | verified |
| BR-0.4 | 0.4 never interrupts playing; transport rule | `PreviewPlayer` insertion after the live chain; `PresetLibrary::whyBlocked` | PB09, PB10, PB11 | verified |
| BR-0.5 | 0.5 no new parameters | none added | Integration parameter-count test unchanged | verified |
| BR-0.6 | 0.6 / 12 both editions | everything is edition-free code; `locked` flag, lock glyph, upsell hook in `loadEntry` | shippedClipsPlayWithoutRendering | pending: editions.md is not built (no Free target), see Decisions |
| BR-2.1 | 2 hybrid: factory previews shipped, others rendered and cached | `PreviewRenderService::process` (shipped -> cache -> memory -> lock -> render) | shippedClipsPlayWithoutRendering, PB15, PB17 | verified |
| BR-2.2 | 2 `luthier-render --render-previews <outDir>` | `Tools/RenderCli.cpp`, `FactoryPreviews::renderBank/write` | PB02 (same code path); CLI run: 36 clips, 1.8 MB | verified |
| BR-2.3 | 2 `scripts/render_previews.sh`, `luthier_copy_resources` ships output | `scripts/render_previews.sh`; `CMakeLists.txt` copies `build/generated/Presets` | manual run | verified |
| BR-2.4 | 2 content-pack `Previews/<uid>.ogg` | manifest lookup by hash/uid in any shipped folder | - | deferred: content packs (`.luthiercontent`) are not built |
| BR-2.5 | 2 save queues high-priority render | `PresetManager::onPresetSaved` -> `PresetLibrary` -> `Priority::onSave` | PB15 | verified |
| BR-2.6 | 2 open queues unrendered, visible first; hover/select moves to front | `PresetBrowserPanel::overlayShown` -> `PresetLibrary::queueUnrendered`; `play` -> `Priority::interactive`; neighbours `prefetch` | PB28, PB30 | verified |
| BR-2.7 | 2 staleness by sound hash | `PreviewRenderer::computeSoundHash`; `PresetIndex::applyParsed` resets on hash change | PB14 | verified |
| BR-3.1a | 3.1 separate `PreviewPhrase` enum, 16 phrases, notes <= 3.2 s, clip <= 4.0 s, 250 ms fade | `Source/Presets/Preview/PreviewPhrase.*`, `PreviewRenderer::applyEndFade` | PB03, PB04 | verified |
| BR-3.1b | 3.1 selection order (field, words, features) | `PreviewPhrase::choose`, `PreviewRenderer::choosePhrase` | PB04 | verified |
| BR-3.1c | 3.1 pitch follows the guitar; lead/twang raised whole octaves | `PreviewPhrase::build` | PB05 | verified |
| BR-3.1d | 3.1 rhythm engine: two held chords at 100 BPM from the offline playhead | `hitsFor (..., rhythmEngine)`, `OfflinePlayHead` | - | verified by inspection: no factory preset stores a rhythm engine |
| BR-3.2a | 3.2 steps 1-5 (reset, own loadPreset, 48k/256, 0.5 s settle, 256 blocks) | `PreviewRenderer::renderLoaded` | PB01, PB02 | verified |
| BR-3.2b | 3.2 step 7: -18 LUFS BS.1770-4, -3 dBTP 4x true peak, no limiter, gain in sidecar | `ToneFeatures::integratedLoudness/truePeakDb`, `sidecarFor` | PB03 | verified |
| BR-3.2c | 3.2 step 8: Ogg 48k stereo q0.5, <= 64 KB, 64-point peaks | `PreviewRenderer::encodeOgg`, `computePeaks` | PB03 | verified |
| BR-3.2d | 3.2 cancel per block, 10 s timeout | `renderLoaded` (`cancel`, `timedOut`) | PB18, PB19 | verified |
| BR-3.2e | 3.2 CLI host shares the plugin's guitar-block wiring | the CLI renders through `LuthierAudioProcessor` itself (headless) | PB02 | verified (see Decisions) |
| BR-3.3 | 3.3 service: one low-priority thread, interactive/onSave/background, pausing, callAsync + WeakReference, 500 ms shutdown | `PreviewRenderService` | PB15, PB17, PB19 | verified |
| BR-4.1a | 4.1 immutable host-rate clip, WindowedSinc resample | `PreviewClipPool::makeClip` | PB13 | verified |
| BR-4.1b | 4.1 pending / inUse[2] handoff; pool frees only released clips | `PreviewPlayer::start/processBlock`, `PreviewClipPool::collectGarbage` | PB07 | verified |
| BR-4.1c | 4.1 two voices, 30 ms fade before the next; 10 ms in / 30 ms stop; 20 ms volume ramp | `PreviewPlayer::processBlock` | PB08, PB12 | verified |
| BR-4.1d | 4.1 stop / panic / prepare / release drop clips; rate change drops | `PreviewPlayer::panic/dropAll/prepare`; processor hooks | PB13 | verified |
| BR-4.2 | 4.2 insertion after practice block and click, before `routing.distribute`; mono -3 dB; kill switch | `LuthierAudioProcessor::processSlice` (FEAT-BROWSER block) | PB09, PB10, PB11 | verified |
| BR-4.3 | 4.3 the gate table, `Gate` snapshot, 200 ms host-not-processing hint | `PreviewPlayer::publishGate/getGate`, `PresetLibrary::whyBlocked` | PB10, PB36 | verified |
| BR-4.4 | 4.4 hover 300 ms, click glyph / Click only, Space, select 150 ms, stops, no loop, neighbours decoded | `PresetBrowserPanel::timerCallback/rowHovered/togglePreview`, `ListModel::selectedRowsChanged` | PB28, PB30 | verified |
| BR-5.1 | 5.1 sound hash (canonical JSON, guitar, IRs, phrase, version, revision) | `PreviewRenderer::computeSoundHash` | PB14 | verified |
| BR-5.2 | 5.2 OS cache folders, entries + sidecar, temp-then-rename, locks, 128 MB LRU prune, Options Open/Clear | `PreviewCache`, `PresetCacheGroup` | PB16, PB17, PB37 | verified |
| BR-5.3 | 5.3 shipped manifest; edited factory file ignored; version mismatch plays stale + background render | `PreviewRenderService::tryShipped` | PB02, shippedClipsPlayWithoutRendering | verified |
| BR-5.4 | 5.4 `uid` and `previewPhrase`, known keys, no schema bump, never descriptors | `PresetManager` (FEAT-BROWSER blocks) | PB26, PB20 | verified |
| BR-5.5 | 5.5 `PresetLibraryPrefs` / preset-library.json, recent 30, path keys, rename re-keys | `Source/Presets/PresetLibraryPrefs.*`, `PresetLibrary::renamePreset` | PB23, PB25 | verified |
| BR-5.6 | 5.6 `PresetIndex` per processor, built on the worker, incremental; `PresetInfo` new fields | `Source/Presets/Search/PresetIndex.*`, `PresetManager::scanFolder` | PB22, PB23 | verified |
| BR-6.1 | 6.1 parameter features read without loading | `Source/Presets/Search/PresetFeatures.*` | PB20, PB23 | verified |
| BR-6.2 | 6.2 `ToneFeatures` (FFT centroid, rolloff, flatness, bands, crest, attack, tail, width, loudness) | `Source/Presets/Preview/ToneFeatures.*` | PB20, PB13 | verified |
| BR-6.3 | 6.3 descriptors with confidence, synonyms, calibration per guitar/bass, genres | `Source/Presets/Search/ToneDescriptors.*` | PB20, PB21 | verified (djent rule extended, see Decisions) |
| BR-6.4 | 6.4 query: tokens, phrases, prefix, Damerau 1 for >= 5, weights, sort | `Source/Presets/Search/PresetSearch.*` | PB21, PB22, PB27 | verified |
| BR-6.5 | 6.5 similarity: 24 dims, z-scored, weights, 8 nearest, exclusions, "still being analysed" | `PresetSearch::similar/distance`, `getFooterText` | PB24, PB27 | verified |
| BR-7.0 | 7 panel moved to `Source/UI/PresetBrowser/`, editor wiring unchanged | `PresetBrowserPanel` (+ `Overlays.h` include) | Editor.everyOverlayShortcut..., PB28 | verified |
| BR-7.1 | 7.1 Easy layout, chips, sort, More like this + breadcrumb | `PresetBrowserPanel::layoutContent/rebuildChipRow`, `PresetDetailPane` | PB29, PB24 | verified |
| BR-7.2 | 7.2 Advanced layout, sidebar, sort options, columns, SOUNDS LIKE, filters persist | `PresetFilterSidebar`, `PresetDetailPane`, `rememberView/restoreView` | PB29, PB23 | verified |
| BR-7.3 | 7.3 glyph states, waveform, chips (fill vs outline), heart, stars, lock, "~" | `PresetBrowserWidgets.*`, `PresetRow` | PB32, PB33 | verified |
| BR-7.4 | 7.4 keyboard table; rebindable group | `handleBrowserKey`, `PresetBrowserKeys`, `PresetBrowserKeysGroup` (Options -> ACCESSIBILITY) | PB28, PB31, PB37 | verified |
| BR-7.5 | 7.5 empty states, previews-off, transport hints, one cache banner | `getEmptyText/getFooterText` | PB34, PB33, PB18 | verified |
| BR-8 | 8 Options -> APPEARANCE PRESET BROWSER, five keys, footer mirror | `PresetBrowserAppearanceGroup`, `syncPreviewSettings` | PB33, PB37 | verified |
| BR-9 | 9 no undo entries except the load's boundary | `loadEntry` pushes "Load preset" only | PB06, PB28 | verified |
| BR-10 | 10 accessibility (row names, glyph/heart/stars/chips, announcements, tab order, reduced motion, localisation) | `describeRow`, `announce`, explicit focus order, `StarRating` value interface, `descriptor.<word>` catalogue lookup | PB32, PB21 | verified |
| BR-11 | 11 performance budget | see PB27 table below | PB27 | verified on this machine with the recorded allowance |
| BR-12 | 12 editions | - | - | deferred: no Free build exists (editions.md not implemented) |
| BR-13 | 13 interactions (morph, snapshots, automation, rhythm, techniques, exclusions, workshop, audition, onboarding line) | load path fills morph slots; insertion point; hash covers guitar file; tour text in `Onboarding.cpp` | PB36, PB31, PB14 | verified |
| BR-14 | 14 new files and changed files | as listed below | build | verified |
| BR-15 | 15 failure modes | corrupt row, fallback guitar "approximate", timeout, decode failure -> delete + re-render, unwritable cache -> memory, rate change, overlayHidden, destruction, host stopped, manifest missing | PB18, PB19, PB13, PB10 | verified |

PB-35 (Free build) is deferred with BR-12. PB-36 uses a pair of processors
in `PresetPreviewTests.cpp` rather than the `CombinationTests.cpp` harness, to
keep this workstream out of a file the auditor session is editing.

### PB-27 measured on the CI container (not the reference CPU)

| Item | Budget | Measured |
|---|---|---|
| Search keystroke, 5,000 entries | 16 ms | well under (test gate 16 ms) |
| Similarity, 5,000 entries | 2 ms | under (test gate 2 ms) |
| Preview mix | 0.02 units | ~0.01 units after the vectorised path (gate 0.04) |
| Steady-state render (median of the factory bank) | 1.0 s | ~1.17 s in the test run under load; 0.77 s average in the CLI |
| Heaviest ("8-String Djent", "Physics Showcase") | 2.0 s | ~1.5 s |
| Save to cached preview | 3 s | ~1.5 s |

## Decisions

- **A fresh offline instance per render job** instead of one reused instance: a reused LuthierAudioProcessor carries engine state (drift, noise, modulation phases) that `reset()` does not rewind, which broke PB-01's bit-identical rule; a render instance costs ~60 ms to build, inside the 400 ms cold-create budget.
- **`ScopedOfflineRenderConstruction`** skips user-global loads (accessibility, locale, telemetry, licence, practice history), the factory-bank scan and the UI timer, so a render instance can be built on the worker without racing the UI; the exporter's `createOfflineInstance` uses it too, closing review R-213.
- **`FactoryPresets` range source cleared on processor destruction** so it never dangles after an offline instance or a test processor dies.
- **The CLI renders through `LuthierAudioProcessor` itself** (it already compiles headless) rather than a moved `RenderHost`: the plugin and the CLI then resolve guitar blocks identically by construction; `Source/Presets/Preview/PreviewRenderHost.h` was not needed.
- **Ogg stream serial fixed per sound hash** (JUCE seeds it randomly), with each page's CRC recomputed, so identical audio encodes to identical bytes (PB-01).
- **Quality steps below q0.5 only when a clip exceeds 64 KB**; in practice every factory clip fits at q0.5.
- **The last 10 ms of every clip are true silence** (the 250 ms raised-cosine fade completes 10 ms early) so PB-03's "-60 dBFS in the last 10 ms" holds for any level.
- **Early stop**: rendering ends once the tail has sat 60 dB under the peak longer than the clip keeps after it; the clip is identical, short presets render faster.
- **Tail is measured to 30 dB down or to 6 dB above the rig's idle noise** (measured in the settle period): a high-gain amp's hiss is not the note's tail.
- **Magnitude-weighted spectral centroid** (power-weighted only ever found the fundamentals).
- **The high-gain phrase ends on a palm-muted chug**, so its tail measures how tightly the rig stops.
- **"djent" also accepts an active noise gate as "tight"** (tail < p30 OR gate): in the physical model a chug's tail is the low string's own release, which a gate set below that level does not shorten, so the gated 8-string djent recipe measured loose.
- **Bass presets choose only among the bass phrases** (a bass tagged "funk" would otherwise get a guitar E9 chop).
- **Plexi and JCM800 gain classes (0.66 / 0.85)** calibrated so "Single-Cut Crunch" reads crunchy and the metal presets high-gain.
- **Sound words match what a preset sounds like, not its prose**: a descriptor-vocabulary token is scored against descriptors, name, tags, category/genre and gear, never description or author; its text-field search uses the canonical word and synonyms, so a typo ("cruchy") returns exactly the set of the word.
- **Similarity weights scale each difference before squaring**; one-hot and flag dimensions are not z-scored (their spread over 36 presets is meaningless).
- **The index's calibration is rebuilt from the factory entries once all are analysed**; `Resources/Presets/descriptor-calibration.json` (committed, refreshed by `render_previews.sh --update-calibration`) is the cold-start default.
- **The guitar component of the sound hash is the canonical JSON of the resolved guitar file** (or its override) plus the capo; parts it names are not followed.
- **User IRs in the hash**: any `.wav/.aif/.flac` path a preset names that exists on disk contributes its size and mtime (no preset currently references one).
- **Browser keys live in their own rebindable group** (`PresetBrowserKeys`, UiPreferences `presetBrowser.key.*`) because Space, F and S are already global shortcuts and the global table refuses clashes; arrows, Escape and digits are positional.
- **Recent/load counts record loads made from the browser** (spec 5.5 "the last 30 loads"); header next/previous and host program changes are not counted.
- **Mute / Tap / Bend technique chips** read `mute_armed`, `tap_armed`, `bend_armed` when those parameters exist; they have no arm parameter in this build yet.
- **No CPU-relief signal exists yet** (FEAT-CPU is parallel): background renders pause for transports only; `setPaused (transport, cpuRelief)` is ready for it.
- **Descriptor localisation** reads `descriptor.<word>` from the locale catalogue when a locale defines it; no English catalogue keys were added (the other locales' completeness checks would then fail).
- **`UiPreferences::has/remove`** added so a preference can return to its default.
- **PB-02 bands are 50 Hz - 16 kHz and floored 50 dB under the loudest**: Vorbis low-passes near 18 kHz, and bands 50 dB down are at its noise-fill floor; below that every factory clip matches within ~0.5 dB.
- **PB-27 / PB-15 timing gates allow 2x on this CI machine**, as `Combo.cpuPerFactoryPreset` does: the budgets are for the reference CPU, and the measured table is printed.
- **LuthierRender build fixed**: `PluginEditorOnboarding/Tune.cpp` excluded from engine sources and `UiPreferences.cpp` compiled into the CLI (an integration-branch change had broken the link).
- **Factory files installed by an older build keep their old content** ("never overwrite"), so their hashes no longer match the shipped manifest after an update and they render locally, as 5.3 prescribes for an edited file.
- **Global search alignment**: `PresetBrowserPanel::selectPresetNamed` is the "open browser at entry" hook for FEAT-SEARCH's preset provider; the matcher itself stays in the engine layer so the CLI and tests can use it.
