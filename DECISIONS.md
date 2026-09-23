# DECISIONS

Judgement calls made during the autonomous run, one line of reason each.
Older decisions live in `GAPS.md` ("Decisions made where nothing had
chosen") and `ambiguity-resolutions.md`.

- **2026-09-22 — Test harness owns its own `RangeState`.** `PresetManager` now
  takes the range state by reference; the harness mirrors the processor rather
  than faking one, so the preset tests exercise the real load order.
