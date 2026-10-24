# CLAP same-value host-write intent

The pinned clap-juce-extensions upstream is commit
`55525c9858d4b25687be7759a5e0f70eccef218e`. Its
`paramSetValueAndNotifyIfChanged` discarded incoming host parameter events when
that value already equaled the current parameter. A same-value write is still
an explicit host choice: its listener notification must reach ParameterBridge
before deferred guitar defaults run.

Concrete release regression on production source `3cb71450`: change the guitar
type to SevenString and explicitly retain Active Electronics at zero. The CLAP
wrapper discards that unchanged zero. Audio processing leaves it zero; the later
state save flushes the pending guitar build, initializes active wiring to one,
and serializes one. CLAP had read zero immediately before saving. This produced
three state-reproducibility failures. Explicit notifying zero writes passed all
25 guitar-type reloads, identifying the discarded event rather than JSON or
state load as the defect.

`scripts/patch_clap_host_intent.py` changes only that wrapper function: it skips
`setValue` for equal values, but retains the existing listener notification under
the existing outgoing-feedback suppression scope. It adds no parameter IDs,
exclusions, action replay, processor state fields, locks, or allocations. The
upstream license remains intact. Exact canonical SHA256 preimage/postimage
checks, atomic replacement, LF/CRLF preservation, check-only mode and idempotence
make the patch reproducible and refuse unknown upstream source.

Coordinator integration: include `cmake/ClapHostIntent.cmake` inside the existing
`if (EXISTS "${LUTHIER_CLAP_EXTENSIONS}/CMakeLists.txt")`, immediately before
`add_subdirectory("${LUTHIER_CLAP_EXTENSIONS}" EXCLUDE_FROM_ALL)`. Do not apply
this patch while that wrapper is compiling. No shared target flags are needed.

## Actual qualification

Evidence directory: `/workspace/luthier-evidence/legacy-restore-3cb/`.
The native control used a separate `git archive 3cb71450 Source`, exact current
3cb headers and unchanged current 3cb native objects; no shared snapshot or main
source was modified.

- Native control: 25 guitar types with explicit Active Electronics zero plus
  three randomized legacy parameter reloads passed 2,037 checks. These controls
  intentionally do not simulate CLAP's discarded notification.
- Native exact discarded-event regression: SevenString, unchanged zero, process,
  save, restore failed 2 of 5 checks on 3cb (`unchanged-red.log`), at save and
  reload. This is evidence of the original wrapper defect, not a passing native
  regression registered in the production test runner.
- Five isolated Python patch tests pass (`patch-green.log`). One compiles the
  exact extracted upstream function body: the original fails unchanged-write
  intent, the patched body passes changed/unchanged notifications, changed-only
  mutation and synchronous feedback suppression. Other tests cover pin refusal,
  untouched check refusal, idempotence/license preservation and CRLF retention.
- Actual CLAP candidate: only the wrapper object was rebuilt with the patch,
  then linked with the unchanged 3cb shared-code archive and current dependency
  objects. All five actual state tests pass (`clap-state-green.log`). Full
  clap-validator 0.3.2: **21 tests, 18 passed, zero failed, three skipped, zero
  warnings** (`clap-full-green.log`). Root's unpatched coherent 3cb artifact had
  15 passed, three failed and the same three skipped.

Remaining integration gate: coordinator must wire the configure include and
rebuild/revalidate the production CLAP artifact and CI platforms. This isolated
candidate is qualification evidence, not a replacement for that combined build.
The pre-existing unscoped guitar-write timing policy remains unchanged; this
patch restores notification of explicit input, not a new transaction policy.
