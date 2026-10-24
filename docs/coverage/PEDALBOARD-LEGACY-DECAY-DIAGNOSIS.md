# Legacy released-string diagnosis (2026-10-24)

This is diagnostic evidence, not a production change or a relaxed acceptance threshold. The original `Combo.releasedStringIsDampedQuickly` remains unchanged. The exact frozen source is `02e4e1fc`; the original x86 output threshold remains35dB and the string requirement remains40dB.

## Establishing a coherent control

- Preserved806 linked objects/libraries and the original native executable before the coordinator's next build:153,921,968 bytes, distinct inodes (reflink/copy, not hardlinks). SHA256 manifest: `/workspace/luthier-evidence/decay-02-link-snapshot/manifest.json`.
- The previously named `native-snapshot-02e4e1fc` directory contains later `PluginProcessor.h` fields. Recompiling the unchanged fixture against it changes three processor allocation-size immediates from0x190320 to0x190560. It is not an exact source snapshot. The new probe uses an independent `git archive 02e4e1fc Source` at `/workspace/luthier-evidence/decay-exact-git02`.
- The earlier17-failure overlays lacked runtime Resources beside their executables. IrLibrary searches executable ancestors, not the working directory. A pure relink of all original objects also produced17 failures without Resources, proving that the test edits were not the cause of that discrepancy.
- Restoring the original resource tree beside the relocated executables restores the original result exactly:1/25 failing checks, P-Style Bass note42,34.7dB decay. Both the pure-object relink and unchanged recompiled fixture agree. The preserved resource manifest covers1,253 files/43,955,732 bytes at `/workspace/luthier-evidence/decay-unchanged-control/resource-hashes.json`.

## Signal-path evidence

The probe copies ComboHarness into a private namespace to avoid ODR conflicts, uses exact git02 headers and frozen original link inputs, and records the actual pre-body summed-string buffer after every callback. All samples were available (`missingRaw=0`). No production sources are modified.

| Diagnostic configuration | P-Style output decay | Pre-body decay | Result |
| --- | ---: | ---: | --- |
| Original behavior |34.7026dB |54.7552dB |1/25 fails |
| Measured idle before pluck |34.7167dB |54.7555dB |1/25 fails |
| Mechanical coupling forced zero |34.4627dB |54.1037dB |1/25 fails |
| Release noise disabled |34.7028dB |54.7555dB |1/25 fails |
| Cabinet disabled through its existing parameter |38.6469dB |54.7552dB |25/25 pass |

At baseline, P-Style output RMS before/after release is0.229847/0.00422968; pre-body RMS is0.0567515/0.000103805. Measured idle is0.000399154, far below half the post-release output. The original `renderEvents` call leaves idleRms zero, but supplying the measured idle does not resolve this failure under the unchanged2×idle guard.

The released string path exceeds the40dB requirement by approximately14.8dB. The output35dB margin is lost downstream: the cabinet path changes the measured decay by approximately3.94dB relative to cabinet-off output. This does not establish whether its contribution is frequency weighting, stored impulse-response energy, or an interaction with the amplifier. It does rule out weakening the string release cap as a justified fix from this evidence.

A separate real-user-gesture selection probe changes the actual instrument/tuning (P-Style note30 rather than42) and yields4/25 failures, including P-Style32.3dB. It is not equivalent to the original fixture and has not been adopted.

Commands, copied probe sources and result logs are under `/workspace/luthier-evidence/decay-probe` and `/workspace/luthier-evidence/decay-unchanged-control`. Production damping/cabinet behavior and acceptance thresholds are unchanged. The coordinator's coherent combined native candidate must still retest the original failure; no full-suite pass is claimed here.
