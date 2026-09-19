# QA AND POLISH SPEC (SHIP GATE)

What it takes for Luthier to ship as a $200 commercial VST. Every item
below is a gate; a build with any unresolved item is not shippable.

Consumer-ready means: a working guitarist installs it, plays for an
hour, and finds nothing broken, ugly, confusing or unresponsive.

## 0. The five ship gates

A build ships only when all five are green:

1. **Zero crashes** across the full test matrix (section 3).
2. **Zero broken UI** (section 4): no clipped controls, no dead buttons,
   no unlocalized strings, no missing tooltips.
3. **Zero audio artefacts** on any factory preset (section 5): no clicks
   on switch, no denormals, no drift, no audible aliasing.
4. **Every advertised feature works** end-to-end (section 6).
5. **Performance targets met** (section 7).

## 1. The test matrix

### Hosts
Ableton Live 12, Logic Pro (latest, macOS), Cubase 13, Studio One 7,
Reaper 7, FL Studio 21, Bitwig Studio 5, Pro Tools 2024 (VST3 deferred
if unsupported), Standalone (bundled).

### Platforms
Windows 10 21H2, Windows 11 24H2, macOS 13, macOS 14, macOS 15, macOS
on Apple Silicon and Intel, Ubuntu 22.04 LTS, Ubuntu 24.04 LTS.

### Sample rates
44.1, 48, 88.2, 96, 176.4, 192 kHz.

### Buffer sizes
32, 64, 128, 256, 512, 1024, 2048 samples.

### CPU classes
- Low: Intel N100, Apple M1 base, AMD 5000 mobile.
- Mid: Ryzen 5 5600X, Apple M2 Pro.
- High: Ryzen 9 7950X, Apple M3 Max.

## 2. Automated regression suite

Every commit runs the existing tests plus:

- **Golden render**: 36 factory presets played through a standard MIDI
  fixture, compared to a golden WAV per (sample rate, block size)
  combination. Any diff > -80 dBFS RMS null fails the build.
- **Parameter fuzz**: 100 000 random parameter states with random audio
  input, verify no NaN, no denormal escape, no output above 0 dBFS + 3 dB.
- **State fuzz**: 10 000 random preset load / snapshot recall / setlist
  step / mod route add / part swap operations, verify no crash, no
  stuck state.
- **Boot time**: instantiation and first-block-processed within 400 ms
  cold, 200 ms warm, on mid CPU class.
- **Memory**: 60-minute standalone session with all features exercised,
  peak RSS under 900 MB, no monotonic growth (delta over any 10-minute
  window under 8 MB).
- **Preset round trip**: every factory preset serialized, deserialized,
  compared. Any parameter drift > float ulp fails.
- **Guitar round trip**: every factory `.luthierguitar` file serialized,
  reloaded; rendered audio identical within -80 dBFS RMS null.
- **Migration round trip**: every pre-M49 preset resolves through the
  migration table to a live `.luthierguitar`; rendered audio within
  -60 dBFS RMS null of the pre-M49 golden.
- **pluginval strictness 10**: every platform, every host format.
  Zero warnings.

Regression suite runs in CI on every push. Nightly runs the full
matrix; a nightly failure blocks the ship gate.

## 3. Crash policy

Zero tolerance. Every crash on any matrix cell is a release blocker.

Explicit crash sources checked:
- Multi-instance: 32 instances in one host, all playing.
- Host format switching: VST3 -> AU -> VST3 mid-session (macOS).
- Sample rate change mid-play: 48 -> 96 -> 44.1 -> 48 within 2 s.
- Block size change mid-play: 128 -> 64 -> 512.
- Preset load mid-play under MIDI storm (1000 notes/s).
- Snapshot recall mid-play under MIDI storm.
- Guitar load mid-play under MIDI storm.
- Part swap in Workshop while playback runs at 22 CPU units.
- CPU stress: 95% host CPU while switching presets.
- MIDI Learn arm / disarm 100x in 10 s.
- Undo / redo across every feature area 1000x.
- Bus layout change mid-play.
- Slide Mode toggle 100x during playback.
- Advanced-range toggle 100x during playback.
- Corrupt preset file: every byte position flipped one at a time in a
  factory preset; verify refusal and stability.
- Corrupt `.luthierguitar` file: same.
- Missing files: delete every referenced IR / sample / part / guitar and
  load a preset that depends on it; verify graceful fallback with
  notification banner.

## 4. UI polish checklist

Every panel walked by a human tester and by the automated UI walker.

Every control:
- [ ] Tooltip in every locale.
- [ ] Label in every locale fits at 100% UI scale.
- [ ] Accessibility role and value description; physical parameters
      include the unit in the announcement.
- [ ] Responds to hover, click, drag, right-click, double-click as
      documented.
- [ ] Returns to default on double-click or right-click reset.
- [ ] Value entry on right-click; accepts pasted values.
- [ ] Value arc fills proportionally; portion beyond stock range paints
      in the warning colour (advanced-ranges.md).
- [ ] Mod arc when a mod route targets it.
- [ ] Focus via Tab; focus ring visible in every palette.
- [ ] Fires the right undo entry.
- [ ] Range-unlock items in right-click appear only in the right state.

Every panel:
- [ ] Header with title, status line, chevron; padlock icon on
      CHARACTER and WORKSHOP tabs where advanced ranges apply.
- [ ] Collapses and re-expands cleanly, remembers state per preset.
- [ ] Renders at 75, 100, 125, 150, 175, 200% UI scale without
      clipping.
- [ ] Renders in every palette without illegible text.
- [ ] Renders under reduced motion without missing feedback (noise-event
      strip becomes a static count; slide bar has no ease).
- [ ] Right-click empty-area menu.
- [ ] "?" that opens docs for that panel.

Every dialog / overlay:
- [ ] Escape closes.
- [ ] Focus lands on first interactive element on open.
- [ ] Screen reader announces the dialog on open.
- [ ] Cannot be lost behind the host.
- [ ] Save-on-change (no OK / Apply anywhere in Options).

Empty states:
- [ ] Every panel that can be empty has a hint per
      gui-integration.md 14.

Workshop-specific:
- [ ] Illustration hit-testing selects the intended part.
- [ ] Drag bounds respected (pickup cannot leave body route without
      advanced ranges).
- [ ] Bench A / B strip loads a stored slot within 30 ms crossfade.
- [ ] Alt-hover audition leaves committed `GuitarSpec` byte-identical.
- [ ] Spectrum-delta pane numbers match offline render within 0.2 dB.
- [ ] "Save As Guitar" writes a valid `.luthierguitar` and reloads
      identical.

Slide Mode:
- [ ] Header glyph, fretboard overlay, Workshop slide part all move
      together.
- [ ] Squeaks suppressed on strings the slide contacts.
- [ ] Pitch tracker announces continuous position.

Advanced-range marking:
- [ ] Warning-colour arc appears when a knob passes stock max.
- [ ] "*" suffix appears in the readout.
- [ ] Padlock icon secondary accent when unlocked, muted otherwise.
- [ ] First-time unlock triggers the one-time explainer; subsequent
      unlocks do not.

## 5. Audio polish checklist

Every factory preset:
- [ ] Renders the fixture MIDI without clicks, denormals, or output
      above 0 dBFS + 3.
- [ ] Loads with no artefact.
- [ ] Sounds like its name says (subjective; requires signoff from
      designated audio lead).
- [ ] DC null: silent input produces silent output within -100 dBFS RMS.
- [ ] Mono compatibility: L+R sum vs L or R alone shows no comb
      cancellations beyond 3 dB in the presence range.
- [ ] Bypass null: plugin bypassed produces bit-identical output to no
      plugin at all.

Every effect pedal:
- [ ] Enable / disable produces no click.
- [ ] All parameters at extremes produce bounded output.
- [ ] Zero-mix / zero-amount is a bypass within -80 dBFS null.

Every amp:
- [ ] Cold start has no transient.
- [ ] Gain sweep 0 -> 100 is monotonic in loudness at 1 kHz sine input.
- [ ] Tone stack at neutral is flat within 1 dB in its designed
      passband.

Realism modules:
- [ ] Squeak on / off: with amount 0%, output bit-identical to squeak
      module disabled.
- [ ] Squeak determinism: fixed seed, same slide event, byte-identical
      buffer across 1000 runs.
- [ ] Pick click / chirp: for a single note at velocity 100 through
      every pick material, click energy matches material profile
      within 0.5 dB.
- [ ] Pick scrape on Rake events: audible above -40 dBFS on wound
      strings, absent on plain.
- [ ] Fret buzz threshold: for a fixture setup at threshold - 1 dB, no
      buzz; at threshold + 3 dB, buzz present.
- [ ] Slide continuous pitch: tuning engine's per-string frequency
      follows the slide bar within 2 cents.
- [ ] Slide vibrato: measurable at slide's vibrato depth.
- [ ] Clank: bar-drop event produces the clank profile in the noise
      output.
- [ ] GuitarCircuit at volume 10, tone 10, active pickup off: matches
      the pre-M44 CableSim cutoff table within 0.5 dB across the guitar
      band.
- [ ] GuitarCircuit at volume 5 with amp at high gain: measurable
      cleanup (per volume-knob-interaction.md 13).
- [ ] Treble bleed circuits: at volume 5, 4 kHz vs 500 Hz level within
      0.5 dB (Kinman) or ~3 dB (None).
- [ ] Aux 8 noise bus: with all noise amounts 0, bit-silent; with
      squeak 100% on a slide fixture, contains the squeak, main minus
      Aux 8 (re-applied) nulls within -60 dBFS.
- [ ] Strum crossing velocity: 6 ms chop has 25-35% higher click energy
      than 25 ms strum at equal MIDI velocity.
- [ ] Rasgueado: four sub-strums 14 ms apart within humanize tolerance.
- [ ] Bass slap: string contact point on the fret produces a two-stage
      transient (attack + fret return) audible within 5 ms.
- [ ] Bass pop: pull-off snap is louder than the follow-through by the
      documented amount.
- [ ] Bass ghost: below the sustain threshold, no pitched content
      above -30 dBFS.
- [ ] Bass double thump: two thumb hits with the second on the
      documented offset.
- [ ] Bass two-finger alternation: alternating events show the
      per-finger amplitude / timing signature.

## 6. Feature completeness checklist

Every entry in gui-integration.md 19:
- [ ] Present in the UI at its documented location.
- [ ] Wired to the backend it references.
- [ ] Persists in preset / snapshot / `.luthierguitar` as documented.
- [ ] Automatable from the host (if applicable).
- [ ] Reachable from a keyboard shortcut (if documented).
- [ ] Has automated tests per its spec's Tests section.

Every user-facing document referenced:
- [ ] Exists and matches built behaviour.
- [ ] Screenshots current (no more than 30 days behind the build).
- [ ] Translated into every ship locale.

MIDI export completeness:
- [ ] Luthier profile: every event class (squeak, pick, buzz, slide,
      workshop, strum, character) round-trips through export and
      re-import with byte-identical rendered audio within -60 dBFS RMS
      null.
- [ ] Generic profile: notes and CCs export as documented; realism
      events export as text meta events.
- [ ] Drag-out export: last take drops as a valid MIDI file to the
      user's Downloads folder.
- [ ] Live MIDI-out alignment: rhythm engine event timestamps match
      internal ticks within 1 sample.

## 7. Performance targets

Baseline: mid CPU class, 48 kHz, 128 sample block, one instance, 25% of
factory presets active in rotation.

- Idle CPU (silence, plugin loaded): <= 1.5% single core.
- Steady state (four-voice polyphony, medium preset): <= 8%.
- Heavy preset (Modern Metal Chug, six-voice, all effects on): <= 22%.
- Realism heavy (fingerstyle acoustic with squeak 100%, buzz on, pick
  chirp on): <= 12%.
- Slide preset (bottleneck electric with feedback amount 30%): <= 18%.
- Bass heavy (funk slap with ghosts, both hands active): <= 15%.
- Convolution stages partitioned, latency <= 128 samples added.
- Preset load: <= 250 ms message-thread time; audio uninterrupted.
- Snapshot recall: <= 30 ms crossfade; audio uninterrupted.
- Guitar load: <= 300 ms message-thread time; audio uninterrupted.
- Part swap: <= 50 ms message-thread time; audio uninterrupted.
- Spectrum-delta worker: <= 40 ms per delta.

Memory:
- Baseline RSS: <= 350 MB.
- With all IR variants loaded: <= 700 MB.
- With full parts library loaded: <= 800 MB.
- Absolute cap: 900 MB peak.

Latency reporting:
- Main out latency accurate within 1 sample.
- Aux DI latency reported separately per routing-io.md 7.

Regression more than 5% between releases: release gated pending
investigation.

## 8. Bug bash procedure

Before every release candidate:

1. Full team internal build lockdown for 48 hours.
2. External bug bash: at least eight guitarists, chosen to cover the
   product's range:
   - Two intermediate acoustic strummers.
   - Two advanced electric players.
   - Two professional session players.
   - One slide player (bottleneck or lap steel).
   - One bassist (fingerstyle and slap).
3. Each runs a 90-minute scripted session covering: install, first-run,
   load a factory preset, edit it, save as user preset, use snapshots,
   use the Workshop to swap at least three parts, use Slide Mode if
   applicable, use practice tools, use tone match, export audio, export
   MIDI (both profiles), exercise the mod matrix, try an advanced-range
   parameter.
4. Every issue logged with steps to reproduce, expected, actual, plus
   screenshot / recording.
5. Every P0 / P1 must be fixed or downgraded with justification before
   release.
6. P2 defer to next release with owner assigned.
7. P3 to backlog.

## 9. Installer polish

Windows:
- Signed installer (EV code-signing certificate).
- Standard / custom install with component picker.
- Uninstaller removes every installed file; preserves user data by
  default with explicit purge checkbox.
- Silent install flag for enterprise.

macOS:
- Notarized .pkg.
- Installs VST3 and AU to standard paths.
- Uninstall script bundled.

Linux:
- .tar.gz and .deb.
- Correct .desktop file, standalone icon.

Every installer:
- Displays licence, requires accept.
- Shows disk space required / available.
- Refuses to overwrite a newer version without confirmation.
- Reports success / failure clearly.

## 10. Documentation gate

Before ship:
- User manual complete and translated (including a Workshop chapter and
  a Realism chapter that covers squeak, buzz, pick, slide, advanced
  ranges).
- All docs in `docs/` up to date.
- Video walkthrough recorded (3 minutes, first-run overview).
- Second video (2 minutes, the Workshop bench).
- Support email monitored; auto-response includes troubleshooting link.
- Community forum or Discord seeded with staff answers to expected
  first-week questions.

## 11. Legal and licensing

- Every third-party library in `THIRD_PARTY_LICENCES.txt`.
- Preset names avoid trademarks ("British Stack", "American Twin", not
  "Marshall" or "Fender"). Legal review pass.
- Part names in the Workshop follow the same rule: reference-style
  names ("Vintage PAF", "'57 Reissue Alnico 2") not marks.
- Every IR is generated or licensed with documentation.
- EULA finalised.
- Refund policy documented on the website.

## 12. The final human check

Before hitting release, one person plays through the plugin for 30
minutes with fresh ears and no agenda. If anything feels unfinished,
gate closes and the team reconvenes.

Not skippable. Automation misses the one thing that matters: does it
feel good to play.

For a Luthier build specifically, the check also includes: build one
guitar from scratch in the Workshop (from a template), save it,
close the plugin, reopen, load the guitar, verify everything is
where it should be.

## 13. Post-release monitoring

For 72 hours after release:
- Crash upload rate monitored hourly (with user opt-in).
- Support email SLA under 8 hours.
- Rollback plan documented and rehearsed: within 30 minutes, the update
  manifest reverts to the previous version.
- Hot-fix release process: if a P0 emerges, a patched build ships
  within 24 hours through an abbreviated gate 1-5 run.
