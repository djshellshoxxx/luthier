# Known issues and limitations

Written honestly. A model that hides its approximations is harder to trust than one
that states them, and a "known issues" file that says "none" is never true.

Describes the source at commit `ca14934e`. The project version in `CMakeLists.txt` is 0.1.0;
no release has been published from this source.

---

## Modelling approximations

These are deliberate simplifications, not defects. Each is a place where the model
stops short of the physics, and each is audible if you go looking for it.

**The body does not load the string.** On a real acoustic the top and the string
exchange energy in both directions, which shifts the string's decay and can produce
a wolf note. Here the coupling is one-way: the string drives the body, the body
does not drive the string. The audible consequence is that a big acoustic body
sustains slightly *more* evenly than the real instrument would.

**The pickup does not damp the string.** Magnetic pull shortens sustain on a real
guitar, which is why a pickup set very high sounds choked. Pickup height here
changes gain and brightness but not decay.

**Dispersion is bounded.** The all-pass cascade produces the right direction and a
realistic magnitude for guitar-range stiffness, but it cannot reach the extreme
partial stretching a piano's bass strings show. At the very top of the neck the
engine also reduces the number of dispersion stages to keep the pitch accurate, so
high fretted notes are modelled slightly less stiffly than open low ones.

**The string is a lumped delay line**, not a bidirectional waveguide with an
explicit nut and bridge. The output is equivalent, but the travelling waves cannot
be tapped separately, which is why the pickup's positional comb is applied as a
filter rather than as a read at a point.

**Speaker cone breakup is linear.** A real speaker distorts at high excursion.

**The tone stack ignores the following stage's input impedance**, which loads the
network slightly in a real amp.

**Sympathetic coupling is bridge-only.** Strings also couple through the neck and
through the air. Those paths are weaker and are not modelled.

---

## Impulse responses

**They are synthesised, not measured.** Shipping real captures would mean licensing
recordings of trademarked instruments and speakers. The IRs are generated from the
same plate, Helmholtz and speaker models the engine uses, which keeps the
convolution and modal paths consistent with each other, but they do not have the
idiosyncratic fine detail of a real capture. Commercial IRs can be loaded instead;
the engine takes any WAV.

**The library is curated, not exhaustive.** 216 body IRs and 504 cabinet IRs cover
the common combinations. A body configuration with no matching IR falls back to the
nearest one for that shape, and then to modal synthesis. A cabinet with no IR falls
back to the procedural speaker model. Neither is silent, but neither is the
convolution you asked for.

**A response swap is heard through the fallback.** Changing a preset must not stall
the audio thread, so the new impulse response is prepared on the thread that asked
for it and the audio thread keeps running on the analytic speaker and body models
until it is ready. The changeover is a single block, and the fallback is a
reasonable speaker rather than a dry signal, so what you hear is a brief change of
character rather than a dropout. It is only noticeable if you switch presets while
a note is ringing.

---

## Interface

**The guitar is drawn, not photographed.** Same licensing reason. The illustration
is generated from the live instrument settings, so it is always correct - including
for an instrument you just built, which a photograph could not be - but it is a
vector drawing, not a photograph.

**The window cutaway is drawn, not clipped.** Plugin windows are rectangular in
every host. The cutaway is carved out of the panel surface rather than out of the
window, which reads as the intended silhouette without host-dependent behaviour.

**Fonts fall back.** The interface asks for Inter and JetBrains Mono and falls back
through a list of system faces. If neither is installed the layout is unchanged but
the letterforms differ.

**The Advanced columns scroll independently.** On a short window, column four in
particular needs a lot of scrolling. Resizing the window taller is the practical
answer.

---

## Performance

**One instance with everything on is not cheap.** Six strings ringing, both pedal
racks loaded, dual-mic cabinet convolution and a large room is a genuinely
expensive patch. On a modest machine expect to need a 256-sample buffer or larger.
Options > Oversampling at 2x is the single biggest saving and is very close to 4x
in sound.

**The string engines run serially.** They are independent and could be
parallelised, but the synchronisation cost at typical buffer sizes outweighs the
benefit, and the sympathetic coupling needs all of them in step each sample. This
may change if profiling on a wider range of machines says otherwise.

**Preset changes rebuild the modal bank** on the message thread. It is staged and
swapped in atomically so the audio thread never blocks, but a very rapid sequence
of preset changes can leave the bank one block behind.

---

## Behaviour that is deliberate but surprising

**Poly mode has latency.** The chord window, 2 ms by default, is what lets a chord
split across a buffer boundary still voice as a chord. It is reported to the host,
so delay compensation handles it. Set it to zero if you would rather have neither.

**Chords are strummed, not triggered together.** Set Strum Speed to zero for a
simultaneous attack.

**The tuning is not perfect.** Realism Detune is on by default because a real
guitar is never exactly in tune. Zero it in Advanced for machine-perfect tuning.

**Notes go slightly sharp up the neck.** That is the intonation model, and real
guitars do it. Zero it in the same place.

**Every pickup off means silence** on an electric guitar. There is nothing to sense
the strings. The instrument is not broken.

**Panic is a hard stop.** It stops every string and also resets the body, cabinet,
effects, room and master stages (`LuthierEngine::panic`, `Source/LuthierEngine.cpp`),
so reverb and delay tails are cut off rather than left to ring out. It leaves the
parameter values where they were. The test
`ResetStop.panicStopsEveryPlayerAndLeavesTheSettings` checks that the players stop, no
parameter moves, and the output falls back to the idle noise floor. An earlier
version of this file said Panic leaves the reverb ringing; the code does not.

---

## Not yet implemented

Things the brief describes that are not in this release, stated plainly rather than
quietly omitted.

**Plugin formats.** VST3 and Standalone are always built. AU is added on macOS only
(`CMakeLists.txt`, `LUTHIER_FORMATS`). CLAP is built when
`ThirdParty/clap-juce-extensions` is present (`scripts/setup_linux.sh` and the CI
workflows fetch it); without it the build has no CLAP target. AAX is not built, so
Pro Tools is not supported.

**Linux build steps.** `scripts/setup_linux.sh` installs the build dependencies and
JUCE 8.0.10 and configures a Release Ninja tree; then
`ninja -C build LuthierTests Luthier_VST3 Luthier_Standalone`, and run the suite
under `xvfb-run -a`. `cpack` in the build tree produces the `.tar.gz` and `.deb`
packages. Linux builds; see "Platform qualification" for what has and has not been
run.

**Signed and notarised installers.** The build produces the plugin bundles; it does
not produce signed installers, which need certificates that cannot be part of a
source repository.

**Verified host testing.** The automated suite covers the plugin's own behaviour,
including opening the editor and exercising its panels and shortcuts inside the test
binary. The CI scripts run pluginval (strictness 5 on push and pull request,
strictness 10 on manual dispatch, `.github/workflows/build.yml`) and clap-validator
(`scripts/ci_build.sh validate`). An earlier version of this file said pluginval
passed at strictness 10 with twenty-five suites and no warnings on 2026-09-19; no log
for that run is kept in the repository, so that claim is withdrawn as not verified.

The thirty-minutes-per-host matrix in the brief has not been carried out, in any host.
`HOST_COMPATIBILITY.md` describes intended handling, not tested behaviour. Testing
against physical MIDI guitar and MPE controllers, and the blind listening comparison
against real guitars, have not been carried out either.

**Drag-out export.** Audio and MIDI export work through the file dialogs; dragging
a rendered file directly out of the plugin window into a DAW track is not
implemented.

**Backing-track file formats.** The backing-track player registers JUCE's basic
formats, so WAV, AIFF, FLAC and Ogg Vorbis always load. MP3 and WMA rely on the
platform decoder — Windows Media on Windows, CoreAudio on macOS — and are not
compiled in directly, so a Linux build would reject them.

---

## Pedalboard

The reference pedalboard is a separate, newer subsystem from the two eight-slot pedal
racks in the Advanced panel. See [PEDALBOARD.md](PEDALBOARD.md) and
[EFFECTS_REFERENCE.md](EFFECTS_REFERENCE.md).

- It is not release-qualified. The first pedalboard release was planned around six
  named models: a phaser, two bucket-brigade choruses, a digital delay, a compressor
  and a looper. All six exist as unregistered prototypes and are **not available to
  users**.
- What users can add today is 13 registered models: 10 generic effects and 3
  prototypes (`Source/ReferencePedals/PedalRegistry.cpp` and the register functions
  beside it).
- Board capacity is 16 slots and 8 snapshots.
- The test `RefPedalCe2.bbdOnOriginalThdGate` fails on every platform: the chorus
  model's distortion with the bucket-brigade simulation on is below the specified
  band. The model stays unregistered. The gate has not been loosened.
- Several other pedalboard gates are open according to the project's handoff records (`docs/handoff/PEDALBOARD-2026-10-28-CONTINUATION.md`; not re-measured here): CPU budget for four of the six
  models; the looper's memory switching, MIDI program change, quantise, half-speed and
  live reverse are not implemented). These apply to the unregistered models and do not
  affect the 13 users can add.
- MIDI learn and tempo handling on the board are partial: no MIDI clock output.
- Whether the board behaves differently in the Free edition is not verified; no
  edition gating was found in the board code.

---

## Platform qualification

- **Linux x64, clang 18:** the unit-test binary builds and has 2,923 tests in 367
  suites. On the CI run for this commit (GitHub Actions run 41796728348), exactly one
  test failed in both editions: `RefPedalCe2.bbdOnOriginalThdGate`.
- **Windows:** the same run additionally failed
  `RefPedalUiLifecycle.tenThousandNativeOperationsAndEditorDestruction`.
- **macOS:** the same run had further failures. Earlier macOS runs also showed an AU
  pluginval crash in the Automation test. The causes are not established here.
- **No native macOS or Windows release qualification exists for this source.**
  Do not treat the formats for those platforms as validated.
- No installer is signed or notarised.

---

## Reporting something

If you hit something that is not on this list:

1. **Help > Debug > Export troubleshooting file.**
2. If it is a crash, also tick **Create log file on crash**, reproduce it, and
   collect that file too.
3. Send both, from `Documents/Luthier/Diagnostics`, to
   `support@luthieraudio.example`, with what you were doing.

The troubleshooting file contains your settings, your host and your audio
configuration. It contains no audio and nothing personal.
