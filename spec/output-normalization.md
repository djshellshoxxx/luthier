# OUTPUT NORMALIZATION SPEC

An optional switch that brings every guitar type, preset and option
combination to one consistent perceived loudness at the main output.

Added 2026-09-24 at the product owner's request, because some sounds are
much too quiet next to others (a nylon-string or a clean single-coil
against a high-gain humbucker). It is additive. It moves nothing that
is already placed. Test ID prefix: **ON-**.

## 0. Ground rules

1. **Off by default, and off means today.** With the switch off, the
   audio path is bit-identical to the build before this feature. The
   normalization stage is skipped entirely, not multiplied by 1.0.
2. **Calibrated static gain, never auto-gain.** The gain depends only on
   the *sound configuration* (section 3). It is measured on a fixed
   reference phrase, never on what the player plays. Playing softer is
   still quieter, and a palm-muted chug is still quieter than an open
   chord. Nothing pumps.
3. **Honest about what it costs.** Normalization erases the natural
   level differences between instruments and settings, so it affects
   the realism the rest of the spec set protects ("honest magnitudes",
   INDEX global rules). The UI says so every time the switch is turned
   on, and permanently in a caption under it.
4. **Boosting can never clip.** A true-peak safety stage after the gain
   holds the main output at or below -1.0 dBTP while the switch is on.
5. **Real-time rules hold.** The audio thread does not allocate, lock,
   hash or render. Measuring happens on a worker thread, and the result
   reaches the audio thread as one atomic word.
6. **Offline renders are deterministic.** The same session renders to
   the same bits, whatever the worker's timing or the cache contents
   (section 4.6).
7. **Main output only.** Per-string outputs and the aux tap outputs keep
   their natural level (section 10).

## 1. User stories

- *Browsing:* "I flip from the Jazz Box preset to Metal Chug and back
  without reaching for my monitor volume."
- *Writing:* "My tune uses a nylon-string section and a crunch section.
  With normalization on they sit at the same level, and I do not have to
  ride a fader in the DAW."
- *Mixing:* "I bounced the song last week with normalization on at
  -14 LUFS. I reopen the project on my laptop and it renders to the same
  level." (host recall, section 6)
- *Realism:* "I compare a Tele and a Les Paul on the Workshop bench to
  hear the real level difference. The caption reminds me to turn
  normalization off." (It stays off unless I turn it on.)
- *Dynamics:* "With it on, digging in is still louder than a light
  touch."

## 2. Behaviour

### 2.1 Switch and target

- **Normalize output loudness**: on / off. Default **off**.
- **Target**: -14, -16, -18, -20 or -23 LUFS. Default **-18 LUFS**.
  -18 is the level preset previews already use
  (`preset-browser-previews.md` 3.2), and it leaves the most headroom.
  -14 suits streaming references. -23 is EBU R128.
- The gain the audio thread applies is
  `clamp (target - measured, -12 dB, +24 dB)`, rounded to 0.01 dB. It is
  applied ahead of `master_gain`. So `master_gain` stays the user's own
  trim, and the target holds at `master_gain` 0 dB.

### 2.2 Transitions (no clicks)

Every change to the applied gain is a **linear-in-dB glide of 300 ms**.
That covers switching on or off, changing the target, and a new
calibration result arriving. The glide is evaluated per 32-sample
segment aligned to absolute timeline position, with linear interpolation
of the linear gain inside each segment. So the gain curve does not
depend on the block size. There is one exception. On a preset or guitar
load whose calibration is already cached (4.4), the new gain rides the
load's own crossfade (`state-model.md` 2 step 7, 30 ms), so the new sound
never plays at the old sound's gain.

### 2.3 When it recalibrates

1. **Discrete configuration events** request a calibration at once, with
   no debounce: preset load, guitar load, part swap, IR slot change,
   pedal type change or move in a rack slot, mod-route add or remove,
   switching normalization on, oversampling change, and undo or redo
   that changes any of these.
2. **Config-role parameter changes** (3.1), from the UI, host automation
   or a MIDI-learned CC, request a calibration **250 ms of timeline
   after the last change**. A continuous automation sweep therefore
   holds the gain until it rests. Then it recalibrates once.
3. **Nothing else** requests a calibration. That includes playing, the
   rhythm engine, the tune player, modulation (which the reference
   render includes as configured), snapshot recall and morph (3.2),
   Performance- and Mix-role parameters, Freeze, E-Bow and feedback.
4. While a request is being measured, the current gain **holds**. When
   the result arrives it glides (2.2). A result for a request that has
   since been superseded is discarded (serial check).

## 3. What "the sound configuration" is

### 3.1 Loudness roles

Every APVTS parameter has exactly one `LoudnessRole`, in a table in
`Source/DSP/Master/LoudnessRoles.h/.cpp`. The default for a parameter id
that is not listed is a **test failure** (ON-26), not a silent default.
That way every future spec has to classify its parameters.

| Role | Meaning in the reference render | Triggers recalibration | Parameters |
|---|---|---|---|
| **Config** | Rendered at its current value, quantised (3.3) | Yes (2.3.2) | Everything not listed below: guitar type and every instrument, string, body, pickup, circuit, amp, cab, mic, room, setup, noise, pick, squeak, slide-mode, tone-strip (`output_mix`, `stereo_width`, `input_gain`), macro (except humanize), `oversampling`, `character` params, and every effect-slot type and parameter except those EffectsChain marks `isPerformanceControl` |
| **Performance** | Rendered at its **default** value | No | `guitar_volume`, `whammy_position`, `playing_mode`, `mpe_enabled`, `bend_range`, `strum_speed`, `strum_direction`, `chord_window`, `vibrato_*`, `legato_window`, `transpose_lock`, `freeze_*`, `ebow_*`, `feedback_*` (all, including `feedback_on`), `hum_*`, `macro_humanize`, `preset_morph_position`, every `scrape_*`, `slap_*`, `pop_*`, `ghost_*`, `double_thump_*`, `strum_crossing_sps` … `chuck_damping` (strum-dynamics), `finger_alternation_variation`, `rest_stroke`, and effect-slot parameters marked `isPerformanceControl` (wah / volume-pedal position, expression targets) |
| **Mix** | Excluded: forced to neutral in the render | No | `master_gain` (0 dB), `limiter_on` (off), `aux1_pre_circuit` (not on main) |

The guitar's volume knob is Performance on purpose. Rolling it back is a
playing gesture that should still make the guitar quieter, as it does on
a real guitar. The pickup selector, per-pickup volumes and the tone knob
are Config. Choosing the neck pickup for a solo is a sound change, and
the product owner listed pickups among the things to normalize.

### 3.2 Snapshots and morph are performance

Snapshot recall and snapshot morph both go through
`SnapshotBank::applyBlend` (`Source/Live/Snapshots.h`). Preset morph goes
through `PresetMorph`. Each of these writes parameters inside a
`ParameterBridge::PerformanceWriteScope`. The bridge tags those writes
with source `performance` in its existing per-parameter `lastWrite`
record, and the change tracker (4.1) ignores them. **Decision:** a
snapshot bank is a set of performances inside one sound (verse vs solo),
and a lead snapshot that is louder on purpose must stay louder. What the
owner asked to equalise is presets, guitar types and settings. Preset
morph is the one crossing between two presets, so it gets its own rule.
When `preset_morph_position` is between 0 and 1 exclusive, the applied
gain is `lerp (gainA, gainB, position)` in dB, using each endpoint
preset's own calibration. The calibrator requests both endpoints when a
morph pair is armed. After a snapshot recall, the next *Config* change
calibrates the current full state, snapshot values included. That is the
sound the user is now editing.

### 3.3 Configuration hash

`NormalizationCalibrator::hashConfig` is SHA-256 over:
- canonical JSON (sorted keys, `%.9g`) of every Config parameter,
  quantised to 1/1024 of its normalised range (choices and bools exact);
- Performance parameters, replaced by their defaults (so they cancel);
- the canonical resolved `GuitarSpec` (the same serialisation
  `preset-browser-previews.md` 5.1 uses), the mod matrix, and the
  effect-rack types;
- size and mtime of every referenced user IR;
- the edition (`Edition.h`), and `kCalibrationRevision` (starts at 1).

The render uses the *quantised* values, so the gain is a pure function of
the hash. That is what makes caching deterministic (4.6).
`kCalibrationRevision` is bumped only when the phrase, the meter or the
engine's level moves (the ON-03 CI gate catches drift beyond 0.5 LU). An
ordinary update keeps stored project gains.

## 4. Engine and processing design

### 4.1 Audio thread

**`MasterBus`** (`Source/DSP/Master/MasterBus.h/.cpp`) gains a member
**`LoudnessNormalizer normalizer`**
(`Source/DSP/Master/LoudnessNormalizer.h/.cpp`, header-light, no
allocation after `prepare`):
- `setEnabled (bool)`, `setTargetLufs (double)`, and
  `publishResult (uint32 serial, int32 gainCentiDb, uint8 flags)`: one
  `std::atomic<uint64_t>` packed word.
- `applyLoadGain (int32 centiDb)`: the cached-load path of 2.2.
- `isActive()`: true while enabled, or while a glide towards 0 dB has not
  finished. **When it is false, `MasterBus::processBlock` runs today's
  loop unchanged** (a block-level branch, so there is no per-sample test
  and no ×1.0).
- When it is active, today's per-sample `g = gainSmooth.next()` becomes
  `g = gainSmooth.next() * normalizer.next()`, at the same point: ahead
  of the DC blocker, the LUFS meter and the limiter.

**True-peak safety.** While the normalizer is active, the existing
lookahead limiter switches its detector to a 4x-oversampled true peak.
The new `Source/DSP/Master/TruePeakDetector.h` is a 48-tap polyphase FIR
(12 taps per phase), BS.1770-4 Annex 2 style, run on the incoming sample
as the limiter already does. The ceiling glides from -0.3 dBFS to
**-1.0 dBTP** over 300 ms. Attack (0.5 ms), release (80 ms), the 1.5 ms
lookahead and the absolute backstop clamp are unchanged, so reported
latency does not move. The stage is forced on while normalization is on,
even when `limiter_on` is off, and the Options page says so. When
normalization is turned off, the detector returns to sample-peak once the
gain glide has finished.

**Change tracker.** `ParameterBridge` (`Source/Parameters.h/.cpp`)
already polls every parameter each block. It gains a
`std::vector<uint8_t> loudnessRole` built in the constructor from 3.1, and
a `ConfigChangeTracker` of plain integers:
- A Config parameter whose value changed this block, with a `lastWrite`
  source that is not `performance`, sets
  `dirtyAt = timelineSample`.
- The discrete events of 2.3.1 call `markConfigDirty (immediate = true)`
  from the command-queue apply (`ui-wiring.md` 4.3), which already runs
  on the audio thread at the block boundary.
- When the tracker is dirty and `timelineSample - dirtyAt >= 0.25 s *
  sr` (or immediate), it increments `requestSerial` (an atomic) and
  records the request's timeline sample. That is two stores. No FIFO, no
  signal. The worker polls.

### 4.2 Worker: `NormalizationCalibrator`

`Source/Support/NormalizationCalibrator.h/.cpp`, owned by
`LuthierAudioProcessor`. It is created when normalization is first
enabled or when a session with it on is restored. It is a
`juce::Thread` at low priority, and it wakes every 10 ms to compare
`requestSerial`. Per request:
1. `LuthierAudioProcessor::captureSoundState()` is a new, worker-safe
   variant of `captureStateBlock()`. It reads parameter atomics and the
   immutable structural snapshots already published for the audio thread
   (GuitarSpec pointer, mod-matrix state, IR slot descriptors). It never
   touches the message thread, so an offline bounce that blocks the
   message thread cannot deadlock it.
2. It hashes (3.3) and looks the hash up in the caches (4.4).
3. On a miss, it renders (4.3) and measures.
4. It publishes `{serial, gainCentiDb, flags}` to the normalizer.
   `flags` records clamped high, clamped low, unmeasurable, estimate or
   failed. It also stores `{hash, measuredLufs, gainDb, source}` for the
   UI and the session.

### 4.3 The reference render

- **Instance.** An offline `LuthierAudioProcessor` from
  `createOfflineInstance()`. It is borrowed from
  `PreviewRenderService` (`preset-browser-previews.md` 3.3) through a new
  top-priority lane, `calibration`. That lane pre-empts a running
  background or interactive preview (which is re-queued) and is **not**
  paused while the transport runs. Until that service exists, the
  calibrator owns one instance with the same rules: lazy, reused,
  destroyed 30 s after the queue empties. The instance's own
  normalization is forced off (`setCalibrationRenderMode (true)`), so
  there is never a nested calibrator.
- **Setup.** The captured state with Performance parameters at their
  defaults, `master_gain` 0 dB, `limiter_on` off, practice tools, tune
  player and preview player idle. It runs at `prepareToPlay (48000, 256)`
  with the live oversampling setting, a fixed 100 BPM transport, and
  `setNonRealtime (true)`. First 0.5 s of silence settles the engine and
  is discarded.
- **Phrase.** A new `NormalizationPhrase`
  (`Source/Support/NormalizationPhrase.h/.cpp`, a sibling of
  `AuditionPhrase`), fed through `LuthierEngine::setDirectMidi`. So it is
  played as written, whatever the rhythm engine, voicer or playing mode.
  The root is the guitar's lowest open string after tuning and capo, as
  in previews.
  - Guitar families: 0-2 s, four down-strummed chords I-IV-V-I from a
    fixed shape table (20 ms spread, velocity 90, 0.5 s each). 2-4 s,
    eight eighth-notes at 120 BPM on the middle strings, velocities
    70 / 100 alternating. 4-5 s, one chord at velocity 100, let ring.
  - Bass family: 0-4 s, a root-fifth-octave eighth-note line at
    velocities 80 / 100. 4-5 s, a held root and octave double stop.
  - The measurement window runs from phrase start to 6.0 s.
- **Meter.** `Source/DSP/Master/Bs1770Meter.h/.cpp` measures
  integrated loudness per ITU-R BS.1770-4: the exact K-weighting
  coefficients recomputed for the sample rate, 400 ms blocks with 75%
  overlap, an absolute gate at -70 LUFS and a relative gate at -10 LU.
  It is the one implementation shared with `PreviewRenderer` 3.2 step 4.
  (Coordinate: the preview spec must use it rather than its own.) The
  existing short-term meter in `MasterBus` is unchanged.
- **Cost.** At most 1.5 s of wall time for the Heavy preset on the CI
  reference machine. A render is cancelled if a newer serial arrives, and
  abandoned at 10 s (section 12).
- **Sample rate.** Calibration always renders at 48 kHz, and the gain is
  used at any live rate. Loudness is rate-invariant to within 0.2 LU
  (ON-33), and one gain per hash is what makes recall exact.

### 4.4 Caches

Caches are looked up in this order. Each is memoisation of the pure
function of 3.3.
1. **In-memory LRU**, 512 entries per process, shared by instances.
2. **Factory table** `Resources/NormalizationFactory.json`, in
   BinaryData, about 50 KB, one per edition. It is generated in CI by
   `luthier-render --calibrate-factory` (`Tools/RenderCli.cpp`) for every
   factory preset x guitar type (36 x 26), so factory sounds never wait.
3. **Disk cache**, `<preview cache root>/normalization/<hash16>.json`
   (`{measuredLufs, gainCentiDb, revision}`). Writes are atomic per
   `file-formats.md` 13. It is a cache, not user data. It is cleared by
   Diagnostics "Reset all settings and clear caches".

**Prefetch.** When a preset load is prepared on the message thread
(`state-model.md` 2), `PresetManager` asks the calibrator's
`lookupCached (hash)`. On a hit, the gain rides in `PresetData` and is
applied with the load (2.2). Loading a tune or a setlist queues
background calibrations for every preset it references, so a section
change or a setlist step does not wait on a measurement.

### 4.5 Analytic estimate

If a render fails or times out, the gain is estimated. The estimate is
the factory-table entry with the same guitar type and amp model and the
nearest drive index (`preset-browser-previews.md` 6.1), flagged
`estimate`. The UI shows it as "about +x dB". It is never cached to disk.

### 4.6 Offline determinism

When `isNonRealtime()` is true (host bounce, `AudioExporter`, `luthier-
render`), the audio thread, at the block where a request fires, polls
the result word with 1 ms sleeps until its serial arrives. It gives up
after 10 s and then uses the estimate. **This is the one sanctioned wait
on the audio thread, and it happens only in non-realtime mode.** Every
host allows blocking there, and it is the only way the render cannot
depend on worker timing. Together with the pure-function gain (3.3), the
timeline-sample debounce (2.3) and the timeline-aligned glide (2.2), the
applied gain curve of an offline render is a pure function of the
session and its automation. `prepareToPlay` in non-realtime mode verifies
a restored calibration synchronously. It may block there, because it is
not the callback. `AudioExporter::run` must call `setNonRealtime (true)`
on its instance (coordination).

## 5. UI

### 5.1 Location (decision)

`gui-integration.md` 5 puts sound-affecting options the plugin owns on
**Options -> AUDIO**, where `AudioPage` already holds Oversampling
(`Source/UI/OptionsPages.h`). "Show tooltips" is on APPEARANCE, which is
about the window, not the sound. The switch goes on AUDIO, under
Oversampling, in a new group:

```
AUDIO
  Oversampling                         [ 4x            v ]
  -- OUTPUT NORMALIZATION ----------------------------------------
  [x] Normalize output loudness
      Target loudness                  [ -18 LUFS      v ]
      Normalization: +7.5 dB   (this sound measures -25.5 LUFS)
      ! Evens out the natural level differences between guitars
        and settings, so relative levels are no longer realistic:
        a nylon-string or a clean single-coil will sound louder
        than it really is next to a high-gain humbucker.
      Applies to the main output only. Per-string and aux outputs
      keep their natural level. The safety limiter stays on while
      normalization is on.
```

**Secondary access (both modes).** The header Meters region
(`gui-integration.md` 2) gets a 40 px `NormalizationBadge`
(`Source/UI/NormalizationBadge.h/.cpp`) to the left of the output meter,
visible only while normalization is on: "N +7.5". In Easy Mode the same
badge also sits under the `LevelMeter` in `EasyPanel`'s meter column.
Clicking the badge (or pressing Enter on it) opens Options -> AUDIO with
the switch focused. Its tooltip is "Output normalization +7.5 dB, target
-18 LUFS. Click for options." That is one interaction from the header,
which meets ground rule 2. Rows the coordinator adds: to
`gui-integration.md` 5, AUDIO gains "output normalization". To 19:
`| Output normalization | MasterBus::LoudnessNormalizer, NormalizationCalibrator | Options AUDIO | Header meter badge (Easy + Adv) | - |`.

### 5.2 States of the readout

| State | Readout | Badge |
|---|---|---|
| Off | "Off. Each sound plays at its natural level." (the warning caption is hidden and the target is disabled) | hidden |
| Measuring (first time, or cache miss) | "Measuring this sound…" (static text, no spinner, so reduced motion is honoured) | "N …" |
| Applied | "Normalization: +7.5 dB (this sound measures -25.5 LUFS)" | "N +7.5" |
| Clamped | "Normalization: +24.0 dB (at the limit; this sound is very quiet)" | "N +24!" |
| Estimate | "Normalization: about +6 dB (estimated; measuring failed)" | "N ~+6" |
| Unmeasurable (< -70 LUFS) | "This sound is silent on the test phrase; level unchanged." | "N 0" |
| Morphing | "Normalization: +4.1 dB (between the two morph presets)" | value |

The readout refreshes from a 10 Hz message-thread timer reading the
calibrator's status struct (`gui-engine-dataflow.md` style: stale after
2 s shows the last value in the muted colour).

### 5.3 The warning

Every time the user turns the switch **on** from the UI, a non-blocking
**info banner** is posted (`Source/UI/Notifications.h`, id
`normalization.on`, gui-integration 15): "Output normalization is on.
Every sound is brought to the same loudness, so the natural level
differences between guitars and settings are gone. Turn it off for
realistic relative levels." It has actions **[Options]** and **[Don't
show again]**, so it does not auto-dismiss. "Don't show again" sets the
`UiPreferences` key `normalization.bannerSuppressed`. The caption under
the switch (5.1) is always shown while it is on and cannot be
suppressed. Turning it on from a restored session posts no banner. The
badge is the reminder there.

### 5.4 Other panels

- **ROUTING tab:** while normalization is on, one caption line: "Output
  normalization applies to the main output only."
- **Workshop bench header:** while normalization is on, a muted note:
  "Normalization is on: level differences between parts are evened out.
  Shadow audition (Alt-hover) still plays at the real level." Shadow
  audition is not a configuration event (2.3.3), so it stays honest.
- **Diagnostics:** "What's on the audio path" lists the stage with its
  gain. The State Inspector shows the hash (16 hex digits), measured
  LUFS, gain, source (factory / memory / disk / render / estimate) and
  the true-peak gain reduction. The debug overlay (`Overlays.cpp`) adds
  lines for normalization gain and true-peak GR.

All strings are in the locale catalog under `options.audio.normalization.*`,
`banner.normalization.*` and `badge.normalization.*`.

## 6. State and serialization (decision: both)

- **Session state (authoritative).** `getStateInformation` writes a root
  key next to `metronome`:
  `"normalization": {"version":1, "enabled":true, "targetLufs":-18.0,
  "calibration":{"hash":"<64 hex>", "measuredLufs":-25.51, "gainCentiDb":751,
  "flags":0, "revision":1}, "morph":[{…A…},{…B…}]}`.
  On restore, the normalizer **snaps** (no glide) to the stored gain from
  the first block, and the calibrator verifies the hash in the
  background. A mismatch (an IR file changed, a revision bump)
  recalibrates and glides in realtime, or runs synchronously in
  `prepareToPlay` when non-realtime (4.6). **Reason:** host recall must
  reproduce the loudness the user mixed at, on any machine, whatever
  that machine's preferences. A preference alone would make the same
  project render at different levels for two collaborators.
- **User preference (default for new instances).** `UiPreferences` keys
  `normalization.defaultEnabled` (false) and
  `normalization.defaultTargetLufs` (-18) record the user's last choice.
  A new instance with no restored state starts from them. A restored
  session **without** the key (every project saved before this feature)
  loads **off**, whatever the preference, so old projects stay
  bit-identical.
- **Not preset data.** The state is outside `presets.toVar()`. Presets,
  snapshots, A/B slots and `.luthierset` / `.luthiertune` files never
  carry it, and loading them never changes it. It is not an APVTS
  parameter (section 7).
- **Restores that keep it.** Undo, redo and A/B recall call
  `setStateInformation` with a full state block today
  (`PluginProcessor.cpp` `undo`, `redo`, `recallSlot`). They move to a
  private `restoreState (data, size, RestoreScope::soundOnly)`, which
  ignores the `normalization` key and keeps the live settings. The host
  path uses `RestoreScope::full`.
- Toggling it or changing the target calls
  `updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true))`,
  so the host marks the project dirty.

## 7. Parameters

**None added.** Normalization is a listening policy, not part of a
sound. An APVTS parameter would be saved into every preset (presets
store the parameter tree), would appear in randomise and in morph, and
would let an automation lane switch the loudness policy mid-song. All of
that contradicts section 6. Host recall is covered by the state chunk.
So the parameter list is untouched, and no append is needed.

## 8. Undo

Changing the switch or the target is an Option, so it is **not
undoable** (`action-and-undo.md` 3.17, "an option, not a value"). It
pushes no entry and no boundary, and undo or redo of other actions never
changes it (6). Calibration results are not undoable either (they are
derived state, 7).

## 9. Accessibility

- The switch is a `ToggleButton` with accessible name "Normalize output
  loudness". Its accessible *description* is the warning caption, so a
  screen reader hears the warning when the switch is focused, whether it
  is on or off.
- The target is a `ComboBox` labelled "Target loudness". It is disabled
  (and says so) while normalization is off.
- The readout is a static-text label. When the applied gain changes by
  0.5 dB or more, it sends a polite announcement ("Normalization plus
  7.5 decibels"), at most one every 2 s, and only at verbosity standard
  or verbose.
- The badge is a focusable `Button` in the header Tab order, after the
  output meter. Its name is "Output normalization, plus 7.5 decibels".
- There is a rebindable command "Toggle output normalization" in the
  shortcut table (accessibility 2), unbound by default. The banner is
  also posted when the command turns the switch on.
- Colour: the "!" glyph and the words carry the warning, and the
  warning colour is never the only cue (`accessibility.md` 3).

## 10. Interactions

| Feature | Behaviour |
|---|---|
| Per-string outputs, Aux 1-6, Aux 8 (`routing-io.md` 2-3) | Untouched. They are taps ahead of the master bus, used for re-amping and stems, where natural instrument level is the point. |
| Aux 7 monitor | Carries post-master main (`LuthierEngine` step 10), so it is normalized, as the player hears it. |
| Master output meter, `LevelMeter`, header LED | Read `MasterBus` after the gain, so they show what is heard. The short-term LUFS readout sits near the target. |
| Kill switch, looper, session recorder, capture | These sit after the master bus in `processSlice`, so they see (and the looper and recorder record) the normalized level. Recorded layers are not renormalized later. |
| Backing track, metronome click, tune click | Mixed after the master bus, so they are not normalized and not measured. The guitar-to-backing balance becomes consistent across presets. |
| Jam mode band mix (parallel spec) | The same rule. The band must be mixed after `master.processBlock`, never into the engine ahead of it. It is not normalized and not in the reference render. Coordinate. |
| Preset browser previews (`preset-browser-previews.md` 4) | Previews are rendered at -18 LUFS. When normalization is **on**, `PreviewPlayer` adds `target + 18` dB. That is limited so the clip's stored true peak plus the offset stays at or below -1 dBTP (the sidecar must record `truePeakDbtp`; coordinate). So a preview sounds as loud as the loaded preset will. When it is **off**, previews behave exactly as their spec says. Preview renders themselves always run with normalization off. |
| Snapshots, snapshot morph, preset morph | See 3.2. |
| Rhythm engine, Tune Builder, techniques | Their output is normalized like playing. They never trigger calibration, and the reference render bypasses them (direct MIDI). Tune load prefetches (4.4). MIDI export and notation are unaffected. |
| Audio export and tune export (`AudioExporter`) | The offline instance receives the session state, normalization included. It renders deterministically (4.6). The exporter's own "normalise file" option is separate and applies to the file afterwards. |
| Workshop, spectrum delta, Tone Match | The spectrum-delta and Tone Match workers render or measure their own taps, with normalization off. Bench note in 5.4. |
| Randomise, Reset (header) | Leave normalization alone. They change the sound, which then recalibrates. Diagnostics "Reset all settings" turns the preference off. |
| Advanced ranges | Values outside the stock range are Config like any other. Free's clamped effective values are what gets measured (below). |
| Freeze, E-Bow, feedback | Performance. Not in the render. Their layers pass through the gain and the true-peak stage. |
| Multi-instance | Each instance has its own switch, target and gain. The LRU and the disk cache are shared. |
| Host automation | Config automation recalibrates after it rests (2.3.2). The switch itself is not automatable (7). |
| Telemetry (opt-in) | One usage boolean, `normalization_enabled`, per `updates-telemetry.md` 6. |

## 11. Editions

It is in **both Free and Pro, identical and unlimited**. It is a
fundamental comfort and safety feature, not a headline one
(`editions.md` 0.2 and 1), and its UI is not a Pro panel. Free's offline
calibration instance is itself Free, so a Pro preset opened in Free is
measured on its neutralised effective sound (`editions.md` 5.1.3) and
still lands on target. CI generates one factory table per edition (4.4).

## 12. Failure modes

| Failure | Response |
|---|---|
| Render throws, produces non-finite output, or exceeds 10 s | Hold the current gain, then the estimate (4.5). Readout "about…". Log to `ErrorLog` with the hash. Retry on the next configuration event. |
| Offline instance cannot be created (memory) | The same, plus one warning banner per session: "Normalization could not measure this sound." |
| Measured below -70 LUFS | `unmeasurable`: keep the current gain, and show the 5.2 text. |
| Gain outside -12..+24 dB | Clamp, and flag it in the readout and badge. The true-peak stage covers the boost. |
| Restored hash mismatch (IR missing or changed) | Recalibrate (6). A missing IR also takes the normal missing-IR banner path. |
| Disk cache unreadable or corrupt | Ignore the entry and re-render. Never a user-visible error. |
| Worker starved by CPU relief (`performance-budget.md` 8) | Realtime: the gain holds longer (still no audible fault). Non-realtime: the wait of 4.6. |
| Host changes sample rate | The normalizer re-prepares its glide and detector. The calibration is rate-independent (4.3), so it is kept. |

## 13. Performance budget

- **Audio thread:** `MasterBus` 0.15 -> **0.22 units** while
  normalization is active (one multiply per sample plus the true-peak
  FIR, about 96 MAC per stereo sample). It is unchanged while inactive.
  The change tracker adds one byte compare per changed parameter per
  block, inside the bridge's existing poll: well under 0.01. Zero
  allocations and zero locks in realtime.
- **Worker:** at most 1.5 s per render on a mid CPU, at low priority, and
  only after a configuration change. Nothing is rendered while the
  configuration is stable.
- **Memory:** it shares the preview service's offline instance. LRU
  512 x 64 B. Factory table about 50 KB.
- **Boot:** no work at instantiation unless a session restores
  normalization on. Then a cache lookup of under 1 ms, and verification
  in the background.

## 14. New and changed files

New: `DSP/Master/LoudnessNormalizer.h/.cpp`,
`DSP/Master/TruePeakDetector.h`, `DSP/Master/Bs1770Meter.h/.cpp`,
`DSP/Master/LoudnessRoles.h/.cpp`,
`Support/NormalizationCalibrator.h/.cpp`,
`Support/NormalizationPhrase.h/.cpp`, `UI/NormalizationBadge.h/.cpp`,
`Resources/NormalizationFactory.json`,
`Tests/NormalizationTests.cpp`,
`Tests/Golden/NormalizationOffHashes.json`.

Changed: `MasterBus`, `ParameterBridge` (role table, tracker,
`PerformanceWriteScope`), `LuthierAudioProcessor` (calibrator
ownership, `captureSoundState`, `restoreState`, session key),
`SnapshotBank::applyBlend` and `PresetMorph` (scope), `PresetManager`
(prefetch), `AudioPage`, `HeaderBar`, `EasyPanel`, `RoutingPanel`,
`WorkshopPanel`, `Overlays`, `AudioExporter` (`setNonRealtime`),
`Tools/RenderCli.cpp` (`--calibrate-factory`).

## 15. Tests

All tests are in `Source/Tests/NormalizationTests.cpp` (LuthierTests),
unless marked GUI (xvfb, alongside `EditorTests.cpp`) or COMBO (on the
`ComboHarness.h` harness). Renders are at 48 kHz, 256-sample blocks,
unless stated. "Loudness" means `Bs1770Meter` integrated loudness of the
main output over the `NormalizationPhrase` window.

1. **ON-01 Off by default.** In a fresh instance with no `UiPreferences`
   file, normalization is disabled, the badge is hidden, the readout
   says "Off", and `LoudnessNormalizer::isActive()` is false.
2. **ON-02 Bit-identical when off (COMBO, slow).** Before any code
   lands, the SHA-256 of the float main output for every factory preset
   x guitar type (36 x 26) x {`NormalizationPhrase`, combo `chord`} is
   generated with today's build and committed as
   `Tests/Golden/NormalizationOffHashes.json`. With normalization off,
   every hash matches. A state with no `normalization` key and one with
   `enabled:false` also render identically.
3. **ON-03 Every factory preset x guitar type within ±1 LU (COMBO,
   slow).** With normalization on at -18, for all 936 combinations, after
   calibration settles, the main-output loudness is -18 ±1.0 LU. A seeded
   10% sample repeats at -14 and -23 with the same tolerance. The CI gate
   also fails if any combination drifts by more than 0.5 LU from its
   factory-table value (the revision rule, 3.3).
4. **ON-04 Spread shrinks.** Over the 936 combinations and the combo
   `chord` phrase, the loudness spread (max - min) is at most 4 LU with
   normalization on. The spread with it off is reported as an artefact.
5. **ON-05 Dynamics preserved.** For 10 presets, the phrase at velocity
   40 vs 120: the dB difference with it on equals the difference with it
   off, ±0.1 dB. The applied gain is bit-identical between the two
   renders.
6. **ON-06 Input-independent gain.** 30 s of silence, then 30 s of
   dense playing, then 30 s of silence, with the configuration unchanged:
   the logged gain word never changes and the calibrator records zero
   requests.
7. **ON-07 Clamping.** A configuration that measures -50 LUFS gets
   +24.00 dB with `clampedHigh`. One at -3 LUFS gets -12.00 dB with
   `clampedLow`. The readout and badge texts match 5.2.
8. **ON-08 Unmeasurable.** `amp_master` at 0 gives `unmeasurable`, the
   gain unchanged, and the 5.2 text.
9. **ON-09 True-peak safety.** With it on at -14, for every factory
   preset x {phrase, `chord`, `fastRepeat`}, the 4x-oversampled true peak
   of the output is ≤ -1.0 dBTP + 0.1 dB, with `limiter_on` both on and
   off. With normalization off, the sample peak is ≤ -0.3 dBFS, as today.
10. **ON-10 Limiter transparency.** At -18, the loudness lost to the
    true-peak stage is ≤ 0.5 LU for at least 95% of the 936 combinations
    and ≤ 1.0 LU for all of them.
11. **ON-11 Toggle is a pure glide.** Toggle on during a sustained
    chord. Output divided sample-wise by the logged applied gain equals
    the off render within 1e-5. The gain curve is monotonic, reaches
    within 0.05 dB of the target at 300 ±11 ms, and no per-sample step
    exceeds the ideal linear-dB slope by more than 1%. The same holds
    when toggling off, and for a target change.
12. **ON-12 Calibration update glides.** Change `amp_model` mid-note.
    The gain holds until the result, then follows the ON-11 bounds, with
    no step.
13. **ON-13 Debounce.** 2 s of continuous `amp_gain` automation gives
    zero requests during the sweep and exactly one request 250 ms
    (±1 block) after the last change.
14. **ON-14 Performance writes do not recalibrate.** Sweeping
    `guitar_volume`, `whammy_position` or a wah slot position, a snapshot
    recall, and snapshot morph moves give zero requests, and the gain is
    bit-unchanged. `guitar_volume` 1.0 -> 0.5 lowers the output by the
    same dB (±0.1) with it on as with it off.
15. **ON-15 Preset morph.** At `preset_morph_position` 0.25 between
    presets with gains +3 and +11 dB, the applied gain is 5.00 dB.
16. **ON-16 Offline determinism.** A 60 s non-realtime session with 6
    preset loads, 3 guitar-type changes and `amp_model` automation is
    rendered 5 times. The worker gets a random 0-500 ms injected delay,
    and the caches are cold, warm and disk-only. Every output is
    bit-identical. The logged gain curve is identical at 64 and 1024
    sample blocks.
17. **ON-17 Realtime safety.** 5 min of realtime playback with 200
    configuration changes, 50 toggles and 20 target changes, with the
    heap-alloc and mutex traps on the audio thread: zero triggers. No
    sleep or wait on the audio thread when `isNonRealtime()` is false.
18. **ON-18 Session round trip.** Save a state with it on, at -14,
    calibrated. A new instance calls `setStateInformation`, and the first
    block's gain equals the stored `gainCentiDb` exactly (no glide, no
    "Measuring"). The first 10 s of output are bit-identical to the
    original instance's.
19. **ON-19 Legacy and preference.** With `defaultEnabled` true: a
    restored state without the key loads off. A fresh instance (no
    restore) loads on at the preferred target.
20. **ON-20 Not preset data.** Save a preset with it on. The preset JSON
    has no `normalization` key. Loading any of the 36 factory presets
    leaves the enabled state and the target unchanged.
21. **ON-21 Undo and A/B do not touch it.** Toggle on, move `amp_gain`,
    then undo: it is still on. A/B flip: still on. Undo stack depth is
    unchanged by the toggle and by target changes.
22. **ON-22 Aux and per-string untouched.** With Layout D and it on
    (+10 dB configuration): Aux 1-6, Aux 8 and all 12 per-string buffers
    are bit-identical to off. Aux 7 equals the post-master main.
23. **ON-23 Meter shows normalized level.** During the phrase, the
    header meter's LUFS reading averages the target ±1.5 LU.
24. **ON-24 Previews.** On at -14: the `PreviewPlayer` gain is +4 dB over
    off, or less when the clip's true peak would exceed -1 dBTP. Off:
    unchanged. Preview renders report normalization disabled.
25. **ON-25 No recursion.** The calibration offline instance reports
    `setCalibrationRenderMode (true)`, creates no calibrator, and the
    live-instance count never exceeds 1 + 1 during calibration.
26. **ON-26 Roles and hash.** Every APVTS parameter id has an explicit
    `LoudnessRole` (the test fails on any unlisted id). Changing each
    Config parameter by one quantisation step changes the hash. Changing
    any Performance or Mix parameter does not.
27. **ON-27 Cache.** A repeated hash is served without a render (render
    counter unchanged) within 5 ms. Every factory preset at its own
    guitar is a factory-table hit. A corrupt disk entry is ignored and
    re-rendered.
28. **ON-28 Preset load with a cached gain.** Switching from a +2 dB to
    a +12 dB factory preset: the first 300 ms of the new preset are
    within ±1 LU of its steady state, so there is no glide from the old
    gain.
29. **ON-29 Failure path.** A forced render failure gives the held gain,
    then the estimate, `flags.estimate`, the "about" readout, and one
    `ErrorLog` entry. In non-realtime mode with a 10 s timeout the
    estimate is used, and the render still completes.
30. **ON-30 Sample rates.** For 10 presets at 44.1, 48 and 96 kHz live
    rates, the loudness is on target ±1 LU, and the same hash gives the
    same gain.
31. **ON-31 Combinations (COMBO).** With it on, crossed with Slide Mode,
    the bass family, the rhythm engine, tune playback across 3 presets
    (no "Measuring" state at section changes, thanks to prefetch),
    Freeze, kill switch and feedback: output finite, true peak
    ≤ -1.0 dBTP, gains as specified.
32. **ON-32 Free edition.** In the Free CI configuration, a Pro preset
    lands on target ±1 LU on the Free output, using the Free factory
    table.
33. **ON-33 Performance.** `MasterBus` active ≤ 0.22 units, inactive
    ≤ 0.15 × 1.10. A Heavy-preset calibration render takes ≤ 1.5 s of
    wall time on the CI reference machine.
34. **ON-34 GUI: Options (xvfb).** Options -> AUDIO shows the switch,
    target, readout and caption. Turning it on posts banner
    `normalization.on` with the 5.3 text and both actions. "Don't show
    again" suppresses the next one. The caption is visible while on and
    hidden while off. The target is disabled while off. The readout
    matches `getNormalizationStatus()` within 0.05 dB.
35. **ON-35 GUI: badge (xvfb).** The badge is visible only while on, in
    both Easy and Advanced mode, at window widths of 1000, 1280 and
    1920. Clicking it opens Options -> AUDIO with the switch focused.
36. **ON-36 Accessibility (xvfb).** The switch, target, readout and
    badge have the accessible names and description of section 9. A Tab
    walk reaches them in order. Gain-change announcements are rate-
    limited to one per 2 s. The shortcut command toggles the switch and
    posts the banner.
