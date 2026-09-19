# ONBOARDING SPEC

First-run and returning-user experience. What the user sees the first
time they open Luthier and how the plugin behaves for the first hour.

## 0. Ground rules

1. First run must produce a great sound in under 30 seconds without any
   configuration.
2. No modal dialog on first run. Nothing that must be dismissed before
   the plugin can be played.
3. The tour is optional. Declined tours never reappear.
4. Every advertised feature is discoverable within one week of normal
   use without ever reading a manual.
5. First run does no network activity. Update checks, telemetry, license
   activation are all opt-in via Options.

## 1. Fresh install state

Preset loaded: `Factory / Rock / Modern Overdrive`.
Guitar: `Les Paul Standard`.
Mode: `Easy`.
Live Mode: off.
Practice drawer: collapsed.

Rationale: this preset is loud, immediately musical, and shows off the
engine's character on the most-recognised guitar.

## 2. Welcome banner

At the first launch of every version, a non-modal banner appears below
the header:

> Welcome to Luthier. Take the 90-second tour? [Yes] [Maybe later] [Don't ask again]

Behaviour:
- `Yes` starts the tour.
- `Maybe later` dismisses; banner returns on the next launch, up to 3
  times total, then stops.
- `Don't ask again` dismisses permanently; user can restart the tour
  from Help -> Take the tour.

## 3. The 90-second tour

Nine steps, one popover each, arrow pointing to the referenced control.
Each popover has a Next, Back, Skip. Escape ends the tour.

1. **Play something**: "Hit a key on your MIDI keyboard. The output LED
   should light."
2. **Try a preset**: highlights the preset name in the header. "Click to
   browse. Try 'Acoustic Fingerstyle'."
3. **Switch to Advanced**: highlights the mode toggle. "Show every knob.
   You'll be here often."
4. **Meet your guitar**: highlights the GUITAR panel. "Change the
   instrument, tuning, and character here."
5. **The rig**: highlights the amp column. "Pedals, amp, cabinet, room.
   Drag pedals to reorder."
6. **The workspace**: highlights Column 4 tabs. "Everything else lives
   here. Rhythm engine, modulation, practice tools, and more."
7. **Snapshots**: highlights the snapshot strip. "Save a moment. Recall
   with one press. Great for live use."
8. **Practice**: highlights the practice drawer chevron. "Metronome,
   looper, backing tracks, ear training. Slide it up when you need it."
9. **Options**: highlights the gear. "MIDI, audio, appearance,
   accessibility, updates. Nothing here changes how it sounds."

At the end: "You're ready. Have fun."

## 4. Post-tour discoverability

For the first week (7 launches or 7 calendar days, whichever first):
- Every panel with a "?" icon shows a subtle pulse on first sight to
  advertise the docs shortcut.
- Every unused Column 4 tab shows a soft accent dot until first opened.
- The randomize dice shows a tooltip on first Randomize hover:
  "Fresh sound in one click."

After the first week, all discoverability hints go quiet.

## 5. First-run defaults for common surprises

- **Sidechain input**: off; enabling requires an explicit choice per
  routing-io.md.
- **Sidechain-to-amp**: off.
- **Session recorder**: off; user must enable in Options -> Diagnostics.
- **Update check**: off; user must enable in Options -> Updates.
- **Telemetry**: off; opt-in per category in Options -> Privacy.
- **Crash reporting**: off; opt-in.
- **Beta channel**: off.
- **Reduced motion**: follows OS preference on first launch.
- **Palette**: default; if OS is in high-contrast mode, switches to high
  contrast automatically on first launch.
- **UI scale**: 100%; if OS DPI scale is over 150%, snap to 125% on
  first launch.
- **Locale**: matches OS locale if in ship set; otherwise en.

## 6. Sample content

Every install includes:
- **36 factory presets** across Electric, Acoustic, Classical, Bass,
  Utility.
- **12 example MIDI clips** in `Resources/Examples/`, one per genre kit,
  playable via the standalone drag-and-drop.
- **6 practice backing tracks** in the same folder, royalty-free.
- **10 example setlists** demonstrating live use.
- **The tour itself** as a reusable interactive walkthrough.

None of the samples are copyrighted third-party material.

## 7. New-user shortcuts to greatness

Three paths from install to satisfaction, each achievable without
reading a manual:

**Path A (30 seconds)**: install, play, done. The default preset is
enough.

**Path B (2 minutes)**: take the tour, then hit Randomize a few times
until something clicks.

**Path C (5 minutes)**: take the tour, load an example MIDI clip in
Standalone, hit play, then swap the preset with prev/next until a good
match.

Each path is documented in the manual and the video walkthrough.

## 8. Returning-user experience

After first-launch flow, subsequent launches:
- Restore the last preset if the plugin was closed clean.
- Restore the last window size, mode, tab, and practice drawer state.
- Skip the welcome banner unless "Maybe later" left it armed.
- Show update banner if a check has been performed and an update is
  available.

## 9. Reset to first-run

Options -> Diagnostics -> "Restore first-run experience". Confirms with
a modal, then clears user-global settings and next launch behaves as if
freshly installed. Preset library and user IRs are preserved.

## 10. Version upgrade experience

On first launch of a new version:
- Welcome banner returns once ("Version X.Y.Z installed. What's new?").
- Any new feature added in that version shows the "NEW" dot per
  gui-integration.md section 20.
- Changelog is one click away from the banner.
- No migration prompt required; preset format is forward-compatible.

If a preset format change is unavoidable, the plugin migrates on load,
writes the new file, and moves the old file to
`~/Documents/Luthier/Presets/Backup/<yyyy-mm-dd>/`. User sees a subtle
info banner.

## 11. Tests

- Fresh install produces expected default state.
- Tour walks through all 9 steps without misalignment at every UI scale.
- Skip tour keeps the plugin in a playable state.
- OS-preference detection: run under high-contrast OS mode, verify
  palette switch; under DPI 200, verify scale snap.
- Version upgrade: install version A, use, upgrade to B, verify banner,
  verify user data intact.
- "Restore first-run" resets user-global settings but keeps user preset
  library.
