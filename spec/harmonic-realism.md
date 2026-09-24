# HARMONIC REALISM SPEC

A harmonic is not a filtered note. It is what a string does when
something touches it at a node: every partial with a node there
survives, every partial without one is killed within a few periods,
and what is left is a string that vibrates at an integer multiple of its
fundamental, with its own upper partials, its own decay and its own
slight inharmonic stretch.

Luthier already has harmonics, and they are wrong in two ways.

1. **They sound at the wrong pitch.** `MidiInterpreter::emitVoicedNote`
   tunes the string to the *touch* fret, and `Excitation` then isolates
   partial *n* of that. A natural harmonic at fret 12 of the low E
   therefore sounds E4, not E3; at fret 7 it sounds 4.49x the open
   string instead of 3x. The string must stay at its stopped length
   (open for a natural harmonic) and sound partial *n* of that.
2. **They are sines.** `Excitation::trigger` step 6 band-passes the
   pluck around one partial (Q 3.2, two passes) and
   `StringEngine::updateLoopCoefficients` multiplies T60 by 0.55. The
   result has no 2n, 3n partials, leaks the fundamental back in as the
   band-passed energy decays, and has a decay that is a guess.

This file replaces both with one mechanism: a **contact** on the
waveguide, i.e. a boundary-condition change, that natural, artificial,
pinch and tapped harmonics all use, and that `two-hand-tapping.md` 4
and `engine-technique-layer.md` 3.2 already name as "the damp-position
input of harmonic-realism 2".

## 0. Ground rules

1. **A harmonic is a touch, not a filter.** The excitation is the
   ordinary pluck (or pick, or tap) of the tool in use; the harmonic
   comes from the contact.
2. **Stopped length sets the pitch.** Natural harmonics sound partial *n*
   of the open (or capoed) string, artificial and tapped harmonics
   partial *n* of the fretted string. The touch never shortens the
   string.
3. **Physics decides the partial.** *n* follows from where the contact
   is, in millimetres, not from velocity. A touch between nodes
   produces a dead thud, which is what a missed harmonic sounds like.
4. **Honest magnitudes.** A clean natural harmonic suppresses the
   fundamental by 35 dB or more. A pinch harmonic on a clean amp is
   weak (10-25 dB under the fundamental); the squeal players know is
   the amp's gain, and the model must not fake it at the string.
5. **Just intonation is real.** Partial 5 is 13.7 cents flat of equal
   temperament and partial 7 is 31.2 cents flat, before inharmonic
   stretch. Those deviations are audible and are kept.

## 1. The physics

A string of vibrating length `L` touched at `x` (from the bridge) with a
finger of contact width `w` damps every partial `k` in proportion to
its displacement at `x`, `sin(k pi x / L)`. Partials with a node at `x`
(`k x / L` integer) are untouched. For `x = L m / n` in lowest terms the
survivors are `n, 2n, 3n ...`: a string at `n f0`.

In the lumped Karplus-Strong loop (`engine.md` 5.2) there is no
position, so the contact is realised as a filter in the loop that has
exactly that response: the **n-tap node comb**

```
M      = sr / f_n,        f_n = n f0 sqrt(1 + B n^2)         (1)
C_n(z) = (1/n) sum_{m=0}^{n-1} z^{+mM}                        (2)
H(z)   = (1 - g) + g e C_n(z)                                 (3)
```

At partial `k`, `C_n = 1` with zero phase when `n | k` and `0` otherwise
(a sum of the n-th roots of unity). So per round trip, node partials
lose `(1 - g + g e)` and all others lose `(1 - g)`:

- `g` is contact strength (0 none, 1 an ideal damper), from
  `harmonic_touch_pressure`.
- `e` is **node efficiency**: `e = exp(-(d / w)^2)`, `d` the distance in
  mm from `x` to the nearest node of partial `n`. `n` is chosen as the
  `n` in 2..8 maximising `e / sqrt(n)`. If the best `e < 0.05` the
  contact is off-node and (3) degenerates to uniform damping `1 - g`.
- `z^{+mM}` reads the delay line `m M` samples *less* delayed than the
  main read. Those samples are already in the buffer: the comb costs
  `n - 1` extra fractional reads and no memory.

Magnitudes: `g = 0.6` removes 8 dB per round trip from non-node
partials. A 70 ms touch on the low E (82 Hz) is 5.8 round trips:
**-46 dB** on the fundamental, which is why a clean 12th-fret harmonic
has no audible fundamental. On the high E (330 Hz) the same touch is 23
round trips. A pinch's 8 ms graze is 0.66 of a round trip on the low E
and 2.6 on the B string: it takes 5-11 dB off the fundamental, so on a
clean string the pinch harmonic stays 10-25 dB *under* the fundamental,
as it does on a real guitar. High gain compresses that gap away
downstream; nothing here boosts it.

Because `|C_n| <= 1` and the combination is convex, `|H| <= 1` at every
frequency. The contact can only remove energy; it cannot destabilise
the loop.

Partial `n` sits at `n f0 sqrt(1 + B n^2)`, and (1) sets `M` from the
stretched frequency, so the harmonic's pitch is the string's real
partial. With the default low E `B = 0.0008`, partial 4 is 11 cents
sharp of `4 f0` and partial 5 is 17 cents sharp, which partly cancels
its 13.7-cent just flatness.

## 2. `StringEngine` contacts (the shared boundary-condition input)

```cpp
struct Contact
{
    double positionFromBridge = 0.5;   // fraction of the vibrating length
    double vibratingLengthMm  = 648.0; // for d and w in mm
    double strength           = 0.6;   // g in (3)
    double widthMm            = 2.5;   // w
    double seconds            = 0.07;  // auto-release; <= 0 holds until cleared
};

static constexpr int kMaxContacts = 4;
int  StringEngine::addContact (const Contact&) noexcept;   // returns slot, or -1
void StringEngine::clearContact (int slot) noexcept;
void StringEngine::clearAllContacts() noexcept;
```

- Contacts are applied in `processSample` between `delayLine.read` and
  `loopFilter.process`, cascaded in slot order. With no active contact
  the path is skipped: bit-identical to today.
- `g` ramps in and out over 1 ms (linear, per sample), so a touch
  landing or lifting never steps the loop.
- `M` and `n` are recomputed on `needsLoopUpdate`, i.e. whenever the
  pitch moves more than 0.2 %, never per sample.
- **Fallback**: if `compensated - (n-1) M < 2` (very short loops: high
  notes, high `n`, low sample rate) the contact cannot be read and the
  string falls back to today's `Excitation` band isolation for that
  note. Counted by the validator; never silent.
- `setHarmonicRestriction` is kept as a shim that adds a contact for
  the given partial at its first node, so existing callers compile.
- While any contact is active, `couplingReceptivity` is 0.35 (today's
  harmonic value). It returns to the damping state's value on release.
- **Removed**: the `t60Scale *= 0.55` harmonic line. The harmonic's
  decay is now the loop filter's at `n f0`, which is shorter for the
  physical reason (higher partials lose more per second).
- **Direct injection**: `processSample (couplingInput)` scales
  everything it is given by `couplingReceptivity`, which today includes
  scrape catches, pick-click noise and feedback. This file splits it
  into `processSample (couplingInput, directInput)`; only coupling is
  scaled. Scrape (`string-scraping.md`), slap (`string-slap-technique.md`)
  and tap impulses (`two-hand-tapping.md`) use `directInput`. That is
  the "multi-source excitation" `engine-technique-layer.md` 3.2 cites.

A contact is not a stop. A tap that stops the string (two-hand-tapping
2) changes pitch through `setTargetFrequency` and uses a contact only
for its landing transient; that is the tapping spec's business.

## 3. The four harmonics

| Kind | Stopped length | Touch at | Excitation | Touch time |
|---|---|---|---|---|
| Natural | open (capo) | the MIDI note's fret (or located, 4.2) | normal pluck, current tool | `harmonic_touch_time` |
| Artificial (harp) | fretted note | fret + `artificial_harmonic_offset` | normal pluck | `harmonic_touch_time` |
| Pinch | fretted note | node nearest pick + `pinch_thumb_offset_mm` | pick at `pluck_position` | `harmonic_brief_touch` |
| Tapped | fretted note, already ringing | fret + `tapped_harmonic_offset` | `Kind::Tap` impulse at the node | 1.5 x `harmonic_brief_touch` |

- **Natural / artificial.** `Excitation::Kind::Harmonic` renders as a
  pluck (`kindGain` 1.0, no band isolation). The pluck-position comb
  applies as usual, so plucking at a node of `n` (e.g. the midpoint for
  a 12th-fret harmonic) kills the harmonic. That is correct and is
  tested.
- **Pinch.** The pick releases at `pluck_position`; the thumb lands
  `pinch_thumb_offset_mm` toward the neck and grazes. Moving the pick
  (CC 70, `PickPosition`, already mapped) sweeps the partial, which is
  the pinch sweep players do. Velocity no longer picks the partial
  (today's `2 + velocity * 3` in `TechniqueEngine::decide` goes).
- **Tapped.** If the target string is ringing, no new excitation is
  voice-stolen: the tap adds a `Kind::Tap` impulse (legato, like
  `HammerOn`) and the contact converts the existing vibration. On a
  silent string it still fires, and is quiet, because an impulse at a
  node barely excites that node's partials.

Artificial offsets map to partials: 12 frets -> 2, 7 -> 3, 5 -> 4,
4 (3.86) -> 5, 19 -> 3 (the 2/3 node), 24 -> 4.

## 4. Pitch and note mapping (the fix)

### 4.1 Touch-fret mapping (default)

The MIDI note, voiced as today, names the **touch** position, which is
how tab writes harmonics (`<12>`). `emitVoicedNote` then sets, for
`NaturalHarmonic`: `e.pitchHz = computeFrequency (s, openFret, bend)`
and the new `e.touchFret = note.fretPosition`. For artificial and
tapped, `e.pitchHz` is the fretted pitch and
`e.touchFret = fret + offset`. `LuthierEngine::triggerNote` uses the
stopped fret for dead spots, fret wear and termination brightness, and
builds the `Contact` from `touchFret`, the scale length and the stopped
fret. `TechniqueEngine::harmonicPartialForFret`'s table is replaced by
the analytic node search of section 1, so fret 3.86, 8.84 and 15.86
(partial 5) are hit exactly rather than as "4", "9" and "16".

### 4.2 Sounding-pitch mapping

With `harmonic_note_mapping = Sounding`, the note names the pitch
heard. `MidiInterpreter` bypasses the voicer for harmonic-armed notes
and calls `HarmonicLocator::find (targetHz, openHz[], maxFrets,
handFret)`: for `n` = 2..7 and each string, accept when
`n f_open sqrt(1 + B n^2)` is within 30 cents of the target and the
node fret is playable; prefer the lowest `n`, then the string nearest
the hand. None found: play an artificial harmonic of the note
`offset` semitones down. The TUNE tab and notation export keep writing
touch-fret notes; this mode is for keyboard players.

## 5. Parameters

Physical rows join the `pick` family (right- and left-hand contact),
because `file-formats.md` fixes the seven family keys and contact with
the string is closest to the pick's contact.

| ID | Name | Stock | Advanced | Default | Unit |
|---|---|---|---|---|---|
| `harmonic_touch_pressure` | Harmonic Touch | 0.2 – 1.0 | 0.0 – 1.0 | 0.6 | g, ratio |
| `harmonic_finger_width` | Finger Contact Width | 1.0 – 6.0 | 0.1 – 20 | 2.5 | mm |
| `harmonic_touch_time` | Touch Time | 20 – 200 | 1 – 1000 | 70 | ms |
| `harmonic_brief_touch` | Pinch / Tap Graze | 3 – 20 | 0.5 – 100 | 8 | ms |
| `pinch_thumb_offset_mm` | Thumb Offset | 2 – 12 | 0 – 40 | 6 | mm |
| `artificial_harmonic_offset` | Artificial Offset | choice 12/7/5/4/19/24 | – | 12 | frets |
| `tapped_harmonic_offset` | Tapped Offset | choice 12/7/5/4/19/24 | – | 12 | frets |
| `harmonic_note_mapping` | Harmonic Notes | choice Touch fret / Sounding | – | Touch fret | – |

`advanced-ranges.md` 3.3: `harmonic_touch_pressure` above 1 would make
(3) non-convex, so its advanced ceiling stays 1.0. Net new: **+8**. All
are declared with their stock range as the declared range
(`RangeRegistry::noteDeclaration`).

## 6. Triggering

Consistent with the existing CC map (`MidiInterpreter::resetCcMapToDefaults`):

| Trigger | Default | Exists? |
|---|---|---|
| Pinch harmonic (held) | CC 72 | yes |
| Natural harmonic (held) | CC 73 | yes |
| Artificial harmonic (held) | **CC 103**, new `MidiTarget::ArtificialHarmonic` | new |
| Tapped harmonic (held) | **CC 104**, new `MidiTarget::TappedHarmonic` | new |
| Harmonic by velocity | `setHarmonicVelocityTriggerEnabled` | yes, unchanged |

CC 102-119 are undefined in MIDI 1.0 and unused in the map (scrape
holds 85, slap 86-87). `TechniqueEngine::decide` priority becomes:
palm mute, pinch, natural, **artificial**, tap, **tapped harmonic**,
then the rest. Today's "not on a node, so artificial" fallback under
CC 73 stays. Tapped harmonic is an excitation source for
`technique-cascade.md` 1 and sits in the Tap row of its matrix
(alternate with slap, conflict with slide and scrape).

## 7. UI, state and export

- `gui-integration.md` 4.4 CHARACTER tab: a **HARMONICS** row inside the
  PICK group: touch pressure, finger width, touch time, graze time,
  thumb offset, both offset dropdowns, note mapping. Tooltips give the
  partial a fret produces.
- Fretboard illustration: during a touch, a hollow ring at the contact
  position (layer 33+, `gui-techniques-updates.md` 4), fading over the
  touch time; a missed (off-node) touch draws dashed.
- Section 19 rows: "Harmonic contact / offsets / mapping | StringEngine
  contacts | Col 4 CHARACTER -> PICK -> HARMONICS | – | –".
- Preset: plain APVTS parameters; no structural state.
- `midi-export.md` NOTE class: the existing harmonic-type flag gains
  `touch_fret` (fractional) and `partial`. Generic profile writes the
  touch-fret note plus CC 72/73/103/104, which is what this plugin reads
  back.

## 8. Performance and realtime safety

- Per contact per sample: `n - 1` Lagrange-5 reads (at most 7) and one
  multiply-add: at most ~50 MACs, only while touching. Budget:
  **0.05 units** with all six strings touching at `n = 8`; idle 0.
- No allocation: contacts are a fixed array of 4 per string, set in
  place. No locks. `M` recomputed at block rate at most.
- `reset()` clears all contacts and their ramps.
- NaN guard and DC blocker unchanged; (3) is energy-non-increasing so
  no new clamp is needed.

## 9. Tests

- **HR-01 Natural harmonic pitch.** Open low E (82.407 Hz,
  `B` from the string), touch fret 12: steady-state pitch at 300 ms is
  `2 f0 sqrt(1 + 4B)` within 2 cents. (Today it is an octave higher.)
- **HR-02 Fundamental suppression.** Same note, clean string output at
  300 ms: partial 1 at least 35 dB below partial 2; partials 3 and 5 at
  least 30 dB below partial 2.
- **HR-03 Not a sine.** Same note: partial 4 is within 30 dB of partial
  2 (the harmonic has its own upper partials).
- **HR-04 Fret 7.** Touch fret 7.02 on the open A: pitch
  `3 f0 sqrt(1 + 9B)` within 3 cents; partials 1 and 2 at least 30 dB
  below partial 3.
- **HR-05 Just intonation.** With `B` forced to 0, touch fret 3.86 on
  the low E sounds 13.7 cents flat of equal-tempered G#4, within
  1 cent.
- **HR-06 Missed touch.** Touch fret 6.0 (no node of `n <= 8` within
  6 mm): RMS over 200-400 ms is at least 18 dB below the fret-7
  harmonic's.
- **HR-07 Finger width.** Touch 3 mm off the 12th-fret node: at width
  2.5 mm the harmonic loses 3-15 dB against an exact touch; at 6 mm it
  loses less than 3 dB.
- **HR-08 Plucking the node.** 12th-fret harmonic with
  `pluck_position` 0.5 is at least 20 dB quieter than with 0.16.
- **HR-09 Artificial.** A string fret 5 (D3), offset 12: pitch D4
  within 3 cents (after stretch); offset 7: `3 x D3` within 3 cents.
- **HR-10 Pinch.** B string fret 5, default pick position, clean string
  output at 200 ms: the selected partial's level relative to the
  fundamental is at least 8 dB higher than on a plain pluck of the same
  note, and the partial is still below the fundamental (ground rule 4).
  Moving `pluck_position` from 0.12 to 0.20 changes the selected `n`.
- **HR-11 Tapped.** A fret 5 ringing for 500 ms, tapped harmonic
  offset 12: within 150 ms partial 2 exceeds partial 1 by 20 dB; no
  voice-steal fade occurred (`stealPending` never set).
- **HR-12 Stability.** 10 s of random contacts (all `n`, strength 1.0,
  widths 0.1-20 mm) on all 12 strings of a 12-string at 44.1/96 kHz: no
  NaN or Inf, peak below 4.0, and with no excitation the string's
  energy is non-increasing block to block.
- **HR-13 Click-free.** Contact landing and lifting on a ringing string:
  the maximum first difference in the 2 ms around each edge is at most
  1.5x the free string's over the same window.
- **HR-14 Sounding-pitch mapping.** Request E4 in Sounding mode on
  standard tuning: the locator returns the A string, `n = 3`, touch
  fret 7.02 +/- 0.05.
- **HR-15 Fallback is counted.** High E fret 20, `n = 8`, at 44.1 kHz
  either realises the contact or falls back to band isolation and the
  validator records the fallback; never neither.
- **HR-16 Contact-free is bit-identical.** A performance with no
  harmonic events renders bit-identical output before and after this
  change (the contact path and the `directInput` split are inert).
- **HR-17 Ranges.** Every physical row is in `RangeRegistry` with a
  valid `PhysicalRange`; `findDeclarationMismatches()` is empty.
- **HR-18 Realtime.** No allocation on the audio thread across HR-01 to
  HR-12 (heap hook); contact CPU at six strings, `n = 8`, within
  0.05 units.
- **HR-19 Export round trip.** Luthier-profile export and re-import of
  a passage with all four kinds nulls to -60 dBFS RMS.
