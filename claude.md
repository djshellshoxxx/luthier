# Interference VST specification workflow

Every Markdown file under `specs/` is part of the product specification,
whether or not another file links to it. The master prompt defines
precedence where specifications overlap.

Before writing code:
1. Run `rg --files specs -g '*.md'` to inventory all specifications.
2. Read every file in that inventory.
3. Create or update `docs/spec-coverage.md` with one row per actionable
   requirement: ID, source file, implementation location, verification
   method, and status.
4. Resolve conflicting requirements using the documented precedence.
   Record any unresolved conflict before implementing the affected part.

During implementation:
- Keep the coverage table current.
- When a spec file is added or changed, read it and update coverage.
- A GUI control, class, or passing build alone does not verify a requirement.

Before reporting the VST complete:
1. Run `rg --files specs -g '*.md'` again.
2. Account for every file and every actionable requirement in the
   coverage table, including specs added during development.
3. Run the relevant tests and verify audio behavior, GUI wiring,
   routing, presets, exports, and release gates.
4. Mark a requirement `verified` only with evidence. Mark anything
   unfinished `pending` or `deferred` with a reason.
5. Report the coverage results and remaining release blockers. Do not
   claim commercial readiness while a required item is unverified.