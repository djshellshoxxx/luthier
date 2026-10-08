# Pedalboard v2 design specs: index

Source: `spec/roadmap/v2/pedalboard-v2.md`. Sorted by ascending ED.

Per-feature ED is not in the roadmap. It is a split of the roadmap's section 9 buckets (Core 25, Migration 11, UI 19, AmpEngine 6, Tests 8; total 69), set out so the per-feature figures sum to the same 69. Treat them as estimates.

| ID | Title | ED | Status | File |
|---|---|---|---|---|
| PB-07 | Latency defect: bypass must not change latency | 0.5 | defect present | [01](01-latency-defect-pb07.md) |
| - | Source layout and integration seams | 0.5 | missing | [02](02-files-and-seams.md) |
| PB-P01 | Split level parameters (host count 216) | 1.0 | missing | [03](03-split-level-params.md) |
| - | Cable popover | 2.0 | missing | [04](04-cable-popover.md) |
| PB-E01 | Edition gating (Free vs Pro) | 2.0 | partial | [05](05-edition-gating.md) |
| PB-U02 | Easy Mode board strip | 2.5 | missing | [06](06-easy-mode.md) |
| PB-03 | Mono and stereo routing | 3.0 | missing | [07](07-mono-stereo.md) |
| PB-00 | New pedal types (Buffer, Ground Isolator, Preamp Stage) | 3.0 | partial | [08](08-pedal-types.md) |
| - | Pedal face | 3.0 | partial | [09](09-pedal-face.md) |
| PB-10, PB-11 | Instance binding and footswitches | 3.0 | partial | [10](10-footswitches-instances.md) |
| PB-U01 | Drag, drop and keyboard editing | 4.0 | missing | [11](11-drag-drop-keyboard.md) |
| PB-08, PB-09 | Bypass modes (True, Buffered) and pop | 5.0 | partial | [12](12-bypass-modes.md) |
| PB-M01, PB-M02, PB-M03 | Board block, file format and v1 migration | 5.0 | missing | [13](13-board-block-migration.md) |
| PB-01, PB-02, PB-13 | Board graph and validation | 6.0 | missing | [14](14-board-graph.md) |
| PB-12 | In-loop effects: AmpEngine split | 6.0 | missing | [15](15-in-loop-amp-split.md) |
| PB-04, PB-05, PB-06 | Compile, swap and retire | 7.0 | partial | [16](16-compile-swap.md) |
| - | Advanced canvas and library drawer | 7.5 | missing | [17](17-canvas-library.md) |
| (shared) | Shared test harness | 8.0 | missing | [18](18-test-harness.md) |
| | **Total** | **69** | | |

Sequencing note: 18 (harness) sorts last by ED but must start with 16, because PB-04/05/06 need it. Start order is 01, 02, 18 (initial), 16, then the rest.

## Findings to resolve before implementation

1. **PB-02 expectation is wrong as written.** A 0 dB split re-merged sums two full copies: +6.0 dB, not dry (14).
2. **PB-03 round trip is wrong as written.** Mono to stereo to mono with `0.7071` downmix gives +3.0 dB on a coherent signal, not within 0.1 dB (07).
3. **Latency fix needs bypass-leg padding.** Removing the bypass term from the sum is not enough: soft bypass crossfades a delayed wet with an undelayed dry and combs for 10 ms. Each bypassed leg must be padded by the pedal's latency (01, 12).
4. **New-pedal default conflicts.** Ground rule 7 says new pedals "start soft-bypassed"; section 2.4 says new pedals use True. Recommended: True, starting bypassed (09).
5. **Free pedal count.** Section 6 says 8 pedals on a Free board; `editions.md` 2.2 says 15 of 22 types; the enum will hold 26 non-None types after v2 (05).
6. **Merge and split design values** (pop 4 ms, -40 dBFS, board size 24/8, buffer impedances) are unmeasured (roadmap 10.6).
