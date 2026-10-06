# Robust ASCII Tab Import Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the three-stage robust ASCII-tab import pipeline so Luthier can recover, tokenize and correctly play a wide range of real-world tab formats, including tuning/legend instructions before or after the tab.

**Architecture:** Add a document-normalization layer ahead of the existing ASCII semantic reader, then a dialect lexer that converts normalized staffs into source-positioned tokens, then adapt `AsciiTabReader`/playback to consume the resolved metadata and tokens. Existing `PerformanceScore`, technique semantics and clean-tab behavior remain the compatibility baseline.

**Tech Stack:** C++17, JUCE, existing Luthier test framework, existing `PerformanceScore`/`RiffCompiler` pipeline.

**Spec:** `docs/superpowers/specs/2026-10-02-tab-import-architecture-design.md`, plus the three detailed layer specs in the same directory.

## Global Constraints

- Preserve existing `NotationImporter` public APIs where practical.
- Existing `TabDialectTests.cpp` behavior remains mandatory unless it proves an existing bug.
- Parse the complete document before semantic conversion so footer metadata has equal authority to header metadata.
- File-defined notation legends override built-in dialect defaults.
- Ambiguous/low-confidence material must not silently generate notes or silently retune playback.
- Keep parsing off the audio thread.
- Preserve original source text and source spans for recovered material.
- Bound input size, lines, recovered systems, columns, warnings and repeat expansion.
- Parameter IDs remain append-only; this feature must not require parameter-ID changes.

## Review Focus

- A tuning declaration after the final staff must configure playback before the first note is rendered.
- Wrapped/unlabelled continuation staffs must not inherit the wrong string identities across parts or sections.
- Ordinary prose beginning with A-G must not be split into false embedded chords.
- File-defined `H`, `B`, `P`, `^`, `/`, and `\\` meanings must override built-in notation safely.
- Multi-guitar annotations must not merge two simultaneous parts onto one string/track silently.

---

### Task 1: Shared normalized document model

**Files:**
- Create: `Source/Notation/TabDocument.h`
- Create: `Source/Tests/TabDocumentNormalizerTests.cpp`

**Interfaces:**
- Produces: `TabSourceSpan`, `TabConfidence`, `TabBlockKind`, `DirectiveScope`, `TabDirective`, `TabNotationDefinition`, `TabDocumentMetadata`, `NormalizedTabBlock`, `NormalizedTabDocument`.

- [ ] Add compile-focused tests that construct the shared model and verify default values/source spans.
- [ ] Add the model types without semantic parsing logic.
- [ ] Verify focused tests compile/pass when a local/CI runner is available.
- [ ] Commit.

### Task 2: Markup cleanup and whole-document metadata discovery

**Files:**
- Create: `Source/Notation/TabDocumentNormalizer.h`
- Create: `Source/Notation/TabDocumentNormalizer.cpp`
- Modify: `Source/Tests/TabDocumentNormalizerTests.cpp`

**Interfaces:**
- Consumes: raw `juce::String`.
- Produces: `bool normalize(const juce::String&, NormalizedTabDocument&, const Options&)`.

- [ ] Add failing tests for footer/header tuning, identical repeated tuning, conflicting tuning, capo, HTML entities and Markdown contamination.
- [ ] Implement bounded source cleanup while preserving repeated spaces.
- [ ] Implement full-document tuning/capo candidate discovery with specificity/conflict diagnostics.
- [ ] Verify focused tests, then existing `TabDialectTests`.
- [ ] Commit.

### Task 3: Legend, directives and block classification

**Files:**
- Modify: `Source/Notation/TabDocumentNormalizer.cpp`
- Modify: `Source/Tests/TabDocumentNormalizerTests.cpp`

**Interfaces:**
- Produces classified `NormalizedTabBlock`s, generalized notation definitions and scoped directives.

- [ ] Add failing tests for legends before/after tab, `7B`, `7H`, `7^9`, harmonic/tremolo prose, section headings, multi-guitar labels and attribution footers.
- [ ] Implement legend block discovery and pattern generalization.
- [ ] Implement section/part/directive scope tracking.
- [ ] Implement staff/chordLyrics/lyric/legend/prose/attribution classification.
- [ ] Verify focused tests and existing tests.
- [ ] Commit.

### Task 4: Staff reconstruction and embedded chords

**Files:**
- Modify: `Source/Notation/TabDocumentNormalizer.cpp`
- Modify: `Source/Tests/TabDocumentNormalizerTests.cpp`
- Create fixtures under: `Source/Tests/Fixtures/AsciiDialectCorpus/`

**Interfaces:**
- Produces reconstructed staff blocks with inherited string identity/confidence, and chord/lyric blocks with recovered chord anchors.

- [ ] Add failing fixtures for wrapped staffs, collapsed labelled rows, unlabelled continuations, lyrics between staffs and the minimized `Ugly Truth` layout pattern.
- [ ] Add failing fixtures for conventional chord rows, damaged embedded chords and ordinary English counterexamples.
- [ ] Implement conservative staff reconstruction using row count/order/width/context evidence.
- [ ] Implement confidence-scored embedded chord recovery with Unicode accidental normalization.
- [ ] Verify corpus and existing tests.
- [ ] Commit.

### Task 5: Dialect token model and lexer

**Files:**
- Create: `Source/Notation/TabDialectLexer.h`
- Create: `Source/Notation/TabDialectLexer.cpp`
- Create: `Source/Tests/TabDialectLexerTests.cpp`

**Interfaces:**
- Consumes: `NormalizedTabDocument`.
- Produces: `TabTokenDocument` with source-positioned fret, technique, timing/layout, voice and ambiguity tokens.

- [ ] Add failing tests for frets, multi-digit frets, `h/p/^`, bends/releases, slides, harmonics, vibrato, mutes, taps, slap/pop and document-defined legends.
- [ ] Add tests for fractional positions (`2.6`) that prove they are not rounded to ordinary frets.
- [ ] Add tests for secondary-guitar parenthesized notation such as `4(7)` and `(12\\0)`.
- [ ] Implement tokenization preserving source columns/confidence and using legend precedence.
- [ ] Verify lexer and legacy dialect tests.
- [ ] Commit.

### Task 6: Semantic importer integration

**Files:**
- Modify: `Source/Notation/AsciiTabReader.cpp`
- Modify: corresponding importer header(s)
- Modify: `Source/Tests/TabDialectTests.cpp`

**Interfaces:**
- Consumes: normalized/token document.
- Produces: existing `PerformanceScore` plus enriched `TabImportDiagnostics`.

- [ ] Add failing end-to-end tests for footer tuning, footer legends, section-wide harmonic/tremolo directives, fractional-position preservation and multi-part diagnostics.
- [ ] Route `readAsciiTab` through normalizer and lexer while retaining clean-tab compatibility.
- [ ] Resolve tuning/capo using the specified precedence rules.
- [ ] Map recognized tokens/directives into existing `ScoreTechnique` semantics.
- [ ] Preserve unsupported/ambiguous material as diagnostics rather than invented notes.
- [ ] Verify all notation/import tests.
- [ ] Commit.

### Task 7: Playback tuning application

**Files:**
- Modify: practice TAB/riff playback component(s) and `RiffCompiler` integration as identified by source tracing.
- Modify/Add: relevant practice playback tests.

**Interfaces:**
- Consumes: `PerformanceTrack::tuning`, `numStrings`, `capoFret` and provenance.
- Produces: tab playback configured to the imported tuning/capo before its first note.

- [ ] Add failing tests proving Standard, Drop D, Eb Standard, DADGAD, explicit note-list and footer tunings affect rendered pitch correctly.
- [ ] Add tests proving capo is applied exactly once.
- [ ] Configure the playback guitar/temporary performance context from imported tuning before playback starts.
- [ ] Surface detected tuning/source in TAB reader status without permanently mutating unrelated user guitar state where avoidable.
- [ ] Verify practice/riff tests and notation tests.
- [ ] Commit.

### Task 8: Corpus regression and diagnostics completion

**Files:**
- Add/minimize fixtures under `Source/Tests/Fixtures/AsciiDialectCorpus/`
- Modify diagnostics structures and tests.

**Interfaces:**
- Produces stable compatibility corpus and detailed recovery diagnostics.

- [ ] Cover all 24 fixture classes specified by the architecture spec with minimized/non-copyrighted examples.
- [ ] Assert system/string/tuning/important-note/technique counts and `no invented notes` invariants rather than exact formatting.
- [ ] Add limits/adversarial tests for oversized input, excessive rows/columns/repeats and ambiguous prose.
- [ ] Verify complete `LuthierTests` suite when runner is available.
- [ ] Commit.
