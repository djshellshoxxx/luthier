# Luthier consolidation and continuation prompt

Work only on `djshellshoxxx/luthier`. Use the existing environment and one
canonical integrator. First read the repository instructions and current
handoffs, inspect all worktrees and dirty/untracked files, and query live PR
heads, checks and active external owners. The SHAs below are observations at
the time of writing, not a substitute for a fresh check. Preserve every unique
commit, local edit and original failure; do not force-push, reset, delete
branches, overwrite release assets or duplicate an active writer.

The selected first-release scope is Phase90, CE-2, CE-2W, DD-3T, Dyna Comp and
RC-5. Keep the other twelve named models and their source/specifications for
future work. Do not lower CPU, THD, audio, realtime or safety limits. Keep
experimental models unregistered until their required gates pass. Preserve
authorship, licenses and the truthful software-artwork description.

Current source lineage: draft PR #83 `claude/quirky-darwin-eoesyi` at
`a8f878d509ed52163331d899b7e77b9545ae9cf2` targets PR #82's integration branch
`25cd2135218a2974244988404fb209cf684a1f0d`; master was
`0f62c3d67bdba055b41e66c7ab8ac9ab4b728662`. PR #83 contains the histories of
#73, #77, #79, #80 and #82. Do not merge those old PRs separately to recover
their work. In particular, merging #77 to master can start its beta2 publisher;
inspect all current workflow side effects before any merge. The newer work is
not release-qualified: Windows Free and Pro built and plugin validators passed
in run 40678198637, but unit tests failed; native UI and Pro painting issues,
CE-2 THD and other model acceptance gates remain. Recheck exact CI outcomes,
review status and current ownership. An external owner was working on native
UI/paint; RC-5 source ownership was not confirmed at the last handoff.

Read `docs/handoff/PEDALBOARD-2026-10-28-CONTINUATION.md`,
`PEDALBOARD-2026-10-27-BANK-GRIND.md`, and on branch
`codex/luthier-ce2-evidence-20261028` at
`d0fa0df76ff32dc8189994b0a138fa58c4316b3a` the CODEX-RESTART,
CE2-ENDPOINTS and RC5-TAP-INTERFACE handoffs. The latter two are documentation
and proposals, not tested DSP changes. The original 81 untraced looper readiness
failures remain unattributed; a later traced run is separate evidence.

PR #76's original head `a2aa30d32f034d9cfa90c01d2119c47023155774` contains a
stress-runner false-green for zero selected tests and overwrites existing logs.
The separate `codex/luthier-soundgrind-repair-20261030` branch at
`e01cc8db3872da76a9a498774f3b0c65d07f97bf` repairs those cases and has seven
passing focused fake-process tests plus a narrow check against an older Linux
test binary. See `docs/handoff/SOUNDGRIND-2026-10-30-REPAIR.md`. It has not had
independent review or CI, is not merged, and does not prove product stress
acceptance. Review it, validate actual runner compatibility and failure paths,
and reconcile it with the latest integration tree without overwriting another
owner's work. Its random scheduling can omit valid suites and it buffers full
child output; don't claim broader coverage than it provides.

PR #81's functional lipo corrections and corresponding test already exist in
the observed #83 source; its original branch also contains generated Python
cache files. PR #78 has potentially useful wiki content but conflicts with the
newer README; reconcile selectively while preserving current status and branding.
PR #72 is an incomplete distinct trial-lock change that conflicts with the
pedalboard tree and needs separate product and engineering review. Recheck all
of these facts before acting.

After ownership and branch reconciliation, continue the highest-value ready
six-pedal implementation slice with focused RED/GREEN tests and appropriate
sanitizer/RT checks. The RC-5 tap handoff proposes an event route but flags
clock authority, persistence and host automation integration decisions; do not
reuse loop pedal gestures as tap tempo. Keep test and source ownership disjoint,
integrate serially, and record exact source SHA, commands, results and remaining
gates at each milestone. Push secret-checked work-branch checkpoints where
authorized and verify remote SHAs. Do not claim a passing full suite from a
focused test or a working product from compilation alone.

Once source qualifies, test Windows standalone, macOS Intel and Apple Silicon,
VST3, CLAP and supported Linux builds with actual validators/hosts. Separate
automated results from hardware/DAW/listening, signing and notarization gaps.
Publish only under current explicit authorization and after applicable QA gates;
never overwrite a release or label unqualified artifacts complete. Do not buy
infrastructure, create credentials, change security settings or submit PR
approvals without the applicable specific authorization. Paid Claude requests
must use the existing shared broker and its current budget/reserve, if authorized.

Honor prior denials: do not retry or reroute the denied Actions read for run
40292011494, the separately blocked additional UI callback stress, denied
`/proc` executable read, or failed native Library packet writes. Their exact
history and private checkpoint recovery details are in the restart handoff.
Do not use a replacement environment to evade an access or usage limit. Report
precise blockers and preserve a restart-ready handoff if work cannot continue.
