# CB-U01: Cable and pedal tier popovers

**Items:** CB-U01
**ED:** 4
**Source:** `cables-and-brands.md` 8, 10 (CB-U01), 11, 12 (item 7); `pedalboard-v2.md` 2.3, 2.4

## Summary

Build the patch-cable popover and the pedal-tier popover so a player who has not read this spec can set tiers and read their meaning at 200 % UI scale.

## Status

Missing. v1 has the guitar-cable choice in the parameter panel and a search synonym for `cable_quality` (`Source/UI/Search/SearchCatalog.cpp`). The popovers are part of `pedalboard-v2.md` 2.3 and 2.4, which are not built.

## User-facing behaviour

**Patch-cable popover** (opened from a cable on the board):
- Tier selector: five entries. Unmodelled appears only on migrated cables (read-only in Free, file 03).
- Length: numeric field, 0.3-10.0 m, with a unit.
- Balanced: toggle.
- Coiled: toggle (Pro).
- Label: text field, optional.
- Delete.
- Vocabulary (open point 7, recommended yes): each tier row shows the v1 name it corresponds to (Economy = Cheap, Standard = Standard, Pro = Studio, Boutique = no v1 name; Vintage is shown as Economy plus coiled). One vocabulary for the player.
- Read-out (D, optional): derived resonance for the current run, for example "Resonance about 4.5 kHz at 6 m". Off by default in the first build; it helps the test.

**Pedal-tier popover** (from a pedal face):
- Tier selector as above.
- Bypass mode: soft, buffered, true.
- Pop on/off (when true bypass is selected).
- Input impedance, read-only (derived from tier, file 09).
- For Ground Isolator (type 24): Lift selector, Off/Lift/Isolate.

**Free:** every Pro control shows greyed with a "Pro" label. Clicking it opens the upgrade message used elsewhere in the app.

## Engine / DSP design

No audio processing. The popover writes board state through the same API as file 03. It never touches `process`.

- All numeric fields use `juce::Slider` or text entry with the unit in the label.
- Strings come from a single table (`CableTierStrings.h`, new). The table holds only generic tier names and the v1 names. No brand names. A test scans the table.
- Layout uses the app's existing look-and-feel scale factor. No hard pixel sizes for text. At 200 % scale every control keeps a 16 px side gutter inside its popover and wraps text rather than clipping.
- Keyboard: every control reachable by Tab, labelled for screen readers where the app already does so.

### RT safety

Not applicable (UI thread). A UI change goes to the compile path (file 04) through the message thread.

## Data model and parameters

None new. The popover reads and writes the fields in file 03.

## State, file format and migration

None. Popover state (open or closed, last tab) is not saved.

## Edition gating

| Control | Free | Pro |
|---|---|---|
| Tier selector | Standard enabled, others greyed "Pro" | all |
| Coiled | greyed "Pro" | enabled |
| Buffer, Ground Isolator entries in the palette | greyed "Pro" | enabled |
| Power mode (in board settings, file 08) | Daisy enabled, others greyed "Pro" | all |

## Performance budget

UI only. No units. Popover open must not block the audio thread (no shared lock; it uses the message-thread state API only).

## Test plan

- **CB-U01 (manual).** A tester who has not read this spec opens each popover at 200 % UI scale and, without help, sets a 6 m Economy cable to Pro, reads the vocabulary, and sets a pedal to true bypass with pop off. Pass if all three are done without asking and no text is clipped.
- Added (automated): every string in `CableTierStrings.h` passes a brand-name scan (the list is kept outside the app).
- Added (automated): at 200 % scale, every popover control's bounds lie inside the popover and no label is clipped (measured text width is at most the control width).
- Added (automated): Free build has every Pro control disabled and shows "Pro".

## Effort and dependencies

- ED 4.
- Depends on: `pedalboard-v2.md` 2.3, 2.4 (layout and popover framework); file 03 (state API); file 09 (input impedance read-out); file 08 (power mode control); `cables-and-brands.md` 12 item 7 (vocabulary decision).
