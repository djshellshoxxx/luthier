# TabDocumentNormalizer Detailed Design

## Responsibility

`TabDocumentNormalizer` converts arbitrary pasted or loaded text into a stable document model without assigning final musical meaning. It removes transport/markup damage, discovers document-wide metadata, classifies blocks, reconstructs likely staff structure, and records confidence/provenance for every recovery.

It must not create `ScoreNote`s, choose final note pitches, or interpret a technique into DSP behavior. Those belong to later layers.

## Inputs and Outputs

```cpp
class TabDocumentNormalizer
{
public:
    struct Options
    {
        int maxInputBytes = 2 * 1024 * 1024;
        int maxLines = 20000;
        int maxRecoveredSystems = 2048;
        int maxColumnsPerSystem = 8192;
        bool recoverWrappedStaffs = true;
        bool recoverCollapsedRows = true;
        bool detectEmbeddedChords = true;
    };

    bool normalize(const juce::String& source,
                   NormalizedTabDocument& out,
                   const Options& options = {});
};
```

On failure, `out.diagnostics` must explain whether the source exceeded safety limits or contained no plausible musical/tab content.

## Processing Phases

The normalizer runs deterministic phases over the complete input. Later phases may use metadata discovered by earlier phases, but no phase may reinterpret already-created music because this layer creates no music.

### Phase 1: Source preservation

Store the original text unchanged in `originalText` for diagnostics and possible future reprocessing.

Line/column coordinates used in provenance refer to this source whenever possible.

### Phase 2: Encoding and markup cleanup

Normalize:

- CRLF, CR and LF to logical lines
- tabs to deterministic column-safe expansion where necessary
- HTML entities including `&#x20;`, `&#xA0;`, `&nbsp;`, `&lt;`, `&gt;`, `&amp;`
- Markdown escapes such as `\*`, `\|`, escaped backslashes where copied as prose
- Markdown emphasis wrappers (`**`, `__`) when they surround entire tab/prose fragments
- code fences while preserving their contents
- Unicode `♯` and `♭` without destroying them; later lexical stages may canonicalize aliases
- typographic apostrophes/quotes only when normalization is semantically harmless
- non-breaking spaces to ordinary spaces while retaining column count information when needed

Do not collapse repeated ordinary spaces globally because spacing frequently carries tab timing/alignment information.

Every cleanup that can move columns must retain a source-to-normalized mapping or a recoverable source span.

## Decorative-line Recognition

Recognize but retain as classified metadata lines such as:

```text
************************************
====================================
------------------------------------
```

A line consisting only of punctuation must not automatically become a tab staff. Context determines whether hyphens belong to a staff or decoration.

## Whole-Document Metadata Discovery

Scan the full normalized document, not just the prefix.

### Tuning

Collect candidates with source locations and confidence.

Exact/high-confidence forms:

```text
Tuning: Drop D
Tuning = E B G D A D
Standard (EADGBE)
Standard tuning (E A D G B E)
Play in normal tuning
DADGAD tuning
Eb Ab Db Gb Bb Eb
```

Recognize common natural-language aliases:

- standard / normal tuning
- half-step down / 1/2 step down
- whole-step down
- Drop D/C/B/A variants when musically valid for the detected instrument
- Open G/D/E/A and other explicitly supported named tunings

A bare note sequence should become a tuning candidate only if surrounding language or structural evidence makes tuning likely.

### Capo

Recognize at minimum:

```text
Capo 2
Capo: 2
Capo on 2nd fret
capo @ 2
```

Also detect statements that clarify whether fret numbers are written relative to the capo or absolute nut position. Preserve ambiguity when not stated.

### Tempo and meter

Collect conventional forms such as BPM/tempo and 3/4, 4/4, 6/8 when clearly identified as metadata.

### Instrument and part labels

Recognize:

```text
Guitar 1
Guitar I
Gtr. 2
second guitar
rhythm guitar
lead guitar
bass
12-string
7-string guitar
8-string guitar
```

Part labels may establish scope for later blocks.

### Section labels

Recognize conventional headings:

```text
Intro
Verse
Verse 2
Chorus
Bridge
Solo
Riff 1
Riff E
Outro
Interlude
Fill
```

Parenthetical instructions attached to a section become section metadata rather than staff text.

## Legend Discovery

Detect legend/key blocks anywhere in the file.

Trigger phrases include:

```text
Key:
Legend:
Notation:
Symbols:
```

and dense definition runs such as:

```text
| b  Bend
| \  Slide down
| ~  Vibrato
| P  Pop
```

Supported definition separators:

- `=`
- `:`
- one or more spaces when the left side is symbol-like and the right side is technique prose
- leading `|`
- bullets (`-`, `*`, `•`)

Pattern generalization is required. For example:

```text
7B = bend
7H = harmonics
7^9 = hammer on
```

should yield generalized patterns equivalent to:

```text
<fret>B -> bend
<fret>H -> harmonic
<fret>^<higher-fret> -> hammer-on
```

The normalizer records the declared pattern and meaning; the lexer decides how to tokenize matching staff text.

## Prose Technique Instructions

Recognize section/global prose that changes interpretation or performance:

```text
all notes harmonics on intro
pick these notes fast, aka trill
pick the following notes as fast as possible aka tremolo picking
all notes are still tremolo picking
palm mute throughout
let ring
second guitar plays harmonic on same string 12th fret
```

Represent these as directives with explicit scope:

```cpp
enum class DirectiveScope { document, part, section, nextSystem, nextPhrase };

struct TabDirective
{
    juce::String canonicalName;
    juce::String rawText;
    DirectiveScope scope;
    TabSourceSpan source;
    TabConfidence confidence;
};
```

The normalizer should identify intent but must not attach the directive to individual score notes yet.

## Block Classification

Partition the normalized lines into coherent blocks.

### Staff block

Evidence may include:

- repeated string labels followed by `|`
- multiple near-equal-width lines dominated by `-`, digits, bars and technique symbols
- known string-note labels in plausible high-to-low or low-to-high order
- continuation of a recently established staff shape

### Chord/lyric block

Evidence includes valid chord tokens aligned above or embedded in lyric text without staff-line structure.

### Lyric block

Human-language lines that do not fit notation structure.

### Legend

Symbol-definition block.

### Prose instruction

Natural-language musical instruction associated with nearby sections/systems.

### Attribution

Email addresses, `Tabbed by`, `Sent by`, copyright/source notes, URLs and signatures.

### Unknown

Preserved but excluded from semantic parsing unless later context resolves it.

## Staff Reconstruction

This is the normalizer's hardest responsibility.

### Labelled complete systems

A conventional six-line system is high confidence when string labels and columns align.

### Unlabelled systems

An unlabelled 4-8-row block may be recovered if:

- it follows a labelled system with the same row count, and
- widths/content are staff-like, or
- instrument metadata strongly constrains string count.

It should inherit string identity from the nearest compatible labelled system in the same part/section only.

### Wrapped rows

When a website wraps long lines, recover continuation pieces only when row ordering and width evidence are strong.

Example logical source:

```text
E|--------very-long-row--------
B|--------very-long-row--------
...
```

may arrive as multiple physical chunks. Recovery should use:

- expected string count
- line character class
- recurring row order
- equal/near-equal chunk lengths
- nearby bar alignment

Do not join arbitrary prose merely because it follows a staff.

### Collapsed systems

Some paste paths concatenate several staff rows onto one physical line. The normalizer may split these if strong delimiters and repeated staff signatures exist, for example multiple `E|... A|... D|...` sequences.

Collapsed unlabelled systems are only medium confidence at best and require stronger surrounding context.

### Lost labels

For rows that start directly with `-----`, preserve inherited string identity separately from text. Do not synthesize visible labels into the source text unless the normalized model records them as inferred.

## Chord/Lyric Recovery

### Conventional chord rows

Recognize whitespace-separated chord sequences.

### Embedded chords caused by markup loss

Handle strings such as:

```text
AIn my eyes, Cindisposed
GIn disguise as no one knF♯mows
The Asun Gin my disB♭grace
```

The detector must score candidates using multiple signals:

1. Valid chord grammar.
2. Uppercase or accidental boundary inconsistent with ordinary spelling.
3. Repeated chord vocabulary across adjacent lines.
4. Candidate positions that form plausible sparse annotations rather than consuming most words.
5. Nearby tuning/key/chord-sheet context.

Counterexample:

```text
And sometimes far too long
```

must not automatically become chord A + `nd sometimes...` merely because it begins with A.

A candidate below the confidence threshold remains untouched lyric text.

## Conflict Resolution

Metadata candidates carry specificity.

Suggested order:

1. Explicit note-list tuning for a named part.
2. Explicit named tuning for a named part.
3. Explicit note-list tuning globally.
4. Explicit named tuning globally.
5. String-label inference.
6. Default instrument tuning.

Two equally specific conflicting declarations should produce an ambiguity diagnostic rather than last-write-wins behavior.

A later footer declaration is not weaker merely because it appears later.

## Safety Limits

Normalization must be bounded. Reject or truncate according to options when input exceeds configured size/line/system/column limits. Emit a clear diagnostic.

No regular expression or parsing strategy may have catastrophic backtracking on adversarial text.

## Diagnostics

Add counters/details for:

- entities decoded
- Markdown wrappers removed
- blocks by type
- staffs reconstructed
- wrapped rows joined
- collapsed rows split
- inherited string labels
- tuning candidates and selected source
- capo candidates
- legend entries
- prose directives
- embedded-chord recoveries
- low-confidence recoveries declined
- metadata conflicts

## Test Requirements

Create focused tests for:

1. tuning at beginning
2. tuning at end
3. tuning both places, identical
4. tuning conflict
5. legend before tab
6. legend after tab
7. HTML entities
8. Markdown emphasis contamination
9. fully labelled staff
10. unlabelled continuation
11. wrapped staff
12. collapsed labelled staff
13. lyrics between staff systems
14. section headings
15. multi-guitar labels
16. attribution footer
17. embedded chords
18. ordinary prose beginning with chord-letter words
19. section-wide harmonic directive
20. tremolo/trill prose directive
21. pathological oversized input

## Acceptance Criteria

- Metadata at either end of the document is equally discoverable.
- The output contains coherent classified blocks with source spans.
- Recovery never destroys the original source text.
- Low-confidence structural guesses are reported/skipped rather than silently promoted.
- Existing clean ASCII tabs normalize without musical-content changes.
- The `Ugly Truth` style document can be divided into section, instruction, staff and attribution blocks even when rows are badly wrapped.
- The `Black Hole Sun` style embedded-chord text is classified as chord/lyric content without misclassifying ordinary English words.
