# CPU QUALITY MODES SPEC

One **CPU quality** setting: **High, Medium, Low, Auto**. It trades a
small, measured amount of sound detail for CPU headroom. It never changes
pitch, timing, latency or what a preset contains. **Low also turns off
every UI animation** through one central switch, `AnimationPolicy`, which
every animated component consults and which Reduced motion now also uses.

Added 2026-09-24 at the product owner's request. **Supersedes
`performance-budget.md` 8** (CPU relief, never implemented); section 7
here replaces it. Additive to `gui-integration.md`; moves nothing.

## 0. Ground rules

1. **Presets sound the same everywhere.** The mode is never stored in a
   preset, snapshot, guitar, tune or setlist. It belongs to the machine
   and the person, not the sound.
2. **Never change pitch or timing.** No reduction moves a fundamental by
   more than 0.1 cent, moves an event by a sample, or changes the latency
   reported to the host.
3. **Small, bounded audible cost.** Every reduction has a stated cost and
   a test bounding it; every factory preset stays within 0.5 dB
   integrated loudness of High (CQ-12).
4. **Final renders are High.** "Always render offline at High" defaults
   on; Luthier's own exports and previews are always High.
5. **Identity rules hold.** Coupling is never off (engine identity rule
   7), the body is never bypassed, per-string outputs are never silenced.
6. **Real-time safe.** A switch never allocates, locks or does I/O on the
   audio thread. Every alternative (truncated IR, sorted modal set, second
   oversampler) is prepared on the message thread; the audio thread only
   selects and crossfades.

## 1. User stories

- *Laptop player (Low CPU class, qa-polish 1):* "Metal Chug crackles. I
  pick Low; it plays cleanly and sounds almost the same."
- *Producer, eight instances:* "Background instances run at Medium via
  the per-instance override; the lead at High. The bounce is High anyway."
- *Gig player:* "Auto steps down and tells me once, instead of glitching."
- *Motion-sensitive or battery-conscious user:* "Low stops everything
  that moves on screen."
- *Preset designer:* "Nobody hears a Low version of my preset by accident."

## 2. Exact behaviour

### 2.1 Levels

`QualityProfile` (Source/Support/QualityProfile.h) is the single
definition; engine and UI read only from it.

| Aspect | High | Medium | Low | Audible cost (bound) |
|---|---|---|---|---|
| Amp oversampling cap | parameter (default 4x) | 2x | 2x | More aliasing on bright high gain: ≤ -60 dBc at 2x (CQ-15) |
| Drive-pedal oversampling cap | parameter | 2x | 1x | Soft-clip aliasing ≤ -50 dBc at defaults (CQ-15); a pedal type that fails gets a Low cap of 2x |
| Dispersion allpass stages (`StringEngine`) | 8 | 4 when f0 ≥ 330 Hz | 4 when f0 ≥ 165 Hz, 2 when f0 ≥ 440 Hz | Partials ≥ 10 move ≤ 3 cents; the fundamental is held by `filterDelayCompensation()`, which already counts `activeDispersionStages` |
| Body convolution IR (`BodyEngine`) | full (≤ 4 s) | 1.5 s | 0.75 s | Tail only; -40 dB tail rule (2.3) |
| Body modal bank | all (≤ 48) | 32 | 20 | Lowest-energy modes dropped; the 8 lowest-frequency modes always kept; body null ≥ 30 dB (CQ-16) |
| Cabinet user / tone-match IRs (`CabinetEngine`) | full | 250 ms | 120 ms | Same tail rule; procedural speaker model unchanged |
| Room early-reflection taps (`RoomEngine::kNumTaps`) | 16 | 16 | 8 loudest, energy-compensated | Fine room structure; FDN tail unchanged |
| `NoiseEngine` pools | full | full | halved (`setDegraded(true)`) | Only with > 8 simultaneous events per class; quietest stolen |
| Mod-matrix control interval | 128 | 128 | 256 samples | ~5 ms staircase before destination smoothing; stays 128 while any LFO runs above 20 Hz |
| Idle-string sleep | off | on | on | None (below -100 dBFS, 2.4) |
| Ring-out truncation (released strings) | off | at -80 dB re note peak | at -60 dB, max 8 ringing out | Last of the tail; held / E-Bow / feedback strings exempt |
| Coupling, pickups, circuit, tone stack, other pedals, feedback, E-Bow, freeze, body coupling bank, noise floor, scrape, per-string outputs | unchanged | unchanged | unchanged | None |
| UI motion (`AnimationPolicy`, 6) | Full | Limited | Off | Display only |
| Live readouts (meters, heatmap, playheads) | requested | requested | ≤ 10 Hz, stepped | Display only |

Left alone on purpose: coupling (0.4 units, identity rule), the realism
modules (≤ 0.15 units each, they are the character), the FDN reverb
pedals (≤ 0.4). Cutting them saves little and is audible.

### 2.2 Oversampling, the parameter and latency

- `effective = min(nominal, cap(level, stage))`, with `nominal =
  min(oversample parameter, sample-rate cap of performance-budget 7)`.
  1x stays 1x. The parameter and its automation still work below the cap.
- **Latency never follows the mode.** `AmpEngine` and `DrivePedalBase`
  get `setOversamplingFactor (int effective, int nominal)` and a
  `LatencyPad` (integer delay, preallocated 8 samples) of
  `Oversampler::latencyFor(nominal) - latencyFor(effective)`, where
  `latencyFor()` is a new static form of the table in
  `Oversampler::getLatencySamples()`. `getLatencySamples()` returns the
  nominal figure, so `LuthierEngine::getLatencySamples()` is
  level-independent and `LuthierAudioProcessor::updateLatency()` never
  calls `setLatencySamples` because of a switch.
- **Click-free factor change.** Each stage holds two `Oversampler`s and
  two copies of its oversampled-rate filter state. The new path starts
  from a copy of the old state (coefficients recomputed for the new rate),
  primed from a 32-sample input history ring; both run for 10 ms (480
  samples at 48 kHz, scaled with rate) under a linear crossfade. This also
  removes the click today's `Oversampler::setFactor()` → `reset()` makes
  when the user changes the parameter.

### 2.3 Convolution truncation

On any IR load (body, cab A/B, tone-match) the message thread builds one
variant per level whose cap is shorter than the IR: normalise the full IR
once; cut at the cap, moved later until the energy after the cut is
≤ -40 dB of the total; 50 ms raised-cosine fade; no renormalisation.
Each variant is its own `juce::dsp::Convolution` with the same fixed
partition `Latency`, installed via `ConvolutionInstaller`, so latency is
identical; variants not shorter than the IR share the full instance. The
audio thread runs the variant for the effective level and, on a switch,
runs old and new together for 20 ms under a linear crossfade. Memory
≤ +12 MB per instance (CQ-31).

### 2.4 String savings

- **Dispersion cap** latches in `StringEngine::excite()` only: a ringing
  note keeps its stage count until re-excited.
- **Idle-string sleep** (Medium, Low): when `getLevel()` < 1e-5, the
  excitation is inactive and |coupling input| < 1e-7 for 100 ms, the delay
  line is cleared once and `processSample` is skipped (output 0). It wakes
  on the first sample with coupling or excitation input > 1e-7, or on
  `excite()` / `touch()`. It still sends (0) and receives in the coupling
  matrix, so sympathetic ring survives.
- **Ring-out truncation** applies to `Damping::Released` strings and open
  strings ringing after note-off: past the stated drop below the note's
  peak, a 30 ms fade to sleep. Under Low, beyond 8 strings ringing out the
  quietest fades (only matters for 12-string, harp tunings or sustain
  pedal). Held, sustain-pedal-held, E-Bow and feedback strings are exempt.

### 2.5 Switching

The audio thread reads one atomic at the top of
`LuthierEngine::processBlock` and calls `LuthierEngine::applyQuality
(const QualityProfile&)`, which only sets integers and flags and starts
crossfades. A **hard switch** (no crossfade) happens only in
`prepareToPlay`, `reset()`, or after 50 ms of engine output below
-90 dBFS. Dropped modal modes and room taps ramp to 0 over 20 ms before
being skipped; noise events already running finish.

### 2.6 Offline rendering

- `effective = (isNonRealtime() && offlineAtHigh) ? High : live level`.
  **Auto offline is always High**, whatever the option: load is
  meaningless offline and renders must be deterministic.
- A change of `isNonRealtime()` is caught in `prepareToPlay`, in an
  overridden `LuthierAudioProcessor::setNonRealtime` (stores an atomic)
  and at the top of `processBlock`, and is applied as a hard switch, so
  renders are bit-identical whatever the live level was (CQ-14).
- `AudioExporter`'s second instance must call `setNonRealtime (true)`
  before `prepareToPlay` (it does not today). Preset-browser previews,
  audition phrases and the Workshop `SpectrumDelta` worker render at High.
- With the option off, offline renders use the fixed live level.

### 2.7 Auto

`CpuLoadMonitor` measures **Luthier's own share only** (`processBlock`
wall time ÷ block duration); the host's total load is not visible to a
plugin, and the UI says so.

| Rule | Value |
|---|---|
| Step down one level | 2 s mean > 65 %, or 2 s p95 > 85 % |
| Minimum dwell between steps | 3 s |
| Step up one level | 20 s mean < 30 % **and** predicted load at the higher level < 50 %, using the ratio measured at that level this session, else 1.25 (High/Medium) and 1.30 (Medium/Low) |
| No step up within | 30 s of a step down |
| Anti-oscillation | 3 step-downs within 10 min → hold for the session; footer shows "AUTO (held)"; choosing Auto again clears it |
| Start level | `auto_last_level` from `performance.json` (default High) |
| Decision | Message-thread timer, 10 Hz, in `QualityController` |
| Notice | Step down: banner (gui-integration 15, no action, 5 s): "Luthier's CPU load is high, so Auto lowered quality to Medium. Options → Audio." At most one per 60 s; switchable off. Step up: silent except the badge. Both: polite screen-reader announcement |

## 3. Data model and files

**Global preference**: new `Documents/Luthier/config/performance.json`,
beside `ui.json`, written temp-and-rename (file-formats 13):

```json
{ "schema": 1, "magic": "luthier.performance",
  "quality": "high",            // "high" | "medium" | "low" | "auto"
  "offline_at_high": true, "auto_notify": true,
  "auto_last_level": "high", "emergency_string_drop": true }
```

Missing or corrupt → these defaults, rewritten on next change, never an
error. Not `UiPreferences`: that store is "about the window"; this is
about the machine. `PerformanceSettings` is the process singleton and
broadcasts changes to every instance in the process; other processes
re-read the file in `prepareToPlay` and on editor open.

**Per-instance override**: `UiState::qualityOverride` ∈ {`global`
(default), `high`, `medium`, `low`, `auto`}. Saved in the host session
with uiState, never in presets (state-model 0.6). It lets one project
keep background instances cheap; with `offline_at_high` on it cannot
change a bounce on another machine.

**Presets and snapshots**: no field; `PresetManager` neither writes nor
reads it.

**Parameters: none added.** Deliberately not a host parameter: it would
be serialised with APVTS preset state (breaking 0.1), automating it is not
musical, and it would let automation alter renders. The `oversample`
parameter keeps its id, range, default and position.

## 4. Engine and processing design

New files:
- `Source/Support/PerformanceSettings.{h,cpp}`: global store, file I/O
  (message thread).
- `Source/Support/QualityProfile.h`: `enum class QualityLevel { High,
  Medium, Low }`, `struct QualityProfile`, constexpr `forLevel()`.
- `Source/Support/CpuLoadMonitor.h`: audio-thread ring (1024 block loads)
  with O(1) running sums for 200 ms / 2 s / 20 s means and a 64-bin
  histogram for p95; atomic read-outs. Becomes the source of
  gui-engine-dataflow 11 (`LuthierEngine::cpuEstimate` stays for the
  debug window).
- `Source/Support/QualityController.{h,cpp}`: owned by
  `LuthierAudioProcessor`; resolves global, override, Auto and
  non-realtime into `std::atomic<int> effectiveLevel`; runs Auto; feeds
  `AnimationPolicy`; injectable clock and load for tests.
- `Source/UI/AnimationPolicy.{h,cpp}` (6), `Source/UI/QualityBadge.{h,cpp}` (5).

Insertion points:
- `LuthierAudioProcessor::processBlock` stamps `CpuLoadMonitor` around the
  whole block and reads `effectiveLevel`; `setNonRealtime` overridden;
  `prepareToPlay` re-resolves.
- `LuthierEngine::applyQuality` forwards to `amp.setOversamplingFactor
  (eff, nom)`; `preEffects` / `postEffects.setOversamplingFactor (effDrive,
  nom)` → `DrivePedalBase`; `StringEngine::setDispersionCap(...)` and
  `setSleepPolicy (bool, double ringOutDb)` plus the ring-out cap in the
  string loop (LuthierEngine.cpp ~2119); `body.setQualityLevel()`
  (variant select, `numActiveModes` cap over modes energy-sorted in the
  existing `stagedModes` build); `cabinet.setQualityLevel()`;
  `room.setTapCount (8 | 16)`; `noise.setDegraded (level == Low)`;
  `modMatrix.setControlIntervalMultiplier (1 | 2)` at the next tick.
- `ParameterBridge::applyStructural` (Parameters.cpp ~1725) passes the
  *nominal* factor; the engine derives the effective one, so the parameter
  and quality paths cannot fight.
- `Oversampler::latencyFor (int)` static added.
- `AccessibilitySettings::getAnimationMs` delegates to
  `AnimationPolicy::transitionMs`.

## 5. UI

**Canonical location: Options → AUDIO, QUALITY section** (`AudioPage`),
where oversampling already lives (GAPS A3) — the same kind of setting.
At page width ≥ 520 px (rows stack below):

```
QUALITY
CPU quality   ( Auto | High | Medium | Low )         <- radio pill group
This instance [ Use global setting (High) v ]         <- override combo
Now running: Medium (Auto)   [#####-----] 41 %        <- Luthier's own share, 4 Hz, stepped
[x] Always render offline at High
[x] Tell me when Auto changes quality
Oversampling  [ 4x v ]   Running at 2x while quality is Medium.
What each level changes >                              <- disclosure: table 2.1 in plain words
```

- The oversampling note shows only while capped; the same text is the
  tooltip of the Advanced column 3 Master oversampling choice.
- AppearancePage adds a muted line under Reduced motion while at Low:
  "Animations are off while CPU quality is Low."
- DIAGNOSTICS: the old relief 7 opt-out becomes "When Luthier runs out of
  CPU, drop the quietest string instead of glitching"
  (`emergency_string_drop`). "What's on the audio path" and the debug
  window (Overlays.cpp ~304) add the level, effective oversampling per
  stage, IR lengths, modal count and sleeping strings.

**Surfacing: footer badge** (gui-integration 12: "Right: CPU %, voice
count"), visible in Easy and Advanced. `QualityBadge` (132 × 16 px)
replaces the CPU text `LuthierAudioProcessorEditor::paint` draws today:

```
[ MED ] ▮▮▮▯▯ 41%   latency 133 smp
```

Label `HIGH` / `MED` / `LOW` / `AUTO·H` / `AUTO·M` / `AUTO·L`, secondary
accent when not High. Bar zones < 50 % normal, 50–80 % warning, > 80 %
error, plus a glyph for monochrome. Click / Enter / Space opens Options →
AUDIO focused on the group. Tooltip: "CPU quality: Medium. Luthier is
using 41 % of the audio time. Click to change." 4 Hz; "-" after 5 s stale.
The header is unchanged (its regions are full at 1280 px). The rebindable
shortcut table (accessibility 2) gains "Cycle CPU quality", unbound.

**Empty / error states**: before playback "Not playing yet" and "-";
held Auto: "Auto is holding at Low: load kept changing. Choose Auto again
to resume."

## 6. AnimationPolicy: one motion switch

A message-thread singleton with listeners. Inputs: Reduced motion
(`AccessibilitySettings`), the most restrictive effective level among
open editors in the process, and the relief level (7). Every animated
thing is classed `Decorative` (motion for feel), `Transition` (an ease
between states) or `LiveReadout` (a value that updates).

| Condition | Motion | Decorative | Transition | LiveReadout |
|---|---|---|---|---|
| High, no reduced motion, relief 0 | Full | requested (≤ 60 Hz) | requested (80 ms) | requested |
| Medium, or relief 1 | Limited | ≤ 30 Hz (animated strings forced to Low style) | requested | requested |
| Reduced motion | Off | none (static frame) | 0 ms | requested |
| Low, or relief ≥ 2 | Off | none | 0 ms | ≤ 10 Hz, stepped, no ballistics |

API: `getMotion()`, `mayAnimate(MotionClass)`, `frameRateHz(MotionClass,
int requested)`, `transitionMs(int)`, `setReliefLevel(int)`,
`addListener()`. `AnimationPolicy::Registration (juce::Component&,
MotionClass, const char* name)` is an RAII member putting the component in
a process-wide registry; `AnimationPolicy::notePaint (const Component&)`
at the top of `paint()` bumps its paint counter (one atomic increment,
always compiled). On `motionPolicyChanged()` components stop or retune
their `juce::Timer` or drop their `VBlankAttachment`; one with nothing to
show at Off has **no running timer**.

Registry minimum and behaviour at Off:

| Component | Class | At Off |
|---|---|---|
| `StringAnimator`, `GuitarBodyComponent`, `FretboardComponent` (animated-strings) | Decorative | Static overlay, fixed glow; repaint only when sounding strings / frets change |
| Chord name and piano-roll key fades (piano-roll-chord-display 4, 1) | Transition | No fades |
| Piano-roll scroll, `TunePanel` transport, `RhythmPanel` step, `LiveStrip` | LiveReadout | 10 Hz, stepped |
| `LevelMeter`, VU needle (visual-polish 4), `OutputLed`, `FeedbackLed` | LiveReadout | 10 Hz, no ballistics; clip LED latches, no pulse |
| Header LED pulse, MIDI Learn pulses (`LuthierKnob`, header), pill pulses | Decorative | Steady outline / colour |
| `AmpFacePanel` valve glow | Decorative | Static, from the drive *parameter* |
| ROOM card room light | Decorative | Static, from room size and wet |
| `DataStreamDisplay`, `NoiseEventStrip` | Decorative | Static counts |
| `BuzzHeatmap` | LiveReadout | 10 Hz |
| `ModSourceCard` LFO phase dot, mod-arc moving indicator (gui-integration 11.1) | Decorative | Static shape and range |
| Knob / slider tween, slide-bar ease, `TapPad` flash, notification slide-in, preset-browser hover, `BenchIllustration` highlight ease | Transition | Instant |
| `QualityBadge` | LiveReadout | 4 Hz |

Timers that only poll state (e.g. `HeaderBar` 6 Hz undo state,
`Notifications` auto-dismiss, `OptionsPage::refresh`) are exempt only via
an allow-list in `AnimationPolicy.cpp` with a one-line reason each.

## 7. Load governor (supersedes performance-budget 8)

Runs in every mode but is **display or emergency only**: sound is reduced
only by the chosen level or by Auto, so an explicit High is respected.
Old steps 1–2 → E1/E2; 3–4 (mod rate, noise pools) → part of Low, reached
via Auto; 5 (reverb taps) dropped as too audible; 6 (freeze shadow
audition) → E2; 7 → E3.

| Level | Enter | Action | Leave |
|---|---|---|---|
| E1 | 200 ms mean > 85 % | `AnimationPolicy` relief 1; data stream suspended | 2 s mean < 70 % |
| E2 | > 90 % for 200 ms in E1 | Relief 2 (motion Off); shadow `GuitarSpec` audition frozen | same |
| E3 | > 100 % for 200 ms | Audio thread fades the least-recently-excited ringing string over 10 ms; banner "CPU limit reached: a string was dropped." (first ever adds "Try CPU quality Auto or Low") | per event; opt-out `emergency_string_drop` |

E3 is decided on the audio thread from `CpuLoadMonitor`'s running sum, so
it works while the message thread is busy. Nothing runs while
`isNonRealtime()`. `StringAnimator::setReliefLevel` (animated-strings 2.6)
is now fed only through `AnimationPolicy`.

## 8. CPU targets

% of one core; performance-budget 0 conditions (mid class, 48 kHz / 128,
one instance, 60 s averages). These are **budgets pending measurement**:
CQ-13 prints the measured values, which replace this table in
`performance-budget.md` at the first mid-class CI run.

| Scenario (performance-budget 1) | High | Medium | Low | Low class (≈ 1.8× mid) at Low |
|---|---|---|---|---|
| Idle | 1.5 | 0.8 | 0.6 | 1.1 |
| Steady (Rock, 4 voices) | 8 | 6.8 | 5.6 | 10 |
| Realism heavy | 12 | 10.2 | 8.4 | 15 |
| Slide with feedback | 18 | 15.3 | 12.6 | 23 |
| Bass heavy | 15 | 12.8 | 10.5 | 19 |
| Heavy (Metal Chug) | 22 | 18.7 | 15.4 | 28 |

- **Hard, machine-independent gates**: Medium ≤ 0.85× High and Low ≤
  0.70× High per scenario in the same run; strict High > Medium > Low per
  factory preset (CQ-12).
- **Where it comes from** (Heavy): amp 2x ≈ -0.7, drive pedals ≈ -0.2,
  dispersion ≈ -0.6 (Medium) / -1.0 (Low), body ≈ -0.25, sleep and
  ring-out ≈ -1.0 or more, mod rate -0.07.
- **Message thread**: steady-state editor paint time at Low ≥ 60 % below
  High with animated strings on (CQ-23).
- **Switch**: ≤ 2× the affected stage for 10–20 ms; p99 block ≤ 1.3×
  steady.

## 9. Undo, accessibility, edition

**Undo**: not undoable; it is an option (action-and-undo 3.17), like the
palette. The per-instance override is uiState, also not undoable.

**Accessibility**: the quality control is a radio group named "CPU
quality" (role `group`, items `radioButton`), arrow keys move within it,
each item's description is its table 2.1 column in one sentence.
`QualityBadge` is a focusable button, last in the footer tab order. Auto
changes and E3 drops send polite announcements (≤ one per 5 s). All
strings go through the catalogue with named placeholders. Low never flips
the Reduced motion preference; they combine only in `AnimationPolicy`,
whose table 6 becomes accessibility 5's list.

**Edition**: both, identical, no Free limit. Editions 0.1 (same quality)
holds: the default is High in both and the mode is the user's own choice.
Gating CPU relief would penalise the low-end machines Free users are
likeliest to have. Add a row to editions 2.5.

## 10. Interactions

- **Presets, snapshots, setlists, A/B, Randomize, Reset (Ctrl+Shift+R)**:
  never read or write the mode. DIAGNOSTICS "Reset all settings" restores
  `performance.json` defaults.
- **Techniques, rhythm engine, tune builder, MIDI export, live MIDI out**:
  timing and content are level-independent (CQ-09, CQ-30); slides and
  bends keep their stage count within a note; E-Bow / feedback strings
  are exempt from truncation.
- **Host automation**: no new parameter; `oversample` automation works
  below the cap; MIDI Learn cannot target the mode.
- **Workshop**: shadow audition plays at the live level; `SpectrumDelta`
  is always High so deltas stay comparable.
- **Tone match**: captured IRs are stored full length; truncation applies
  at playback only.
- **Routing**: every output carries signal at every level; per-output
  latency (routing-io 7) unchanged via the pad.
- **Sample rate > 96 kHz**: part of *nominal*, so latency changes only in
  `prepareToPlay`, as today.
- **Easy mode**: full access via the footer badge and Options.
- **Multiple instances**: share the global setting; each has its own
  override and Auto.
- **Golden renders (qa-polish 2)**: offline, so High; must pass at any
  live level (CQ-32).

## 11. Failure modes

| Failure | Behaviour |
|---|---|
| `performance.json` missing / corrupt | Defaults (High), rewritten on next change, debug log |
| Truncated IR build fails (memory) | That level uses the full IR; noted in Diagnostics; no banner |
| Host bounces in real time without `setNonRealtime` | Render uses the live level; help text says so; `AudioExporter` unaffected |
| Host flips non-realtime while audio flows | Crossfade; a repeat render from the same state is still deterministic |
| Auto oscillates | Hold rule (2.7) |
| Message thread blocked | Auto, E1 and E2 pause; E3 still works |
| A component forgets to register | CQ-22 fails the build |

## 12. Tests

All are in `LuthierTests`. GUI tests run under xvfb in the style of
`Source/Tests/EditorTests.cpp`; preset matrices use `ComboHarness.h`
`Rig`. The level is forced with `QualityController::forceLevelForTesting`;
Auto uses an injected clock and load feed.

1. **CQ-01 Profile table.** `QualityProfile::forLevel` equals table 2.1
   field for field; a source scan finds no literal caps outside
   `QualityProfile.h`.
2. **CQ-02 Settings file.** Every field round-trips; a corrupt or missing
   file gives the defaults; writes are temp-and-rename.
3. **CQ-03 Not in presets.** A preset saved at Low has no quality key;
   loading any factory preset leaves the level unchanged; the APVTS
   parameter count and order are unchanged.
4. **CQ-04 Override.** `qualityOverride` round-trips through
   `getStateInformation`; "global" follows global changes, a fixed
   override ignores them.
5. **CQ-05 Oversampling cap.** For parameter {1, 2, 4, 8}x × every level,
   the effective factor is min(nominal, cap); 1x stays 1x.
6. **CQ-06 Constant latency.** `getLatencySamples()` equals the High value
   for every level and parameter; 50 switches during playback cause zero
   `setLatencySamples` calls.
7. **CQ-07 Latency accuracy.** At each level a delta impulse through a
   high-gain preset arrives within 1 sample of the reported latency, on
   the main output and every aux.
8. **CQ-08 Pitch.** E2, A2, E3, E4, E5 on every string: the fundamental
   is within 0.1 cent of High at each level, partial 10 within 3 cents.
9. **CQ-09 Timing.** A 64-event phrase on a clean DI preset has
   sample-identical onsets across levels.
10. **CQ-10 Note-boundary dispersion.** After a mid-note switch,
    `activeDispersionStages` is unchanged until the next `excite()`.
11. **CQ-11 Click-free switching.** A sustained high-gain chord cycles
    High → Low → Medium → High every 250 ms, 40 times. Around each switch
    the max second difference is ≤ 1.5× its max over the preceding
    100 ms; output is finite; no block exceeds 1.3× the steady p99.
12. **CQ-12 Factory preset matrix.** Every factory preset × {High,
    Medium, Low} renders a chord and a strummed phrase at 48 kHz / 128:
    finite, peak ≤ 0 dBFS, BS.1770 integrated loudness within 0.5 dB of
    High, median CPU over 5 runs strictly High > Medium > Low. Summed over
    all presets, Medium ≤ 0.85× High and Low ≤ 0.70× High. A mid-render
    level switch passes the CQ-11 click criterion.
13. **CQ-13 Scenario budgets.** Prints the section 8 table as measured.
    Ratio gates are enforced everywhere; absolute values (≤ budget ×
    1.10) only on a runner tagged mid-class.
14. **CQ-14 Offline at High.** Live Low then `setNonRealtime(true)`
    renders bit-identical to a render after live High. With the option
    off the render equals the Low render. Auto offline equals High either
    way.
15. **CQ-15 Aliasing bounds.** 1 kHz sine: the amp at gain 10 at 2x has
    alias products ≤ -60 dBc; each drive pedal at defaults at 1x has
    ≤ -50 dBc. A pedal that fails must have a Low cap of 2x in the table.
16. **CQ-16 Body and IR.** Truncation obeys the -40 dB tail rule; the
    modal cap keeps the 8 lowest modes; the body-response null against
    High is ≥ 30 dB; truncated variants have the full IR's latency.
17. **CQ-17 Auto.** Steps down after 2 s at a 70 % mean and not before;
    the p95 trigger works alone; 3 s dwell between steps; steps up only
    after 20 s below 30 % with a passing prediction and never within 30 s
    of a down-step; holds after 3 downs in 10 min; starts at
    `auto_last_level`.
18. **CQ-18 Notification.** One banner per down-step, at most one per
    60 s; none on an up-step; none with `auto_notify` off; an
    announcement every time.
19. **CQ-19 Governor.** E1, E2 and E3 enter and leave at the stated
    thresholds. E3 fades one string over 10 ms with its banner, is
    disabled by the opt-out, and fires with a blocked message thread. No
    governor action while non-realtime.
20. **CQ-20 Real-time safety.** 60 s of playback switching level every
    100 ms, with Auto and the governor active: zero allocation-trap and
    lock-trap hits (performance-budget 10).
21. **CQ-21 Policy truth table.** Every combination of reduced motion ×
    level × relief {0, 1, 2} gives the Motion, rates and `transitionMs`
    of table 6; `AccessibilitySettings::getAnimationMs` returns
    `transitionMs`.
22. **CQ-22 Registry completeness.** A source scan of `Source/UI/**` for
    `startTimer`, `startTimerHz`, `VBlankAttachment`, `ComponentAnimator`
    and `juce::Animator`: every hit's class holds an
    `AnimationPolicy::Registration` or is on the poll-only allow-list. The
    runtime registry of fully built Advanced and Easy editors contains
    every class in the section 6 table that exists in the build.
23. **CQ-23 Low means no animation repaints** (GUI). Advanced mode with
    every column 4 tab visited, then Easy mode, animated strings and chord
    names on, a chord ringing under the sustain pedal. At Low, over 2 s:
    every Decorative and Transition registrant paints 0 times and has no
    running timer or `VBlankAttachment`; every LiveReadout paints ≤ 21
    times. Control: at High at least one Decorative registrant paints more
    than 30 times. Editor paint time at Low is ≥ 60 % below High.
24. **CQ-24 Transitions at Low.** A programmatic knob change paints its
    target on the first paint; MIDI Learn arm shows a steady outline with
    0 repaints over 2 s; the chord name has no intermediate opacity; a
    notification appears in one frame.
25. **CQ-25 Reduced motion independence.** Reduced motion at High turns
    Decorative off and keeps LiveReadout at the requested rate. Low leaves
    the Reduced motion toggle unchecked.
26. **CQ-26 UI** (Easy and Advanced). The AUDIO page shows the 4-item
    radio group, override combo, two toggles and status line; the
    oversampling note appears only when capped; the badge text is right in
    all 6 states; clicking the badge opens AUDIO with focus in the group;
    the Low note shows on AppearancePage.
27. **CQ-27 Accessibility.** Arrow keys cycle the group; screen-reader
    names and descriptions are present; the badge is last in the footer
    tab order; announcements are rate-limited; "Cycle CPU quality" can be
    bound.
28. **CQ-28 Multi-instance.** With two processors, a global change reaches
    both and an instance with an override is unaffected.
29. **CQ-29 Edition.** The Free configuration passes CQ-01 to CQ-28
    unchanged.
30. **CQ-30 Combinations.** At Low with slide, feedback 30 % and E-Bow,
    driven strings are never truncated or put to sleep. Per-string outputs
    are non-silent at every level. Snapshot recall, tune playback and
    rhythm-engine output are sample-identical in timing, and MIDI out bytes
    are identical across levels. With sleep on, an open unison string's
    sympathetic resonance is within 1 dB of High.
31. **CQ-31 Memory.** IR variants add ≤ 12 MB RSS per instance with every
    IR slot filled at 4 s.
32. **CQ-32 Golden renders.** The qa-polish 2 golden suite passes with the
    live level at Low and at Auto.

Coordinator follow-ups (not edited by this spec): `gui-integration.md`
(QUALITY rows in 5 AUDIO, badge in 12, a row in 19);
`performance-budget.md` (mark 8 superseded, add the section 8 table);
`file-formats.md` (`performance.json`); `state-model.md`
(`qualityOverride` in uiState); `accessibility.md` 5 (point to
`AnimationPolicy`); `animated-strings.md` 2.6 and
`piano-roll-chord-display.md` 4 (read `AnimationPolicy`, not
`isReducedMotion()`); `gui-engine-dataflow.md` 11 (source is
`CpuLoadMonitor`); `editions.md` 2.5 (add the row); `host-integration.md`
(`setNonRealtime` handling).
