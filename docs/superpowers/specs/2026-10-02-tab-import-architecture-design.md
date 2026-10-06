# Robust ASCII Tab Import Architecture

## Purpose

Luthier must import and play the widest practical range of real-world guitar and bass tablature copied from websites, forums, archives, Markdown, email, plain-text files, and user-edited notes. The importer must prefer partial, diagnosable recovery over silent corruption. It must detect tuning and notation instructions whether they appear before or after the tab, and it must preserve enough provenance to explain what was inferred.

This design extends the existing `AsciiTabReader` rather than replacing its working musical semantics. The current reader already handles many conventional ASCII-tab techniques, tunings, string counts, repeats, and timing heuristics. The new architecture inserts explicit document-recovery and lexical layers ahead of semantic conversion.

## Research Basis

The design follows several recurring ideas from prior art:

- Text tab parsers such as TabKit and `tablature-parser` treat fret/technique sequences as structured tokens rather than unrelated characters.
- FretBench demonstrates the value of a diverse, tuning-aware corpus for evaluating tab understanding.
- Optical Music Recognition literature commonly separates primitive/token recognition from syntactic and semantic reconstruction. The Music Tree Notation work explicitly models primitives as a tree/AST-like representation before final semantic interpretation.
- Modern OMR systems use an intermediate structured representation rather than mapping noisy source data directly to a final score. That is the same separation used here for damaged or inconsistent ASCII tab.

References:

- https://github.com/juliocarneiro/tabkit
- https://github.com/1j01/tablature-parser
- https://github.com/jmcapra/FretBench
- https://link.springer.com/article/10.1007/s10032-024-00485-8
- https://link.springer.com/article/10.1007/s10032-023-00432-z

## Architecture

The pipeline has four conceptual stages, three of which are new or substantially upgraded:

1. `TabDocumentNormalizer`
2. `TabDialectLexer`
3. `AsciiTabReader` semantic conversion
4. Existing `PerformanceScore` -> `RiffCompiler` / playback path

The first three stages are specified separately.

```text
raw text/file
    |
    v
TabDocumentNormalizer
    - HTML/Markdown cleanup
    - metadata/legend discovery
    - block classification
    - wrapped/collapsed staff recovery
    - confidence/provenance
    |
    v
NormalizedTabDocument
    |
    v
TabDialectLexer
    - fret tokens
    - technique tokens
    - multi-voice tokens
    - section directives
    - timing/layout coordinates
    - ambiguity markers
    |
    v
TabTokenDocument
    |
    v
AsciiTabReader semantic stage
    - tuning/capo/instrument resolution
    - PerformanceScore construction
    - technique semantics
    - timing heuristic
    - diagnostics
    |
    v
PerformanceScore
    |
    v
RiffCompiler / tab reader playback
```

## Core Principles

### 1. Parse the whole document before the music

Instructions at the end of a tab must affect music that appeared earlier. Therefore tuning, capo, notation legends, instrument notes, section-wide technique instructions, and guitar-part labels are collected across the complete document before semantic note conversion begins.

Examples that must work:

```text
Tuning: Drop D
...
TAB
```

and:

```text
TAB
...
Tuning: Drop D
```

and:

```text
TAB
...
Key:
/ = slide up
\ = slide down
7B = bend
7H = harmonic
7^9 = hammer on
```

### 2. Recover structure before assigning meaning

A wrapped or collapsed staff is first reconstructed as rows/columns. Musical interpretation happens only after that recovery. This prevents a digit in prose from becoming a fret and prevents line-wrap damage from changing string identity.

### 3. Explicit confidence and provenance

Every inferred element can carry:

```cpp
struct TabSourceSpan
{
    int sourceLineStart;
    int sourceLineEnd;
    int sourceColumnStart;
    int sourceColumnEnd;
};

enum class TabConfidence
{
    exact,
    high,
    medium,
    low
};
```

Examples:

- `Tuning: DADGAD` -> `exact`
- six labelled strings `e B G D A D` -> `high`
- six unlabelled rows following a labelled six-string system -> `medium`
- six arbitrary numeric prose lines -> `low`

Low-confidence material must never silently generate normal notes unless a stronger structural constraint resolves it.

### 4. File-defined notation overrides defaults

If the file says:

```text
P = Pop
```

then `P` means pop for that document even if another dialect normally uses `P` differently.

Precedence:

1. Explicit document legend
2. Section-scoped instruction
3. Recognized named dialect convention
4. Luthier default notation
5. Unknown/diagnostic

### 5. No invented retuning

An explicit tuning anywhere in the file is authoritative unless the document clearly contains multiple instruments/parts with different tunings. Inferred string-label tuning may fill missing information. Ambiguous prose must not silently retune playback.

### 6. Partial import is a supported result

A document can import useful music while reporting recoverable damage.

Diagnostics should distinguish:

- exact import
- recovered import
- partial import
- ambiguous sections skipped
- unsupported notation preserved as metadata

## Shared Data Contracts

### `NormalizedTabDocument`

```cpp
struct NormalizedTabDocument
{
    juce::String originalText;
    juce::String normalizedText;
    std::vector<NormalizedTabBlock> blocks;
    TabDocumentMetadata metadata;
    TabNotationDictionary notation;
    TabImportDiagnostics diagnostics;
};
```

### `NormalizedTabBlock`

```cpp
enum class TabBlockKind
{
    staff,
    chordLyrics,
    lyric,
    sectionHeading,
    legend,
    proseInstruction,
    attribution,
    unknown
};

struct NormalizedTabBlock
{
    TabBlockKind kind;
    juce::StringArray lines;
    TabSourceSpan source;
    TabConfidence confidence;
    juce::String sectionName;
    int partIndex = 0;
};
```

### `TabDocumentMetadata`

Must support:

- named tuning
- explicit note-list tuning
- tuning string count
- capo fret
- whether fret numbers are relative to capo or nut when stated
- tempo
- meter
- instrument type/string count
- part/guitar labels
- section names
- global technique directives
- section technique directives
- source attribution text

### `TabTokenDocument`

```cpp
struct TabTokenDocument
{
    std::vector<TabTokenSystem> systems;
    TabDocumentMetadata metadata;
    TabNotationDictionary notation;
    TabImportDiagnostics diagnostics;
};
```

## Multi-Guitar / Multi-Part Policy

The importer must distinguish instructions such as:

```text
Guitar 1
Guitar 2
second guitar plays...
during the fill guitar two plays...
```

Initial implementation may flatten simultaneous parts into separate `PerformanceTrack`s or, where current score APIs constrain this, preserve part identity in normalized/token form and import the primary part with an explicit diagnostic. It must not merge two guitars onto one string merely because their ASCII rows are adjacent.

## Chord/Lyric Policy

Chord-over-lyric formats are document content, not tablature staffs. They should be recognized separately so they are not counted as malformed tab.

The normalizer must support conventional separated chords:

```text
A              C
In my eyes, indisposed
```

and markup-damaged embedded chords such as:

```text
AIn my eyes, Cindisposed
GIn disguise as no one knF#mows
```

Embedded-chord recovery is confidence-scored. It should require evidence such as valid chord grammar plus repeated chord vocabulary/capitalization/location patterns. It must not blindly interpret every initial A-G letter in English words as a chord.

Chord grammar should recognize at minimum:

- roots A-G
- `#`, `b`, Unicode `♯`, `♭`
- major/minor (`m`, `min`, `maj`)
- 5, 6, 7, maj7, m7, 9, 11, 13
- sus2/sus4
- addN
- diminished/augmented
- slash bass notes
- parenthesized extensions where unambiguous

Chord recognition is primarily for readability/metadata in this phase. It must not fabricate a picked guitar performance from chord lyrics unless a future explicit chord-performance feature requests that behavior.

## Tuning Policy

The complete document is scanned before staff semantics.

Supported forms include:

```text
Tuning: D A D G A D
Tuning: DADGAD
Tuning: Eb Ab Db Gb Bb Eb
Tuning: E2 A2 D3 G3 B3 E4
Standard tuning (EADGBE)
Standard (EADGBE)
Play in normal tuning
Tuned down 1/2 step
Half step down
Drop D
Drop C
Open G
```

Footer and post-tab forms have equal authority to header forms.

Conflicting tuning declarations require diagnostics and deterministic resolution:

- same tuning repeated -> no conflict
- generic `standard` plus explicit note list -> explicit list wins
- two incompatible explicit tunings for one part -> mark ambiguous and require part/section resolution; do not guess

## Technique Legend Policy

A document legend may define symbol or token behavior globally or for a section.

Example:

```text
Key:
/ = slide up
\ = slide down
7B = bend
7H = harmonics
7^9 = hammer on
```

The parser should derive patterns, not literal fret 7 rules. `7B = bend` means `<fret>B` is a bend pattern. `7H = harmonics` means `<fret>H` is a harmonic pattern unless contradicted by stronger local context.

## Fractional Fret / Harmonic Positions

Values such as `2.6` must not be rounded into ordinary frets. They are represented as a fractional-position token with raw text preserved. Semantic conversion may map recognized harmonic touch points to harmonic techniques. Unknown fractional positions remain preserved with a warning rather than becoming invented fretted notes.

## Performance Requirements

Import occurs off the audio thread. Correctness takes priority over micro-optimization, but parsing should remain linear or near-linear in document size under normal input.

Limits must prevent pathological input from consuming unbounded memory or time:

- maximum input bytes
- maximum lines
- maximum recovered systems
- maximum strings/system
- maximum columns/system
- maximum warnings retained
- maximum repeat expansion

Existing limits such as repeat and measure clamps must remain or be strengthened.

## Diagnostics Requirements

The existing diagnostics object should be extended to report at least:

- recovered wrapped lines
- reconstructed systems
- inferred string labels
- tuning source line(s)
- legend entries recognized
- legend conflicts
- ambiguous tuning
- ambiguous token count
- chord/lyric blocks detected
- multi-part blocks detected
- fractional positions preserved
- low-confidence blocks skipped
- HTML/Markdown cleanup count

User-facing summaries should remain concise while detailed diagnostics remain available for tests/logging.

## Corpus Strategy

Add a fixture corpus with minimal representative documents rather than copying full copyrighted tabs.

Fixture classes:

1. clean conventional six-string tab
2. header tuning
3. footer tuning
4. legend after tab
5. legend before tab
6. HTML-entity contamination
7. Markdown contamination
8. wrapped six-line staff
9. collapsed rows
10. continuation block without labels
11. bass 3/4/5-string examples
12. seven/eight-string examples
13. alternate tuning
14. multiple guitars
15. section-wide harmonic instruction
16. tremolo/trill prose instruction
17. fractional harmonic positions
18. parenthesized second-guitar notes
19. chord-over-lyric
20. markup-damaged embedded chords
21. malformed/partial tab
22. contradictory tuning
23. unknown legend symbols
24. intentionally ambiguous prose

## Backward Compatibility

Existing `TabDialectTests.cpp` behaviors remain mandatory unless a test is demonstrably asserting a bug. Existing public importer APIs should remain source-compatible where practical.

## Acceptance Criteria

The architecture is complete when:

- tuning/legend metadata is discovered anywhere in the document
- damaged layout is normalized before music semantics
- lexical tokens retain source positions and confidence
- semantic conversion never silently rounds/guesses ambiguous notation
- common existing tab syntax continues to pass
- multi-part and chord/lyric content is classified rather than misparsed as tab
- diagnostics explain every recovery that could materially change playback
- the playback path receives the resolved imported tuning/capo before the first tab note is rendered
