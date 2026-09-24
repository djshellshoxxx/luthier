## piano-roll-chord-display.md

Nothing of this spec (added 2026-09-24) is on this checkout: no piano roll strip, no on-screen keyboard or UI keyboard state in the processor, no chord name on the guitar, no Visual aids options; only the building blocks it names exist (`Rhythm/ChordDetector`, `LuthierEngine::getStringActivity`, `RubricVoicer`, Options General "Show tooltips on hover"). The visual branch has just started (e401027, 2026-09-24 16:39, "groundwork ... wired up in following commits"): `Support/SoundingNotes` (double-buffered atomic snapshot, not yet published by the engine), `UI/ChordNameFader` (0.35 peak, 60 ms in, 30 ms burst, 1.2 s hold, 0.8 s out, 60 ms crossfade, reduced motion) and `UI/VisualAids` (the four user preferences), with no tests and no UI. Owner gaps: the strip, keyboard play/latch/fingering, range, naming and placement, UiState fields, the Options section and every PR/CD/OP test.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| PR-0a (§0.1, §7) | Display reads never allocate/lock on audio thread; publish ≤ 12 stores + 1 atomic increment | (visual) `Support/SoundingNotes::publish` (not yet called) | n/a | - | OWNED |
| PR-0b (§0.1, §3) | Key play uses the existing on-screen keyboard MIDI path (ordinary MIDI ch 1) — no UI keyboard state exists here | - | - | - | OWNED |
| PR-0c (§0.2, §2) | Show what sounds (incl. voicer/rhythm notes) from engine string activity | (visual) `SoundingNotes` frame (bits, per-string note, start sample) | - | - | OWNED |
| PR-1 (§1) | Strip: keyboard 18-28 px + 4 s right-to-left roll 40-80 px | - | - | - | OWNED |
| PR-2 (§1) | Range follows tuning + capo, lowest open string to highest last fret, padded to octaves; out-of-range keys dimmed but live | - | - | - | OWNED |
| PR-3 (§1) | Colours: lit key = string colour 85 %, 150 ms release fade; bars 60 %, length = duration | - | - | - | OWNED |
| PR-4 (§1) | C octave labels; middle C marked | - | - | - | OWNED |
| PR-5 (§1) | Advanced: collapsible strip under the fretboard in col 2, 72 px default, drag 40-140 px, state in UiState | - | - | - | OWNED |
| PR-6 (§1) | Easy: 56 px strip under the illustration | - | - | - | OWNED |
| PR-7 (§1) | Strip header: ROLL/KEYS toggle, Latch, Show fingering | - | - | - | OWNED |
| PR-8 (§2) | SoundingNotes snapshot: 128-bit set + per-string note/start, double-buffered, atomic sequence, audio only stores | (visual) `Support/SoundingNotes` | n/a | - | OWNED |
| PR-9 (§2) | 30 Hz UI drain; 250 ms staleness clears keys | - | - | - | OWNED |
| PR-10 (§2) | Slide/bend moves the key at the semitone crossing; roll bar tick when bent > 20 c | (visual) `SoundingNotes` bendCents | - | - | OWNED |
| PR-11 (§3) | Click/drag glissando; velocity 40-110 by click height; notes into processor UI keyboard state, fretboard shows landing | - | - | - | OWNED |
| PR-12 (§3) | Latch: clicks toggle held set; Play/Enter sends one chord (rhythm engine strums, else crossing-speed strum); Clear/Escape | - | - | - | OWNED |
| PR-13 (§3) | Show fingering: RubricVoicer voicing as hollow ghost dots on the fretboard overlay; unreachable key x + tooltip "below this guitar's range" | - | - | - | OWNED |
| PR-14 (§3) | Computer keyboard A-L / W-P / Z-X only while the strip has focus | - | - | - | OWNED |
| CD-1 (§4) | Naming via ChordDetector: one PC note name (key-signature spelling; Bb/Eb flats), two PCs power chord "E5" or "C E", three+ chord symbol or PC list below confidence floor | `Rhythm/ChordDetector` (exists) | - | - | OWNED |
| CD-2 (§4) | Placement: centred over lower bout in GuitarBodyComponent (Easy + Advanced), display face, 12 % height clamped 28-96 px, text colour, peak 0.35, drawn in live overlay pass | - | - | - | OWNED |
| CD-3 (§4) | Timing: 60 ms in, 30 ms burst merge, hold ≤ 1.2 s, 0.8 s out, 60 ms crossfade, legato/bend updates in place | (visual) `UI/ChordNameFader` constants + `update/getOpacity` | - | - | OWNED |
| CD-4 (§4) | Reduced motion: no fades | (visual) `ChordNameFader::setReducedMotion` | - | - | OWNED |
| CD-5 (§4) | "Announce chord names": polite announcement ≤ 1 per 1.5 s | (visual) `VisualAids::announceChordNames` (pref only) | - | - | OWNED |
| OP-1 (§5, §0.3) | Options > General "Visual aids": chord names (on), announce (off, gated), piano roll per mode (Adv on / Easy off), roll shows Keys / Keys+Roll; UiPreferences, saved at once, not preset/param | (visual) `UI/VisualAids` getters/setters | - (no Options controls yet) | - | OWNED |
| ST-1 (§6) | UiState pianoRollExpanded / pianoRollHeight / pianoLatch / pianoShowFingering; latched set in plugin state, not presets; not undoable | - | n/a | - | OWNED |
| PF-1 (§7) | Roll repaints dirty region at 30 Hz within 2 ms at 1920x1080 | - | - | - | OWNED |
| T-PR01 (§8) | Test: chord lights exactly its keys within 2 frames in string colours (incl. voicer notes) | | | - | OWNED |
| T-PR02 (§8) | Test: release clears keys by 200 ms | | | - | OWNED |
| T-PR03 (§8) | Test: key click -> keyboard-state note-on, engine sounds it | | | - | OWNED |
| T-PR04 (§8) | Test: Latch C E G + Play strums C major | | | - | OWNED |
| T-PR05 (§8) | Test: Show fingering ghost dots match RubricVoicer | | | - | OWNED |
| T-PR06 (§8) | Test: range follows Drop D and capo 2 | | | - | OWNED |
| T-PR07 (§8) | Test: no allocation from snapshot publish | | | - | OWNED |
| T-CD01 (§8) | Test: "G#", "G", "Am7", "E5" | | | - | OWNED |
| T-CD02 (§8) | Test: peak 0.35 ± 0.02 in 80 ms, hold ≤ 1.2 s, 0 by 2.1 s | | | - | OWNED |
| T-CD03 (§8) | Test: 25 ms strum -> one name | | | - | OWNED |
| T-CD04 (§8) | Test: option off -> nothing drawn, no detection | | | - | OWNED |
| T-CD05 (§8) | Test: reduced motion no intermediate opacities | | | - | OWNED |
| T-OP01 (§8) | Test: options persist across editor/processor, not in presets | | | - | OWNED |
| T-OP02 (§8) | Test: every control reachable in both modes with focus and SR labels | | | - | OWNED |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=39 -->
