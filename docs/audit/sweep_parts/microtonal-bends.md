## microtonal-bends.md

Re-verified against the current checkout: only the old global bend model exists (`bend_range` in semitones 1-48, default 2; MPE per-note bend via `mpe_enabled`; `vibrato_rate/depth/shape` in Advanced PERFORMANCE). There is no BendEngine, bend source stack, per-string ranges, quantise, scale loading, pre-bend, curves, BEND page, popover, mirror, presets or `Bend.*` test; the techniques-branch work never landed here. MB-1, MB-4 and MB-6 are PARTIAL and the rest MISSING.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| MB-1 (§0.1, §0.3, §1) | Continuous per-string pitch; ordered source stack global + per-string + vibrato + pre-bend + slide, adding | `bend_range` (semitones 1-48, default 2) global bend, MPE per-note bend (`mpe_enabled`), `vibrato_rate/depth/shape`; no ordered source stack, per-string or pre-bend sources | n/a | - | PARTIAL |
| MB-2 (§1) | Per-source scale factor + optional latency compensation — no latency compensation on branch | none (no per-source scale factor or latency compensation) | n/a | - | MISSING |
| MB-3 (§0.2, §2) | Global bend source PB / expression / CC; fretboard drag | none (`bend_range` driven by pitch wheel only; no source selector) | none (no BEND page; no fretboard bend drag) | - | MISSING |
| MB-4 (§2) | Global range cents default 200, advanced 2400 (branch: stock 2400, adv 4800) | `bend_range` in semitones (default 2 = 200 c, max 48 = 4800 c); not `bend_global_range` in cents or a PhysicalRange row | Advanced PERFORMANCE bend range control | - | PARTIAL |
| MB-5 (§2, §0.4) | Per-string source MPE Y / CC per string / none; 6 per-string ranges 200 c | none: no BendEngine/bend_* params on this checkout | n/a | - | MISSING |
| MB-6 (§2) | Vibrato source LFO/AT/MPE Z, rate 3-10 (6), depth 5-50 (20), onset 200 ms | `vibrato_rate/depth/shape` (Advanced PERFORMANCE); no source select, onset delay or 50 ms ramp | Advanced PERFORMANCE vibrato controls | - | PARTIAL |
| MB-7 (§2-3) | Quantise none/quarter/semitone/24/22/31/53-EDO/custom with snap strength | none: no BendEngine/bend_* params on this checkout | n/a | - | MISSING |
| MB-8 (§2-3) | Custom .scl / .tun loading | none: no BendEngine/bend_* params on this checkout | n/a | - | MISSING |
| MB-9 (§2) | Pre-bend via keyswitch/CC, amount -200 c, releases | none: no BendEngine/bend_* params on this checkout | n/a | - | MISSING |
| MB-10 (§2) | Bend curve / release curve linear / exponential / drawn | none: no BendEngine/bend_* params on this checkout | n/a | - | MISSING |
| MB-11 (§4) | setBendSourceStack / setBendQuantise; range change at next note-on | none: no BendEngine/bend_* params on this checkout | n/a | - | MISSING |
| MB-12 (§4) | ModMatrix `PreBendEvent` source class — deferred on owner branch | none: no BendEngine/bend_* params on this checkout | n/a | - | MISSING |
| MB-13 (§5) | Techniques > Microtonal Bends sub-tab | none: no BendEngine/bend_* params on this checkout | none (no Techniques tab or BEND page) | - | MISSING |
| MB-14 (§5) | Easy bend/vibrato "…" popover (range + vibrato) | none: no BendEngine/bend_* params on this checkout | none (no bend/vibrato popover) | - | MISSING |
| MB-15 (§5) | CHARACTER PLAYING group "Microtonal" section — mirror at CHARACTER foot | none: no BendEngine/bend_* params on this checkout | none (no Microtonal section in CHARACTER) | - | MISSING |
| MB-16 (§5) | Fretboard cents badge; heavy bend pushes dot along fret gap | none: no BendEngine/bend_* params on this checkout | none (no bend arc or cents badge in `FretboardComponent`) | - | MISSING |
| MB-17 (§6) | Cascade: bend compatible with all; bent slap | none: no BendEngine/bend_* params on this checkout | n/a | - | MISSING |
| MB-18 (§7) | Five presets (Whole-Tone, Quarter-Tone Blues, Maqam, Wide Vibrato, Whammy Two-Octave) | none: no BendEngine/bend_* params on this checkout | n/a | - | MISSING |
| MB-19 (§8) | MIDI export per-source SysEx; generic PB split per channel — deferred on owner branch | none: no BendEngine/bend_* params on this checkout | n/a | - | MISSING |
| MB-T1 (§9) | Test: global +100 c on every note | none (feature absent) | n/a | - | MISSING |
| MB-T2 (§9) | Test: per-string bend only string 3 | none (feature absent) | n/a | - | MISSING |
| MB-T3 (§9) | Test: vibrato onset delay; depth within 50 ms | none (feature absent) | n/a | - | MISSING |
| MB-T4 (§9) | Test: quarter-tone snap lands on 50 c | none (feature absent) | n/a | - | MISSING |
| MB-T5 (§9) | Test: .scl within 0.5 c | none (feature absent) | n/a | - | MISSING |
| MB-T6 (§9) | Test: pre-bend -200 c releases | none (feature absent) | n/a | - | MISSING |
| MB-T7 (§9) | Test: range change mid-note no jump | none (feature absent) | n/a | - | MISSING |
| MB-T8 (§9) | Test: bend + slap on low E | none (feature absent) | n/a | - | MISSING |
| MB-T9 (§9) | Test: preset round-trips every field | none (feature absent) | n/a | - | MISSING |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=3 MISSING=25 OWNED=0 -->
