# 03 DI capture and re-amp from file

STG-11. Source roadmap: 3.7.

**Summary.** One-button **Capture DI** writes the Aux 1 DI signal to a WAV beside
the preset; **Re-amp from file** plays it back through the rig.

**Status.** Partial. The routes are cited as existing (`spec/routing-io.md` 2 Aux 1,
5 re-amp). Two caveats found in review:
- No re-amp symbol was found in `Source/` (case-insensitive search for `re-amp`,
  `reamp`, `re_amp`). The roadmap's "re-amp symbols in `Parameters.cpp` and
  `PluginProcessor.cpp`" is not confirmed. Treat re-amp as **to verify, else build**.
- `CaptureRanges` (`Source/UI/CaptureRanges.{h,cpp}`) is a range helper for tone
  match, not a DI recorder.
- The one-button capture file does not exist.

## User-facing behaviour
- **Capture DI** button (transport/stage panel). Records from Aux 1 until stopped,
  or for a set length. Writes `~/Documents/Luthier/Captures/<preset>_<yyyymmdd-hhmmss>.wav`.
- **Re-amp from file** loads a capture. Preset shows the file name.
- Missing capture file on preset load: preset loads, a notice shows, re-amp is
  disabled for that preset.

## Engine / DSP
- Capture (write path):
  - Audio thread copies the Aux 1 block into a preallocated SPSC FIFO
    (1 s of audio at the host rate, float). No locks, no allocation.
  - A background writer drains the FIFO into a WAV writer (32-bit float, mono,
    host sample rate, recorded in the header).
  - If the FIFO overruns, samples are dropped and the count is shown in the UI.
    Capture state reports `dropped`.
- Re-amp (read path):
  - Load the file on a background thread into a prefetch ring (2 s).
  - Sample-rate mismatch: convert once at load time with the codebase's offline
    resampler, never on the audio thread.
  - Audio thread reads the ring and feeds Aux 1's input to the rig in place of the
    live DI. The live input is muted while re-amp is active.
- Internal math in double; file in float (32-bit is lossless for the rig's input).
- `reset()` clears both FIFOs and stops writes.

## Data model and parameters
- No automatable parameters. Capture and re-amp are commands and transport state.
- Preset stage block (see State): `di_capture_file` (string, leaf name only),
  `reamp_enabled` (bool).

## State / file format / migration
- Capture files are separate WAVs, referenced by leaf name in the preset's stage block.
  The preset is not embedding audio.
- Files are never deleted by the plugin.
- Presets without the stage block: no capture, re-amp off.

## Edition gating
- Free (roadmap 6).

## Performance budget
- Audio thread: one block copy per block when capturing (about 0.005 units, D).
- Writer and prefetch: off the audio thread.

## Test plan
- **STG-11.** Run the rig with a fixed test DI (seed fixed) live, and record the
  Aux 1 DI for the same 5 s. Re-amp the capture through the same settings.
  Assert RMS of the re-amp output over the steady 5 s segment equals the live
  pass within 0.1 dB, and the sample difference is below -60 dBFS RMS
  (float round-trip only).
- Assert `dropped == 0` in a 60 s capture on the test host.
- Assert a missing file loads the preset with the notice and re-amp disabled.

## Effort and dependencies
- **ED 4.**
- Depends on: `spec/routing-io.md` (Aux 1, re-amp); `spec/tone-match.md`
  (`CaptureRanges`, unrelated but near); `spec/practice-tools.md` 10 (data-location
  pattern); `spec/file-formats.md` (stage block).
