# Legacy bus indices and the three appended board sends

The original PluginBusTests count describes the output list through Jam Bass:
23 buses (main, seven aux pairs, twelve per-string buses, Noise and two Jam
outputs). BOARD_CORE6.8/E54 subsequently requires Pedal B, Wet only and Dry only
to append after that entire prefix, default disabled. The declaration in coherent
production b5 already supplies those26outputs. Retaining the old23count therefore
fails the old oracle even though no legacy index moved.

Commit3acbd218 reuses only Claude's one-line count correction from
b3ec7a41f1accbad83d2f25f74a131200b7b1ea5, preserving its author/date and source
attribution. No ParameterVisibility/GuiReachability change is reused. The
additional positive tests freeze every legacy name/index/default width and add:

- Exact output indices23Pedal B,24Wet only,25Dry only; stereo native widths and
  default-disabled state. Existing Output and Sidechain names/defaults remain.
- All eight independent combinations of the three stereo sends are accepted,
  with each of the23legacy output channel sets unchanged.
- Each appended send rejects mono and quad widths. All four combinations of
  legacy aux/per-string layouts remain accepted with the new sends disabled.

Every original PluginBusTests assertion remains, with only the count updated for
the explicit appended-bus requirement. Existing audio/noise, host MIDI capability,
mono rejection and broader PluginBuses-suite checks execute unchanged. No product
routing or licensing behavior was edited or bypassed. Qualification is the actual
Pro native build; this does not claim a separate Free or DAW/device matrix.

## Actual immutable b5 qualification

Evidence: `/workspace/luthier-evidence/20261025-plugin-bus-fixture-b5-c709b4d7/`.
Source identity:b5bec374f01ede236350300770f5721709a39831. The saved object manifest
has813entries, independently hash-verified before and after. One PluginBusTests
object was recompiled against an exact separate `git archive b5 Source` using
saved b5 target flags, then substituted into the original frozen link command.
No current-main source/object or shared snapshot was changed.

- Original complete PluginBuses suite:10cases/84checks, one failure, at the old
  output count. The target original case has six assertions; five already pass.
- Qualified candidate complete suite:12cases/**446checks**, zero failures.
  All original84checks pass; the new positive cases add111+251checks.
- Original and candidate receive the same immutable Resources and exact b5
  CMakeLists.txt, because an existing host-capability test reads that file.
  The initial harness omitted CMakeLists and recorded a separate infrastructure
  failure; that log is retained as initial-harness-missing-cmake.log. Supplying
  the unmodified pinned file restores the existing test's intended condition;
  no assertion or source-lookup behavior was changed.

Logs, commands, exits, source/binary hashes and provenance are adjacent. The
coordinator's combined native build remains a separate integration gate.
