# JAM MODE SPEC: a synthesized backing band that follows your chords

A drummer and a bass player who listen to what the guitarist plays and
play along, live and in time. The chords come from what the player
holds (or from the Tune Builder's progression). The tempo comes from the
host transport, or from the plugin's own clock and tap tempo. Every
sound is synthesized: a small modal drum kit and a physically modelled
bass string. No samples.

Added 2026-09-24. Additive to `gui-integration.md`: it adds one Column 4
tab, one group to the Easy rhythm strip and one Live Strip pill, and it
moves nothing that is already placed. Test prefix **JM-**.

## 0. Ground rules

1. **No samples.** Drums are modal and stochastic-particle synthesis
   (section 5). The bass is a waveguide string (section 6). Nothing is
   read from an audio file. Count-in clicks are stick hits from the kit
   synth.
2. **The band is an audio module that belongs to the processor, not the
   guitar.** `JamEngine` sits next to `Metronome` and `Looper` in
   `LuthierAudioProcessor`. It never goes through the guitar's body,
   pickup, amp or cab, and it never writes into the guitar's event
   queue. The Tune Builder stays a MIDI writer (INDEX "Tune Builder is a
   MIDI writer"): Jam only *reads* the tune's chords.
3. **Musical, not instant.** The bass changes chord on a musical
   boundary (section 3.3), the way a real bassist does. Drums never wait
   for a chord.
4. **Real-time safe.** Everything that runs per block is pre-allocated
   in `prepareToPlay`. Styles and chord maps cross threads by atomic
   pointer swap (ui-wiring 4.3). No locks, no allocation, no strings on
   the audio thread (engine.md 0.2, performance-budget 0.4-0.5).
5. **Deterministic.** The same MIDI, transport, parameters and seed
   produce bit-identical audio at any block size, online or offline.
   All randomness is `RtRandom` seeded by (preset jam seed, bar index,
   lane).
6. **Aligned with the guitar.** Jam reports no latency of its own. It
   schedules every event `L` samples after its musical time, where `L`
   is the plugin's reported latency (`getLatencySamples()`). After the
   host's delay compensation, band and guitar land together on the grid.
7. **Silent and free when off.** With `jam_enabled` off, `JamEngine` is
   not called. With it on but stopped, only the conductor runs
   (<= 0.02 units).

## 1. Purpose and user stories

- *Practising alone:* "I strum Am-F-C-G and a drummer and bassist play
  along, and they change when I change."
- *Standalone on the couch:* "I play one chord, the band comes in on it,
  and it stops when I put the guitar down."
- *Stage / busking:* "I tap four beats on a footswitch and the band
  enters in that tempo, then I call a fill with another switch."
- *Songwriting:* "My Tune Builder progression plays, and the bass lands
  exactly on each change with a walk-up into the chorus."
- *In a DAW:* "The band locks to my session tempo. I send drums and bass
  to their own tracks, then drag the last 8 bars out as MIDI."

## 2. Behaviour overview

`JamEngine` has four states: **Off** (`jam_enabled` false), **Armed**
(enabled, waiting for its start trigger), **Counting** (count-in),
**Playing**, and **Ending** (playing its ending, then Armed).

### 2.1 Start modes (`jam_start_mode`)

| Mode | Starts when | Clock |
|---|---|---|
| Auto (default) | Host transport starts; with no host transport running, the first note | Host if playing, else own |
| Host Transport | Host transport starts. Never on its own | Host ppq, bar position, tempo, meter |
| First Note | The first note-on (velocity >= 20) while Armed. Beat 1 is that note's sample | Own clock at `getEffectiveTempo()` |
| Count-In | JAM Start pressed: `jam_count_in_bars` bars of stick clicks, then beat 1 | Own clock |
| Tap In | The 4th tap of TapTempo (header T, Live Strip pad, learned footswitch). The taps set tempo and phase; beat 1 lands one beat after the 4th tap | Own clock at the tapped tempo |

Pressing JAM Start (or `J`, or `jam_play` rising) in any mode except Host
Transport starts at once: Count-In mode counts in, and the other modes
start on the next beat of the own clock. With the host playing, pressing
Start joins the host grid at the next bar line.

### 2.2 Stop

- **Host stops** (in Auto or Host Transport): the band plays its ending
  if `jam_ending` is on, otherwise it cuts. Returns to Armed.
- **JAM Stop / `J` / `jam_play` falling:** ending on the next downbeat.
  Pressing it again during the ending cuts at once (20 ms fade).
- **Stop when I stop playing** (`jam_stop_on_silence`): no note-on for
  `jam_silence_bars` whole bars (held or sustained notes do not count,
  looper playback MIDI does). The band plays its ending on the next
  downbeat. In Auto and First Note modes the next note restarts it.
  The check is ignored while following the host transport, because the
  DAW owns start and stop there.
- **Ending:** the drums play the style's ending figure (a crash and kick
  on the downbeat, choked after the style's ring time). The bass plays
  the current root, which rings for 1 beat and is then damped. No ending
  when `jam_ending` is off.
- **Panic** (header P): stops the band with a 5 ms choke of every voice,
  sets `jam_play` false and returns to Armed.

### 2.3 Tempo and meter

Priority, highest first: host tempo while the host plays; the Tune
Builder's own clock while a tune plays with the host stopped; the Jam
own clock at `LuthierAudioProcessor::getEffectiveTempo()` (tap tempo,
then last host tempo, then 120). A tap while Playing on the own clock
changes the tempo at the next bar line. Meter comes from the host
(`PositionInfo::getTimeSignature`), else from the tune, else 4/4. A
style declares the meters it has patterns for (all have 4/4; Ballad and
Country have 3/4; Ballad has 6/8). For any other meter the band plays
the built-in *generic* bar: kick on 1, hat on every beat, snare on the
last beat, bass on the root on beat 1. The status line says "Generic
groove (style has no 7/8)". Below 50 bpm a style plays its double-time
pattern; above 220 bpm, its half-time pattern.

When the own clock is driving, the processor hands its ppq and tempo to
`LuthierEngine::setTransportPosition` / `setTempoBpm`, the way it does
for the tune's clock today, so the RhythmEngine strums on the band's
grid.

## 3. Following the chords

### 3.1 Chord sources (`jam_chord_source`)

- **Live:** `JamChordFollower` owns its own `ChordDetector` (same class,
  same templates and confidence floor as the rhythm engine). It is fed
  the block's MIDI after MIDI Learn, the live-performance consumers and
  the tune's chord-channel merge in `processBlock`, plus the looper's
  playback-layer MIDI (section 11). If `RhythmEngine::isDriving()` is
  true, Jam takes `getCurrentChord()` instead, so the band and the
  strum always agree.
- **Tune:** the chord map of the playing tune (3.4).
- **Auto (default):** Tune while `TunePlayer::isPlaying()` or the
  practice PROG looper plays a progression; otherwise Live.

**What changes the chord.** Only a detection with at least 2 pitch
classes (a power chord counts) and confidence >= `kConfidenceFloor`.
Single notes are melody: they never change the band's chord. An Unknown
detection keeps the previous chord. Before any chord is known the bass
rests and the status reads "Waiting for a chord". The chord's bass note
is used when the chord is a slash chord (`ChordSymbol::isSlash()`) and
the style's bass line plays roots.

### 3.2 What each part does with a chord

The drums ignore harmony. The bass line (section 6.2) turns the style's
degree tokens into notes from the chord's `ChordTemplate::intervalMask`:
R = root (or slash bass), 3 = the template's third (major or minor; no
third means R), 5 = fifth (b5 / #5 when the template has one), 7 = the
template's seventh, else the octave, 8 = octave.

### 3.3 Reaction latency rules (live chords)

A chord is *detected* when the ChordDetector burst window closes
(<= 30 ms after its first note). The bass changes on the next
**quantum** Q of `jam_follow`:

| Follow | Q | Typical use |
|---|---|---|
| Tight | next 8th | funk, EDM |
| Natural (default) | next beat | most styles |
| Relaxed | next half-bar | ballad, reggae |
| Bar | next bar line | jazz, very slow songs |

**Grace window:** if the chord's first note came within
G = min(90 ms, one 16th) *after* a quantum boundary, the bass changes
immediately at the detection sample, not at the next Q. A bassist who
hears the change right on the beat plays it slightly late rather than
a whole beat late. A note already sounding keeps sounding until the
change. At the change the old note is damped (Released) and the new one
is plucked.

### 3.4 Anticipation

When the next chord is known ahead of time, the bass changes *exactly*
on the chord's time and may play the style's approach token (`A`, a
chromatic or diatonic step into the next root) on the last 8th before
it:
- **Tune:** `TuneTimeline::build` also emits a `JamChordMap` (sorted
  array of { ppq, root, bass, templateIndex, sectionIndex }, max 1024
  entries). The message thread builds it with the timeline and passes it
  to `JamEngine::setChordMap` by atomic pointer swap. The retired map is
  freed on the message thread.
- **Section changes** in the map schedule an automatic fill into the new
  section, and apply the section's jam intensity if it has one (13.2).
- **Prediction** (`jam_predict`, Live only): `JamChordFollower` keeps
  the last 32 bar-aligned chord changes. When one cycle of 1, 2, 4, 8
  or 16 bars has repeated *twice* in full (same chords at the same
  beat, within one quantum), it predicts the next change and treats it
  as anticipated. If a live chord contradicts a prediction, the bass
  corrects at the next Q. Prediction then stays off until the cycle has
  repeated twice again. The status line shows the predicted next chord
  in italics.

## 4. Grooves, intensity and fills

### 4.1 Factory styles

Ten styles. The style suggests a kit, a bass voice and a RhythmEngine
genre kit (`GenreKitLibrary` name, applied only if the preset's
`link_rhythm_kit` is on).

| # | Style | Grid | Swing | Kit | Bass voice | Default follow |
|---|---|---|---|---|---|---|
| 0 | Rock | 16 | 0 | Studio | Pick | Natural |
| 1 | Pop | 16 | 0 | Studio | Finger | Natural |
| 2 | Funk | 16 | 8 % | Studio | Finger (muted ghosts) | Tight |
| 3 | Blues Shuffle | 12 (triplet) | n/a | Vintage | Finger | Natural |
| 4 | Country | 16 (+3/4) | 0 | Vintage | Pick | Natural |
| 5 | Metal | 16 | 0 | Arena | Muted Pick | Tight |
| 6 | Reggae | 16 | 12 % | Vintage | Finger | Relaxed |
| 7 | Jazz Swing | 12 (triplet) | n/a | Jazz | Upright (walking) | Bar |
| 8 | Ballad | 16 (+3/4, 6/8) | 0 | Vintage | Finger | Relaxed |
| 9 | EDM | 16 | 0 | Machine | Muted Pick | Tight |

Each style has variations **A** and **B**, each with a groove for each
intensity 1-5, at least 4 fills (1 beat, 2 beats, 1 bar, 1 bar big), an
ending, and a double-time and a half-time bar.

### 4.2 Intensity (`jam_intensity`, 1-5)

- **1:** Sparse. Rim or brushes (Jazz, Ballad) and closed hat or ride.
  Bass in whole or half notes on R.
- **2:** Light. Kick on 1 and 3, snare on the backbeat. Bass on the
  quarters.
- **3:** The standard groove.
- **4:** Full. Open hats, busier kick. 8ths in the bass with 5 and 8.
  Fills every `jam_fill_every` bars.
- **5:** Driving. Ride or crash on the quarters, crash on every
  4-bar downbeat, busiest bass line, fills twice as often.

**Dynamics follow** (`jam_dynamics_follow`): the mean note-on velocity
over the last 2 bars, compared with 80, moves the *effective* intensity
by -1, 0 or +1. The boundaries are velocity 55 and 105, with a hysteresis
of 8. The result is clamped to 1-5. The status shows it as "3 (+1)".

### 4.3 Fills, humanise, swing

- `jam_fill_every` (Off / 2 / 4 / 8 / 16 bars) plays a fill in the last
  bar of each period. The fill size grows with intensity (1-2: 1-beat,
  3: 2-beat, 4-5: 1-bar). The choice among a style's fills of that size
  uses the bar-seeded RNG.
- **Fill Now** (`jam_fill_now` rising, the FILL button, `Shift+J`):
  starts on the next beat and runs to the bar line. With less than one
  beat left, it plays the last 2 beats of the next bar. Tune section
  changes and the ending also call fills.
- **Humanise** (`jam_humanise`, 0-100 %): at 100 %, timing sigma is
  6 ms for drums and 8 ms for bass, and velocity sigma is 8 %. Drums
  have a consistent per-lane push/pull (hat -2 ms, snare +3 ms at
  100 %). The downbeat kick is never moved more than 2 ms. Applied when
  scheduled, never stored (rhythm-engine rule 5).
- **Swing** (`jam_swing`, -50..+50 %) offsets the style's swing (16-step
  grids only). Triplet-grid styles ignore it.

### 4.4 Style changes while playing

| Change | Takes effect |
|---|---|
| `jam_intensity`, dynamics offset | next beat |
| `jam_style`, `jam_variation`, `jam_kit`, `jam_bass_voice` | next bar line (a fill is not interrupted; it waits for the bar after the fill) |
| `jam_swing`, `jam_humanise`, `jam_follow` | next scheduled event |
| Mixer parameters | at once, smoothed 20 ms (engine.md 0.4) |
| Kit tuning / damping | next hit per piece (ringing modes keep their coefficients) |

## 5. The drum kit synth (`JamDrumKit`)

Physically-informed modal synthesis. Each piece is a bank of damped
two-pole resonators (`ModalResonatorBank`, 4-wide SIMD, double
precision). The bank is driven by a contact-force pulse or a stochastic
exciter. Each piece has a DC blocker and a NaN guard (engine.md 0.3).

| Piece | Model | Key physics |
|---|---|---|
| Kick | 6 membrane modes (ratios 1, 1.594, 2.136, 2.296, 2.653, 2.918) + 2 resonant-head modes coupled at 0.3 | Beater: raised-cosine force pulse 1.5 ms (felt) / 0.6 ms (plastic). Tension modulation: f(t) = f0 (1 + k a(t)^2), k up to 0.25 at velocity 127 (the pitch drop of a hard hit) |
| Snare | 8 batter modes + snare-wire collision | Wires: noise gated by max(0, abs(head displacement) - threshold), band-passed 3.5 kHz, decay 80-250 ms. Accent > 115 adds a rim-shot contact. Brush excitation: filtered-noise sweep of 120-400 ms (Jazz, Ballad at intensity <= 2) |
| Toms x3 | 6 membrane modes each, with pitch drop | Tuned a 4th / 3rd apart per kit |
| Hi-hat | 32 inharmonic modes, f_k = f_h k^1.35 with seeded +-3 % jitter | One choke group. Closed T60 50-90 ms, open 0.9-1.4 s, pedal "chick" 30 ms. Closing chokes an open hat by ramping damping over 10 ms (a physical choke, not a fade) |
| Ride | 48 modes 300 Hz-14 kHz, T60 3-6 s (low) / 1-2 s (high) | Bell hits weight the lowest 8 modes. Re-striking adds to the ringing state (no voice restart) |
| Crash | 48 modes, T60 2-3 s | "Bloom": high-band energy rises with a 30 ms attack, a cheap stand-in for the nonlinear energy cascade |
| Rim / sticks | 3 wood modes + 4 faint batter modes | Also the count-in click |
| Shaker | PhISEM stochastic particles (64 beads) | Two resonances (3.2 kHz, 6.5 kHz), energy decay per shake |

**Voices:** a fixed pool. Kick 2, snare 2, each tom 2, hat 1, ride 1,
crash 1, rim 1, shaker 1. The oldest voice of a piece is stolen with a
2 ms damping ramp. A piece whose bank energy is below -100 dBFS is
skipped.

**Kit styles** (`jam_kit`) set f0, damping, shell size and cymbal
brightness. `jam_kit_auto` lets the style pick the kit.

| Kit | Kick f0 | Snare f0 | Character |
|---|---|---|---|
| Studio | 55 Hz | 200 Hz | tight, medium damping |
| Vintage | 62 Hz | 185 Hz | looser heads, darker cymbals |
| Arena | 48 Hz | 175 Hz | low, long toms, more room |
| Jazz | 78 Hz | 240 Hz | 18" kick, ride-led, brushes available |
| Machine | 50 Hz, 120 ms sweep | 220 Hz | short decays, strong pitch sweeps (EDM) |

`jam_kit_tuning` shifts every membrane f0 by semitones. It stays a
tension change: mode ratios hold and pitch drop scales with tension.
`jam_kit_damping` scales membrane T60 (head muffling). `jam_kit_room` is
the send to a 4-line FDN kit room (0.1 s-0.6 s). The pieces are panned
per `jam_kit_perspective` (Audience: hat on the right) and scaled by
`jam_kit_width`.

## 6. The bass (`JamBassVoice`)

### 6.1 Decision: a dedicated bass voice, not a second `LuthierEngine`

A second full `LuthierEngine` running a bass guitar would cost about
**5.8 units**, using performance-budget.md 1: strings 4/12 of 2.5 =
0.83, coupling 0.4, body 0.6, pickup 0.5, circuit 0.05, amp 1.5,
cab 0.4, room 0.3, master 0.15, noise engines about 0.5, bass
techniques 0.25, plus a few pedals. That is 73 % of the 8-unit
steady-state budget, it would break the 22-unit heavy-preset cap, and it
adds about 30 MB of DSP state, a second IR set and a 300 ms guitar load.

`JamBassVoice` reuses the parts of that chain that carry the realism,
the waveguide and the excitation:
- **Two `StringEngine` instances**, ping-ponged. The new note plucks the
  idle one while the previous one is set to `Damping::Released` (the
  fretting finger lifting), so a note change never clicks and never
  cuts a tail unnaturally.
- **String choice like a bassist:** 4 strings E1 A1 D2 G2, 864 mm scale,
  roundwound .105-.045 via `StringMaterials`. Each note goes on the
  string that gives the lowest fret <= 7 nearest the previous note's
  position. `setPhysical` is applied to the idle instance before the
  pluck.
- **`Excitation`** in finger, pick or palm-muted pick
  (`Damping::PalmMuteBass`). Upright uses a flesh pluck at 0.18 of the
  string length with higher loop damping.
- **`JamBassTone`**: a pickup-position comb (P-style at 0.21 of the
  scale length, not used for Upright), a 2-pole pickup resonance at
  4.5 kHz, Q 1.2, a 3-band tone controlled by `jam_bass_tone`, and a
  gentle tube-style saturation at 2x oversampling. Upright uses a 2-mode
  body (95 Hz, 180 Hz) instead of the pickup.

Measured budget **0.6 units** (2 x 0.21 string + 0.08 tone + 0.05
saturation). That is 10 % of the full-engine option. For full-rig bass
realism, Pro users send the Jam bass part to MIDI out (section 9) and
into a second Luthier instance loaded with a bass (the
companion-instance path in tune-builder 6).

### 6.2 Bass lines

Each style's groove has a bass lane of degree tokens per step: `R 3 5 7
8` (3.2), `A` (approach, only when the next chord is anticipated;
otherwise R), `W` (walking step: a chord tone or passing tone toward the
next root, Jazz), `m` (muted ghost at 40 % velocity), `-` (tie) and `.`
(rest). Register: E1-C3. The octave of each note is chosen to minimise
the leap from the previous note, with the root kept within E1-A2.
Deterministic: no RNG in the pitch choice.

**Bass voice** `jam_bass_voice`: Auto (the style's), Finger, Pick,
Muted Pick, Upright.

## 7. Mixer and routing

- `jam_volume` sets the band level (dB). `jam_balance` is an
  equal-power crossfade: -1 drums only, +1 bass only.
  `jam_drums_pan` and `jam_bass_pan` pan each part. `jam_drums_mute`
  and `jam_bass_mute` mute each part.
- **The bass rests when the player is the bassist:** if the loaded
  guitar's family is bass (`RhythmEngine::isBassFamily()`), the Jam bass
  is silent. The status says "You're the bassist - Jam bass is resting".
- **Output** (`jam_output`): Main; Separate (Aux 9 "Jam Drums" and
  Aux 10 "Jam Bass", both stereo); or Main + Separate. The two buses are
  **appended after Aux 8** in `buildBusesProperties()`, so no existing
  bus number moves. `isBusesLayoutSupported`'s aux test is extended to
  them. The ROUTING tab gains two aux strips (mute, solo, gain, meter)
  with tap description "Jam band, post Jam mixer". On Layout A or C
  (no aux), Separate falls back to Main and the JAM tab shows the inline
  notice of section 8.4.
- Jam is summed **after the looper** (the looper records the guitar
  only; 11) and **before the session recorder** (a take includes the
  band). Tone-match capture and the monitor mix never include it. The
  kill switch mutes it too, through a new `KillSwitch::applyBlockRamp`
  that applies the ramp this block already computed, without advancing
  it.

## 8. UI

### 8.1 Advanced Mode: Column 4 JAM tab

The tab goes right after TUNE. The fixed order (gui-integration 4.4,
with gui-techniques-updates' TECHNIQUES) becomes:
`WORKSHOP | MOD | RHYTHM | TUNE | JAM | LIVE | ROUTING | TONE MATCH |
CHARACTER | PRACTICE | NOTATION | MIDI OUT | CONTROLLERS | TECHNIQUES |
HELP`. The `AdvancedPanel` tabs table gets `{ "JAM", jamPanel.get() }`
after TUNE.

```
+--------------------------------------------------------------------+
| JAM [ARMED o] [> START / [] STOP] [FILL]  PLAYING . Rock B . 3(+1) |
|      Am7 -> F (predicted) . 112 bpm host . bar 17.3          [?][v]|
+----------------------------+---------------------------------------+
| STYLE                      | FEEL                                  |
| [ list: 10 styles + User ] | Intensity (1)(2)(3)(4)(5)             |
| Variation (A)(B)           | Fills every [8 bars v]  Swing  Human. |
| [x] Link guitar rhythm kit | [x] Dynamics follow                   |
+----------------------------+---------------------------------------+
| FOLLOW                     | START / STOP                          |
| Source [Auto v]            | Start [Auto v]   Count-in [1 v] bars  |
| Follow (Tight)(Natural)    | [x] Stop when I stop playing  [2] bars|
|        (Relaxed)(Bar)      | [x] Play an ending                    |
| [x] Predict repeats        |                                       |
+----------------------------+---------------------------------------+
| KIT  [Studio v] [x] auto   | BASS  [Auto v]  Tone ( )              |
| Tuning ( ) Damping ( ) Room ( ) Width ( ) Perspective [Audience v] |
+--------------------------------------------------------------------+
| MIXER  Volume ( )  Balance ( )  Drums pan ( ) [M]  Bass pan ( ) [M]|
|        Output [Main v]   drums meter ||||   bass meter ||||        |
+--------------------------------------------------------------------+
| LANES  chord  | Am7        | F          | C  (pred) | G  (pred)   |
|  crash  ride  hat  snare  kick  toms  bass   (1 bar, playhead)     |
|  [Drag last [8 v] bars]  [Export MIDI...]                          |
+--------------------------------------------------------------------+
```

- The lane view is read-only. It shows the bar being played, with
  lanes, velocity as opacity and the playhead. The chord track shows the
  last 2 bars and the next 2, with predicted and tune chords marked. It
  is drawn from the `JamStatus` snapshot (8.3).
- At the 480 px minimum column width, FEEL/START and KIT/BASS stack into
  single columns and the lane view keeps 96 px.

### 8.2 Easy Mode and Live Strip

- **Easy rhythm strip** (gui-integration 3.5) gains a **JAM** group at
  its right end. It has a large JAM toggle pill (88 x 32 px): the first
  press arms, and it shows ARMED, COUNT, PLAYING or ENDING. Then a style
  dropdown (10 + User), a 5-dot intensity control and a "Band" volume
  mini-knob (`jam_volume`). Clicking the pill while Playing stops the
  band. This satisfies gui-integration 0.3: the band's style, level and
  on/off are all visible in Easy.
- **Live Strip** (gui-integration 9) gains a JAM pill after Tap: tap to
  start or stop, long-press for FILL. It shows only while `jam_enabled`
  is on. The live 44 px hit target applies.
- **Shortcuts** (gui-integration 17, rebindable): `J` start/stop,
  `Shift+J` fill, `Alt+J` arm/disarm. The Help cheat sheet lists them.

### 8.3 Live data (gui-engine-dataflow)

`JamStatus` is published by the audio thread after each block. It is
double-buffered with an atomic sequence number and holds: state, bar,
beat, effective intensity, style, variation, current chord, next chord
and its source (tune / predicted / none), active fill, 16 lane-hit
bitmasks for the current bar, and peak levels. The UI drains it at
30 Hz. Stale after 250 ms: the playhead hides and the status line shows
"-".

### 8.4 Empty states and errors

- Armed, no chord yet: "Play a chord - the band follows. Or press
  START for drums first."
- Source Tune, no tune: "No tune is playing - following what you play."
- Separate output without aux buses: "Separate outputs need the
  multi-out layout (B or D). The band is on the main output."
- Bass family loaded: the bass group shows the resting message of 7.
- Unsupported meter: "Generic groove - Rock has no 7/8."
- Style file failed to load (13): banner plus fallback name in STYLE.
- Host Transport mode with no play head: "This host sends no transport;
  the band keeps its own time."

## 9. MIDI out and export

- **Live MIDI out:** `MidiOutConfig` gains `jamParts` (default off). A
  routing-panel checkbox "Jam band" sits next to "Tune-builder
  playback". Drums use GM channel 10 (kick 36, rim 37, snare 38, closed
  hat 42, pedal hat 44, open hat 46, toms 45/47/50, crash 49, ride 51,
  bell 53, shaker 82). Bass uses channel 11. Both channels can be set in
  the MIDI OUT tab and are saved in the preset's `midi_out` block. The
  events are sample-accurate, including the latency offset of rule 0.6.
- **Capture:** `JamCapture` keeps a fixed ring of 8192 events (the last
  64 bars at most), written on the audio thread with no allocation.
- **Drag-out:** the lane view's handle drags the last N bars (4 / 8 / 16
  / 32 / all). It is a Type 1 file with tracks "Jam Drums" and "Jam
  Bass" and a tempo map, as the midi-export 4.2 drag-out. The Generic
  profile is the default. In the Luthier profile, a
  `LUTHIER: JAM style=... variation=... intensity=... kit=...` text meta
  is added at each change.
- **Export MIDI...** opens the midi-export 4.1 dialog with the range
  preset to Jam capture, and the track split "per instrument".
- **Tune export** (tune-builder 9) gains "Include Jam band" (default on
  when `jam_enabled`). Audio: the band is in the render. MIDI: the two
  Jam tracks are added.

## 10. Parameters

Appended at the end of the parameter list, in this order, after the last
existing parameter. They are never reordered. The
`jam_kit_tuning` / `jam_kit_damping` pair is physical: `PhysicalRange`,
new `RangeFamily::jam`, appended before `numFamilies` in
`PhysicalRange.h`.

| # | ID | Name | Range | Default |
|---|---|---|---|---|
| 1 | `jam_enabled` | Jam | bool | off |
| 2 | `jam_play` | Jam Play | bool (transient) | off |
| 3 | `jam_fill_now` | Jam Fill | bool (transient, momentary) | off |
| 4 | `jam_style` | Jam Style | Rock, Pop, Funk, Blues Shuffle, Country, Metal, Reggae, Jazz Swing, Ballad, EDM, User | Rock |
| 5 | `jam_variation` | Jam Variation | A, B | A |
| 6 | `jam_intensity` | Jam Intensity | int 1-5 | 3 |
| 7 | `jam_fill_every` | Jam Fills Every | Off, 2, 4, 8, 16 bars | 8 |
| 8 | `jam_follow` | Jam Follow | Tight, Natural, Relaxed, Bar | Natural |
| 9 | `jam_predict` | Jam Predict | bool | on |
| 10 | `jam_chord_source` | Jam Chords From | Auto, Live, Tune | Auto |
| 11 | `jam_start_mode` | Jam Start | Auto, Host Transport, First Note, Count-In, Tap In | Auto |
| 12 | `jam_count_in_bars` | Jam Count-In | int 0-2 | 1 |
| 13 | `jam_stop_on_silence` | Jam Stop When I Stop | bool | on |
| 14 | `jam_silence_bars` | Jam Silence Bars | int 1-8 | 2 |
| 15 | `jam_ending` | Jam Ending | bool | on |
| 16 | `jam_dynamics_follow` | Jam Dynamics Follow | bool | on |
| 17 | `jam_swing` | Jam Swing | -50..+50 % | 0 |
| 18 | `jam_humanise` | Jam Humanise | 0-100 % | 50 |
| 19 | `jam_kit` | Jam Kit | Studio, Vintage, Arena, Jazz, Machine | Studio |
| 20 | `jam_kit_auto` | Jam Kit From Style | bool | on |
| 21 | `jam_kit_tuning` | Jam Kit Tuning | stock -6..+6 st; advanced -12..+12 st | 0 |
| 22 | `jam_kit_damping` | Jam Kit Damping | stock 10-90 %; advanced 0-100 % | 40 |
| 23 | `jam_kit_room` | Jam Kit Room | 0-100 % | 25 |
| 24 | `jam_kit_width` | Jam Kit Width | 0-100 % | 70 |
| 25 | `jam_kit_perspective` | Jam Kit Perspective | Audience, Drummer | Audience |
| 26 | `jam_bass_voice` | Jam Bass Voice | Auto, Finger, Pick, Muted Pick, Upright | Auto |
| 27 | `jam_bass_tone` | Jam Bass Tone | 0-1 | 0.5 |
| 28 | `jam_volume` | Jam Volume | -60..+6 dB | -6 |
| 29 | `jam_balance` | Jam Drums/Bass | -1..+1 | 0 |
| 30 | `jam_drums_pan` | Jam Drums Pan | -1..+1 | 0 |
| 31 | `jam_bass_pan` | Jam Bass Pan | -1..+1 | 0 |
| 32 | `jam_drums_mute` | Jam Drums Mute | bool | off |
| 33 | `jam_bass_mute` | Jam Bass Mute | bool | off |
| 34 | `jam_output` | Jam Output | Main, Separate, Main + Separate | Main |

Net new: **+34**. **Transient** (`jam_play`, `jam_fill_now`): automatable
and MIDI-learnable (a footswitch starts the band). They are excluded
from preset save and load, snapshot capture and recall, morph and
randomise, the same way `preset_morph_position` is excluded in
`PresetManager.cpp`. A host session restores them as off, so reopening
a project never starts the band. `jam_fill_now` resets itself to off
one block after it rises. Every parameter except these two is captured
in snapshots.

## 11. Interactions

- **Rhythm engine:** the band uses the rhythm engine's chord while it is
  driving (3.1). The own clock feeds the rhythm engine's transport (2.3).
  `link_rhythm_kit` applies the style's genre kit through
  `GenreKitLibrary::apply` on style change (message thread). The rig
  preset is never loaded.
- **Looper:** while the band plays, a new loop's length is quantised to
  whole bars and recording starts on the next downbeat. The loop records
  the guitar only (7). The looper's playback-layer MIDI feeds the chord
  follower (new `Looper::renderPlaybackMidi (juce::MidiBuffer&, int)`),
  so a looped rhythm part keeps the band changing while the player
  solos. It does not count as "playing" for stop-on-silence unless the
  loop is playing.
- **Metronome and tune click:** while the band's drums are audible, the
  practice metronome and the tune click go silent. The visual beat dots
  keep running. User preference "Metronome goes quiet while the band
  plays" (default on) sits in the JAM tab and is stored in
  UiPreferences. A count-in always uses the band's sticks.
- **Tune Builder playback, no double drums:** while Jam drums are
  audible, the tune's Percussion layer (TunePart::percussion, chuck
  noise) is not sent to the engine. Its layer strip shows the pill
  "Replaced by Jam drums". If the section has a bass line other than
  Off and the instrument is not a bass, the tune's bass-channel notes
  are played by `JamBassVoice` *instead of* Jam's own bass line. This
  gives the tune's bass an actual sound on a guitar instance
  (tune-builder 6). If the instrument is a bass, the engine plays the
  tune bass and Jam bass rests (7). Sections may carry jam hints (13.2).
- **Practice PROG and backing track:** PROG is an anticipated chord
  source (3.1). A backing track plays alongside without any automatic
  muting. The JAM tab shows the hint "A backing track is also playing".
- **Snapshots / setlist:** jam parameters recall like any parameter,
  with the quantisation of 4.4. A setlist step changes style at the next
  bar and never stops a playing band.
- **Host sync:** with the host playing, the band is locked to ppq and
  `ppqPositionOfLastBarStart`. A cycle jump or locate re-syncs at the
  new position: the pattern restarts at the right step, ringing voices
  decay naturally and the bass re-plucks. Tempo automation is followed
  per block.
- **Techniques / realism:** unaffected. The band does not touch the
  guitar path, so guitar CPU relief, technique cascade and noise engines
  are independent.
- **Multi-instance:** each instance has its own band. Help notes that
  two jamming instances means two drummers.

## 12. State, undo, accessibility

**Preset** (`.luthierpreset`, file-formats 2): the parameters, plus a
new top-level block
`"jam": { "style_ref": null | "User/My Shuffle.luthierjam",
"link_rhythm_kit": false, "seed": 4849997 }`.
A missing block loads defaults. Schema stays 3: the block is optional
and additive. Snapshots store the `jam` block beside the parameters.

**`.luthierjam`** (new row in file-formats 1): JSON with
`"magic": "luthier.jam"`, `"schema": 1`, `meta`, `meters`, `grid`,
`swing`, `kit`, `bass_voice`, `rhythm_kit`, `follow`, `grooves.{A,B}.
{1..5}.lanes` (step strings: `.` rest, `g` ghost 30, `x` 90, `X` 118,
`?` 50 % chance of 90, `o` open hat), `bass` (token strings), `fills`,
`ending`, `double_time`, `half_time`. Factory styles are built in code
(`JamStyleLibrary::addFactoryStyles`). They can be overridden by name
from `Resources/Jam/`. User styles live in `~/Documents/Luthier/Jam/`,
appear as "User" plus a picker, and are saved atomically (file-formats
13). An in-plugin groove editor is out of scope here. It goes to
`spec/proposals/` if wanted.

**Undo** (action-and-undo): jam parameter moves are class 3.1 (grouped,
200 ms). Style, kit, voice, output and follow choices are class 3.2.
Toggles are class 3.3. Start, stop, fill, count-in and tap-in are
**not undoable** (transport, like tap tempo; section 7). Choosing a user
style file is one entry `jam-style-file`.

**Accessibility:** every control has a label, a value interface, a
tooltip and a docs entry. The tab order follows the sketch, left to
right and top to bottom. The JAM pill announces its state changes
("Band playing, Rock, intensity 3"). At verbosity High, chord changes
are announced at most once per 2 s. The lane view exposes a text
description of the current bar ("Kick 1 and 3, snare 2 and 4, hats 8ths;
bass A, A, E, G"). Under reduced motion the playhead jumps per beat.
Lanes carry glyphs as well as colours, so they read in monochrome and
in every palette.

## 13. Failure modes

1. **Malformed or missing style file:** use the factory style named in
   its `style` field, else Rock. Banner (error-recovery "warning"),
   logged once. `style_ref` is kept verbatim for the next save.
2. **Chord map larger than 1024 entries:** truncated at the message
   thread with a log line. Anticipation ends at the truncation point.
3. **No play head, or one without ppq:** own clock (8.4 notice).
4. **Sample-rate change or re-prepare:** coefficients rebuilt in
   `prepare`, state reset, band returns to Armed.
5. **CPU relief** (performance-budget 8): a new step between 4 and 5,
   "cymbal banks 48 -> 24 modes, hat 32 -> 16". Bass and timing are
   never degraded.
6. **Oversized host block:** handled by the processor's existing
   slicing. The Jam clock is sample-based, so slices change nothing
   (JM-05).
7. **NaN/Inf in a bank:** guard clamps it and resets that piece. The
   diagnostics counter increments.

### 13.2 Tune jam hints

`.luthiertune` sections may carry `"jam": { "intensity": 1-5,
"fill_into": true }`. This is optional and additive, and old tunes load
unchanged. Coordinate with tune-builder 11.

## 14. Performance budget

New rows for performance-budget.md 1 (48 kHz, 128-sample block):

| Module | Budget (units) | Notes |
|---|---|---|
| JamConductor + ChordFollower | 0.05 | control rate; <= 0.02 when stopped |
| JamDrumKit | 0.9 | worst case, every piece ringing (about 200 modes, SIMD); typically 0.4 |
| JamBassVoice | 0.6 | 2 x StringEngine 0.42 + tone 0.08 + 2x OS saturation 0.05 |
| JamMixer + kit room | 0.15 | 4-line FDN, pans, meters |

Jam total: <= 1.7 worst case, typically about 1.1. New total scenario
**Jam** (Rock preset, 4 voices, band at intensity 5): <= 10 units.
Memory: under 2 MB (banks, 128 KB capture ring, style library).
Style library load: <= 20 ms on the message thread. Added latency: 0
samples (rule 0.6). Reaction latency: drum start in First Note mode is
0 musical samples (same block and sample offset as the note, + L). Bass
change is as in 3.3, and exactly on time when anticipated.

## 15. Edition split (editions.md)

**Free-limited** (a new row in editions 2.3, beside the rhythm engine).
A band to play with is the most direct "play for an hour" hook, and the
rhythm engine it builds on is fundamental.
- **Free:** 4 styles (Rock, Pop, Blues Shuffle, Ballad), both
  variations and all intensities and fills. 2 kits (Studio, Vintage).
  Bass voices Finger and Pick. Every start, stop and follow mode,
  prediction, and following the read-only demo tunes. Main output only.
- **Pro:** the other 6 styles and user `.luthierjam` styles, the Arena,
  Jazz and Machine kits, Muted Pick and Upright, `jam_kit_tuning` and
  `jam_kit_damping` (H8 deep realism), Separate outputs (H7), Jam MIDI
  out, drag-out and export (H5), and Tune anticipation from edited tunes
  (H2).
- Locked entries are marked per editions 4.1. A Pro preset loaded in
  Free plays the nearest Free style and kit, and carries the values
  (editions 5.1).

## 16. New classes and files

`Source/Jam/`: `JamEngine`, `JamConductor`, `JamChordFollower`
(`JamChordMap`), `JamBassLine`, `JamStyle` + `JamStyleLibrary`,
`JamCapture`, `JamStatus`. `Source/DSP/Jam/`: `ModalResonatorBank`,
`DrumPieces` (Membrane, SnareWires, CymbalBank, Rim, PhisemShaker),
`JamDrumKit`, `JamBassVoice` (`JamBassTone`), `KitRoom`. `Source/UI/`:
`JamPanel`, `JamLaneView`, plus a JAM group in `EasyPanel` and a pill in
`LiveStrip`. Tests: `Source/Tests/JamTests.cpp`, `JamDspTests.cpp`,
`JamPanelTests.cpp`. The `processBlock` insertion points are:
1. `jam.handleMidi` after the tune merge (`engine.setDirectMidi` line).
2. The clock in the transport section.
3. `jam.renderBlock` into `jamStems` after `engine.processBlock`.
4. The mix after `looper.processBlock`, before `sessionRecorder`.
5. `routing.distribute` for Aux 9/10.
6. The MIDI out after `midiOutRouter.emit`.

## 17. Tests

Unit (LuthierTests):
- **JM-01** Every factory style parses. It has A/B x intensity 1-5
  grooves, >= 4 fills, an ending, and double- and half-time bars. Every
  lane string length equals its grid.
- **JM-02** Transport lock: host at 120 bpm 4/4 playing from ppq 0.
  Every kick of Rock intensity 3 starts within 1 sample of
  (grid sample + L), with humanise 0, over 64 bars.
- **JM-03** Tempo automation 90 -> 140 bpm over 8 bars. The hit drift
  against the host grid is <= 1 sample at every beat.
- **JM-04** Determinism: two offline renders of the same 30 s MIDI
  fixture with seed S are bit-identical. A different seed differs.
- **JM-05** Block-size independence: renders at blocks 32, 128, 512 and
  2048 (sliced) null to <= -120 dBFS.
- **JM-06** Natural follow: Am held, F played 200 ms after beat 2. The
  bass plays F on beat 3 (+-1 sample + humanise 0), not before.
- **JM-07** Grace window: F played 40 ms after beat 2 at 120 bpm. The
  bass changes at the detection sample (<= 30 ms + 1 block after the
  note).
- **JM-08** Tight / Relaxed / Bar quanta land on the next 8th /
  half-bar / bar for a chord at a random offset (100 trials).
- **JM-09** Single notes and Unknown detections never change the bass
  chord (1000 random single-note phrases).
- **JM-10** Slash chord C/G with style tokens R: the bass plays G.
- **JM-11** Anticipation from a tune: every chord change's bass note
  starts exactly at the change (+L). `A` tokens produce an approach
  note a semitone or diatonic step from the next root.
- **JM-12** Prediction: a 4-bar loop repeated twice turns prediction on
  in cycle 3. A deviating chord in cycle 4 is corrected at the next Q
  and turns prediction off until 2 more clean cycles.
- **JM-13** RhythmEngine driving: the Jam chord equals
  `getCurrentChord()` at every block.
- **JM-14** First Note start: the kick and crash of beat 1 start at the
  note's sample offset + L.
- **JM-15** Tap In: 4 taps at 500 ms put beat 1 at tap4 + 500 ms
  (+-1 ms), tempo 120.
- **JM-16** Count-in: 1 bar of 4 stick hits, then beat 1. With
  count-in 0 there are no sticks.
- **JM-17** Stop on silence (2 bars): the ending starts on the downbeat
  after 2 full silent bars. A held chord does not count as playing.
  Following host transport disables the check.
- **JM-18** Host stop plays the ending (on) or cuts within 20 ms (off).
  Panic chokes all voices to < -90 dBFS within 10 ms and sets `jam_play`
  off.
- **JM-19** Intensity changes at the next beat. Style, variation and kit
  change at the next bar line, never mid-fill.
- **JM-20** Fill Now pressed with more than 1 beat left fills to the bar
  line. With less than 1 beat left, it fills the last 2 beats of the
  next bar.
- **JM-21** Dynamics follow: velocity 40 for 2 bars gives effective
  intensity -1, and 120 gives +1. Clamped at 1 and 5, with hysteresis
  (no flapping at 55 +- 4).
- **JM-22** Unsupported meter 7/8 plays the generic bar. 3/4 Ballad uses
  its own 3/4 groove.
- **JM-23** Host cycle jump from bar 9 to bar 1: the next hit is the
  bar-1 pattern step and nothing is left hanging.

DSP:
- **JM-24** Kick modes: the FFT of a 127-velocity kick has peaks at
  f0 x {1, 1.594, 2.136} +- 2 %. The pitch 5 ms after onset is higher
  than at 150 ms by 10-25 %.
- **JM-25** Snare wires: raising the wire threshold to max removes
  > 90 % of energy in 3-6 kHz after 20 ms. Default wires decay within
  250 ms.
- **JM-26** Hat choke: closed after open drops the hat's 5-10 kHz energy
  by >= 40 dB within 15 ms, without a click (max sample step < 0.05).
- **JM-27** Ride re-strike adds to the ringing state: no discontinuity
  at the second hit.
- **JM-28** `jam_kit_tuning` +12 st (advanced) doubles the kick f0
  +- 1 %. Stock clamps at +-6 st.
- **JM-29** Bass pitch: the fundamental of every note E1-C3 is within
  +-3 cents after 100 ms. Note changes have no click (max step < 0.05).
- **JM-30** Bass string choice: an A1-D2-G2 walk uses one position
  (frets <= 7). The two `StringEngine` instances alternate.
- **JM-31** No sample files are opened: the file-open hook sees zero
  audio-file reads over a 60 s jam.
- **JM-32** Stability: 10 min of intensity 5 at 44.1-192 kHz produces no
  NaN/Inf and DC < -60 dBFS.

Real-time and performance:
- **JM-33** Zero allocations and zero lock acquisitions on the audio
  thread over 5 min of jamming with style, kit and chord-map swaps every
  bar (heap and mutex traps).
- **JM-34** Budgets: JamDrumKit <= 0.9 x 1.10, JamBassVoice <= 0.6 x
  1.10, total Jam <= 1.7. Jam scenario <= 10 units. Jam enabled but
  stopped <= 0.02. Jam off costs 0.

State, routing, MIDI:
- **JM-35** Preset round-trip of all 32 non-transient params and the
  `jam` block is exact. `jam_play` and `jam_fill_now` are never written
  to presets or snapshots and come back off after host state reload.
- **JM-36** Parameter append: the indices of every pre-existing
  parameter are unchanged, and the jam IDs are the last 34 in table
  order.
- **JM-37** Separate output on Layout B: the band is on Aux 9/10 and
  absent from main, and main + separate is on both. On Layout A the
  band is on main. The bus numbers of Aux 1-8 and the per-string buses
  are unchanged.
- **JM-38** The looper records without the band (a looper loop nulls
  against a jam-off render). The session recorder take contains it.
  The kill switch mutes it in 3 ms.
- **JM-39** Jam MIDI out: drums on ch 10 with the GM map, bass on ch 11.
  Note-ons are sample-aligned with the audio onsets (+-1 sample).
- **JM-40** Drag-out of 8 bars gives a valid Type 1 file with 2 tracks,
  the correct tempo map and bar-aligned events that re-import to the
  same notes.
- **JM-41** A malformed `.luthierjam` falls back to its named factory
  style, raises a banner and keeps `style_ref` on save.

Combination:
- **JM-42** Tune playing with the percussion layer on and Jam drums on:
  no chuck-noise percussion events reach the engine. Drums off:
  percussion returns.
- **JM-43** Tune bass line on a guitar instance with Jam bass on:
  `JamBassVoice` plays exactly the tune bass-channel notes, and Jam's
  own line is silent. On a bass instrument, Jam bass is silent.
- **JM-44** The metronome goes silent while the drums are audible (pref
  on) and clicks with the pref off.
- **JM-45** Snapshot recall mid-bar changes style at the next bar and
  never stops the band. A preset load while Playing keeps the band
  running with the new preset's jam params.
- **JM-46** Own clock drives the RhythmEngine: with the host stopped and
  the band playing, strums land on the band's grid within 1 sample.

GUI (xvfb, as in `EditorTests.cpp`):
- **JM-47** The JAM tab sits between TUNE and LIVE, selects and paints
  at 480-1600 px Col 4 widths without clipping. Every control named in
  8.1 is present and focusable in the documented Tab order.
- **JM-48** Easy mode: the JAM pill, style, intensity and Band volume
  exist and drive their parameters. The pill cycles ARMED -> PLAYING ->
  ENDING with the engine state. The Live Strip pill shows only when
  `jam_enabled` is on.
- **JM-49** Shortcuts J, Shift+J and Alt+J act only when no text field
  has focus. They are rebindable and listed in the cheat sheet.
- **JM-50** Every empty-state and error string of 8.4 appears in its
  forced state. Screen-reader labels exist for every control. The lane
  view's accessible description matches the current bar.
- **JM-51** Free build: the locked styles, kits, voices and outputs show
  the lock glyph and the upsell panel, and a Pro preset plays the
  nearest Free style.
