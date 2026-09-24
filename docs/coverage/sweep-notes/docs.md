# SPEC-SWEEP notes: docs worker

- [RM-21] make_irs.py seeds from a stable crc32 hash and takes `--out DIR`; the shipped 720 IRs are NOT regenerated — every file would change (they came from a randomised seed) and with them every convolution rig's sound and possibly tuned test thresholds. README states this. Regenerate deliberately in a separate change if wanted.
- [UM-20] Easy-mode data stream paragraph kept — visual constructs `DataStreamDisplay` (RM-30), so the doc becomes true on merge.
- [TS-22] Doc softened to the tested rig instead of adding a whole-factory-bank mono render (the Combo-style per-preset renders are already the slow part of the suite).
- [PT-19/PF-*/KS-22] Doc-only corrections to docs/PLAYING_TECHNIQUES.md, docs/PRESET_FORMAT.md and docs/KEYBOARD_SHORTCUTS.md done here at the coordinator's request; the matching part rows were updated.
