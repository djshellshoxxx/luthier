# JAM MODE SPEC: a synthesized backing band that follows your chords

A drummer and a bass player who listen to the chords the guitarist plays
(or the Tune Builder's progression) and play along, live and in time.
Tempo comes from the host transport, or from the plugin's own clock and
tap tempo. Every sound is synthesized: a small modal drum kit and a
waveguide bass string. No samples.

Added 2026-09-24. It is additive to `gui-integration.md`. It adds one
Column 4 tab, one group in the Easy rhythm strip, a Live Strip pill and
two aux buses at the end. It moves nothing already placed. Test prefix
**JM-**.

## 0. Ground rules

1. **No samples.** The drums use modal and stochastic-particle synthesis
   (section 5). The bass is a `StringEngine` waveguide (6). Count-in
   clicks are stick hits from the kit synth.
2. **The band belongs to the processor, not the guitar.** `JamEngine`
   sits beside `Metronome` and `Looper` in `LuthierAudioProcessor`. It
   never passes through the guitar's body, pickup, amp or cab, and it
   never writes into the guitar's event queue. The Tune Builder stays a
   MIDI writer (INDEX global rules). Jam only *reads* the tune's chords.
3. **Musical, not instant.** The bass changes chord on a musical
   boundary (3.3), the way a bassist does. The drums never wait for a
   chord.
4. **Real-time safe.** All per-block storage is allocated in
   `prepareToPlay`. Styles and chord maps cross threads by atomic
   pointer swap (ui-wiring 4.3). The audio thread uses no locks, no
   allocation and no strings.
5. **Deterministic.** The same MIDI, transport, parameters and seed give
   bit-identical audio at any block size, online or offline. All
   randomness is `RtRandom`, seeded by (preset jam seed, bar index,
   lane).
6. **Aligned with the guitar.** Jam reports no latency of its own. It
   schedules every event `L` samples after its musical time, where
   `L = getLatencySamples()`. After host delay compensation, band and
   guitar land together on the grid.
7. **Free when off.** With `jam_enabled` off, `JamEngine` is not called.
   While armed but stopped, only the conductor runs (<= 0.02 units).

## 1. User stories

- *Practising alone:* "I strum Am-F-C-G. Drums and bass come in and
  change when I change."
- *Standalone:* "I play one chord and the band starts on it. When I put
  the guitar down, it stops."
- *Stage:* "I tap four beats on a footswitch and the band enters at that
  tempo. Another switch calls a fill."
- *Songwriting:* "My tune plays, and the bass lands exactly on every
  change, with a walk-up into the chorus."
- *DAW:* "The band locks to my session. I route drums and bass to their
  own tracks and drag the last 8 bars out as MIDI."

## 2. Transport behaviour

The engine has five states: **Off**, **Armed** (enabled, waiting for its
start), **Counting**, **Playing** and **Ending**.

### 2.1 Start (`jam_start_mode`)

| Mode | Starts when | Clock |
|---|---|---|
| Auto (default) | the host transport starts; with no host transport running, the first note | host if playing, else own |
| Host Transport | the host transport starts, and only then | host ppq, bar start, tempo, meter |
| First Note | first note-on (velocity >= 20) while Armed; beat 1 = that note's sample | own clock |
| Count-In | START pressed; `jam_count_in_bars` of stick clicks, then beat 1 | own clock |
| Tap In | the 4th TapTempo tap (header T, Live Strip pad, learned footswitch); taps set tempo *and* phase; beat 1 = one beat after tap 4 | own clock |

START (the button, `J`, or `jam_play` rising) starts the band in every
mode except Host Transport. It counts in when the mode is Count-In, and
otherwise starts on the next own-clock beat. With the host playing, the
band joins the host grid at the next bar line.

### 2.2 Stop

- **Host stops** (Auto, Host Transport): the ending plays, or the band
  cuts if `jam_ending` is off. The engine goes back to Armed.
- **STOP** (`J`, or `jam_play` falling): the ending plays on the next
  downbeat. A second STOP during the ending cuts with a 20 ms fade.
- **Stop when I stop playing** (`jam_stop_on_silence`): if no note-on
  arrives for `jam_silence_bars` whole bars, the ending plays on the next
  downbeat. Held or sustained notes do not count as playing. Looper
  playback MIDI does. In Auto and First Note modes, the next note starts
  the band again. The rule does not apply while the band follows the
  host transport, because the DAW owns start and stop.
- **Ending:** drums play the style's ending (crash and kick on the
  downbeat, choked after the style's ring time). The bass holds the root
  for one beat, then damps it.
- **Panic:** a 5 ms choke of every voice, `jam_play` set off, back to
  Armed.

### 2.3 Tempo and meter

Tempo priority:
1. The host tempo while the host plays.
2. The tune's own clock while a tune plays and the host is stopped.
3. `LuthierAudioProcessor::getEffectiveTempo()`: tap, then the last host
   tempo, then 120.

A tap while the band plays on its own clock re-tempos at the next bar.

Meter comes from the host (`PositionInfo::getTimeSignature`), else from
the tune, else 4/4. Every style has 4/4. Country and Ballad also have
3/4, and Ballad has 6/8. For any other meter the band plays a generic
bar: kick on 1, hat on every beat, snare on the last beat, bass root on 1.
Below 50 bpm a style plays its double-time bar, and above 220 bpm its
half-time bar.

While the band's own clock drives, the processor passes its ppq and
tempo to `LuthierEngine::setTransportPosition` / `setTempoBpm`, as it
already does for the tune's clock. The RhythmEngine then strums on the
band's grid.

## 3. Following the chords

### 3.1 Sources (`jam_chord_source`: Auto, Live, Tune)

- **Live:** `JamChordFollower` owns its own `ChordDetector` (same class,
  templates, burst window and `kConfidenceFloor` as the rhythm engine).
  It is fed the block's MIDI after MIDI Learn, after the live consumers
  and after the tune chord-channel merge in `processBlock`, plus the
  looper's playback MIDI (11). While `RhythmEngine::isDriving()` is true,
  Jam uses `getCurrentChord()` instead, so strum and band always agree.
- **Tune:** the playing tune's chord map (3.4).
- **Auto** (default): Tune while `TunePlayer::isPlaying()`, or while the
  practice PROG looper plays a progression. Otherwise Live.

Only a detection with >= 2 pitch classes (power chords count) at or
above the confidence floor changes the chord. Single notes are melody.
An Unknown detection keeps the old chord. Before the first chord, the
bass rests and the status reads "Waiting for a chord". Slash chords
(`ChordSymbol::isSlash()`) make `R` tokens play the bass note.

Drums ignore harmony. Bass tokens resolve against the chord's
`ChordTemplate::intervalMask`:
- `R`: the root.
- `3`: the template's third, or R if the template has no third.
- `5`: the fifth (b5 or #5 if the template has one).
- `7`: the template's seventh, or the octave if it has none.
- `8`: the octave.

### 3.2 Reaction latency (live chords)

A chord is detected when the burst window closes (<= 30 ms after its
first note). The bass changes at the next quantum Q of `jam_follow`:

| Follow | Q | Default for |
|---|---|---|
| Tight | next 8th | Funk, Metal, EDM |
| Natural | next beat | most styles |
| Relaxed | next half-bar | Reggae, Ballad |
| Bar | next bar line | Jazz Swing |

**Grace window.** If the chord's first note falls within
G = min(90 ms, one 16th) *after* a quantum boundary, the bass changes at
once, at the detection sample. A bassist who hears the change on the
beat plays it slightly late rather than a whole beat late. At every
change, the old note is damped (`Damping::Released`) and the new one
plucked.

### 3.3 Anticipation

When the next chord is known ahead of time, the bass changes exactly on
time, and a style's `A` token plays an approach note on the last 8th
before the change.
- **Tune:** `TuneTimeline::build` also emits a `JamChordMap`, a sorted
  array of at most 1024 entries `{ppq, root, bass, templateIndex,
  sectionIndex}`. It is handed to `JamEngine::setChordMap` by atomic
  swap, and freed on the message thread. A section change schedules a
  fill into the section and applies the section's jam hint (11).
- **Prediction** (`jam_predict`, Live only): the follower keeps the last
  32 bar-aligned changes. Once a cycle of 1, 2, 4, 8 or 16 bars has
  repeated twice in full (same chords, same beat, within one Q), the
  next change counts as anticipated. A contradicting live chord is
  corrected at the next Q, and prediction stays off until two more clean
  cycles. The status line shows predicted chords in italics.

## 4. Grooves

### 4.1 Factory styles

| # | Style | Grid | Kit | Bass voice | Follow |
|---|---|---|---|---|---|
| 0 | Rock | 16 | Studio | Pick | Natural |
| 1 | Pop | 16 | Studio | Finger | Natural |
| 2 | Funk | 16, swing 8 % | Studio | Finger + ghosts | Tight |
| 3 | Blues Shuffle | 12 (triplet) | Vintage | Finger | Natural |
| 4 | Country | 16, + 3/4 | Vintage | Pick | Natural |
| 5 | Metal | 16 | Arena | Muted Pick | Tight |
| 6 | Reggae | 16, swing 12 % | Vintage | Finger | Relaxed |
| 7 | Jazz Swing | 12 (triplet) | Jazz | Upright, walking | Bar |
| 8 | Ballad | 16, + 3/4, 6/8 | Vintage | Finger | Relaxed |
| 9 | EDM | 16 | Machine | Muted Pick | Tight |

Every style has:
- variations A and B, each with a groove for every intensity level;
- at least four fills (1 beat, 2 beats, 1 bar, 1 bar big);
- an ending;
- a double-time bar and a half-time bar;
- a suggested RhythmEngine genre kit, applied via
  `GenreKitLibrary::apply` only if the preset's `link_rhythm_kit` is on.
  The kit's rig preset is never loaded.

### 4.2 Intensity (`jam_intensity`, 1-5)

1. Rim or brushes with hat or ride; bass in whole or half notes.
2. Kick on 1 and 3, backbeat snare; bass in quarters.
3. The standard groove.
4. Open hats and a busier kick; bass in 8ths using 5 and 8.
5. Ride or crash quarters, a crash every 4 bars, the busiest bass, and
   fills twice as often.

**Dynamics follow** (`jam_dynamics_follow`) moves the *effective*
intensity by one step:
- -1 when the mean note-on velocity over the last 2 bars is below 55;
- +1 when it is above 105;
- 8 points of hysteresis, and the result stays within 1-5.

The status line shows this as "3 (+1)".

### 4.3 Fills, humanise, swing

- **Fill period.** `jam_fill_every` (Off, 2, 4, 8 or 16 bars) puts a fill
  in the last bar of each period. The fill grows with intensity: 1 beat
  at 1-2, 2 beats at 3, 1 bar at 4-5. A bar-seeded draw chooses among
  fills of that size.
- **Fill Now** (`jam_fill_now` rising, FILL, `Shift+J`) plays from the
  next beat to the bar line. If less than one beat is left, it plays the
  last 2 beats of the next bar.
- **Humanise** (`jam_humanise`). At 100 %, timing sigma is 6 ms for drums
  and 8 ms for bass, and velocity sigma is 8 %. Each lane keeps a steady
  push or pull (hat -2 ms, snare +3 ms). The downbeat kick never moves
  more than 2 ms. Humanise is applied at scheduling time and never stored
  (rhythm-engine rule 5).
- **Swing.** `jam_swing` offsets a 16-grid style's swing. Triplet grids
  ignore it.

### 4.4 Changes while playing

| Change | Takes effect |
|---|---|
| intensity, dynamics offset | next beat |
| style, variation, kit, bass voice | next bar line; a running fill finishes first |
| swing, humanise, follow | next scheduled event |
| mixer | immediately, smoothed over 20 ms (engine.md 0.4) |
| kit tuning, damping | each piece's next hit |

## 5. Drum kit synth (`JamDrumKit`)

The kit is physically informed. Each piece is a bank of damped two-pole
resonators (`ModalResonatorBank`: double precision, 4-wide SIMD), driven
by a contact-force pulse or a stochastic exciter. Every piece has a DC
blocker and a NaN guard (engine.md 0.3).

| Piece | Model and physics |
|---|---|
| Kick | 6 membrane modes (ratios 1, 1.594, 2.136, 2.296, 2.653, 2.918) plus 2 resonant-head modes coupled at 0.3. Beater: raised-cosine force pulse, 1.5 ms felt / 0.6 ms plastic. Tension modulation f(t) = f0 (1 + k a(t)^2), k up to 0.25 at velocity 127. |
| Snare | 8 batter modes. Snare-wire collision: noise gated by max(0, abs(head) - threshold), band-passed at 3.5 kHz, 80-250 ms decay. Accents > 115 add rim contact. Brush excitation: 120-400 ms filtered-noise sweep (Jazz, Ballad, intensity <= 2). |
| Toms x3 | 6 membrane modes each, with pitch drop, tuned a 4th or 3rd apart |
| Hi-hat | 32 inharmonic modes, f_k = f_h k^1.35 with seeded +-3 % jitter. T60: closed 50-90 ms, open 0.9-1.4 s, pedal chick 30 ms. Closing an open hat chokes it by ramping damping over 10 ms. |
| Ride | 48 modes, 300 Hz-14 kHz, T60 3-6 s low / 1-2 s high. A bell hit weights the lowest 8 modes. A re-strike adds to the ringing state. |
| Crash | 48 modes, T60 2-3 s. "Bloom": the high band rises over 30 ms, a stand-in for the nonlinear energy cascade. |
| Rim / sticks | 3 wood modes and faint batter modes; also the count-in |
| Shaker | PhISEM stochastic particles (64 beads), resonances at 3.2 and 6.5 kHz |

**Voice pool (fixed).** Kick 2, snare 2, each tom 2, hat 1, ride 1,
crash 1, rim 1, shaker 1. Stealing takes the oldest voice with a 2 ms
damping ramp. A bank below -100 dBFS is skipped.

**Kit styles** (`jam_kit`, or chosen by the style when `jam_kit_auto` is
on):

| Kit | Kick f0 | Snare f0 | Character |
|---|---|---|---|
| Studio | 55 Hz | 200 Hz | tight, medium damping |
| Vintage | 62 Hz | 185 Hz | looser heads, darker cymbals |
| Arena | 48 Hz | 175 Hz | low, long toms, more room |
| Jazz | 78 Hz | 240 Hz | 18" kick, ride-led, brushes |
| Machine | 50 Hz, 120 ms sweep | 220 Hz | short decays, strong sweeps (EDM) |

**Kit controls.**
- `jam_kit_tuning` is a tension change. It shifts every membrane f0 but
  keeps the mode ratios, and the pitch drop scales with it.
- `jam_kit_damping` scales membrane T60 (head muffling).
- `jam_kit_room` sends to a 4-line FDN room (0.1-0.6 s).
- Pan follows `jam_kit_perspective` (Audience puts the hat on the right),
  scaled by `jam_kit_width`.

## 6. Bass (`JamBassVoice`)

### 6.1 Decision: a dedicated voice, not a second `LuthierEngine`

A second full engine running a bass would cost about **5.8 units** by
performance-budget.md 1: strings 0.83 (4/12 of 2.5), coupling 0.4, body
0.6, pickup 0.5, circuit 0.05, amp 1.5, cab 0.4, room 0.3, master 0.15,
noise engines ~0.5, bass techniques 0.25, plus pedals. That is 73 % of
the 8-unit steady-state budget. It breaks the 22-unit
heavy cap and adds about 30 MB of DSP state, a second IR set and a
300 ms guitar load.

`JamBassVoice` instead keeps the parts that carry the realism:
- **Two `StringEngine` instances, ping-ponged.** The new note plucks the
  idle instance. The previous one gets `Damping::Released` (the finger
  lifting), so there is no click and no cut tail.
- **String choice like a bassist.** Four strings (E1 A1 D2 G2), 864 mm
  scale, roundwound .105-.045 via `StringMaterials`. The engine picks the
  string giving the lowest fret <= 7 nearest the previous position, and
  applies `setPhysical` to the idle instance before the pluck.
- **`Excitation`:** finger, pick, or palm-muted pick
  (`Damping::PalmMuteBass`). Upright uses a flesh pluck at 0.18 of the
  scale, with higher loop damping.
- **`JamBassTone`:**
  - a pickup-position comb at 0.21 of the scale;
  - a 2-pole pickup resonance at 4.5 kHz, Q 1.2;
  - a 3-band tone (`jam_bass_tone`);
  - tube saturation at 2x oversampling.

  Upright replaces the pickup with 2 body modes (95 and 180 Hz).

Budget: **0.6 units** (2 x 0.21 + 0.08 + 0.05), about a tenth of the
full-engine option. For full-rig realism, Pro users send the Jam bass
part to MIDI out (9) and into a second Luthier instance loaded with a
bass. That is the companion-instance path of tune-builder 6.

### 6.2 Lines

Each style's bass lane holds one token per step:
- `R 3 5 7 8`: chord tones (3.1).
- `A`: approach, only when the next chord is anticipated; otherwise R.
- `W`: walking step toward the next root.
- `m`: muted ghost at 40 % velocity.
- `-`: tie. `.`: rest.

The register is E1-C3. Each octave is chosen for the smallest leap, with
the root kept within E1-A2. No randomness is used in pitch choice.
`jam_bass_voice` is Auto (the style's own), Finger, Pick, Muted Pick or
Upright.

## 7. Mixer and routing

- **Mix.** `jam_volume`, plus `jam_balance`, an equal-power drums/bass
  crossfade (-1 drums only, +1 bass only). Each part has its own pan and
  mute.
- **The player is the bassist.** If the loaded guitar is a bass
  (`RhythmEngine::isBassFamily()`), Jam bass is silent, and the status
  reads "You're the bassist - Jam bass is resting".
- **`jam_output`.** Choices are Main, Separate, or Main + Separate.
  Separate uses **Aux 9 "Jam Drums"** and **Aux 10 "Jam Bass"** (both
  stereo):
  - The buses are appended after Aux 8 in `buildBusesProperties()`, so
    no existing bus number moves.
  - `isBusesLayoutSupported` treats them as aux pairs.
  - ROUTING gains two strips (mute, solo, gain, meter), with the tap
    description "Jam band, post Jam mixer".
  - On Layout A or C, Separate falls back to Main and shows the notice
    in 8.4.
- **Mix point in `processBlock`:**
  - after `looper.processBlock`, so the looper records guitar only;
  - before `sessionRecorder`, so takes include the band.

  Tone-match capture and the monitor mix never contain the band. The
  kill switch mutes it through a new `KillSwitch::applyBlockRamp`, which
  reapplies this block's computed ramp without advancing it.

## 8. UI

### 8.1 Advanced: Column 4 JAM tab

JAM goes directly after TUNE:

`WORKSHOP | MOD | RHYTHM | TUNE | JAM | LIVE | ROUTING | TONE MATCH |
CHARACTER | PRACTICE | NOTATION | MIDI OUT | CONTROLLERS | TECHNIQUES |
HELP`

The band is the tune's companion: RHYTHM, TUNE and JAM sit together. In
code, add `{ "JAM", jamPanel.get() }` after TUNE in the `AdvancedPanel`
tabs table.

```
+--------------------------------------------------------------------+
| JAM [ARMED o] [> START | [] STOP] [FILL]   PLAYING . Rock B . 3(+1)|
|      Am7 -> F (predicted) . 112 bpm host . bar 17.3         [?] [v]|
+----------------------------+---------------------------------------+
| STYLE  [10 styles + User]  | FEEL  Intensity (1)(2)(3)(4)(5)       |
| Variation (A)(B)           | Fills every [8 v]  Swing ( ) Human ( )|
| [x] Link guitar rhythm kit | [x] Dynamics follow                   |
+----------------------------+---------------------------------------+
| FOLLOW  Source [Auto v]    | START/STOP  Start [Auto v] Count [1 v]|
| (Tight)(Natural)(Relaxed)  | [x] Stop when I stop playing  [2] bars|
| (Bar)  [x] Predict repeats | [x] Play an ending                    |
+----------------------------+---------------------------------------+
| KIT [Studio v][x]auto Tuning( ) Damping( ) Room( ) Width( ) [Aud v]|
| BASS [Auto v] Tone ( )                                             |
| MIXER Vol( ) Bal( ) Drums pan( )[M] Bass pan( )[M] Out[Main v] ||| |
+--------------------------------------------------------------------+
| LANES chord | Am7 | F | C (pred) | G (pred) |   + 7 drum lanes,    |
|       bass lane, playhead      [Drag last [8 v] bars] [Export MIDI]|
+--------------------------------------------------------------------+
```

- The lane view is read-only. It shows the current bar (velocity drawn as
  opacity, with a playhead) and a chord track of 2 bars back and 2 ahead,
  with predicted and tune chords marked.
- At the 480 px minimum width, the paired groups stack and the lanes
  keep 96 px.
- The "Metronome goes quiet while the band plays" switch sits in
  START/STOP (see 11).

### 8.2 Easy Mode, Live Strip, shortcuts

- **Easy rhythm strip** (gui-integration 3.5). A JAM group is added at
  the right end:
  - JAM pill (88 x 32 px): the first press arms. The pill shows ARMED,
    COUNT, PLAYING or ENDING, and a press while playing stops the band.
  - Style dropdown.
  - 5-dot intensity control.
  - "Band" volume mini-knob.

  This meets gui-integration 0.3: the band's style, level and state are
  never hidden.
- **Live Strip** (gui-integration 9). A JAM pill after Tap: a tap
  starts or stops the band, a long press is FILL. It is shown only while
  `jam_enabled` is on, and uses 44 px targets.
- **Shortcuts** (gui-integration 17, rebindable). `J` start/stop,
  `Shift+J` fill, `Alt+J` arm/disarm. They appear in the cheat sheet.

### 8.3 Live data (gui-engine-dataflow)

The audio thread publishes a `JamStatus` snapshot after each block
(double buffer plus an atomic sequence number). It holds:
- state, bar, beat and effective intensity;
- style and variation;
- current chord, next chord and its source (tune, predicted or none);
- the active fill;
- 16 lane-hit bitmasks;
- part peaks.

The UI drains it at 30 Hz. After 250 ms without an update, the snapshot
is stale: the playhead hides and the status line shows "-".

### 8.4 Empty states and errors

- Armed, no chord yet: "Play a chord - the band follows. Or press START
  for drums first."
- Source Tune, nothing playing: "No tune is playing - following what you
  play."
- Separate output without aux: "Separate outputs need the multi-out
  layout (B or D). The band is on the main output."
- Unsupported meter: "Generic groove - Rock has no 7/8."
- No play head in Host Transport mode: "This host sends no transport;
  the band keeps its own time."
- A bass is loaded: the bass group shows the resting message from 7.
- A style file failed to load: the banner of 13, and the fallback name
  in STYLE.

## 9. MIDI out and export

- **Live MIDI out.** `MidiOutConfig` gains `jamParts` (default off),
  shown as the checkbox "Jam band" next to "Tune-builder playback".
  - Drums go out on GM channel 10: kick 36, rim 37, snare 38, closed hat
    42, pedal hat 44, open hat 46, toms 45/47/50, crash 49, ride 51,
    bell 53, shaker 82.
  - Bass goes out on channel 11.
  - Both channels are editable in MIDI OUT and saved with the preset's
    MIDI-out config.
  - Events are sample-accurate, including the `L` offset of rule 0.6.
- **Capture.** `JamCapture` is a fixed ring of 8192 events (at most the
  last 64 bars), written on the audio thread without allocation.
- **Drag-out.** The lanes' handle drags the last 4, 8, 16 or 32 bars, or
  all of them, as in midi-export 4.2. The file is Type 1 with a tempo map
  and two tracks, "Jam Drums" and "Jam Bass". The Generic profile is the
  default. The Luthier profile adds a `LUTHIER: JAM style= variation=
  intensity= kit=` text meta at each change.
- **Export MIDI...** opens the midi-export 4.1 dialog, with range "Jam
  capture" and split "per instrument".
- **Tune export** (tune-builder 9) gains "Include Jam band" (default on
  while `jam_enabled`). Audio renders include the band. MIDI exports add
  the two Jam tracks.

## 10. Parameters

All jam parameters are appended after the last existing parameter, in
this order, and never reordered. `jam_kit_tuning` and `jam_kit_damping`
are physical: they are `PhysicalRange` in a new `RangeFamily::jam`,
inserted before `numFamilies` in `PhysicalRange.h`.

| # | ID | Range | Default |
|---|---|---|---|
| 1 | `jam_enabled` | bool | off |
| 2 | `jam_play` | bool, transient | off |
| 3 | `jam_fill_now` | bool, transient, momentary | off |
| 4 | `jam_style` | Rock, Pop, Funk, Blues Shuffle, Country, Metal, Reggae, Jazz Swing, Ballad, EDM, User | Rock |
| 5 | `jam_variation` | A, B | A |
| 6 | `jam_intensity` | int 1-5 | 3 |
| 7 | `jam_fill_every` | Off, 2, 4, 8, 16 bars | 8 |
| 8 | `jam_follow` | Tight, Natural, Relaxed, Bar | Natural |
| 9 | `jam_predict` | bool | on |
| 10 | `jam_chord_source` | Auto, Live, Tune | Auto |
| 11 | `jam_start_mode` | Auto, Host Transport, First Note, Count-In, Tap In | Auto |
| 12 | `jam_count_in_bars` | int 0-2 | 1 |
| 13 | `jam_stop_on_silence` | bool | on |
| 14 | `jam_silence_bars` | int 1-8 | 2 |
| 15 | `jam_ending` | bool | on |
| 16 | `jam_dynamics_follow` | bool | on |
| 17 | `jam_swing` | -50..+50 % | 0 |
| 18 | `jam_humanise` | 0-100 % | 50 |
| 19 | `jam_kit` | Studio, Vintage, Arena, Jazz, Machine | Studio |
| 20 | `jam_kit_auto` | bool | on |
| 21 | `jam_kit_tuning` | stock -6..+6 st; advanced -12..+12 st | 0 |
| 22 | `jam_kit_damping` | stock 10-90 %; advanced 0-100 % | 40 |
| 23 | `jam_kit_room` | 0-100 % | 25 |
| 24 | `jam_kit_width` | 0-100 % | 70 |
| 25 | `jam_kit_perspective` | Audience, Drummer | Audience |
| 26 | `jam_bass_voice` | Auto, Finger, Pick, Muted Pick, Upright | Auto |
| 27 | `jam_bass_tone` | 0-1 | 0.5 |
| 28 | `jam_volume` | -60..+6 dB | -6 |
| 29 | `jam_balance` | -1..+1 | 0 |
| 30 | `jam_drums_pan` | -1..+1 | 0 |
| 31 | `jam_bass_pan` | -1..+1 | 0 |
| 32 | `jam_drums_mute` | bool | off |
| 33 | `jam_bass_mute` | bool | off |
| 34 | `jam_output` | Main, Separate, Main + Separate | Main |

Display names follow the pattern "Jam Style", "Jam Kit Tuning" and so
on. Net new: **+34**.

**Transient parameters** (`jam_play`, `jam_fill_now`):
- They are automatable and MIDI-learnable, so a footswitch can start the
  band.
- They are excluded from preset save/load, snapshots, morph and
  randomise, the same way `preset_morph_position` is excluded in
  `PresetManager.cpp`.
- Host state restores them off, so opening a project never starts the
  band.
- `jam_fill_now` resets itself one block after it rises.

## 11. Interactions

- **Rhythm engine.**
  - Jam uses the rhythm engine's chord while that engine drives (3.1).
  - The band's own clock drives the rhythm engine's grid (2.3).
  - `link_rhythm_kit` applies the style's genre kit (4.1).
- **Looper.**
  - While the band plays, new loops are quantised to whole bars and start
    recording on the next downbeat.
  - Loops record the guitar only (7).
  - A new `Looper::renderPlaybackMidi (juce::MidiBuffer&, int)` feeds the
    loop's stored MIDI to the chord follower. The band keeps following a
    looped rhythm part while the player solos.
- **Metronome and tune click.** Both go silent while the drums are
  audible; the visual beat keeps running. This is the user preference
  "Metronome goes quiet while the band plays" (UiPreferences, default
  on). Count-ins always use the band's sticks.
- **Tune playback: no double drums or bass.**
  - While Jam drums are audible, the tune's Percussion layer
    (`TunePart::percussion`, chuck noise) is not sent to the engine, and
    its layer strip shows "Replaced by Jam drums".
  - Tune section has a bass line (not Off), guitar instrument: the
    tune's bass-channel notes play through `JamBassVoice` *instead of*
    Jam's own line. This gives the tune bass a real sound (tune-builder
    6).
  - Bass instrument: the engine plays the tune bass and Jam bass rests.
  - `.luthiertune` sections may carry an optional jam hint,
    `"jam": {"intensity": 1-5, "fill_into": true}`. Old tunes are
    unaffected.
- **PROG looper and backing track.** PROG is an anticipated chord source.
  A backing track plays alongside with no automatic muting; the JAM tab
  hints "A backing track is also playing".
- **Snapshots and setlist.** Jam parameters recall like any other, with
  the quantisation of 4.4. A recall or setlist step never stops a
  playing band. A preset load keeps the band running with the new
  preset's jam settings.
- **Host sync.**
  - The band locks to ppq and `ppqPositionOfLastBarStart`.
  - A cycle jump or locate re-syncs at the new position: the pattern step
    is recomputed, ringing voices decay naturally and the bass re-plucks.
  - Tempo automation is followed every block.
- **Techniques and realism.** Unaffected: the band never touches the
  guitar path.
- **Multi-instance.** Each instance has its own band. Help notes that two
  jamming instances means two drummers.

## 12. State, undo, accessibility

**Preset.** The parameters, plus an optional top-level block
`"jam": {"style_ref": null | "User/My Shuffle.luthierjam",
"link_rhythm_kit": false, "seed": 4849997}`. It stays schema 3, and a
missing block means defaults. Snapshots store the block too.

**`.luthierjam`** is a new row in file-formats 1. It is JSON with:
- `"magic": "luthier.jam"`, `"schema": 1`, `meta`;
- `meters`, `grid`, `swing`;
- `kit`, `bass_voice`, `rhythm_kit`, `follow`;
- `grooves.{A,B}.{1..5}` with drum lanes as step strings: `.` rest, `g`
  ghost 30, `x` 90, `X` 118, `?` 50 % chance of 90, `o` open hat;
- `bass` token strings;
- `fills`, `ending`, `double_time`, `half_time`.

Factory styles are built in code (`JamStyleLibrary::addFactoryStyles`)
and can be overridden by name from `Resources/Jam/`. User styles live in
`~/Documents/Luthier/Jam/`, appear as "User" plus a file picker, and are
saved atomically (file-formats 13). An in-plugin groove editor is out of
scope; propose it in `spec/proposals/`.

**Undo** (action-and-undo): knob moves are class 3.1 (grouped within
200 ms), choices 3.2, toggles 3.3; picking a user style file is one
`jam-style-file` entry. START, STOP, FILL, count-in and tap-in are
**not undoable** (transport, like tap tempo in section 7).

**Accessibility.** Every control has a label, a value interface, a
tooltip and docs; Tab order follows the sketch. The pill announces state
changes ("Band playing, Rock, intensity 3"). At verbosity High, chord
changes are announced at most once every 2 s. The lane view's accessible
description reads the bar ("Kick 1 and 3, snare 2 and 4, hats 8ths;
bass A A E G"). Under reduced motion the playhead steps per beat. Lanes
use glyphs as well as colour.

## 13. Failure modes

| Failure | Response |
|---|---|
| Malformed or missing `.luthierjam` | Fall back to the factory style named in its `style` field, else Rock. Warning banner, logged once. `style_ref` is kept on save. |
| Chord map over 1024 entries | Truncated on the message thread and logged. Anticipation stops there. |
| No play head, or no ppq | Own clock, with the 8.4 notice. |
| Sample-rate change / re-prepare | Coefficients rebuilt, state reset, back to Armed. |
| CPU relief (performance-budget 8) | A new step between 4 and 5: cymbal banks 48 -> 24 modes, hat 32 -> 16. Bass and timing are never degraded. |
| Oversized host block | The processor's existing slicing handles it. The Jam clock is sample-based (JM-05). |
| NaN/Inf in a bank | The guard clamps, the piece is reset, and a diagnostics counter is incremented. |

## 14. Performance budget

Add to performance-budget.md 1 (48 kHz, 128-sample block):

| Module | Units | Notes |
|---|---|---|
| JamConductor + JamChordFollower | 0.05 | control rate; <= 0.02 when stopped |
| JamDrumKit | 0.9 | all pieces ringing (~200 modes, SIMD); typically 0.4 |
| JamBassVoice | 0.6 | 2 x StringEngine 0.42, tone 0.08, 2x OS saturation 0.05 |
| JamMixer + kit room | 0.15 | 4-line FDN, pans, meters |

Jam total <= 1.7 units (typically ~1.1). New scenario **"Jam"** (Rock
preset, 4 voices, band at intensity 5): <= 10 units. Memory under 2 MB;
style library load <= 20 ms; added plugin latency 0. Reaction latency:
First Note drums 0 musical samples (same block and offset as the note,
plus `L`); bass per 3.2, exactly on time when anticipated.

## 15. Edition split (editions.md 2.3, beside the rhythm engine)

**Free-limited.** A band to play along with is the strongest "play for
an hour" hook, and it builds on the rhythm engine, which is
fundamental.

**Free:** Rock, Pop, Blues Shuffle and Ballad (both variations, every
intensity, fills); Studio and Vintage kits; Finger and Pick bass; every
start, stop and follow mode, plus prediction; following the read-only
demo tunes; main output.

**Pro:** the other 6 styles and user styles; Arena, Jazz and Machine
kits; Muted Pick and Upright bass; kit tuning and damping (H8); Separate
outputs (H7); Jam MIDI out, drag-out and export (H5); anticipation from
edited tunes (H2).

Locked items follow editions 4.1. A Pro preset in Free plays the nearest
Free style and kit, and keeps the stored values (editions 5.1).

## 16. New classes and insertion points

- `Source/Jam/`: `JamEngine`, `JamConductor`, `JamChordFollower`
  (`JamChordMap`), `JamBassLine`, `JamStyle` / `JamStyleLibrary`,
  `JamCapture`, `JamStatus`.
- `Source/DSP/Jam/`: `ModalResonatorBank`, `DrumPieces` (Membrane,
  SnareWires, CymbalBank, Rim, PhisemShaker), `JamDrumKit`,
  `JamBassVoice` (`JamBassTone`), `KitRoom`.
- UI: `JamPanel`, `JamLaneView`, a JAM group in `EasyPanel`, a pill in
  `LiveStrip`.
- Tests: `Source/Tests/JamTests.cpp`, `JamDspTests.cpp`,
  `JamPanelTests.cpp`.

Insertion points in `LuthierAudioProcessor::processBlock`:
1. `jam.handleMidi` after the tune merge (the `engine.setDirectMidi`
   line).
2. The clock in the transport section.
3. `jam.renderBlock` into `jamStems` after `engine.processBlock`.
4. The mix after `looper.processBlock`, before `sessionRecorder`.
5. Aux 9 and 10 in `routing.distribute`.
6. Jam MIDI after `midiOutRouter.emit`.

## 17. Tests

Engine and timing (unit tests in `LuthierTests`):
- **JM-01** Every factory style parses and is complete: A/B x 1-5
  grooves, >= 4 fills, an ending, double-time and half-time bars. Every
  lane length equals its grid.
- **JM-02** Host at 120 bpm 4/4: with humanise 0, every Rock kick over 64
  bars starts within 1 sample of grid + `L`.
- **JM-03** Tempo automation from 90 to 140 bpm over 8 bars: drift
  against the host grid is <= 1 sample at every beat.
- **JM-04** Two offline renders of the same 30 s fixture and seed are
  bit-identical. A different seed gives a different render.
- **JM-05** Renders at block sizes 32, 128, 512 and 2048 (sliced) null
  to <= -120 dBFS.
- **JM-06** Natural follow, F played 200 ms after beat 2: the bass
  changes on beat 3 exactly, not before.
- **JM-07** Grace window, F played 40 ms after beat 2 at 120 bpm: the
  bass changes at the detection sample, <= 30 ms plus one block after
  the note.
- **JM-08** Tight, Relaxed and Bar: in 100 random trials, the bass
  changes on the next 8th, half-bar and bar respectively.
- **JM-09** 1000 random single-note phrases and Unknown detections never
  change the bass chord.
- **JM-10** C/G with `R` tokens: the bass plays G.
- **JM-11** Tune anticipation: every change's bass note starts exactly
  at the change + `L`. `A` tokens approach the next root by a semitone
  or a scale step.
- **JM-12** Prediction: a 4-bar cycle repeated twice predicts in
  cycle 3. A deviation in cycle 4 is corrected at the next Q and switches
  prediction off until 2 clean cycles.
- **JM-13** While `RhythmEngine::isDriving()`, Jam's chord equals
  `getCurrentChord()` every block.
- **JM-14** First Note: beat 1's kick and crash start at the note's
  offset + `L`.
- **JM-15** Tap In, 4 taps 500 ms apart: tempo 120, beat 1 at tap 4 +
  500 ms (+-1 ms).
- **JM-16** Count-in 1 gives 4 stick hits then beat 1. Count-in 0 gives
  no sticks.
- **JM-17** Stop on silence (2 bars): the ending falls on the downbeat
  after 2 silent bars. A held chord does not count as playing. The rule
  is disabled under host transport.
- **JM-18** Host stop: with the ending on, the ending plays; off, the
  band cuts within 20 ms. Panic brings every voice below -90 dBFS within
  10 ms and sets `jam_play` off.
- **JM-19** Intensity changes on the next beat. Style, variation and kit
  change on the next bar, never mid-fill.
- **JM-20** Fill Now with more than 1 beat left fills to the bar line.
  With less than 1 beat left, it fills the last 2 beats of the next bar.
- **JM-21** Dynamics follow: velocity 40 gives -1 and velocity 120 gives
  +1, clamped at 1 and 5, with no flapping at velocity 55 +- 4.
- **JM-22** 7/8 plays the generic bar. Ballad in 3/4 plays its own
  groove.
- **JM-23** A host cycle jump from bar 9 to bar 1 plays bar 1's step
  next and leaves no voices hanging.

DSP:
- **JM-24** Velocity-127 kick: FFT peaks at f0 x {1, 1.594, 2.136}
  +- 2 %. Pitch at 5 ms is 10-25 % above pitch at 150 ms.
- **JM-25** Snare wires at maximum threshold remove > 90 % of the
  3-6 kHz energy after 20 ms. Default wires decay within 250 ms.
- **JM-26** Closing an open hat drops its 5-10 kHz energy by >= 40 dB
  within 15 ms, and the maximum sample step stays < 0.05.
- **JM-27** A ride re-strike causes no discontinuity.
- **JM-28** `jam_kit_tuning` +12 st (advanced) doubles kick f0 +- 1 %.
  In stock range it clamps at +-6 st.
- **JM-29** Bass notes E1-C3 are within 3 cents of pitch after 100 ms,
  and note changes step < 0.05.
- **JM-30** An A1-D2-G2 line stays in one position (frets <= 7), and the
  two `StringEngine` instances alternate.
- **JM-31** A 60 s jam performs no audio-file reads (file-open hook).
- **JM-32** 10 min at intensity 5, at every rate from 44.1 to 192 kHz:
  no NaN or Inf, and DC below -60 dBFS.

Real-time and performance:
- **JM-33** 5 min of jamming, with style, kit and chord-map swaps every
  bar: zero audio-thread allocations and zero audio-thread locks.
- **JM-34** Budgets, each within 1.10 x its figure in 14:
  - JamDrumKit <= 0.9 units;
  - JamBassVoice <= 0.6;
  - Jam total <= 1.7;
  - "Jam" scenario <= 10;
  - armed and stopped <= 0.02;
  - off: 0.

State, routing, MIDI:
- **JM-35** The 32 non-transient jam parameters and the `jam` block
  round-trip exactly through a preset. `jam_play` and `jam_fill_now`
  never reach presets or snapshots, and are off after a host-state
  reload.
- **JM-36** Every pre-existing parameter index is unchanged, and the jam
  IDs are the last 34, in table order.
- **JM-37** Layout B, Separate: the band is on Aux 9 and 10 and not on
  main. Main + Separate: it is on both. Layout A: it is on main. Aux 1-8
  and the per-string bus numbers are unchanged.
- **JM-38** A loop recorded with the band playing nulls against a jam-off
  loop. The session take contains the band. The kill switch mutes the
  band within 3 ms.
- **JM-39** Jam MIDI out: drums on channel 10 with the GM map, bass on
  channel 11. Note-ons are within 1 sample of the audio onsets.
- **JM-40** An 8-bar drag-out is a valid Type 1 file with 2 tracks and a
  tempo map, and re-imports to the same notes.
- **JM-41** A malformed `.luthierjam` falls back to its named factory
  style, shows a banner, and keeps `style_ref` on save.

Combination:
- **JM-42** Tune with its percussion layer on and Jam drums on: no chuck
  percussion reaches the engine. With Jam drums off, the percussion
  returns.
- **JM-43** Tune bass on a guitar instance: `JamBassVoice` plays exactly
  the tune's bass-channel notes and Jam's own line is silent. On a bass
  instrument, Jam bass is silent.
- **JM-44** The metronome is silent while the drums are audible with the
  preference on, and clicks with it off.
- **JM-45** A mid-bar snapshot recall changes style at the next bar.
  Neither a recall nor a preset load stops the band.
- **JM-46** Host stopped, band playing: rhythm-engine strums land on the
  band's grid within 1 sample.

GUI (xvfb, in the style of `EditorTests.cpp`):
- **JM-47** The JAM tab sits between TUNE and LIVE. It selects and paints
  without clipping at Col 4 widths of 480-1600 px. Every control in 8.1
  is focusable, in the sketch's order.
- **JM-48** The Easy pill, style, intensity and Band volume drive their
  parameters. The pill follows ARMED, PLAYING, ENDING. The Live Strip
  pill shows only while `jam_enabled` is on.
- **JM-49** `J`, `Shift+J` and `Alt+J` do nothing while a text field has
  focus. They are rebindable and appear in the cheat sheet.
- **JM-50** Every 8.4 message appears in its forced state. Every control
  has a screen-reader label. The lane description matches the bar.
- **JM-51** Free build: the locked styles, kits, voices and outputs show
  the lock and the upsell panel. A Pro preset plays the nearest Free
  style.
