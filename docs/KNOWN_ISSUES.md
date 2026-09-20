# Known issues and limitations

Written honestly. A model that hides its approximations is harder to trust than one
that states them, and a "known issues" file that says "none" is never true.

Version 1.0.0.

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

**Panic does not clear the reverb.** It stops every string, which is what a panic
button is for, but the tail of the room and the delays is left to ring out rather
than being cut off abruptly.

---

## Not yet implemented

Things the brief describes that are not in this release, stated plainly rather than
quietly omitted.

**CLAP and Linux builds.** The CMake project is platform-neutral and the code has
no Windows-specific dependencies, but only VST3, AU and standalone are built and
tested here. CLAP needs the CLAP wrapper added to the JUCE build.

**Signed and notarised installers.** The build produces the plugin bundles; it does
not produce signed installers, which need certificates that cannot be part of a
source repository.

**Verified host testing.** The automated suite covers the plugin's own behaviour
thoroughly. pluginval 1.0.3 passes at strictness 10 against the current build,
re-run on 2026-09-19 after the extension specs landed: twenty-five suites, no
failures and no warnings. The multi-out bus layouts are covered by it rather than
merely present - the bus tests enumerate the full output set, then enable all
buses, disable the non-main ones and restore the default layout. The editor no
longer depends on that run: the test suite builds the UI and opens the window
itself, so the panels, the shortcuts and the Options tabs are exercised on every
build rather than only when someone remembers to run an external tool.

The thirty-minutes-per-host matrix in the brief is a manual exercise that has not
been carried out here. The same applies to testing against physical MIDI guitar
and MPE controllers, and to the blind listening comparison against real guitars.

**Drag-out export.** Audio and MIDI export work through the file dialogs; dragging
a rendered file directly out of the plugin window into a DAW track is not
implemented.

**Backing-track file formats.** The backing-track player registers JUCE's basic
formats, so WAV, AIFF, FLAC and Ogg Vorbis always load. MP3 and WMA rely on the
platform decoder — Windows Media on Windows, CoreAudio on macOS — and are not
compiled in directly, so a Linux build would reject them.

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
