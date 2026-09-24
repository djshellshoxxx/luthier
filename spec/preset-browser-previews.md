# PRESET BROWSER: PREVIEWS AND SEARCH SPEC

A preset browser you can hear before you commit to anything. Hover or
arrow onto a preset and a 2-4 second phrase plays in that preset's own
sound, without loading it. Type "warm clean" or "djent" and the list
narrows to presets that really sound that way. Ask for "Sounds like
this" and get the eight nearest presets.

Added 2026-09-24 at the product owner's request. It is additive to
`gui-integration.md`: the browser stays where it is (the header preset
name, `Ctrl+O`, an overlay). It rebuilds the inside of
`PresetBrowserPanel` (`Source/UI/Overlays.h/.cpp`) and keeps the
"Uses Techniques" chip (`gui-techniques-updates.md` 7), the Morph row
(`ambiguity-resolutions.md` 5.2) and the guitar thumbnail
(`guitar-illustration.md` 15, `gui-engine-dataflow.md` 18).

## 0. Ground rules

1. **Every preview is rendered by Luthier's engine.** No sound is
   recorded, sampled or hand-made. A preview is what `luthier-render`,
   or the plugin's own offline instance, produces from the preset plus a
   built-in MIDI phrase. Given the same inputs the render is identical
   to the bit (deterministic offline render).
2. **Previewing never changes the live instance.** No parameter, no
   guitar and no engine state of the playing instance is touched.
   Previews are rendered by a separate offline instance and played back
   as a finished clip. Nothing about previewing goes on the undo stack
   (`action-and-undo.md` 7, "Preset browser navigation").
3. **Real-time safe.** On the audio thread playback is one additive mix
   of a buffer that was prepared in advance. It does no allocation, no
   locks, no file I/O and no resampling (`engine.md` 0.2,
   `performance-budget.md` 0.4-0.5).
4. **A preview never interrupts the player.** It is mixed on top of the
   live output. It never stops, mutes, retriggers or re-voices a note
   that is sounding, and it never plays while a host or tune transport
   is running unless the user has asked for that (section 4.3).
5. **Presets are fundamental** (`editions.md` 1). Previews, search,
   filters, favourites, ratings and similarity are in both editions
   (section 12).
6. **No new automatable parameters.** Every setting here is a user
   preference or library data. None of it is sound state.

## 1. User stories

- *Browsing:* "I arrow down the list and hear each preset play a short
  phrase that suits it, at a safe level, without losing the sound I
  have loaded."
- *Searching by words:* "I type `warm clean` and get the jazz and
  neo-soul cleans, not the metal presets that happen to say 'clean' in
  their description."
- *Finding by ear:* "I like this one. Show me others like it."
- *Keeping my favourites:* "I star the ones I use, filter to favourites
  on stage, and see what I loaded recently."
- *Sharing my own presets:* "When I save a preset, it gets a preview and
  tags just like the factory ones."
- *Free user:* "I can hear the Pro presets before deciding to upgrade."

## 2. Where previews come from (decision)

**Hybrid.** Factory and content-pack presets ship with previews
rendered at build time. User presets, third-party presets and edited
factory files are rendered on a background thread and cached on disk.

Why:
1. **Free cannot render Pro presets.** The Pro code is not in the Free
   binary (`editions.md` 0.5), so the 20 locked presets can only be
   heard from shipped audio. Pro uses the same files, so there is one
   pipeline.
2. **Instant first open.** A new user's first hover plays at once, with
   no queue of 36 renders on first launch.
3. **Size is trivial.** 36 clips of 4 s at up to 64 KB each is at most
   2.3 MB against the 200 MB content budget (`factory-content.md` 0.8).
4. **Consistent tags.** The spectral features of the factory bank are
   computed once, at build time, and they also set the thresholds the
   auto-tagger uses for everyone (section 6.3).

### 2.1 Build-time previews

- A new CLI mode `luthier-render --render-previews <outDir>`
  (`Tools/RenderCli.cpp`) renders every `FactoryPresets` definition
  through the same `PreviewRenderer` (section 3.2) that the plugin uses.
  It writes:
  - `Resources/Presets/Previews/<uid>.ogg`
  - `Resources/Presets/Previews/previews.json`, the manifest (5.3)
  - `Resources/Presets/descriptor-calibration.json` (6.3)
- A new script, `scripts/render_previews.sh`, runs in CI after the
  build. The files are copied by `luthier_copy_resources`.
  `editions.md` 9's `scripts/render_demos.sh`, which renders the
  20-second upsell demos, calls it.
- A `.luthiercontent` pack may carry `Previews/<uid>.ogg` beside its
  presets. The manifest's sha256 list covers them. A pack without
  previews gets local renders.

### 2.2 Runtime previews

- **On save.** `PresetManager::saveAs` and `saveCurrent` queue a render
  at high priority once the atomic write (`file-formats.md` 13) has
  finished. The preview is normally ready before the user next opens the
  browser.
- **On first view.** When the browser opens, the rows that have no
  valid preview are queued: visible rows first, then the rest of the
  filtered list at idle priority. A hover or selection on a row with no
  preview moves it to the front of the queue.
- **Stale detection** uses the sound hash (5.1). A file changed on disk,
  a changed referenced guitar or user IR, or a new plugin version all
  give a new hash, which is a cache miss.

## 3. Rendering

### 3.1 Phrases

A new `PreviewPhrase` (`Source/Presets/Preview/PreviewPhrase.h/.cpp`)
works like `AuditionPhrase` in `Support/AudioExporter.h`, but is a
separate enum so that the stored `AuditionPhrase` indices never move.
It builds a timed `juce::MidiMessageSequence`. The notes last at most
3.2 s. The clip is 3.2 s of notes plus a tail, hard-capped at 4.0 s
with a 250 ms raised-cosine fade to the end.

| Id | Content | Tempo | Chosen for |
|---|---|---|---|
| `acoustic_strum` | G, Cadd9, D, strummed D-DU-UDU | 104 | Acoustic, not fingerstyle |
| `fingerstyle` | Travis pattern on C, then G/B | 96 | tags or names with fingerstyle, folk, parlor, DADGAD |
| `nylon_comp` | Am7 to D9 bossa comp | 110 | Classical / nylon, not flamenco |
| `rasgueado` | E Phrygian rasgueado burst and a single-note answer | 120 | flamenco |
| `clean_arp` | Cadd9 to Em7, arpeggiated, let ring | 90 | Electric, drive index < 0.3 |
| `crunch_riff` | A5 C5 D5 stabs plus a pentatonic fill | 112 | drive index 0.3-0.6 |
| `highgain_riff` | palm-muted chugs on the lowest string plus two power-chord stabs | 120 | drive index > 0.6, or tags metal / djent |
| `lead_lick` | minor pentatonic lick, one bend, vibrato on the last note | 100 | tags lead / shred / solo |
| `jazz_comp` | Dm9, G13, Cmaj9 shell voicings, swung | 120 | jazz, not lead |
| `funk_chop` | E9 sixteenth chops with ghosted mutes | 100 | funk, wah, reggae (off-beat variant) |
| `twang_lick` | hybrid-picked double stops | 110 | country, twang, rockabilly, surf |
| `slide_lick` | open-G slide phrase | 84 | `slide_mode` on, or tag slide |
| `ambient_swell` | one chord with a volume swell and a long release | 70 | ambient, swell, shoegaze, reverb blend > 0.5 |
| `bass_groove` | root-fifth-octave eighths | 100 | bass family |
| `bass_slap` | slap and pop octave groove | 100 | bass with `slap_armed` |
| `bass_walk` | one bar of walking line | 120 | bass and jazz, fretless |

**Selection order.**
1. The preset's optional `previewPhrase` field (5.4).
2. The first row of the table whose rule matches the preset's category,
   tags or name. Tokens are matched case-insensitively, and rows are
   tried top to bottom.
3. A fallback by features: bass family gives `bass_groove`; drive index
   above 0.6 gives `highgain_riff`, above 0.3 gives `crunch_riff`; nylon
   strings give `nylon_comp`; an acoustic body gives `acoustic_strum`;
   anything else gives `clean_arp`.

"Drive index" is defined in 6.1.

**Pitch follows the guitar.** Phrases are written relative to a root.
The root is set to the preset guitar's lowest open string after tuning
and capo, so Drop C riffs in C and a 5-string bass grooves on low B.
High phrases (`lead_lick`, `twang_lick`) are raised by the smallest
number of octaves that puts them within the guitar's range.

**Rhythm engine.** If the preset has `rhythmEngine` enabled, the phrase
becomes two held chords (the preset's own pattern then plays them)
at a fixed 100 BPM from the offline host's playhead. That way what you
preview is what you get when you load and hold a chord.

Techniques armed in the preset (scrape, slap, slide, mute grid, tap,
bend) apply as they would live, because the render is the real engine.

### 3.2 `PreviewRenderer`

`Source/Presets/Preview/PreviewRenderer.h/.cpp`. It has no UI, runs on a
worker thread, and is used by both the plugin and `luthier-render`.

1. It gets an offline processor from
   `LuthierAudioProcessor::createOfflineInstance()`, which is the same
   factory `AudioExporter` uses. It is created lazily, kept for reuse,
   and destroyed 30 s after the queue empties. In the CLI the factory
   returns `RenderCli.cpp`'s `RenderHost`, moved to
   `Source/Presets/Preview/PreviewRenderHost.h` so both can share it.
2. Per job:
   - `reset()`, then load the preset file through the instance's own
     `PresetManager::loadPreset (File)`. That resolves the guitar block
     (`onGuitarBlockLoaded`) and does the fallbacks exactly as a live
     load would.
   - `prepareToPlay (48000, 256)` and 4x oversampling (the default,
     whatever the live instance uses). There is no host tempo; the fixed
     tempo from 3.1 is used.
   - Render 0.5 s of silence to settle the engine, which is discarded.
     Then render the phrase and tail in 256-sample blocks.
3. **Analysis** (section 6.2) runs on the float result.
4. **Loudness.** It measures integrated loudness (ITU-R BS.1770-4,
   K-weighted, gated) and applies a gain to reach **-18 LUFS**. If the
   4x-oversampled true peak would then be above **-3 dBTP**, the gain is
   lowered further. No limiter is used. The applied gain goes in the
   sidecar.
5. **Encoding.** Ogg Vorbis, 48 kHz stereo, quality 0.5 (about
   128 kbps), at most 64 KB. It also stores a 64-point peak envelope for
   the row waveform.
6. **Cancellation.** A cancel flag is checked every block. A render
   that takes longer than **10 s** of wall time is abandoned and marked
   failed.

The Free edition renders with the neutralised effective values of
`editions.md` 5.1.3. That happens automatically, because the offline
instance is a Free instance.

### 3.3 `PreviewRenderService`

`Source/Presets/Preview/PreviewRenderService.h/.cpp`. It is owned by
`LuthierAudioProcessor`, so renders continue while the editor is closed.

- One `juce::Thread` at low priority. It holds a priority queue with
  three levels: `interactive` (hovered or selected, at most 1),
  `onSave`, and `background`. A new interactive request replaces the
  previous one if that one has not started yet.
- It renders one job at a time.
- Background jobs are paused while the host or tune transport is
  running, and while `performance-budget.md` 8 CPU relief is engaged.
- Results come back to the message thread through
  `juce::MessageManager::callAsync` with a `WeakReference`. The index and
  the UI update from there.
- A shutdown sets the cancel flag and joins within 500 ms. The
  processor destructor calls it.

## 4. Playback

### 4.1 `PreviewPlayer`

This is the audio-thread side. It lives in `Source/Presets/Preview/
PreviewPlayer.h/.cpp` and is a member of `LuthierAudioProcessor`.

- `PreviewClip` holds an immutable stereo `juce::AudioBuffer<float>`
  already resampled to the current host rate. The resampling is done on
  a loader thread with `juce::WindowedSincInterpolator`, not on the
  audio thread. It also holds the source hash and its length.
- Handoff:
  - The message thread publishes `std::atomic<const PreviewClip*>
    pending`. The audio thread exchanges it into its current voice.
  - The audio thread publishes `std::atomic<const PreviewClip*>
    inUse[2]`, one for each of its two voices.
  - The message thread's `PreviewClipPool` frees a clip only when it is
    neither pending nor in use. The same pattern is used as the
    `GuitarSpec` swap (`ui-wiring.md` 4.3).
- Two voices allow a switch: the old clip fades out over 30 ms and the
  new one starts when that fade ends. Clips never sum at full level.
- Fades are raised-cosine: 10 ms in, 30 ms on stop, and the baked 250 ms
  fade at the end of the clip. Level is the preview volume (4.3) as a
  gain, smoothed linearly over 20 ms (`engine.md` 0.4).
- `stop()`, `panic()` (it is called from `LuthierAudioProcessor::panic`),
  `prepareToPlay` and `releaseResources` all fade and then drop the
  clips. A sample-rate change drops them; the loader resamples again on
  the next request.

### 4.2 Insertion point

In `LuthierAudioProcessor::processSlice`, the preview mix goes **after**
the practice block (looper and session recorder) and the tune and
metronome click mix. It goes **before** `routing.distribute(...)`.

Consequences, all intended:
- The preview reaches the main output, and the monitor mix too because
  that copies main.
- It is not captured by the looper, the session recorder, Tone Match
  `capture`, the per-string or aux buses, MIDI out or MIDI export.
- The header output meter shows it, because that is what comes out.
- The kill switch silences it: `PreviewPlayer` reads
  `killSwitch.isActive()` and fades out.

The call is `previewPlayer.processBlock (mainOut, numSamples)`, adding
into channels 0-1 of the main bus. With a mono main bus the two channels
are summed at -3 dB.

### 4.3 When a preview may play (transport rules)

| Situation | Hover or auto-on-selection | Explicit (play glyph, Space) |
|---|---|---|
| Idle | plays | plays |
| Live notes sounding, or MIDI note-on within the last 500 ms | suppressed | plays, mixed on top; the live notes are untouched |
| Host transport playing (`AudioPlayHead` `getIsPlaying()`) | suppressed | suppressed, with the hint "Previews pause while the host plays" |
| Tune Builder transport playing | as host transport | as host transport |
| "Preview while playing" option on | the two transport rows become "plays" |
| Offline bounce (`isNonRealtime()`) | never | never |
| Kill switch engaged | silent | silent |
| Previews option off | never | never; the play glyph is hidden |

The UI decides whether a request is allowed. It reads a
`PreviewPlayer::Gate` snapshot that the audio thread publishes each
block through relaxed atomics: transport playing, non-realtime, live
activity age and kill.

The audio thread checks non-realtime and kill again, because the UI's
view can be up to one frame old.

A request is dropped if the processor has not called `processBlock`
for 200 ms. In that case the footer shows "The host is not processing
audio. Arm or monitor the track to hear previews."

### 4.4 Triggers

| Trigger | When a preview starts | When it stops |
|---|---|---|
| **Hover** (default) | The pointer rests 300 ms on a row. | Mouse leaves the row or the list; Escape; Space; the overlay closes; a load. |
| **Click** | A click on the row's play glyph. With the Click-only trigger setting, a single click on the row does it too. | Clicking the glyph again, or the other stop events above. |
| **Keyboard** | Space toggles the preview of the selected row. With "Preview on selection" on (the default), moving with the arrows starts it after 150 ms. | Space, Escape, the overlay closes, or a load. |

A preview plays once. It does not loop.

Clips for the rows next to the selection (one above and one below) are
decoded ahead of time, so moving with the arrows is instant.

## 5. Data model and files

### 5.1 Sound hash

The sound hash is SHA-256 over:
- the preset JSON after canonicalisation (sorted keys, numbers written
  with `%.9g`) with the descriptive fields removed (`name`, `category`,
  `author`, `description`, `tags`, `uid`, `previewPhrase`, `meta`,
  `pluginVersion`);
- the canonical serialisation of the resolved `GuitarSpec` (the same
  one as the guitar-illustration thumbnail key);
- the size and mtime of every referenced user IR;
- the phrase id;
- `JucePlugin_VersionString`;
- `PreviewRenderer::kRenderRevision` (starts at 1).

Renaming or retagging a preset keeps its preview. Changing its sound
makes a new one. Every plugin update renders user previews again,
lazily. That is the right choice: an engine change can change the
sound, and engine versions are not tracked more finely than releases.

### 5.2 Cache on disk

Previews are a cache, not user data, so they are kept out of
`~/Documents/Luthier`, which is often synced to iCloud or OneDrive:
- macOS: `~/Library/Caches/Luthier/PresetPreviews/`
- Windows: `%LOCALAPPDATA%\Luthier\Cache\PresetPreviews\`
- Linux: `$XDG_CACHE_HOME/luthier/preset-previews/`, or
  `~/.cache/...` if that is not set.

In Free the folder is "Luthier Free", following `editions.md` 7.2
`contentFolder`.

Each entry is `<hash32>.ogg` plus a sidecar `<hash32>.json`, where
`hash32` is the first 32 hex characters of the hash. The sidecar holds:
schema, magic `luthier.preview`, the full hash, the phrase, the plugin
version, the render time in ms, the gain in dB, whether the render is
approximate, the 64 peaks, the feature record (6.2) and the
descriptors (6.3).

- Writes follow `file-formats.md` 13: a temp file, then a rename.
- Two instances do not render the same preset twice: a `<hash32>.lock`
  file is created exclusively and treated as stale after 30 s. A
  duplicate render would be harmless anyway.
- Size cap is **128 MB** with LRU eviction by sidecar mtime, which is
  updated on every play. It is pruned when the service starts and after
  every 50 writes.
- Options -> FILE LOCATIONS gains "Preview cache: Open / Clear".

### 5.3 Shipped manifest

`Resources/Presets/Previews/previews.json` holds, for each factory
preset: `uid`, `soundHash`, `phrase`, `pluginVersion`, and the same
features, descriptors and peaks as a sidecar.

When a user's factory file hashes to a different value (because they
edited it), the shipped clip is ignored and a local render is queued. A
manifest whose `pluginVersion` differs (a dev build) is still played,
marked stale, and a render is queued in the background.

### 5.4 Preset file additions

These are tail fields beside the existing top-level `name` and `tags`.
The as-built writer keeps meta at the root; `file-formats.md` 2 shows it
as `meta`, and the fields go wherever `name` is. Both are optional and
both are added to the known-field list in `PresetManager::fromVar`
(PresetManager.cpp, the `known` array):
- `uid`: a UUID string, written on the first save of a user preset.
  Factory uids are `factory:<name>`. It keys favourites and ratings, so
  they survive a rename or move.
- `previewPhrase`: an optional phrase id from 3.1, for authors.

No schema bump is needed: both are new optional fields
(`file-formats.md` 15). Auto-descriptors are **never** written into
preset files. They are derived data, they depend on the engine version,
and they live in the cache and the index.

### 5.5 Library preferences

`~/Documents/Luthier/config/preset-library.json` is user-global. It
travels with the user's documents and is in neither the preset nor the
session.

```json
{ "schema": 1, "magic": "luthier.presetlibrary",
  "entries": { "<uid or relative path>": { "favourite": true, "rating": 4,
               "lastLoaded": "2026-09-24T10:00:00Z", "loadCount": 12 } },
  "recent": ["<uid>", "..."] }
```

- `recent` keeps the last 30 loads.
- A preset without a `uid` (read-only folders) is keyed by its path
  relative to its search folder.
- A rename through the browser re-keys the entry.
- Writes go through immediately, like `UiPreferences`. The class is
  `PresetLibraryPrefs` (`Source/Presets/PresetLibraryPrefs.h/.cpp`).

### 5.6 `PresetIndex`

`Source/Presets/Search/PresetIndex.h/.cpp`. There is one per processor.
It is built from `PresetManager` on the preview worker. For each preset
it holds everything the browser shows and searches:
- the `PresetInfo` fields;
- `uid`, guitar name and family;
- amp model name, drive index and techniques used (read from the
  preset's `parameters` without loading it);
- the source (Factory, User, Pack, Extra) and modified time;
- the feature vector and descriptors from the manifest or cache.

`PresetInfo` gains `uid`, `guitarName`, `family`, `ampName` and
`modified`, filled by `scanFolder`.

It is rebuilt incrementally on `PresetManager`'s change broadcast. A
cold build of 500 presets takes 1.5 s or less on the worker. The UI
shows names at once and fills in the rest as it arrives.

## 6. Search and auto-tagging

### 6.1 Parameter features

These are read from the JSON without loading the preset:
- **drive index** (0-1): amp gain weighted by the amp model's gain class,
  plus active drive, fuzz and distortion pedals, each weighted by its
  level;
- reverb amount: room blend times decay, plus reverb pedal mix;
- delay mix;
- compression, from active compressor settings;
- guitar family and body type;
- pickup type: single-coil, humbucker, piezo or none;
- string material: steel, nylon or flatwound;
- number of strings and lowest open pitch;
- `slide_mode`, the technique arm flags, and whether the rhythm engine
  is on.

### 6.2 Spectral features

These are taken from the rendered preview at render time, before the
loudness gain is applied:
- spectral centroid (median of 2048-point frames, log Hz);
- 85 % rolloff;
- spectral flatness at 1-5 kHz (a grit proxy);
- band energy ratios: below 250 Hz, 250 Hz to 2 kHz, above 2 kHz;
- crest factor;
- attack sharpness (mean onset strength);
- tail decay (seconds for the energy to fall 30 dB after the last
  note-off);
- stereo width (1 minus the L/R correlation);
- loudness before normalisation.

This is `ToneFeatures` (`Source/Presets/Preview/ToneFeatures.h/.cpp`).
It uses `juce::dsp::FFT`, runs off the audio thread, and gives the same
result every time.

### 6.3 Descriptors

`ToneDescriptors` (`Source/Presets/Search/ToneDescriptors.h/.cpp`)
holds a fixed vocabulary. Each descriptor has a rule and a confidence
from 0 to 1, and a descriptor is attached when its confidence is 0.5 or
more.

Thresholds are percentiles of the factory corpus, computed separately
for guitar and bass. They are shipped in
`descriptor-calibration.json`, so a user preset is judged on the same
scale as the factory ones.

| Descriptor (synonyms searched) | Rule (all conditions contribute to confidence) |
|---|---|
| warm (dark, mellow, smooth, round) | centroid < p35, high band < p40, flatness < p50 |
| bright (sparkly, chimey, glassy, crisp) | centroid > p65, or high band > p70 |
| clean | drive index < 0.25 and flatness < p50 |
| crunchy (crunch, gritty, edge of breakup, breakup) | drive index 0.3-0.6 |
| high-gain (heavy, metal, chug, distorted) | drive index > 0.6 |
| fuzzy (fuzz) | an active fuzz pedal, or flatness > p85 with drive > 0.5 |
| djent | high-gain, and 7 or more strings or lowest pitch at B1 or below, and tail decay < p30 (tight) |
| spacious (ambient, wet, washy, big) | tail decay > p75, or reverb amount > 0.5, or delay mix > 0.35 |
| dry (tight, close) | tail decay < p25 and delay mix < 0.1 |
| twangy (twang, snappy) | single-coil, attack > p70, bright |
| percussive | attack > p80 and crest > p70 |
| compressed (smooth sustain) | compression > 0.4, or crest < p20 |
| fat (thick, full) | low band > p65 and mid band > p50 |
| jazz | warm and clean, and hollow/archtop body or neck humbucker, or genre jazz |
| acoustic, nylon, bass, slide | taken directly from parameter features |

Genres (rock, blues, jazz, country, folk, classical, metal, funk,
reggae, latin, indie, ambient, punk, surf, soul) come from the
category, the tags and names in the factory-content.md 0.4 list. They
feed the Genre filter.

### 6.4 Query

`PresetSearch` (`Source/Presets/Search/PresetSearch.h/.cpp`) runs on
the message thread against the in-memory index.

- The query is lowercased and split on whitespace. Quoted phrases stay
  as one token. Multi-word vocabulary entries ("edge of breakup",
  "clean jazz") are matched greedily first.
- **Every token must match** at least one field. Matching is by prefix.
  Tokens of 5 characters or more allow one edit (Damerau), so "djnet"
  and "cruchy" still work.
- Fields searched, with score weights:

  | Field | Weight |
  |---|---|
  | name | 5 |
  | author tags | 4 |
  | descriptors | 3 x confidence |
  | category, genre | 3 |
  | guitar, amp | 2 |
  | author | 1.5 |
  | description | 1 |

  Sort is by score, then name.
- "warm clean" therefore matches presets tagged or described as warm
  and clean, ahead of a preset whose description only contains
  "clean".
- Every keystroke re-filters, with no debounce (see the budget in 11).

### 6.5 Similarity ("Sounds like this")

- The feature vector has 24 dimensions: the 10 spectral features of 6.2,
  plus drive, reverb, delay and compression, plus family one-hot (4),
  pickup one-hot (3), nylon/flat (2) and slide (1). Each dimension is
  z-scored with the calibration file.
- Weights: spectral 1.0, drive 1.5, family 2.0, the others 0.7.
- The result is the 8 nearest by weighted Euclidean distance, brute
  force. It excludes the preset itself and presets with the same sound
  hash, and it respects the Source filter.
- Presets with no features yet (still rendering) are left out, and a
  footer shows "3 presets still being analysed".

## 7. UI

The browser stays one overlay opened from the header preset name or
`Ctrl+O` (`gui-integration.md` 2, 17, 19). Its layout follows the mode
the editor is in. `PresetBrowserPanel` moves out of `Overlays.cpp` into
`Source/UI/PresetBrowser/` (see 14). `PluginEditor`'s
`showOverlay (&presetBrowser)` wiring does not change.

### 7.1 Easy Mode (780 x 560, the current size)

```
+--------------------------------------------------------------------------+
| PRESETS                                                         [Close]  |
| [ Search: warm clean_____________________ x ]  (spk) [====o---] Previews |
| (All)(Electric)(Acoustic)(Classical)(Bass)(Fav)(Recent)(Uses Techniques v)|
+-----------------------------------------------+--------------------------+
| > (heart) Jazz Hollowbody   warm clean jazz  *****| [guitar thumbnail]    |
|   (heart) Semi-Hollow Chime bright clean     ***  | Jazz Hollowbody       |
|   ...                     [row waveform sweep]    | Electric - Jazz       |
|                                                   | warm  clean  jazz     |
|                                                   | description...        |
|                                                   | [More like this]      |
+-----------------------------------------------+--------------------------+
| [Save As...]  [Morph] A:... ===o=== B:...            [Delete]  [ Load ]  |
+--------------------------------------------------------------------------+
```

Chips are single-select for family, and toggles for Fav and Recent.
Sort is always relevance while there is a query, and name otherwise.

"More like this" replaces the list with the 8 similar presets. A
breadcrumb "Like: Jazz Hollowbody [x]" takes you back.

### 7.2 Advanced Mode (1040 x 680, clamped to the window less 40 px)

```
+-----------+---------------------------------------------+---------------+
| SOURCE    | [Search_________________] Sort:[Relevance v]| thumbnail     |
| [x]Factory| > * (h) Name        Category  Guitar  Amp  Tags  Stars| name, cat |
| [x]User   |   ...                                       | descriptors   |
| [x]Packs  |                                             | (auto chips   |
| FAMILY    |                                             |  outlined)    |
| GENRE  v  |                                             | author, guitar|
| TECHNIQUES|                                             | amp, techniques|
|  (Uses Techniques chip + 6) |                           | description   |
| TONE      |                                             | SOUNDS LIKE   |
|  warm bright clean crunchy..|                           |  8 rows, each |
| RATING >= |                                             |  with play    |
| (h) Favourites  Recent      |                           |               |
| [Clear filters]             |                           |               |
+-----------+---------------------------------------------+---------------+
| [Save As...] [Morph row]   (spk)[vol] Previews: On      [Delete] [Load] |
+--------------------------------------------------------------------------+
```

- **Sort options:** Relevance (only while there is a query), Name,
  Category, Rating, Recently loaded, Most loaded, Date modified, and
  Similarity (only in "Sounds like" view).
- **Columns:** the list has sortable column headers.
- **Filters:** selections within one group are ORed; groups are ANDed.
  The filter state persists in UiPreferences.

### 7.3 Row and detail elements

- **Play glyph.** It is play when idle, a sweep while playing, a small
  spinner "Preparing preview..." while rendering, and a "!" when failed
  (tooltip: "Preview could not be rendered: <reason>. The preset can
  still be loaded."). It is hidden when previews are off.
- **Row waveform.** The waveform is the 64 peaks in the text-muted
  colour. The part already played takes the accent colour.
- **Descriptor chips.** Author tags are drawn filled. Auto-descriptors
  are drawn outlined with the tooltip "Detected from the sound". Clicking
  a chip adds it to the search.
- **Heart and stars.** The heart toggles favourite. The stars set a
  rating from 1 to 5; clicking the current star clears it.
- **Locked Pro presets (Free).** These show a lock glyph and play their
  preview. Enter or Load opens the upsell panel (`editions.md` 4.2).
- **Approximate marker.** A render that used a fallback guitar or IR
  shows "~" with the tooltip "Rendered with a fallback guitar; loading
  will show which part is missing."

### 7.4 Keyboard

The browser consumes these keys while it has focus. That overrides the
global Space (tune play/pause) and digit (snapshot) bindings of
`gui-integration.md` 17 while the overlay is focused.

| Key | Action |
|---|---|
| Typing any printable key, or Ctrl+F | focus search |
| Down from search | go to the list |
| Up / Down | move the selection; auto-preview (4.4) |
| PageUp / PageDown / Home / End | jump |
| Space | toggle the preview of the selected row |
| Enter | load (a double-click also loads) |
| Escape | stop the preview if one is playing; otherwise close (two presses at most, never stuck) |
| F | toggle favourite |
| 0-5 | set rating (0 clears) |
| S | open "Sounds like this" for the selection |
| Ctrl+Backspace | clear search and filters |

All keys are rebindable in Options -> ACCESSIBILITY under the group
"Preset browser".

### 7.5 Empty states and errors

- **No results:** "No presets match "warm djent". Try fewer words or
  clear filters." [Clear filters]
- **Favourites empty:** "Tap the heart on a preset to keep it here."
- **Recent empty:** "Presets you load appear here."
- **Previews off:** footer "Previews are off - Options -> Appearance".
- **Host not processing, or transport rule:** the footer hints of 4.3.
- **Cache folder not writable:** previews work in memory for the
  session. One Notifications banner: "Preview cache folder is not
  writable; previews will be re-rendered next time."

## 8. Options

A new **PRESET BROWSER** group at the bottom of Options -> APPEARANCE
(`AppearancePage` in `OptionsPages.h`), beside the tooltips switch. All
are `UiPreferences` keys, written immediately.

| Key | Control | Default |
|---|---|---|
| `presetPreview.enabled` | Previews On / Off | On |
| `presetPreview.trigger` | Hover / Click only | Hover |
| `presetPreview.onSelect` | Preview on keyboard selection | On |
| `presetPreview.volumeDb` | Preview volume, -40 to 0 dB | -6 dB |
| `presetPreview.whileTransport` | Preview while host or tune plays | Off |
| `presetBrowser.sort`, `.filters`, `.viewSimilar` | remembered view | - |

The browser footer's speaker icon and slider are mirrors that write the
same keys, so both places are always in sync.

At the defaults a preview is about -24 LUFS with peaks at or below
-9 dBFS.

## 9. Parameters, state, undo

- **Parameters:** none added. The parameter list is unchanged.
- **Preset:** only the optional `uid` and `previewPhrase` fields (5.4).
  A load, save, load round trip keeps them byte-identical under the
  existing round-trip test.
- **Session / uiState:** nothing. What the browser shows is
  UiPreferences; the favourite and rating library is
  `preset-library.json`.
- **Undo:**
  - Loading is the existing state boundary: `loadSelected()` still
    calls `pushUndoState ("Load preset")`.
  - Previewing, filtering, sorting, favourites, ratings and "Sounds
    like" are all off the undo stack. They are navigation or
    library data, like a rename (`action-and-undo.md` 3.8 and 7).

## 10. Accessibility

- **List rows** implement `AccessibilityHandler` with the name "Jazz
  Hollowbody, Electric Jazz, factory, favourite, 4 stars, warm, clean,
  jazz". The value interface is not used.
- **Play glyph** is a button: "Preview Jazz Hollowbody".
- **Announcements** (polite, verbosity Normal or above): "Previewing
  Jazz Hollowbody" and "Preview ready". A failed render is always
  announced.
- **Stars** are a slider-like control (`AccessibleValueInterface`, 0-5).
  The heart is a toggle.
- **Filter chips** are toggle buttons. The sidebar is in Tab order after
  the search box and before the list.
- **Reduced motion:** the waveform sweep becomes a static "playing"
  label, and the spinner becomes the text "Preparing".
- **Colour:** auto versus author chips differ in shape (outline versus
  fill), not only in colour.
- **Hover-only is never the only way:** keyboard and click reach
  everything a hover does.
- All strings go in the locale catalogue, including the descriptor
  words. Descriptor search matches in both the locale and English, so a
  French user can type "chaud".

## 11. Performance budget

Reference CPU as in `performance-budget.md`.

| Item | Budget |
|---|---|
| Audio-thread preview mix | <= 0.02 units; zero allocations and zero locks |
| Trigger to audible, clip decoded in memory | <= 30 ms (plus the dwell of 4.4) |
| Trigger to audible, clip on disk (decode and resample) | <= 60 ms |
| Render one 4 s preview, steady-state preset | <= 1.0 s wall, one core |
| Render the heaviest factory preset (8-String Djent, Physics Showcase) | <= 2.0 s |
| Offline instance creation (cold) | <= 400 ms (`performance-budget.md` 5) |
| Worker instance RSS while alive | <= 300 MB extra; freed 30 s after idle; total stays under the 900 MB cap |
| Decoded clip memory | <= 16 MB (LRU of decoded clips) |
| Disk cache | <= 128 MB LRU; shipped factory previews <= 3 MB |
| Search keystroke to repainted list | <= 16 ms at 5,000 presets; <= 4 ms at 500 |
| Similarity query | <= 2 ms at 5,000 presets |
| Browser open with a warm index | <= 150 ms to the first painted list |
| Cold index build | <= 1.5 s for 500 presets, on the worker |

## 12. Editions

| Feature | Free | Pro |
|---|---|---|
| Previews of Free factory presets and the user's own presets (rendered on save and on view) | Yes | Yes |
| Previews of the 20 Pro factory presets | Yes, shipped clips; the presets are locked | Yes |
| Search, descriptors, filters, sort, favourites, ratings, recent | Yes | Yes |
| "Sounds like this" | Yes; locked Pro presets can appear, marked with a lock | Yes |
| Techniques filter | Yes; it filters by what presets use, which Free can see | Yes |

These are "Both" rows under `editions.md` 2.3 ("User presets: save,
load, browse, tags, favourites"). The reason is `editions.md` rule 0.2:
the browser is fundamental. Hearing locked presets is the quiet upsell
of rule 0.8, with no modal and nothing on a timer.

Free's local renders use Free's effective values, so a Free user never
hears a preview that their edition cannot play.

## 13. Interactions

- **Morph** (`ambiguity-resolutions.md` 5.2): previews behave the same.
  Enter loads into the selected slot, as `loadSelected()` does today.
- **Snapshots:** the preview plays the preset's base state, which is
  what a load gives. A snapshot recall while previewing is unaffected.
- **Host automation / MIDI Learn:** untouched. Previews write no
  parameters.
- **Rhythm engine / Tune Builder:** the live engines are untouched. A
  tune transport blocks previews (4.3). Preset-rendered previews respect
  the preset's own rhythm engine (3.1).
- **Techniques:**
  - The "Uses Techniques" chip (`gui-techniques-updates.md` 7) is kept
    in both layouts: the chip row in Easy, the TECHNIQUES group in
    Advanced.
  - Arm flags come from 6.1.
- **MIDI export, notation, capture, looper, session recorder,
  per-string and aux outputs:** previews are excluded, by the insertion
  point in 4.2.
- **Workshop:** the guitar thumbnail uses `guitar-illustration.md` 15.
  Saving a guitar that presets reference changes their hashes, so they
  are rendered again when next viewed.
- **Setlists:** a setlist step loads presets normally. Nothing here
  changes that.
- **Content updates:** pack previews are verified by the pack signature
  (2.1).
- **Onboarding:** the tour's preset stop (`onboarding.md`) says "Hover
  to hear, Enter to load". Coordinate the tour text.
- **Audition button / Workshop shadow audition:** these are live-engine
  notes. A preview mixes on top of them; neither stops the other.

## 14. New and changed code

New files:
- `Source/Presets/Preview/`:
  - `PreviewPhrase`
  - `PreviewRenderer`
  - `PreviewRenderHost` (moved from `Tools/RenderCli.cpp`)
  - `PreviewRenderService`
  - `PreviewCache` (disk, lock files, LRU, manifest)
  - `PreviewClipPool`
  - `PreviewPlayer`
  - `ToneFeatures`
- `Source/Presets/Search/`:
  - `PresetIndex`
  - `PresetSearch`
  - `ToneDescriptors`
- `Source/Presets/PresetLibraryPrefs`
- `Source/UI/PresetBrowser/`:
  - `PresetBrowserPanel` (moved from `Overlays`)
  - `PresetRow`
  - `PresetFilterSidebar`
  - `PresetDetailPane`
- `scripts/render_previews.sh`
- `Resources/Presets/Previews/`
- `Resources/Presets/descriptor-calibration.json`

Changed files:
- `PresetManager`: `PresetInfo` fields, `uid` on save, a render request
  on save, known keys.
- `LuthierAudioProcessor`: owns `PreviewPlayer` and
  `PreviewRenderService`; the processSlice insertion; `panic`.
- `Tools/RenderCli.cpp`: `--render-previews`.
- `OptionsPages` (`AppearancePage`, `FileLocationsPage`).
- `CMakeLists.txt`: link `juce_audio_formats` with
  `JUCE_USE_OGGVORBIS=1`, and the new sources.

## 15. Failure modes

| Failure | Response |
|---|---|
| Preset JSON corrupt or refused | No preview; the row shows "!" and is still searchable by name. Load gives the normal banner. |
| Missing guitar or IR at render | Render with the load fallback; mark approximate (7.3). |
| Render throws, or goes past 10 s | Mark failed and log it. Retry once on the next browser open, then not again until the hash changes. |
| Decoding a cached clip fails | Delete the entry and re-render. |
| Cache not writable, or disk full | Keep clips in memory for the session; one banner (7.5). |
| Sample rate changes mid-preview | Fade and stop; resample again on the next request. |
| Editor closed during a preview | `overlayHidden()` stops it with a fade. The processor keeps rendering queued on-save jobs. |
| Processor destroyed | The service cancels and joins (500 ms or less); clips are freed after the audio thread releases them. |
| Host stops calling processBlock | Request dropped; footer hint (4.3). |
| Two instances render the same hash | The lock file prevents it; a duplicate write is atomic and harmless. |
| Shipped manifest missing (a stripped install) | Factory presets render locally like user presets. |

## 16. Tests

These run in `LuthierTests`. GUI tests run under xvfb in the style of
`Source/Tests/EditorTests.cpp`. The new test files are
`PresetPreviewTests.cpp`, `PresetSearchTests.cpp` and
`PresetBrowserUiTests.cpp`.

1. **PB-01** Rendering the same preset twice with `PreviewRenderer`
   gives bit-identical float buffers and identical Ogg bytes.
2. **PB-02** Every factory preset's fresh render matches its shipped
   clip: log-spectrum difference 1.0 dB RMS or less over 40 bands, and
   loudness within 0.5 LU (codec tolerance). The manifest `soundHash`
   equals the hash computed from `FactoryPresets::toVar`.
3. **PB-03** Every clip is 2.0-4.0 s long. Integrated loudness is
   -18 ±0.5 LUFS, unless peak-limited to -3 dBTP (true peak -3.0 dBTP
   or less). The last 10 ms are below -60 dBFS.
4. **PB-04** Phrase selection: "Modern Metal Chug" and "8-String Djent"
   get `highgain_riff`; "Strummed Dreadnought" gets `acoustic_strum`;
   "P-Bass Flatwound" gets `bass_groove`; "Flamenco Rasgueado" gets
   `rasgueado`; "Blues Slide" gets `slide_lick`. A `previewPhrase` field
   overrides the table.
5. **PB-05** The phrase root follows tuning: the lowest note of
   "Drop C Riff"'s phrase is C2, and "5-String Low B"'s is B0.
6. **PB-06** Previewing does not change the live instance. APVTS state,
   `GuitarSpec` hash, snapshot bank and undo depth are identical before
   and after 20 previews.
7. **PB-07** No allocations and no locks on the audio thread: 60 s of
   continuous preview switching every 150 ms with the heap and mutex
   traps armed.
8. **PB-08** Fades: the first 10 ms rise monotonically from 0. A stop
   reaches -90 dBFS within 30 ms. A switch never has both clips above
   -60 dBFS in the same sample.
9. **PB-09** A held live note is not interrupted. The per-string output
   during a preview equals the same render with no preview, with a null
   below -120 dB on the per-string aux.
10. **PB-10** Transport rules (4.3): with the host playhead playing,
    hover and Space requests produce silence. With
    `whileTransport` on, they play. `isNonRealtime()` always gives
    silence. The kill switch gives silence within 30 ms.
11. **PB-11** Exclusion: during a preview, the looper, session
    recorder, Tone Match capture, aux buses and MIDI out receive
    exactly what they receive without it (null or byte-equal).
12. **PB-12** Level: at -6 dB volume the output peak with no live
    playing is -9 dBFS or less. Changing the volume is smoothed, with
    no step above 0.01 in linear gain per sample.
13. **PB-13** A sample-rate change from 44.1 to 96 kHz mid-preview
    stops it cleanly. The next preview plays at the correct pitch:
    centroid within 1 % of the 48 kHz reference.
14. **PB-14** Cache key: renaming or retagging keeps the hash. Changing
    one parameter, the referenced guitar file, the plugin version
    string or `kRenderRevision` changes it.
15. **PB-15** On save: `saveAs` produces a cache entry within 3 s on
    the test runner. The sidecar has features and descriptors.
16. **PB-16** LRU: filling the cache to 130 MB prunes it to 128 MB or
    less, evicting the least recently played. Lock files older than 30 s
    are ignored.
17. **PB-17** Two processors requesting the same hash at once give one
    render (counter), and both play.
18. **PB-18** Failures: a corrupt preset gives a failed row and no
    crash. A render past the timeout, forced by a test hook, is
    abandoned within 10.5 s. An unwritable cache folder gives in-memory
    playback plus exactly one banner.
19. **PB-19** Shutdown: destroying the processor mid-render returns
    within 500 ms, with no leak (leak detector).
20. **PB-20** Descriptors on the factory bank:
    - "Jazz Hollowbody" has warm, clean and jazz.
    - "8-String Djent" has high-gain and djent.
    - "Ambient Swell" has spacious.
    - "Semi-Hollow Chime" has bright.
    - "Single-Cut Crunch" has crunchy.
    - None of the Bass presets has djent.
21. **PB-21** Search:
    - "warm clean" returns "Jazz Hollowbody" in the top 3 and no
      preset whose drive index is above 0.5.
    - "djent" returns "8-String Djent" first.
    - "cruchy" (a typo) finds the same set as "crunchy".
    - "clean jazz" equals "jazz clean".
22. **PB-22** Search fields: a query that matches only the author,
    guitar name or amp name finds the preset. A name match outranks a
    description-only match.
23. **PB-23** Filters: Family=Bass and Genre=funk AND together; two
    Genres OR. "Uses Techniques" with Slap returns exactly the presets
    with `slap_armed` on. Favourites and Recent reflect
    `preset-library.json`.
24. **PB-24** Similarity: the 8 neighbours of "Modern Metal Chug"
    include "Drop C Riff" and "8-String Djent", and none of them is
    acoustic or classical. The preset itself is never listed. The result
    is deterministic.
25. **PB-25** Favourites and ratings survive a rename through the
    browser, an editor close and reopen, and a new processor instance.
    They are never written into the preset file.
26. **PB-26** Preset file: `uid` is created on the first save and kept
    on re-save. Load-save round-trips of factory and user files stay
    byte-identical (the existing `file-formats.md` 16 test).
    `previewPhrase` is preserved.
27. **PB-27** Performance:
    - search at 5,000 synthetic index entries takes 16 ms or less per
      keystroke;
    - similarity takes 2 ms or less;
    - a warm browser open takes 150 ms or less;
    - one steady-state render takes 1.0 s wall or less on the reference
      runner (a CI perf job, not a unit gate);
    - preview mix cost is 0.02 units or less.
28. **PB-28** GUI, Easy (xvfb): the overlay opens with `Ctrl+O`.
    Typing focuses search. Down moves to the list. Space starts a
    preview (PreviewPlayer reports active). Escape stops it, and a
    second Escape closes the overlay. Enter loads, and an undo boundary
    "Load preset" is pushed.
29. **PB-29** GUI, Advanced: the sidebar, sort combo, columns and
    "Sounds like" pane are visible and laid out without overlap at
    1280 x 800 and 2560 x 1600. In Easy they are absent, and the chip row
    and "More like this" are present.
30. **PB-30** Hover: resting 300 ms on a row starts a preview. Leaving
    the list stops it within 30 ms of audio time. With trigger = Click
    only, hovering never starts one.
31. **PB-31** While the browser is focused, digits and Space do not
    recall snapshots or toggle the tune transport. With it closed they
    do.
32. **PB-32** Accessibility: every row, play glyph, heart, star control
    and chip has a non-empty accessible name. Tab order goes search,
    chips or sidebar, list, detail, footer. "Previewing <name>" is
    announced. Reduced motion shows no sweep animation (pixel-stable
    across frames).
33. **PB-33** Options: the five `presetPreview.*` keys persist across
    instances. The footer mirror and the APPEARANCE group stay in sync.
    Previews off hides the play glyphs and blocks every trigger.
34. **PB-34** Empty states: a nonsense query shows the no-results text
    with Clear filters. Empty Favourites and Recent show their hints.
35. **PB-35** Edition (Free build config): the 20 Pro presets are
    listed locked and preview from shipped clips. Enter opens the upsell
    panel, not a load. Free renders of user presets use neutralised Pro
    values: a preset with `slap_armed` on renders equal to one with it
    off.
36. **PB-36** Combination (`CombinationTests.cpp` harness): a preview
    during Morph mode, tune playback with `whileTransport` on, slide
    mode on the live instance, an active mod matrix, and a MIDI Learn
    arm. Every live module's output is unchanged relative to the run
    with no preview, and no assertion fires.
37. **PB-37** GuiReachability: the browser, its Options group and the
    cache Open/Clear buttons are reachable in 3 interactions or fewer
    from the header in both modes (`gui-integration.md` 0.2).
