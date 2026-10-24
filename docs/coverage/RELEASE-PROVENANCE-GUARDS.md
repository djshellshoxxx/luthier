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

The final draft step fails closed unless the release lookup explicitly returns404;
an existing release or access/network/server error stops it. The existing release
action additionally disables asset replacement using its documented
overwrite_files input. No existing tag, release or asset is changed by these
source changes. All native tests, strictness10 validators, LTO and package gates
remain prerequisites; no unfinished-preview bypass is added.

Verification: scripts/tests/test_release_workflow_guards.py executes the actual
workflow shell against a scratch real Git remote. Six tests pass, covering branch
advancement after freeze, annotated tags, conflicting tags, version/endpoint
validation and simulated404/200/403/500 responses. Against the old workflow the
same harness records four assertion failures and four missing-guard subtest
errors. YAML and embedded Bash syntax checks pass. No release or tag was created;
real platform package/provenance output remains pending release qualification.
