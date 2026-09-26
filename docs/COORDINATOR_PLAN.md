# Coordinator plan: finish, beta-test, fork (product-owner order, 2026-09-26)

## Phase 1: every helper finishes
Merge into `claude/luthier-cloud-session-5lzlix` once each branch reports done and green:

- review
- audit
- feat-search, feat-strings, feat-assist, feat-riffs, feat-jam, feat-browser, feat-mic, feat-normalize, feat-cpu
- spec-sweep

After each merge, build all targets and run the non-Combo suites, then the full suite, clap-validator and pluginval.

## Phase 2: beta test and fix
The auditor, who is also the beta tester, tests every new feature:

- alone and in pairwise combination (extending the Combo harness);
- through the GUI;
- with state save and restore;
- in the Standalone, VST3 and CLAP builds.

The auditor fixes what it finds and reports in `docs/audit/BETA_TEST_REPORT.md`.

**Exit condition:** the full suite is green (or has documented physical exceptions), and the validators pass.

## Phase 3: fork the editions
Tag the finished integration branch as `v1.0-full`. It stays as it is: every feature, no licensing.

| Edition | Branch | Built by | What it does |
|---|---|---|---|
| Free | `claude/luthier-free` | FREE helper | Implements `spec/editions.md`: removes or pares down the headline "$200" features, and keeps the fundamentals and basic effects in full, so it still feels commercial-grade. No licensing. |
| Pro | `claude/luthier-pro` | PRO helper | Everything in the full version, plus licensing per `spec/licensing.md` (details to be discussed with the product owner; the helper uses the spec's recommended defaults until then), plus reverse-engineering hardening. |

The Pro hardening must be in the shipped binary, not just the maintained source. Renaming identifiers in the source alone does almost nothing, because release builds already strip local names. What leaks is:

- exported and dynamic symbols;
- RTTI and typeinfo class names;
- plain-text strings;
- JUCE class names;
- the licence-check structure.

So the Pro helper:

1. Adds a build-time identifier-obfuscation pass. `scripts/obfuscate_pro.py` generates an obfuscated copy of `Source/`, with classes, functions, members and locals renamed to meaningless tokens and a stable map kept out of the shipped artefacts. Release builds compile that copy. The maintained source stays readable, so it can still be fixed and merged from `v1.0-full`.
2. Sets `-fvisibility=hidden`, strips symbols, uses LTO, and exports only the plugin entry points.
3. Encrypts licence-related and other sensitive strings at compile time.
4. Adds anti-tamper integrity checks and scatters the licence checks across the code (per `licensing.md`).
5. Adds a test that scans the built binaries (`nm`, `strings`) for leaked readable names, and fails the build if any appear.

## Phase 4: audit both editions
The auditor and beta tester audit and test both Free and Pro:

- full suite;
- validators;
- GUI;
- edition gating (Free has none of the Pro features; Pro presets load gracefully in Free);
- licensing flows in Pro;
- the Pro binary scan.

The CLI easter egg is the last feature. It goes into `v1.0-full` before the fork, so both editions carry it.
