## preset-browser-previews.md

Only the pieces this spec says it keeps exist here: the `PresetBrowserPanel` overlay in `UI/Overlays.h` opened by `header.onOpenPresetBrowser` and the `presetBrowser` Ctrl+O shortcut, with a plain search box, category box and the Morph row. The guitar thumbnail (`GuitarThumbnails`, tested by `Thumbnails::workerRendersCachesAndEvictsAt200`) is now merged; the "Uses Techniques" chip is not present in Source. Nothing new in this spec exists yet: previews, render service, player, cache, index, descriptors, similarity, library prefs and the new layouts. Re-verified: those rows are MISSING.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| PB-K1 (intro, §7) | Browser stays one overlay opened from header preset name / Ctrl+O (`presetBrowser` id unchanged); Morph row kept | `UI/Overlays.h:PresetBrowserPanel` (`morphToggle`, `morphSlider`) | Header preset name `HeaderBar::onOpenPresetBrowser` (`PluginEditor.cpp:106`), Ctrl+O (`Accessibility.cpp:532`) | `Editor::everyOverlayShortcutOpensItsOwnOverlayAndEscapeClosesIt`, `PresetMorph::theBrowserMorphRowFillsTheSelectedSlot` | DONE |
| PB-K2 (intro) | Keep guitar thumbnail and "Uses Techniques" chip in the rebuilt browser — thumbnail on visual: `Overlays.h GuitarThumbnails thumbnails`; chip on techniques: `Overlays.cpp techniqueChip` + `TechniquesUiTests` | Thumbnail: `UI/Guitar/GuitarThumbnails.*`, `Overlays.h thumbnails`, used in `Overlays.cpp` (getForPreset); "Uses Techniques" chip: none found in Source | PresetBrowserPanel thumbnail only | `Thumbnails::workerRendersCachesAndEvictsAt200`; chip untested/absent | PARTIAL |
| PB-0 (§0) | Ground rules: own-engine renders, live instance untouched, RT-safe mix, never interrupts, no params, both editions | - | - | - | MISSING |
| PB-2 (§2) | Hybrid sourcing: shipped factory previews via `luthier-render --render-previews`, `scripts/render_previews.sh`, content-pack previews; runtime renders on save/open/hover, staleness by hash | - | n/a | - | MISSING |
| PB-3.1 (§3.1) | `PreviewPhrase` table (15 phrases), selection order, pitch follows guitar, rhythm-engine presets — not built on this checkout (`AuditionPhrase` in `Support/AudioExporter.h` is separate and untouched) | - | n/a | - | MISSING |
| PB-3.2 (§3.2) | `PreviewRenderer` on offline instance (`createOfflineInstance` exists, `PluginProcessor.h:456`), `PreviewRenderHost` moved from RenderCli, job steps, -18 LUFS / -3 dBTP, Ogg q0.5 + peaks, cancel/timeout | - | n/a | - | MISSING |
| PB-3.3 (§3.3) | `PreviewRenderService` owned by processor, priorities, pause on transport/relief, async results, 500 ms join | - | n/a | - | MISSING |
| PB-4.1 (§4.1) | `PreviewPlayer`: resampled immutable clips, atomic hand-off, `PreviewClipPool`, two voices/crossfade, fades, smoothed volume, stop paths | - | n/a | - | MISSING |
| PB-4.2 (§4.2) | Insertion in `processSlice` after practice/click and before `routing.distribute`; excluded from looper/recorder/capture/buses/MIDI; kill switch | - | n/a | - | MISSING |
| PB-4.3 (§4.3) | Gate table (idle, live notes, transport, `whileTransport`, non-realtime/kill), `PreviewPlayer::Gate`, 200 ms no-process hint | - | - | - | MISSING |
| PB-4.4 (§4.4) | Triggers: 300 ms hover, click glyph/"Click only", Space, on-select 150 ms; stops; neighbour pre-decode | - | - | - | MISSING |
| PB-5.1 (§5.1) | Sound hash (canonical JSON minus meta, GuitarSpec, IR stat, phrase, version, `kRenderRevision`) | - | n/a | - | MISSING |
| PB-5.2 (§5.2) | Per-OS disk cache, sidecar JSON, atomic writes, lock files, 128 MB LRU, Options FILE LOCATIONS Open/Clear | - | - | - | MISSING |
| PB-5.3 (§5.3) | Shipped `previews.json` manifest behaviour | - | n/a | - | MISSING |
| PB-5.4 (§5.4) | Preset `uid` + `previewPhrase` fields in `PresetManager::fromVar` known list — absent (`PresetManager.cpp:537`) | - | n/a | - | MISSING |
| PB-5.5 (§5.5) | `PresetLibraryPrefs` (favourites, rating, recent 30, load counts) in `preset-library.json` | - | - | - | MISSING |
| PB-5.6 (§5.6) | `PresetIndex` + extended `PresetInfo` (uid, guitarName, family, ampName, modified), incremental — absent (`PresetManager.h:24 PresetInfo` has name/category/author/description/tags/file/isFactory only) | - | n/a | - | MISSING |
| PB-6.1-6.3 (§6.1-6.3) | Parameter features, `ToneFeatures` spectral analysis, `ToneDescriptors` rule table + calibration | - | n/a | - | MISSING |
| PB-6.4 (§6.4) | `PresetSearch` tokenised weighted fuzzy query, no debounce — absent (current `searchBox` is a plain filter) | - | - | - | MISSING |
| PB-6.5 (§6.5) | 24-dim similarity, 8 nearest, exclusions, "still being analysed" footer | - | - | - | MISSING |
| PB-7.1 (§7.1) | Easy layout (search, previews speaker/volume, family chips, Fav/Recent, rows with glyph+waveform+stars, detail + More like this, footer) | - | - | - | MISSING |
| PB-7.2 (§7.2) | Advanced 1040x680 layout: sidebar filters (source, family, genre, techniques, tone, rating), sort options/column sort, SOUNDS LIKE pane, persisted filters | - | - | - | MISSING |
| PB-7.3 (§7.3) | Row glyph states, waveform, author vs auto chips, heart/stars, locked Pro rows, approximate "~" | - | - | - | MISSING |
| PB-7.4 (§7.4) | Browser key table (search focus, nav, Space, Enter, Escape x2, F, 0-5, S, Ctrl+Backspace), overrides global Space/digits, rebindable group | - | - | - | MISSING |
| PB-7.5 (§7.5) | Empty states and cache-unwritable banner | - | - | - | MISSING |
| PB-8 (§8) | Options APPEARANCE "PRESET BROWSER" group, `presetPreview.*` / `presetBrowser.*` UiPreferences keys, footer mirror | - | - | - | MISSING |
| PB-9 (§9) | No params/uiState; load stays a "Load preset" boundary; browsing not undoable — new parts not built on this checkout | - | n/a | - | MISSING |
| PB-10 (§10) | Accessibility (row names, glyph button, stars value, announcements, tab order, reduced motion, localisation) | - | - | - | MISSING |
| PB-11 (§11) | Performance budget table | - | n/a | - | MISSING |
| PB-12 (§12) | Both editions; Free previews locked Pro presets from shipped clips; upsell on load | - | - | - | MISSING |
| PB-13 (§13) | Interactions (morph, snapshots, automation, rhythm/tune, techniques chip, exclusions, Workshop re-render, audition, onboarding) | - | - | - | MISSING |
| PB-14 (§14) | New files under `Presets/Preview`, `Presets/Search`, `UI/PresetBrowser`; CMake `JUCE_USE_OGGVORBIS=1` | - | n/a | - | MISSING |
| PB-15 (§15) | Failure modes table | - | n/a | - | MISSING |
| PB-T1 (§16 PB-01..05) | Rendering tests | - | - | - | MISSING |
| PB-T2 (§16 PB-06..13) | Playback safety tests | - | - | - | MISSING |
| PB-T3 (§16 PB-14..19) | Cache tests | - | - | - | MISSING |
| PB-T4 (§16 PB-20..26) | Search, similarity, library/file tests | - | - | - | MISSING |
| PB-T5 (§16 PB-27..37) | Performance, GUI, a11y, options, editions, combination tests | - | - | - | MISSING |

<!-- counts DONE=1 NO-GUI=0 NO-TEST=0 PARTIAL=1 MISSING=36 OWNED=0 -->
