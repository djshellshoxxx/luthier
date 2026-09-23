# DECISIONS

Judgement calls made during the autonomous run, one line of reason each.
Older decisions live in `GAPS.md` ("Decisions made where nothing had
chosen") and `ambiguity-resolutions.md`.

- **2026-09-22 — Test harness owns its own `RangeState`.** `PresetManager` now
  takes the range state by reference; the harness mirrors the processor rather
  than faking one, so the preset tests exercise the real load order.
- **Right-click "Restrict to stock range" follows `advanced-ranges.md` 4, not
  gui-integration 16.** Section 16 says "only if unlocked at preset level";
  advanced-ranges 4 says restrict is offered only for a control in the
  per-control list. The more specific spec wins; a family-level lock lives on
  Options RANGES.
- **The locked-range notice is a `BubbleMessageComponent` at the control.**
  advanced-ranges 6.3 asks for an inline notice at the control, not a banner;
  a bubble anchored to the knob is that without every panel hosting a notice.
- **The Options RANGES master toggle clears per-control unlocks.** Locking is
  meant to take them away, and unlocking all makes them redundant.
- **Controls re-attach on a range-generation counter polled by the editor's
  4 Hz timer.** A SliderAttachment copies the range once; the swap happens
  inside preset loads that know nothing about windows.
- **Fixed the processor undo stack's off-by-one** rather than working around
  it: one action could not be undone and the first undo after two reverted
  both. Each entry now holds its before-state and, once undone, its after-state.
- **No separate "advanced-range clamped" banner (gui-integration 15).** A preset
  load cannot clamp here: values are stored normalised against the file's own
  `ranges` block, which is applied first. The only clamps are explicit narrowings
  (RANGES lock, right-click restrict, per-row Clamp), and each reports at the
  place it happened - a confirmation with the count, or a bubble at the control.
- **Tab-header padlocks wait for their tabs to hold physical parameters.**
  gui-integration 21 names CHARACTER and WORKSHOP; today only `amp` and
  `circuit` have ranges and neither lives in a tab. `RangesUi::drawPadlock` is
  the shared glyph for when they do (TODO items 3 and 7).
