# 05 Mistakes: late and wrong notes (roadmap 3.11)

**TQ IDs:** TQ-12
**ED:** 3
**Status:** partial. `humanize.miss_pct` exists (`rhythm-engine.md` 137); no late or wrong notes.
**Depends on:** `humanize` (`rhythm-engine.md`), `PhraseBoundaries` from 04, the seeded streams from `character-wear.md` 1.

**Summary.** Two optional humanize controls add occasional late notes and occasional wrong notes. Both are off by default.

## 1. User-facing behaviour
- Humanize card: Late (0-5 %) and Wrong (0-2 %). Tooltip: "Occasionally late or wrong notes, as real playing has".
- 0 is off. The miss control is unchanged.

## 2. Engine / DSP design
- Applied on the message thread at compile time, per note, in this order:
  1. `miss_pct` (existing): the note is removed. A missed note is never late or wrong.
  2. `mistake_wrong_pct`: with probability p, the pitch is replaced by a neighbour in the current scale, one or two semitones away (chosen by the seed). Scale comes from the riff key (the existing scale data in the riff).
  3. `mistake_late_pct`: with probability p, the onset shifts later by a seeded value in 40-120 ms (uniform). Only the onset moves; the note's length is shortened by the same amount so the offset is unchanged.
- **Playability.** A wrong note must sit on the same string at a fret within the hand's span. If the neighbour is not playable on that string, the wrong draw is discarded (no mistake on that note). Late notes need no re-voicing.
- **First note of a phrase is exempt** from both. Phrase starts come from `PhraseBoundaries` (04).
- **Seeding.** Uses the character seed with a salt unique to this technique (see INDEX cross-cutting table). It must be a separate stream from timing jitter and from snap noise, so TQ-14 holds.
- Probability draws are per note; a single draw per note decides both late and wrong to avoid double-counting (wrong is checked first; a note that is wrong is not also late).
- No audio-thread work beyond the note's existing events.

## 3. Data model and parameters
| ID | Range | Default | Automatable | Control |
|---|---|---|---|---|
| `mistake_late_pct` | 0-5 % | 0 | yes | Humanize card |
| `mistake_wrong_pct` | 0-2 % | 0 | yes | Humanize card |

## 4. State, file format, migration
- Both are humanize settings, appended with default 0. v1 presets load at 0.
- Compiled riffs record the altered note (pitch or onset), so the same riff re-renders identically without re-drawing unless the seed changes. Note: this makes the seed part of riff identity.

## 5. Edition gating
- Pro. Free presets with non-zero values load at 0 with a notice.

## 6. Performance budget
- Under 0.01 units. Compile-time only.

## 7. Test plan
- **TQ-12.** At late 5 % over 10,000 notes, the late fraction is 4-6 %. Assert no late note is the first note of a phrase.
- Late shift for each late note lies in 40-120 ms.
- Wrong at 2 % over 10,000 notes: wrong fraction 1.5-2.5 %; every wrong note is 1 or 2 semitones away and in the scale; no wrong note is the first of a phrase.
- A note with `miss_pct` 100 is never late or wrong.
- Playability: no wrong note falls outside the hand span on its string.

## 8. Open decisions
- Mistakes default off (roadmap open question 3; confirm with the rhythm owner).
- Wrong notes are not scale-aware for chromatic riffs. Behaviour then is +/-1 or 2 semitones, not scale-based. Confirm.
