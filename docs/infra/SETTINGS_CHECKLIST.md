# GitHub repository settings checklist (owner-only)

These toggles live in the GitHub web UI and can only be set by the repository
owner/admin — CI and automation cannot enable them. Work through them once for
the public `djshellshoxxx/luthier` repo. Each links to the exact settings page.

## 1. Branch protection on the default branch (`master`)

- Settings → **Branches** → Add branch ruleset / protection rule for `master`.
- Require a pull request before merging; require status checks to pass
  (select the `ci` / build and `CodeQL` checks once they have run at least once).
- Require branches to be up to date; block force-pushes and deletions.
- Consider the same ruleset for `codex/luthier-beta` while it is the active base.

## 2. Secret scanning + push protection

- Settings → **Code security and analysis**.
- Enable **Secret scanning**.
- Enable **Push protection** (blocks commits that contain detected secrets).

## 3. CodeQL / code-scanning alerts

- Settings → **Code security and analysis** → **Code scanning**.
- The `.github/workflows/codeql.yml` workflow runs on push/PR to `master` and
  `codex/luthier-beta` and weekly. Confirm **Code scanning alerts** are enabled
  so results appear under the repo's **Security** tab.

## 4. Actions enabled for the public repo

- Settings → **Actions** → **General**.
- Confirm Actions are **enabled** (Allow all actions and reusable workflows, or
  the org policy equivalent).
- Under **Workflow permissions**, ensure workflows can write security events
  (needed for CodeQL upload) — "Read repository contents and packages
  permissions" plus the per-workflow `security-events: write` already declared.
- Public-repo runners for Linux, Windows, and macOS are free; the ci-cadence
  workflow now builds all three.

## 5. Sentry secrets

- Settings → **Secrets and variables** → **Actions**.
- Confirm the repository secrets **`SENTRY_DSN`** and **`SENTRY_AUTH_TOKEN`**
  exist (they are already set). They are only consumed when the OFF-by-default
  `LUTHIER_ENABLE_SENTRY` build option is turned on. See
  [CRASH_REPORTING.md](CRASH_REPORTING.md).

---

Nothing here changes plugin behaviour; these are repository governance and
security settings only.
