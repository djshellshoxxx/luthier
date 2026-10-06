# TAB Import Research Notes

Date: 2026-10-02
Branch: `codex/deep-audit-fixes-final2`

## Why this research was done

Real-world ASCII guitar/bass tabs are not one format. They are informal documents combining monospaced staffs, prose, chord sheets, tuning/capo instructions, legends, multiple guitar parts, lyrics, section labels and website-copy artifacts. Luthier already has a capable dialect reader, but examples supplied during audit exposed document-recovery problems that should be solved before adding more syntax exceptions.

## Current Luthier strengths

`Source/Notation/AsciiTabReader.cpp` and `Source/Tests/TabDialectTests.cpp` already cover a strong base:

- six-string and bass tab
- seven-string inference
- alternate tunings
- named and explicit-note tunings
- capo/metre metadata
- hammer-ons and pull-offs
- bends, releases and pre-bends
- slides
- vibrato
- natural/artificial harmonics
- palm mute
- taps
- slap/pop
- dead notes
- ties
- repeats
- ragged/unlabelled staffs
- timing from horizontal grid
- partial-file recovery and diagnostics

The new work should preserve this behavior and put document recovery ahead of it rather than replacing it.

## User-supplied failure classes

### Tuning at the end

Tabs sometimes place `Tuning:`, `Standard (EADGBE)`, `Play in normal tuning`, etc. after the music. Metadata therefore must be scanned across the entire document before playback configuration.

### Legends at the end

Example:

```text
| b  Bend
| \  Slide down
| ~  Vibrato
| P  Pop
```

These definitions may apply to notation that occurred earlier. A one-pass music-first parser cannot safely interpret the earlier symbols.

### Example-specific legend dialect

```text
7B = bend
7H = harmonics
7^9 = hammer on
```

The literal fret number is an example, not part of the rule. The importer needs generalized patterns such as `<fret>B` and `<fret>^<higher-fret>`.

### Web/Markdown damage

Examples contained:

- `&#x20;`
- `&#xA0;`
- Markdown `**...**`
- escaped backslashes
- multiple staff rows collapsed onto one line
- long staffs wrapped across many lines
- missing string labels on continuation systems

This requires a document-normalization/reconstruction stage.

### Embedded chord markup loss

Example shape:

```text
AIn my eyes, Cindisposed
GIn disguise as no one knF♯mows
```

A naive regex would also misread normal words such as `And` as `A` + lyric. Embedded chord recovery therefore needs confidence scoring using chord grammar plus cross-line context.

### Multiple guitars

Examples include prose such as `During the fill guitar two plays` and parenthesized secondary notes such as `4(7)`. The parser must preserve part identity instead of flattening both guitars into one string event.

### Section-wide performance instructions

Examples:

```text
all notes harmonics on intro
pick these notes fast, aka trill
pick the following notes as fast as possible aka tremolo picking
```

These are scoped directives rather than standalone fret glyphs.

### Fractional positions

Old/author-specific tabs may contain values like `2.6`. These must not be rounded to ordinary frets. Preserve them as fractional-position tokens until semantics can determine whether they represent a harmonic touch point or another convention.

## Open-source prior art

### TabKit

https://github.com/juliocarneiro/tabkit

Useful ideas:

- parse ASCII into a structured representation
- treat multi-digit frets as one event
- explicit technique markers for hammer/pull/slide/bend/vibrato
- keep parsing separate from rendering/playback

Takeaway for Luthier: tokenize notation before semantic playback conversion.

### tablature-parser

https://github.com/1j01/tablature-parser

Useful because its README explicitly notes the hard parts that simple parsers often leave unresolved: alternate tunings, articulations, other instruments and vague spacing/timing.

Takeaway: do not pretend ASCII spacing is exact rhythm; preserve coordinates and confidence.

### TABPlayer

https://github.com/BudBCoulson/TABPlayer

A small example of converting ASCII tab to notes plus playing techniques before playback.

Takeaway: parser output should be semantic/structured, not coupled directly to synthesizer actions.

### FretBench

https://github.com/jmcapra/FretBench

A benchmark corpus for reasoning over guitar tablature across multiple tunings.

Takeaway: compatibility needs corpus-style regression fixtures across tuning and notation classes, not only isolated unit strings.

### gtrsnipe

https://github.com/scottvr/gtrsnipe

Supports multiple text/symbolic formats and shares a fretboard mapping model across input/output/playback features.

Takeaway: centralize tuning/string/fret semantics rather than reimplementing pitch rules in every importer/player.

## Research literature

### Music Tree Notation / OMR representation

https://link.springer.com/article/10.1007/s10032-024-00485-8

The paper argues for normalizing recognized musical primitives into a structured tree/AST-like representation before converting to final formats.

Takeaway: Luthier should use a token/intermediate representation between noisy source text and `PerformanceScore`.

### End-to-end OMR and intermediate encodings

https://link.springer.com/article/10.1007/s10032-023-00432-z

The background discussion describes the traditional separation between primitive recognition and recovering syntactic relationships, and discusses structured text encodings for music.

Takeaway: recovery, lexical recognition and semantic interpretation should remain distinct stages even though Luthier's source is text rather than an image.

## Resulting architecture

```text
raw text
  -> TabDocumentNormalizer
  -> NormalizedTabDocument
  -> TabDialectLexer
  -> TabTokenDocument
  -> semantic AsciiTabReader stage
  -> PerformanceScore
  -> RiffCompiler / playback
```

The detailed specs are:

- `docs/superpowers/specs/2026-10-02-tab-import-architecture-design.md`
- `docs/superpowers/specs/2026-10-02-tab-document-normalizer-design.md`
- `docs/superpowers/specs/2026-10-02-tab-dialect-lexer-design.md`
- `docs/superpowers/specs/2026-10-02-tab-semantic-import-playback-design.md`

## Implementation principles carried forward

1. Scan the complete document before interpreting music.
2. File-defined legends override default dialect meanings.
3. Footer tuning has the same authority as header tuning.
4. Preserve source spans and confidence for recovered structures.
5. Never turn low-confidence prose into normal notes silently.
6. Never coerce fractional fret positions into integers silently.
7. Preserve multiple guitar parts separately.
8. Chord/lyric recognition does not automatically synthesize chord voicings.
9. Apply imported tuning/capo before the first playback note.
10. Keep the existing `PerformanceScore` and tested AsciiTabReader musical behavior as the compatibility target.
