# Autonomous development instructions

Inspect the latest repository state, active pull requests, CI, documentation, and outstanding issues before choosing work. Do not duplicate an active contributor's changes. Identify a small, independently testable unfinished feature or verified defect within the repository scope. Prioritize correctness, security, release blockers, tests, and consolidation of existing code.

Implement the chosen task rather than only writing a plan. Write or update tests, make modular changes, run available validation, and report what was actually executed. Do not invent test results, completion percentages, or API costs. Respect existing project architecture and licensing. Do not deploy, merge, modify secrets, or alter unrelated workflows. If blocked, document the blocker and complete another independent safe task if time permits.

Keep changes reviewable and focused. The GitHub Actions runner will create a draft PR from changed files. Do not create branches, push, or open PRs from within the Codex session. Do not modify .github/workflows or .github/agent-prompts as part of feature development. Never expose credentials.

Use efficient context and tokens; use available project instructions and skills when appropriate. Research external implementations only when needed and evaluate license compatibility before reuse.
