# Documentation gaps - DOCS area

Tracking fixes to user documentation (USER_MANUAL.md, KEYBOARD_SHORTCUTS.md, TROUBLESHOOTING.md, PRESET_FORMAT.md, PLAYING_TECHNIQUES.md).

Branch: `claude/luthier-gaps-docs`

## Summary

Verified documentation against code and corrected factual errors. Focused on critical user-facing issues where the docs conflict with what the code actually does.

## Fixed issues

| Row ID | Section | Issue | Status |
|--------|---------|-------|--------|
| UM-2 | USER_MANUAL | Six macro knobs → Seven (added Character) | DONE |
| UM-9 | USER_MANUAL | Undo depth "64 steps" → "200 steps" | DONE |
| UM-18 | USER_MANUAL | Removed fretboard from Easy mode description | DONE |
| UM-20 | USER_MANUAL | Updated piano roll description (not scrolling data stream) | DONE |
| UM-39 | USER_MANUAL | Hover behavior "value replaces label" → "appears above" | DONE |
| KS-22 | KEYBOARD_SHORTCUTS | Hover behavior correction | DONE |
| TS-2 | TROUBLESHOOTING | macOS paths to system /Library, added CLAP, noted hand-copy option | DONE |
| TS-7 | TROUBLESHOOTING | Format vs magic field: updated to "magic": "luthier.preset" | DONE |
| TS-20 | TROUBLESHOOTING | Strum Speed zero impossible → set to max 800 strings/s | DONE |
| PF-10 | PRESET_FORMAT | Metadata table: replaced format with magic field | DONE |
| PF-15 | PRESET_FORMAT | Macro prefix list: added Character and spare modulation macros | DONE |
| PF-18 | PRESET_FORMAT | midiMap: "Every mapped CC is written, defaults included" | DONE |
| PF-19 | PRESET_FORMAT | MidiTarget enum table: updated all 22 values to match code | DONE |
| PT-15 | PLAYING_TECHNIQUES | Whammy bridge names: use UI names not brand names | DONE |
| PT-17 | PLAYING_TECHNIQUES | Up-stroke velocity: 78% → 85% force | DONE |
| PT-18 | PLAYING_TECHNIQUES | Split Freeze and E-Bow into separate sections | DONE |
| PT-19 | PLAYING_TECHNIQUES | Amp feedback: rewrote to physical loop model with 5 controls | DONE |
| PT-23 | PLAYING_TECHNIQUES | Aftertouch: removed "can be switched to bend" (not implemented) | DONE |

## Additional work completed

| Row ID | Section | Issue | Status |
|--------|---------|-------|--------|
| KS-18 | KEYBOARD_SHORTCUTS | Added S (Slide), Ctrl+N (New preset), Ctrl+Alt+E (Show preset), Ctrl+[/] (tabs) | DONE |

## Not yet completed

These items require code changes or extensive testing beyond documentation scope:

| Row ID | Section | Notes |
|--------|---------|-------|
| PF-14 | PRESET_FORMAT | Missing keys should fall back to default - requires ParameterBridge code fix |
| PT-2 | PLAYING_TECHNIQUES | Harmonic velocity trigger - requires parameter wiring |
| PT-4, PT-8, PT-11, PT-22, PT-23, PT-29 | PLAYING_TECHNIQUES | Various test additions - not docs scope |
| UM-*, TS-*, KS-* | Various | Most remaining rows are NO-TEST (need tests added), not docs issues |

## Verification notes

- Undo steps: confirmed kMaxEntries = 200 in UndoHistory.h
- Character macro: confirmed in Parameters.h line 253
- Whammy names: confirmed bridgeTypeNames() in Parameters.cpp line 284
- Up-stroke force: confirmed kUpStrokeForce = 0.85 in StrumGesture.h line 227
- Freeze vs E-Bow: confirmed separate mechanisms in FreezeOverlay.h header comment
- Feedback controls: confirmed feedbackAmount/Distance/Angle/Focus/OctaveBias in Parameters.h
- MidiTarget enum: verified against MidiInterpreter.h enum definition

All changes made without modifying C++ code except for documentation clarifications in comments.
