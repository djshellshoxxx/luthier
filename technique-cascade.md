# TECHNIQUE CASCADE SPEC

Engine sub-spec. Rules for combining techniques (scraping, slide,
slap, muting, tapping, microtonal bends) on the same or different
strings in the same time window.

Without this, every "does slap + slide work" question is a guess.
This file names the answer.

## 0. Ground rules

1. **Every technique has a compatibility class**: pitch controller,
   damping controller, excitation source, or effect layer. Two
   techniques can coexist per string only if their classes and
   physical constraints allow.
2. **The player can always cascade compatible techniques.** The
   engine never refuses a combination that's physically valid.
3. **Incompatible combinations resolve deterministically.** Priority
   table below decides who wins; nothing crashes or produces
   surprise behaviour.
4. **Cross-string combinations are always compatible.** Slap on
   the low E while sliding on the high E: no problem.
5. **UI mirrors compatibility.** Techniques tab shows greyed-out
   or "conflict" indicators when the user tries to combine
   incompatible items on the same string.

## 1. Technique class table

| Technique | Compatibility class(es) | Notes |
|---|---|---|
| String scraping | Excitation source | Impulsive events into string |
| Slide (bar position) | Pitch controller + Damping controller | Bar sets pitch and adds contact damping |
| Slap (thumb / pop / palm / body tap) | Excitation source | Impulsive events; body tap bypasses string |
| Muting (palm, ghost, chuka, fret mute) | Damping controller | Adds damping to strings |
| Two-hand tapping | Damping controller + Pitch controller | Tap acts as movable capo |
| Microtonal bends | Pitch controller (offset) | Adds pitch offset, does not compete for pitch |
| Fingerstyle attack (from fingerstyle-attack.md) | Excitation source | Alternative to pick |
| Pick (from pick-noise.md) | Excitation source | Standard excitation |

## 2. Same-string compatibility matrix

Rows are already-active techniques; columns are techniques the
user tries to add on the same string in the same window (~200 ms).

|   | Scrape | Slide | Slap | Mute | Tap | Bend |
|---|---|---|---|---|---|---|
| **Scrape** | queue | conflict | conflict | compatible (dulls scrape) | conflict | compatible |
| **Slide** | conflict | (same technique) | conflict | compatible (muted slide) | conflict | compatible |
| **Slap** | conflict | conflict | queue | compatible (mute-then-slap) | alternate | compatible |
| **Mute** | compatible | compatible | compatible | (same technique) | compatible | compatible |
| **Tap** | conflict | conflict | alternate | compatible | queue | compatible |
| **Bend** | compatible | compatible | compatible | compatible | compatible | (same technique) |

Legend:
- **compatible**: both apply; effects layer.
- **queue**: same class, latest request queues after current
  gesture completes.
- **alternate**: both can occur but not overlapping in time; latest
  wins if forced overlap.
- **conflict**: physically incompatible; priority table decides.

## 3. Conflict resolution priority

When two conflicting techniques target the same string:

1. **User-triggered gestures beat automatic ones.** A user pressing
   the tap button wins over an auto-slide gesture.
2. **Most-recent wins for same-priority.** Later request preempts
   the earlier.
3. **Preemption is graceful.** The preempted gesture releases (as
   if the user let go). No hard clicks; a 10 ms crossfade covers
   the transition.
4. **Slide holds priority over scrape and tap once engaged.**
   Because slide is a continuous state, not an event. To end a
   slide, the user must lift the bar; new events on that string
   cannot preempt.
5. **Mute can always be added.** Muting is additive damping; it
   never conflicts.
6. **Bend can always be added.** Bend is an offset; it never
   conflicts.

## 4. Cascade schedule (audio pipeline)

Per block, the engine runs the technique modules in this order,
using each's output as inputs to the next where relevant:

```
1. TechniqueEngine: interprets user input, produces gesture events.
2. Cascade resolver: applies section 3 rules; emits per-string
   final gestures.
3. Excitation stage:
   - PickEngine, FingerstyleEngine (from fingerstyle-attack.md).
   - ScrapeEngine.
   - SlapEngine.
4. Damping stage:
   - MuteEngine (from muting-rhythm.md).
   - Tap events (damping half).
   - Slide contact damping.
5. Pitch controller stage:
   - Tap events (pitch half: sounding-length change).
   - Slide bar position.
   - Bend offset from microtonal-bends.md.
6. StringEngine: consumes excitations, dampings, pitch offsets in
   one integrated update per sample.
7. BodyCoupling (from body-coupling.md): body tap bypasses to here
   directly.
```

Every stage is optional; if none of its inputs are active this
block, it costs nothing.

## 5. Combined-technique presets

Presets that demonstrate valid combinations:

- **Metal Lead Combo**: Bend + Tap + Mute. Tap arpeggio with palm
  mute on the low strings and bends on the melody.
- **Funk Slap Groove**: Slap + Mute + Bend. Standard funk with
  occasional string bends.
- **Slide Blues**: Slide + Bend (vibrato as bend source) + Mute
  (palm mute behind slide).
- **Percussive Tap**: Tap + Mute (extreme) + Body Tap (rhythm).
- **Scrape Intro**: Scrape + Mute (gradual mute release for build-up).
- **Full Cascade Demo**: showcase preset that walks through every
  compatible pair over 30 seconds.

## 6. GUI conflict indicators

Techniques tab: when the user arms a technique that conflicts with
one already active on the same string, the conflicting technique's
pill shows a red slash icon and a tooltip explains: "Conflicts with
Slide on strings 3-6. Slide will preempt on trigger."

Cascade is otherwise silent: everything just works.

## 7. Testing cascade combinations

Test matrix: for every pair (A, B) in the compatibility matrix,
- Same string: verify documented outcome (both apply, queue,
  alternate, or conflict per table).
- Different string: verify both apply independently.

For every combined preset in section 5:
- Load, play a scripted MIDI sequence exercising the intended
  combination, verify output matches a saved reference within
  0.5 dB.

Fuzz test: 10 000 random gesture sequences across all six
techniques on all strings; no crashes, no orphaned state, CPU
within budget.

## 8. Interaction with existing specs

- Uses the existing `TechniqueEngine` from the phase 1 extensions
  as the input side. This spec adds the cascade resolver stage
  after it.
- Uses `modulation-matrix.md` for source routing (pitch bend
  sources, vibrato sources).
- Uses `state-model.md`'s state layers unchanged.
- Uses `action-and-undo.md`'s undo entries: each cascade decision
  is not a user action per se, so it doesn't push an undo entry;
  only the arming of a technique does.

## 9. Performance

- Cascade resolver: O(strings) per block, negligible.
- Total added budget for all six new technique modules on a busy
  passage (all six active on different strings): ~2.5% CPU on
  mid class.
- Body Tap adds up to 1% CPU while active (drives body modes).

## 10. Tests

- Every cell of the compatibility matrix has a fixture and a test.
- Preemption crossfade: preempted gesture ends over 10 ms with no
  click (measured spectral discontinuity < 0.5 dB in adjacent
  frames).
- Cross-string independence: slap on E, slide on B, tap on G, bend
  on A: every string outputs the intended pitch and excitation.
- Full-cascade preset renders match reference within 0.5 dB across
  30 seconds.
- Conflict indicator UI: arming a conflicting technique shows the
  slash icon within one UI frame.
