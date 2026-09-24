# Open-source ideas for Luthier

Research note, 2026-09-24. Scope: open-source guitar, bass and plucked-string
synths, and the open projects around them (amp/cab, notation, transcription,
tuning, looping, practice). The goal is ideas that are **new to Luthier or
clearly better than what the spec set already has**. This document proposes
nothing binding. Per `CLAUDE_CODE_BRIEF.md`, anything adopted goes to a
proposal or spec file first.

What was checked in Luthier before proposing anything: `spec/INDEX.md` (what
each file adds), the section headings of `spec/gui-integration.md`,
`spec/engine.md` 5-15, `spec/spec.md`, `sustain-and-decay.md` 3,
`body-coupling.md`, `tone-match.md`, `practice-tools.md`,
`performance-budget.md`, `host-integration.md`, `microtonal-bends.md`,
`fret-buzz.md` 5, `proposals/visual-polish.md`, `piano-roll-chord-display.md`,
and `Source/DSP/String/StringEngine.h`. It also allows for the features being
specified now: auto-articulation, jam mode, preset browser with previews and
search, riff library, draggable mic placement, animated strings, global
search / command palette, piano roll and chord names.

## Licence rule used throughout

Luthier is a closed-source commercial product. For every source below:

- **Code reusable**: MIT, BSD-2/3, 0BSD, Apache-2.0, STK licence, Faust
  libraries (LGPL with an exception that lets the compiled output use any
  licence). Keep the notices and list them in `THIRD_PARTY_LICENCES.txt`
  (`qa-polish.md` 11).
- **File-level copyleft**: MPL-2.0 (alphaTab). You can link it unchanged, but
  any changes to MPL files must be published. In practice, use it as a
  format reference only.
- **Idea only, do not copy code**: GPL-2/3 (Surge XT, Vital, Dexed, Odin 2,
  ZynAddSubFX, Guitarix, AIDA-X, Proteus, NAM-LV2, SooperLooper, MuseScore,
  Impro-Visor, Sapphire, x42 midifilter), LGPL applications that would have to
  be linked statically into a plugin (TuxGuitar, LSP, YARG). Algorithms from
  papers are always fine.
- **Model weights are licensed separately from code.** Check each one. For
  example, NeuralNote's code is Apache-2.0, but its v2 MuScriptor weights are
  CC BY-NC.

Several project sites could not be reached from the research sandbox
(faustlibraries.grame.fr, surge-synthesizer.github.io, doc.sccode.org,
dafx.de). Where that happened, the claims come from the GitHub source or
READMEs, and the text says so.

---

## 1. Executive summary: the top 15

Ranked by value to a $200 guitar instrument. The ranking weighs user impact,
differentiation, fit with physical modelling, effort (S under 2 weeks, M 2-6
weeks, L over 6 weeks for one engineer) and risk. Tags: **P** premium feel,
**U** usability, **F** fun.

### 1. Audio-driven excitation and a "sympathetic resonator" mode
- **What:** any audio input excites Luthier's strings directly: a real
  guitar's piezo or DI, a tap on the desk, a drum loop, a voice. The input is
  injected at the excitation point, so the strings, sympathetic coupling,
  body, pickups and amp all respond. A second mode has no MIDI at all: the
  strings are held at a chord or drone (for example sitar-style drone strings
  or a 12-string's octave courses) and ring in sympathy with whatever audio
  comes in. That makes Luthier an effect as well as an instrument.
- **Where it came from:** Mutable Instruments Rings. Its resonator takes
  external audio as the exciter, detects strums from the input, and has
  "sympathetic string" and "quantized sympathetic string" chord modes
  ([rings/dsp/part.cc](https://github.com/pichenettes/eurorack/blob/master/rings/dsp/part.cc),
  MIT). Csound `repluck` is a plucked string driven by an audio signal
  ([manual](https://csound.com/docs/manual/repluck.html)). Surge XT's String
  oscillator has audio-input exciter modes
  ([StringOscillator.cpp](https://github.com/surge-synthesizer/surge/blob/main/src/common/dsp/oscillators/StringOscillator.cpp),
  GPL).
- **How it fits Luthier:** `routing-io.md` already has a stereo sidechain
  input, but it is used only for re-amp (at the amp stage) and envelope
  followers. Add a new `ExcitationSource::Audio` to `Excitation`
  (`engine.md` 5.5). It takes the sidechain, runs it through the existing
  pick/finger excitation filters and a gain/transient gate, and splits it to
  the strings: by pitch for resonator mode, or per string when a hex input
  arrives through `controllers.md` 4. Onset detection on the input can raise
  "strum" events (as Rings does), so auto-articulation and the rhythm engine
  see them. Coupling-energy clamps from `engine.md` 5.6 still apply. Add
  resonator-mode presets to the factory content.
- **Licence:** Rings is MIT, so its strum detector and chord tables can be
  reused with attribution. Surge is idea only.
- **Effort:** M. **Risk:** feedback and runaway energy (use the existing clamp
  and NaN guards) and latency on live input (zero-latency path, no
  lookahead). **Tags:** P, F. No commercial guitar model does this well, so it
  is a strong demo.

### 2. A "Capture amp" slot that loads NAM and RTNeural models
- **What:** a fourth amp mode beside the modelled amps. It loads `.nam`
  (Neural Amp Modeler A1/A2) and RTNeural/AIDA-X `.json` models. Tens of
  thousands of free community captures (TONE3000) become available inside
  Luthier. The mode includes input calibration in dBu, loudness
  normalisation from model metadata, and a quality control for A2
  "slimmable" models, which scale CPU inside a single file.
- **Where it came from:**
  [NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore)
  (MIT) and [NeuralAmpModelerPlugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin)
  (MIT: input calibration, output normalisation, IR, gate).
  [RTNeural](https://github.com/jatinchowdhury18/RTNeural) (BSD-3).
  [AIDA-X](https://github.com/AidaDSP/AIDA-X) (GPL: amp+cab chain, idea only).
  [neural-amp-modeler-lv2](https://github.com/mikeoliphant/neural-amp-modeler-lv2)
  (GPL: A2 "lite" quality switch, oversampling at integer multiples of the
  training rate, bypass during silence).
  [Proteus](https://github.com/GuitarML/Proteus) (GPL: knob-conditioned LSTM
  captures). A2 and slimmable background:
  [TONE3000](https://www.tone3000.com/blog/slimmable-nam-the-future-of-neural-amp-modeler).
- **How it fits Luthier:** `AmpEngine` gets `Mode::Capture`, and the rest of
  the chain is unchanged. The fit is unusually good because
  `volume-knob-interaction.md`'s `GuitarCircuit` already produces a
  physically scaled pickup voltage. That voltage can be mapped straight to
  NAM's `input_level_dbu` metadata, so a captured amp breaks up at the same
  picking strength as the real one. No other NAM host knows the true
  instrument level. Cab stays in `CabinetEngine`, and models tagged
  "full rig" bypass the cab automatically. The existing `tone-match.md` 4
  Capture utility can record the NAM training signal pair, so users can
  capture their own amp and train it offline with NAM's trainer.
- **Licence:** NAM Core and RTNeural code are reusable (MIT, BSD). User
  models belong to the users. Do not ship third-party captures without a
  licence.
- **Effort:** M. **Risk:** CPU. A1 "standard" is heavy with 12 strings, so
  add it to `performance-budget.md`, default to A2/slim, and put the budget
  in the eco tier. There is also a support burden from bad models, so
  validate them on load (`error-recovery.md`). **Tags:** P, U.

### 3. A true dual-polarisation string with pluck direction
- **What:** replace `sustain-and-decay.md` 3's time-varying-loss
  approximation with two coupled waveguides per string, one vertical and one
  horizontal. Pick angle and stroke direction set the initial energy split.
  The two stages of decay, the slow beating (0.2-2 Hz) and the
  "aftersound" then come out of the model instead of being enforced. The
  spec itself defers the beating to `body-coupling.md`. This closes that gap.
- **Where it came from:** SuperCollider `DWGPlucked2`: two coupled strings
  with `mistune` and a coupling factor `gc`
  ([help](https://github.com/supercollider/sc3-plugins/tree/main/source/DWGUGens),
  GPL, idea only). STK `Mandolin`: detuned `Twang` pairs
  ([Mandolin.h](https://github.com/thestk/stk/blob/master/include/Mandolin.h)).
  Research: Bank and Karjalainen, "Passive admittance matrix modeling for
  guitar synthesis" (DAFx-10), which adds a stable dual-polarisation bridge
  coupling; and
  [Acta Acustica 2020 on polarisation coupling](https://acta-acustica.edpsciences.org/articles/aacus/full_html/2020/03/aacus200019/aacus200019.html).
- **How it fits Luthier:** inside `StringEngine`, add a second delay and loop
  filter per string (the fractional-delay and dispersion code is shared).
  Excitation gains a `pluck_angle` value, taken from `pick-noise.md` pick
  angle and from `strum-dynamics.md` stroke direction (up vs down strums
  differ audibly). The body-coupling admittance becomes a 2x2 matrix per
  string. Keep the current approximation as the eco path.
- **Licence:** idea or paper.
- **Effort:** L. **Risk:** string CPU rises by about 1.6-1.8x against a
  2.5-unit string budget, and the coupled system must stay stable (use a
  passive admittance design). Needs an A/B listening gate. **Tags:** P. This
  is the realism upgrade reviewers notice on long sustained chords.

### 4. Audio to tab: drop in a recording and get Luthier MIDI with strings, frets and bends
- **What:** drag a WAV or MP3 of a guitar part (or a whole song, see #14)
  into Luthier. It transcribes the part to notes with pitch bends, assigns
  string and fret, and plays it back through the current guitar. The same
  path feeds the tab view, MIDI export and the riff library ("learn this
  lick").
- **Where it came from:** Spotify
  [Basic Pitch](https://github.com/spotify/basic-pitch) (Apache-2.0,
  polyphonic, detects pitch bends, ONNX/CoreML/TFLite models).
  [NeuralNote](https://github.com/DamRsn/NeuralNote) (Apache-2.0) shows
  Basic Pitch working inside a JUCE plugin with drag-out MIDI. Its v2
  MuScriptor weights are CC BY-NC, so do not use them. For string and fret
  assignment:
  [FretNet](https://github.com/cwitkowitz/guitar-transcription-continuous)
  and the
  [TabCNN inhibition work](https://github.com/cwitkowitz/guitar-transcription-with-inhibition),
  both trained on GuitarSet.
- **How it fits Luthier:** run it offline on a worker thread and never on the
  audio thread. Basic Pitch gives notes and bends. `ChordVoicer` and
  `engine.md` 2's string-choice rules give the fingering, which avoids
  shipping a second neural model. The output is an ordinary `.mid` in the
  Luthier profile (`midi-export.md`), so undo, tab and export all work
  unchanged. The inference runtime is RTNeural or ONNX Runtime (MIT).
- **Licence:** Basic Pitch code is Apache-2.0. Confirm the weights' licence
  in the repo before shipping. GuitarSet-trained models: check the dataset
  terms.
- **Effort:** L. **Risk:** accuracy on distorted guitar (present results as
  "draft", fully editable) and binary size (Basic Pitch is small, under a few
  MB). **Tags:** F, U.

### 5. DAW articulation maps, generated from Luthier's keyswitch table
- **What:** one-click export of Luthier's keyswitches, CC triggers and
  technique zones as a Cubase Expression Map, Logic Articulation Set, Studio
  One Sound Variations and a REAPER Reaticulate bank. Show the keyswitch
  labels on the on-screen keyboard and piano roll. Accept the SFZ-style
  conventions: a latching keyswitch range, momentary keyswitches (held
  down), and "previous note" keyswitches for legato.
- **Where it came from:** SFZ keyswitch opcodes as implemented in
  [sfizz](https://github.com/sfztools/sfizz) (BSD-2): `sw_last`,
  `sw_lolast`/`sw_hilast`, `sw_down`/`sw_up`, `sw_previous`, `sw_vel`,
  `sw_label`, `sw_default`.
  [Reaticulate](https://github.com/jtackaberry/reaticulate) (Apache-2.0;
  its icons cannot be redistributed).
- **How it fits Luthier:** the keyswitch numbers are already fixed
  (`DECISIONS.md`, `engine-technique-layer.md`), but they are spread across
  eight technique specs. Put them in one `ArticulationTable`. The exporter,
  the keyboard labels, auto-articulation's override list, the help tab and
  the command palette all read from that one table. `host-integration.md`
  9.3 already plans VST3 note expression for Cubase, and this is the cheaper
  first step.
- **Licence:** these are file formats and conventions, so no code is needed.
- **Effort:** S. **Risk:** low. **Tags:** U, P. This is what composers
  mention first in reviews.

### 6. Modulation list overlay, preview before commit, and highlighting targets on hover
- **What:** (a) a single list of every modulation route in the preset, where
  the user can edit depth, mute, solo or delete each route. (b) While
  dragging a source onto a knob, the sound previews the route before it is
  committed. (c) Hovering or selecting a source lights up every destination
  it drives, and the live modulated value moves on each arc.
- **Where it came from:** Surge XT's Modulation List (Alt+M; add, modify,
  mute and delete all routes in one place), described in its
  [manual](https://surge-synthesizer.github.io/manual-xt/) (not reachable
  from the sandbox, so taken from search summaries), GPL. Vital's drag and
  drop with "a preview of the modulation before committing" and animated
  targets ([vital.audio](https://vital.audio/), GPL).
- **How it fits Luthier:** `gui-integration.md` 11 already has mod arcs and
  drag-to-modulate. Add an overlay in Col 4 MOD, plus a command-palette
  entry "Show modulation list". Preview uses the shadow-audition pattern
  that already exists (`ui-wiring.md`). If the user cancels the drag, the
  route is discarded, and a committed route is one undo entry
  (`action-and-undo.md`).
- **Licence:** idea only.
- **Effort:** S-M. **Risk:** low. **Tags:** U, P.

### 7. Adaptive CPU level of detail and sleeping silent voices
- **What:** degrade quality automatically and gracefully instead of
  crackling. When many strings are sounding or the block is tight, reduce
  the dispersion allpass order, body-mode count and coupling update rate on
  the quietest strings. Strings, body tails, convolution tails and noise
  generators fully sleep when they fall below -120 dBFS. An offline render
  always uses full detail.
- **Where it came from:** Rings sets modal resolution from the voice count
  (`64 / polyphony - 4`) in
  [part.cc](https://github.com/pichenettes/eurorack/blob/master/rings/dsp/part.cc)
  (MIT). NAM-LV2 bypasses the model during silence. Dexed ships
  engine-resolution variants ([Dexed](https://github.com/asb2m10/dexed)).
  Pianoteq-style "polyphony / quality" tiers.
- **How it fits Luthier:** extend `performance-budget.md` with a
  per-module LOD table and a governor. The governor reads a smoothed
  block-time estimate and changes LOD only at a note-on or when a voice is
  below -60 dB, so there are no audible steps. Use `isNonRealtime()` to
  force full detail during offline renders. Unit tests: null-test a render
  at LOD 0 against a render at LOD 1 for loud strings.
- **Licence:** Rings' formula is MIT. The rest are ideas.
- **Effort:** M. **Risk:** audible switching. Guard with switch-at-quiet
  rules and a crossfade. **Tags:** U (live and laptop users), P.

### 8. Whole-instrument microtuning through MTS-ESP, with fret geometry that follows it
- **What:** when an MTS-ESP master is present, or a `.scl`/`.kbm` is loaded,
  retune the whole instrument, not only the bends. The Workshop then draws
  the matching frets: straight 12-TET, "True Temperament" wiggly frets, or
  arbitrary EDO frets such as 19-EDO or 24-EDO for Middle-Eastern and
  microtonal guitars. The fret-position engine uses the tuning table, so
  slides, vibrato and buzz stay consistent with the new frets.
- **Where it came from:** [ODDSound MTS-ESP client](https://github.com/ODDSound/MTS-ESP)
  (0BSD; per-note retuning queries, note filtering, MTS SysEx fallback).
  The Surge team's [tuning-library](https://github.com/surge-synthesizer/tuning-library)
  (MIT, header-only SCL/KBM parser). Dexed also supports MTS-ESP and SCL/KBM.
- **How it fits Luthier:** `TuningEngine` (`engine.md` 3) and
  `microtonal-bends.md` already accept `.scala`/`.tun` for bends only. Make
  `TuningEngine` the one owner of a 128-entry frequency table: MTS-ESP when
  connected, then the user SCL/KBM, then the temperament presets in
  `spec.md`. Add a "fret system" part to `guitar-workshop.md`, drawn per
  `guitar-illustration.md`.
- **Licence:** both libraries are reusable.
- **Effort:** S (engine), M (with Workshop frets). **Risk:** low. **Tags:**
  P, F.

### 9. CLAP per-note modulation, note expressions and remote-control pages
- **What:** when shipping CLAP (planned for v1.1 in `host-integration.md`),
  implement CLAP polyphonic modulation and note expressions (tuning,
  brightness, pressure per note). Bitwig users can then modulate per string
  and per note natively. Also publish CLAP remote-control pages so hardware
  controllers show "Pick / Tone / Amp / FX" pages without MIDI learn.
- **Where it came from:** Surge XT 1.1 added CLAP with polyphonic modulation
  ([KVR news](https://www.kvraudio.com/news/surge-synth-team-updates-free-surge-xt-to-v1-1---clap-support-and-more-55528)).
  Odin 2 and Dexed ship CLAP. Luthier already vendors
  `clap-juce-extensions` (MIT).
- **How it fits Luthier:** note IDs map to the string that
  `MidiInterpreter` assigned. Per-note modulation lands on the same
  per-string destinations that the MPE path in `controllers.md` already
  drives. Remote-control pages come from the Easy Mode strips.
- **Licence:** the CLAP SDK is MIT.
- **Effort:** M. **Risk:** host differences. **Tags:** P, U.

### 10. Modal bodies from measured and simulated data, with interpolation between them
- **What:** today the modal body mode scales every mode frequency in
  proportion to body size (`engine.md` 6.2). Real bodies do not scale that
  way: bracing, top thickness and shape move modes independently. Precompute
  modal banks offline for each Workshop body, bracing and top-thickness
  combination using finite element analysis of 3D meshes. At runtime,
  interpolate between the nearest precomputed banks as the user drags body
  parameters.
- **Where it came from:** Faust `pm.modeInterpRes` (a 40-mode resonator
  interpolated over shape and scale from 3D models) in
  [physmodels.lib](https://github.com/grame-cncm/faustlibraries/blob/master/physmodels.lib)
  (LGPL with an exception for compiled output). The
  [mesh2faust](https://github.com/grame-cncm/faust/tree/master-dev/tools/physicalModeling/mesh2faust)
  FEM tool, used offline only (Michon, Martin and Smith, ICMC 2017).
- **How it fits Luthier:** this is an offline factory pipeline in
  `Tools/`. It produces JSON modal banks consumed by `BodyEngine` modal mode
  and by `part-acoustics.md`'s body fields. The Workshop spectrum delta
  (`workshop-ui.md` 6) then shows honest, non-proportional changes. Tap
  tones and wolf notes (`body-coupling.md`) come out of the same banks.
- **Licence:** tool output is data, and the Faust library exception allows
  closed use. Do not ship mesh2faust itself.
- **Effort:** L. It is mostly content engineering and calibration against
  measured bodies. **Risk:** getting materials and boundary conditions
  wrong. Validate against published guitar modal data. **Tags:** P.

### 11. Wave-digital-filter guitar circuit and drive pedals
- **What:** model the pickup, pots, tone cap, cable, fuzz and drive input
  stages as one wave digital filter (WDF) network. The two-way interactions
  a player hears then come out of the circuit: a Fuzz Face cleaning up from
  the guitar's volume knob, a treble booster reacting to the pickup
  inductance, a buffer changing all of it.
- **Where it came from:** [chowdsp_wdf](https://github.com/Chowdhury-DSP/chowdsp_wdf)
  (BSD-3, header-only, R-type adaptors, diode models, SIMD). Guitarix
  generates circuit models from netlists
  ([guitarix](https://github.com/brummer10/guitarix), GPL, idea only).
- **How it fits Luthier:** `GuitarCircuit` (`volume-knob-interaction.md`)
  models amp-input loading, but a pedal's input impedance is treated as a
  separate block. Let the first pedal in `PreEffectsChain` offer a WDF
  "port" that loads `GuitarCircuit` directly. Oversample 4x per
  `engine.md` 0.10.
- **Licence:** chowdsp_wdf is reusable.
- **Effort:** M. **Risk:** CPU and numerical stiffness. The library handles
  stiffness well. **Tags:** P.

### 12. Jam mode: "trade fours" and a grammar-based lick generator
- **What:** in jam mode, the player plays four bars and Luthier answers with
  four bars built from a genre lick grammar fitted to the current chord
  progression (Tune Builder / chord detector). The grammar also seeds the
  riff library with endless variations that respect the key, the chord tones
  and guitar playability.
- **Where it came from:** [Impro-Visor](https://github.com/Impro-Visor/Impro-Visor)
  (GPL, idea only): grammar-based improvisation, style patterns, interactive
  "trading".
- **How it fits Luthier:** the jam mode and riff library specs being written
  now. The grammar produces abstract rhythm and interval slots. The existing
  voicer and fingering rules turn them into strings and frets, and the
  output is ordinary MIDI (Tune Builder rule: "a MIDI writer"). The call is
  captured by `PerformanceCapture` (`notation-export.md` 6).
- **Licence:** idea only. Write the grammars in-house.
- **Effort:** M. **Risk:** musical quality. Hand-curate the grammars per
  genre kit. **Tags:** F.

### 13. Groove extraction: humanise from a reference
- **What:** drop in a MIDI or audio loop. Luthier extracts its micro-timing
  and accent template, per 16th note, and applies it to strum and fingerpick
  patterns. A strum can then sit with the drummer's groove instead of random
  jitter.
- **Where it came from:** Magenta GrooVAE, including a "humanize" model that
  converts quantised patterns into grooves
  ([music_vae](https://github.com/magenta/magenta/tree/main/magenta/models/music_vae),
  Apache-2.0). The deterministic template approach (as in DAW groove pools)
  needs no model at all.
- **How it fits Luthier:** add a `GrooveTemplate` to the `rhythm-engine.md`
  `StrumScheduler` and `FingerpickScheduler`, stored in `.luthierpattern`.
  Onset detection comes from the backing-track tempo estimator
  (`practice-tools.md` 3). Humanize (`spec.md`) gains "from template".
- **Licence:** start with the deterministic approach. The ML variant uses
  Apache code, but the weights need checking.
- **Effort:** M. **Risk:** low. **Tags:** F, P.

### 14. Play-along with the guitar removed: stem separation for backing tracks
- **What:** load a song into the backing-track player and choose "remove
  guitar" or "remove bass". Luthier separates the stems offline and plays
  the song without that part, and #4 can transcribe the removed part.
- **Where it came from:** [demucs.cpp](https://github.com/sevagh/demucs.cpp)
  (MIT, C++17 plus Eigen) runs the
  [Demucs](https://github.com/facebookresearch/demucs) v4 `htdemucs_6s`
  model, which has guitar and piano stems.
- **How it fits Luthier:** `practice-tools.md` 3. Run it on a background
  thread with progress, and cache the stems beside the user's file (the rule
  that the plugin never bundles audio still holds).
- **Licence:** code is MIT. The Demucs repo states MIT for the pretrained
  weights, but the training data included MUSDB. Get a legal opinion before
  shipping, or offer the weights as an optional download.
- **Effort:** L. **Risk:** CPU time (minutes per song), roughly 80-300 MB of
  weights, and the separated guitar stem is "experimental". **Tags:** F, U.

### 15. Coupled string courses and a proper jawari bridge
- **What:** (a) 12-string, mandolin and Nashville courses become coupled
  string pairs with a small mistuning and a coupling factor, so they
  shimmer and beat like the real thing. Today they are independent strings.
  (b) Sitar mode's buzz becomes a cheap curved-bridge nonlinearity on the
  string loop.
- **Where it came from:** `DWGPlucked2` (`mistune`, `gc`), STK `Mandolin`
  (`setDetune`, two `Twang`s with the same excitation), and Rings' "curved
  bridge" nonlinearity in
  [string.cc](https://github.com/pichenettes/eurorack/blob/master/rings/dsp/string.cc)
  (MIT).
- **How it fits Luthier:** use the `CouplingMatrix` with a strong entry
  between course partners, excited together with a sub-millisecond offset.
  Add a `course_detune_cents` field to strings in `guitar-workshop.md`.
  `fret-buzz.md` 5's sitar mode can use the curved-bridge function as its
  buzz generator.
- **Licence:** Rings code is reusable with MIT attribution. The rest are
  ideas.
- **Effort:** S-M. **Risk:** low. **Tags:** P, F.

---

## 2. Catalogue by theme

Every project surveyed, with its licence, what is notable, and a verdict.
Items already in the top 15 are only cross-referenced.

### 2.1 String and body physics

| Project | Licence | Notable | Verdict for Luthier |
|---|---|---|---|
| **Faust physmodels.lib** ([src](https://github.com/grame-cncm/faustlibraries/blob/master/physmodels.lib)) | LGPL + exception | Bidirectional waveguide blocks (`chain`, `stringSegment`), `openStringPickUp` / `openStringPickDown` with **separate excitation and pickup points**, `guitarBridge` / `guitarNuts` reflection filters, `modeInterpRes` | The **bidirectional form** is worth checking. Luthier uses a lumped EKS (`engine.md` 5.2) and puts pickup position in `PickupEngine` 7.1 as a comb filter. With a real two-segment waveguide, the pickup comb follows fretting, bends and harmonics touch-points with no extra bookkeeping. Before adopting, check that 7.1 recomputes the comb from the *fretted* length. If it does, leave it. `modeInterpRes`: see #10. |
| **mesh2faust** ([tool](https://github.com/grame-cncm/faust/tree/master-dev/tools/physicalModeling/mesh2faust)) | GPL tool, output free | FEM of a 3D mesh to a modal model | Offline factory pipeline only (#10). |
| **STK** ([repo](https://github.com/thestk/stk)) | STK licence (permissive) | `Guitar`: N `Twang` strings, bridge coupling through a one-pole filter and gain, **pluck excitation read from a body soundfile** (commuted synthesis). `Mandolin`: 12 body-IR soundfiles as excitation, selected by "mic position"; detuned pairs. `StifKarp`: allpass-cascade stiffness, `pickupPosition`, `stretch`. | Commuted synthesis gives a **cheap "eco body"**: excite each string with the body IR, which saves the summed-body convolution. It loses string-body-string feedback, so offer it only as the eco tier or for the Free edition (`editions.md` says the quality is identical, so this needs a decision). Mandolin's IR-per-mic-position fits the draggable mic placement for acoustics, because the acoustic "mic" can blend commuted IRs recorded at several positions. |
| **Mutable Instruments Rings** ([string.cc](https://github.com/pichenettes/eurorack/blob/master/rings/dsp/string.cc), [part.cc](https://github.com/pichenettes/eurorack/blob/master/rings/dsp/part.cc)) | MIT | Dispersion allpass with a "stretch point"; parallel FIR+IIR damping with a crossfade to infinite decay above 0.95; curved-bridge nonlinearity; read tap moves for position; audio-in excitation; strum detection; chord-quantised sympathetic strings; resolution that scales with polyphony | #1, #7, #15. The **crossfade to infinite sustain** is also a clean way to do the E-Bow and freeze without energy drift. |
| **Mutable Instruments Elements** ([dsp](https://github.com/pichenettes/eurorack/tree/master/elements/dsp)) | MIT | Exciter with bow, blow and strike (mallet, plectrum, granular "particles"); 64-mode resonator; `tube` | **Granular "particles" exciter**: a brush or rain-like excitation (fingers brushing strings, a "chime rake" effect). A bow exciter: see 2.2. |
| **SuperCollider DWGPlucked / DWGPlucked2 / DWGPluckedStiff** ([sc3-plugins](https://github.com/supercollider/sc3-plugins/tree/main/source/DWGUGens)) | GPL | Separate DC-decay (`c1`) and HF-loss (`c3`) parameters; two coupled strings with `mistune`/`gc`; a stiff variant | #3, #15. The `c1`/`c3` split is a clearer two-parameter decay UI than one damping value. It is worth mirroring in Workshop string inspector wording ("sustain" vs "brightness decay"). |
| **Csound wgpluck2 / repluck** ([manual](https://csound.com/docs/manual/repluck.html)) | LGPL | Pluck point, **pickup point** and bridge reflection coefficient; `repluck` is audio-excited | #1. Nothing else new. |
| **ChucK StifKarp** ([ugen_stk.cpp](https://github.com/ccrma/chuck/blob/main/src/core/ugen_stk.cpp)) | MIT or GPL | STK StifKarp exposed with `stretch`, `sustain`, `pickupPosition` | Nothing beyond STK. |
| **Surge XT String oscillator** ([src](https://github.com/surge-synthesizer/surge/blob/main/src/common/dsp/oscillators/StringOscillator.cpp)) | GPL | Two delay lines blended; 16 exciter modes (burst or continuous noise, pink, sine, ramp, **audio in**); soft clip in the loop; selectable sinc, linear or ZOH interpolation; negative feedback extension (half-wave "clarinet-like" strings) | #1. **Soft saturation inside the loop** is a stable way to bound energy for feedback and E-Bow modes. Consider it as a secondary guard beside the coupling clamp. |
| **Capstone (KhoiLe12)** ([repo](https://github.com/KhoiLe12/Capstone)) | not stated | JUCE C++17; multi-port waveguide bridge junction; 32-mode soundboard | Confirms the architecture. Nothing Luthier lacks. |
| **Research: longitudinal waves / "phantom partials"** (Bank & Sujbert; used by Pianoteq) | papers | Longitudinal string motion gives inharmonic partials at about 2 x f, which matters on wound bass strings and hard picking | Worth a small spec for bass realism (`bass-techniques.md`). Effort M. Not top 15 because it is subtle on guitar. |

### 2.2 Excitation and articulation

| Project | Licence | Notable | Verdict |
|---|---|---|---|
| **sfizz / SFZ** ([repo](https://github.com/sfztools/sfizz), archived 2026-06) | BSD-2 | Keyswitch semantics: latch range, `sw_down`/`sw_up` momentary, `sw_previous` (articulation chosen by the previous note, i.e. legato), `sw_vel` (velocity taken from the previous note), labels shown on the keyboard | #5. `sw_previous` is a useful pattern for auto-articulation: a slide or hammer is decided from the previous note on that string. That matches Luthier's legato rule (`spec.md` 237), and the table formalises it. |
| **Reaticulate** ([repo](https://github.com/jtackaberry/reaticulate)) | Apache-2.0 (icons not redistributable) | REAPER articulation banks and a UI | #5, as an export target. |
| **Faust `violinBow` / `bowTable`**, **STK Bowed**, **Elements bow** | LGPL-exc / permissive / MIT | A friction bow table on a waveguide | **Bowed guitar** (the Jimmy Page violin bow, or EBow "forward/reverse" feel). Luthier's `EBowDriver` is a magnetic driver. A bow exciter is a small add-on to `Excitation` with a big "fun" factor. Effort S-M. |
| **x42 midifilter.lv2 "strum"** ([repo](https://github.com/x42/midifilter.lv2)), **pizmidi midiStrum**, **Mildon Strummer** (freeware) | GPL / freeware | Collect a chord, then spread the notes over time | Luthier's `strum-dynamics.md` (crossing velocity, strikers) is far ahead. Nothing to take. |
| **Elements "particles"** | MIT | Granular excitation | Brush strokes and rakes (see 2.1). |

### 2.3 Amp, cab and effects

| Project | Licence | Notable | Verdict |
|---|---|---|---|
| **NeuralAmpModelerCore / Plugin** | MIT | A1/A2 models, dBu input calibration, output normalisation, tone stack, gate, IR; `benchmodel` tool | #2. `benchmodel` is also a good pattern for Luthier's per-module CPU CI (see 2.8). |
| **neural-amp-modeler-lv2** | GPL | A2 "lite" below quality 0.5; correct oversampling at integer multiples; bypass during silence | #2, #7 (ideas). |
| **AIDA-X** | GPL | RTNeural JSON models, runtime IR resampling (r8brain), pre/post EQ, clip meters | #2. The idea of **resampling an IR at runtime** on load is already implied by `tone-match.md`. Confirm that it is specified. |
| **RTNeural** | BSD-3 | Compile-time and runtime model APIs; Eigen and xsimd backends | The inference engine for #2 and #4. |
| **Proteus / GuitarML** ([repo](https://github.com/GuitarML/Proteus)) | GPL | **Knob-conditioned** captures: a model with the gain knob as an input | Later stage of #2: a "capture with a knob" is where NAM A1 is weakest. Idea only. |
| **chowdsp_wdf** | BSD-3 | WDF circuits | #11. |
| **Guitarix / GxPlugins** ([repo](https://github.com/brummer10/guitarix)) | GPL | Netlist-generated circuit models; hosts NAM and RTNeural; headless Raspberry Pi mode; online preset sharing | Idea only. "Headless" suggests nothing new for a plugin. |
| **Airwindows** ([repo](https://github.com/airwindows/airwindows)) | MIT | Hundreds of small, cheap, high-quality processors (console, tape, reverbs such as Galactic) | A **reusable source of cheap, good-sounding post-effects** with MIT code. For example, a tape-echo saturation stage, or Galactic as an "ambient" reverb, for well under `performance-budget.md`'s 0.5-unit pedal cap. Check each algorithm's quality one by one. |
| **LSP plugins** ([repo](https://github.com/sadko4u/lsp-plugins)) | LGPL-3/GPL-3 | Room Builder (IRs from a 3D room model), IR loaders | Idea for draggable mic placement: compute early reflections from mic and source positions in a simple room model, instead of switching fixed IRs. It pairs with STK's IR-per-position approach. |
| **Rakarrack, Tunefish, Helm, TAL-NoiseMaker, Yoshimi** | GPL | Surveyed | Nothing that Luthier's effects, modulation or preset specs lack. Skipped. |

### 2.4 Modulation and performance

| Project | Licence | Notable | Verdict |
|---|---|---|---|
| **Surge XT** ([repo](https://github.com/surge-synthesizer/surge)) | GPL | Modulation List (Alt+M); CLAP polyphonic modulation; MPE; MSEG editor; full screen-reader support, including an "enable all recommended accessibility features" menu item; Shift+F10 context menus | #6, #9. The one-click accessibility preset is a cheap add to `accessibility.md`. The **MSEG** (multi-segment envelope with a drawing editor) is worth considering as an LFO/EG shape in `modulation-matrix.md`, for scripted swells and volume-knob "violining". |
| **Vital** ([repo](https://github.com/mtytel/vital)) | GPL (commercial licence available) | Modulation preview before commit, animated modulation on every target, per-route curve remap | #6. **Per-route response curves** (power/curve per mod route) are missing from `modulation-matrix.md` 3 as far as its headings show. Effort S. |
| **Odin 2** ([repo](https://github.com/TheWaveWarden/odin2)) | GPL | Drag-and-drop modulation, CLAP, MPE, arpeggiator | Nothing new beyond Surge and Vital. |
| **Dexed** ([repo](https://github.com/asb2m10/dexed)) | GPL (engine Apache-2.0) | Cartridge (bank) browser, SysEx, engine resolution variants, MTS-ESP, MPE, keyboard-accessible scalable UI | #7, #8. A cartridge-style **"pack" browser** is a pattern for content update packs (`factory-content.md` 11). |
| **MTS-ESP / tuning-library** | 0BSD / MIT | Global microtuning | #8. |
| **Magenta GrooVAE** | Apache-2.0 | Humanise / groove transfer | #13. |

### 2.5 Presets and content

| Idea | Source | Verdict |
|---|---|---|
| Type-ahead search over name, tags and author; favourites; folders | Vital, Surge, Odin | Already in the preset-browser work. Make sure **author** and **"more from this pack"** are search facets (`file-formats.md` already stores `author` and `tags`). |
| "Similar sounds" | none of the OSS synths do it well | Luthier knows each preset's physical fingerprint (guitar, pickups, amp gain, effects). A cosine similarity over those fields gives "find similar" nearly for free. S. |
| Community model and IR libraries | NAM / TONE3000, Proteus ToneLibrary | With #2, Luthier becomes a host for a very large free ecosystem. That is marketing value. |
| Pack or cartridge import | Dexed | See 2.4. |

### 2.6 UI, UX and visualisation

| Idea | Source | Verdict |
|---|---|---|
| Animated inner workings (live waveforms on modules) | Vital | Clashes with `proposals/visual-polish.md` rule 4 ("No motion"). The exception being written for animated strings is the right, limited dose. Recommend only **live modulation dots on arcs** (#6), which are "already live" data. |
| Modulation list overlay | Surge | #6. |
| Keyswitch labels on the keyboard | sfizz / SFZ `sw_label` | #5. |
| One-click accessibility preset | Surge | Add to `accessibility.md`. S. |
| Skins / user themes | Surge, Vital | Low value. Luthier's single guitar-shop theme is a brand asset. Skip. |
| Note-highway practice view | [YARG](https://github.com/YARC-Official/YARG) (LGPL, idea only) | For the practice drawer: a scrolling tab "highway" with section loop and a speed trainer that steps up the tempo after N clean passes. It reuses the tab reader and the new piano roll's scrolling. The fun factor for younger players is high. M. |
| Tab rendering and cursor sync | [alphaTab](https://github.com/CoderLine/alphaTab) (MPL-2.0), [TuxGuitar](https://github.com/helge17/tuxguitar) (LGPL), MuseScore (GPL) | Use them as format references for **Guitar Pro import** into the tab reader (`practice-tools.md` 6). Luthier exports GP (`notation-export.md`), and import closes the loop. Write your own parser, using alphaTab's documented behaviour as a reference only. |

### 2.7 Practice and composition

| Idea | Source | Verdict |
|---|---|---|
| Audio to tab | Basic Pitch, NeuralNote, FretNet | #4. |
| Stem removal | demucs.cpp | #14. |
| Trading fours and lick grammar | Impro-Visor | #12. |
| Looper EDP functions: **multiply** (extend a loop by whole cycles), **insert**, **substitute** (replace while hearing the old layer), round mode (actions end at the loop boundary) | [SooperLooper](https://github.com/essej/sooperlooper) (GPL, idea only) | `practice-tools.md` 2 has overdub, replace, reverse and half-speed. Multiply and round mode are what looper players expect. S. |
| Groove templates | GrooVAE / groove pools | #13. |

### 2.8 Engineering, QA and CPU

| Idea | Source | Verdict |
|---|---|---|
| Voice-count LOD and silence bypass | Rings, NAM-LV2 | #7. |
| Per-model benchmark binary in CI (`benchmodel`) | NAM Core | Add a `LuthierBench` target that renders each factory preset offline and fails CI when a module exceeds its `performance-budget.md` units by more than 10%. S. |
| Parse-check tools for every user file format | tuning-library's `parsecheck` | A small CLI that loads every `.luthier*` file in a folder and reports schema errors. Good for `file-formats.md` migration tests and support tickets. S. |
| Soft clip inside the loop as an energy guard | Surge String | A secondary guard for feedback, E-Bow and coupling (2.1). |
| Crossfade to infinite decay | Rings | A clean freeze and E-Bow sustain (2.1). |
| Commuted-synthesis eco body | STK | 2.1. |

---

## 3. What Luthier already does better

For reassurance. Across these projects, nothing comes close on:

- **Breadth of articulation and technique interaction.** The technique
  cascade, strum crossing velocity, fingerstyle contact profiles, slap,
  tapping, scrape, squeak and buzz. The open strummers only delay notes, and
  Rings, STK and Faust have one exciter each.
- **The guitar as physical parts.** The Workshop bill of parts, part
  acoustics and "physical deltas only" have no open-source equivalent.
  mesh2faust is the closest in spirit, and it is a research tool.
- **Signal-chain realism before the amp.** `GuitarCircuit` pot, cap and
  cable loading, the noise floor, string ageing, environment and tuning
  stability. Open amp sims start at the input jack.
- **Honest ranges.** Stock and advanced ranges with a padlock are not found
  anywhere else.
- **Product engineering.** An undo taxonomy, a state-model intersection
  matrix, per-host quirks, error-recovery responses and per-module CPU
  budgets are specified more rigorously than in any surveyed project. Surge
  is the only one close, on accessibility.
- **Composition and practice in one instrument.** Tune Builder, the rhythm
  engine, practice tools and MIDI/notation export. The open world splits
  these across TuxGuitar, Impro-Visor and SooperLooper.

---

## 4. Recommended next specs

Each would be a new spec file, or a delta to one, following the house style
(ground rules, physics or behaviour, engine insertion, UI location in
`gui-integration.md` 19, file format, tests).

1. **`audio-excitation.md` (#1).** Adds `ExcitationSource::Audio` and a
   Resonator mode. Covers: sidechain input conditioning (HPF, gate,
   transient shaper); per-string routing from a hex input versus pitch-split
   routing from mono; onset-to-strum events for auto-articulation;
   resonator-mode chord and drone tables, with Rings' tables as a reference;
   energy safety (coupling clamp plus an in-loop soft clip); a
   zero-latency guarantee. UI: an INPUT pill on the Playing strip and a
   Resonator page in Col 4. Tests: no runaway with white noise at 0 dBFS
   for 60 s, and the pluck-from-audio onset is within 1 ms.
2. **`capture-amp.md` (#2).** Adds `AmpEngine::Mode::Capture`. Covers:
   `.nam` A1/A2 and RTNeural `.json` loading on a worker thread; validating
   metadata; `GuitarCircuit` volts to `input_level_dbu` calibration;
   loudness normalisation; a quality tier linked to the #7 governor; cab
   bypass for full-rig captures; the capture-and-train workflow through
   `tone-match.md` 4; error responses; CPU budget rows; licence notices.
3. **`dual-polarisation.md` (a delta to `sustain-and-decay.md` 3 and
   `body-coupling.md` 2).** Two waveguides per string, a
   `pluck_angle` from pick angle and stroke direction, a 2x2 admittance per
   string, and the eco fallback to the current approximation. Tests: beat
   rate in 0.2-2 Hz on a detuned-polarisation fixture; the knee matches
   SUS-02 within 1 dB; up-strum and down-strum spectra differ.
4. **`articulation-maps.md` (#5).** One `ArticulationTable` gathered from
   all the technique specs; exporters for Cubase, Logic, Studio One and
   REAPER; keyboard and piano-roll labels; momentary, latched and
   previous-note semantics. Tests: round-trip each export format, and no
   keyswitch collisions (it absorbs the collision checks already in
   `fingerstyle-attack.md`).
5. **`audio-to-tab.md` (#4, with #14 as a phase 2).** An offline
   transcription job model: runtime choice (RTNeural or ONNX), Basic Pitch
   integration, bend extraction to `microtonal-bends.md` events, fingering
   through `ChordVoicer`, and output to the riff library, tab and Tune
   Builder. Phase 2 adds stem separation for backing tracks, with a licence
   gate on the weights.
6. **`cpu-lod.md` (a delta to `performance-budget.md`, #7).** A per-module
   LOD table, the governor's inputs and hysteresis, switch-at-quiet rules,
   the silence-sleep threshold, full detail for offline renders, and a
   `LuthierBench` CI gate.
7. **`microtuning.md` (#8).** `TuningEngine` as the one owner of the
   frequency table; MTS-ESP and SCL/KBM precedence; a fret-system part and
   its illustration; interaction with capo, bends and buzz.
8. **`modulation-list.md` (a GUI delta, #6 plus Vital's per-route
   curves).** The overlay layout, preview-before-commit through shadow
   audition, source-hover highlighting, per-route curve, keyboard and
   screen-reader behaviour.
9. **`clap-expressions.md` (a delta to `host-integration.md`, #9).** Note ID
   to string mapping, polyphonic modulation destinations, note expressions,
   remote-control pages, and the Bitwig test plan.
10. **`body-modal-library.md` (#10).** The offline FEM pipeline in `Tools/`,
    the JSON bank schema, the interpolation rule, calibration against
    measured bodies, and a Workshop spectrum-delta test.

Smaller items that can go straight into existing specs: coupled courses and
the curved-bridge sitar (`guitar-workshop.md`, `fret-buzz.md` 5, #15), the
bow exciter (`Excitation`), looper multiply and round mode
(`practice-tools.md` 2), the one-click accessibility preset
(`accessibility.md`), "similar sounds" in the preset browser, Guitar Pro
import in the tab reader, and the note-highway practice view.
