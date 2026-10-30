# SoundGrind runner repair checkpoint

Base: PR #76 `a2aa30d32f034d9cfa90c01d2119c47023155774` on master
`0f62c3d67bdba055b41e66c7ab8ac9ab4b728662`. Branch:
`codex/luthier-soundgrind-repair-20261030`. This branch is a focused PR #76
repair, not the newer pedalboard integration tree. Do not merge or replace the
separate pedalboard work. The parent PR's source and prior failures are retained.

Original behavior: a zero-test LuthierTests selection exited zero and was
reported as success; opening an existing log in write mode erased old evidence.
The repair creates logs exclusively, checks every supplied filter with the
runner's `--list`, and requires a nonzero successful test summary for each
executed round. Child failures and timeouts remain failures and are logged.
Selection remains random with replacement: this is not a guarantee that every
valid suite is executed, and it is not randomized audio or UI stress.

Tests: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s scripts -p
test_soundgrind_stress.py -v` passed 7 tests. Before the fix, three of the first
four tests failed; after the first fix a separate unscheduled-invalid-filter
test failed, and passed after preflight validation. Tests use fake child
processes; no denied UI callback stress was run. A saved older Linux Free
LuthierTests binary at `/workspace/luthier/build-free-qualification-20261026/`
was used for a single `constantsAndPodSizesAreFrozen` run (exit 0) and a
nonexistent filter (selection rejected, exit 1). This validates the runner
against that binary's report format, not current product or release acceptance.

Follow-up review replaced Unix-shebang fake child executables in the Python
tests with mocked process results so the tests do not depend on Unix execution
semantics. `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s scripts -p
'test_*.py'` passed 21 tests locally on Linux. Actual Windows/macOS execution
and native CI are still unverified; this is not an independent review.

Outstanding: original PR #76 CI included failed platform cells; the repaired
branch has not been run through CI or independently reviewed. The runner still
buffers complete child output before retaining the last 30,000 characters.
Do not claim every suite ran or that any pedalboard release gate is green.
External DSP/UI ownership, six selected pedals, deferred models, prior denials
and current integration gates remain as recorded in the pedalboard handoffs.

Resume: inspect `git status --short --branch`, `git rev-parse HEAD`, the exact
diff against `a2aa30d3`, current PR #76 head and checks, then review and run
appropriate checks before proposing integration. Preserve untracked builds;
do not overwrite the original PR branch, merge, publish or change credentials
without applicable authorization. Obtain this document's final commit SHA via
`git rev-parse HEAD`; it cannot self-identify its own commit.
