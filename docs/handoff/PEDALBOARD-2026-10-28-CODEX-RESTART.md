# Luthier Codex restart handoff — 2026-10-28

## Identity and ownership

Repository: `djshellshoxxx/luthier`. This document is on the owned work branch
`codex/luthier-ce2-evidence-20261028`, built on PR 83 source
`a8f878d509ed52163331d899b7e77b9545ae9cf2`. The two preceding commits
`f769d2d6bd87159fbf5eab1d5dbfc1cc819e99b5` and
`209114b3742ba8714d0cfabbca7acc720ba7d068` add handoff documents only; do not
transfer test results to their commit IDs as if new tests ran. Use `git rev-parse
HEAD` and `git ls-remote origin` to obtain this document's actual commit and
verify the remote before resuming; its own SHA cannot be embedded in itself.

At this checkpoint tracked files were otherwise clean. Preserved untracked local
directories: `build-free-qualification-20261026/`, `scripts/__pycache__/`, and
`scripts/tests/__pycache__/`. Do not clean or reset them. No build/test process
was found during this bounded handoff. No unpushed source was identified before
this document. Local worktrees under `/workspace/luthier-workers/` include older
lanes; preserve them and inspect status before any integration.

The parent is checking current external Claude ownership. Native UI and paint
diagnostics are externally owned; RC-5 source ownership is not yet confirmed.
Do not edit those files or duplicate their work until reconciled. Codex is the
canonical source integrator. No PR approval is authorized without specific user
confirmation. No deployment, publication, new credentials/security settings,
provider purchases or environment replacement. Claude work, if authorized,
goes only through the existing shared broker; consult its current ledger and
reserve before any request, rather than assuming available balance.

## Status and evidence

The selected first-release models are Phase90, CE-2, CE-2W, DD-3T, Dyna Comp
and RC-5; twelve other models remain deferred and preserved. Six selected
models remain unregistered. Existing core and prototype implementation does
not constitute feature or release acceptance.

Read [continuation](PEDALBOARD-2026-10-28-CONTINUATION.md) for source-pinned
RC-5 wrapper, looper origin-underrun fix, Linux sanitizer/RefPedal results,
Windows diagnostics and model gate data. Read [bank grind](PEDALBOARD-2026-10-27-BANK-GRIND.md)
for the original 81 untraced readiness failures and separate traced diagnostic;
the original failure is not retroactively resolved. Read
[CE-2 endpoints](PEDALBOARD-2026-10-28-CE2-ENDPOINTS.md) for the 120-render
private experiment, source and hashes. The five-rate BBD-on low-level THD
gate remains RED versus its unchanged 0.5–3% bound; sampled endpoint blends
do not justify an unsourced correction. The Phase90 CPU <0.4% and 24-bit
criterion, DD-3T CPU <0.5%, CE-2 bypass jump, and Dyna gates remain open; see
the linked source-specific handoffs for measured values and rejected candidates.

Parent reported Windows run `40678198637` on `a8f878d5`: Free 2/2905 and Pro
3/2922 tests failed, including CE-2 THD and native UI lifecycle in both, plus
Pro painting budget. This continuation did not independently read the run;
verify with an authorized available route before relying on detailed diagnostics.
Prior run `40649780764` is recorded in the continuation handoff. No test or build
was run for the two Codex documentation commits or this handoff. Final-source
Windows/macOS Intel and ARM/Linux standalone, VST3 and CLAP validation,
hardware/DAW and manual acceptance, and qualified release packaging remain open.

## Resume in dependency order

1. Inspect current local status, worktrees, live refs/PRs and external owner's
   final handoff; preserve uncommitted and unique work. Compare before merging;
   do not overwrite another writer or force-push. This branch is a documentation
   descendant of PR 83 and has not been merged into that external branch.
2. Once RC-5 ownership is clear, use [tap interface proposal](PEDALBOARD-2026-10-28-RC5-TAP-INTERFACE.md)
   for a test-first bounded integration. It proposes an appended trigger for
   the already-specified TEMPO button, with explicit event/clock/state questions;
   it is not an implementation. Native UI/paint remains with its current owner.
3. Retain all original sound, CPU, safety and realtime thresholds, test actual
   behavior and preserve RED evidence. Reconcile model changes serially; only
   qualify native products on the resulting exact source, then address hardware
   and DAW acceptance and release under current user authorization.

Useful read-only starting commands from `/workspace/luthier`:

```sh
git status --short --branch
git rev-parse HEAD
git worktree list
git ls-remote origin refs/heads/codex/luthier-ce2-evidence-20261028 refs/heads/claude/quirky-darwin-eoesyi refs/heads/claude/luthier-pedalboard-integration
git log --oneline --decorate -20
```

The supported private evidence checkpoint `libfile_1047f71682648191b5c2e265aed98eaf`
version 32 is `luthier-lookahead-traced-1e7aaf1c-20261027.zip`, SHA-256
`b76baa81cbb676166aa0550a3b1b22dddabec7af39767a88772cc9167d4c2fd1`. Recovery
requires published base `22d6b18d5e6bb024efc5143eba9e01ce0836eb46` and the
verified bridge bundle described in earlier handoffs. It predates the PR 83
source and the later endpoint results; do not claim those bytes are in v32.
The endpoint raw files are local under `/workspace/luthier-evidence/`, not
confirmed on GitHub. Preserve the predecessor evidence and original failures.

Prior denied Actions read for run `40292011494` returned HTTP 403; its cause is
unknown. Do not retry or reroute that denied read. The separately blocked extra
UI callback stress and the denied `/proc` executable read also remain excluded.
Two native Library packet writes failed helper discovery with a network error;
do not retry those writes or switch their upload route. These restrictions do
not convert other recorded native tests into passes.
