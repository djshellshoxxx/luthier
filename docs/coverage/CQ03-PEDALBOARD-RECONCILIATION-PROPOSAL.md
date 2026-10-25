# CQ03 and pedalboard admission: review candidate

Status: proposal only. The checked-in CQ03 assertion and product remain unchanged.
The candidate patch is in `probes/cq03-reconciliation-candidate.patch`; independent
coordinator review is required before applying it. No parameter is renamed,
removed, reordered, hidden, or filtered from serialization.

## Original requirements and observed conflict

`spec/cpu-quality-modes.md` section 3 makes the machine's global performance
quality a preference, not an APVTS parameter or preset field. Its CQ-03 criterion
requires presets not to change this quality and the feature not to add/reorder
host parameters. The existing executable test broadly rejects every registered
ID starting with `quality` or containing `cpu`.

Separately, `spec/reference-pedalboard/specs/BOARD_CORE.md` section 13 explicitly
names `ref_board_cpu_cap` as the user's board admission limit (default 30%). This
control limits admission using declared model costs. It does not select the
global QualityController level or persist the automatic governor's live state.
The board's separate overload response is not this admission parameter either.

At frozen source `b5bec374f01ede236350300770f5721709a39831`, the original native
CQ03 fails exactly one of 1,037 checks: `unexpected parameter ref_board_cpu_cap`.
GitHub run 40109481251, Linux Pro job 120918194917 has the same sole CQ03 failure
among 1,177 checks. Different total counts are retained, not asserted equivalent.
The native binary SHA256 is
`63ec5f392e9fcc7069832de0e17286f13d12430ced091f98a151476e3fd27538`.

Actual registration route: `Parameters.cpp` appends `extendedHostParameters`;
entry 20 is the integer `ref_board_cpu_cap` with range 1..100 and default 30.
ExtendedHostAutomationCapture converts host writes to physical values.
`PluginProcessor::serviceReferenceExtendedParameters` maps entry 20 to the
structural `BoardModel` key `cpuCapPct`. Preset JSON filtering cannot fix the
failing assertion, which inspects `getParameters()` directly.

## Proposed clarification and regression

Retain the original forbidden-name predicate for every registered ID except the
single exact, case-sensitive ID `ref_board_cpu_cap`. Do not introduce a `ref_`,
board, CPU-prefix, or arbitrary whitelist exception. Preserve all existing
preset/global-quality/oversample checks verbatim. This explicitly clarifies the
old feature-scoped requirement when combined with the later mandated board ID;
it does not claim the old blanket predicate passes.

The additional regression checks exactly one integer admission parameter,
range 1..100, default 30, and its original neighboring IDs. It writes 1, 100 and
30 through the real host parameter and public host-state service, verifies the
actual BoardModel cap, and verifies global Low preference and live Low level
remain unchanged. Each write also rechecks the original forbidden global
quality fields in preset output. Existing CQ03 still checks every actual other
registered ID and every available factory preset.

Candidate-only native relink against the same frozen product objects passes
1,037 existing checks plus 28 new isolation checks. This is focused evidence,
not an integrated full-suite result. The separate GUI test in that diagnostic
binary remains red; its failures have not been waived.

## Evidence and independent review

- `probes/cq03-original-b5.txt`: unchanged native failure.
- `probes/cq03-candidate-b5.txt`: candidate-only focused results.
- `probes/timefx-ci-job120918194917-focused.txt`: exact authorized job-log excerpt.
- Full local immutable source/command/log artifacts:
  `/workspace/luthier-evidence/timefx-cq03-p08/diagnostic`.

Relevant Claude commit `b3ec7a41` does not address CQ03. Its blanket `ref_`
intentionally-hidden GUI classification would suppress unrelated missing board
controls and is not part of this proposal. `6d9048b5` changes only the separate
board clock-glide API test. Neither was merged or adopted here.
