# AUTO ARTICULATION (PERFORMANCE ASSIST) SPEC

Performance Assist is one switch that turns plain MIDI into a guitar
performance without keyswitches. The MIDI can come from a keyboard, a DAW
clip, the piano roll or the Tune melody. Performance Assist picks the
string and position. It turns legato into hammer-ons, pull-offs and
slides. It adds delayed vibrato, pick attack, alternate-picking dynamics,
strum direction and spread, palm-muted chugs, and the occasional grace
bend. A style menu and an Amount control shape all of this.

Added 2026-09-24 at the product owner's request. Additive to
`gui-integration.md`, `engine.md` 2 and 4, `technique-cascade.md` and
`midi-export.md`. It moves nothing that is already placed.

## 0. Ground rules

1. **Off is today.** With `aa_enabled` off, `aa_amount` at 0 %, or
   `aa_rules` at 0, the output is bit-identical to a build without this
   feature: the same events, the same random draws, the same samples.
   Performance Assist has its own counter-based hash. It never draws from
   `MidiInterpreter::rng`, so humanisation stays the same sequence whether
   Performance Assist is on or off.
2. **Explicit always wins.** Anything the player or the file asks for
   explicitly beats an automatic decision on that note. Section 5 lists
   what counts as explicit. This is technique-cascade.md 3 rule 1 applied
   to MIDI inference.
3. **Zero added latency.** Performance Assist looks only at the past and
   at the note in hand. The only look-ahead is the existing Poly chord
   window (`chord_window`, 0-20 ms, default 2 ms), which is already
   reported to the host, and Performance Assist does not change it.
   Turning Performance Assist on or off never changes `getLatencySamples()`.
   Reasons:
   - performance-budget.md 4 caps main-out latency at 128 samples at
     48 kHz, and the chord window already uses 96 of them.
   - A latency change in mid-song makes hosts recompute delay
     compensation (host-integration.md 5).
   - A look-ahead long enough to know a note's length (100-500 ms) could
     never be played live.
   Section 3.6 covers the gap this leaves: a note's future is guessed from
   its context, and a wrong guess is corrected while the note is still
   sounding.
4. **Deterministic and sample-accurate.** Every decision is a pure
   function of four things: the event stream's absolute sample stamps, the
   parameters, the guitar, and the host transport position at those
   samples. Block boundaries, flush timing and thread timing do not enter
   into it. Decisions are identical at every block size and in realtime
   and offline renders. Audio is bit-identical at the same block size.
5. **Harmonics are never automatic.** Performance Assist never emits
   NaturalHarmonic, PinchHarmonic, ArtificialHarmonic, Tap, SlideGuitar
   or MutedPick. It never arms a Techniques-tab technique.
6. **No allocation or locks on the audio thread.** All state is fixed
   arrays sized by `kMaxStrings`. The display feed is a single-producer,
   single-consumer (SPSC) POD ring.
7. **What it did is visible and captured.** Every decision is labelled on
   the fretboard (section 7.3). It also reaches PerformanceCapture,
   notation and MIDI export as the technique that was actually played
   (section 9).

## 1. Purpose and user stories

- **Pianist:** "I play a melody on my keyboard and it sounds like a
  guitarist played it: slurs where I play legato, and vibrato on the
  long notes."
- **Producer:** "I drop a MIDI clip from a pack onto the track, pick
  'Rock', and the chords strum down and up on the beat while the low
  eighths chug palm-muted."
- **Songwriter:** "The melody I drew in the TUNE tab plays with slides
  and bends, and the TAB export shows them."
- **Guitarist with a keyboard controller:** "I switch it off and get
  exactly what I had yesterday. When I hold my palm-mute pedal, the pedal
  is what counts."
- **Screen-reader user:** "I can find out what it just did to my
  phrase."

## 2. Styles and Amount

`aa_style` selects a row of constants. `aa_amount` (a = 0..1) scales that
row:
- Time windows are multiplied by `(0.5 + a)`.
- Depths and intensities are multiplied by `(0.5 + a)`.
- Probabilities are multiplied by `min(1, 2a)`.

The table values are the values at a = 0.5. At a = 0, no rule runs (rule
0.1).

| Style | Legato max IOI / max interval | Slide min overlap / max interval | Vibrato delay / depth / rate | Accent velocity | Palm mute: register / depth | Strum: crossing / up-strokes | Ornament: probability / kind | Open-string bias |
|---|---|---|---|---|---|---|---|---|
| Clean/Pop | 250 ms / 3 st | 40 ms / 2 st | 350 ms / 12 c / 5.2 Hz | 105 | off | 220 sps / on | 0 | +1 |
| Blues | 400 / 3 | 25 / 4 | 220 / 30 / 5.5 | 100 | off | 180 / on | 0.30 / bend-into 1-2 st; fall 0.20 | 0 |
| Rock | 300 / 4 | 35 / 3 | 260 / 25 / 5.8 | 96 | 7 st / 0.55 | 260 / on | 0.15 / bend-into; fall 0.10 | 0 |
| Metal | 220 / 5 | 50 / 2 | 200 / 35 / 6.5 | 90 | 10 st / 0.75 | 400 / off (downs only) | 0.10 / bend-into | lowest string +2, others -1 |
| Jazz | 350 / 3 | 30 / 2 | 400 / 8 / 4.8 | 110 | off | 600 / off | 0.15 / slide-in 1 fret | -2 |
| Country | 260 / 3 | 30 / 5 | 380 / 10 / 5.5 | 100 | 5 st / 0.35 | 240 / on | 0.25 / bend-into 2 st; fall 0.15 | +1 |
| Fingerstyle | 350 / 3 | 40 / 2 | 400 / 10 / 5.0 | 108 | off | pinch-roll 15 ms | 0 | +2 |
| Bass | 250 / 5 | 40 / 5 | 450 / 8 / 4.8 | 105 | off | none (together) | 0.10 / slide-in 2 frets | +1 |

Notes on the columns:
- **IOI** is the time between the two notes' onsets (inter-onset
  interval).
- **st** is semitones, **c** is cents.
- **Accent velocity** is a MIDI velocity, 1-127.
- **Palm-mute register** is measured in semitones above the lowest open
  string, capo included.
- **Up-strokes on** means section 3.7 alternates strum direction. **Off**
  means every strum is a downstroke.

Each style also sets a legato chain cap, the number of consecutive slurs
before the next note is re-picked:
- Metal: 6.
- Rock and Blues: 4.
- All other styles: 3.

Each row lives as a `constexpr AutoArticulationStyle` in
`AutoArticulationStyles.cpp`. The unit tests pin the tuning, so changing
any number is a spec change.

The styles work on any guitar family:
- **Bass style on a guitar:** no strums, and slide-in ornaments.
- **Guitar style on a bass:** the strum rule plays chords together, and
  vibrato depth is halved.

Factory bass presets that enable Performance Assist use the Bass style.

## 3. Rules (exact behaviour)

Each rule has one bit in `aa_rules`: position 0, legato 1, slide 2,
vibrato 3, attack 4, alternate 5, strum 6, ornament 7, palm mute 8.

Rules run in that order for each voiced note, inside the note's own
decision, stamped at its arrival sample. "Held" and "released" are always
judged from the absolute sample stamps of note-ons and note-offs
(`AutoArticulator::History`, per string), never from the interpreter's
flush-time slot state. Section 4.2 explains why.

### 3.1 Position (Mono, and single notes in Poly)

A hand tracker keeps the hand position `H`: the index-finger fret,
measured from the capo. The box is `[H, H + 4]`.

For each string where the note is playable (`TuningEngine::
frequencyToFretPosition`, from 0 to `getHighestPlayableFret`), the
candidate's cost is the sum of:

| Term | Cost |
|---|---|
| Frets outside the box (0 for an open string) | +3 per fret |
| Strings skipped, beyond an adjacent move from the previous note's string | +1 per string |
| Same string as the previous note, when that note is legato-eligible (3.2) | -2 |
| Open-string bias (style table) | minus the bias, for an open string |
| String is held by another sounding note | +4 |
| Fret above 12 | +1 |

The candidate with the lowest cost wins. Ties go to the lower fret, then
to the thicker string (the higher string index).

When the chosen fret is outside the box, `H` moves to `fret - 1`,
clamped. Open strings never move `H`.

For chords, `RubricVoicer::setPreferredPosition (H)` runs before
`voice()`. `H` then takes the voicing's hand position, so the melody and
the chords share a position.

**Late join.** A Poly note that arrives after the chord window has closed,
but no more than 40 ms after the group's first note (60 ms in
Fingerstyle) while the group is still held, is the player's roll. It is
placed on a free string, and adjacent strings get a -1 bonus. It sounds
at its own arrival, as strum-dynamics 1.1 "played spread" does.

### 3.2 Legato: hammer-on and pull-off

The new note N is on the same string as the previous note P on that
string, and all of the following hold:
- P is still held at N's arrival, or P was released no more than 15 ms
  before it.
- The IOI is no more than the style's legato max IOI.
- The interval is 1 semitone or more and no more than the style's legato
  max interval.
- N's velocity is no more than P's velocity + 12/127. A harder note is
  re-picked.
- The legato chain cap has not been reached.

Then N is a HammerOn if it is higher and a PullOff if it is lower. A
pull-off to an open string is allowed.

A note-off from P that arrives after N's arrival but before N is flushed
(inside the chord window) is deferred to N's flush sample. N takes the
string over, and no release is heard between the two.

Fretless instruments keep today's rule: legato becomes Slide.

### 3.3 Slide

The same conditions as 3.2 apply, and in addition:
- P is still held for at least the style's slide minimum overlap past
  N's arrival.
- The interval is no more than the style's slide max interval.

Then N is a `Technique::Slide` with `slideFromFret = P.fret`. The slide
time comes from `TechniqueEngine::slideDurationFor`.

A 4-7 st same-string move that meets the overlap condition is a position
shift, also a Slide. Slide takes precedence over 3.2.

### 3.4 Delayed vibrato

A note gets vibrato when all of the following hold:
- It has been held for the style's vibrato delay.
- It is a lead note: no more than 2 strings are held, and it is the
  highest held note.
- It is not PalmMute.
- No explicit pitch gesture is present (section 5).

The vibrato ramps in over 300 ms to the style's depth, at the style's
rate. The rate varies by a deterministic ±4 % per note, taken from the
hash.

It rides the existing per-string vibrato path
(`LuthierEngine::updatePerBlockModulation`). The per-string target
becomes `max(ccTarget, autoVibratoCents[s])`. It ends at release. When
Performance Assist is switched off mid-note, the vibrato ramps out over
150 ms.

### 3.5 Attack

These change `Excitation::Params` in `triggerNote`:
- **Accent:** velocity at or above the accent velocity. `brightness`
  ×(1 + 0.25k) and `noiseAmount` ×(1 + 0.8k), where k is the depth
  scale. The note is captured with an accent mark.
- **Soft:** velocity below 40/127. `brightness` ×0.85 and `noiseAmount`
  ×0.5.

Velocity itself is never changed by this rule.

### 3.6 Palm mute

Styles that have a palm-mute register use this rule. The note, or every
note of a Poly chord of up to 3 notes, must lie within that register. The
note is then PalmMute, with a per-note amount of style depth × (0.5 + a),
when one of the following holds:
- **Chug context:** the previous note in the register lasted no more
  than 65 % of its IOI, and the IOI was no more than 300 ms (350 ms in
  Metal).
- **Repeated pitch:** the note repeats the previous pitch, with an IOI of
  no more than 250 ms.
- **Metal only:** a first note at velocity 70 or above, no more than
  5 semitones above the lowest open string.

**Mute lift** corrects a wrong guess. An auto-muted note that is still
held after `max(1.5 × IOI_ref, 220 ms)` ramps to Open damping over 60 ms,
as a palm lifting off. The lift is a scheduled engine event at an
absolute sample.

### 3.7 Alternate picking and strum

**Single notes.** Consecutive picked notes (Pluck or PalmMute) with an
IOI of no more than 200 ms (220 ms in Metal) alternate between down and
up.

How the direction is chosen:
- **Transport running:** an even 16th-grid position is down and an odd
  one is up. That is strict alternate picking.
- **Transport stopped:** the direction toggles, and resets to down after
  a gap of more than 300 ms.

What an up-stroke changes:
- Velocity ×(1 - 0.08k).
- Brightness ×0.9.
- Pick noise ×0.8.

On a bass, when the player uses fingers, `BassFingerstyle` already
alternates, and this rule does nothing.

**Chords** (at least 2 voiced notes that arrived together, and not played
spread). Performance Assist fills the existing `StrumRequest` before
`StrumGesture::plan`, and nothing else in the strum path changes.

Direction:
- **Transport running:** the style's grid applies. Pop, Rock, Blues and
  Country use 8ths, and Blues swings them by the host's position. On the
  beat is down, off the beat is up. Metal and Jazz are always down.
- **Transport stopped:** the strum is down after 450 ms or more of
  silence. Otherwise it is the opposite of the last strum.

Speed: `sourceSps = style crossing × (0.75 + 0.5 v)`, where v is the
group's peak velocity. A louder strum is a faster strum.

Up-strokes: each string's force is scaled by `1 - 0.35 × (lowness)`,
where lowness is 0 for the highest struck string and 1 for the lowest.
Every note the player pressed still sounds (`missScale = 0`).

Style exceptions:
- **Fingerstyle:** the lowest note sounds at 0 ms and the others ascend
  across 15 ms (thumb, then fingers).
- **Bass:** double-stops sound together.

### 3.8 Ornaments (lead styles, single notes only)

**Bend-into.** A note qualifies when all of the following hold:
- It starts a phrase: nothing has been held for 250 ms or more. In
  place of that, with the transport running, it may land on a beat after
  an ascending step of 2 semitones or less.
- Its velocity is 80 or above.
- Its fret is k + 1 or higher, where k is 1, or 2 if the approach was
  2 semitones or more up. Bends above k = 1 are allowed only on the
  3 highest strings of a guitar.
- `hash(noteOrdinal, midiNote) < probability`.

If so, the note is played at `fret - k`, and an auto pitch curve takes it
from 0 to +k × 100 cents. The rise takes 110 ms in Blues, 90 ms in Rock,
70 ms in Metal and 140 ms in Country.

**Slide-in** (Jazz and Bass). Instead of a bend, the note is a Pluck with
`slideFromFret = fret - 1` (Bass: `fret - 2`). The slide takes 45 ms
(Bass: 60 ms).

**Fall.** A lead note qualifies when:
- It was held for 500 ms or more.
- At its note-off, nothing else is held.
- No note-on shares its sample.
- The hash falls under the style's fall probability.

Its release is deferred by 120 ms, reusing `StringSlot::releaseDueAt`.
During that 120 ms the pitch glides down 2 semitones (1 in Country),
then the string releases.

## 4. Engine design and insertion points

### 4.1 New files

These are the new files. Each one is added to the plugin target and to
`LuthierTests`:

| File | Contents |
|---|---|
| `Source/Model/Playing/AutoArticulator.h/.cpp` | Owned by `MidiInterpreter`, like `strumGesture`. Holds `History[kMaxStrings]`, the hand tracker, `ExplicitContext`, the hash, and the per-string auto pitch and vibrato curves. `prepare (sr, numStrings)`, `reset()` (called from `MidiInterpreter::reset`, so on transport start, preset load and panic). |
| `Source/Model/Playing/AutoArticulationStyles.h/.cpp` | The style table from section 2. |
| `Source/Model/Playing/AutoArticulationFeed.h` | A 128-entry SPSC ring of `{ int64 sample; int8 string; float fret; uint8 label; uint8 rule; }` for the UI. |

The `AutoArticulator` API:
- `setSettings (const AutoArticulationSettings&)`: enabled, style,
  amount, rules.
- `setExplicitContext (const ExplicitContext&)`.
- `planSingle (...)`, which returns a `VoicedNote`.
- `planStrum (StrumRequest&, ...)`.
- `decorate (NoteOnEvent&, Technique decided, bool explicitTech, int64 arrival)`.
- `onNoteOff (...)`, which returns the extra delay for a fall.
- `autoPitchCents (s, int64 sample)`.
- `autoVibratoCents (s, int64 sample)`.

### 4.2 Changes to existing code

**`PlayingEvents.h`.** `NoteOnEvent` gains these fields. The defaults
leave today's behaviour alone:
- `uint16 autoRules = 0`
- `double attackBrightnessScale = 1.0`
- `double attackNoiseScale = 1.0`
- `double palmMuteAmount = -1.0` (-1 means use the controller amount)
- `bool upStroke = false`
- `int autoOrnament = 0`
- `int64 arrivalSample`

**`TechniqueEngine`.**
- `setLegatoInferenceEnabled (bool)`. When it is false, `decide` skips
  its own legato and slide inference. `MidiInterpreter` sets it to false
  only while Performance Assist is effective and rule 1 or 2 is on, so
  today's inference comes back in every other case.
- `decide` gains `bool& explicitOut`, which is true when a controller
  trigger chose the technique (the first seven branches).

**`MidiInterpreter`.**
- `handleNoteOn` (Mono) and `flushChordGroup` (single note and late join)
  call `planSingle` instead of `voicer->voiceSingleNote`.
- `flushChordGroup` calls `planStrum` before `strumGesture.plan`.
- `emitVoicedNote` calls `decorate` after `technique->decide`.
- `handleNoteOff` applies the legato hand-over (3.2) and the fall (3.8).

Both paths into `processBlock` run through these hooks: the host MIDI and
the direct notes (Tune melody).

Held state comes from the stamps because the interpreter flushes a Poly
group one chord window after the notes arrive. By then, a note-off that
came 1 ms after the new note has already been handled. Judging from slot
state would therefore depend on the chord window and the block size.

**`LuthierEngine`.**
- Before `midi.processBlock`, it fills `ExplicitContext` from
  `techniqueTriggers`, `slap`, the scrape, the mute grid, the Tap arm
  and the slide mode.
- `triggerNote` applies the four new `NoteOnEvent` fields after the
  technique `switch` on `Excitation::Params`. When `palmMuteAmount >= 0`,
  it replaces `technique.getPalmMuteAmount()`.
- `triggerNote` pushes the feed and the capture marks (section 9).
- `ScheduledEvent` gains `kind = dampingLift` for the mute lift (3.6).
- `updatePerBlockModulation` adds `autoPitchCents` and uses the
  vibrato `max` (3.4). The curves are evaluated at the block's end sample
  from absolute time, so they have the same smoothness as a played MIDI
  bend.

**`ParameterBridge`.** It reads the four parameters (section 6) through
the fast table and calls `interp.setAutoArticulation (settings)`. In
Free, any style or rule that is not in Free is resolved into its
effective value (section 11) on the message thread, as editions.md 7.5
requires.

**`MidiImportTargets` and `PerformanceCapture`.** Section 9.

## 5. What counts as explicit

Each row says what Performance Assist does when the condition holds:

| Condition | What Performance Assist does |
|---|---|
| Guitar Controller playing mode, or MPE on | Bypassed entirely. The notice reads "Off in Guitar Controller / MPE mode: your controller already articulates." |
| Rhythm engine driving | Its events replace the interpreter's (`rhythm.isDriving()`), so they are never assisted. Direct notes (the Tune melody) still are. |
| A controller-triggered technique (palm-mute CC > 0.05, muted pick, pinch, harmonic trigger or velocity trigger, tap trigger, slide CC, Slide Mode) | That technique is kept. Only the attack and alternate rules may add dynamics, and they do not when the technique is a harmonic or a tap. |
| Slap held or armed velocity zone hit, a scrape active on the string, or Tap armed (two-hand-tapping.md 5 legato promotion) | No rule runs on that string or note. |
| Mute grid active (muting-rhythm.md) | The palm-mute rule is off. |
| Mod-wheel or aftertouch vibrato above 0.02, or pitch bend nonzero on the string in the last 100 ms | The vibrato and ornament rules are off on that string. |
| Strum CC 76/77 moved in the last 2 s | That direction and speed win over 3.7. |
| Notes from a Luthier-profile import (NOTE events carry techniques) | Treated as pre-articulated. The import player sends them with `aa=pre` and `decorate` is skipped, so a round trip does not articulate twice. |

## 6. Parameters

All four are appended at the end of the parameter list, after
`aux_1_pre_circuit` or after whatever is last at merge time. They are
never inserted.

None of them is physical, so there is no `PhysicalRange` and no advanced
range.

| ID | Name | Range | Default |
|---|---|---|---|
| `aa_enabled` | Performance Assist | bool | off |
| `aa_style` | Assist Style | Clean/Pop, Blues, Rock, Metal, Jazz, Country, Fingerstyle, Bass | Clean/Pop |
| `aa_amount` | Assist Amount | 0-100 % | 60 % |
| `aa_rules` | Assist Rules | int 0-511 (bitmask, section 3) | 511 |

All four are automatable. A parameter change takes effect at the next
note-on, at its sample. Effects already running (vibrato, lift, fall)
finish or ramp out (3.4).

In snapshot morphs, `aa_enabled`, `aa_style` and `aa_rules` switch at the
50 % point, and `aa_amount` interpolates.

## 7. UI

### 7.1 Easy mode: Playing strip (gui-integration.md 3.3)

The mode column (130 px) becomes two rows:

```
+--------------+----------------------------------------------------+
| [Mode    v]  | Attack Body Drive Tone Space Humanize Character ...|
| [AUTO•][Rock v]                                                   |
+--------------+----------------------------------------------------+
```

- **AUTO pill** (56 × 22):
  - Filled with the primary accent when on, outlined when off.
  - The state dot flashes for 120 ms on every automatic decision
    (gui-techniques-updates.md 2 convention).
  - Tap toggles it.
  - Hold, or Enter + Down, opens a popover with the Amount knob, the
    style's one-line description and a "More in RHYTHM tab" link.
- **Style combo** (70 px): the 8 styles. Styles that are locked in Free
  show a padlock.

### 7.2 Advanced mode: Col 4 RHYTHM tab, new first group PLAYING

Decision: the group sits in the RHYTHM tab and not the TECHNIQUES tab,
for three reasons:
- RHYTHM already holds the MIDI-to-performance controls (voicer, strum).
- `RhythmPanel` already reads `playing_mode`.
- TECHNIQUES is Pro (H3), and Performance Assist is fundamental.

The PLAYING group sits above the rhythm-engine enable row. It is a new
component, `Source/UI/PerformanceAssistGroup.h/.cpp`, placed by
`RhythmPanel::resized`:

```
PLAYING ─────────────────────────── ON · Rock · 60%   [v]
[Mode: Poly v]  [x] Performance Assist  [Style: Rock v]  (Amount)
Rules: [x]Positions [x]Legato [x]Slides [x]Vibrato [x]Attack
       [x]Alt picking [x]Strums [x]Ornaments [x]Palm mute
Recent: 12.4s  str 3 fr 7  Hammer-on   (list, last 16, newest first)
(notice line: bypass reason from section 5, or empty)
```

- The Playing mode control here is a mirror. The Easy strip stays
  canonical.
- The rule checkboxes are one `AttachedSwitch` per bit, through a small
  bitmask adapter over `aa_rules`.
- The group collapses per ground rule 6. Collapsed, the status line
  still shows the state.

When the TECHNIQUES tab is built, its CASCADE view shows a read-only
"Auto" row with the same bypass notice.

### 7.3 "Show what it did" overlay

Labels appear on `FretboardComponent`, and in Easy mode on the fretboard
of `GuitarBodyComponent`, at the note's string and fret. They sit in the
live overlay layer (guitar-illustration.md 5, layer 33+).

| Decision | Label |
|---|---|
| Hammer-on | `H` |
| Pull-off | `P` |
| Slide up / down | `/` / `\` |
| Vibrato, when it starts | `~` |
| Palm mute | `PM` |
| Up-stroke (shown only on up-strokes) | `↑` |
| Accent | `>` |
| Bend-into | `b½` / `b1` |
| Fall | `↘` |
| Strum | A vertical arrow spanning the struck strings, at the nut side |

How the labels are drawn:
- 10 pt text on a pill in the string's colour, at 0.9 opacity.
- Each label fades over 600 ms.
- Under reduced motion there is no fade: the label shows for 600 ms and
  then goes.
- The text glyph means the labels still read in monochrome palettes.

Data flow, in gui-engine-dataflow.md style:
- **Source:** `AutoArticulationFeed`, pushed from `triggerNote`, from the
  lift and from the vibrato start.
- **UI drain:** the existing 30 Hz fretboard timer.
- **Staleness:** entries older than 600 ms are dropped unread.

### 7.4 Options

The Visual aids section (piano-roll-chord-display.md 5) gains **Show
Performance Assist labels**, default on. It is a UiPreferences entry, not
a parameter and not preset data. With it off, the feed is still drained
so the decision list works, but nothing is drawn on the fretboard.

### 7.5 Shortcut, help, empty states, errors

- **Shortcut:** `A` toggles `aa_enabled`. It is rebindable and added to
  gui-integration.md 17. The piano-roll strip's A key only applies while
  that strip has focus.
- **Help:** a HelpContent topic, "performance-assist", with a `?` on the
  group.
- **Empty decision list:** "Play something: what Performance Assist
  decides appears here."
- **Style locked in Free:** the upsell panel (editions.md 4.2).
- **Errors:** there are no error states. Every input is valid.

### 7.6 Feature-to-location index row (gui-integration.md 19)

| Feature | Backend | Primary | Secondary | Shortcut |
|---|---|---|---|---|
| Performance Assist (auto articulation) | AutoArticulator | Col 4 RHYTHM -> PLAYING | Easy playing strip AUTO pill + style | A |

## 8. State, undo, accessibility

**State.** The four parameters are saved in the preset's `parameters`
block, in snapshots, and in host state. No schema bump is needed. An
older preset has no `aa_*` keys, so it loads with the defaults (off),
which is the old sound.

**UI state.** `UiState` gains `playingGroupCollapsed`. The decision list
is session state and is not saved.

**Undo** follows action-and-undo.md:
- The toggle and the rule checkboxes are 3.3 entries: "Turn on
  Performance Assist", "Turn off Slides rule".
- Style is a 3.2 entry.
- Amount is a 3.1 entry, merged within 200 ms.
- Decisions are audio events and are never undoable (7).

**Accessibility.**
- The AUTO pill announces "Performance Assist, on, style Rock, amount 60
  percent".
- Each rule checkbox is labelled with the rule's name and a
  one-sentence description.
- The decision list is an accessible list. Its rows read "12.4 seconds,
  string 3 fret 7, hammer-on". Nothing is announced automatically,
  because that would be too chatty.
- Tab order: mode, toggle, style, amount, rules in reading order, list.
- The `A` shortcut appears in the shortcuts overlay.

## 9. Capture, notation and MIDI export

`PerformanceCapture` already receives `e.technique` from `triggerNote`,
so automatic hammer-ons, pull-offs and slides are captured with no
change. The following are added:
- `mark (palmMute, amount)` for an auto palm mute.
- `mark (vibrato, rate, depth)` when the vibrato starts.
- `mark (accent)`.
- A bend-into is captured at `fret - k`, with `bend()` points following
  the curve. Notation reads it as "fret 5, bend ½".
- A fall is captured as `mark (slideOut)`, and a slide-in as
  `mark (slideIn)`.
- A pick stroke is captured as `mark (pickStrokeUp)` or
  `mark (pickStrokeDown)`. These are two new `ScoreTechnique::Type`
  values, appended before `numTypes`. They map to MusicXML
  `<up-bow>` / `<down-bow>` and to Guitar Pro pickstroke.

`CapturedNote` gains `uint16 autoRules`. The NOTATION tab and the live
TAB view draw automatic techniques in the secondary accent.

**Luthier profile.** The NOTE event gains the tagged field `aa=<hex
mask>`, written only when the mask is nonzero. Older readers keep it
under midi-export 2.2. On import, NOTE events are pre-articulated
(section 5), so the -60 dBFS round-trip null of midi-export 2.2 still
holds with Performance Assist on.

**Generic profile.** It writes the notes and pitch bend as performed,
with the usual `LUTHIER:` text metas.

**Live MIDI out.** It carries the performed strings and strum times
through the string-activity stream, which it already does.

## 10. Performance budget

| Item | Budget |
|---|---|
| `AutoArticulator` (control-rate, per note-on) | 0.02 units; 3 µs per note-on worst case (12 strings × 1 candidate each, plus the strum plan) |
| Per block | O(numStrings): the curve evaluation |
| Memory | Less than 4 KB, all of it in the interpreter object |
| Latency | +0 samples |
| UI overlay | Inside the existing 2 ms live-overlay budget |
| Decision list | Repaints only when the feed delivered, at 30 Hz at most |

The idle cost with Performance Assist off is one branch per note-on.

## 11. Editions

| Edition | What it gets |
|---|---|
| Both | All nine rules run. On/off, Amount, the overlay, the decision list and the shortcut. |
| Free | Styles Clean/Pop, Rock, Fingerstyle and Bass. `aa_rules` is non-automatable, with the " (Pro)" name suffix, and its effective value is always 511. |
| Pro | Blues, Metal, Jazz and Country, and the per-rule checkboxes (H3-adjacent genre specialisation). |

A Pro preset loaded in Free plays a Pro style as its nearest Free style:
- Blues and Metal play as Rock.
- Jazz and Country play as Clean/Pop.

The original value is written back unchanged (editions.md 5.1). The
banner is shown once.

Reason: slurs, vibrato and strumming are "basic techniques from MIDI",
which are fundamental in both editions (editions.md 2.1). The
genre-specialist styles are a reason to pay.

## 12. Interactions

- **Humanize.** Humanisation applies after Performance Assist and is
  unchanged: its jitter moves times, and the strum speed variation still
  multiplies `sourceSps`.
- **Strum dynamics.** The strum rule only fills `StrumRequest`. Strikers,
  chucks, acceleration and tilt stay as strum-dynamics.md sets them.
- **Rhythm engine.** When it is driving, its events are never assisted
  (section 5).
- **Tune builder.** The melody and bass (direct notes) are assisted, and
  playback and export renders match. Coordination: Tune export's
  notation and MIDI write the written notes (TuneMidi). An "As performed"
  option that exports from an offline capture is proposed to
  tune-builder.md.
- **Piano roll** (piano-roll-chord-display.md):
  - Its keys are ordinary MIDI, so they are assisted.
  - The lit keys show the sounding pitch, including the bend tick.
  - "Show fingering" ghost dots must use `planSingle` for single notes
    and `H` for chords (coordinate PR-05).
- **Techniques and the cascade.** Automatic decisions are the lowest
  priority in technique-cascade.md 3, which already says so. Mute and
  bend add as usual.
- **Slide Mode.** The legato, slide and ornament rules are off. The
  vibrato rule drives bar vibrato through the existing slide path.
- **Snapshots, presets, setlists and host automation.** These act through
  parameters (section 6). A preset load calls `reset()`, which clears the
  history and the hand position.
- **Character and tuning.** Positions respect the capo, tuning and
  12-string courses exactly as `RubricVoicer` does.

## 13. Failure modes

| Case | Response |
|---|---|
| The note is not playable on any string | Falls back to `voicer->voiceSingleNote`, as today. No decoration. |
| Queue overflow | The existing `PlayEventQueue` overflow handling applies. Performance Assist adds no events, only fields; the lift and the fall use the schedule's existing overflow rule. |
| Contradictory context, for example legato to a string now held by a chord | The rule does not fire; the note is a Pluck. |
| A wrong palm-mute guess | Handled by the lift (3.6). |
| A wrong fall guess (a note arrives during the fall) | The new note starts normally on its own string. If it is on the same string, it cancels the fall glide and becomes legato from the current pitch. |

## 14. Tests

The test prefix is AA. The unit tests go in
`Source/Tests/AutoArticulationTests.cpp`, and the GUI tests go in
`Source/Tests/AutoArticulationUiTests.cpp` (run under xvfb like
`EditorTests.cpp`). The combination tests go in `CombinationTests.cpp`.
All of them are in the `LuthierTests` target.

1. **AA-01:** `aa_enabled` off: rendering the 8 ComboHarness phrases
   gives bit-identical audio and an identical event list compared with
   the golden renders from before this feature. The same holds for
   Amount 0 % and for `aa_rules` = 0.
2. **AA-02:** Performance Assist on draws nothing from
   `MidiInterpreter::rng`. The humanisation draw count and values are
   identical with it on and off.
3. **AA-03:** `getLatencySamples()` is identical with Performance Assist
   off and on, at chord window 0, 2 and 20 ms.
4. **AA-04:** Clean/Pop, Mono: C4-D4 with a 10 ms overlap and an IOI of
   150 ms sounds D4 as a HammerOn on C4's string. D4-C4 sounds as a
   PullOff.
5. **AA-05:** The same pair with a 60 ms overlap is a Slide with
   `slideFromFret` equal to C4's fret. With a 150 ms gap it is a Pluck.
6. **AA-06:** A pair where the second note's velocity is 30 or more
   higher is re-picked (Pluck).
7. **AA-07:** Poly, chord window 2 ms: a legato source note-off arriving
   1 ms after the destination's arrival produces no NoteOff before the
   HammerOn (3.2 hand-over).
8. **AA-08:** The legato chain cap: 8 overlapping ascending notes in
   Clean/Pop give exactly 3 slurs, then a Pluck, then 3 slurs, then a
   Pluck.
9. **AA-09:** Position: the scale A3-A4 in Mono stays within a 5-fret box
   (the maximum fret minus the minimum fret of the fretted notes is 4 or
   less), and the hand shifts at most once.
10. **AA-10:** Position ties resolve by lower fret, then thicker string.
    The same input over 100 runs gives the same strings.
11. **AA-11:** A note held 800 ms in Blues gets `autoVibratoCents` of 0
    before 220 ms, and reaches 30 c × depth scale ±1 c by 520 ms. With
    CC1 at 64 the auto value is 0.
12. **AA-12:** In a 4-note chord held 2 s, no string gets auto vibrato.
13. **AA-13:** Velocity 120 in Rock gives an excitation brightness 1.25k
    above the same note at velocity 90. The note's velocity is
    unchanged.
14. **AA-14:** Rock: E2 16ths at 120 bpm, 60 % duty. Notes 2 and later
    are PalmMute with amount 0.55 × (0.5 + a). In Jazz none are.
15. **AA-15:** Mute lift: an auto-muted note held 1 s changes to Open
    damping at `max(1.5 × IOI, 220 ms)` ±1 sample, with the ramp
    finished within 60 ms.
16. **AA-16:** Repeated A4 16ths at 120 bpm, transport running, give
    strokes D U D U from bar 1 beat 1. Transport stopped, a gap of more
    than 300 ms resets the next stroke to D.
17. **AA-17:** A simultaneous chord on beat 1 is a downstroke and the
    one on the "and" of 1 is an up-stroke. The crossing time matches the
    style's sps × (0.75 + 0.5v) to ±1 sample. All pressed notes sound.
18. **AA-18:** Metal: every strum is a downstroke. Fingerstyle: the
    lowest note comes first and the others are within 15 ms, in
    ascending order.
19. **AA-19:** Played-spread chords (a 12 ms roll with a 20 ms window)
    keep their own timing. Strum CC77 moved gives the CC's direction.
20. **AA-20:** Late join: a note 30 ms after a held 3-note chord lands on
    a free string and sounds at its own arrival sample.
21. **AA-21:** Bend-into: with probability forced to 1, the note sounds
    at `fret - k`. The pitch reaches the target ±5 c within the style's
    rise time plus one block. The capture shows a bend of +k.
22. **AA-22:** A fall defers the release by 120 ms ±1 sample and lowers
    the pitch by 2 st ±10 c. A note-on on another string during the fall
    does not cancel it; one on the same string does.
23. **AA-23:** Over 10 000 random phrases in all styles, no emitted
    technique is a harmonic, Tap, SlideGuitar or MutedPick.
24. **AA-24:** Explicit wins: with the palm-mute CC held, a note keeps
    PalmMute at the CC amount, and neither the legato nor the slide rule
    changes it. Pinch trigger: no rule decorates.
25. **AA-25:** Guitar Controller mode and MPE: the event list is
    identical with Performance Assist on and off.
26. **AA-26:** Rhythm engine driving: its event list is identical with
    Performance Assist on and off. Tune direct melody notes are assisted.
27. **AA-27:** Slap armed with a velocity zone, and Tap armed: those
    notes are not decorated.
28. **AA-28:** Block-size independence: the same MIDI at block sizes 32,
    64, 256, 512, 1024 and 2048, and split host blocks, gives an
    identical decision feed (samples, strings, labels).
29. **AA-29:** Realtime versus offline render of a 30 s phrase set gives
    bit-identical audio at the same block size.
30. **AA-30:** No allocation and no lock in `processBlock` with
    Performance Assist on under the heap hook (performance-budget.md 0.4),
    across all styles.
31. **AA-31:** Cost: a 16-voice stress MIDI stays within +0.02 units of
    the Performance Assist off measurement (performance-budget.md 9).
32. **AA-32:** Capture: an assisted phrase exports to MusicXML with
    hammer-on, pull-off, slide, palm-mute, vibrato, bend, accent and
    up-bow elements on the right notes. A Luthier MIDI export carries
    `aa=` on the assisted NOTEs.
33. **AA-33:** Round trip: a Luthier-profile export, re-imported with
    Performance Assist on, renders within -60 dBFS RMS null of the source
    (no double articulation).
34. **AA-34:** Parameters: the four IDs exist, are the last four in the
    layout, and keep their order. Every earlier parameter index is
    unchanged against the checked-in index list.
35. **AA-35:** Preset, snapshot and host-state round trip keeps all four
    values. A preset without `aa_*` loads as off.
36. **AA-36:** Free build: Metal in a preset plays as Rock (the decision
    feed equals Rock's), and saving writes Metal back unchanged.
    `aa_rules` is non-automatable.
37. **AA-37 (GUI):** The Easy AUTO pill toggles `aa_enabled`, the style
    combo sets `aa_style`, and hold opens the Amount popover. Its dot
    flashes within 2 UI frames of a decision.
38. **AA-38 (GUI):** The Advanced RHYTHM tab shows the PLAYING group
    first. Each rule checkbox flips exactly its bit. The status line
    reads "ON · Rock · 60%". The section 5 notices appear in Guitar
    Controller mode, MPE mode and when the rhythm engine is driving.
39. **AA-39 (GUI):** An `H` label is drawn at the hammer-on's string and
    fret within 2 frames and is gone by 700 ms. With reduced motion there
    are no intermediate opacities. With the option off, nothing is drawn
    but the list still fills.
40. **AA-40 (GUI):** Accessibility: every control is reachable by Tab in
    the stated order and has a screen-reader label. `A` toggles the
    feature. The decision list rows read as specified.
41. **AA-41 (undo):** Toggle, style, amount drag and rule checkbox each
    make exactly one undo entry with the stated description, and undo
    restores the value. Played notes make none.
42. **AA-42 (combination):** Performance Assist on, crossed with Slide
    Mode, feedback, the E-Bow, whammy, capo 5, drop D, a 12-string and a
    5-string bass, for all 8 styles, rendering all ComboHarness phrases:
    output is finite, bounded and present, and no note sounds outside its
    string's range.
