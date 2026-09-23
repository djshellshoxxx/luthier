# Luthier MIDI profile: byte-level encoding

This is the encoding that `spec/midi-export.md` section 2.1 refers to. The code
is in `Source/Export/`: `LuthierMidiEvents` (events and payloads),
`MidiProfiles` (files) and `LiveMidiOut` (live MIDI out). The tests are in
`Source/Tests/MidiExportTests.cpp`.

Wire format version: **1**. Every event class is at schema **1**.

## 1. File layout

A standard MIDI file, format 1, with PPQ timing from 96 to 3840 (default 960).
SMPTE timing is refused on import.

| Track | Contents |
|---|---|
| 0 | Luthier header (first event, Luthier profile only), `FF 03` title, `FF 02` copyright, `FF 59` key signature, `FF 58` time signatures, `FF 51` tempo map |
| 1 to n | `FF 03` track name, the pitch-bend-range RPN for every channel the track uses, then channel messages and extension events in time order |

The pitch-bend-range block is `CC 101 0`, `CC 100 0`, `CC 6 <semitones>`,
`CC 38 <cents>`, `CC 101 127`, `CC 100 127`, all at tick 0. Import removes it
from the performance and reads the range from it (Generic) or from the header
(Luthier).

Track splits (`split=` in the header):

| Split | One track per | Track name |
|---|---|---|
| `single` | performance | the title, or "Guitar" |
| `section` | SECTION start event (anything earlier goes in "Start") | the section name |
| `instrument` | part (0 guitar, 1 bass) | the part name from the header |
| `string` | MIDI channel (channel N plays string N) | "String N" |

A note-off always goes in the same track as its note-on.

## 2. Header (Luthier profile)

The first event of track 0 is a sequencer-specific meta event:

```
FF 7F <len> 7D 'L' 'U' 'T' 'H' 'I' 'E' 'R' <wire version> <fields> <checksum>
```

`<fields>` is ASCII text: `key=value` pairs separated by single spaces, with
values escaped as in section 5. `<checksum>` is the sum of the field bytes,
seven bits.

| Key | Meaning |
|---|---|
| `profile` | `luthier` |
| `sr` | sample rate the performance was captured at (required) |
| `ppq` | ticks per quarter note |
| `split` | the track split |
| `sysex` | 1 if every event has a SysEx copy |
| `messages` | channel messages in the file, not counting RPN blocks |
| `events` | extension events in the file |
| `bend` | pitch-bend range in semitones |
| `parts` | part names, comma separated, each escaped (a comma is `%2C`) |
| `guitar`, `preset`, `seed` | identifiers; left out when identifiers are stripped |

Readers ignore keys they do not know. A file whose header is missing is read as
Generic without a warning. A header whose checksum fails, or whose counts do
not match the file, is refused.

## 3. Exact timing: `LUTHIER-AT`

A tick carries the beat. When a channel message does not fall exactly on a
tick, or belongs to a part other than 0, or the file has more than one event
track, it is preceded on the same tick by a text meta (`FF 01`):

```
LUTHIER-AT dt=<samples> part=<n> n=<index>
```

- `dt`: samples from the tick's own sample to the message's sample. The tick's
  sample is `round(seconds(tick) * sr)` through the file's tempo map.
- `part`: omitted for 0.
- `n`: the message's position in the performance, written in multi-track files
  so that import restores the order of messages that share a sample.

A reader at another sample rate ignores `dt` and places messages by tick, and
says so. `dt` makes the round trip exact at every PPQ.

## 4. Extension events

Every extension event is four consecutive events on its tick:

```
FF 01 "LUTHIER-BEGIN <CLASS> <schema>"
FF 01 "LUTHIER: <CLASS> <fields>"                          text copy
F0 7D 'L' 'T' <wire version> <payload> <checksum> F7       SysEx copy (unless sysex=0)
FF 01 "LUTHIER-END"
```

`<payload>` is `<CLASS> <schema> <fields>`. The text copy is the payload with
the schema taken out, which the BEGIN marker carries. The checksum is the sum
of the payload bytes, seven bits. Payload bytes are 0x20 to 0x7E.

`<fields>` starts with `dt=` (samples from the tick, as in section 3) and
`part=` when they are not 0, followed by the class's own fields.

A reader accepts either copy. When both are present they must agree, or the
file is refused. A class this version does not know is kept: its fields if
they parse as tagged text, otherwise its bytes, and it is written back
unchanged.

Removing the header leaves a valid Generic file. The markers and SysEx are
ignored by any reader that does not know them.

## 5. Values

- Integers: decimal, optional leading `-`.
- Reals: the shortest decimal that reads back as the same double (`0.6`,
  `200`, `6.0221407600000000e+23`). Live MIDI out writes six decimal places,
  trailing zeros removed.
- Words and text: UTF-8, with space, `=`, `%`, control characters and every
  byte above 0x7E written as `%XX`. `%00` is not allowed.
- Lists: comma-separated items inside one value (`curve=0:0,0.5:2,1:2`).

Keys are `[a-z0-9_]`. Class names are `[A-Z0-9_]`. `dt` and `part` are
reserved.

## 6. Classes and fields (schema 1)

Units: durations and delays in ms, pitch in semitones, vibrato depth in cents,
positions along the neck in frets, intensities 0 to 1, strings indexed from 0
(the highest), -1 for none. A field that is missing reads as the default shown,
and import lists it (`CLASS.field`) in its load notification.

| Class | Fields (default) |
|---|---|
| `NOTE` | `ch` (1), `key` (60), `str` (-1), `fret` (-1), `flags` (none): comma list of `pm`, `dead`, `ghost`, `natural`, `pinch`, `artificial`, `tapharm`, `tap`, `hammer`, `pull`, `trill`, `accent`, `staccato`, `letring`, each optionally `:value[:second]` |
| `BEND` | `ch`, `key`, `str`, `art` (whole / half / release / prebend / ghost), `from` (0), `to` (0), `dur` (0), `tech` (bend), `value`, `second`, `curve` (position 0-1 : semitones) |
| `SLIDE` | `ch`, `key`, `str`, `kind` (legato / shift / in / out / up / down), `from`, `to` (frets), `dur`, `tech`, `value`, `second`, `curve` |
| `VIBRATO` | `ch`, `key`, `str`, `rate` (5 Hz), `depth` (0 cents), `delay` (0), `style` (finger / wrist / whammy), `tech`, `value`, `second`, `curve` |
| `WHAMMY` | `ch`, `key`, `str`, `target` (0), `dur` (0), `tech`, `value`, `second`, `curve` |
| `STRUM` | `dir` (down / up), `cv` (crossing velocity, strings per second), `striker` (pick), `mute` (0), `mask` (strings, bit 0 is string 0) |
| `RASGUEADO` | `dir`, `seq` (finger:dir:ms:velocity, comma separated) |
| `PICK` | `ch`, `str`, `material` (celluloid), `thick` (0.71 mm), `angle` (0), `tip` (standard), `wear` (0), `chirp` (0) |
| `SQUEAK` | `trigger` (slide / shift / drag / release), `str`, `start`, `end` (frets, `spec/string-squeak.md` 11), `dur`, `intensity` (string-squeak's "level"), `material` (nickel) |
| `BUZZ` | `str`, `fret`, `dur`, `intensity`, `sitar` (0) |
| `SLIDE_BAR` | `pos` (frets), `path` (ms:frets points over time, `spec/slide-guitar.md` 8), `pressure` (lift / light / full), `slant` (degrees), `material` (glass) |
| `CLANK` | `trigger` (fret), `mask`, `intensity` |
| `CHARACTER` | `what` (seed / environment), `seed` (identifier), `temp` (20 C), `humidity` (45 %) |
| `WORKSHOP` | `slot`, `fit` (the part fitted), `was` (the part it replaced) |
| `BASS_TECH` | `tech` (slap / pop / ghost / lhslap / thump / pluck), `str`, `pos` (0.5), `force` (0.8) |
| `RANGES` | `param`, `on`, `value`, `min`, `max` (the stock range; a value outside it warns on import) |
| `SNAPSHOT` | `slot`, `name`, `morph` (ms) |
| `SECTION` | `name`, `index`, `edge` (start / end) |

`tech`, `value`, `second` and `curve` on the four technique classes are the
notation technique the event came from (`PerformanceScore`), so a score
survives a MIDI round trip exactly.

## 7. Versioning

- A class that changes its fields moves to schema 2 and keeps reading schema 1.
  A reader given a newer schema reads the fields it knows, keeps the rest, and
  warns.
- The wire version (the byte after `LT`, and after `LUTHIER` in the header)
  changes only if the framing does. A newer wire version in an event of an
  unknown class is kept as bytes; in a known class, or in the header, it is
  refused.

## 8. Generic profile

The same file layout without the header, the `LUTHIER-AT` tags, the markers or
the SysEx. Messages sit on the nearest tick. With realism on, each extension
event is one text meta, `LUTHIER: <CLASS> <fields>`, with no `dt`, no `part`
and no identifier fields:

```
LUTHIER: STRUM dir=down cv=200 striker=pick mute=0 mask=63
LUTHIER: SQUEAK trigger=slide dur=140 intensity=0.6 material=phosphor
```

Import reads a Generic file's channel messages only. A track whose name starts
with "Bass" is part 1.

## 9. Live MIDI out

`LuthierSysExOut` writes the SysEx copy of section 4 with no `dt` (the
MIDI buffer offset is the timing), from fixed storage on the audio thread. A
host that does not know Luthier drops it. Another instance reads it with
`LuthierEvents::decodeSysEx`.

## 10. `.midprofile`

```json
{ "magic": "luthier.midprofile", "schema": 1,
  "meta":   { "name": "Stems for mixing", "app": "Luthier" },
  "config": { "profile": "luthier", "ppq": 960, "split": "string",
              "includeRealism": false, "sysex": true, "stripIdentifiers": false,
              "classes": [ "NOTE", "BEND", "STRUM" ] } }
```

A missing or wrong `magic`, or a missing `config`, is refused. PPQ is clamped
to 96 to 3840. Unknown profile, split or class names are warnings. A newer
schema loads what it can and warns.

## 11. Refusals

Import refuses a file it cannot read exactly and names the byte:
`Byte 0x0001A2 (418): a SysEx data byte is above 0x7F`. It checks every length
against the bytes that are present and never reads past them. It also refuses
more than 4,000,000 events.
