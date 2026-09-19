# QA AND POLISH SPEC (SHIP GATE)

What it takes for Luthier to ship as a $200 commercial VST. Every item
below is a gate; a build with any unresolved item is not shippable.

Consumer-ready means: a working guitarist installs it, plays for an hour,
and finds nothing broken, ugly, confusing or unresponsive.

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

Every ship gate runs across the full matrix:

### Hosts
- Ableton Live 12 (latest and one version back).
- Logic Pro (latest, macOS).
- Cubase 13 (latest).
- Studio One 7.
- Reaper 7 (latest).
- FL Studio 21.
- Bitwig Studio 5.
- Pro Tools 2024 (AAX not in scope for v1.0, VST3 in Pro Tools deferred).
- Standalone (bundled).

### Platforms
- Windows 10 21H2.
- Windows 11 24H2.
- macOS 13 Ventura.
- macOS 14 Sonoma.
- macOS 15 Sequoia.
- macOS on Apple Silicon and Intel.
- Ubuntu 22.04 LTS.
- Ubuntu 24.04 LTS.

### Sample rates
44.1, 48, 88.2, 96, 176.4, 192 kHz.

### Buffer sizes
32, 64, 128, 256, 512, 1024, 2048 samples.

### CPU classes
- Low: Intel N100, Apple M1 base, AMD 5000 series mobile.
- Mid: Ryzen 5 5600X, Apple M2 Pro.
- High: Ryzen 9 7950X, Apple M3 Max.

## 2. Automated regression suite

Every commit runs the existing 185+ tests plus:

- **Golden render**: 30 factory presets played through a standard MIDI
  fixture, compared to a golden WAV per (sample rate, block size)
  combination. Any diff > -80 dBFS RMS null fails the build.
- **Parameter fuzz**: 100 000 random parameter states with random audio
  input, verify no NaN, no denormal escape, no output above 0 dBFS + 3 dB
  (limiter guaranteed to catch anything under this).
- **State fuzz**: 10 000 random preset load / snapshot recall / setlist
  step / mod route add operations, verify no crash, no stuck state.
- **Boot time**: instantiation and first-block-processed within 400 ms
  cold, 200 ms warm, on the mid CPU class.
- **Memory**: 60-minute standalone session with all features exercised,
  peak RSS under 900 MB, no monotonic growth (delta over any 10-minute
  window under 8 MB).
- **Preset round trip**: every factory preset serialized, deserialized,
  compared. Any parameter drift > float ulp fails.
- **pluginval strictness 10**: on every platform, every host format
  (VST3 always, AU on macOS). Zero warnings.

Regression suite runs in CI on every push. Nightly builds run the full
matrix; a nightly failure blocks the ship gate until resolved.

## 3. Crash policy

Zero tolerance. Every crash of any kind on any of the matrix cells is a
release blocker.

Crash sources checked explicitly:
- Multi-instance: 32 instances in one host, all playing at once.
- Host format switching: VST3 -> AU -> VST3 mid-session (macOS).
- Sample rate change mid-play: 48 -> 96 -> 44.1 -> 48 within 2 seconds.
- Block size change mid-play: 128 -> 64 -> 512.
- Preset load mid-play under MIDI storm (1000 notes/s).
- Snapshot recall mid-play under MIDI storm.
- CPU stress: run at 95% host CPU while switching presets.
- MIDI Learn arm/disarm 100x in 10 seconds.
- Undo / redo across every feature area 1000x.
- Bus layout change mid-play.
- Corrupt preset file: every byte position flipped one at a time in a
  factory preset; verify plugin refuses to load and remains stable.
- Missing files: delete every referenced IR / sample and load a preset
  that depends on it; verify graceful fallback with notification banner.

## 4. UI polish checklist

Every panel is walked by a human tester and by the automated UI walker.

Every control:
- [ ] Has a tooltip in every locale.
- [ ] Has a label in every locale that fits at 100% UI scale.
- [ ] Has an accessibility role and value description.
- [ ] Responds to hover, click, drag, right-click, double-click as
      documented.
- [ ] Returns to default on double-click or right-click reset.
- [ ] Enters value entry on right-click and accepts pasted values.
- [ ] Shows the value arc filled proportionally (knob/slider).
- [ ] Shows a mod arc when a mod route targets it.
- [ ] Focuses via Tab; focus ring visible in every palette.
- [ ] Fires the right undo entry.

Every panel:
- [ ] Header with title, status line, chevron.
- [ ] Collapses and re-expands cleanly, remembers state per preset.
- [ ] Renders at every UI scale (75%, 100%, 125%, 150%, 175%, 200%)
      without clipping.
- [ ] Renders in every palette without illegible text.
- [ ] Renders under reduced motion without missing feedback.
- [ ] Has a right-click empty-area menu.
- [ ] Has a "?" that opens docs for that panel.

Every dialog / overlay:
- [ ] Escape closes.
- [ ] Focus lands on first interactive element on open.
- [ ] Screen reader announces the dialog on open.
- [ ] Cannot be lost behind the host (modal, positioned within window).
- [ ] Save-on-change (no OK/Apply button anywhere in Options).

Empty states:
- [ ] Every panel that can be empty has a hint per
      gui-integration.md section 13.

## 5. Audio polish checklist

Every factory preset:
- [ ] Renders the fixture MIDI without clicks, denormals, or output
      above 0 dBFS + 3.
- [ ] Loads with no artefact (crossfade covers preset load per
      ui-wiring.md section 5).
- [ ] Sounds like its name says (subjective; requires a signoff from a
      designated audio lead, not just automated checks).
- [ ] Passes DC null: silent input produces silent output within
      -100 dBFS RMS.
- [ ] Mono compatibility: L+R sum vs L or R alone shows no comb
      cancellations beyond 3 dB in the presence range.
- [ ] Bypass null: plugin bypassed produces bit-identical output to no
      plugin at all.

Every effect pedal:
- [ ] Enable/disable produces no click.
- [ ] All parameters exercised at extremes produce bounded output.
- [ ] Zero-mix / zero-amount is a bypass within -80 dBFS null.

Every amp:
- [ ] Cold start (first block after load) has no transient.
- [ ] Gain sweep 0 -> 100 is monotonic in loudness at 1 kHz sine input.
- [ ] Tone stack at neutral has flat magnitude within 1 dB in its
      designed passband.

## 6. Feature completeness checklist

Every entry in gui-integration.md section 18's feature-to-location index:
- [ ] Present in the UI at its documented location.
- [ ] Wired to the backend it references.
- [ ] Persists in preset / snapshot as documented.
- [ ] Automatable from the host (if applicable).
- [ ] Reachable from a keyboard shortcut (if documented).
- [ ] Has automated tests per its spec's Tests section.

Every user-facing document referenced in gui-integration.md:
- [ ] Exists and matches the built behaviour.
- [ ] Screenshots are current (no more than 30 days behind the build).
- [ ] Translated into every ship locale.

## 7. Performance targets

Baseline: mid CPU class, 48 kHz, 128 sample block, one instance, 25% of
factory presets active in rotation.

- Idle CPU (silence, plugin loaded): <= 1.5% of a single core.
- Steady-state CPU (four-voice polyphony, medium preset): <= 8% of a
  single core.
- Heavy preset (Modern Metal Chug, six-voice polyphony, all effects on):
  <= 22% of a single core.
- Convolution stages: partitioned, latency <= 128 samples added.
- Preset load: <= 250 ms message-thread time; audio uninterrupted.
- Snapshot recall: <= 30 ms crossfade; audio uninterrupted.

Memory:
- Baseline RSS: <= 350 MB.
- With all IR variants loaded: <= 700 MB.
- Absolute cap: 900 MB peak.

Latency reporting:
- Main out latency reported accurately within 1 sample.
- Aux DI latency reported separately per routing-io.md section 7.

If any performance target regresses more than 5% between releases, the
release is gated pending investigation.

## 8. Bug bash procedure

Before every release candidate:

1. Full team internal build lockdown for 48 hours.
2. Six-guitarist external bug bash: two intermediate, two advanced, two
   professional, none previously involved in the build.
3. Each guitarist runs a 90-minute scripted session covering: install,
   first-run, load a factory preset, edit it, save as user preset, use
   snapshots, use the practice tools, use tone match, export audio, use
   the notation export, exercise the mod matrix.
4. Every issue logged with steps to reproduce, expected, actual, and a
   screenshot or recording.
5. Every P0/P1 issue must be fixed or downgraded with justification
   before release.
6. P2 issues can defer to the next release with owner assigned.
7. P3 issues go to backlog.

## 9. Installer polish

Windows:
- Signed installer (Extended Validation certificate).
- Standard install / custom install with component picker.
- Uninstaller removes every file installed; preserves user data by
  default with an explicit checkbox to purge.
- Silent install flag for enterprise deployment.

macOS:
- Notarized .pkg.
- Installs VST3 and AU to standard paths.
- Uninstall script bundled in Applications/Luthier/.

Linux:
- .tar.gz and .deb both.
- Correct .desktop file, correct standalone icon.
- VST3 to `~/.vst3/`; standalone to `/usr/local/bin/` or user-writable
  path if unprivileged.

Every installer:
- Displays licence, requires acceptance.
- Shows disk space required and available.
- Refuses to overwrite a newer version without confirmation.
- Reports success or failure clearly at the end.

## 10. Documentation gate

Before ship:
- User manual complete and translated.
- All docs in `docs/` up to date.
- Video walkthrough recorded (3 minutes, first-run overview).
- Support email monitored; auto-response includes link to
  troubleshooting doc.
- Community forum or Discord seeded with staff answers to expected
  first-week questions.

## 11. Legal and licensing

- Every third-party library listed in `THIRD_PARTY_LICENCES.txt`.
- Every trademark used in preset names avoided (no "Marshall", use
  "British Stack"; no "Fender", use "American Twin"). Verified by a
  legal review pass.
- Every IR is either generated or licensed with documentation.
- EULA finalised.
- Refund policy documented on the website.

## 12. The final human check

Before hitting the release button, one person plays through the plugin
for 30 minutes with fresh ears and no agenda. If anything at all feels
unfinished, ship gate closes and the team reconvenes.

This step is not skippable. It exists because automation misses the one
thing that matters: does it feel good to play.

## 13. Post-release monitoring

For 72 hours after release:
- Crash upload rate monitored hourly (with user opt-in per
  updates-telemetry.md).
- Support email response SLA under 8 hours.
- Rollback plan documented and rehearsed: within 30 minutes, the update
  manifest can be reverted to the previous version so users get the old
  build on their next launch.
- Hot-fix release process: if a P0 emerges, a patched build ships within
  24 hours, following an abbreviated version of gates 1-5.
