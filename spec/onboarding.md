# ONBOARDING SPEC

First-run and returning-user experience. What the user sees the first
time they open Luthier and how the plugin behaves for the first hour.

## 0. Ground rules

1. First run produces a great sound in under 30 seconds without any
   configuration.
2. No modal dialog on first run.
3. The tour is optional. Declined tours never reappear.
4. Every advertised feature is discoverable within one week of normal
   use without ever reading a manual.
5. First run does no network activity. Update checks, telemetry,
   license activation are all opt-in via Options.

## 1. Fresh install state

Preset loaded: `Factory / Rock / Modern Overdrive`.
Guitar: `Les Paul Standard` (from the shipped `.luthierguitar` library).
Mode: Easy.
Live Mode: off.
Practice drawer: collapsed.
Workshop: closed.
Slide Mode: off.
Ranges: stock (per-preset advanced-ranges flag off).

Realism defaults tuned so the user immediately hears the plugin is not
a sampler: squeak amount 25%, pick click at material default, fret
buzz threshold at "player-friendly action" from fret-buzz.md's setup
styles. Nothing exaggerated; every hint is small and real.

## 2. Welcome banner

At the first launch of every version, a non-modal banner appears below
the header:

> Welcome to Luthier. Take the 2-minute tour? [Yes] [Maybe later] [Don't ask again]

Behaviour:
- `Yes` starts the tour.
- `Maybe later` dismisses; banner returns on the next launch, up to 3
  times total, then stops.
- `Don't ask again` dismisses permanently; user can restart the tour
  from Help -> Take the tour.

## 3. The tour

Twelve steps, one popover each, arrow pointing to the referenced
control. Each popover has Next / Back / Skip. Escape ends the tour.

1. **Play something**: "Hit a key on your MIDI keyboard. The output LED
   should light."
2. **Try a preset**: highlights the preset name in the header. "Click
   to browse. Try 'Acoustic Fingerstyle'."
3. **Switch to Advanced**: highlights the mode toggle. "Show every knob.
   You'll be here often."
4. **Meet your guitar**: highlights the GUITAR panel. "Change the
   instrument, tuning, and character here."
5. **The rig**: highlights the amp column. "Pedals, amp, cabinet, room."
6. **The workspace**: highlights Column 4 tabs. "Everything else lives
   here: modulation, rhythm, tunes, tone match, practice, and more."
7. **The Workshop**: highlights the wrench in the header. "Every part
   of the guitar is a real part. Swap pickups, change strings, drop a
   different bridge. Try it. Escape to close."
8. **Sketch a tune**: highlights the TUNE tab. "Type a chord
   progression, hit play. Add a melody in one click if you want to."
9. **Snapshots**: highlights the snapshot strip. "Save a moment, recall
   with one press."
10. **Practice**: highlights the practice drawer chevron. "Metronome,
    looper, backing tracks, ear training. Slide it up when you need it."
11. **Slide Mode**: highlights the slide glyph. "Turn this on for
    bottleneck, lap steel or dobro. The fretboard becomes continuous."
12. **Options**: highlights the gear. "Audio, MIDI, appearance,
    accessibility, updates. Nothing here changes how it sounds."

At the end: "You're ready. Have fun."

## 4. Post-tour discoverability

For the first week (7 launches or 7 calendar days, whichever first):
- Every panel with a `?` icon shows a subtle pulse on first sight.
- Every unused Column 4 tab shows a soft accent dot until first opened.
- The randomize dice shows a tooltip on first Randomize hover:
  "Fresh sound in one click."
- Workshop wrench pulses on first sight.
- TUNE tab pulses on first sight.
- Slide glyph pulses on first sight.

After the first week, discoverability hints go quiet.

## 5. First-run defaults for common surprises

- Sidechain input: off.
- Sidechain-to-amp: off.
- Session recorder: off.
- Update check: off.
- Telemetry: off.
- Crash reporting: off.
- Beta channel: off.
- Reduced motion: follows OS preference on first launch.
- Palette: default; if OS is in high-contrast mode, switch to high
  contrast on first launch.
- UI scale: 100%; if OS DPI scale is over 150%, snap to 125%.
- Locale: matches OS locale if in ship set; otherwise en.
- Range mode: stock.
- Slide Mode: off.
- Live Mode: off.

## 6. Sample content

Every install includes:
- **36 factory presets** across Electric, Acoustic, Classical, Bass,
  Utility.
- **12 factory guitars** as `.luthierguitar` files (Les Paul, Strat,
  Tele, PRS, ES-335, D-28, OM-21, classical, resonator, 12-string,
  P-bass, J-bass style references, none with trademarked names).
- **60+ factory parts** across every slot the Workshop supports.
- **12 tune templates** (tune-builder.md 10).
- **6 example tunes** ready to open (`.luthiertune`), each written to
  show off a different capability: a fingerstyle etude, a jazz standard,
  a folk sketch, a metal riff, a slide blues, a funk-slap bass line.
- **12 example MIDI clips** in `Resources/Examples/`, one per genre kit.
- **6 practice backing tracks**, royalty-free.
- **10 example setlists**.
- **The tour itself** as a reusable interactive walkthrough.

None of the samples are copyrighted third-party material.

## 7. Advanced-range first encounter

The first time a user turns any control past its stock max (a control
that has an advanced range), a one-time popover appears:

> This control has a stock range that matches real guitars and an
> advanced range for exaggerated effects. You're leaving the stock
> range. Values marked with * play back the same; presets with any
> advanced values show a padlock icon. Options -> Ranges lets you set
> the default. [OK, got it]

Dismissible. Never shown again. Can be re-triggered by "Restore
first-run experience" in Options -> Diagnostics.

## 8. Three-minute tune first encounter

If the user opens the TUNE tab in their first session, a one-time
inline hint appears at the top of the tab:

> Type a chord progression like "Am F C G", hit play, and you have a
> tune. Add a melody in one click. When you're ready, Ctrl+E exports.

Dismissible.

## 9. Workshop first encounter

If the user opens the Workshop in their first session, a one-time
inline hint appears in the bench header:

> This is every part of your guitar. Click any part to swap it. Try
> Alt-hover on a card to hear it before committing. Escape closes.

Dismissible.

## 10. Three paths from install to satisfaction

- **Path A (30 seconds)**: install, play, done. Default preset is
  enough.
- **Path B (2 minutes)**: take the tour, hit Randomize a few times.
- **Path C (5 minutes)**: take the tour, load one of the example tunes,
  hit play, swap the guitar in the Workshop while it loops.

Each path is documented in the manual and video walkthroughs.

## 11. Returning-user experience

- Restore last preset if closed clean.
- Restore last window size, mode, tab, practice drawer state, Slide
  Mode state.
- Restore last opened tune (if any).
- Skip welcome banner unless "Maybe later" left it armed.
- Show update banner if a check has been performed and an update is
  available.

## 12. Reset to first-run

Options -> Diagnostics -> "Restore first-run experience". Confirms with
a modal, clears user-global settings and next launch behaves as if
freshly installed. Preset library, guitar library, tune library and
user parts are preserved.

## 13. Version upgrade

- Welcome banner returns once with "Version X.Y.Z installed. What's
  new?".
- Any new feature added shows the "NEW" dot on its entry point.
- Changelog one click away.
- Preset / guitar / tune formats forward-compatible; if a migration is
  required, the plugin migrates on load and moves the old file to a
  dated Backup folder.

## 14. Tests

- Fresh install produces expected default state.
- Tour walks through all 12 steps without misalignment at every UI
  scale.
- Skip tour keeps the plugin in a playable state.
- OS preference detection: run under high-contrast OS mode, verify
  palette switch; under DPI 200, verify scale snap.
- Version upgrade: install version A, use, upgrade to B, verify banner,
  user data intact.
- "Restore first-run" resets user-global settings but keeps user
  library.
- First-encounter popovers each fire exactly once until reset.
