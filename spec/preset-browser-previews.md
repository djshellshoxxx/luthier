# PRESET BROWSER: PREVIEWS AND SEARCH SPEC

A preset browser you can hear before you commit. Hover or arrow onto a preset and a 2-4 s phrase plays in that
preset's own sound, without loading it. Typing "warm clean" or "djent" narrows the list to presets that really
sound that way, and "Sounds like this" lists the eight nearest presets.

Added 2026-09-24 at the product owner's request, additive to `gui-integration.md`. The browser stays where it is:
an overlay opened from the header preset name or `Ctrl+O`. This spec rebuilds the inside of `PresetBrowserPanel`
(`Source/UI/Overlays.h/.cpp`) and keeps three existing pieces: the "Uses Techniques" chip
(`gui-techniques-updates.md` 7), the Morph row (`ambiguity-resolutions.md` 5.2) and the guitar thumbnail
(`guitar-illustration.md` 15, `gui-engine-dataflow.md` 18).

## 0. Ground rules

1. **Rendered by Luthier's own engine.** Nothing is recorded or sampled. A preview is what `luthier-render` or the
   plugin's offline instance produces from the preset plus a built-in MIDI phrase, and it is bit-identical for
   identical inputs.
2. **The live instance is never touched.** Previewing changes no parameter, guitar, engine state or undo entry.
   Previews are finished clips rendered by a separate offline instance, and browsing stays off the undo stack
   (`action-and-undo.md` 7).
3. **Real-time safe.** On the audio thread a preview is one additive mix of a buffer prepared in advance: no
   allocation, lock, file I/O or resampling (`engine.md` 0.2, `performance-budget.md` 0.4-0.5).
4. **A preview never interrupts playing.** It mixes on top of the live output and never stops, mutes or
   retriggers a sounding note. It does not play while a host or tune transport runs unless the user opts in
   (4.3).
5. **No new automatable parameters.** Everything here is a user preference or library data.
6. **Both editions.** Presets are fundamental (`editions.md` 1), so all of this ships in Free and Pro (section 12).

## 1. User stories

- "I arrow down the list and hear each preset play a fitting phrase at a safe level, without losing my sound."
- "I type `warm clean` and get jazz cleans, not metal presets whose description happens to say 'clean'."
- "Show me presets that sound like this one."
- "I star my stage presets, filter to favourites and recent, and rate them."
- "My saved presets get a preview and tags like the factory ones."
- Free user: "I can hear the Pro presets before deciding to upgrade."

## 2. Where previews come from (decision: hybrid)

Factory and content-pack presets **ship previews rendered at build time**. User presets, third-party presets and
edited factory files are **rendered on a background thread and cached on disk**. The reasons:
- Free cannot render Pro presets, because that code is not in the binary (`editions.md` 0.5). Shipped clips are
  the only way a Free user can hear the 20 locked presets, and Pro uses the same files.
- The first open is instant.
- The cost is small: 36 clips of 64 KB or less is 2.3 MB or less, against the 200 MB budget of
  `factory-content.md` 0.8.
- The factory features are computed once and also calibrate the auto-tagger (6.3).

**Build time.** A new CLI mode, `luthier-render --render-previews <outDir>` (`Tools/RenderCli.cpp`), renders every
`FactoryPresets` definition through the plugin's own `PreviewRenderer` (3.2). It writes:
- `Resources/Presets/Previews/<uid>.ogg`, one per preset;
- the manifest `previews.json` (5.3);
- `Resources/Presets/descriptor-calibration.json`.

A new script, `scripts/render_previews.sh`, runs it in CI, and `luthier_copy_resources` ships the output.
`editions.md` 9's `render_demos.sh` calls this script. A `.luthiercontent` pack may carry `Previews/<uid>.ogg`,
covered by its signed manifest; a pack without previews gets local renders.

**Run time.**
- `PresetManager::saveAs` and `saveCurrent` queue a high-priority render after the atomic write
  (`file-formats.md` 13).
- Opening the browser queues every row that has no valid preview, visible rows first and the rest at idle
  priority.
- Hovering or selecting an unrendered row moves it to the front of the queue.
- Staleness is detected by the sound hash (5.1). An edited file, a changed guitar or user IR, or a new plugin
  version is a cache miss.

## 3. Rendering

### 3.1 Phrases

A new `PreviewPhrase` (`Source/Presets/Preview/PreviewPhrase.h/.cpp`) builds a timed `juce::MidiMessageSequence`,
like `AuditionPhrase` in `Support/AudioExporter.h`. It is a separate enum so that stored `AuditionPhrase` indices
never move. Notes last 3.2 s or less. The clip (phrase plus tail) is hard-capped at 4.0 s and ends with a 250 ms
raised-cosine fade.

| Id | Content | BPM | Chosen for |
|---|---|---|---|
| `acoustic_strum` | G, Cadd9, D strummed D-DU-UDU | 104 | Acoustic, not fingerstyle |
| `fingerstyle` | Travis pattern, C to G/B | 96 | fingerstyle, folk, parlor, DADGAD |
| `nylon_comp` | Am7 to D9 bossa comp | 110 | Classical or nylon, not flamenco |
| `rasgueado` | E Phrygian rasgueado plus an answer | 120 | flamenco |
| `clean_arp` | Cadd9 to Em7 arpeggio, let ring | 90 | Electric with drive < 0.3 |
| `crunch_riff` | A5 C5 D5 stabs plus a pentatonic fill | 112 | drive 0.3-0.6 |
| `highgain_riff` | palm-muted chugs on the lowest string plus 2 stabs | 120 | drive > 0.6, metal, djent |
| `lead_lick` | pentatonic lick with one bend and final vibrato | 100 | lead, shred, solo |
| `jazz_comp` | Dm9 G13 Cmaj9 shells, swung | 120 | jazz, not lead |
| `funk_chop` | E9 16th chops with ghost mutes (off-beat for reggae) | 100 | funk, wah, reggae |
| `twang_lick` | hybrid-picked double stops | 110 | country, twang, rockabilly, surf |
| `slide_lick` | open-G slide phrase | 84 | `slide_mode` on, or slide |
| `ambient_swell` | one swelled chord with a long release | 70 | ambient, swell, shoegaze, room blend > 0.5 |
| `bass_groove` / `bass_slap` / `bass_walk` | root-fifth-octave eighths / slap-pop octaves / walking bar | 100/100/120 | bass family / bass with `slap_armed` / bass with jazz or fretless |

**Selection order:**
1. The preset's own `previewPhrase` field (5.4).
2. The first table row whose words appear in the category, tags or name.
3. A fallback by features: bass family gives `bass_groove`; drive > 0.6 gives `highgain_riff`; drive > 0.3 gives
   `crunch_riff`; nylon gives `nylon_comp`; an acoustic body gives `acoustic_strum`; anything else gives
   `clean_arp`.

**Pitch follows the guitar.** The phrase root is the lowest open string after tuning and capo, so "Drop C Riff"
riffs in C and "5-String Low B" grooves on B0. Lead and twang phrases are raised whole octaves until they fit the
guitar's range.

**Rhythm engine.** When the preset's `rhythmEngine` is on, the phrase becomes two held chords, and the preset's own
pattern plays them at 100 BPM from the offline playhead. The preview then sounds like loading the preset and
holding a chord. Armed techniques apply as they would live, because this is the real engine.

### 3.2 `PreviewRenderer` (`Source/Presets/Preview/PreviewRenderer.h/.cpp`)

The renderer runs on a worker thread, has no UI, and is shared by the plugin and the CLI.
- **Plugin instance.** It uses `LuthierAudioProcessor::createOfflineInstance()`, the factory `AudioExporter` already
  uses. The instance is created lazily, reused, and destroyed 30 s after the queue empties.
- **CLI instance.** It uses `RenderCli.cpp`'s `RenderHost`, moved to `Source/Presets/Preview/PreviewRenderHost.h`.
  The moved host wires `captureGuitarBlock` and `onGuitarBlockLoaded` the way `PluginProcessor.cpp` does, so guitar
  blocks resolve identically in the plugin and the CLI.

**Each job:**
1. `reset()`.
2. Load through the instance's own `PresetManager::loadPreset (File)`, so the fallbacks are those of a live load.
3. `prepareToPlay (48000, 256)` with the default 4x oversampling.
4. Discard 0.5 s of settling.
5. Render the phrase and tail in 256-sample blocks.
6. Analyse the result (6.2).
7. Normalise loudness to BS.1770-4 integrated **-18 LUFS**, lowering the gain further if the 4x true peak would
   exceed **-3 dBTP**. No limiter is used, and the gain goes in the sidecar.
   (output-normalization.md 4.3, 10:) measure with the shared `Source/DSP/Master/Bs1770Meter.h` and 4x
   `TruePeakDetector.h` rather than a meter of its own, and record the clip's `truePeakDbtp` in the sidecar.
   Preview renders always run with output normalization off (`setCalibrationRenderMode`); while it is on,
   `PreviewPlayer` adds `OutputNormalization::getPreviewGainOffsetDb (truePeakDbtp)` (`target + 18` dB, limited
   to -1 dBTP). The calibrator borrows this service's offline instance through a top-priority `calibration`
   lane once the service exists.
8. Encode Ogg Vorbis, 48 kHz stereo, q0.5, 64 KB or less, plus a 64-point peak envelope.

A cancel flag is checked every block. A job that runs longer than 10 s of wall time is abandoned and marked
failed. A Free offline instance renders with Free's neutralised values (`editions.md` 5.1.3) automatically.

### 3.3 `PreviewRenderService` (`Source/Presets/Preview/PreviewRenderService.h/.cpp`)

The service is owned by `LuthierAudioProcessor`, so renders queued by a save finish even with the editor closed.
- **Thread.** One low-priority `juce::Thread`, one job at a time.
- **Priorities.** `interactive` (at most one job; a new request replaces one not yet started), then `onSave`, then
  `background`.
- **Pausing.** Background jobs pause while a transport runs or CPU relief is active (`performance-budget.md` 8).
- **Results.** They are delivered with `MessageManager::callAsync`, guarded by a `WeakReference`.
- **Shutdown.** The service cancels and joins within 500 ms.

## 4. Playback

### 4.1 `PreviewPlayer` (`Source/Presets/Preview/PreviewPlayer.h/.cpp`)

The player is a member of `LuthierAudioProcessor`.
- **Clip.** A `PreviewClip` is an immutable stereo buffer already at the host rate. A loader thread resamples it
  once with `juce::WindowedSincInterpolator`.
- **Handoff.** It follows the `GuitarSpec` swap pattern (`ui-wiring.md` 4.3). The message thread sets
  `std::atomic<const PreviewClip*> pending`. The audio thread takes it and publishes `inUse[2]`, one per voice.
  `PreviewClipPool` frees a clip only when it is neither pending nor in use.
- **Two voices.** When one clip replaces another, the old one fades out over 30 ms and the new one starts after
  that. Two clips are never summed at full level.
- **Fades.** Raised-cosine: 10 ms in and 30 ms on stop, besides the fade built into each clip.
- **Volume.** Smoothed linearly over 20 ms (`engine.md` 0.4).
- **Stopping.** `stop()`, `panic()` (called from `LuthierAudioProcessor::panic`), `prepareToPlay` and
  `releaseResources` fade out and drop the clips. A sample-rate change drops them, and they are resampled on the
  next request.

### 4.2 Insertion point

`LuthierAudioProcessor::processSlice` calls `previewPlayer.processBlock (mainOut, numSamples)`, which adds into main
channels 0-1 (a mono bus gets both, at -3 dB). The call goes after the practice block (looper, session recorder)
and the click mix, and before `routing.distribute (...)`. The result:
- The preview is heard on the main output and in the monitor mix, and the header meter shows it.
- The looper, session recorder, Tone Match `capture`, per-string and aux buses, MIDI out and MIDI export never
  contain it.
- The kill switch silences it: the player reads `killSwitch.isActive()` and fades out.

### 4.3 When a preview may play

| Situation | Hover / auto-on-select | Explicit (glyph, Space) |
|---|---|---|
| Idle | plays | plays |
| Live notes sounding, or a note-on within 500 ms | suppressed | plays on top; live notes untouched |
| Host (`getIsPlaying()`) or tune transport running | suppressed | suppressed, hint "Previews pause while the host plays" |
| ...with `presetPreview.whileTransport` on | plays | plays |
| `isNonRealtime()`, the kill switch engaged, or previews off | never | never |

The UI decides using a `PreviewPlayer::Gate` snapshot of relaxed atomics that the audio thread publishes each
block: playing, non-realtime, live-activity age and kill. The audio thread checks non-realtime and kill again
itself. If `processBlock` has not run for 200 ms, the request is dropped and the footer says "The host is not
processing audio. Arm or monitor the track to hear previews."

### 4.4 Triggers

**Start.**
- **Hover** (the default): the pointer rests on a row for 300 ms.
- **Click**: a click on the row's play glyph. With the "Click only" setting, a click anywhere on the row.
- **Keyboard**: Space toggles the preview. With "Preview on selection" on, an arrow key starts one after 150 ms.

**Stop.** Leaving the row or the list, Escape, Space, closing the overlay, or loading a preset.

A preview plays once and does not loop. The clips of the two neighbouring rows are decoded ahead of time, so
arrowing is instant.

## 5. Data model and files

**5.1 Sound hash.** SHA-256 over:
1. the preset JSON, canonicalised (sorted keys, `%.9g` numbers), minus `name`, `category`, `author`,
   `description`, `tags`, `uid`, `previewPhrase`, `meta` and `pluginVersion`;
2. the canonical resolved `GuitarSpec`, the same serialisation the thumbnail key uses;
3. the size and mtime of each referenced user IR;
4. the phrase id;
5. `JucePlugin_VersionString`;
6. `PreviewRenderer::kRenderRevision` (starts at 1).

A rename or retag keeps the preview, and any change to the sound makes a new one. A plugin update lazily
re-renders user previews, because releases are the finest grain at which the engine is versioned.

**5.2 Disk cache.** Previews are a cache, not user data, so they stay out of `~/Documents/Luthier`, which is often
synced by iCloud or OneDrive:

| OS | Location |
|---|---|
| macOS | `~/Library/Caches/Luthier/PresetPreviews/` |
| Windows | `%LOCALAPPDATA%\Luthier\Cache\PresetPreviews\` |
| Linux | `$XDG_CACHE_HOME/luthier/preset-previews/`, falling back to `~/.cache` |

In Free the folder is "Luthier Free" (`editions.md` 7.2).
- **Entries.** Each is `<hash32>.ogg` plus a sidecar `<hash32>.json` holding schema, magic `luthier.preview`, the
  full hash, phrase, plugin version, render ms, gain dB, `approximate`, the 64 peaks, features (6.2) and
  descriptors (6.3).
- **Writes** are temp file then rename (`file-formats.md` 13).
- **Locking.** A `<hash32>.lock` file created exclusively stops two instances rendering the same hash. A lock older
  than 30 s is stale.
- **Size.** The cap is **128 MB**, evicting the least recently played (each play touches the sidecar's mtime). It
  is pruned at service start and every 50 writes.
- **UI.** Options -> FILE LOCATIONS gains "Preview cache: Open / Clear".

**5.3 Shipped manifest.** `Resources/Presets/Previews/previews.json` has one entry per factory preset: `uid`,
`soundHash`, `phrase`, `pluginVersion`, features, descriptors and peaks.
- A factory file the user edited has a different hash, so the shipped clip is ignored and a local render is queued.
- If `pluginVersion` does not match (dev builds), the shipped clip still plays, marked stale, and a background
  render is queued.

**5.4 Preset file additions.** Two optional tail fields go beside the top-level `name`/`tags`. The as-built writer
puts meta at the root; `file-formats.md` 2 shows a `meta` block. The new fields go wherever `name` is.
- `uid`: a UUID written on the first save of a user preset; factory presets use `factory:<name>`. It keys
  favourites and ratings, so they survive a rename or a move.
- `previewPhrase`: an optional phrase id from 3.1, set by the author.

Both fields are added to the `known` array in `PresetManager::fromVar`. There is no schema bump
(`file-formats.md` 15). Auto-descriptors are **never** written into preset files: they are derived data that
depends on the engine version.

**5.5 Library preferences.** `PresetLibraryPrefs` (`Source/Presets/PresetLibraryPrefs.h/.cpp`) is user-global. It
writes through immediately, like `UiPreferences`, to `~/Documents/Luthier/config/preset-library.json`:
```json
{ "schema": 1, "magic": "luthier.presetlibrary",
  "entries": { "<uid | relative path>": { "favourite": true, "rating": 4, "lastLoaded": "2026-09-24T10:00:00Z", "loadCount": 12 } },
  "recent": ["<uid>", "..."] }
```
`recent` holds the last 30 loads. Presets with no `uid` (read-only folders) are keyed by their path relative to
the search folder. A rename in the browser re-keys the entry.

**5.6 `PresetIndex`** (`Source/Presets/Search/PresetIndex.h/.cpp`). There is one per processor, built on the preview
worker from `PresetManager`. Each entry holds:
- `PresetInfo` plus uid, guitar name and family, and amp name;
- drive index and techniques used, read from `parameters` without loading the preset;
- source (Factory, User, Pack or Extra) and modified time;
- the feature vector and descriptors.

`PresetInfo` gains `uid`, `guitarName`, `family`, `ampName` and `modified`, filled in `scanFolder`. The index
updates incrementally on `PresetManager`'s change broadcast. Names appear at once, and the rest fills in as it
arrives.

## 6. Search and auto-tagging

**6.1 Parameter features**, read from the JSON without loading:
- **drive index** (0-1): `amp_gain` weighted by the amp model's gain class, plus active drive, fuzz and distortion
  pedals weighted by level;
- **reverb amount**: `room_blend` x `room_decay` plus the reverb pedal mix;
- delay mix and compressor amount;
- family and body; pickup type (single, humbucker, piezo); string material (steel, nylon, flat);
- string count and lowest open pitch;
- `slide_mode`, the technique arm flags (`scrape_armed`, `slap_armed` and the others in
  `gui-techniques-updates.md` 2), and whether the rhythm engine is on.

**6.2 Spectral features.** `ToneFeatures` (`Source/Presets/Preview/ToneFeatures.h/.cpp`) is deterministic and uses
`juce::dsp::FFT`. It measures the preview before loudness gain:
- median spectral centroid (2048-point frames, log Hz) and 85 % rolloff;
- flatness over 1-5 kHz, a proxy for grit;
- energy ratios in three bands: below 250 Hz, 250 Hz-2 kHz and above 2 kHz;
- crest factor and attack sharpness (mean onset strength);
- tail decay: seconds to fall 30 dB after the last note-off;
- stereo width (1 minus the L/R correlation);
- loudness before normalisation.

**6.3 Descriptors.** `ToneDescriptors` (`Source/Presets/Search/ToneDescriptors.h/.cpp`) holds a fixed vocabulary.
Each rule gives a confidence from 0 to 1, and a descriptor is attached at 0.5 or above. `pNN` means a percentile of
the factory corpus, calibrated separately for guitar and bass and shipped in `descriptor-calibration.json`.

| Descriptor (searched synonyms) | Rule |
|---|---|
| warm (dark, mellow, smooth, round) | centroid < p35, high band < p40, flatness < p50 |
| bright (sparkly, chimey, glassy, crisp) | centroid > p65 or high band > p70 |
| clean | drive < 0.25 and flatness < p50 |
| crunchy (crunch, gritty, breakup, edge of breakup) | drive 0.3-0.6 |
| high-gain (heavy, metal, chug, distorted) | drive > 0.6 |
| fuzzy (fuzz) | active fuzz pedal, or flatness > p85 with drive > 0.5 |
| djent | high-gain, and (7 or more strings or lowest pitch B1 or below), and tail < p30 |
| spacious (ambient, wet, washy, big) | tail > p75, or reverb > 0.5, or delay > 0.35 |
| dry (tight, close) | tail < p25 and delay < 0.1 |
| twangy (twang, snappy) | single-coil, attack > p70, bright |
| percussive | attack > p80 and crest > p70 |
| compressed | compression > 0.4 or crest < p20 |
| fat (thick, full) | low band > p65 and mid band > p50 |
| jazz | warm and clean, and (hollow or archtop, or neck humbucker, or jazz genre) |
| acoustic, nylon, bass, slide | taken directly from 6.1 |

Genres (the `factory-content.md` 0.4 list) come from category, tags and name, and drive the Genre filter.

**6.4 Query.** `PresetSearch` (`Source/Presets/Search/PresetSearch.h/.cpp`) runs on the message thread over the
in-memory index.
- **Tokens.** The query is lowercased and split on whitespace. Quoted phrases and multi-word vocabulary ("edge of
  breakup") are matched first.
- **Matching.** Every token must match some field, by prefix. Tokens of 5 or more characters tolerate one Damerau
  edit.
- **Weights.** name 5, author tags 4, descriptors 3 x confidence, category and genre 3, guitar and amp 2, author
  1.5, description 1. Results sort by score, then name.
- **Speed.** The list refilters on every keystroke with no debounce (budget in 11).

**6.5 Similarity.** Each preset has a 24-dimension vector, z-scored by the calibration file:
- 10 spectral features;
- drive, reverb, delay and compression;
- family one-hot (4), pickup one-hot (3), nylon and flat (2), and slide (1).

The weights are spectral 1.0, drive 1.5, family 2.0, the rest 0.7. The 8 nearest by weighted Euclidean distance
are found by brute force. The preset itself, presets with the same sound hash and anything excluded by the Source
filter are left out. Presets not yet analysed are skipped, and the footer shows "N presets still being analysed".

## 7. UI

The browser is still one overlay, and its layout follows the editor mode. `PresetBrowserPanel` moves to
`Source/UI/PresetBrowser/`. `PluginEditor`'s `showOverlay (&presetBrowser)`, `header.onOpenPresetBrowser` and the
`presetBrowser` shortcut id are unchanged.

**7.1 Easy Mode** (780 x 560, the current size):
```
| PRESETS                                                           [Close] |
| [Search: warm clean__________________ x]      (spk)[===o--] Previews      |
| (All)(Electric)(Acoustic)(Classical)(Bass) (Fav)(Recent)(Uses Techniques) |
| > (h) Jazz Hollowbody   warm clean jazz  *****  | [guitar thumbnail]      |
|   (h) Semi-Hollow Chime bright clean     ***    | name, category, chips   |
|   [play glyph + 64-peak waveform per row]       | description             |
|                                                 | [More like this]        |
| [Save As...] [Morph] A:... ===o=== B:...                 [Delete] [Load]  |
```
- **Chips.** Family chips are single-select. Fav and Recent are toggles.
- **Sort.** Relevance while there is a query, otherwise by name.
- **"More like this"** shows the 8 similar presets, with a breadcrumb "Like: <name> [x]" to go back.

**7.2 Advanced Mode** (1040 x 680, clamped to the window minus 40 px):
```
| SOURCE      | [Search_______________] Sort [Relevance v]   | thumbnail     |
| FAMILY      | > (h) Name  Category  Guitar  Amp  Tags  Rating | chips, author |
| GENRE v     |   ...                                        | guitar, amp   |
| TECHNIQUES  |                                              | techniques    |
|  (Uses Techniques + six) |                                 | SOUNDS LIKE   |
| TONE chips  |                                              |  8 rows, each |
| RATING >=   |                                              |  with play    |
| (h)Fav Recent [Clear filters]                              |               |
| [Save As...] [Morph row]       (spk)[vol] Previews: On     [Delete] [Load] |
```
- **Sort options.** Relevance (only with a query), Name, Category, Rating, Recently loaded, Most loaded, Date
  modified, and Similarity (only in the "Sounds like" view). Column headers also sort.
- **Filters.** OR within a group, AND across groups. The filter state persists in UiPreferences.

**7.3 Rows and detail pane.**
- **Play glyph**, one state at a time:
  - play when idle;
  - a sweep while playing;
  - a spinner, "Preparing preview...", while rendering;
  - "!" on failure, with the tooltip "Preview could not be rendered: <reason>. The preset can still be loaded.";
  - hidden when previews are off.
- **Waveform.** The row waveform comes from the peaks; the part already played is in the accent colour.
- **Tags.** Author tags are filled chips. Auto-descriptors are outlined chips with the tooltip "Detected from the
  sound". Clicking a chip adds it to the search.
- **Favourite and rating.** The heart toggles favourite. The stars set a rating from 1 to 5; clicking the current
  star clears it.
- **Locked Pro presets (Free).** They show a lock and still preview. Enter or Load opens the upsell panel
  (`editions.md` 4.2).
- **Approximate.** Renders that used a fallback show "~", with the tooltip "Rendered with a fallback guitar;
  loading will show which part is missing."

**7.4 Keyboard.** The focused overlay consumes Space and the digits, overriding the global tune play/pause and
snapshot keys of `gui-integration.md` 17.

| Key | Action |
|---|---|
| a printable key, or Ctrl+F | focus the search box |
| Down (in search) | move into the list |
| Up / Down | move the selection (auto-preview per 4.4) |
| PgUp / PgDn / Home / End | jump |
| Space | toggle the preview |
| Enter, or double-click | load |
| Escape | stop the preview if one is playing, otherwise close (two presses at most) |
| F | toggle favourite |
| 0-5 | set the rating (0 clears it) |
| S | "Sounds like this" |
| Ctrl+Backspace | clear the search and filters |

These keys are rebindable in Options -> ACCESSIBILITY, group "Preset browser".

**7.5 Empty states and errors.**
- **No results:** "No presets match "warm djent". Try fewer words or clear filters." with a [Clear filters] button.
- **Favourites empty:** "Tap the heart on a preset to keep it here."
- **Recent empty:** "Presets you load appear here."
- **Previews off:** the footer shows "Previews are off - Options -> Appearance". The transport and host hints of
  4.3 also appear in the footer.
- **Cache not writable:** previews play from memory, and one banner appears: "Preview cache folder is not
  writable; previews will be re-rendered next time."

## 8. Options

A new **PRESET BROWSER** group at the bottom of Options -> APPEARANCE (`AppearancePage`), beside the tooltips
switch. The values are `UiPreferences` keys, written immediately. The speaker icon and slider in the browser
footer mirror the same keys.

| Key | Control | Default |
|---|---|---|
| `presetPreview.enabled` | Previews on/off | On |
| `presetPreview.trigger` | Hover, or Click only | Hover |
| `presetPreview.onSelect` | Preview on keyboard selection | On |
| `presetPreview.volumeDb` | Preview volume, -40 to 0 dB | -6 dB |
| `presetPreview.whileTransport` | Preview while the host or a tune plays | Off |
| `presetBrowser.sort`, `.filters`, `.viewSimilar` | remembered view | - |

At the defaults a preview plays at about -24 LUFS, with peaks at -9 dBFS or below.

## 9. Parameters, state, undo

- **Parameters:** none added.
- **Presets:** only the optional `uid` and `previewPhrase` fields (5.4), which round-trip.
- **Session and uiState:** nothing. The view lives in UiPreferences, and favourites and ratings in
  `preset-library.json`.
- **Undo.** Loading keeps its existing state boundary (`loadSelected()` calls `pushUndoState ("Load preset")`).
  Previewing, filtering, sorting, favourites, ratings and "Sounds like" are navigation or library data, so none
  of them goes on the undo stack (`action-and-undo.md` 3.8, 7).

## 10. Accessibility

- **Rows.** Each row's `AccessibilityHandler` is named like "Jazz Hollowbody, Electric Jazz, factory, favourite,
  4 stars, warm, clean, jazz".
- **Controls.**
  - The play glyph is the button "Preview <name>".
  - The stars expose an `AccessibleValueInterface` from 0 to 5.
  - The heart and the chips are toggles.
- **Announcements.** "Previewing <name>" and "Preview ready" are polite and need Normal verbosity or higher.
  Failures are always announced.
- **Tab order:** search, chips or sidebar, list, detail pane, footer.
- **Reduced motion.** A static "playing" label replaces the sweep, and the text "Preparing" replaces the spinner.
- **Colour.** Author and auto chips differ by fill versus outline, not by colour alone.
- **No hover-only paths.** The keyboard and clicks reach everything hover does.
- **Localisation.** Descriptor words are in the locale catalogue, and search matches both the localised word and
  the English one.

## 11. Performance budget

These use the reference CPU of `performance-budget.md`.

| Item | Budget |
|---|---|
| Audio-thread preview mix | 0.02 units or less; zero allocations and zero locks |
| Trigger to audible | 30 ms or less if decoded, 60 ms or less from disk (after the dwell) |
| Render one 4 s preview | steady-state preset 1.0 s wall or less on one core; heaviest factory preset ("8-String Djent", "Physics Showcase") 2.0 s or less |
| Offline instance | cold create 400 ms or less; 300 MB RSS extra or less while alive; freed after 30 s idle; total under the 900 MB cap |
| Memory and disk | decoded clips 16 MB or less; cache 128 MB or less; shipped factory previews 3 MB or less |
| Search keystroke to repaint | 16 ms or less at 5,000 presets, 4 ms or less at 500; similarity 2 ms or less at 5,000 |
| Browser open | 150 ms or less with a warm index; cold index build 1.5 s or less per 500 presets, on the worker |

## 12. Editions

Everything in this spec ships in both Free and Pro, as "Both" rows under `editions.md` 2.3 ("browse, tags,
favourites"), following rule 0.2.
- **Free.** It previews its 16 factory presets and the user's presets, and previews the 20 locked Pro presets from
  shipped clips.
- **Locked Pro presets.** Loading one opens the upsell panel. They can appear in "Sounds like this" with a lock.
  This is rule 0.8's quiet upsell: no modal and no timer.
- **Local renders in Free** use Free's effective values, so no Free user hears a sound their edition cannot play.

## 13. Interactions

- **Morph.** Enter loads into the selected slot, as `loadSelected()` does now.
- **Snapshots.** A preview is the base state, which is what a load gives. Snapshot recall is unaffected.
- **Host automation and MIDI Learn.** Untouched, because previews write no parameters.
- **Rhythm engine and Tune Builder.** The live engines are untouched, and a running tune blocks previews (4.3). A
  preset's own rhythm engine shapes its preview (3.1).
- **Techniques.** The "Uses Techniques" chip is kept in the Easy chip row and in the Advanced TECHNIQUES group,
  driven by the arm flags of 6.1.
- **MIDI export, notation, capture, looper, session recorder, aux and per-string buses.** Previews are excluded by
  the insertion point in 4.2.
- **Workshop.** Saving a guitar changes the hashes of the presets that reference it, so they re-render when next
  viewed.
- **Audition button and Workshop shadow audition.** These are live notes. A preview mixes on top of them, and
  neither stops the other.
- **Onboarding.** The preset stop of the tour gains "Hover to hear, Enter to load".

## 14. New and changed code

**New files:**
- `Source/Presets/Preview/`: `PreviewPhrase`, `PreviewRenderer`, `PreviewRenderHost`, `PreviewRenderService`,
  `PreviewCache`, `PreviewClipPool`, `PreviewPlayer`, `ToneFeatures`.
- `Source/Presets/Search/`: `PresetIndex`, `PresetSearch`, `ToneDescriptors`.
- `Source/Presets/PresetLibraryPrefs`.
- `Source/UI/PresetBrowser/`: `PresetBrowserPanel` (moved), `PresetRow`, `PresetFilterSidebar`,
  `PresetDetailPane`.
- `scripts/render_previews.sh`, and `Resources/Presets/Previews/` with `descriptor-calibration.json`.

**Changed files:**
- `PresetManager`: new `PresetInfo` fields; `uid` on save; render request on save; the known keys.
- `LuthierAudioProcessor`: owns the player and the service; the insertion in `processSlice`; `panic`.
- `RenderCli.cpp`: `--render-previews`.
- `OptionsPages`: `AppearancePage` and `FileLocationsPage`.
- `CMakeLists.txt`: `JUCE_USE_OGGVORBIS=1` and the new sources in the plugin, `LuthierTests` and `LuthierRender`.

## 15. Failure modes

| Failure | Response |
|---|---|
| Corrupt or refused preset | No preview; the row shows "!". The row is still searchable by name, and a load gives the normal banner. |
| Guitar or IR missing at render | Render with the load fallback and mark it approximate. |
| Render throws, or runs past 10 s | Mark failed and log. Retry once on the next browser open, then only after the hash changes. |
| Cached clip fails to decode | Delete it and render again. |
| Cache unwritable, or the disk full | Previews live in memory for the session; show one banner. |
| Sample-rate change mid-preview | Fade and stop; resample on the next request. |
| Editor closes during a preview | `overlayHidden()` fades the preview out. On-save renders continue. |
| Processor destroyed | Cancel and join in 500 ms or less. Clips are freed once the audio thread releases them. |
| Host stops calling `processBlock` | Drop the request and show the footer hint. |
| Shipped manifest missing | Factory presets render locally. |

## 16. Tests

Unit tests are in `LuthierTests`, in `PresetPreviewTests.cpp` and `PresetSearchTests.cpp`. GUI tests run under
xvfb in `PresetBrowserUiTests.cpp`, in the style of `EditorTests.cpp`. Combination tests use the
`CombinationTests.cpp` harness.

**Rendering**
1. **PB-01** Rendering the same preset twice gives bit-identical float buffers and identical Ogg bytes.
2. **PB-02** Each factory preset's fresh render matches its shipped clip: log spectrum within 1.0 dB RMS over 40
   bands, and loudness within 0.5 LU. The manifest `soundHash` equals the hash of `FactoryPresets::toVar`.
3. **PB-03** Every clip is 2.0-4.0 s long, measures -18 ±0.5 LUFS (unless peak-bound), has a true peak of
   -3.0 dBTP or less, and its last 10 ms are below -60 dBFS.
4. **PB-04** Phrases are chosen correctly:
   - "Modern Metal Chug" and "8-String Djent" get `highgain_riff`;
   - "Strummed Dreadnought" gets `acoustic_strum`;
   - "P-Bass Flatwound" gets `bass_groove`;
   - "Flamenco Rasgueado" gets `rasgueado`;
   - "Blues Slide" gets `slide_lick`;
   - a `previewPhrase` field overrides the choice.
5. **PB-05** The lowest phrase note is C2 for "Drop C Riff" and B0 for "5-String Low B".

**Playback safety**
6. **PB-06** After 20 previews, the live APVTS, `GuitarSpec` hash, snapshot bank and undo depth are unchanged.
7. **PB-07** Switching previews every 150 ms for 60 s with the heap and mutex traps armed gives zero traps.
8. **PB-08** Fades:
   - the first 10 ms rises monotonically;
   - a stop reaches -90 dBFS within 30 ms;
   - during a switch, the two clips are never both above -60 dBFS at the same sample.
9. **PB-09** A held live note's per-string aux output nulls below -120 dB against the same run with no preview.
10. **PB-10** Transport rules:
    - with the host playhead playing, hover and Space are silent;
    - with `whileTransport` on, they play;
    - under `isNonRealtime()` a preview is always silent;
    - the kill switch silences a preview within 30 ms.
11. **PB-11** The looper, session recorder, capture, aux buses and MIDI out null against, or are byte-equal to,
    the same run with no preview.
12. **PB-12** At -6 dB volume with no playing, the output peak is -9 dBFS or less. A volume change never moves the
    linear gain by more than 0.01 per sample.
13. **PB-13** A switch from 44.1 kHz to 96 kHz mid-preview stops it cleanly, and the next preview's centroid is
    within 1 % of the reference.

**Cache**
14. **PB-14** A rename or retag keeps the sound hash. Changing one parameter, the guitar file, the version string or
    `kRenderRevision` changes it.
15. **PB-15** After `saveAs`, a cache entry with features and descriptors exists within 3 s.
16. **PB-16** A cache filled to 130 MB is pruned to 128 MB or less, evicting the least recently played. Locks older
    than 30 s are ignored.
17. **PB-17** Two processors requesting the same hash cause exactly one render, and both play it.
18. **PB-18** Failures:
    - a corrupt preset gives a failed row and no crash;
    - a forced timeout is abandoned within 10.5 s;
    - an unwritable cache plays from memory and shows exactly one banner.
19. **PB-19** Destroying the processor mid-render returns within 500 ms, with no leaks.

**Search**
20. **PB-20** Factory descriptors:
    - "Jazz Hollowbody" is warm, clean and jazz;
    - "8-String Djent" is high-gain and djent;
    - "Ambient Swell" is spacious;
    - "Semi-Hollow Chime" is bright;
    - "Single-Cut Crunch" is crunchy;
    - no Bass preset is djent.
21. **PB-21** Queries:
    - "warm clean" puts "Jazz Hollowbody" in the top 3, and no result has drive above 0.5;
    - "djent" puts "8-String Djent" first;
    - "cruchy" returns the same set as "crunchy";
    - "clean jazz" returns the same set as "jazz clean".
22. **PB-22** An author-, guitar- or amp-only match is found. A name match outranks a description-only match.
23. **PB-23** Filters:
    - Family=Bass and Genre=funk AND together;
    - two genres OR together;
    - "Uses Techniques"+Slap returns exactly the presets with `slap_armed` on;
    - Favourites and Recent match `preset-library.json`.
24. **PB-24** The "Sounds like" results for "Modern Metal Chug" include "Drop C Riff" and "8-String Djent", contain
    no acoustic or classical preset, never the preset itself, and are deterministic.

**Library and file**
25. **PB-25** Favourites and ratings survive a rename, an editor reopen and a new processor, and are never written
    into the preset file.
26. **PB-26** `uid` is created on the first save and kept afterwards, and `previewPhrase` is preserved. The
    existing byte-identical round-trip test (`file-formats.md` 16) still passes.

**Performance**
27. **PB-27** At 5,000 synthetic index entries:
    - search takes 16 ms or less per keystroke;
    - similarity takes 2 ms or less;
    - a warm browser open takes 150 ms or less;
    - the preview mix costs 0.02 units or less;
    - a steady-state render takes 1.0 s or less (CI perf job).

**GUI**
28. **PB-28** Easy mode, keyboard:
    - `Ctrl+O` opens the browser;
    - typing focuses search, and Down moves into the list;
    - Space starts a preview, and the player reports it active;
    - Escape stops it, and a second Escape closes the browser;
    - Enter loads and pushes a "Load preset" boundary.
29. **PB-29** Advanced mode: the sidebar, sort, columns and "Sounds like" pane lay out without overlap at 1280 x 800
    and at 2560 x 1600. In Easy mode they are absent, and the chip row and "More like this" are present.
30. **PB-30** Resting on a row for 300 ms starts a preview, and leaving the list stops it within 30 ms. With the
    "Click only" trigger, hovering never starts one.
31. **PB-31** With the browser focused, digits and Space do not recall snapshots or toggle the tune transport. With
    it closed, they do.
32. **PB-32** Accessibility:
    - every row, glyph, heart, star control and chip has an accessible name;
    - the tab order matches section 10;
    - "Previewing <name>" is announced;
    - reduced motion gives pixel-stable frames while a preview plays.
33. **PB-33** The five `presetPreview.*` keys persist across instances, and the footer mirror stays in sync. With
    previews off, the glyphs are hidden and every trigger is blocked.
34. **PB-34** A nonsense query shows the no-results text and Clear filters. Empty Favourites and Recent show their
    hints.

**Editions and combinations**
35. **PB-35** In the Free build:
    - the 20 Pro presets are listed locked and preview from shipped clips;
    - Enter on one opens the upsell panel;
    - a user preset with `slap_armed` on renders identically to the same preset with it off.
36. **PB-36** A preview during Morph, tune playback with `whileTransport` on, live slide mode, an active mod matrix
    and a MIDI Learn arm leaves every live module's output unchanged compared with no preview, and no assertion
    fires.
37. **PB-37** The browser, its Options group and the cache Open/Clear buttons are each within 3 interactions of the
    header, in both modes (`gui-integration.md` 0.2).
