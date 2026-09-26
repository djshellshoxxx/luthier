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

## Expression/effects access — DO NOT duplicate; enforce ease-of-use (2026-09-26, owner)

The system the owner asked for already exists in the specs — do NOT build a
parallel one:
- Auto "appropriate effects" with one knob + style names = **Performance
  Assist / Auto Articulation** (spec/auto-articulation.md): `aa_amount`
  0-100% (the wet/dry-style knob), `aa_style` named by genre (Clean/Pop,
  Blues, Rock, Metal, Jazz, Country, Fingerstyle, Bass). It auto-applies
  slide on interval/octave jumps, legato, vibrato, palm mute, ornaments —
  scaled by amount, biased by style. Explicit input always overrides.
- Manual per-technique triggers exist via the Playing strip (gui-integration
  3.3), keyswitch, CC, and MPE (spec/controllers.md, per-technique specs).
- MIDI round-trip exists BOTH ways: spec/midi-export.md is "MIDI EXPORT AND
  IMPORT" — Luthier profile is lossless (every technique/event class
  round-trips), Generic profile imports as a PerformanceScore; File -> Import
  -> MIDI or drag-drop.

REQUIREMENT for GAPS-GUI + the beta/accuracy owners (why the owner "saw no
way in the GUI"): make this EASY and DISCOVERABLE, and confirm it fires
effects APPROPRIATELY. Acceptance:
1. Performance Assist Amount + Style is a first-class, obvious control in
   Easy mode (not buried); every automatable technique/effect param has a
   visible control (Combo test everyAutomatableParameterHasAVisibleControl).
2. Each noise/technique effect (slap, scrape, squeak, pick noise, buzz,
   slide) has a discoverable manual trigger AND, where musically valid,
   participates in Performance Assist; where auto is NOT musically valid for
   an effect, it stays manual-only (do not force auto).
3. Appropriateness is validated by the auto-articulation tests plus the
   accuracy audits (existing instruments/parts, and effects). Report any
   technique that fires at the wrong time as an accuracy finding.
No new spec or engine for this — verification + GUI wiring only.

## Single-coil hum — surface in Options; DO NOT duplicate (2026-09-26, owner)

Owner wants an Options switch for pickup/mains hum on electric + bass, level
adjustable, audibly dynamic as played. This ALREADY EXISTS as the noise-floor
model — do NOT build a new hum engine:
- Engine: PickupEngine mains hum = fundamental + 0.35×3rd + 0.15×5th, scaled
  by `noise_amp_buzz` × single-coil share; humbucker = exactly 0; volume knob
  and amp gain change it; region via `noise_mains_hz` (50/60). Spec:
  spec/noise-floor.md 2.1. Applies to bass single-coils too (J-bass hums,
  split-P cancels) — correct as-is.
REQUIREMENT (GUI + verify only; assign a Sonnet implementer via check-in):
1. Add an easy **Options -> AUDIO** switch "Single-coil hum" + a level control
   (bind to `noise_amp_buzz`; region toggle 50/60 Hz bound to `noise_mains_hz`),
   discoverable, with a one-line caption ("Realistic single-coil/mains hum;
   humbuckers cancel it"). Default OFF so factory presets are unchanged.
2. Works for electric AND bass guitars (already does via single-coil share).
3. Confirm noise-floor.md 2.x sources are actually built; if only the legacy
   `noise_amp_buzz` exists, the plain hum satisfies the owner's fallback
   ("just a background hum will do") — the extended sources are a realism bonus,
   not a blocker.
4. Tests: the Options switch toggles the hum (NF-style level check); humbucker
   still gives 0; 50 vs 60 Hz changes the fundamental.
No new spec/engine. Additive GUI note in spec/noise-floor.md marking the
Options surface.

## Finger squeak — make it audible/discoverable (2026-09-26, owner)

Owner cannot hear finger squeaks. Spec exists (spec/string-squeak.md) and is
implemented (squeakAmount ~0.25, squeakMinTravel 1.5 frets). Root cause of
"can't hear it": (1) it triggers ONLY on a legato position shift (finger not
lifted); a normal new pluck at a new position is NOT a trigger, so ordinary
plucked MIDI produces none; (2) honest level is 20-30 dB below the note.
DO NOT rewrite the model. Assign effects-audit + a Sonnet implementer:
1. Verify a legato/slide shift of >= squeak_min_travel actually produces
   audible squeak on wound strings, across string types (wound vs plain: plain
   strings squeak little/none — correct). Add a test that measures squeak
   present on a legato shift and ~0 on a plain string.
2. Make the squeak DISCOVERABLE and TURN-UP-ABLE: ensure squeakAmount (and
   min-travel) have a reachable GUI control (Character/Realism area per
   gui-integration.md), so a user who wants more can raise it well above the
   realistic default without editing automation.
3. Ensure Performance Assist legato/slide rules actually generate the
   sustained position shifts that trigger squeak, so enabling Assist yields
   audible finger noise during normal playing.
4. Keep honest magnitudes as the DEFAULT; the control lets the owner exaggerate.
No new spec/engine. If any of 1-3 is genuinely missing in code (not just quiet),
that is a bug to fix, not a tuning tweak.

## Windows + macOS PAUSED (2026-09-26, owner)

Hold ALL Windows and macOS work until the project is fully complete; build
cross-platform versions only at the very end.
- CI: ci-cadence.yml gate forced windows=false, macos=false (revert note in
  the file). Linux is the only CI platform for now.
- Helpers: do NOT spawn Windows/macOS build/test/packaging work. Task #8
  (macOS build) stays deferred. Cross-platform verification is the final step
  before release, after everything else is done, audited and tested.
- Linux Standalone + VST3 + CLAP remain the working targets.

## Credit-resilience: finish everything across usage resets (2026-09-26, owner)

The whole job (all workers, full beta test, full audit) must COMPLETE even if
we hit the weekly usage limit mid-flight. Design:
- The coordinator check-in is RECURRING (cron), so it keeps firing and
  auto-resumes after each usage-limit reset without manual re-arming.
- Every check-in: for EACH tracked worker call get_session. If a worker is
  failed / stalled / stopped at a usage limit / idle with work still left in
  its coverage or handoff, restart a FRESH session on the SAME model + branch,
  told to continue from its branch state and handoff note (never lose progress;
  branches hold committed work). Do this after each reset for any that stalled.
- The auditor and beta tester KEEP LOOPING until everything is audited and
  tested: every existing + new instrument, every effect/technique, every
  feature spec, GUI reachability, state save/restore, Standalone/VST3/CLAP
  (Linux only for now). Only stop when the coverage/audit docs show all items
  verified with no open findings.
- Do NOT declare the project done while any worker has remaining rows, any
  audit/beta item is unverified, or any open finding stands.

## External agent (ChatGPT/Codex) collaboration (2026-09-26, owner)

An external agent collaborates via the repo. See docs/helpers/EXTERNAL_AGENT_BRIEF.md.
Coordinator each check-in: `git fetch` also lists `codex/luthier-*` branches;
treat a green, up-to-date codex branch exactly like a helper branch — Linux
build + test, then merge into integration (coordinator is the ONLY merger).
External agent owns: feature implementations once specced (ui-scaling, tuner,
midi-learn, randomize+ab, tab-export, amp-cab-ir) + light instruments (tenor,
acoustic bass, extended-range bass). Claude owns hard DSP, merges, audits,
beta, editions. No overlap; same parameter-marker / CRLF / no-Win-Mac rules.
