# PRESET BROWSER: PREVIEWS AND SEARCH SPEC

A preset browser you can hear before you commit. Hover or arrow onto a preset and a 2-4 second
phrase plays in that preset's own sound, without loading it. Type "warm clean" or "djent" and the
list narrows to presets that really sound that way. "Sounds like this" lists the eight nearest.

Added 2026-09-24 at the product owner's request, additive to `gui-integration.md`. The browser stays
where it is: the header preset name or `Ctrl+O` opens it as an overlay. This spec rebuilds the inside
of `PresetBrowserPanel` (`Source/UI/Overlays.h/.cpp`) and keeps three existing pieces:
- the "Uses Techniques" chip (`gui-techniques-updates.md` 7);
- the Morph row (`ambiguity-resolutions.md` 5.2);
- the guitar thumbnail (`guitar-illustration.md` 15, `gui-engine-dataflow.md` 18).

## 0. Ground rules

1. **Every preview is rendered by Luthier's own engine.** Nothing is recorded, sampled or made by
   hand. A preview is what `luthier-render` (or the plugin's offline instance) produces from the
   preset plus a built-in MIDI phrase. The same inputs give the same bits.
2. **Previewing never touches the live instance.** No parameter, guitar, engine state or undo entry
   of the playing instance changes. Previews come from a separate offline instance and play back as
   finished clips. Browsing stays off the undo stack (`action-and-undo.md` 7).
3. **Real-time safe.** On the audio thread, playback is one additive mix of a buffer prepared in
   advance. There is no allocation, lock, file I/O or resampling there (`engine.md` 0.2,
   `performance-budget.md` 0.4-0.5).
4. **A preview never interrupts the player.** It mixes on top of the live output and never stops,
   mutes or retriggers a sounding note. It never plays while a host or tune transport runs unless
   the user asks for that (4.3).
5. **No new automatable parameters.** Everything here is a user preference or library data, not
   sound state.
6. **Presets are fundamental** (`editions.md` 1), so all of this is in both editions (section 12).

## 1. User stories

- "I arrow down the list and hear each preset play a fitting phrase at a safe level, without losing
  the sound I have loaded."
- "I type `warm clean` and get the jazz cleans, not metal presets that mention 'clean'."
- "I like this one; show me others like it."
- "I star my stage presets, filter to favourites, and see what I loaded recently."
- "When I save a preset, it gets a preview and tags just like the factory ones."
- Free user: "I can hear the Pro presets before I decide to upgrade."

## 2. Where previews come from (decision: hybrid)

Factory and content-pack presets **ship previews rendered at build time**. User presets,
third-party presets and edited factory files are **rendered on a background thread and cached on
disk**.

Why hybrid:
- **Free needs shipped audio.** Free cannot render Pro presets, because Pro code is absent from the
  Free binary (`editions.md` 0.5). Shipped clips are the only way to hear the 20 locked presets,
  and Pro uses the same files.
- **Instant first open.** A new user's first hover plays immediately, with no queue of 36 renders.
- **Negligible size.** 36 clips at 64 KB or less each come to 2.3 MB or less, against the 200 MB
  content budget (`factory-content.md` 0.8).
- **Consistent tagging.** The factory features are computed once, and they also calibrate the
  auto-tagger for everyone (6.3).

### 2.1 Build time

- **Rendering.** A new mode, `luthier-render --render-previews <outDir>` (`Tools/RenderCli.cpp`),
  renders every `FactoryPresets` definition through the same `PreviewRenderer` the plugin uses
  (3.2). It writes:
  - `Resources/Presets/Previews/<uid>.ogg` for each preset;
  - the manifest `previews.json` (5.3);
  - `Resources/Presets/descriptor-calibration.json` (6.3).
- **CI.** `scripts/render_previews.sh` (new) runs after the build. `luthier_copy_resources` copies
  the output. The `scripts/render_demos.sh` of `editions.md` 9 calls it.
- **Content packs.** A `.luthiercontent` pack may carry `Previews/<uid>.ogg`, covered by its signed
  manifest. A pack without previews gets local renders.

### 2.2 Run time

- **On save.** `PresetManager::saveAs` and `saveCurrent` queue a high-priority render after the
  atomic write (`file-formats.md` 13).
- **On first view.** Opening the browser queues the rows with no valid preview: visible rows
  first, then the rest of the filtered list at idle priority. Hovering or selecting an unrendered
  row moves it to the front.
- **Staleness.** Invalidation is by sound hash (5.1). Any of these is a cache miss: an edited file,
  a changed referenced guitar or user IR, or a new plugin version.

## 3. Rendering

### 3.1 Phrases

A new class `PreviewPhrase` (`Source/Presets/Preview/PreviewPhrase.h/.cpp`) builds each phrase as a
timed `juce::MidiMessageSequence`, in the style of `AuditionPhrase` in `Support/AudioExporter.h`.
It is a separate enum so the stored `AuditionPhrase` indices never move.

The notes of a phrase last 3.2 s or less. The clip is the phrase plus its tail, hard-capped at
4.0 s, and ends with a 250 ms raised-cosine fade.

| Id | Content | BPM | Chosen for |
|---|---|---|---|
| `acoustic_strum` | G, Cadd9, D strummed D-DU-UDU | 104 | Acoustic that is not fingerstyle |
| `fingerstyle` | Travis pattern on C, then G/B | 96 | fingerstyle, folk, parlor, DADGAD |
| `nylon_comp` | Am7 to D9 bossa comp | 110 | Classical or nylon, not flamenco |
| `rasgueado` | E Phrygian rasgueado plus a single-note answer | 120 | flamenco |
| `clean_arp` | Cadd9 to Em7 arpeggio, let ring | 90 | Electric, drive index < 0.3 |
| `crunch_riff` | A5 C5 D5 stabs plus a pentatonic fill | 112 | drive index 0.3-0.6 |
| `highgain_riff` | palm-muted chugs on the lowest string plus 2 power-chord stabs | 120 | drive > 0.6, metal, djent |
| `lead_lick` | pentatonic lick, one bend, vibrato on the end | 100 | lead, shred, solo |
| `jazz_comp` | Dm9, G13, Cmaj9 shells, swung | 120 | jazz (not lead) |
| `funk_chop` | E9 16th chops with ghost mutes (off-beat variant for reggae) | 100 | funk, wah, reggae |
| `twang_lick` | hybrid-picked double stops | 110 | country, twang, rockabilly, surf |
| `slide_lick` | open-G slide phrase | 84 | `slide_mode` on, or slide |
| `ambient_swell` | one swelled chord with a long release | 70 | ambient, swell, shoegaze, room blend > 0.5 |
| `bass_groove` | root-fifth-octave eighths | 100 | bass family |
| `bass_slap` | slap and pop octaves | 100 | bass with `slap_armed` |
| `bass_walk` | one bar of walking line | 120 | bass with jazz or fretless |

**How a phrase is chosen**, first match wins:
1. The preset's optional `previewPhrase` field (5.4).
2. The table, top to bottom. A row matches when its words appear in the preset's category, tags or
   name (case-insensitive).
3. A fallback by features:
   - bass family: `bass_groove`;
   - drive above 0.6: `highgain_riff`;
   - drive above 0.3: `crunch_riff`;
   - nylon strings: `nylon_comp`;
   - acoustic body: `acoustic_strum`;
   - anything else: `clean_arp`.

**Pitch follows the guitar.** A phrase is written relative to a root. The root is set to the
guitar's lowest open string after tuning and capo, so "Drop C Riff" riffs in C and "5-String Low B"
grooves on B0. Lead and twang phrases are raised by whole octaves until they fit the guitar's range.

**Rhythm engine.** If the preset has `rhythmEngine` enabled, the phrase becomes two held chords.
The preset's own pattern plays them at a fixed 100 BPM from the offline host's playhead. What you
preview is what you get when you load the preset and hold a chord.

**Techniques.** Armed techniques (scrape, slap, slide, mute grid, tap, bend) apply exactly as they
would live, because this is the real engine.

### 3.2 `PreviewRenderer`

The renderer is `Source/Presets/Preview/PreviewRenderer.h/.cpp`. It has no UI, runs on a worker
thread, and is shared by the plugin and the CLI.

**The offline instance.** The plugin gets it from `LuthierAudioProcessor::createOfflineInstance()`,
the same factory `AudioExporter` uses. It is created lazily, reused between jobs, and destroyed 30 s
after the queue empties. The CLI instead uses `RenderCli.cpp`'s `RenderHost`, which moves to
`Source/Presets/Preview/PreviewRenderHost.h`. The move also wires `captureGuitarBlock` and
`onGuitarBlockLoaded` the way `PluginProcessor.cpp` does, so the CLI resolves the guitar block
exactly like the plugin.

**Each job:**
1. Call `reset()`.
2. Load the preset through the instance's own `PresetManager::loadPreset (File)`, so guitar
   resolution and fallbacks match a live load.
3. Call `prepareToPlay (48000, 256)`. Oversampling stays at the default 4x.
4. Render 0.5 s of settle-in and discard it.
5. Render the phrase and its tail in 256-sample blocks.
6. Analyse the result (6.2).
7. Normalise the loudness (below).
8. Encode (below).

**Loudness.** Measure BS.1770-4 integrated loudness and gain the clip to **-18 LUFS**. If the 4x
true peak would then exceed **-3 dBTP**, lower the gain further. No limiter is used. The applied
gain goes in the sidecar.

**Encoding.** Ogg Vorbis, 48 kHz stereo, quality 0.5, 64 KB or less. The renderer also stores a
64-point peak envelope for the row waveform.

**Cancellation.** A cancel flag is checked every block. A job that runs past **10 s of wall time**
is abandoned and marked failed.

**Free edition.** A Free offline instance is a Free build, so it renders with the neutralised
effective values of `editions.md` 5.1.3 automatically.

### 3.3 `PreviewRenderService`

`Source/Presets/Preview/PreviewRenderService.h/.cpp` is owned by `LuthierAudioProcessor`, so
on-save renders finish even when the editor is closed.

- **The thread.** One low-priority `juce::Thread` renders one job at a time.
- **The queue** has three priorities:
  - `interactive`: at most one job; a newer request replaces one that has not started;
  - `onSave`;
  - `background`.
- **Pausing.** Background jobs pause while the host or tune transport runs, and while CPU relief
  is engaged (`performance-budget.md` 8).
- **Results** reach the message thread through `MessageManager::callAsync`, guarded by a
  `WeakReference`.
- **Shutdown**, called from the processor destructor, cancels and joins within 500 ms.

## 4. Playback

### 4.1 `PreviewPlayer`

`PreviewPlayer` (`Source/Presets/Preview/PreviewPlayer.h/.cpp`) is a member of
`LuthierAudioProcessor`.

**Clips.** A `PreviewClip` is an immutable stereo buffer at the host rate. It is resampled once on
a loader thread with `juce::WindowedSincInterpolator`.

**Handoff**, in the pattern of the `GuitarSpec` swap (`ui-wiring.md` 4.3):
- The message thread sets `std::atomic<const PreviewClip*> pending`.
- The audio thread takes it and publishes `inUse[2]`, one slot per voice.
- `PreviewClipPool` frees a clip only when it is neither pending nor in use.

**Two voices** let one clip replace another: the old clip fades out over 30 ms and the new one
starts after that fade. Two clips are never summed at full level.

**Fades and level.** Fades are raised-cosine: 10 ms in and 30 ms on stop, plus the fade baked into
the clip's end. The volume (section 8) is a gain smoothed linearly over 20 ms (`engine.md` 0.4).

**Stopping.** These all fade out and release the clips:
- `stop()`;
- `panic()`, called from `LuthierAudioProcessor::panic`;
- `prepareToPlay`;
- `releaseResources`.

A sample-rate change drops the clips; they are resampled again on the next request.

### 4.2 Insertion point

In `LuthierAudioProcessor::processSlice`, call `previewPlayer.processBlock (mainOut, numSamples)`
(additive on channels 0-1, a mono bus gets both at -3 dB):
- **after** the practice block (looper, session recorder) and the tune and metronome click mix;
- **before** `routing.distribute (...)`.

As a result:
- **What hears it.** The preview reaches the main output and the monitor mix. The header output
  meter shows it.
- **What never hears it.** The looper, the session recorder, Tone Match `capture`, the per-string
  and aux buses, MIDI out and MIDI export.
- **Kill switch.** `PreviewPlayer` reads `killSwitch.isActive()` and fades to silence.

### 4.3 When a preview may play (transport rules)

| Situation | Hover or auto-on-select | Explicit (glyph, Space) |
|---|---|---|
| Idle | plays | plays |
| Live notes sounding, or a MIDI note-on within 500 ms | suppressed | plays on top; the live notes are untouched |
| Host transport playing (`getIsPlaying()`) or Tune transport playing | suppressed | suppressed; hint "Previews pause while the host plays" |
| ... with `presetPreview.whileTransport` on | plays | plays |
| `isNonRealtime()` (offline bounce) | never | never |
| Kill switch engaged | silent | silent |
| Previews off | never | never (glyph hidden) |

Two checks enforce this:
- **UI side.** The UI decides from a `PreviewPlayer::Gate` snapshot. The audio thread publishes it
  each block with relaxed atomics: playing, non-realtime, live-activity age and kill.
- **Audio side.** The audio thread checks non-realtime and kill again, because the UI's snapshot
  can be a frame old.

If `processBlock` has not run for 200 ms, the request is dropped and the footer says: "The host is
not processing audio. Arm or monitor the track to hear previews."

### 4.4 Triggers

**Starting a preview:**
- **Hover** (the default trigger): the pointer rests on a row for 300 ms.
- **Click:** a click on the row's play glyph. With the trigger set to "Click only", a click
  anywhere on the row.
- **Keyboard:** Space toggles the preview of the selected row. With "Preview on selection" on,
  arrowing to a row starts its preview after 150 ms.

**Stopping a preview:**
- the mouse leaves the row, or the list;
- Escape, or Space;
- the overlay closes;
- a preset loads.

Previews play once and do not loop. The clips of the rows above and below the selection are
decoded ahead of time, so arrowing is instant.

## 5. Data model and files

### 5.1 Sound hash

The sound hash is SHA-256 over six inputs:
1. The preset JSON, canonicalised (sorted keys, `%.9g` numbers), with the descriptive fields
   removed: `name`, `category`, `author`, `description`, `tags`, `uid`, `previewPhrase`, `meta`,
   `pluginVersion`.
2. The canonical serialisation of the resolved `GuitarSpec`, the same one the thumbnail key uses.
3. The size and mtime of each referenced user IR.
4. The phrase id.
5. `JucePlugin_VersionString`.
6. `PreviewRenderer::kRenderRevision`, starting at 1.

What that means:
- Renaming or retagging a preset keeps its preview. Changing its sound makes a new one.
- A plugin update lazily renders user previews again. Releases are the finest grain at which the
  engine is versioned, so that is the safe choice.

### 5.2 Disk cache

The cache is not user data, so it stays out of `~/Documents/Luthier`, which is often synced by
iCloud or OneDrive:

| OS | Location |
|---|---|
| macOS | `~/Library/Caches/Luthier/PresetPreviews/` |
| Windows | `%LOCALAPPDATA%\Luthier\Cache\PresetPreviews\` |
| Linux | `$XDG_CACHE_HOME/luthier/preset-previews/` (default `~/.cache`) |

In Free the folder name is "Luthier Free" (`editions.md` 7.2).

**Entries.** Each entry is two files:
- `<hash32>.ogg`, the audio;
- `<hash32>.json`, the sidecar: `schema`, magic `luthier.preview`, the full hash, the phrase, the
  plugin version, the render time in ms, the gain in dB, `approximate`, the 64 peaks, the features
  (6.2) and the descriptors (6.3).

**Writing and housekeeping:**
- Writes are temp-file-then-rename (`file-formats.md` 13).
- A `<hash32>.lock` file, created exclusively and treated as stale after 30 s, stops two instances
  from rendering the same hash.
- The cache is capped at **128 MB**, evicting least-recently-played first (the sidecar mtime is
  touched on each play).
- The cache is pruned when the service starts and after every 50 writes.
- Options -> FILE LOCATIONS gains "Preview cache: Open / Clear".

### 5.3 Shipped manifest

`Resources/Presets/Previews/previews.json` has one entry per factory preset: `uid`, `soundHash`,
`phrase` and `pluginVersion`, plus the features, descriptors and peaks.

- **Edited factory file.** If the installed file's hash differs, because the user edited it, the
  shipped clip is ignored and a local render is queued.
- **Version mismatch** (dev builds). The shipped clip is still played, marked stale, and a
  background render is queued.

### 5.4 Preset file additions

Two optional tail fields go beside the existing top-level `name` and `tags`. The as-built writer
keeps these at the root; `file-formats.md` 2 shows them under `meta`. The fields go wherever `name`
lives.
- `uid`: a UUID string, created on the first save of a user preset. Factory presets use
  `factory:<name>`. It is the key for favourites and ratings, so they survive a rename or move.
- `previewPhrase`: an optional phrase id from 3.1, set by the preset's author.

Both fields are added to the `known` array in `PresetManager::fromVar`. They are new optional
fields, so there is no schema bump (`file-formats.md` 15).

Auto-descriptors are **never** written into preset files. They are derived, depend on the engine
version, and live in the cache and the index.

### 5.5 Library preferences

`PresetLibraryPrefs` (`Source/Presets/PresetLibraryPrefs.h/.cpp`) is user-global. It is stored in
`~/Documents/Luthier/config/preset-library.json`, written immediately like `UiPreferences`:

```json
{ "schema": 1, "magic": "luthier.presetlibrary",
  "entries": { "<uid | relative path>": { "favourite": true, "rating": 4,
               "lastLoaded": "2026-09-24T10:00:00Z", "loadCount": 12 } },
  "recent": ["<uid>", "..."] }
```

- `recent` holds the last 30 loads.
- Entries are keyed by `uid`. A preset with no `uid` (in a read-only folder) is keyed by its path
  relative to its search folder.
- A rename through the browser re-keys the entry.

### 5.6 `PresetIndex`

`PresetIndex` (`Source/Presets/Search/PresetIndex.h/.cpp`) has one instance per processor and is
built on the preview worker from `PresetManager`. Each entry holds:
- `PresetInfo`;
- the uid, guitar name and family, and amp name;
- the drive index and techniques used, read from the `parameters` block without loading the preset;
- the source: Factory, User, Pack or Extra folder;
- the modified time;
- the feature vector and descriptors, from the manifest or the cache.

`PresetInfo` gains the fields `uid`, `guitarName`, `family`, `ampName` and `modified`. They are
filled in `scanFolder`.

The index rebuilds incrementally on `PresetManager`'s change broadcast. The UI shows names
immediately and fills in the rest as it arrives.

## 6. Search and auto-tagging

### 6.1 Parameter features

These are read from the preset JSON without loading it:
- **Drive index** (0-1): `amp_gain` weighted by the amp model's gain class, plus active drive,
  fuzz and distortion pedals weighted by their level.
- **Reverb amount:** `room_blend` times `room_decay`, plus the reverb pedal mix.
- **Other effects:** delay mix, and compression from compressor settings.
- **The instrument:** guitar family and body, pickup type (single, humbucker, piezo), string
  material (steel, nylon, flat), string count and lowest open pitch.
- **Playing flags:** `slide_mode`, the technique arm flags (`scrape_armed`, `slap_armed` and the
  others in `gui-techniques-updates.md` 2), and whether the rhythm engine is on.

### 6.2 Spectral features

`ToneFeatures` (`Source/Presets/Preview/ToneFeatures.h/.cpp`) computes these from the preview,
before loudness gain. It uses `juce::dsp::FFT` and is deterministic.
- median spectral centroid over 2048-point frames, as log Hz;
- 85 % rolloff;
- spectral flatness from 1 to 5 kHz (a proxy for grit);
- band energy ratios: below 250 Hz, 250 Hz to 2 kHz, above 2 kHz;
- crest factor;
- attack sharpness (mean onset strength);
- tail decay: seconds for a 30 dB fall after the last note-off;
- stereo width (1 minus the L/R correlation);
- loudness before normalisation.

### 6.3 Descriptors

`ToneDescriptors` (`Source/Presets/Search/ToneDescriptors.h/.cpp`) is a fixed vocabulary.
- **Confidence.** Each descriptor has a rule that gives a confidence from 0 to 1. The descriptor is
  attached when the confidence is 0.5 or more.
- **Calibration.** `pNN` is a factory-corpus percentile, calibrated separately for guitar and bass
  and shipped in `descriptor-calibration.json`. User presets are judged on the factory's scale.

| Descriptor (searched synonyms) | Rule |
|---|---|
| warm (dark, mellow, smooth, round) | centroid < p35, high band < p40, flatness < p50 |
| bright (sparkly, chimey, glassy, crisp) | centroid > p65 or high band > p70 |
| clean | drive < 0.25 and flatness < p50 |
| crunchy (crunch, gritty, breakup, edge of breakup) | drive 0.3-0.6 |
| high-gain (heavy, metal, chug, distorted) | drive > 0.6 |
| fuzzy (fuzz) | active fuzz pedal, or flatness > p85 with drive > 0.5 |
| djent | high-gain, and (7 or more strings or lowest pitch B1 or below), and tail decay < p30 |
| spacious (ambient, wet, washy, big) | tail > p75, or reverb amount > 0.5, or delay mix > 0.35 |
| dry (tight, close) | tail < p25 and delay mix < 0.1 |
| twangy (twang, snappy) | single-coil, attack > p70 and bright |
| percussive | attack > p80 and crest > p70 |
| compressed (smooth sustain) | compression > 0.4 or crest < p20 |
| fat (thick, full) | low band > p65 and mid band > p50 |
| jazz | warm and clean, and (hollow or archtop body, or neck humbucker, or jazz genre) |
| acoustic, nylon, bass, slide | taken directly from the parameter features |

**Genres.** The genres are rock, blues, jazz, country, folk, classical, metal, funk, reggae, latin,
indie, ambient, punk, surf and soul (the `factory-content.md` 0.4 list). They are taken from the
category, tags and name, and feed the Genre filter.

### 6.4 Query

`PresetSearch` (`Source/Presets/Search/PresetSearch.h/.cpp`) runs on the message thread over the
in-memory index.

**Parsing:**
- The query is lowercased and split on whitespace. Quoted phrases stay whole.
- Multi-word vocabulary entries such as "edge of breakup" are matched first, greedily.

**Matching:**
- Every token must match at least one field.
- Matching is by prefix.
- A token of 5 characters or more tolerates one Damerau edit, so "cruchy" and "djnet" still match.

**Weights.** Hits are summed with these weights; results sort by score, then name:

| Field | Weight |
|---|---|
| name | 5 |
| author tags | 4 |
| descriptors | 3 x confidence |
| category, genre | 3 |
| guitar, amp | 2 |
| author | 1.5 |
| description | 1 |

So "warm clean" puts presets that really are warm and clean ahead of one whose description merely
says "clean". The list is filtered on every keystroke, with no debounce (budget in section 11).

### 6.5 Similarity ("Sounds like this")

**The feature vector** has 24 dimensions, each z-scored with the calibration file:
- the 10 spectral features;
- drive, reverb, delay and compression;
- family one-hot (4), pickup one-hot (3), nylon and flat (2), and slide (1).

**Weights:** spectral 1.0, drive 1.5, family 2.0, everything else 0.7.

**The search** is brute force: the 8 nearest by weighted Euclidean distance. It excludes the preset
itself and any preset with the same sound hash, and respects the Source filter.

Presets that have not been analysed yet are skipped. The footer says "N presets still being
analysed".

## 7. UI

It is still one overlay. The layout follows the editor's current mode. `PresetBrowserPanel` moves
from `Overlays.cpp` to `Source/UI/PresetBrowser/`. `PluginEditor`'s `showOverlay (&presetBrowser)`,
`header.onOpenPresetBrowser` and the `presetBrowser` shortcut id do not change.

### 7.1 Easy Mode (780 x 560, the current size)

```
+-------------------------------------------------------------------------+
| PRESETS                                                        [Close]  |
| [Search: warm clean______________________ x]   (spk)[===o--] Previews    |
| (All)(Electric)(Acoustic)(Classical)(Bass) (Fav)(Recent)(Uses Techniques)|
+----------------------------------------------+--------------------------+
| > (h) Jazz Hollowbody   warm clean jazz ***** | [guitar thumbnail]       |
|   (h) Semi-Hollow Chime bright clean    ***   | Jazz Hollowbody          |
|   ...  [play glyph + 64-peak waveform]        | Electric - Jazz          |
|                                               | warm clean jazz          |
|                                               | description  [More like this]|
+----------------------------------------------+--------------------------+
| [Save As...] [Morph] A:... ===o=== B:...               [Delete] [Load]  |
+-------------------------------------------------------------------------+
```

- **Chips.** The family chips are single-select. Fav and Recent are toggles.
- **Sort** is relevance while there is a query, and name otherwise.
- **"More like this"** swaps the list for the 8 similar presets. A breadcrumb "Like: <name> [x]"
  returns to the full list.

### 7.2 Advanced Mode (1040 x 680, clamped to the window less 40 px)

```
+------------+-------------------------------------------------+-------------+
| SOURCE     | [Search_____________________] Sort [Relevance v]| thumbnail   |
| FAMILY     | > (h) Name   Category  Guitar  Amp  Tags  Rating| name, cat   |
| GENRE v    |   ...                                           | tag chips   |
| TECHNIQUES |                                                 | author,     |
|  (Uses Techniques + 6) |                                     | guitar, amp,|
| TONE (warm bright clean crunchy ...)                         | techniques  |
| RATING >=  |                                                 | SOUNDS LIKE |
| (h)Fav Recent [Clear filters]                                | 8 rows      |
+------------+-------------------------------------------------+-------------+
| [Save As...] [Morph row]      (spk)[vol] Previews: On      [Delete] [Load] |
+----------------------------------------------------------------------------+
```

- **Sort** options: Relevance (only while there is a query), Name, Category, Rating, Recently
  loaded, Most loaded, Date modified, and Similarity (only in the "Sounds like" view). The column
  headers also sort.
- **Filters** combine with OR inside a group and AND across groups. The filter state is remembered
  in UiPreferences.

### 7.3 Row and detail elements

**Play glyph.** It has one state at a time:
- a play triangle when idle;
- a sweep while playing;
- a spinner with "Preparing preview..." while rendering;
- "!" when the render failed, with the tooltip "Preview could not be rendered: <reason>. The preset
  can still be loaded.";
- hidden when previews are off.

**Waveform.** The row waveform is drawn from the 64 peaks. The part already played is in the
accent colour.

**Tags.** Author tags are filled chips. Auto-descriptors are outlined chips with the tooltip
"Detected from the sound". Clicking a chip adds it to the search.

**Favourite and rating.** The heart toggles favourite. The stars set a rating from 1 to 5; clicking
the current star clears it.

**Locked Pro presets (Free).** These show a lock but still preview. Load or Enter opens the upsell
panel (`editions.md` 4.2).

**Approximate renders** show "~", with the tooltip "Rendered with a fallback guitar; loading will
show which part is missing."

### 7.4 Keyboard

While the overlay has focus it consumes Space and the digit keys. That overrides the global tune
play/pause and snapshot recall of `gui-integration.md` 17.

| Key | Action |
|---|---|
| any printable key, or Ctrl+F | focus the search box |
| Down (from search) | move into the list |
| Up / Down | move the selection (auto-preview per 4.4) |
| PgUp / PgDn / Home / End | jump |
| Space | toggle the preview of the selected row |
| Enter | load (double-click also loads) |
| Escape | stops the preview if one is playing, otherwise closes (two presses at most) |
| F | toggle favourite |
| 0-5 | set the rating (0 clears) |
| S | "Sounds like this" |
| Ctrl+Backspace | clear search and filters |

All of these are rebindable under Options -> ACCESSIBILITY, group "Preset browser".

### 7.5 Empty states and errors

- **No results:** "No presets match "warm djent". Try fewer words or clear filters.", with a
  [Clear filters] button.
- **Favourites empty:** "Tap the heart on a preset to keep it here."
- **Recent empty:** "Presets you load appear here."
- **Previews off:** the footer says "Previews are off - Options -> Appearance".
- **Transport or host not processing:** the footer shows the hint from 4.3.
- **Cache not writable:** previews work from memory for the rest of the session. One banner:
  "Preview cache folder is not writable; previews will be re-rendered next time."

## 8. Options

A new **PRESET BROWSER** group goes at the bottom of Options -> APPEARANCE (`AppearancePage`),
beside the tooltips switch. The settings are `UiPreferences` keys, written immediately. The browser
footer's speaker icon and slider mirror the same keys.

| Key | Control | Default |
|---|---|---|
| `presetPreview.enabled` | Previews on/off | On |
| `presetPreview.trigger` | Hover or Click only | Hover |
| `presetPreview.onSelect` | Preview on keyboard selection | On |
| `presetPreview.volumeDb` | Preview volume, -40 to 0 dB | -6 dB |
| `presetPreview.whileTransport` | Preview while the host or tune plays | Off |
| `presetBrowser.sort`, `.filters`, `.viewSimilar` | Remembered view (no control) | - |

At the defaults a preview plays at about -24 LUFS, with peaks at -9 dBFS or lower.

## 9. Parameters, state, undo

- **Parameters:** none added. The parameter list is untouched.
- **Preset:** only the optional `uid` and `previewPhrase` fields (5.4). Both survive the round trip.
- **Session and `uiState`:** nothing new. View preferences go in UiPreferences, and favourites and
  ratings go in `preset-library.json`.
- **Undo.** Loading keeps its existing state boundary (`loadSelected()` calls
  `pushUndoState ("Load preset")`). Previewing, filtering, sorting, favourites, ratings and
  "Sounds like" are navigation or library data, so none of them goes on the stack
  (`action-and-undo.md` 3.8, 7).

## 10. Accessibility

- **Rows.** Each row has an `AccessibilityHandler` whose name reads like "Jazz Hollowbody, Electric
  Jazz, factory, favourite, 4 stars, warm, clean, jazz".
- **Controls.** The play glyph is the button "Preview <name>". The stars expose an
  `AccessibleValueInterface` (0-5). The heart and the chips are toggle buttons.
- **Announcements** are polite at Normal verbosity or higher: "Previewing <name>" and "Preview
  ready". A failed render is always announced.
- **Tab order:** search, chips or sidebar, list, detail pane, footer.
- **Reduced motion.** The static label "playing" replaces the sweep. The text "Preparing" replaces
  the spinner.
- **Chips** differ by outline versus fill, not by colour alone.
- **Hover is never required.** The keyboard and click reach everything hover does.
- **Localisation.** Every string is in the locale catalogue, descriptor words included. Search
  matches the localised word and the English one ("chaud" finds warm).

## 11. Performance budget

These use the reference CPU of `performance-budget.md`.

| Item | Budget |
|---|---|
| Audio-thread preview mix | 0.02 units or less; zero allocations and zero locks |
| Trigger to audible, clip already decoded | 30 ms or less, plus the dwell time |
| Trigger to audible, clip on disk (decode and resample) | 60 ms or less |
| Render one 4 s preview, steady-state preset | 1.0 s wall or less, on one core |
| Render the heaviest factory preset ("8-String Djent", "Physics Showcase") | 2.0 s or less |
| Offline instance cold create | 400 ms or less (`performance-budget.md` 5) |
| Worker instance while alive | 300 MB RSS extra or less, freed after 30 s idle, total still under the 900 MB cap |
| Decoded clips in memory | 16 MB or less (LRU) |
| Disk | cache 128 MB or less; shipped factory previews 3 MB or less |
| Search keystroke to repaint | 16 ms or less at 5,000 presets; 4 ms or less at 500 |
| Similarity query | 2 ms or less at 5,000 presets |
| Browser open, warm index | 150 ms or less; cold index build 1.5 s or less per 500 presets on the worker |

## 12. Editions

| Feature | Free | Pro |
|---|---|---|
| Previews of the Free factory presets and the user's own presets | Yes | Yes |
| Previews of the 20 Pro factory presets | Yes, from shipped clips; the presets are locked | Yes |
| Search, descriptors, filters, sort, favourites, ratings, recent | Yes | Yes |
| "Sounds like this" | Yes (locked presets can appear, marked) | Yes |

These are "Both" rows under `editions.md` 2.3 ("browse, tags, favourites"), per rule 0.2. Hearing
the locked presets is the quiet upsell of rule 0.8: no modal and no timer. Local renders in Free use
Free's effective values, so a Free user never hears a preview their edition cannot play.

## 13. Interactions

- **Morph:** Enter loads into the selected slot, as `loadSelected()` does today.
- **Snapshots:** a preview is the base state, which is what a load gives. A snapshot recall during a
  preview is unaffected.
- **Host automation and MIDI Learn:** untouched, because previews write no parameters.
- **Rhythm engine and Tune Builder:** the live engines are untouched. A running tune transport
  blocks previews (4.3). A preset's own rhythm engine shapes its own preview (3.1).
- **Techniques:** the "Uses Techniques" chip (`gui-techniques-updates.md` 7) is kept in the Easy
  chip row and in the Advanced TECHNIQUES group, and is driven by the arm flags of 6.1.
- **MIDI export, notation, capture, looper, session recorder, aux and per-string buses:** previews
  are excluded, by the insertion point in 4.2.
- **Workshop:** the thumbnail comes from `guitar-illustration.md` 15. Saving a guitar changes the
  hashes of the presets that reference it, so they re-render when next viewed.
- **Audition button and Workshop shadow audition:** these are live-engine notes. A preview mixes on
  top, and neither stops the other.
- **Onboarding:** the tour's preset stop adds "Hover to hear, Enter to load" (`onboarding.md`).
- **Content packs:** pack previews are verified by the pack signature.

## 14. New and changed code

**New files:**
- `Source/Presets/Preview/`: `PreviewPhrase`, `PreviewRenderer`, `PreviewRenderHost` (moved from
  the CLI), `PreviewRenderService`, `PreviewCache`, `PreviewClipPool`, `PreviewPlayer`,
  `ToneFeatures`.
- `Source/Presets/Search/`: `PresetIndex`, `PresetSearch`, `ToneDescriptors`.
- `Source/Presets/PresetLibraryPrefs`.
- `Source/UI/PresetBrowser/`: `PresetBrowserPanel` (moved), `PresetRow`, `PresetFilterSidebar`,
  `PresetDetailPane`.
- `scripts/render_previews.sh`, `Resources/Presets/Previews/`,
  `Resources/Presets/descriptor-calibration.json`.

**Changed files:**
- `PresetManager`: the new `PresetInfo` fields, a `uid` on save, a render request on save, and the
  known keys.
- `LuthierAudioProcessor`: owns `PreviewPlayer` and `PreviewRenderService`; the `processSlice`
  insertion; `panic`.
- `RenderCli.cpp`: `--render-previews`.
- `OptionsPages`: `AppearancePage` and `FileLocationsPage`.
- `CMakeLists.txt`: `JUCE_USE_OGGVORBIS=1` and the new sources, in the plugin, `LuthierTests` and
  `LuthierRender`.

## 15. Failure modes

| Failure | Response |
|---|---|
| Preset JSON is corrupt or refused | No preview. The row shows "!" and is still searchable by name. Loading it gives the normal banner. |
| Guitar or IR missing at render | Render with the load fallback and mark the preview approximate. |
| Render throws, or runs past 10 s | Mark failed and log it. Retry once on the next browser open, then not again until the hash changes. |
| A cached clip will not decode | Delete the entry and render again. |
| Cache not writable, or disk full | Keep previews in memory for the session and show one banner. |
| Sample-rate change during a preview | Fade and stop. The next request resamples. |
| Editor closes during a preview | `overlayHidden()` fades it out. On-save renders continue. |
| Processor destroyed | Cancel and join within 500 ms. Each clip is freed after the audio thread lets it go. |
| Host stops calling `processBlock` | Drop the request and show the footer hint. |
| Shipped manifest missing | Factory presets render locally, like user presets. |

## 16. Tests

**Where the tests go:**
- Unit tests go in `LuthierTests`, in the new files `PresetPreviewTests.cpp` and
  `PresetSearchTests.cpp`.
- GUI tests run under xvfb in the style of `EditorTests.cpp`, in the new file
  `PresetBrowserUiTests.cpp`.
- Combination tests use the `CombinationTests.cpp` harness.

**Rendering:**
- **PB-01** The same preset rendered twice gives bit-identical float buffers and identical Ogg
  bytes.
- **PB-02** Each factory preset's fresh render matches its shipped clip:
  - log-spectrum difference 1.0 dB RMS or less, over 40 bands;
  - loudness within 0.5 LU;
  - the manifest `soundHash` equals the hash computed from `FactoryPresets::toVar`.
- **PB-03** Every clip:
  - is 2.0-4.0 s long;
  - measures -18 ±0.5 LUFS, unless peak-bound, and has a true peak of -3.0 dBTP or less;
  - ends with 10 ms below -60 dBFS.
- **PB-04** Phrase selection, with a `previewPhrase` field overriding all of these:
  - "Modern Metal Chug" and "8-String Djent" get `highgain_riff`;
  - "Strummed Dreadnought" gets `acoustic_strum`;
  - "P-Bass Flatwound" gets `bass_groove`;
  - "Flamenco Rasgueado" gets `rasgueado`;
  - "Blues Slide" gets `slide_lick`.
- **PB-05** The phrase root follows tuning: the lowest note is C2 for "Drop C Riff" and B0 for
  "5-String Low B".

**Playback safety:**
- **PB-06** After 20 previews, the live APVTS state, `GuitarSpec` hash, snapshot bank and undo depth
  are all unchanged.
- **PB-07** Switching previews every 150 ms for 60 s, with the heap and mutex traps armed on the
  audio thread, trips zero traps.
- **PB-08** Fades:
  - the first 10 ms rise monotonically from 0;
  - a stop reaches -90 dBFS within 30 ms;
  - during a switch, the two clips are never both above -60 dBFS at the same sample.
- **PB-09** A held live note is unaffected: its per-string aux output nulls below -120 dB against
  the same run with no preview.
- **PB-10** Transport rules:
  - with the host playhead playing, hover and Space give silence;
  - with `whileTransport` on, they play;
  - `isNonRealtime()` always gives silence;
  - the kill switch silences a preview within 30 ms.
- **PB-11** The looper, session recorder, capture, aux buses and MIDI out get output that is
  byte-equal or nulls to the same run without a preview.
- **PB-12** Level and smoothing:
  - at -6 dB volume with no live playing, the output peak is -9 dBFS or less;
  - a volume change moves the linear gain by no more than 0.01 per sample.
- **PB-13** A switch from 44.1 kHz to 96 kHz mid-preview stops cleanly. The next preview's centroid
  is within 1 % of the 48 kHz reference.

**Cache:**
- **PB-14** The sound hash:
  - is unchanged by a rename or a retag;
  - changes with one parameter, the guitar file, the version string or `kRenderRevision`.
- **PB-15** After `saveAs`, a cache entry with features and descriptors exists within 3 s.
- **PB-16** Filling the cache to 130 MB prunes it to 128 MB or less, least-recently-played first.
  Lock files older than 30 s are ignored.
- **PB-17** Two processors asking for the same hash at once cause exactly one render, and both
  play it.
- **PB-18** Failures:
  - a corrupt preset gives a failed row and no crash;
  - a forced timeout is abandoned within 10.5 s;
  - an unwritable cache plays from memory and shows exactly one banner.
- **PB-19** Destroying the processor mid-render returns within 500 ms, with no leaks.

**Search and tags:**
- **PB-20** Factory descriptors:
  - "Jazz Hollowbody" is warm, clean and jazz;
  - "8-String Djent" is high-gain and djent;
  - "Ambient Swell" is spacious;
  - "Semi-Hollow Chime" is bright;
  - "Single-Cut Crunch" is crunchy;
  - no Bass preset is djent.
- **PB-21** Queries:
  - "warm clean" has "Jazz Hollowbody" in its top 3, and no result has a drive index above 0.5;
  - "djent" returns "8-String Djent" first;
  - "cruchy" returns the same set as "crunchy";
  - "clean jazz" returns the same set as "jazz clean".
- **PB-22** Field weights:
  - a query matching only the author, guitar or amp still finds the preset;
  - a name match ranks above a description-only match.
- **PB-23** Filters:
  - Family=Bass combined with Genre=funk is an AND;
  - two genres together are an OR;
  - "Uses Techniques" with Slap returns exactly the presets with `slap_armed` on;
  - Favourites and Recent match `preset-library.json`.
- **PB-24** The 8 "Sounds like" neighbours of "Modern Metal Chug":
  - include "Drop C Riff" and "8-String Djent";
  - contain no acoustic or classical preset;
  - never include the preset itself;
  - are deterministic.

**Library and file:**
- **PB-25** Favourites and ratings survive a rename, an editor reopen and a new processor. They are
  never written into the preset file.
- **PB-26** Preset fields:
  - a `uid` is created on the first save and kept afterwards;
  - `previewPhrase` is preserved;
  - the existing byte-identical round-trip test (`file-formats.md` 16) still passes.

**Performance:**
- **PB-27** Performance, measured on 5,000 synthetic index entries where a count applies:
  - search takes 16 ms or less per keystroke;
  - similarity takes 2 ms or less;
  - a warm browser open takes 150 ms or less;
  - the preview mix costs 0.02 units or less;
  - one steady-state render takes 1.0 s or less, checked by the CI perf job.

**GUI:**
- **PB-28** Easy mode, keyboard path:
  - `Ctrl+O` opens the browser;
  - typing focuses the search box, and Down moves into the list;
  - Space starts a preview, and the player reports it active;
  - Escape stops the preview, and a second Escape closes the browser;
  - Enter loads, and a "Load preset" boundary is pushed.
- **PB-29** Layout:
  - in Advanced, the sidebar, sort combo, columns and "Sounds like" pane lay out without overlap at
    1280 x 800 and at 2560 x 1600;
  - in Easy, those are absent, and the chip row and "More like this" are present.
- **PB-30** Hover:
  - resting 300 ms on a row starts a preview;
  - leaving the list stops it within 30 ms;
  - with "Click only", hovering never starts one.
- **PB-31** While the browser has focus, digits and Space do not recall snapshots or toggle the tune
  transport. With the browser closed, they do.
- **PB-32** Accessibility:
  - every row, play glyph, heart, star control and chip has an accessible name;
  - the tab order is as in section 10;
  - "Previewing <name>" is announced;
  - reduced motion gives pixel-stable frames while a preview plays.
- **PB-33** Options:
  - the five `presetPreview.*` keys persist across instances;
  - the footer mirror stays in sync with the Options group;
  - with previews off, the glyphs are hidden and every trigger is blocked.
- **PB-34** Empty states: a nonsense query shows the no-results text and Clear filters, and empty
  Favourites and Recent show their hints.

**Editions and combinations:**
- **PB-35** In the Free build:
  - the 20 Pro presets are listed locked and preview from shipped clips;
  - Enter opens the upsell panel;
  - a user preset with `slap_armed` on renders identically to the same preset with it off.
- **PB-36** A preview during Morph, tune playback (with `whileTransport` on), live slide mode, an
  active mod matrix and a MIDI Learn arm:
  - every live module's output is unchanged compared with the same run without the preview;
  - no assertion fires.
- **PB-37** Reachability (`gui-integration.md` 0.2): the browser, its Options group and the cache
  Open/Clear buttons are each within 3 interactions of the header, in both modes.
