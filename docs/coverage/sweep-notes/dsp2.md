# SPEC-SWEEP phase 2 notes: dsp2

- [SQ-8] A held note re-voiced to another fret (key still down, finger never lifted) is still struck, but also squeaks through `PlayingNoise::onShift` (`NoteOnEvent::shiftFromFret`, set in `MidiInterpreter::emitVoicedNote`). This covers ChordVoicer revoices and guitar-controller re-fretting. Imported Luthier SLIDE events still do not reach the engine: import renders channel-voice messages only. That part is deferred with BT-12, which needs the same Luthier-event replay path.
- [SQ-27] Fret wear scales squeak roughness by 1 + 0.15 x wear(fret) x character amount, the same amount scaling every other character-wear effect uses.
- [FB-21] A worn crown sits wear x character amount x 0.4 mm below the board line (`SetupGeometry::fretWearMm`). The 12th-fret action is measured against the board line, so wear does not move the saddle.
- [PT-6] The metronome follows the plugin's effective tempo (host, or tap while the host is stopped) by default. Typing a tempo, starting a ramp, a routine or speed-trainer tempo, or the latency wizard turns following off. Sessions saved before the field existed load with following off, so they keep their saved tempo.
- [PT-24] MP3 uses JUCE's own decoder (`JUCE_USE_MP3AUDIOFORMAT=1`), which is part of juce_audio_formats, rather than dr_mp3. The spec's intent, no LGPL dependency, still holds.
- [PT-28/29] Pitch and tempo shift use an in-house WSOLA stretcher with grains read at the pitch ratio (`Practice/TimePitchShifter`). It runs on the audio thread, allocation-free after prepare, and is bypassed at neutral settings.
- [PT-38] The fifteenth progression is I-IV-vi-V.
- [PT-39] The ear trainer played every note on string i at fret (note mod 24), which is the wrong pitch. Each note now goes on the lowest free string and fret that sounds it (`EarTab::placeOnStrings`). Note offsets are still ignored: the notes sound together.
- [HI-10] A mono main output is refused, per host-integration 2 and DECISIONS C-26.
- [HI-2/HI-31] `NEEDS_MIDI_OUTPUT TRUE`, which matches `producesMidi()`.
- [TM-6] The body IR slot runs inside the engine, in place of the body model's response, and blends against it by its mix. It is mono and hears the same excitation the body model hears.
- [TM-17] Cab Match plays its test signal on Aux 1 (DI), or on the main out when there is no aux bus, and starts the reference capture in the same block.
- [BT-6] slap/pop position join the buzz range family. The stock range is the declared 5-400 mm, because the spec's 20-200 and 10-150 would re-map saved values. Advanced is 2-800 mm.
- [SG-10] Slide pressure above 0.8 chokes the note: at 1.0 up to 35 % comes off the sustain scale.
- [HI-29] The host time signature reaches the practice metronome while it follows. The rhythm engine is step-based and has no metre, so that part is left open.
- [TM-5] Cab/EQ match analysis runs on a juce::Thread::launch worker, and only the saved file and the result text come back through callAsync. Cancelling (restart) drops a result that is still on its way.
