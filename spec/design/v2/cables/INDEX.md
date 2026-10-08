# Cables and brands v2: design index

Source: `spec/roadmap/v2/cables-and-brands.md`. Ordered by ascending effort (ED from its section 11). Ties are ordered by dependency: items that others read come first.

Grouping: section 11 gives effort per work row, not per item, so each file covers one row and its CB-NN items. Subsections 3.1-3.4, 4.1-4.3 and 5.1-5.4 are described inside the file that owns them.

| ID | Title | ED | Status | File |
|---|---|---|---|---|
| CB-06 | Microphonic scale and coiled flag | 1 | partial | [01-microphonic-scale-and-coiled-flag.md](01-microphonic-scale-and-coiled-flag.md) |
| CB-08 | True-bypass pop | 2 | missing | [02-true-bypass-pop.md](02-true-bypass-pop.md) |
| CB-E01, CB-M01 | State, migration and Free rules | 3 | missing | [03-state-migration-free-rules.md](03-state-migration-free-rules.md) |
| CB-13 | RT checks and tests | 3 | missing | [04-rt-checks-and-tests.md](04-rt-checks-and-tests.md) |
| CB-01, CB-02 | Tiers, cable model and resonance | 4 | partial | [05-tiers-cable-model-resonance.md](05-tiers-cable-model-resonance.md) |
| CB-07 | Buffer and Ground Isolator DSP | 4 | missing | [06-buffer-and-ground-isolator.md](06-buffer-and-ground-isolator.md) |
| CB-U01 | Cable and pedal tier popovers | 4 | missing | [07-popovers.md](07-popovers.md) |
| CB-03, CB-04, CB-05, CB-12 | Hum, ground count and power mode | 6 | partial | [08-hum-ground-count-power.md](08-hum-ground-count-power.md) |
| CB-09, CB-10, CB-11 | Pedal tiers: voicing, noise and loading | 8 | missing | [09-pedal-tiers.md](09-pedal-tiers.md) |

**Total: 35 ED** (about 7 weeks), matching section 11.

## Open points that block implementation

1. **CB-07 is physically inconsistent as written** (file 06). A Buffer after a cable does not isolate the cable's capacitance from the pickup. Reword to "after a Buffer" or move the Buffer to the guitar end.
2. **v1 Vintage changes v1 renders** (file 01). The +9 dB penalty conflicts with ground rule 4 and CB-M01. Needs a mapping version per preset.
3. **Effective cable run** (file 05). The walk through true-bypassed pedals is a derivation, not in the spec. Confirm.
4. **Free loading a Pro board** (file 03). Rule proposed: render stored tiers, lock edits, Buffer and Isolator transparent.
5. **Two-section voicings** (file 09). The Overdrive example needs a shelf and a dip, so "one biquad" becomes up to two.
6. **Input-cable hum on v1 boards** (file 08). Proposed `G = 1` for v1 boards so v1 renders are unchanged.

## Coverage check

Every CB ID in section 10 is in exactly one file: CB-01, 02 (05); CB-03, 04, 05, 12 (08); CB-06 (01); CB-07 (06); CB-08 (02); CB-09, 10, 11 (09); CB-13 (04); CB-E01, CB-M01 (03); CB-U01 (07).
