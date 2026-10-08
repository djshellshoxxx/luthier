# 07 Left-handed mode

**IDs:** SU-07 (and the mirror checks in SU-11). **ED:** 6. **Status:** missing. No handedness setting in `Parameters.h` or the UI. `GuitarRenderer.h` 13 draws the instrument from the right-handed player's view.

**Summary.** A `handedness` setting mirrors the view, the labels and the shortcut map. It changes no sound.

## 1. User-facing behaviour

Setup panel: Handedness, Right or Left. When Left:

- **Image.** The instrument picture and body are mirrored. The fretboard and picking side swap.
- **Technique cards and tooltips.** Fretting and picking hands are swapped.
- **Shortcuts.** Hand-specific keys in `KEYBOARD_SHORTCUTS.md` map to the mirrored hand.
- **Strings.** Two label options: `mirror` (the right-handed order, mirrored) and `left_strung` (the order a left-handed guitar is strung in). Physical pitch of each string is fixed by the instrument. The option changes which side the labels are read from, not any pitch.
- **Strum.** Down and up are labelled from the player's hand. The stored `strum_direction` value does not change; only the label does.

## 2. Engine and model

No audio change. Handedness never enters the render path.

View transform: a mirror of the x coordinate in the instrument illustration, `x' = W - x`, applied in `Source/UI/Guitar/GuitarRenderer` when drawing. The part data is unchanged.

Shortcut map: a table from action to key for the left-handed layout. Only hand-specific actions are remapped. The map is built once on the message thread.

## 3. Data model and parameters

- `handedness`: state, right | left, default right. Not automatable.
- `string_label_order`: state, mirror | left_strung, default mirror. Used only when `handedness` is left.
- `strum_direction`: unchanged.

## 4. State, file format, migration

- Setup block fields `handedness`, `string_label_order`. Missing means right and mirror.
- v1 presets load as right. Renders are bit-identical.

## 5. Edition

Free and Pro (both).

## 6. Performance budget

- UI only. Zero audio-thread cost.
- Mirror: a transform applied at paint time, or a mirrored path built once at load. No allocation in the paint path.
- Shortcut table: built once.

## 7. Test plan

- **SU-07.** Same preset, same notes, handedness right and left: output buffers are bit-identical (`memcmp`).
- Mirror: each hotspot's left-handed x equals `W - x` for the right-handed x, within 0.5 px.
- Shortcuts: every hand-specific entry in `KEYBOARD_SHORTCUTS.md` has a left-handed mapping; no two actions share a key.
- Strum: toggling handedness leaves `strum_direction` equal; the label flips.
- String order: toggling `string_label_order` leaves the tuning array equal; only labels change.
- Manual (SU-11): every new control is keyboard reachable.

## 8. Effort and dependencies

- ED 6 (roadmap 9).
- Depends on: `Source/UI/Guitar/GuitarRenderer`, `KEYBOARD_SHORTCUTS.md`, technique pages (`Source/UI/Techniques/TechniquePages.cpp`).

## 9. Open

- Confirm the two string-order options with a left-handed player before labelling them (roadmap 10.3).
