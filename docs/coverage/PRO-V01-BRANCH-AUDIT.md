# Focused residual branch audit for Pro v0.1 Beta

Reviewed 2026-10-17 against `master` `ea50a2b50b9901eed3d9914f03a804acb7ac2446`.
Read-only source/history review; no branch switches, merges, commits, deletions or pushes.
This review covers the five historical residual branches below. PR72, PR73 and PR74 have separate release dispositions.

## Results

| Remote branch | Commit absent from master ancestry | Evidence and disposition |
| --- | --- | --- |
| `claude/luthier-cloud-session-5lzlix` | `e1ec373663d10d736ff114e391dbb348233b274e` | **Distinct unfinished feature; preserve and defer.** Introduces trial lifecycle in `Source/Updates/Telemetry.*`, runtime edition authority in `Source/Support/Edition.*`, processor startup wiring, `TrialLockTests.cpp` and `spec/trial-lock.md`. Current master has compile-time Pro authority plus test overrides, without these trial APIs. This is not redundant. The commit explicitly calls itself increment 1 and says increment 2 must make the lock visibly/audibly effective via edition substitution, tab gating and countdown/unlock UI. Merging only this historical increment does not establish a verified complete trial product. Deferral is release scope, not a claim that the feature has already shipped. The branch records an older owner decision for a trial; publishing a non-expiring full Pro beta should follow the current release instruction rather than treating this audit as a product-policy decision. |
| `claude/luthier-codex63-integ` | `5fbf05133e6f269c96b6f6b9c32bbf3c0f72f644` | **Merge identity only; no integration needed.** Both merge parents, `f60f7f98` and `ebb3597e`, are already ancestors of master. `git show --remerge-diff 5fbf0513` produces no residual patch, and its combined diff is empty. The large first-parent diff is the already-inherited other parent, not omitted work. Do not interpret the five normalization files in the second-parent diff as new residual changes; they come from the already-inherited first parent. |
| `claude/luthier-micface-workshop-fix` | `2783c66ba755d3f406b0d8ee08f557d13c7c8ed5` | **Superseded by stronger shared fixture.** Adds `setWorkspaceTabNamed("MOD")` separately to two tests in `Source/Tests/MicPlacementUiTests.cpp`. Master already includes ancestor `9a00a3a3e0b037c0afc4f0fe2e707f6a22243c26`: `Rig` selects MOD whenever entering Advanced, covering both tests and other uses; `TabPreference` restores the previously persisted tab afterward. Cherry-picking the residual would duplicate setup. |
| `codex/luthier-beta-looper-concurrency` | `4e02fc6633ce2fe1c5107ea1fb4e66b9af18ca09` | **Already exact source equivalent.** The commit corrects three `juce::String::indexOf(startIndex, text)` calls. The entire `Source/Tests/LooperConcurrencyTests.cpp` blob at this residual commit is byte-identical to master. Both have SHA256 `0adb6735e0f6f1f462ccde7be66810e30eea9a328cadc5070bd1d5777375569d`. No cherry-pick needed. |
| `codex/luthier-licensing-core` | `7bef96afa73c45e8cd1fe5bd998e87dfdfd83355` | **Already exact source equivalent.** Adds `<juce_cryptography/juce_cryptography.h>` before SHA256 use. The entire `Source/Licence/MachineFingerprint.cpp` blob at this commit is byte-identical to master. Both have SHA256 `6a4aa222a051dc64e85ff3cdf06313610ce394d59e8ff4d4c5f70a6bdf0ea6f7`. No cherry-pick needed. |

The two exact source fixes are visible in master's merge history through `3f918153` and consolidation PR67 (`33c551ed`), even though their original commit identities are absent. Use source equivalence rather than ancestry alone to decide whether work remains.

## Verification boundary

For each branch, `git log master..origin/<branch>` contains exactly the one residual commit listed above. The audit inspected those commits, master blobs, ancestry and merge resolution differences. It does not establish runtime correctness or native platform build success; release CI remains required. No relevant implementation delta from these five branches needs integration for the currently scoped full Pro release. The trial branch remains intentionally preserved unfinished work.
