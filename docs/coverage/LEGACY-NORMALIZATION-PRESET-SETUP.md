# Factory-normalization load diagnosis, exact b5

2026-10-25. This closes the preset-load setup blocker recorded in
[the cabinet asset-key investigation](LEGACY-CABINET-ASSET-KEYS.md). It does not close
the separate original PStyle 35 dB output-decay gate.

## Cause

The unchanged b5 executable fails all four factory preset loads before calibration.
An isolated public-API probe linked to the immutable b5 objects reports Pro edition,
70 factory definitions, zero listed presets and index −1 for all four requested names.
No JSON parser, edition gate or normalization measurement is reached.

The matching resource tree has `Presets/descriptor-calibration.json`, but no
`Presets/Factory` directory. `PresetManager::getFactoryPresetFolder` probes only an
existing shipped Factory directory. It therefore selects
`/home/agent/Documents/Luthier/Presets/Factory`. In this execution environment Documents
is absent and `/home/agent` is not writable (`os.access(..., W_OK) == false): writes are
restricted to the workspace and temporary directories. The processor's normal directory
creation and generated-bank writes consequently produce no bank. No automatic-approval
request or rejection was involved; this was the runtime filesystem restriction.

## Controlled setup correction and retained criteria

A separate copy of the original Resources and the exact original executable was made
under `/workspace/luthier-evidence/normalization-b5/writable-bank/`. Only an empty
`Resources/Presets/Factory` directory was initially added. The unchanged application's
normal generator then wrote its 70 factory presets there. All four requested loads
succeeded; trace comparisons report `generatedMatch=yes`, revision 3, and correct
requested/effective guitar choices. The existing loader also created four migration
backup files under the generated bank. All 1,253 original resource files are unchanged.
This is an explicit environment setup correction, not a claim that the original
resource directory inventory was identical after the run.

The unmodified ON03_FactoryTableDoesNotDrift test passes all 12 checks on the original
b5 executable. The same test also passes all 12 against the cabinet-key correction,
linked from the same b5 object snapshot and using the same generated factory bank.
The factory table, hashes, input phrases, four selected combinations and 4 LU criterion
are unchanged. No production source change was needed to diagnose the load failures.

| Combination | Factory LUFS | Original b5 fresh | Corrected cabinet fresh |
|---|---:|---:|---:|
| p41_g18 | -22.51 | -23.5956 | -22.2469 |
| p68_g21 | -22.27 | -23.1143 | -23.1550 |
| p21_g20 | -31.60 | -30.9785 | -30.0016 |
| p30_g05 | -23.40 | -23.2395 | -23.5536 |

Largest corrected difference is 1.5984 LU, within the existing 4 LU gate. These are four
specified factory combinations, not an exhaustive normalization sweep or an acceptance
claim for compiler-specific golden hashes.

## Provenance and reproduction

Immutable inputs:
`/workspace/luthier-evidence/native-b5-link-snapshot-20261025/`.
All 813 manifest object/library hashes were independently verified with zero mismatches.
The copied baseline executable hash equals the saved native executable hash. Pro flags
are unchanged. Headers were archived from exact b5bec374 source, not borrowed from the
DSP worker's divergent PluginProcessor/LuthierEngine interfaces. Cabinet-overlay source
changes are only the two production files from 71b8dffa; all other link objects come from
the b5 snapshot. Current root build objects were not read or changed.

Artifacts, commands, exact archived source, copied Resources, native baseline and linked
overlay are under `/workspace/luthier-evidence/normalization-b5/`. Committed logs and
provenance are `pedalboard-dsp-evidence/normalization-b5-*`. The public-API probe is
observational; the 12-check acceptance result comes from the unmodified original test.

To reproduce the setup distinction, stage an unchanged executable and Resources copy
in a fresh workspace directory. First run `tests ON03_FactoryTableDoesNotDrift` with
no Factory directory: under this restricted Documents environment it fails four loads.
Create only `Resources/Presets/Factory`, then run the same command in a new process.
The ordinary generator now creates the factory bank and the real calibration comparisons
run. Keep generated files separate from immutable input resources and preserve their
hash/directory delta. No HOME override, edition change, preset substitution, table
regeneration or threshold relaxation is necessary.

This runner must provide either its ordinary writable Documents destination or the
explicit writable generated-bank directory. The controlled workspace-bank route above
already demonstrates the cabinet correction passes the original bounded factory gate.
