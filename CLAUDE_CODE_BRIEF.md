# CLAUDE CODE BRIEF

Read this before touching any code. It tells you how to use the spec set
as a coherent whole rather than as a pile of documents to skim.

## Who you are and what you are building

You are implementing Luthier, a $200 commercial physically-modelled guitar
VST. The current state is in `PROGRESS.md`. The end state is a plugin
that a working guitarist installs, plays for an hour, and finds nothing
broken, ugly, confusing or unresponsive.

## The problem you were brought in to fix

The engine has features. The GUI does not surface them. A user opens the
plugin and cannot find the modulation matrix, cannot find the tone match
tools, cannot find the character panel, cannot find the practice
drawer. This makes the whole build feel unfinished.

Your primary job is to close that gap. Your secondary job is to bring
every feature to the level of polish a $200 product demands.

## The order to do this in

1. Read the whole spec set once, in this order:
   - `README.md`
   - `spec.md`
   - `engine.md`
   - `theme.md`
   - `include.md`
   - `PROGRESS.md`
   - `REVIEW.md`
   - Each of the 11 phase-1 extension specs listed in `INDEX.md`.
   - `ambiguity-resolutions.md`
   - `gui-integration.md` (this is the central document; read it twice).
   - `ui-wiring.md`
   - `onboarding.md`
   - `performance-budget.md`
   - `qa-polish.md`
   - `installer.md`

2. Audit the current build against `gui-integration.md` section 18 (the
   feature-to-location index). For every row where the location does not
   currently exist in the UI, note it. Do not fix anything yet. The
   audit produces a gap list.

3. For each gap in the list, build the missing UI panel following
   `ui-wiring.md` conventions. Every panel must:
   - Use the attachment pattern in ui-wiring section 2.
   - Match the visual identity in `theme.md`.
   - Have accessibility per `accessibility.md`.
   - Have a tooltip and a docs entry per `gui-integration.md`.

4. Resolve every ambiguity per `ambiguity-resolutions.md`. Do not
   improvise; the resolutions are chosen.

5. Run the polish pass per `qa-polish.md` sections 4 and 5. Every panel
   walked, every control checked, every audio artefact hunted.

6. Run the performance pass per `performance-budget.md`. Measure per
   module. Fix regressions.

7. Run the onboarding pass per `onboarding.md`. Fresh install produces
   the documented state. Tour works end to end.

8. Run the installer pass per `installer.md`. Build and test installers
   on every platform in the matrix.

9. Run the bug bash per `qa-polish.md` section 8.

10. Do the final human check per `qa-polish.md` section 12.

Only after all ten do you consider the build shippable.

## Rules of engagement

**Do not skip specs.** Every file in the set exists because a specific
gap was found. Skipping one means shipping that gap.

**Do not improvise where a spec is explicit.** If ambiguity-resolutions
says the doubler default is -8 cents, it is -8, not -7 or "close to that".

**Do not add features not in the spec.** If you think a feature is
missing, add it to a proposal file first and get review, do not add it to
the code.

**Do not drop features that are in the spec.** If a feature seems
redundant or hard, escalate. Do not silently omit.

**Follow the layout in gui-integration.md exactly.** If Column 4 tabs
have a documented order, that is the order. If a knob is documented as
being in Easy Mode's rig strip, that is where it lives.

**Follow the threading contract in ui-wiring.md exactly.** No shortcuts.
The audio thread does not touch UI. The UI thread does not touch
audio-owned state. Communication is through the documented channels.

**Every parameter is in the APVTS.** If you are tempted to add a side
channel, you are wrong; find another way.

**Every user-visible string is in the locale catalog.** No hard-coded
strings, even for English.

**Every feature has tests.** Tests are in the file that specs the
feature; add them to `LuthierTests`. A feature without tests is a
feature that will break.

**PROGRESS.md gets updated after every milestone.** Include what worked,
what broke, and what surfaced during testing.

## What "consumer-ready" means

Not: "the feature works when I test it once with the ideal input".
Not: "the code compiles and passes the existing tests".
Not: "it looks good in a screenshot".

Yes: a guitarist who bought this at $200 sits down for an hour, tries
things they were curious about, sends the audio through their DAW to a
friend, and comes away wanting to buy it again if it were somehow
possible to.

Every choice you make is scored against that bar.

## Handling conflicts between docs

If two specs disagree:

1. `qa-polish.md` overrides on ship-readiness questions.
2. `gui-integration.md` overrides on UI location and access questions.
3. `ui-wiring.md` overrides on how UI attaches to backend.
4. `ambiguity-resolutions.md` overrides on the five items REVIEW.md
   flagged.
5. `performance-budget.md` overrides on CPU and memory questions.
6. `engine.md` overrides on DSP ground rules.
7. Otherwise, later spec numbers in `INDEX.md` override earlier ones.

If a genuine conflict cannot be resolved by this hierarchy, stop coding
and produce a written question with proposed answers.

## Test discipline

Every claim you make in code has to be provable. Every claim a spec makes
about behaviour has to be tested. The 185 tests in the current build are
a floor, not a ceiling.

Every extension spec has a "Tests" section. Every one of those tests must
land in `LuthierTests` and pass. Adding a feature without adding its
tests is not "moving on to the next thing", it is "leaving a bomb for
the ship gate".

## When you are done

You are done when:
- Every row of `gui-integration.md` section 18 is present in the UI.
- Every test in every spec passes.
- Every gate in `qa-polish.md` section 0 is green.
- The final human check in `qa-polish.md` section 12 has been performed
  and passed.
- PROGRESS.md has a "READY TO SHIP" marker at the top and a signed-off
  release checklist.

Anything short of this is a partial build. Do not stop early. Do not
consider a mostly-complete build shippable.

## One more thing

The user who is paying you to build this is also going to use it. If
they open the plugin after your work and cannot find a feature they know
you built, you have failed. If they open it and can play something
beautiful within a minute of loading, you have succeeded.

Build for that user.
