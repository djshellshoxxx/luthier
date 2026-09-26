# TAB EXPORT: THE TUNE BUILDER'S SYMBOLIC SCORE SPEC

`notation-export.md` already specifies full guitar TAB export - ASCII tab
(2.3), MusicXML with complete technique fidelity (2.1), and Guitar Pro 8
with complete technique fidelity (2.2) - for anything captured live
(section 6, `PerformanceCapture`). `tune-builder.md` 9.3 already lists the
same three formats as a Tune's notation export, and 5's ground rule and
14's interaction list both say a Tune's notation and MIDI exports "fall
out of" or "consume" the Tune's `PerformanceScore`.

Nothing specifies how a `PerformanceScore` for a Tune comes to exist.
`notation-export.md` 6 is explicit that the struct "is only ever filled by
`NotationImporter` reading a file" or by `PerformanceCapture` watching
live engine activity - and a Tune's chord grid, melody piano roll and
rhythm pattern are symbolic data (`tune-builder.md` 1, 11), not something
anyone has necessarily played. Exporting a Tune to tab today would require
rendering it through the engine in real time and capturing the result,
which works but is slow, and is not what either spec currently commits to.

This file specifies **`TuneToScore`**, the second, symbolic producer of
`PerformanceScore` that closes that gap, plus one additive format -
a combined, printable tab-and-standard-notation page - that the existing
three formats don't cover. It changes nothing about how ASCII tab,
MusicXML or Guitar Pro are written once a `PerformanceScore` exists;
that remains entirely `notation-export.md`'s.

Test ID prefix: **TABX-**.

## 0. Ground rules

1. **`TuneToScore` never touches the audio thread.** It runs on the
   worker pool (`ui-wiring.md` 19) that already handles notation export,
   IR resample and spectrum delta, exactly as `notation-export.md` 0.1
   requires of every notation module.
2. **Deterministic.** The same `Tune` (`tune-builder.md` 1, 11), the same
   loaded guitar and tuning, and the same rhythm pattern / genre kit
   produce byte-identical `PerformanceScore` output on every run. A tune
   is a document; exporting it twice must give the same tab twice.
3. **What it produces is what would actually be played**, not a naive
   transcription of the chord and melody data. Voicing, strum/fingerpick
   pattern behaviour, and technique resolution reuse the same decision
   logic the engine applies live, run offline.
4. **No new export formats compete with `notation-export.md`'s three.**
   ASCII tab, MusicXML and Guitar Pro are written by the exact exporters
   `notation-export.md` 2.1-2.3 already specify, fed a `PerformanceScore`
   from this module instead of from `PerformanceCapture`. Only the
   combined print page (section 3) is new, and it is additive, not a
   replacement for any existing format.

## 1. `TuneToScore`

New module, `Source/Notation/TuneToScore.{h,cpp}`, invoked from the TUNE
tab's export dialog and from `midi-export.md` 4.1's export dialog when its
source is a loaded Tune rather than a live capture.

### 1.1 Inputs

A `Tune` (`tune-builder.md` 1's in-memory model, the same one
`.luthiertune` serializes), the currently loaded `GuitarSpec` and tuning
(for string/fret voicing), and the rhythm pattern / genre kit each section
names (`tune-builder.md` 11's `rhythm_pattern`, `genre_kit`).

### 1.2 Per-section conversion

For each section, in setlist order (`tune-builder.md` 11's `setlist`
array, expanded by its `repeats`):

1. **Chords.** Each chord cell (`root`, `quality`, `beats`) is voiced by
   the same `RubricVoicer` instance the rhythm engine uses live
   (`ambiguity-resolutions.md` 4, `chordTones` mode, DECISIONS' rubric-
   voicer entry), called directly rather than through the audio path, so
   the string/fret choice, omissions and density cap match what the
   section's `RhythmEngine` settings would actually produce. The voicer's
   own previous-voicing state carries across chords within a section for
   4.4's transition-bonus continuity, and resets at each section boundary
   like the live engine's state boundary does (`tune-builder.md`
   DECISIONS entry on section boundaries).
2. **Strum / fingerpick pattern.** The section's pattern
   (`rhythm-engine.md`) is expanded against the chord voicings to produce
   per-note timing, crossing order, mutes and ghosts
   (`strum-dynamics.md`), exactly as the live rhythm engine would - this
   is what makes the exported tab strummed rather than block-chorded.
3. **Melody.** Each melody note (`start`, `dur`, `pitch`, `vel`,
   `tune-builder.md` 11) is assigned a string and fret by the same
   sticky, most-recently-used-string bias `controllers.md` 4 and the live
   `MidiInterpreter` use, seeded fresh at each section boundary.
   Consecutive melody notes close enough in time and pitch to have
   triggered a hammer-on, pull-off or slide live (`engine.md` 4's
   `TechniqueEngine` thresholds) are marked with that technique, so a
   drawn or generated melody reads with the same technique notation a
   played one would.
4. **Layers** (bass line, pad, arp, countermelody, percussion,
   `tune-builder.md` gui-integration row) are written to their own
   `PerformanceScore` tracks, voiced the same way as the main melody
   where they carry pitched content.
5. **Chord symbols** are written directly from the section's own chord
   cells (no detection needed, unlike `notation-export.md` 4's live path -
   the Tune already knows its chords).
6. **Section boundaries** become `PerformanceScore` measure markers
   carrying the section's name, matching `tune-builder.md` 9.3's
   "Section headings preserved."

### 1.3 What it does not resolve

Live-only realism (per-strike character-wear jitter, noise-floor events,
sympathetic ring, environment drift) has no bearing on notation and is
never written to the score, matching `notation-export.md` 1's own scope
(pitch, technique and timing - not audio character). A Tune's swing and
feel settings (`tune-builder.md` 11's `swing`, `feel_pct`) do affect the
computed timing in step 2, since they change when a strum actually lands.

### 1.4 Relationship to `PerformanceCapture`

`TuneToScore` and `PerformanceCapture` (`notation-export.md` 6) are two
producers of the same `PerformanceScore` struct; nothing downstream (the
three exporters, the live TAB view) needs to know which one filled it.
Playing a Tune live (transport running, `tune-builder.md` DECISIONS' "TUNE
in the plugin" entry) still also feeds `PerformanceCapture` as an ordinary
performance, since that path plays real MIDI through the real engine; the
two can disagree in fine timing (capture reflects actual engine/host
jitter; `TuneToScore` reflects the tune's written intent) and that
disagreement is expected and harmless - the export dialog's source picker
(section 4) makes the choice explicit rather than silently preferring one.

## 2. Where "if feasible" lands: it's feasible, and it's exactly this

The task this file answers asked for ASCII tab and, if feasible, MusicXML
/ Guitar Pro-compatible export from the Tune Builder. Both are already
fully specified end-to-end by `notation-export.md` and were already
promised for tunes by `tune-builder.md` 9.3; `TuneToScore` is what makes
that promise buildable without a real-time render. No new tab notation
format is invented here.

## 3. Additive format: combined print page

One new export format, alongside `notation-export.md`'s existing four
(MusicXML, Guitar Pro, ASCII tab, MIDI): **"Tab + Standard Notation
(Print)"**.

- A read-only, paginated document: a standard notation staff directly
  above a six-line tab staff for the same measures, one system per line,
  chord diagrams at first occurrence (reusing `notation-export.md` 2.2's
  diagram data, not a second diagram engine), section headings as running
  titles, and a title block from `Tune.meta` (title, tempo, key, time
  signature - `tune-builder.md` 11).
- Page size Letter or A4 (Options -> Localization's existing region
  setting supplies the default; user-overridable in the export dialog).
- Rendered through JUCE's printing graphics context where the host
  platform provides one (`juce::Printer` / native print-to-PDF); where it
  does not, the exporter falls back to one flattened PNG per page at
  150 dpi and says so in the export summary, rather than failing the
  export. Either way, the layout pass (measures per system, page breaks)
  is the same pure function, so the two outputs always agree on content.
- Consumes the same `PerformanceScore` as every other format (section 1
  or `PerformanceCapture`, per the source picker in section 4) - it is a
  fifth *rendering* of existing data, not a sixth data model.
- Not exposed for MIDI import (`notation-export.md` 5): a print page is
  not a machine-readable score and does not participate in the round-trip
  guarantees section 5's tests hold the other four formats to.

## 4. UI

### 4.1 TUNE tab (`gui-integration.md` 4.4, `tune-builder.md` 9.3)

The TUNE tab's existing export dialog gains no new fields for the three
existing formats (still `notation-export.md`'s own options) but its
**source** is now explicit when a Tune is loaded: a small toggle,
**"From the written tune" (default) / "From a live take"**, chosen once
per export. "From the written tune" calls `TuneToScore`; "From a live
take" uses the most recent `PerformanceCapture` ring contents
(`notation-export.md` 6.2), for a player who specifically wants what was
actually played, drift and all.

A one-click **Tab** button sits beside the export dialog's opener, for the
common case (`gui-integration.md` 0.2's three-interaction rule): it runs
the export dialog pre-set to ASCII tab, "From the written tune," entire
tune, default destination - one click, one file, no dialog to fill in.
Holding Alt opens the full dialog instead, matching the drag-out
Alt-modifier convention `midi-export.md` 4.2 already uses.

### 4.2 Notation export dialog (`notation-export.md` 5, File -> Export ->
Notation)

Gains the fifth format in its dropdown: "Tab + Standard Notation
(Print)." When chosen, the dialog's per-format options row shows page
size and "Include chord diagrams" (default on); the existing preview pane
shows a scaled thumbnail of the first page instead of the first bar of
text.

### 4.3 Empty and error states

| Condition | Message |
|---|---|
| Export from written tune with no melody and no chords in any section | "This tune is empty. Add a chord or a melody note first." |
| Print format requested on a platform with no native print/PDF graphics context | Export proceeds as flattened PNGs; summary reads "Saved as 5 PNG pages (this system has no PDF printer available)." |
| "From a live take" chosen with an empty capture ring | Falls back to "From the written tune" with a one-time notice, rather than exporting nothing. |

## 5. Interactions with other specs

- **`notation-export.md`**: unchanged for the three formats it already
  owns; gains a second `PerformanceScore` producer (this file) and one
  additive format (section 3). See that file's own additive note in its
  section 6.
- **`midi-export.md`**: a Tune's MIDI export (`midi-export.md` 9,
  `tune-builder.md` 9.2) may also source from `TuneToScore`'s
  `PerformanceScore` instead of a live render, for the same "instant,
  deterministic" reason - see that file's own additive note in its
  section 9. The Luthier-profile round-trip guarantee
  (`midi-export.md` 2.2) is unaffected: whichever producer filled the
  score, the same MIDI writer runs.
- **`tune-builder.md`**: this file is the concrete mechanism behind that
  spec's 5, 9.3 and 14 statements that notation "falls out of" or is
  "consumed from" the Tune's `PerformanceScore`. No field in
  `tune-builder.md`'s own data model changes.
- **`rhythm-engine.md`**, **`strum-dynamics.md`**,
  **`ambiguity-resolutions.md`** 4 (rubric voicer): reused directly,
  offline, per section 1.2; none of their own specs change.
- **Practice drawer TAB tool** (`practice-tools.md` 9): unaffected -
  that is a live-scrolling view of `PerformanceCapture`'s ring, not an
  export, and is out of this file's scope.

## 6. Tests

1. **TABX-01** Determinism: exporting the same `Tune` twice, with no
   intervening state change, produces byte-identical `PerformanceScore`
   output and byte-identical files in every format.
2. **TABX-02** Voicing parity: for a fixture progression, `TuneToScore`'s
   chord voicings match what the live rhythm engine actually plays for
   the same section settings, string-for-string and fret-for-fret.
3. **TABX-03** Strum expansion: a section using a known strum pattern
   produces per-note timing, crossing order and mutes in the score that
   match an offline expansion of `strum-dynamics.md`'s own model for that
   pattern.
4. **TABX-04** Melody technique inference: a fixture melody with a
   half-step run within the hammer-on threshold is marked with hammer-on
   /pull-off technique flags in the score; a run outside the threshold is
   not.
5. **TABX-05** Section boundaries: a three-section tune produces three
   measure-marker groups with the correct names and voicer-state resets
   at each boundary.
6. **TABX-06** Layers: bass, pad and countermelody layers land on their
   own tracks with correct pitches.
7. **TABX-07** Format parity via existing exporters: `TuneToScore`'s
   output, run through `notation-export.md`'s ASCII, MusicXML and Guitar
   Pro exporters, passes that spec's own round-trip tests unmodified.
8. **TABX-08** Print page: a fixture tune renders the documented number of
   pages at Letter and A4, with chord diagrams at first occurrence only
   and section headings as running titles; the PNG fallback path produces
   pages whose content matches the PDF path's layout pass output.
9. **TABX-09** Source picker: "From the written tune" and "From a live
   take" on the same played-and-matching tune produce scores that agree
   on pitch and string/fret assignment, and are allowed to differ in fine
   timing.
10. **TABX-10** One-click Tab button: produces an ASCII tab file at the
    default destination with no dialog shown; Alt-click opens the full
    dialog instead.
11. **TABX-11** Worker-thread safety: exporting a 10-minute tune touches
    no audio-thread state and produces zero allocation-trap hits on the
    audio thread during a simultaneous playback render.
12. **TABX-12** Empty tune and empty-ring fallbacks produce the documented
    messages rather than an empty or crashing export.
