# Immutable release source and destination

The generic release workflow previously forwarded a mutable branch name to its
platform jobs and final tag creation. A branch advancing during native builds
could therefore produce different source on different platforms. Its shared
build identity also used the workflow commit when an explicit source was selected.

The version job now resolves checkout once to its full commit SHA, rejects an
existing tag at another source (including annotated tags), and forwards the frozen
SHA to every build and draft step. Build identity uses that explicit source. A
new checksum-covered BUILD-PROVENANCE.json records source, selected matrix and CI
run while separating manual acceptance and signing from automated qualification.

Independent review caught a flaw in the first guard: GitHub's by-tag lookup
omits drafts, and update-or-create actions can still edit existing release
metadata even when asset replacement is disabled. That reviewed first checkpoint
is preserved in history and was not integrated into the coordinator source.

The corrected final draft path lists every release page, including drafts, and
fails closed on lookup errors. Per-tag workflow serialization prevents this
workflow racing itself. The publication step rechecks the tag SHA and creates a
missing ref using a create-only API, never a ref update. A concurrently created
or conflicting tag stops publication. The existing update-or-create action is
replaced by gh release create --draft, which creates through POST and uploads to
the newly returned release; there is no PATCH/UPDATE or existing-asset fallback.
The CLI source was independently reviewed at installed2.46.0 and current trunk.
An unrelated administrator forcing tag changes or externally creating duplicate
drafts is outside workflow serialization; this code does not change protection
settings and does not mutate the other draft. Final verification cannot be an
atomic server-side tag-and-release transaction; every output is still a draft.

All native tests, strictness10 validators, LTO and package gates remain
prerequisites; no unfinished-preview bypass is added. No real tag/release was
created by local fixture verification.

Verification: scripts/tests/test_release_workflow_guards.py executes the actual
workflow shell against a scratch real Git remote. Eight tests pass, covering
branch advancement after freeze, annotated tags, conflicting tags, version/path
validation, draft-aware pagination, failed lookup statuses, final tag recheck and
create failure without update/upload fallback. The initial old workflow run
recorded four assertion failures and four missing-guard subtest errors. YAML and
embedded Bash syntax checks pass. Real native package/provenance output remains
pending release qualification.

A second independent review reproduced an early-match SIGPIPE with a large
paginated tag list under pipefail. The guard now uses a here-string instead of
a printf-to-grep pipeline; a10,001-entry early-match regression retains this
case. No reviewed failure was published as a passing production gate.
