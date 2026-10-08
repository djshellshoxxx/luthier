# 04 String-change wizard

**IDs:** SU-03. **ED:** 5. **Status:** partial (restring exists in `string-aging.md` 234-238; stretch settling exists in `tuning-stability.md` 2.1; no combined workflow or timer).

**Summary.** A three-step panel that composes restring, a stretch timer and a check. No new model.

## 1. User-facing behaviour

A single panel with three steps and a Back button.

1. **Restring.** Choose one string or all six. Their age resets to zero.
2. **Stretch.** Shows the expected settling drift from `tuning-stability.md` 2.1 and starts a timer (default 30 minutes). When the timer ends, the panel asks for a retune, once.
3. **Check.** Runs the intonation check (`06`), shows the action and relief readout (`fret-buzz.md` 7), and a tuner pass listing strings that moved since the last check.

The timer and the last-check reference are session state. They are not saved; closing the app clears them.

## 2. Engine and model

- No new model. Step 1 calls the existing restring command. Step 2 reads the settling curve from `tuning-stability.md` 2.1 at elapsed time `t` and displays it. Step 3 calls `06` and the `fret-buzz.md` readouts.
- "Moved" rule (D): string `s` has moved when `|cents_now(s) - cents_at_last_check(s)| > 2 ct`. The first check has no reference and reports "baseline set".
- Timer runs on the message thread (`juce::Timer`). The audio thread is not involved.

## 3. Data model and parameters

- `stretch_timer_min`: UI state, not saved, 0 to 120, default 30. 0 turns the timer off.
- Writes the existing `string_age` (reset to 0). No new automatable parameter.

## 4. State, file format, migration

- Nothing new is saved. The restring writes the existing `string_age` field.
- The wizard keeps no file state. A save and reload contains no stretch or check key.

## 5. Edition

Free and Pro (both).

## 6. Performance budget

- UI and message thread only. Zero audio-thread cost.
- The timer fires once per minute at most.

## 7. Test plan

- **SU-03.** Restring all sets `string_age` to 0 for all six strings. The timer shows 30 minutes by default.
- Restring one string: only that string's age resets; the others are unchanged.
- After 30 minutes of simulated time the retune prompt is raised once, not repeatedly.
- Back from Step 3 to Step 1 does not reset ages a second time.
- Save and reload contains no `stretch` or `check` key.

## 8. Effort and dependencies

- ED 5 (roadmap 9).
- Depends on: `string-aging.md` (restring), `tuning-stability.md` 2.1 (settling), `06` (intonation check), `fret-buzz.md` 7 (action and relief).

## 9. Open

None specific to this feature. The "moved" threshold (2 ct) is D.
