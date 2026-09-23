# Autonomous build instructions

You are working unattended. The user is NOT available. Never ask questions or wait for confirmation.

## Rules
1. Read the spec/design .md files in the `spec/` folder to understand the goal (start with `spec/CLAUDE_CODE_BRIEF.md` and `spec/INDEX.md`).
2. Read `spec/TODO.md` and `spec/DECISIONS.md` if they exist, to see where the previous iteration left off.
3. When anything is ambiguous, make the best engineering/design choice yourself and log it in `spec/DECISIONS.md` with a one-line reason. Do not stop to ask.
4. Keep `spec/TODO.md` up to date: remaining tasks, in-progress, done.
5. Work in small steps. After each meaningful step: build, run tests, fix failures, then git commit with a clear message.
6. If something is blocked (missing dependency, broken tool), work around it or move to another task and note it in `spec/TODO.md`.

## Definition of done
- Every item in the spec is implemented
- The project builds with zero errors
- All tests pass
- `spec/TODO.md` has no remaining items

## Completion signal
ONLY when every item above is genuinely true:
1. Create an empty file named .ralph-done in the repo root
2. Output exactly: <promise>COMPLETE</promise>

Never output the promise to escape the loop early. If not done, just keep working.
