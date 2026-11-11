# Cabinet asset keys: display-label regression

2026-10-25. This is a lookup correction, not a new cabinet model or a decay-gate exception.

`CabinetEngine::irFileNameFor` used generic UI names as resource identifiers. The default
Vintage30/SM57 requested `4x12/british_60_w_modern_classic_dynamic_cap_edge_close.wav`,
which does not ship. In the preserved resource tree, token fallback selected
`celestion_vintage_30_neumann_u87_cap_edge_close.wav`. The corrected lookup requests
and selects `celestion_vintage_30_shure_sm57_cap_edge_close.wav`.

Stable enum-indexed asset keys now cover every cabinet, speaker, mic, position and
distance. UI labels and saved enum values do not change. Array-count assertions detect
added choices. Existing invalid-enum behavior is retained: speaker/mic clamp; invalid
cabinet, position and distance use 4x12, cap edge and close respectively.

For unsupported combinations, existing exact-first, same-cabinet token scoring and
missing-cabinet 4x12 fallback remain. Equal-score cabinet candidates now use ascending
case-sensitive filename order. This is a reproducible fallback, not an acoustic
nearest-neighbor claim. Body-IR fallback retains its previous policy. User-supplied IR
loading, mic-stage bypass, convolution normalization and procedural fallback are untouched.

## Tests first and independent review

The initial three tests failed 1,015/1,591 checks before any production edit. The final
four-test fixture (including invalid enum guards) fails 1,017/1,593 checks against the
original objects and passes all 1,593 against the correction. It covers all enum key
choices, all 504 actual shipped cabinet assets, unsupported combinations, display labels
and invalid inputs. The shipped catalog test establishes correspondence to real files;
the literal key test alone would not do so.

Engine worker independently reviewed the live source diff: enum/fallback semantics,
array bounds and cabinet-only lexical ties had no blocking findings. It correctly noted
that unsupported choices retain token similarity, not a physical nearest-neighbor model.

Selected existing regressions with the corrected objects:

| Fixture | Result |
|---|---|
| Cabinet filter, including initial three asset tests | 8 tests / 1,638 checks pass |
| IrLibrary | 2 / 6 pass |
| IrReload | 3 / 47 pass |
| MicPlacement.aUserIrBypassesItsMicsStage | 1 / 6 pass |
| MicPlacement.legacyFidelityAgainstTheShippedIrs | 1 / 5 pass; 420 legacy IR comparisons |
| Original Combo.releasedStringIsDampedQuickly | 24/25 checks pass; PStyle remains below 35 dB |
| Normalization.ON03_FactoryTableDoesNotDrift | 4/4 preset loads fail in both b5 original and overlay; calibration comparison blocked |
| NormalizationGolden | Compiler-specific golden skipped (gcc-14 versus clang-18); no golden acceptance claimed |

The normalizer's separate target fixture reports 7 checks passing, but all six selected
states render identically; combined with the confirmed preset-load failures this does
not qualify factory normalization after a corrected cabinet asset selection. No calibration
table or golden hashes were regenerated.

## Unchanged decay criterion and remaining blocker

No production damping, latency or decay test code changed. The actual b5 executable
still fails the original 35 dB / 250 ms / 30 ms output-window criterion for PStyle note42.
An instrumented frozen-source run with the corrected two objects measures:

| Measurement | Original | Corrected lookup |
|---|---:|---:|
| PStyle output attenuation | 34.7026 dB | 34.7390 dB |
| PStyle raw-string attenuation | 54.7552 dB | 54.7552 dB |
| Output RMS before release | .229847 | .212501 |
| Output RMS after release | .00422968 | .00389414 |

Thus the wrong-mic asset was real, but correcting it does not close the output decay
release blocker. The separate SUS-08 raw-string 40 dB criterion passes this case. Earlier
latency-aligned-window calculations remain diagnostic only; the original oracle is intact.

## Reproduction and provenance

Full immutable run artifacts and command arrays are at
`/workspace/luthier-evidence/cabinet-asset-fix/`. Compile commands derive from
`/workspace/luthier-evidence/decay-probe/commands.json`; link inputs are the separately
copied `decay-02-link-snapshot`, with only CabinetEngine.cpp and IrLibrary.cpp objects
replaced and the new test object added. The original CombinationTests object is retained.
The two production source files, CabinetEngine.h, CabinetVoices.h and CombinationTests.cpp
have no changes between original02 and b5. A fresh b5 native binary independently
reproduced the original decay failure. This is a targeted object-overlay result, not a
claim that all products were rebuilt with this change.

All 1,253 resource files are SHA256-identical between the frozen control, b5 executable
Resources and current root Resources. The committed provenance JSON records canonical
manifest hashes. Raw red/green, path-selection, decay and selected regression logs are
under `pedalboard-dsp-evidence/cabinet-asset-*`.

After integration, normal native commands are:

```
ninja -C build -j3 LuthierTests
build/LuthierTests_artefacts/Release/LuthierTests CabinetAssetKeys
build/LuthierTests_artefacts/Release/LuthierTests releasedStringIsDampedQuickly
build/LuthierTests_artefacts/Release/LuthierTests aUserIrBypassesItsMicsStage
build/LuthierTests_artefacts/Release/LuthierTests ON03_FactoryTableDoesNotDrift
```

Filters match a suite or a test-name substring separately: use `releasedStringIsDampedQuickly`,
not `Combo.releasedStringIsDampedQuickly`, which selects zero tests. An exploratory linker
accidentally included the original Combo object twice; that duplicated run was discarded
and the single-registration run is the committed evidence. Neither setup mistake is
counted as additional acceptance evidence.
