# FEAT2-TAB

Implementation ready for Linux verification; tests have **not** run here.

| Requirement | File | Test | Status |
| --- | --- | --- | --- |
| Deterministic per-string TAB, bar boundaries | Source/Notation/AsciiTabWriter.* | TabExport/knownRiffHasExactStringsAndBars | Implemented; Linux pending |
| Exact bend, slide and palm-mute fixture | Source/Tests/TabExportTests.cpp | TabExport/techniquesHaveExactAlignedOutput | Added; Linux pending |
| Preserve closely spaced onsets and aligned widened cells | Source/Notation/AsciiTabWriter.cpp | TabExport/denseOnsetsDoNotOverwriteOrConcatenateFrets; widenedCellsKeepBeatRulerAligned | Implemented; Linux pending |
| Note techniques, combined glyphs, symbol density and mute spans | Source/Notation/AsciiTabWriter.cpp | TabExport/allNoteTechniquesHaveReadableGlyphs; combinedTechniquesDoNotEraseOneAnother; palmMuteSpanAndDensityAreExplicit | Implemented; Linux pending |
| Every track/section; ranges follow individual meters; wrapping | Source/Notation/AsciiTabWriter.cpp | TabExport/everySectionAndTrackIsExported; rangeUsesEachMeasuresTimeSignature; denseBarWrapsWithoutDroppingTokens | Implemented; Linux pending |
| Existing live-view window clamping | Source/Notation/NotationExport.cpp | TabExport/windowClampsToExistingMeasures; existing Notation/liveTabWindowRendersASlice | Preserved; Linux pending |
| Captured performance technique model | Source/Export/MidiPerformance.cpp (existing bridge) | TabExport/midiPerformanceTechniquesReachBothNotationWriters | Covered by fixture; Linux pending |
| File export and preview use the same renderer | Source/UI/HeaderBar.cpp; Source/UI/NotationPanel.cpp; Source/Notation/NotationExport.cpp | Existing NotationPanel suite; manual File > Export notation, choose .txt | Existing route reused; manual verification pending |
| MusicXML / Guitar Pro TAB | Source/Notation/NotationExport.cpp (existing writers) | New MIDI bridge fixture plus existing Notation MusicXML/GP tests | Reused; Linux and external application compatibility pending |

## Decisions and limits

- No new parameters or spec changes. Shared exporter only delegates its two ASCII functions to the new writer. Existing File > Export notation supports .txt/.musicxml/.gp; NOTATION panel selects formats explicitly.
- Note onsets between grid ticks add columns rather than round onto another note. ASCII spacing is a readable event grid, not exact proportional timing or a lossless interchange format. Note durations and continuous bend/vibrato curves are not fully represented.
- Harmonic wrappers compose with technique suffixes. PM on each note identifies its string; the upper PM line shows the union of mute durations, including sustained notes across bars.
- Every measure starts a system. Dense bars continue with ':'; only actual bar boundaries use '|'. A single unusually long chord/token is retained intact even if it exceeds the requested width.
- Range selection retains whole intersecting measures, consistent with the existing notation export options.
- The score bridge renders note-attached techniques. Non-note performance events (workshop actions, incidental noises and other MIDI extension events without ScoreNote representation) remain in MIDI export, not TAB.
- Existing MusicXML/GP writers are reused, not rewritten. Guitar Pro application import, full MusicXML schema validation, and fidelity of every existing technique mapping remain deferred. Staff notation is a separate FEAT2-NOTATION lane.

## Verification / handoff

- This environment is Windows; WSL enumeration fails with Wsl/EnumerateDistros/Service/E_ACCESSDENIED. No Linux toolchain is available. No Windows/macOS build was attempted, per project rules.
- docs/HANDOFF.md reports Actions blocked by the account spending limit. Baseline and changed Linux suites remain unrun. Do not merge until the Claude coordinator verifies them.
- Requested gate: `scripts/setup_linux.sh`, `ninja -C build -j$(nproc) LuthierTests`, then `xvfb-run -a build/LuthierTests_artefacts/Release/LuthierTests TabExport`, `Notation`, `NotationPanel`, `MidiExport`, and the full suite.
- Native Git cannot authenticate to the private remote in this sandbox. Files are published via the authenticated GitHub Git Data API using the actual remote parent/tree. Local partial-snapshot history is not pushed.
- `scripts/fixeol.sh` is run before commits; only this lane's files are published. Linux verification is explicitly blocked under the user's commit-and-document fallback.
