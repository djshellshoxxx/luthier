# Factory riff sources

One `.riffdef` file per genre, compiled by `Tools/generate_factory_riffs.py`
into `Resources/Riffs` (spec/riff-library.md 2). Every item is original,
written for Luthier: no transcriptions, nothing "in the style of" anyone, and
no artist, band, song or album names (`deny.txt`, checked by the generator).

## File layout

```
defaults author L origin original date 2026-09-24

riff blues.lick.delta-turnaround-3 "Delta Turnaround 3"
  type lick   instrument guitar6   tuning standard   capo 0
  key A minor_pentatonic   tempo 84   meter 4/4   feel shuffle
  difficulty 2   tags turnaround box-1   free yes
  chords 0:A7 4:E7
  play:
    t8: 1.8b2~ 1.8 1.5 2.8b1r 2.5 3.7/9 3.9~ ~ r e: 3.7 h3.9 |
```

`#` starts a comment. Header words (any order, any number of lines):

| Word | Values |
|---|---|
| `type` | `riff` `lick` `strum` `bass` (must match the id) |
| `instrument` | `guitar6` `guitar7` `guitar12` `bass4` `bass5` `bass6` |
| `tuning` | guitar: `standard` `drop_d` `half_down` `drop_c` `open_g` `open_d` `open_e` `dadgad`; guitar7: `standard` `drop_a`; bass4: `standard` `drop_d`; bass5/6: `standard` |
| `capo` | 0-12 |
| `key` | root (`C` `C#` `Db` ... `B`) and scale: `ionian` `dorian` `phrygian` `lydian` `mixolydian` `aeolian` `locrian` `harmonic_minor` `melodic_minor` `major_pentatonic` `minor_pentatonic` `blues` `chromatic` |
| `tempo` | 30-300 bpm |
| `meter` | e.g. `4/4` `3/4` `6/8` `12/8` |
| `feel` | `straight` `shuffle` `swing` `half_time` `laid_back` `driving` (a tag: the timings already hold the feel) |
| `difficulty` | 1-5 |
| `tags` | lower-case words joined by `-` |
| `free` | `yes` puts the item in the Free set |
| `chords` | `<beat>:<symbol>` ... |
| `author`, `origin`, `date` | usually from `defaults`; origin must be `original` |

The id is `<genre>.<type>.<slug>`; the name is `[Descriptor] [Figure] [n]`,
under 32 characters. Ids never change once shipped; a rename changes the name.

## The `play:` body

Strings are numbered as tab is read: 1 is the highest.

- **Durations** are sticky until changed: `w: h: q: e: s: t:` (whole to 32nd),
  a dot for dotted (`q.:`), `t8:` / `t16:` for triplet eighths / sixteenths.
- `r` is a rest, and a lone `~` holds the previous note (or chord) for one more
  duration.
- **Note** `<string>.<fret>`; `<string>.x` is a dead note.
- **Chord** `[6.3 5.2 4.0 3.0 2.0 1.3]`, or a shape written low string to high:
  `[x32010]`, or with dashes for two-digit frets `[x-10-12-12-11-10]`.
- **Bar lines** `|`: every bar must add up to the meter exactly, and the last
  bar must be closed. 1 to 8 bars, at most 64 beats.

Prefixes (before the note or chord):

| Prefix | Meaning |
|---|---|
| `h` `p` | hammer-on, pull-off (needs the note before it, on the same string, to end exactly where this one starts; hammers go up, pulls go down) |
| `D` `U` `Dx` | strum down, up, and a muted chuck (chords only) |
| `sl` `po` `th` `lh` | bass slap, pop, double thump, left-hand slap (BASS_TECH) |

Suffixes (after the fret or the closing `]`, in any order):

| Suffix | Technique token | Notes |
|---|---|---|
| `b<n>` | `bend` | n semitones, 0.5-3 (1.5 on bass) |
| `b<n>r` | `bendrelease` | bend up and back within the note |
| `pb<n>` / `pb<n>r` | `prebend` | bent before the pick; `r` releases it |
| `~` | `vibrato` | |
| `/<f>` `\<f>` | `slidelegato` | slide to fret f; the next note on the string is usually f |
| `s/<f>` `s\<f>` | `slideshift` | the arrival is picked |
| `/in` | `slidein` | into the note from below |
| `\in` | `slidedown` | into the note from above |
| `\out` | `slideout` | off the note, downward |
| `/out` | `slideup` | off the note, upward |
| `pm` / `pm0.5` | `pm` | palm mute, optional depth 0-1 |
| `<>` | `natural` | on a node fret: 3 4 5 7 9 12 16 19 24 |
| `*` | `pinch` | |
| `ah` / `ah<n>` | `artificial` | n frets above the fretted note, default 12 |
| `tah` / `tah<n>` | `tapharm` | tapped harmonic, default 12 above |
| `T` | `tap` | frets up to 36 |
| `tr<f>` | `trill` | trill with fret f |
| `wb<n>` | `whammy` | dip (negative) or raise, semitones |
| `!` `'` `lr` `g` | `accent` `staccato` `letring` `ghost` | |
| `v<1-127>` | | velocity (default 0.8 of full) |
| `cv<n>` | | strum crossing speed in strings per second (default 200) |

A palm mute and a tap cannot share a note.

## Checking

```
python3 Tools/generate_factory_riffs.py --only blues   # one genre
python3 Tools/generate_factory_riffs.py                # everything, writes Resources/Riffs
python3 Tools/generate_factory_riffs.py --check        # CI: fails if Resources/Riffs is stale
```
