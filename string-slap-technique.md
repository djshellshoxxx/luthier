# STRING SLAP TECHNIQUE SPEC

Slap is documented for bass in `bass-techniques.md`. This file extends
that with:
- User-modifiable controls surfaced in the Techniques tab.
- Slap on guitar (rarer but valid: acoustic slap-percussion, hybrid
  slap-strumming).
- General slap gesture engine that both cases share.

## 0. Ground rules

1. **Do not duplicate bass-techniques.md.** The bass slap mechanics
   (thumb-slap, pop, ghost, double thump) live there. This file adds
   user controls and the guitar generalisation.
2. **Slap is a strike, not an amplitude modifier.** The string is
   physically struck by a thumb or slapped by a hand; the impact
   generates a slap-buzz event against the fretboard plus the string
   excitation.
3. **Position and force are independent.** Position (where along the
   string) shapes the tone; force shapes the loudness and fret-buzz
   contribution.
4. **Slap works on any guitar with wound strings.** The engine
   doesn't enforce bass-only; the user can slap an acoustic for
   percussion.

## 1. User controls

Techniques tab, Slap sub-tab:

- **Slap type**: 4-way (Thumb Slap, Finger Pop, Palm Slap, Body Tap).
  - Thumb Slap: bass thumb technique.
  - Finger Pop: index/middle yank.
  - Palm Slap: whole-hand percussive slap across muted strings
    (guitar and bass funk chuck).
  - Body Tap: slap on the guitar body (acoustic percussive).
- **Trigger source**: velocity zone, keyswitch, CC, MPE zone, or
  Playing strip button.
- **Contact position** (mm from bridge): defaults to 60 mm for
  thumb slap, 40 mm for pop, 100 mm for palm slap.
- **Contact force**: 0-1 (default 0.6). Scales string excitation
  and slap-buzz event amplitude.
- **String mask**: which strings the slap affects. Bass thumb
  defaults to low E/A. Palm slap defaults to all strings.
- **Ghost mode**: when on, the fretting hand mutes the string just
  before the slap so the strike produces a percussive thump with no
  clear pitch. Trigger via a modifier keyswitch or a dedicated CC.
- **Rebound**: on / off. Double thump on thumb rebounds by default;
  the rebound gap is user-adjustable (default 60 ms).
- **Snap-back** (bass only): controls how hard the string snaps
  back against the fretboard after the slap. Higher values = more
  clacky, funkier tone.
- **Body tap resonance**: body tap uses the body-coupling.md mode
  bank; user chooses which body part (top, side, back) the tap
  contacts, which weights different modes.

## 2. Engine integration

New micro-module `SlapEngine`. Sits alongside `ScrapeEngine` and
`SlideEngine`. Consumes gestures from `TechniqueEngine`, produces
excitation events into `StringEngine` and slap-buzz events into
`NoiseEngine`'s fret-buzz path (`fret-buzz.md`).

Interface:
```cpp
class SlapEngine {
    void trigger(const SlapGesture& g);
    void processBlock(int n);
    void reset();
};
```

Body Tap is routed differently: the gesture bypasses `StringEngine`
entirely and drives a direct impulse into `BodyCoupling`'s mode bank
at the tap location.

Insertion: after `TechniqueEngine`, alongside `ScrapeEngine`, before
`StringEngine` and `BodyCoupling`.

## 3. Interaction with bass-techniques.md

`bass-techniques.md`'s slap and pop use the same underlying
`SlapEngine`. The controls exposed there stay; this spec adds the
sub-tab UI, generalises to guitar, and adds Palm Slap and Body Tap
as new gesture types.

Migration: existing bass presets keep working; the SlapEngine reads
the same fields the older spec produced.

## 4. Presets

- **Bass Slap Standard**: thumb + pop, defaults from
  bass-techniques.md.
- **Bass Slap Aggressive**: high force, high snap-back.
- **Funk Guitar Palm Slap**: palm slap on all muted strings, chuka
  rhythm.
- **Acoustic Body Tap**: body tap on top, weighted toward top mode.
- **Percussive Fingerstyle**: mix of thumb slaps on bass strings
  and body taps between notes.

## 5. Cascade compatibility

Per `technique-cascade.md`. Slap is compatible with palm muting
(mute-then-slap is standard funk), microtonal bends (bend after
slap for the "yowl"), and tapping in alternation (rare but valid).

Slap is not compatible with slide (bar contact prevents thumb
access), or with scraping the same string in the slap window.

## 6. GUI location

Techniques tab, Slap sub-tab. Also surfaced in the Playing strip
Tool selector (from `fingerstyle-attack.md`) which gains "Slap"
and "Pop" tool options.

## 7. Tests

- Bass thumb slap on low E at position 60 mm, force 0.6: output
  matches bass-techniques.md's reference within 1 dB.
- Palm slap on muted strings produces a broadband percussive event
  with < -25 dB pitched content.
- Body tap drives body-coupling modes without exciting strings
  (< -60 dB on string outputs when strings are damped).
- Ghost mode: string is muted just before slap, thump has no clear
  pitch.
- Double-thump rebound gap matches configured value ± 3 ms.
- Slap on plain string (high E on a guitar): audible but with
  reduced buzz component (plain strings buzz less against frets).
- CPU: idle < 0.05%; active < 0.6% per slap event.
- Preset save / restore round-trips every added field.
