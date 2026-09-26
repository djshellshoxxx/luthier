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

## Model policy for helpers (2026-09-26, product owner)

Default split stays: Sonnet for routine gap work, Opus for hard DSP and
merges, Haiku for docs and text.

**Fable escalation rule.** Escalate a single stuck bug to a short Fable
helper (`claude-fable-5-1`) only when ALL of these hold:

1. Opus has already made two failed attempts on the *same* bug.
2. Confidence is medium-to-low that Opus fixes it on its next turn, or the
   evidence says it will need several more turns first. (If Opus looks
   likely to land it next turn, stay on Opus — do not escalate.)
3. The Fable helper is scoped to that one bug with a tight brief.
4. Once the bug is fixed and verified, work switches back to Opus. Fable is
   an escalation for the stuck bug only, never the standing model.

Measure each escalation: note the usage meter before and after, and compare
against what the two Opus attempts used.

## Pro copy protection: strength target (2026-09-26, product owner)

The Pro edition's copy protection is to be as strong as we can reasonably
make it, layering the standard anti-reverse-engineering hardening already
listed in Phase 3 (build-time identifier obfuscation, hidden visibility +
symbol strip + LTO, compile-time string encryption, scattered licence
checks, anti-tamper integrity checks, the binary leak-scan test) AND at
least one novel, project-specific mechanism that does not appear in
off-the-shelf DRM.

Candidate directions for the novel mechanism, to be designed and chosen with
the product owner when we reach the Pro phase (this is DRM for our own
commercial product; the design is recorded here, not built yet):

- **DSP-fingerprint gating.** Fold the licence state into the physical
  model itself, not a separate boolean, so that a bypassed or patched check
  leaves the audio subtly and progressively wrong (detune drift, damping
  errors) rather than cleanly unlocked. The correct coefficients are derived
  from the licence, so a cracked binary that skips the check produces an
  instrument that measurably misbehaves on the test phrases.
- Tie that derivation to a per-licence value so a shared/leaked licence is
  distinguishable, and keep the "unlicensed" path a graceful demo (noise
  burst / periodic mute) rather than a crash.

Exact licensing model, activation, and offline policy remain Q-L1..Q-L9,
still to be settled with the product owner before the Pro helper starts.

## Expansion wave (2026-09-26, product owner): features + new instruments

Token discipline: every new helper's system prompt gets docs/helpers/TERSE_MODE.md.
Model per task: Haiku = docs/product text; Sonnet = routine specs + UI-side
feature implementation; Opus = hard DSP (new instrument acoustics, Chapman
Stick excitation, tone-match), merges, and the sound-accuracy audit; Fable =
stuck-bug escalation only (2-failed-Opus rule).

### A. Product doc (Haiku, PRODUCT-DOC)
Full feature + capability list and a product description, from the specs and
built code. Living draft; refresh after the new features land.

### B. Feature specs + implementation (all tiers from the desirability list)
Specs first (Sonnet, FEAT2-SPECS), then implementation (Sonnet, with Opus for
tone-match DSP), then audit + test.
- T1-1 Resizable/scalable UI (user scale 75-200%, remembered).
- T1-2 Built-in tuner + adjustable global tuning reference (A=432..446).
- T1-3 MIDI Learn / CC mapping on any parameter.
- T1-4 Constrained "Randomize" + A/B compare in header.
- T2-1 Doubler (auto double-tracking) — confirm spec.md:315 build state.
- T2-2 Tone-match end-to-end (tone-match.md) — Opus for DSP.
- T2-3 Notation + MIDI export INCLUDING guitar TAB export format.
- T3-1 Amp/cabinet section with user IR (impulse response) loading.
- T4 polish: tooltips+learn toggle, in/out meters + clip, sympathetic-string
  resonance control, round-robin / humanized pick attack. Confirm which already
  exist before building.

### C. New instruments (research + DSP + graphics + integration)
None of these exist today (workshop is modular: scale length, string count,
5/6/8-string and multi-scale already supported). Split by research need:
- Needs scientific-literature research (Opus, INSTRUMENT-RESEARCH → per-model
  spec): Chitarra sarda; Guitarrón mexicano (fretless mariachi bass, octave
  courses); Chapman Stick (TWO-HAND TAPPING excitation, dual zone/split — a new
  excitation model, biggest item); Zon (graphite/composite neck material →
  even, bright, long sustain).
- Mostly buildable on existing part machinery, lighter research (Sonnet):
  Tenor guitar (4-string, CGDA/DGBE, short scale); Acoustic bass guitar (hollow
  acoustic body + 34" bass neck); Extended-range bass (5/6-string already
  partly there — confirm and finish).
Each: body/part acoustics model, workshop graphical image (guitar-illustration.md
/ guitar-workshop.md), factory preset, full integration, tests.

### D. Accuracy audit (Opus, ACCURACY-AUDIT) + beta test
- Verify EACH existing and new instrument's sound is as accurate as the model
  allows; find any guitar using the wrong audio model or mis-modeled.
- Same accuracy pass for strings, pickups, and other parts.
- Beta tester confirms every instrument plays and is represented correctly.
- Report where measured/published data is missing so the owner can source it.

Sequencing: A + B-specs + C-research start now. Implementation follows its spec;
audit + beta follow implementation. Coordinator check-in expands each wave.
