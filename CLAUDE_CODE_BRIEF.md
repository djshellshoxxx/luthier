# CLAUDE CODE BRIEF

Read this before touching any code. It tells you how to use the spec
set as a coherent whole rather than as a pile of documents to skim.

## Who you are and what you are building

You are implementing Luthier, a $200 commercial physically-modelled
guitar VST that also sketches tunes. The current state is in
`PROGRESS.md`. The end state is a plugin that a working guitarist
installs, plays for an hour, and finds nothing broken, ugly, confusing
or unresponsive; and also opens later to write a song.

## The problem you were brought in to fix

Two problems, layered.

1. The engine has features. The GUI does not surface them. A user
   opens the plugin and cannot find the modulation matrix, cannot find
   the tone match tools, cannot find the character panel, cannot find
   the practice drawer, cannot find the Workshop.
2. The plugin plays like a synth, not a guitar, until you add the
   realism specs (advanced-ranges, GuitarCircuit, pick / squeak / buzz /
   slide, the Workshop, strum dynamics, bass, MIDI export). Once those
   land, it needs a composition layer (`tune-builder.md`) so people
   can actually make music with it, not just play notes.

Your job is to close both gaps and bring every feature to $200 polish.

## The order to do this in

1. Read the whole spec set once, in this order:
   - `README.md`
   - `spec.md`
   - `engine.md`
   - `theme.md`
   - `include.md`
   - `PROGRESS.md`
   - `REVIEW.md`
   - Each of the 11 phase-1 extension specs in the INDEX order.
   - Each of the 12 phase-2 realism specs, in order. `advanced-ranges.md`
     must come first because every physical parameter depends on its
     `PhysicalRange` wrapper.
   - `tune-builder.md` (phase 3).
   - `ambiguity-resolutions.md`
   - `gui-integration.md` (central; read it twice).
   - `ui-wiring.md`
   - `onboarding.md`
   - `performance-budget.md`
   - `qa-polish.md`
   - `installer.md`
   - The nine deep-integration specs (phase 5 in INDEX):
     - `file-formats.md`
     - `factory-content.md`
     - `error-recovery.md`
     - `state-model.md`
     - `gui-engine-dataflow.md`
     - `guitar-illustration.md`
     - `input-routing.md`
     - `host-integration.md`
     - `action-and-undo.md`

2. Audit the current build against `gui-integration.md` section 19
   (the feature-to-location index). For every row where the location
   does not currently exist in the UI, note it. Do not fix anything
   yet. The audit produces a gap list.

3. For each gap in the list, build the missing UI panel following
   `ui-wiring.md` conventions. Every panel must:
   - Use the attachment pattern in ui-wiring section 2.
   - Match the visual identity in `theme.md`.
   - Have accessibility per `accessibility.md`.
   - Have a tooltip and a docs entry per `gui-integration.md`.
   - For physical parameters, use the `PhysicalRange` wrapper so
     advanced-range marking works without per-panel code.

4. Resolve every ambiguity per `ambiguity-resolutions.md`. Do not
   improvise; resolutions are chosen.

5. Wire the realism modules: NoiseEngine pool, GuitarCircuit (which
   replaces CableSim), SlideEngine, Workshop parts swap and shadow
   `GuitarSpec` audition.

6. Wire the Tune Builder and MIDI export. Tune Builder is a MIDI
   writer only, never an audio-path module. MIDI export must round-trip
   in Luthier profile to within -60 dBFS RMS null.

7. Run the polish pass per `qa-polish.md` sections 4 and 5.

8. Run the performance pass per `performance-budget.md`. Measure per
   module. Fix regressions.

9. Run the onboarding pass per `onboarding.md`. Fresh install produces
   documented state. Tour, including Workshop / Slide / Tune steps,
   works end to end.

10. Run the installer pass per `installer.md`. Build and test on every
    platform in the matrix.

11. Run the bug bash per `qa-polish.md` section 8.

12. Do the final human check per `qa-polish.md` section 12.

Only after all twelve do you consider the build shippable.

## Rules of engagement

**Do not skip specs.** Every file exists because a specific gap was
found. Skipping one means shipping that gap.

**Do not improvise where a spec is explicit.** If ambiguity-resolutions
says the doubler default is -8 cents, it is -8. If tune-builder says
the Tune Builder never touches the audio path, it never does.

**Do not add features not in the spec.** If you think a feature is
missing, add it to a proposal file first and get review; do not add it
to the code.

**Do not drop features that are in the spec.** If a feature seems
redundant or hard, escalate. Do not silently omit.

**Follow the layout in gui-integration.md exactly.** Column 4 tab order
is fixed: WORKSHOP | MOD | RHYTHM | TUNE | LIVE | ROUTING | TONE MATCH
| CHARACTER | PRACTICE | NOTATION | MIDI OUT | CONTROLLERS | HELP.

**Follow the threading contract in ui-wiring.md exactly.** No
shortcuts. Audio thread does not touch UI. UI thread does not touch
audio-owned state. Communication is through documented channels only.

**Every parameter is in the APVTS**, wrapped in `PhysicalRange` if it
is a physical parameter. If you are tempted to add a side channel, you
are wrong; find another way.

**Structural state (mod matrix, patterns, IRs, `GuitarSpec`, parts,
`ranges` block, `Tune`) never flows through parameters.** It flows
through the command / result queue with atomic pointer swaps.

**Every user-visible string is in the locale catalog.** No hard-coded
strings, even for English.

**Every feature has tests.** Tests are in the file that specs the
feature; add them to `LuthierTests`. A feature without tests is a
feature that will break.

**PROGRESS.md gets updated after every milestone.** Include what
worked, what broke, and what surfaced during testing.

## What "consumer-ready" means

Not: "the feature works when I test it once with the ideal input".
Not: "the code compiles and passes the existing tests".
Not: "it looks good in a screenshot".

Yes: a guitarist who bought this at $200 sits down for an hour, tries
things they were curious about, sketches a tune, exports it, sends the
audio and the MIDI through their DAW to a friend, and comes away wanting
to buy it again.

Every choice you make is scored against that bar.

## Handling conflicts between docs

If two specs disagree:

1. `qa-polish.md` overrides on ship-readiness questions.
2. `gui-integration.md` overrides on UI location and access.
3. `ui-wiring.md` overrides on how UI attaches to backend.
4. `ambiguity-resolutions.md` overrides on the seven items it resolves.
5. `performance-budget.md` overrides on CPU and memory.
6. `part-acoustics.md` overrides on part-to-engine mappings.
7. `advanced-ranges.md` overrides on parameter range semantics.
8. `guitar-workshop.md` overrides on part data model (supersedes any
   hard-coded guitars in the older `engine.md`).
9. `volume-knob-interaction.md` overrides on the pickup-to-amp path
   (`CableSim` from the older `engine.md` is removed, not deprecated).
10. `engine.md` overrides on DSP ground rules.
11. `file-formats.md` overrides on file schema, migration and atomicity.
12. `error-recovery.md` overrides on failure response.
13. `state-model.md` overrides on state layers and concurrent-activity intersections.
14. `gui-engine-dataflow.md` overrides on live UI drain rates and staleness.
15. `guitar-illustration.md` overrides on the visible guitar (parts, colours, string materials, family switching).
16. `input-routing.md` overrides on input consumer order and veto rules.
17. `host-integration.md` overrides on host contract and per-host quirks.
18. `action-and-undo.md` overrides on undo grouping and state boundaries.
19. Otherwise, later spec numbers in `INDEX.md` override earlier ones.

If a genuine conflict cannot be resolved by this hierarchy, stop
coding and produce a written question with proposed answers.

## Test discipline

Every claim in code has to be provable. Every claim a spec makes about
behaviour has to be tested. The existing tests are a floor, not a
ceiling.

Every extension, realism, composition and integration spec has a
"Tests" section. Every one of those tests must land in `LuthierTests`
and pass. Adding a feature without tests is leaving a bomb for the
ship gate.

## When you are done

You are done when:
- Every row of `gui-integration.md` section 19 is present in the UI.
- Every test in every spec passes.
- Every gate in `qa-polish.md` section 0 is green.
- The final human check in `qa-polish.md` section 12 has been
  performed and passed.
- `PROGRESS.md` has a "READY TO SHIP" marker at the top and a
  signed-off release checklist.

Anything short of this is a partial build. Do not stop early. Do not
consider a mostly-complete build shippable.

## One more thing

The user who is paying you to build this is also going to use it. If
they open the plugin after your work and cannot find a feature they
know you built, you have failed. If they open it, sketch a chord
progression, add a melody in one click, hit play and hear a real
guitar, you have succeeded.

Build for that user.
