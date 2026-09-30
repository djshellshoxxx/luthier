## muting-rhythm.md

Not implemented in the compiled tree: `Source/WIP/Rhythm/Muting.*`, `WIP/UI/MuteGroup.*` and `WIP/Tests/MutingTests.cpp` are uncompiled drafts (CMakeLists excludes `Source/WIP/`); there is no `DSP/Techniques/MuteEngine`, no `mute_type` in `RhythmPattern`, no MUTE sub-tab, Easy button or Mute Row. The only live muting is the older PalmMute/MutedPick keyswitch articulation and strum-dynamics `chuck_amount`/`chuck_damping` (rows MR-8, MR-12 PARTIAL). Re-verified.

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| MR-1 (§0.2, §1) | Seven mute types (open, palm light 150 ms / heavy 50 ms / extreme 20 ms, ghost, chuka, fret mute); muted strike is a thump + damped ring | - (`WIP/Rhythm/Muting.*` draft, not compiled) | - | - | MISSING |
| MR-2 (§1) | Per-type position/pressure defaults, overridable per step | - (`WIP/Rhythm/Muting.*` draft, not compiled) | - | - | MISSING |
| MR-3 (§0.3-0.4, §2) | `mute_type` per pattern step; JSON extension; missing = open | - (`WIP/Rhythm/Muting.*` draft, not compiled); `Rhythm/RhythmPattern` has no mute step | n/a | - | MISSING |
| MR-4 (§2, §4) | Live 16-step Mute Grid (runtime pattern) synced to host tempo, in Advanced Col 4 | - (`WIP/Rhythm/Muting.*` draft, not compiled) | - (`WIP/UI/MuteGroup.*` not compiled) | - | MISSING |
| MR-5 (§3) | Master mute mode overrides all pattern types | - (`WIP/Rhythm/Muting.*` draft, not compiled) | - | - | MISSING |
| MR-6 (§3) | Palm position (35 mm) / pressure (0.5) | - (`WIP/Rhythm/Muting.*` draft, not compiled) | - | - | MISSING |
| MR-7 (§3) | Fretting-hand mute style rock spread / classical | - (`WIP/Rhythm/Muting.*` draft, not compiled) | - | - | MISSING |
| MR-8 (§3) | Chuka source: strums with dynamics < 0.3 | `LuthierEngine.cpp:1393-1405` strum-dynamics chuck (`Damping::Chuck`, `chuck_amount`/`chuck_damping`); no Chuka mute type or dynamics<0.3 rule | `UI/StrumGroup.h` chuck amount/damping | - | PARTIAL |
| MR-9 (§3) | Random mute humanise 0-1 (default 0) | - (`WIP/Rhythm/Muting.*` draft, not compiled) | - | - | MISSING |
| MR-10 (§3) | Ghost note velocity 0.4 | - (`WIP/Rhythm/Muting.*` draft, not compiled) | - | - | MISSING |
| MR-11 (§4) | RhythmEngine passes mute_type with each note-on | - (`NoteOnEvent` has no muteType; `RhythmEngine` has none) | n/a | - | MISSING |
| MR-12 (§4) | StringEngine applies initial damping + post-strike release | `DSP/String/StringEngine.h:48,100` `Damping` (LightTouch/PalmMute/Chuck) and `LuthierEngine.cpp:1395` chuck damping exist; no Muted damping (`setMutedDamping`) or post-strike release | n/a | - | PARTIAL |
| MR-13 (§5) | Mute stacks with slap/scrape/slide/tap/bend | - (`WIP/Rhythm/Muting.*` draft, not compiled) | n/a | - | MISSING |
| MR-14 (§6) | Six presets (Metal Chug 16ths ... Classical Staccato) | - (grid presets only in WIP `Muting.cpp`) | - | - | MISSING |
| MR-15 (§7) | Techniques > Muting sub-tab: 16-step editor + all controls | - | - (`WIP/UI/MuteGroup.*` not compiled; no TECHNIQUES tab) | - | MISSING |
| MR-16 (§7) | Easy Playing strip 4-way Mute button (Off/Light/Heavy/Extreme) | - (no `mute_master_mode` param) | - (`UI/EasyPanel.h` has no Mute button) | - | MISSING |
| MR-17 (§7) | RHYTHM tab Mute row | - | - (`UI/RhythmPanel` has no mute row) | - | MISSING |
| MR-18 (§8) | MIDI export: Luthier SysEx NOTE `mute_type`; generic text meta | - (deferred in the spec work; no mute_type in `Export/MidiPerformance.cpp`) | n/a | - | MISSING |
| MR-T1 (§9) | Test: palm heavy low E T60 40-60 ms | - (`WIP/Rhythm/Muting.*` draft, not compiled) | n/a | - (`WIP/Tests/MutingTests.cpp` draft, not compiled) | MISSING |
| MR-T2 (§9) | Test: ghost < -30 dB pitched | - (`WIP/Rhythm/Muting.*` draft, not compiled) | n/a | - (`WIP/Tests/MutingTests.cpp` draft, not compiled) | MISSING |
| MR-T3 (§9) | Test: chuka at dyn 0.2 no sustained pitch | - (`WIP/Rhythm/Muting.*` draft, not compiled) | n/a | - (`WIP/Tests/MutingTests.cpp` draft, not compiled) | MISSING |
| MR-T4 (§9) | Test: grid paint applies within a bar | - (`WIP/Rhythm/Muting.*` draft, not compiled) | n/a | - (`WIP/Tests/MutingTests.cpp` draft, not compiled) | MISSING |
| MR-T5 (§9) | Test: humanise 0.5 ~50% over 100 loops | - (`WIP/Rhythm/Muting.*` draft, not compiled) | n/a | - (`WIP/Tests/MutingTests.cpp` draft, not compiled) | MISSING |
| MR-T6 (§9) | Test: preset save/restore round-trips grid | - (`WIP/Rhythm/Muting.*` draft, not compiled) | n/a | - (`WIP/Tests/MutingTests.cpp` draft, not compiled) | MISSING |
| MR-T7 (§9) | Test: patterns without mute_type play identically | - (`WIP/Rhythm/Muting.*` draft, not compiled) | n/a | - (`WIP/Tests/MutingTests.cpp` draft, not compiled) | MISSING |
| MR-T8 (§9) | Test: slap carries its mute; grid override propagates | - (`WIP/Rhythm/Muting.*` draft, not compiled) | n/a | - (`WIP/Tests/MutingTests.cpp` draft, not compiled) | MISSING |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=2 MISSING=24 DEFERRED=0 -->
