# Tab Semantic Import and Playback Detailed Design

## Responsibility

The semantic stage consumes `TabTokenDocument` and produces a valid `PerformanceScore` plus import diagnostics. It resolves tuning, capo, string identity, technique semantics, timing heuristics, multi-part behavior, and section directives. The playback path must then configure the instrument from the imported score before the first tab note is rendered.

This stage must not repair malformed text layout; that belongs to `TabDocumentNormalizer`. It must not reinterpret raw glyph strings; that belongs to `TabDialectLexer`.

## Primary Requirements

1. Imported tuning is applied before playback begins.
2. Tuning instructions found after the tab are equally valid because metadata was collected before semantic conversion.
3. Capo and tuning are not double-applied.
4. Uncertain lexical material is never silently converted into confident notes.
5. Existing `PerformanceScore`/`RiffCompiler` behavior remains the musical target.
6. Multiple guitar parts remain separable where the score model permits it.
7. Section-wide instructions such as harmonics or tremolo picking are applied to the intended notes only.

## Proposed Entry Point

Existing public `NotationImporter` APIs should remain compatible. Internally the ASCII path becomes conceptually:

```cpp
bool NotationImporter::readAsciiTab(const juce::String& source,
                                    PerformanceScore& out)
{
    NormalizedTabDocument normalized;
    TabTokenDocument tokens;

    if (! normalizer.normalize(source, normalized))
        return false;

    if (! lexer.lex(normalized, tokens))
        return false;

    return semanticReader.buildScore(tokens, out, lastDiagnostics);
}
```

Exact class/function placement may follow existing `NotationImporter` conventions, but the three responsibilities remain separate.

## Tuning Resolution

### Source priority

Resolve tuning from the metadata candidates selected by the normalizer:

1. explicit per-part note-list tuning
2. explicit per-part named tuning
3. explicit global note-list tuning
4. explicit global named tuning
5. complete string-label inference
6. partial string-label inference only when instrument/string count makes it unambiguous
7. standard tuning for the resolved instrument

Footer instructions have the same priority as header instructions.

### Standard/normal tuning

`normal tuning`, `standard tuning`, `Standard (EADGBE)` and equivalent phrases resolve to the standard tuning appropriate for the detected instrument. If the document clearly describes a bass, do not force six-string guitar EADGBE.

### Named alternate tunings

Maintain/extend named tuning support for common guitar tunings, with explicit note lists preferred when available.

At minimum retain/support:

- Standard
- half-step down
- whole-step down
- Drop D
- Drop C
- Drop B
- DADGAD
- Open G
- Open D
- Open E

Additional named tunings may be added through a data table rather than branching parser code.

### Conflicts

If two equally authoritative declarations conflict for the same part and cannot be scoped to separate sections/parts, semantic import must:

- preserve both in diagnostics
- mark tuning ambiguous
- avoid silently choosing the later line
- either use a safe explicit fallback and mark playback as requiring confirmation, or import without auto-retune depending on current UI capabilities

The implementation plan should prefer a non-destructive UI warning over guessed retuning.

## String Count and Ordering

The score must support the instrument/string count recovered from the staff where current Luthier constraints permit it.

Expected cases include:

- 3-string partial bass staff
- 4/5-string bass
- 5-line partial guitar tab
- 6-string guitar
- 7-string guitar
- 8-string guitar
- up to existing `kMaxStrings`

String label ordering must be normalized to the `PerformanceTrack` convention already used in Luthier. Do not reverse twice when the source is low-to-high rather than high-to-low.

## Capo Semantics

The imported score stores capo separately from open-string tuning.

Rules:

1. Tuning describes open strings before capo unless source explicitly states otherwise.
2. Capo raises sounding pitch according to existing score/compiler semantics.
3. Fret numbers in ordinary chord/tab notation are assumed relative to the capo where that is the established convention, but if the source explicitly says frets are absolute/from nut, preserve that distinction.
4. Never add capo semitones into the tuning array and then apply capo again.

Tests must verify Drop D + capo and half-step-down + capo combinations.

## Pitch Derivation

For an ordinary fret token:

```text
soundingPitch = resolvedOpenStringPitch + fret + capoContribution
```

using the exact existing `PerformanceScore`/compiler convention to avoid duplicated pitch logic.

The semantic stage should prefer storing string/fret/capo/tuning and let the existing compiler calculate final playback pitch where that is already how the model works.

## Fractional Positions

`fractionalPosition` tokens do not become integer frets.

Resolution policy:

1. If a scoped harmonic directive and known harmonic-position mapping make the token meaningful, represent it using the closest existing harmonic semantic that preserves intended pitch/technique.
2. If Luthier cannot accurately model the position, preserve it as import annotation/diagnostic and skip or approximate only when the UI explicitly reports the approximation.
3. Never `round()`, `floor()` or `ceil()` a fractional source value into an ordinary fret without explicit notation meaning.

## Technique Mapping

Map canonical lexical meanings to existing `ScoreTechnique::Type` values.

Examples:

```text
hammerOn           -> ScoreTechnique::Type::hammerOn
pullOff            -> ScoreTechnique::Type::pullOff
bend                -> ScoreTechnique::Type::bend
bendRelease         -> ScoreTechnique::Type::bendRelease
preBend             -> ScoreTechnique::Type::preBend
slideBetween        -> slideLegato or current matching type
slideIn             -> slideIn
slideOut            -> slideOut
vibrato             -> vibrato
naturalHarmonic     -> naturalHarmonic
artificialHarmonic  -> artificialHarmonic
palmMute            -> palmMute
tap                 -> tap
slap                -> slap
pop                 -> pop
deadNote            -> deadNote
staccato            -> staccato
accent              -> accent
```

The lexer has already disambiguated glyph meaning; this stage should not inspect raw `H`, `P`, `^`, etc. except for diagnostics.

## Section-Wide Directives

Apply normalized scoped directives after tokenization and before final score validation.

### Harmonics

For:

```text
Intro (all notes harmonics on intro)
```

all eligible notes in that section receive the appropriate harmonic technique unless an individual token explicitly overrides it.

### Tremolo picking

For:

```text
pick the following notes as fast as possible aka tremolo picking
```

preserve a canonical tremolo-picking annotation/technique where the score model supports it. If no dedicated score technique exists, retain a section/note annotation and emit a diagnostic rather than abusing an unrelated technique.

### Trill

`pick these notes fast, aka trill` should only become a trill when the source clearly denotes alternation between pitches or the current model has a compatible trill representation. Do not equate generic fast picking with trill automatically.

### Let ring / palm mute

Span directives apply across the scoped column range or section as defined by normalized/lexical annotations.

## Timing Heuristics

ASCII spacing is approximate, not authoritative notation.

Preserve current Luthier behavior where tested, with these rules:

1. Explicit bar boundaries establish measure segmentation.
2. Token horizontal positions within a bar determine relative onset when no better timing information exists.
3. Multi-digit frets occupy one musical event at their starting column.
4. Reconstructed line wrapping must not insert musical time.
5. Spaces introduced only by normalization must not create timing unless marked source-authentic.
6. Repeated notes close together should remain distinct events.
7. Section prose such as `spacing isn't 100% correct` lowers timing confidence but does not discard notes.

Expose timing confidence in diagnostics when useful.

## Repeats and Structural Instructions

Retain current support for repeat markers and textual counts with existing expansion safety limits.

Recognize structural prose such as:

```text
End with Riff 1 and Riff E.
```

Initial implementation should represent this as a structural reference directive. It may expand known sections only if section identity and repeat order are unambiguous and bounded. Otherwise preserve it as an instruction and diagnostic rather than inventing ordering.

## Multi-Part Semantics

### Separate parts

If the document explicitly identifies multiple guitars and tokens can be attributed to them, create separate tracks/voices where current `PerformanceScore` supports it.

Examples:

```text
During verse guitar two plays...
During the fill guitar two plays...
second guitar plays harmonic on same string 12th fret
```

### Parenthesized second-part notation

When prose establishes parenthetical notation as guitar 2:

```text
0(0)
4(7)
10(5)
(12\0)
```

primary and secondary tokens must remain separate simultaneous events.

If current playback can only audition one track, import both but choose a documented primary audition track or provide a mix where supported. Never merge them onto one string event.

## Chord/Lyric Semantics

Chord tokens and lyrics are preserved as auxiliary song information where model support exists. They do not automatically create guitar note events.

This includes damaged inline chord cases such as:

```text
GIn disguise as no one knF#mows
```

The parser's job in this phase is to keep `G` and `F#m` recognizable and keep the lyric text readable. Generating chord voicings is a separate feature.

## Score Validation

Before reporting successful import:

- every note references a valid string
- integer fret values are within supported limits
- measure/voice references are valid
- tuning contains enough values for used strings
- capo is bounded
- note timing is non-negative and finite
- technique parameters are finite and within accepted ranges
- no low-confidence unknown token has become a normal note without an explicit recovery rule

A partially valid score may succeed with warnings if useful music remains.

## Playback Integration

This requirement is central to the feature.

Before TAB playback begins:

1. obtain the imported track's resolved string count, tuning and capo
2. compare with current instrument/playback state
3. configure the playback engine to the imported tuning/string setup through the existing sanctioned engine/parameter path
4. apply capo exactly once
5. only then compile/start the riff

Do not mutate DSP objects directly from the GUI thread if existing command/snapshot infrastructure is required.

### User visibility

The TAB reader should display at least:

```text
Detected tuning: Drop D (D A D G B E)
Capo: 2
Source: footer instruction
Confidence: explicit
```

For standard tuning:

```text
Detected tuning: Standard (E A D G B E)
```

For ambiguity:

```text
Tuning conflict detected; automatic retune disabled
```

### Non-destructive behavior

TAB playback tuning should not permanently alter an unrelated user guitar preset unless that is already expected by the practice workflow. Preferred behavior is temporary playback/session tuning with restoration after closing/leaving the imported tab, or an explicit user-visible apply action if current architecture requires persistent instrument changes.

The implementation plan must inspect current `RiffCompiler`/Practice ownership and choose the least destructive existing mechanism.

## Diagnostics

Add/report:

- resolved tuning and source
- capo and source
- string count source
- default tuning assumed
- timing confidence
- section directives applied
- techniques skipped/approximated
- fractional positions unresolved
- tracks/parts imported
- structural references not expanded
- auto-retune applied/disabled

## Tests

### Tuning

- standard header
- standard footer
- `Standard (EADGBE)`
- `Play in normal tuning`
- Drop D header/footer
- DADGAD
- Eb standard
- explicit octave note list
- 7-string tuning
- bass standard
- contradictory tuning

### Capo

- standard + capo 2
- Drop D + capo 2
- half-step down + capo
- explicit absolute-fret instruction if supported

### Technique scope

- intro-wide harmonics
- PM span
- individual technique overrides section default
- tremolo instruction preserved

### Multi-part

- labelled Guitar 1/Guitar 2 blocks
- parenthesized secondary notes with declaration
- parentheses without declaration remain ambiguous

### Timing

- existing clean timing tests
- wrapped staff produces identical timing to unwrapped equivalent
- normalizer-added spacing does not change timing

### Playback

- imported alternate tuning is active before first rendered note
- capo is applied once
- leaving preview restores prior tuning if temporary-session design is used
- ambiguous tuning does not silently retune

## Acceptance Criteria

- Tabs with footer tuning play in that tuning from the first note.
- Tabs with header tuning behave identically.
- Capo and tuning produce correct pitches without double transposition.
- The semantic layer never needs to parse raw legend glyphs.
- Section-wide instructions affect only their intended scope.
- Multiple guitars remain logically separate.
- Fractional positions are never silently coerced to integer frets.
- Existing valid imports remain valid.
- The UI can explain which tuning was used and where it came from.
