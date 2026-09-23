# ACCESSIBILITY AND LOCALIZATION SPEC

Fills a gap that no other spec document addresses. If the plugin ships
without this, guitarists with visual impairments, colour vision differences
or non-English first languages will hit walls.

## 0. Ground rules

1. Every control has an **accessible label**, a **role**, and an
   **accessible value description**. These are set at construction time on
   the JUCE `AccessibilityHandler` for each component.
2. Nothing important is conveyed by **colour alone**. Every colour-coded
   state also uses a shape, position or text label.
3. Every keyboard shortcut is **rebindable** and printable.
4. All UI strings live in a **catalog**, not hard-coded in source.
5. Font sizes scale with a user UI-scale preference, from 75% to 200%,
   without breaking layout.

## 1. Screen reader support

- Uses JUCE's `AccessibilityHandler` and `AccessibleValueInterface` for
  every knob, slider, button, toggle, dropdown, meter, list.
- Meters report their peak dBFS as accessible values.
- Fretboard component: exposes each fret as a child element with a
  descriptive label ("String 3, fret 5, current note: G").
- Snapshot strip: each button exposes its snapshot name.
- Overlay dialogs announce themselves on open ("Options dialog opened") and
  focus lands on the first interactive element.
- Escape closes any overlay and returns focus to the launcher.

Tested with NVDA (Windows), VoiceOver (macOS) and Orca (Linux). Regression
target: every UI element reachable via Tab and readable by NVDA.

## 2. Keyboard-only navigation

- Every control reachable via Tab / Shift-Tab in a defined order (left to
  right, top to bottom, respecting panel groupings).
- Arrow keys adjust values on the focused control (finer with Shift, coarser
  with Ctrl).
- Enter opens dropdowns and confirms dialogs.
- Escape cancels dialogs and dismisses overlays.
- F1 opens context help for the focused control.
- All plugin actions (panic, save, snapshot next/prev, etc.) reachable by
  shortcut without opening a menu.
- A "Show all shortcuts" overlay lists every shortcut, filtered by search.

## 3. Colour vision accommodation

Alternate palettes selectable in Options -> Appearance:
- **Default** (matches theme.md).
- **Deuteranopia-safe** (green-blind): swap the green success indicator to
  a blue that is distinguishable, alter the meter gradient to blue-teal-
  orange-red.
- **Protanopia-safe** (red-blind): similar remap, red clip becomes magenta.
- **Tritanopia-safe** (blue-blind): shift accents away from teal.
- **High contrast**: pure black background, white primary text, single
  vivid accent, no gradients.
- **Light**: inverted, with paper background, dark text, muted accents.

Each palette is a full theme override loaded via the same theme system
theme.md defines. Palettes ship as `Resources/Themes/*.json`.

Meters use both colour and shape: gradients under -18 dB use a narrower
strip; over 0 dB uses a bracket icon that a colour-blind user still sees.

## 4. Font sizing and layout

- UI scale: 75, 100, 125, 150, 175, 200%.
- Base font sizes from theme.md scale accordingly; minimum readable size
  never drops below 10 px effective at 100%.
- Panels reflow rather than clip when scale increases: knobs shrink to the
  smaller variant defined in theme.md, and if that still doesn't fit,
  wrap to a new row.
- Window minimum size grows with scale; window recovers gracefully when
  the host restores a saved size that no longer fits at the current scale
  (choose a compatible smaller scale automatically and warn once).

## 5. Motion and animation

- Reduced-motion toggle in Options.
- When on: disables the scrolling data-stream animation in empty panels,
  reduces control-change animation from 80 ms ease to 0 ms hard set,
  disables the header LED pulse.
- Value changes still animate visually via a static colour change to
  preserve feedback.

## 6. Localization

- String catalog in `Resources/i18n/<locale>.json`.
- Format: flat key-value JSON, keys are stable identifiers, values are
  the display strings.
- Locales at ship: en, en-GB, es, es-419, fr, de, pt-BR, ja, zh-CN,
  zh-TW, ko, ru, pl, nl, it.
- Locale switch in Options -> Appearance. Does not require a restart;
  UI redraws with new strings.
- Missing key falls back to en; missing en logs a warning at debug level.

String catalog rules:
- No string concatenation in code. Use format placeholders: `"Loaded {n}
  presets"` not `"Loaded " + n + " presets"`. Placeholders are named, not
  positional.
- Right-to-left scripts (ar, he) are not in the ship set but the layout
  engine must not assume LTR. Test with a bidi fixture before ship.

## 7. Documentation

- User manual is authored in English, translated to ship locales.
- The plugin's Help panel displays the manual in the current UI locale.
- Keyboard shortcuts panel is a live view of the current bindings, not a
  static string.

## 8. Text rendering

- All labels honour system font settings where possible; the theme's fonts
  are the preference, not a lock.
- Numeric readouts remain in the tabular monospace font from theme.md
  regardless of locale.
- Complex scripts (CJK, Arabic) trigger a fallback font stack that
  includes system CJK and Arabic fonts.

## 9. Options UI

New tab: Options -> Accessibility.

Contents:
- Screen reader announcements verbosity: minimal, standard, verbose.
- Keyboard shortcuts: full rebind table with search and reset-to-default.
- UI scale slider.
- Palette dropdown.
- Reduced motion toggle.
- Font override (system default, theme default, or user-selected).

Options -> Localization:
- Locale dropdown.
- Fallback locale dropdown (default en).
- Custom string catalog path (advanced).

## 10. Tests

- Screen-reader smoke: automated NVDA run through every panel via a
  reference script, verify every control announces label and value.
- Keyboard-only session: automated Tab walk hits every advertised control,
  no dead-end.
- Colour palettes: contrast ratio between text and background >= 4.5 (WCAG
  AA) on Default, High contrast, and Light. Palette switch does not clip
  any panel.
- Locale round trip: every ship locale renders every panel without
  clipping at 100% and 150% UI scale.
- Reduced motion: verify no animation frames after toggle-on.
- Font fallback: swap to a locale requiring CJK glyphs, verify no missing-
  glyph boxes in shipped strings.
