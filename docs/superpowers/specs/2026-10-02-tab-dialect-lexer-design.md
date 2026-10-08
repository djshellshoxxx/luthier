# TabDialectLexer Detailed Design

## Responsibility

`TabDialectLexer` converts normalized staff/document blocks into a structured, source-aware token stream. It recognizes fret values, columns, techniques, bar/repeat markers, multi-voice annotations, section directives and uncertain constructs without deciding final pitch, timing or playback behavior.

It must consume `NormalizedTabDocument` and produce `TabTokenDocument`.

## Design Goals

1. Support multiple historical and web-tab dialects through aliases and document-defined legends.
2. Preserve horizontal source coordinates because ASCII spacing often carries approximate timing.
3. Preserve uncertainty instead of coercing unknown syntax into a known technique.
4. Generalize legend examples (`7B = bend`) into token patterns (`<fret>B`).
5. Keep lexical behavior deterministic and testable independently from `PerformanceScore`.

## Core Types

```cpp
enum class TabTokenKind
{
    fret,
    fractionalPosition,
    deadNote,
    bar,
    repeatStart,
    repeatEnd,
    repeatCount,
    technique,
    annotation,
    unknown
};

struct TabToken
{
    TabTokenKind kind;
    int row = -1;
    int columnStart = 0;
    int columnEnd = 0;
    int integerValue = 0;
    double numericValue = 0.0;
    juce::String rawText;
    juce::String canonicalMeaning;
    TabSourceSpan source;
    TabConfidence confidence = TabConfidence::exact;
};
```

`columnStart` and `columnEnd` refer to the recovered logical staff row rather than arbitrary original-line wrapping.

## Lexical Context

```cpp
struct TabLexContext
{
    const TabNotationDictionary* dictionary = nullptr;
    const TabDocumentMetadata* metadata = nullptr;
    juce::String sectionName;
    int partIndex = 0;
    int stringCount = 0;
};
```

Each staff system is lexed with context produced by the normalizer.

## Base Fret Grammar

Recognize integers from 0 through the configured maximum fret.

Multi-digit values are a single fret token when within the supported range.

Values above maximum are not silently clamped. Existing compatible behavior for obviously concatenated digits may be retained only where current tests specify it; otherwise emit an ambiguity/diagnostic.

Examples:

```text
0
7
12
24
```

## Fractional Position Grammar

Recognize numeric forms such as:

```text
2.6
3.2
```

as `fractionalPosition`, never ordinary fret numbers.

Store:

- raw value
- floating numeric value
- row/column
- any applicable section directive such as `all notes harmonics`

The lexer must not decide whether `2.6` is a harmonic touch point, microtonal fret, typo or author-specific notation.

## Default Technique Aliases

The built-in dictionary should cover at least:

### Hammer-on

```text
7h9
7H9
7^9   when the document/default dialect defines caret as hammer and target is higher
```

### Pull-off

```text
9p7
9P7
9^7   when the document/default dialect defines caret by direction
```

### Bends

```text
7b9
7b
7B
7bfull
7b1/2
7^
7pb9
```

Document legends override ambiguous meanings.

### Bend release

```text
7b9r7
7br
```

### Slides

```text
5/7
7\5
5s7
/9
9\
```

Canonical technique should preserve direction and whether it is slide-in, slide-out or between two frets.

### Vibrato

```text
7~
7~~
7v
```

### Harmonics

```text
<12>
12*
12H
12NH
[7]      where existing dialect defines artificial harmonic
```

Because `H` can mean hammer-on in some dialects, adjacency and document legend control interpretation.

### Palm mute

```text
PM----
P.M.---
7PM
```

Support spans where a marker line applies to multiple notes/columns.

### Dead/muted notes

```text
x
X
```

### Tap

```text
t12
T12
5h8t12
```

### Slap/pop

Support bass-style `S`/`P` only where built-in dialect or document legend makes the meaning sufficiently clear.

### Accents/staccato/ties

Preserve current Luthier forms such as `>`, `.`, `=` where established by tests.

## Legend-Derived Grammar

The lexer receives generalized definitions from `TabNotationDictionary`.

Example document legend:

```text
7B = bend
7H = harmonics
7^9 = hammer on
```

Dictionary patterns:

```text
<fret>B                -> bend
<fret>H                -> naturalHarmonic
<fret>^<higher-fret>   -> hammerOn
```

The generalized pattern must constrain token classes rather than use unrestricted regular expressions.

Suggested pattern atoms:

```cpp
enum class TabPatternAtomKind
{
    literal,
    fret,
    higherFret,
    lowerFret,
    whitespace,
    endOfToken
};
```

This provides bounded matching and prevents catastrophic regular expressions from user-defined legends.

## Contextual Disambiguation

### `H`

`7H9` is likely hammer-on in a conventional dialect; `7H` may be harmonic if the legend explicitly says so.

Priority:

1. document legend exact/generalized pattern
2. section directive
3. unambiguous built-in token shape
4. contextual direction/neighbor fret
5. unknown token

### `P`

Can mean pull-off or pop. `7p5` between frets is pull-off. A standalone prefix/suffix in a bass tab may be pop when dialect/context supports it.

### `^`

May be bend or hammer/pull depending on the file. The document's legend wins. Without legend, only forms already established in current Luthier tests are accepted.

## Section-Wide Technique Directives

If the normalizer reports:

```text
Intro (all notes harmonics on intro)
```

tokens within that scoped section receive an annotation such as:

```cpp
TabTokenAnnotation { "harmonic", scopeSource, confidence };
```

The lexer does not yet convert every fret token into a `ScoreTechnique`; it attaches lexical annotations for the semantic stage.

Likewise:

```text
pick the following notes as fast as possible aka tremolo picking
```

creates a scoped `tremoloPicking` annotation.

## Multiple Voices / Guitar Parts

Recognize notation such as:

```text
--0(0)--2(0)--4(7)--10(5)--
```

when surrounding prose states that parenthesized numbers are guitar 2.

The lexer should emit two voice/part tokens sharing horizontal coordinates, e.g.:

```cpp
struct TabVoiceToken
{
    int partIndex;
    TabToken token;
};
```

A parenthesized fret without multi-part context remains an annotation/ambiguous construct rather than automatically becoming guitar 2.

For forms such as:

```text
(12\0)
```

the complete secondary gesture must remain associated with the secondary part.

## String Rows and Logical Columns

Each reconstructed staff row has stable string identity supplied by the normalizer when known.

The lexer records fret/technique coordinates using logical columns. Column width occupied by a multi-digit fret must not shift later source timing unexpectedly; semantic timing should reference token starts and normalized staff width rather than token character count alone.

## Bar and Repeat Tokens

Recognize:

```text
|
|:
:|
x3
(play 4 times)
```

Repeat count prose already classified by the normalizer can become structured repeat directives. Clamp expansion later in semantic conversion according to safety policy.

## Chord Tokens

Chord/lyric blocks use a separate lexical submode.

```cpp
struct ChordToken
{
    int textOffset;
    juce::String raw;
    juce::String canonical;
    int rootPitchClass;
    int bassPitchClass = -1;
    TabConfidence confidence;
};
```

Normalize Unicode accidentals to canonical equivalents while preserving raw spelling:

```text
F♯m -> F#m
B♭  -> Bb
```

Do not generate fret/string events from chord tokens in this version.

## Unknown Syntax

Unknown characters inside an otherwise valid staff are retained in diagnostics with row/column/source position.

Policy:

- harmless decorative/fill characters may be ignored with a count
- unknown characters adjacent to a fret are preserved as unknown modifiers
- an unknown modifier must not silently map to a technique
- too many unknown glyphs may lower system confidence but should not necessarily discard known notes

## Timing Preservation Contract

The lexer does not assign beats. It must preserve enough layout information for the semantic layer to do so:

- system width
- bar boundaries
- token start/end columns
- row lengths
- wrapped-segment boundaries
- whether spaces came from source or normalizer reconstruction where material

## Diagnostics

Report at minimum:

- token counts by kind
- document-legend patterns used
- built-in aliases used
- ambiguous tokens
- unknown glyphs
- fractional positions
- multi-part parenthetical tokens
- section-wide annotations
- conflicts between legend and built-in dialect

## Test Matrix

Technique fixtures must cover positive and ambiguity cases:

- `7h9`, `9p7`, `7^9`, `9^7`
- `7B` with legend vs without legend
- `7H` with harmonic legend
- `7H9` conventional hammer form
- `7b9r7`
- `/7`, `7\`, `5/7`, `7\5`
- `7~`, `7v`
- `<12>`, `12*`, `12H`
- `x`, `X`
- `T12`
- PM spans
- fractional `2.6`
- unknown modifier after fret
- `0(0)` secondary part
- `(12\0)` secondary slide
- Unicode chord accidentals
- ordinary lyrics not tokenized as tab

## Acceptance Criteria

- All tokens retain source and logical staff coordinates.
- A file-defined legend changes lexical meaning deterministically.
- Fractional positions remain fractional.
- Ambiguous symbols remain explicit rather than guessed.
- Existing conventional notation tests remain compatible.
- Multi-part parenthetical notation can be represented without flattening into the primary part.
- Section directives survive into the semantic layer with correct scope.
