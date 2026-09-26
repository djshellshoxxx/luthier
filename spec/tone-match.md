# TONE MATCH SPEC: user IRs, cab match, EQ match, capture

Extends spec.md's cabinet and body sections. Turns the "engine loads any WAV"
sentence from README.md into a first-class user-facing feature, and adds two
matching tools that fit the modelling paradigm without turning Luthier into a
sample-based product.

## 0. Ground rules

1. User IRs are files on disk, loaded on the message thread, handed to the
   audio thread by pointer swap under a lock-free FIFO.
2. All IRs are resampled to the current host sample rate before the swap.
   Resampling uses windowed-sinc, minimum length 512 taps.
3. IRs longer than `max_ir_seconds` (default 4 s) are truncated on load.
4. Convolution latency for user IRs matches the built-in cabinet
   convolution latency; longer user IRs do not introduce additional
   reported latency.
5. Match analysis (cab match, EQ match) runs entirely on a worker thread.
   The audio thread never processes reference audio.

## 1. User IR loader

Two slots per convolution point:
- **Body IR slot**: replaces the body engine's IR when engaged.
- **Cabinet IR slots (2)**: replace mic 1 and mic 2 IR respectively.
  (mic-placement.md, FEAT-MIC): an engaged user cabinet IR bypasses that
  mic's placement stage (the IR already is a placement); the placement view
  shows the user IR's name instead of the handle. The factory non-anchor
  cabinet IRs remain browsable here but are no longer auto-loaded by the
  mic position.

Each slot exposes:
- File path (any WAV, AIFF, FLAC, up to 6 channels).
- Channel selector (which channel of a multi-channel IR to use, plus
  "auto-sum to mono").
- Gain trim (-24 to +24 dB).
- Length trim (start offset in samples, end trim in samples).
- Predelay (0-100 ms, useful for cab IRs with unwanted pre-ringing).
- Reverse toggle.
- Mix (0-100%) between the built-in model and the user IR.

File browser under `~/Documents/Luthier/IRs/` with drag-and-drop from OS.
Recent-IRs list of last 20 files.

## 2. Cab match

**Purpose**: capture the sonic signature of a real cabinet (or another
plugin's cabinet) as an IR, using the same amp signal both sides.

Workflow:
1. User selects "Cab Match" in the routing panel.
2. Plugin sends a test signal from its DI output (Aux 1) to the reference
   rig (external amp + cab miked, or another plugin).
3. User records the reference return into Luthier's sidechain input.
4. Plugin also records its own internal amp output through its current cab
   model.
5. Match algorithm computes an IR that, when convolved with Luthier's amp
   output, best approximates the reference return.

Test signals:
- Exponential sine sweep, 20 Hz to 20 kHz, 6 seconds. Preferred, deconvolves
  cleanly.
- MLS (maximum length sequence), 4 seconds. Fallback if sweep introduces
  amp-dependent artefacts.
- Guitar-like transient burst library, for hybrid matching that biases the
  IR toward guitar-relevant frequencies.

Algorithm:
- Deconvolve reference return by the test signal to get the reference IR.
- Trim to `max_ir_seconds`, window with a Hann fade-out on the last 5% of
  samples.
- Save as WAV to `~/Documents/Luthier/IRs/Cab Match/<name>.wav`.
- Auto-load into the current cabinet IR slot.

UI shows a progress bar, an estimated tail length, and a null-test result
(main-out through matched IR vs. reference input, RMS null in dB).

## 3. EQ match

**Purpose**: match Luthier's tone to a reference recording without capturing
an IR. Useful when the reference is a mix, not an isolated cab.

Workflow:
1. User drags an audio file into the EQ Match panel, or loops a section of
   the sidechain input.
2. User records the same passage played through Luthier.
3. Algorithm computes long-term magnitude spectra of both, fits a
   min-phase FIR of user-selected length (256/1024/4096 taps) that shapes
   Luthier's spectrum to match the reference.

Options:
- Match band: full-range or user-selected (e.g., 200 Hz to 6 kHz).
- Aggressiveness: 0-100%, weights the correction magnitude.
- Preserve dynamics: skips broadband gain matching, corrects only spectral
  shape.

Output is a filter applied at a user-chosen position in the signal path:
pre-amp, post-amp, post-master. Filter can be saved and recalled per
preset.

Not a substitute for cab match, and clearly labelled as such. EQ match
does not reproduce time-domain behaviour (reflections, cab ringing).

## 4. Capture

Capture is the raw record path used by Cab Match and EQ Match, but also
exposed as a general utility:

- Record from any input: main out, DI, sidechain, per-string.
- Record length: 100 ms to 60 s.
- Output: WAV 32-bit float, saved to `~/Documents/Luthier/Captures/`.
- Autotrim silence at start and end (user-toggleable).

## 5. IR library organization

Folder convention:
```
~/Documents/Luthier/IRs/
  Bodies/
    Acoustic/
    Electric/
  Cabinets/
    User/
    Cab Match/
  Rooms/
  Special/
```

Metadata sidecar `.json` alongside each IR:
```json
{
  "name": "Marshall 4x12 Greenback SM57 On-Axis",
  "type": "cabinet",
  "sample_rate": 48000,
  "length_ms": 500,
  "author": "user",
  "tags": ["marshall", "greenback", "sm57"],
  "notes": "captured with Luthier cab match, 2026-04-12"
}
```

Sidecar is optional. Missing sidecars fall back to filename parsing.

## 6. UI

New panel, `TONE MATCH`, sharing the Column 4 tab strip.
Contents:
- IR slot editors (body, cab 1, cab 2), each a card with the parameters
  from section 1.
- Cab Match wizard button, opens a step-by-step guide inside the panel.
- EQ Match wizard button, similar.
- Capture utility as a small pane below the wizards.
- IR library browser with tag filter and search.

## 7. Preset integration

Presets store IR references by relative path when the IR lives under a
user IR folder registered in Options, and by absolute path otherwise.
Missing IRs fall back to the built-in model with a warning banner in the
plugin header.

## 8. Tests

- IR loader: load 100 IRs of varying length and sample rate, verify
  correct resampling (impulse-in, verify FFT magnitude within 0.5 dB of
  original).
- Cab match sweep deconvolution: known IR -> convolve with sweep ->
  deconvolve -> compare recovered IR to source, verify within -60 dBFS
  null.
- EQ match: spectrum fit for known EQ curves (shelf, bell, notch) within
  1 dB across the fit band.
- Capture: 60 s capture from main out matches offline render of the same
  passage within -80 dBFS null.
- Sample-rate change during IR playback: verify no clicks and IRs are
  re-resampled correctly.
