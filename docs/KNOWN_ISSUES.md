# Known issues and limitations

Written honestly. A model that hides its approximations is harder to trust than one
that states them, and a "known issues" file that says "none" is never true.

Updated for the beta2 candidate `7fcb88c6` and the November 19 consolidation.
The project version in `CMakeLists.txt` is 0.1.0. No beta2 release has been
published. Exact native results and remaining gates are recorded in
[the current consolidation status](status/CONSOLIDATION-2026-11-19.md).

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

**Replacement responses are prepared off the audio thread.** Cabinet and body IR
banks now prime their latency and use bounded crossfades when a prepared replacement
becomes active. The former one-block fallback handoff has been repaired; the
production-path tests and limits are recorded in
[IR-HANDOFF-2026-11-16.md](status/IR-HANDOFF-2026-11-16.md). A different IR still changes
the instrument's sound, including notes already ringing.

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

**Plugin formats.** This beta targets Windows VST3 and Standalone, plus universal
macOS VST3, AU and Standalone. Linux and CLAP remain development targets; CLAP's
state round-trip defects keep it out of the release. AAX is not built, so Pro Tools
is not supported.

**Linux build steps.** `scripts/setup_linux.sh` installs the build dependencies and
JUCE 8.0.10 and configures a Release Ninja tree; then
`ninja -C build LuthierTests Luthier_VST3 Luthier_Standalone`, and run the suite
under `xvfb-run -a`. `cpack` in the build tree produces the `.tar.gz` and `.deb`
packages. Linux builds; see "Platform qualification" for what has and has not been
run.

**Signed and notarised installers.** Packaging scripts can sign Windows installers
and sign/notarise macOS installers when the required identities are configured.
A successful build does not prove signing. The release's per-platform logs and
provenance must state the actual result.

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

- **0.1 beta scope: one pedal.** The browser, the Add menu and drag-and-drop offer
  only the Four-Stage Phaser (`mxr-phase90-script`), the first named model developed
  (`Source/ReferencePedals/ReleaseCatalogue.h`). The original plan of six named models
  (adding two bucket-brigade choruses, a digital delay, a compressor and a looper) is
  deferred; those five stay unregistered.
- The 10 generic effects and 3 legacy prototypes remain registered only so that a
  session saved by an earlier build still loads them. They cannot be added.
- Phaser gates still open: CPU under 0.4 % of one core is not met on the 2.1 GHz Xeon
  reference measurements (0.42-0.78 %); the specification's "peak delta < 1 LSB 24b"
  bypass criterion has no ruling on how it is measured; the 30-minute model soak and a
  hardware listening comparison have not been run.
- Board capacity is 16 slots and 8 snapshots.
- The CE-2 distortion regression was repaired without changing the original
  acceptance limits; see [CHORUS-QUALIFICATION-2026-11-16.md](status/CHORUS-QUALIFICATION-2026-11-16.md).
  The chorus stays outside the one-pedal beta catalogue. Passing its distortion
  test does not close the other model-specific acceptance gates.
- Several other pedalboard gates are open according to the project's handoff records (`docs/handoff/PEDALBOARD-2026-10-28-CONTINUATION.md`; not re-measured here): CPU budget for four of the six
  models; the looper's memory switching, MIDI program change, quantise, half-speed and
  live reverse are not implemented). These apply to the unregistered models and do not
  affect the beta's one-pedal Add catalogue. Legacy sessions can still load their
  previously registered generic/prototype pedals.
- MIDI learn and tempo handling on the board are partial: no MIDI clock output.
- Whether the board behaves differently in the Free edition is not verified; no
  edition gating was found in the board code.

---

## Platform qualification

The latest completed release attempt inspected is
[run 44259292225](https://github.com/djshellshoxxx/luthier/actions/runs/44259292225),
which tested exact source `7fcb88c601288e7a7a02cf64548a822b9c3fcfc4`.

- All four native Windows/macOS Free/Pro build, plugin-validator and packaging
  jobs passed with LTO and strictness 10. Windows edition-coexistence installation
  checks also passed.
- Twelve of sixteen full-suite shards passed. macOS Free passed all four shards.
- The four failing shards each reported one performance failure: Windows Free
  and Pro part swaps at p95 54.4/57.0 ms against 50 ms; macOS Pro warm instantiation
  at 1287.5 ms against 1000 ms; and macOS Pro guitar loading at p95 1414.3 ms
  against 1200 ms. The thresholds remain unchanged.
- These failures prevented draft release creation. Subsequent code changes need
  a new complete qualification run; the earlier results are not final-head evidence.
- Linux was not selected for this release attempt. Manual DAW/hardware/listening
  acceptance and signing status remain separate from automated qualification.

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
