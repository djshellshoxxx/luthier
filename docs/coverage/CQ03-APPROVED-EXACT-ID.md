# CQ03 exact-ID reconciliation

Owner approved2026-10-25: allow only the required `ref_board_cpu_cap`, retain
CPU quality preference exclusions, add exact-case regressions and preserve RED.
This approval does not cover the separately blocked UI stress task.

Before change, the actual build8b executable at source5d2f4e58 fails exactly
one of1177 checks: unexpected parameter ref_board_cpu_cap. The verified70-preset
bank is exercised; this is not the earlier empty-bank1037-check result.

The original preset JSON assertions, all70 factory-load/live-quality checks,
oversample ID/choices/default and all unrelated predicates remain intact.
Only the exact case-sensitive required board admission ID is exempted. A shared
test policy rejects neighboring ref_* CPU-quality IDs, suffixes, altered case
and global quality preferences. Additional real-processor checks pin uniqueness,
physical1..100/default30, append-only neighbors and actual host writes while
global preference/live quality stayLow. No production parameter or quality
behavior changes; no general ref_* exemption or JSON serialization filter.

The reused bounded proposal is10cd405a's candidate patch, not its entire divergent
branch. Current before/after commands, hashes and logs are retained under
`/workspace/luthier-evidence/cq03-approved-source8`. Post-build results are recorded
in the next handoff; no pass is inferred from editing this file.

Fresh verification on2026-10-26: test-only incremental build exit0; unchanged
factory setup ON03 passed1/12; corrected CQ03 passed3 tests/1217 checks/exit0
with70 active factory presets. The original1177 checks all pass, followed by
12 exact-ID policy and28 real-processor regressions. Test executable SHA256
`4761d086625a79947d1898ab88fee917b8652951e2ea1729363475a720add2a0`.
Production products were not rebuilt or changed by this test-only correction.
Small before/after logs and identity are in [cq03-approved-evidence](cq03-approved-evidence).
