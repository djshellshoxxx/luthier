# ENGINE SPEC: Luthier — the complete DSP engine, defined so Claude Code cannot get it wrong

## 0. Ground rules Claude Code must follow through the entire engine

Read these before writing a line of code.

1. **All DSP math is double-precision (`double`) internally.** Convert to float only at the final output stage. Single-precision accumulates error too fast in the feedback loops used here (waveguides, resonators, coupling matrices).
2. **All buffers are pre-allocated in `prepareToPlay()`.** No `new`, `malloc`, `std::vector::resize`, `std::string` construction, or file I/O anywhere in `processBlock()`. This is not a style preference; violating it causes glitches in real DAWs.
3. **Every recursive stage (waveguide, resonator, feedback filter) has a DC blocker at 5-10 Hz on its output** and a NaN/Inf guard clamping to `[-4.0, 4.0]`. Every one. No exceptions.
4. **Every parameter that affects DSP is smoothed** with `SmoothedValue<double, ValueSmoothingTypes::Linear>` over 20 ms unless specified otherwise. Filter cutoffs use exponential smoothing over 30 ms. Discrete choices (filter type, pickup selection) crossfade over 5 ms between old and new coefficients.
5. **All sample-rate-dependent computation lives in `prepareToPlay()`.** No hardcoded 44100 or 48000 anywhere. Recompute on rate change; the plugin must survive a host switching from 44.1 k to 96 k mid-session.
6. **All time-based parameters are stored in seconds or Hz, never in samples.** Convert to samples per-block using the current sample rate. This makes preset compatibility trivial across sample rates.
7. **Denormals off at the top of every `processBlock()`** via `juce::ScopedNoDenormals`. Waveguide feedback loops manufacture denormals faster than any other DSP structure; without this, CPU spikes silently to 100%.
8. **All modules expose a `reset()` method** that clears all state (delay lines, filter states, envelope followers, coupling matrix, feedback accumulators). Called on transport start, preset load, and panic.
9. **All modules are unit-testable in isolation.** Every DSP class takes its sample rate and max block size in `prepareToPlay`, produces output from input via `processBlock`, and holds no global state. Tests feed known signals and verify known outputs.
10. **The engine runs at a fixed internal oversampling ratio for nonlinear stages** (amp, drive, waveshapers): default 4x, user-adjustable to 2x or 8x. Everything else runs at host rate. Aliasing on drive/distortion stages is the single biggest failure mode of amateur guitar plugins.

If any of these rules conflict with a piece of "clever" code, the rule wins. Rewrite the clever code.

## 1. Top-level engine architecture

```
[MIDI input from host]
        ↓
[MIDI Interpreter] → parses notes, CCs, MPE, pitch-bend into per-string events
        ↓
[Playing Technique Engine] → decides articulation per note (pluck, hammer-on, slide, bend, palm mute, harmonic, tap)
        ↓
[Tuning Engine] → converts note+string+fret into a target frequency in Hz for each string
        ↓
[String Engine × N] → one waveguide per string (6 for standard, up to 12), running in parallel
        ↓                    ↑ (sympathetic coupling matrix reads and writes across all strings)
        ↓
[String Sum Bus]
        ↓
[Body Engine] → convolution or modal synthesis on the summed string signal
        ↓
[Pickup Engine] (electric/electric-acoustic only) → applies pickup transfer function; multiple pickups blend here
        ↓
[Instrument Output Bus] → this is the "raw guitar" signal, mono
        ↓
[Cable Simulation] → optional capacitance roll-off
        ↓
[Pre-Amp Effects Chain] → compressor, wah, envelope filter, octaver, pitch shifter, drive/OD/fuzz pedals
        ↓
[Amp Engine] → preamp tube stages, tone stack, phase inverter, power amp, output transformer
        ↓
[Post-Amp Effects Chain (loop)] → modulation, delay, reverb
        ↓
[Cabinet + Microphone Engine] → speaker IR + mic IR + mic placement
        ↓
[Room Engine] → early reflections + ambient reverb
        ↓
[Master Bus] → master gain, safety limiter at -0.3 dBFS
        ↓
[Output]
```

Each block is a self-contained module. Data flows one direction. The **only** back-flow is the sympathetic coupling matrix, which reads all string states each block and adds a coupling signal back into each string's delay line before that string processes its next sample.

## 2. MIDI Interpreter (module: `MidiInterpreter`)

**Input**: raw `MidiBuffer` from the host each block.
**Output**: a queue of typed events routed to specific engine modules.

Event types:
- `NoteOnEvent { stringIndex, pitchHz, velocity, technique, timestampSamples }`
- `NoteOffEvent { stringIndex, timestampSamples }`
- `PitchBendEvent { stringIndex, semitones, timestampSamples }` (per-string for MPE, global for standard MIDI)
- `PressureEvent { stringIndex, value, timestampSamples }` (aftertouch)
- `CCEvent { ccNumber, value, timestampSamples }`
- `SustainPedalEvent { on, timestampSamples }`
- `WhammyEvent { semitones, timestampSamples }` (from whammy pedal MIDI or user-mapped CC)

**MIDI modes** (selectable per preset):

**Mode A: Mono/Single-string**
- All incoming notes are routed to a single string (the "active string").
- Active string changes based on pitch: use the algorithm below.
- Overlapping notes on the same string trigger the Playing Technique Engine's legato detection.

**Mode B: Polyphonic/Chord**
- Incoming notes are voiced across strings by the Chord Voicing Engine.
- Standard MIDI (channel 1) with multiple simultaneous notes is treated as a chord.
- The voicing engine finds a playable fingering and assigns each note to a string.

**Mode C: Guitar Controller (MPE or per-channel)**
- MIDI channel 1 = high E (string index 0), channel 2 = B (index 1), etc.
- Or MPE: each note gets its own channel; the plugin infers the string from note pitch and assignment rules.
- Per-string pitch bend and pressure supported natively.

**String assignment algorithm** (Mode A and Mode B fallback):

Given a target pitch in Hz, find the string whose open pitch is below the target and closest to it, subject to constraints:
- Fret position must be between 0 and `maxFrets` for that string.
- If the pitch is above the highest note of the highest string, assign to the highest string and clip to the highest fret.
- If multiple strings can play the pitch, prefer the string closest to the currently-active string (to minimize hand movement in Mono mode) or the string that produces the "sweetest" voicing in Poly mode (fewer open strings, tighter voicings preferred).

Pseudocode:
```cpp
int assignString(double pitchHz, const std::array<double, N_STRINGS>& openPitches, int maxFrets) {
    int best = -1;
    double bestSemitonesUp = 1e9;
    for (int i = 0; i < N_STRINGS; ++i) {
        double semitonesUp = 12.0 * std::log2(pitchHz / openPitches[i]);
        if (semitonesUp < 0 || semitonesUp > maxFrets) continue;
        if (semitonesUp < bestSemitonesUp) {
            bestSemitonesUp = semitonesUp;
            best = i;
        }
    }
    return best; // returns -1 if no string can play this pitch
}
```

**CC assignments** (default map, user-remappable):
- CC 1 (mod wheel): vibrato depth
- CC 2 (breath): whammy bar
- CC 4 (foot): expression
- CC 11 (expression): master output level
- CC 64 (sustain): let all strings ring
- CC 65 (portamento): slide mode toggle
- CC 66 (sostenuto): hold current notes
- CC 67 (soft): palm mute amount
- CC 70-79: user-mappable
- Aftertouch: vibrato depth (default) or bend (user choice)

## 3. Tuning Engine (module: `TuningEngine`)

**Purpose**: converts a `(stringIndex, fretPosition, pitchBendCents)` tuple into a target frequency in Hz for the string engine.

**Input**:
- `stringIndex` (0 to N-1)
- `fretPosition` (double, 0.0 for open string, fractional values allowed for fretless and bends)
- `pitchBendCents` (double, additional pitch offset in cents)

**Output**:
- `targetFrequencyHz` (double)

**Configuration** (per string):
- `openStringFrequencyHz` (double, the fundamental when open)
- `detuneCents` (double, per-string intentional detune from ideal)
- `intonationErrorSlope` (double, cents per fret; models how real guitars go slightly sharp as you fret higher)
- `temperament` (enum: EqualTemp12, JustIntonation, Meantone, Werckmeister3, Kirnberger3, Custom)

**Standard tuning frequencies (Hz)**:
- String 0 (high E): 329.628 (E4)
- String 1 (B): 246.942 (B3)
- String 2 (G): 195.998 (G3)
- String 3 (D): 146.832 (D3)
- String 4 (A): 110.000 (A2)
- String 5 (low E): 82.407 (E2)

For 7-string add: string 6 (low B) = 61.735 (B1).
For 8-string add: string 7 (low F#) = 46.249 (F#1).

**Pitch computation for a fretted note (equal temperament)**:
```
f_target = f_open * 2^((fretPosition + pitchBendCents/100 + detuneCents/100 + intonationErrorSlope*fretPosition/100) / 12)
```

**For other temperaments**: use a lookup table of 12 pitch ratios per octave, then apply octave multiplication for higher frets. Custom temperament stored as 12 doubles in the preset.

**Alternate tunings** (per-string open frequency overrides):
- Drop D: string 5 → 73.416 (D2)
- Drop C: string 5 → 65.406 (C2)
- DADGAD: strings [3,4,5] = [D3, A2, D2], plus normal high E→D, B→A, G→G
- Open G: [D3, G3, D3, G2, B2, D2] mapped appropriately
- Half-step down: multiply every string frequency by `2^(-1/12)`
- Full-step down: multiply by `2^(-2/12)`

**Custom tuning**: user sets any note+cents offset per string. Stored in preset as an array of doubles.

**Detuning behavior**:
- **Manual detune**: user-set per string, ±100 cents, applied as constant offset.
- **Realism detune**: on preset load or user request, randomize each string by ±(0 to 20) cents. Stored in preset so it persists.
- **Drift over time**: optional, models a guitar going slowly out of tune. Off by default. When on, each string drifts by a small random amount every 30 seconds of playback (up to ±5 cents).

**Intonation error**: real guitars go progressively sharp as you fret higher because pressing down stretches the string slightly. Model with `intonationErrorSlope` (default 0.3 cents per fret). At fret 12, that's an extra 3.6 cents. Adjustable per-string.

## 4. Playing Technique Engine (module: `TechniqueEngine`)

**Purpose**: decides what kind of note event to send to the string engine based on the MIDI stream and context.

**Techniques**:
- `Pluck`: normal note-on with pick or finger.
- `HammerOn`: same string, previous note still ringing, pitch higher, velocity low-to-medium.
- `PullOff`: same string, previous note still ringing, pitch lower, velocity low-to-medium.
- `Slide`: same string, previous note ringing, either pitch, sustain pedal or CC 65 active OR very fast transition (< 40 ms).
- `Bend`: pitch-bend message > 0 on a sustained note; string tension modulates.
- `Vibrato`: cyclic pitch-bend or aftertouch driving a vibrato modulator.
- `PalmMute`: CC 67 active OR user's mapped mute CC above threshold; note plays with heavy damping.
- `NaturalHarmonic`: user-mapped CC or specific velocity range; note plays as harmonic (nulls all partials except integer divisions).
- `PinchHarmonic`: high velocity + specific technique CC; excites a specific harmonic.
- `Tap`: specific technique CC + note; simulates two-handed tap.
- `SlideGuitar`: bottleneck mode toggle; every note played with continuous pitch and slide characteristics.
- `Strum`: multi-note chord in Poly mode; notes staggered across time by strum-speed parameter.

**Technique detection logic (per new note event)**:
```
if PalmMuteCC > threshold:
    technique = PalmMute
elif PinchHarmonicTrigger active:
    technique = PinchHarmonic
elif NaturalHarmonicTrigger active OR velocity in harmonic range:
    technique = NaturalHarmonic
elif TapTrigger active:
    technique = Tap
elif SlideGuitarMode active:
    technique = SlideGuitar
elif previous note on same string still active:
    dt = time since previous note
    if SlideMode active OR dt < 40ms:
        technique = Slide
    elif newPitch > previousPitch AND velocity < 80:
        technique = HammerOn
    elif newPitch < previousPitch AND velocity < 80:
        technique = PullOff
    else:
        technique = Pluck (voice-steal previous)
else:
    technique = Pluck
```

**How each technique translates to string engine parameters**:

- **Pluck**: send `NoteOnEvent` with a fresh excitation impulse. Impulse shape depends on pick vs fingers, pick material, pick position. See String Engine.
- **HammerOn**: do not re-excite the string. Instead, send a `ReExciteEvent` with a light impulse (10-30% of a normal pluck) and update the delay-line length to the new pitch. String state carries over.
- **PullOff**: same as HammerOn but with slightly different excitation spectrum (finger snapping off vs finger landing).
- **Slide**: over the slide duration, ramp the delay-line length from the previous pitch's target length to the new pitch's target length. Add a small filtered-noise excitation proportional to slide speed (string noise from finger movement on wound strings).
- **Bend**: modulate the delay-line length target continuously as pitch-bend messages arrive, using smoothing.
- **Vibrato**: apply a low-frequency modulation to the delay-line length. Rate and depth from CC/aftertouch.
- **PalmMute**: reduce the string's damping filter cutoff dramatically (from ~5 kHz open to ~800 Hz muted) and increase feedback attenuation (shortens decay time).
- **NaturalHarmonic**: replace the excitation with a filtered impulse that primarily excites the harmonic at position `fret/12` of string length. Also shorten the delay line effectively so that only that harmonic sustains.
- **PinchHarmonic**: excite with a filtered impulse targeting a chosen harmonic (usually 2nd, 3rd, or 4th).
- **Tap**: same as HammerOn but with a slightly harder excitation.
- **SlideGuitar**: disable fret quantization; all pitches are continuous. Excitation has a slightly softer attack. Add continuous small pitch modulation (bottleneck vibrato).
- **Strum**: for each note in the chord, offset the note-on timestamp by `(stringIndex * strumDelayMs)` for downstrum, reversed for upstrum. Add strum-direction velocity curve (up-strums are typically lighter).

## 5. String Engine (module: `StringEngine`)

**One instance per string.** This is the heart of the plugin.

### 5.1 Signal flow within one string
```
[Excitation Impulse] ──→ [+] ──→ [Delay Line (fractional)] ──→ [Loop Filter] ──→ [Loss Filter] ──→ [DC Blocker] ──→ [NaN Guard] ──┬──→ output
                         ↑                                                                                                          │
                         └──────────────────────── feedback ────────────────────────────────────────────────────────────────────────┘
                         ↑
                         └── coupling input from other strings (from Sympathetic Coupling Matrix)
```

### 5.2 Delay line (fractional)

The delay line implements the string as a bidirectional waveguide, but for simplicity and CPU efficiency we use a single lumped delay line with a loop filter. This is the standard extended Karplus-Strong (EKS) formulation.

**Delay length in samples**:
```
delaySamples = sampleRate / targetFrequencyHz
```

`targetFrequencyHz` comes from the Tuning Engine and may change every sample during a bend or slide.

**Fractional delay** required because `delaySamples` is almost never an integer. Use one of:
- **Linear interpolation** (fast, mildly attenuates high frequencies; acceptable).
- **Lagrange interpolation, 3rd or 5th order** (better, preferred).
- **Allpass interpolation** (best for stable pitch modulation during bends/slides; use this).

Recommended: 1st-order allpass fractional delay. The formula:
```
coefficient = (1 - frac) / (1 + frac)   where frac = fractionalPart(delaySamples)
```

The delay line buffer is a circular buffer of size `max(sampleRate / minFrequency) + interpolationTaps + safety_margin`. For a 40 Hz low limit at 96 kHz: 96000/40 = 2400 samples. Round up to next power of two: 4096. That's the buffer size per string.

**Handling pitch changes**:
- When `targetFrequencyHz` changes (bend, slide, vibrato), the delay length must change smoothly.
- Smooth the delay length itself using an exponential smoother with a time constant of about 2 ms.
- Do NOT jump the delay length between blocks or you'll hear clicks.
- During very fast pitch changes (dive-bombs), the delay length changes by large amounts per block; the allpass fractional delay handles this without clicks because it interpolates continuously.

### 5.3 Loop filter (models frequency-dependent decay)

A one-pole lowpass in the feedback loop. Higher frequencies decay faster than lower ones on a real string. The loop filter also determines the overall decay time.

**Filter formula** (one-pole IIR):
```
y[n] = b0 * x[n] + a1 * y[n-1]
where b0 = 1 - dampingCoefficient
      a1 = dampingCoefficient
```

`dampingCoefficient` in [0.0, 0.99]. Higher = brighter, longer sustain. Lower = duller, shorter sustain.

**Per-string tuning of damping**: wound strings (low E, A) get higher damping (brighter loop filter closes slower) because they have thicker cores and more sustain. Plain strings (high B, E) get slightly lower damping.

**Playing technique modulates damping**:
- Normal: `dampingCoefficient` from string material/gauge preset (~0.95 for steel electric, ~0.90 for nylon classical).
- Palm mute: drop to ~0.60.
- Muted picking (light left-hand touch): drop to ~0.75.

### 5.4 Loss filter (models inharmonicity)

Real strings have partials at slightly stretched frequencies:
```
f_n = n * f_0 * sqrt(1 + B * n^2)
```

`B` is the inharmonicity coefficient, small (~0.0001 to 0.001) but audible.

Implement as a **first-order allpass in the feedback loop** whose phase response introduces the stretching. Multiple cascaded allpass filters can approximate this more accurately; one is usually enough for guitar (thin strings are near-perfect; only thick wound strings show much inharmonicity).

**Coefficient formula for allpass**:
```
c = (1 - k * B) / (1 + k * B)
```
where `k` is a tuning constant. For guitar, `k` is around 15-40 depending on desired inharmonicity strength. This allpass is inserted in the feedback loop between the loop filter and the delay line write.

**Per-string inharmonicity values (default)**:
- High E: B = 0.00008
- B: B = 0.00010
- G: B = 0.00015 (wound on some guitars, plain on others; user-settable)
- D: B = 0.00030 (wound)
- A: B = 0.00050 (wound)
- Low E: B = 0.00080 (wound, thickest)

### 5.5 Excitation

Excitation is what gets injected into the delay line to start the string vibrating. Not white noise. A physically-informed impulse.

**Pluck excitation model**:

Generate an impulse of length equal to about the pluck position expressed in samples. The impulse is shaped like a triangle wave whose slope depends on pluck velocity and material.

Steps:
1. Compute `pluckLengthSamples = round(delaySamples * pluckPosition)` where `pluckPosition` is 0.0 at the bridge to 0.5 at the 12th-fret position. Typical: 0.1 to 0.2 for near-bridge, 0.3 to 0.5 for near-neck.
2. Generate a triangle wave of length `pluckLengthSamples` scaled by velocity.
3. Apply an excitation filter based on pick material:
   - Pick (nylon/celluloid/delrin): bandpass around 2-4 kHz, slight resonance.
   - Fingernail: bandpass around 3-5 kHz, sharper.
   - Fingertip (flesh): lowpass around 2 kHz, softer.
   - Thumb: lowpass around 1 kHz, warmest.
4. Sum the shaped excitation into the delay line at all positions in one block (this is standard for lumped-delay-line KS; the position information is baked into the shape rather than the position of insertion).
5. For more accuracy, use "comb-filtering the excitation": subtract a delayed copy of the excitation from itself with delay equal to `2 * pluckLengthSamples`. This creates a spectral notch at the pluck position's node, which is what happens physically.

**Excitation for pick vs fingers vs thumb**: use different filter presets as above, and a small random attack time (0.5-2 ms) to model human variation.

### 5.6 Sympathetic coupling (module: `CouplingMatrix`)

Every string couples to every other string through the bridge. When string A vibrates, string B ("open" or fretted, whichever state it's in) receives a small excitation at frequencies matching its own partials.

**Implementation**: at the top of each processBlock, for each string, sample its current output level. Then, for each other string, add a scaled version of that sample into its delay line's excitation input, filtered through a bandpass at the receiving string's fundamental frequency.

Coupling coefficient between strings i and j is stored in a 6x6 (or N x N) matrix. Default values are around 0.01-0.03 (weak coupling; too much causes runaway or muddiness).

Symmetric: `coupling[i][j] = coupling[j][i]`.

**Safety cap**: total coupling energy per string per block is clamped so runaway is impossible. If sum of coupling inputs exceeds a threshold, scale them down proportionally.

**When a string is muted (palm mute or user-muted)**, it still receives coupling but its damped delay line dissipates the energy quickly.

### 5.7 State reset
`reset()` clears the delay line buffer to zeros, resets the loop filter state, resets the fractional-delay state, resets the DC blocker.

### 5.8 Voice stealing
In Mono mode, when a new note plays on a string that's already ringing, ramp the current string output down over 5 ms, then trigger the new excitation. Never hard-cut.

## 6. Body Engine (module: `BodyEngine`)

**Purpose**: colors the string signal with the resonance of the guitar body.

**Two implementations, user-selectable per preset**:

### 6.1 Convolution mode (default; most accurate)
- Load a body IR (impulse response) for the selected guitar body.
- Convolve the summed string signal with the IR.
- Use **partitioned FFT convolution** (JUCE has `dsp::Convolution`).
- IR length typically 100-500 ms for a guitar body.
- Partition size 128 or 256 for low latency; report latency to host via `setLatencySamples()`.

Body IRs live in `Resources/BodyIRs/<guitarType>/<size>_<wood>_<age>.wav`. Ship 100+ IRs.

### 6.2 Modal synthesis mode (dynamic; responds to parameter changes)
- Bank of 30-50 second-order resonant filters at the guitar body's measured modal frequencies.
- Each mode: frequency, Q, gain.
- Sum of all modal outputs = body response.
- **Modal frequencies scale with body dimensions**: as user adjusts body size, modes shift proportionally.
- Air resonance mode (typically 90-140 Hz for guitar) is one specific resonator.
- Top plate modes, back plate modes, side modes each contribute a subset.

Modal parameters stored per-body in JSON.

**Body engine output** is the input to the Pickup Engine (electric) or directly the "instrument output" (acoustic).

## 7. Pickup Engine (module: `PickupEngine`)

**Purpose**: simulates electric guitar pickups transducing string vibration into an electrical signal.

**Per-pickup parameters**:
- Type (SingleCoil, Humbucker, P90, Piezo, MagneticSoundhole, InternalMic).
- Position along string (0.0 = bridge to 0.5 = midpoint; typical: bridge = 0.13, middle = 0.25, neck = 0.40).
- Coil count (1 or 2).
- Coil spacing (for humbucker; typical 15-20 mm expressed as fraction of string length).
- Coil resistance (kΩ; affects output level and treble roll-off).
- Coil inductance (Henries; affects resonant peak).
- Coil capacitance (pF; affects HF roll-off).
- Magnet type (Alnico2/3/5/Ceramic; each has a stored EQ curve).
- Height above strings (mm; affects gain and treble).

### 7.1 Pickup position filter (comb effect)

A pickup at position `p` of the string samples the string's motion at that point. That means it emphasizes some harmonics and attenuates others based on nodal alignment. Model as:
```
H(w) = sin(w * p * L / c)
```
where `L` is string length and `c` is wave speed. In discrete terms, this becomes a comb filter: `y[n] = x[n] - x[n - pickupDelay]` where `pickupDelay = round(delaySamples * 2 * p)`.

**Result**: partials at `f_n` where `n * p` is an integer are nulled. Bridge pickup (small p) nulls high harmonics only; neck pickup (larger p) nulls lower harmonics. This is why bridge pickups sound bright and neck pickups sound warm.

### 7.2 Pickup electrical model (LCR tank)

A magnetic pickup is an inductor with parasitic capacitance and resistance. The transfer function is a second-order lowpass with a resonant peak.

```
resonantFreq = 1 / (2 * pi * sqrt(L * C))
Q = (1/R) * sqrt(L/C)
```

For typical pickups:
- Single-coil: L=2.5H, C=200pF, R=6kΩ → resonant peak around 7 kHz.
- Humbucker: L=6H, C=180pF, R=8kΩ → resonant peak around 4 kHz.
- P90: L=4H, C=200pF, R=8kΩ → resonant peak around 5-6 kHz.

Implement as a second-order IIR biquad (peak or lowpass with resonance).

### 7.3 Magnet type EQ

Alnico 2, 3, 5, Ceramic each impose slight EQ character. Stored as one-band peaking EQ presets:
- Alnico 2: gentle mid boost around 800 Hz.
- Alnico 3: slight scoop (least colored).
- Alnico 5: bright, more upper mids.
- Ceramic: aggressive high end, tighter low end.

### 7.4 Humbucker (two coils, hum cancellation)

Two single-coil models with opposite polarity, summed. In-phase coils cancel hum (external 60 Hz noise). The coil-position offset produces a comb filter between them, giving the humbucker its characteristic fatter tone.

Coil-tap toggle: outputs one coil only, becomes single-coil-like.

### 7.5 Piezo pickup

Not a magnetic pickup. Samples the bridge vibration directly. Very bright, percussive, and has no comb filter based on pickup position.

Implementation: take the raw string output (before body convolution) or the body output (both work; different sound). Apply a highpass at 40 Hz to remove DC, then a slight lowpass at 15 kHz. Add a small resonance around 3 kHz for the characteristic piezo brightness.

### 7.6 Internal condenser mic

For high-end acoustic-electric guitars. Simulates a small mic inside the body cavity. Warm, woody, natural. Implementation: take the body engine output (post-convolution) and apply a gentle EQ tilt.

### 7.7 Blend and pickup selection

Multiple pickups: each has an on/off toggle and a gain. Standard configurations:
- 3-position switch (bridge / middle / neck)
- 5-position switch (bridge / bridge+middle / middle / middle+neck / neck)
- Independent volume knobs per pickup (like Les Paul)
- Coil tap toggle per humbucker

Selection changes crossfade over 5 ms to avoid clicks.

## 8. Whammy Bar / Tremolo Engine (module: `WhammyEngine`)

**Purpose**: global pitch modulation of all strings simultaneously, simulating a vibrato/tremolo arm.

**Input**: whammy position in semitones (double, typically -24 to +12; can be more for Floyd Rose or dive-bombs).

**Behavior modes**:

### 8.1 Vintage Tremolo (Fender-style)
- Range: usually ±2 semitones for subtle vibrato.
- Down-only in some vintage bridges; user-selectable.
- Applied as a per-string pitch offset in cents, all strings by the same amount.

### 8.2 Floyd Rose (locking tremolo)
- Range: -24 semitones (dive-bomb) to +12 semitones (pull-up).
- Locks tuning stability regardless of extreme use.
- Applied same as vintage but with wider range.
- Include the characteristic "spring resonance": add a filtered ambient noise (springs behind the bridge vibrating sympathetically) when the whammy bar returns to center. Filtered noise burst, short (50-100 ms), spectral character around 200-500 Hz with resonance.

### 8.3 Steinberger TransTrem
- Range: variable.
- **Maintains string intervals through the bend** (chords stay in tune while bending).
- Implementation: instead of applying a constant cent offset to all strings (which detunes chords), apply a **frequency ratio** to all strings. Every string is multiplied by `2^(whammyOffsetSemitones/12)`.
- Includes preset positions (transpose lock): user can lock the whammy at specific detentes for global transposition.

### 8.4 Bigsby / TranStrem-lite
- Small range, ±1 semitone typically.
- Smoother, more limited response.

**Implementation**:

The whammy engine adds a global pitch offset (in cents or as a ratio) that is summed into each string's tuning calculation:

```
finalPitchHz = tuningEngine.computePitch(string, fret, bend) * whammyRatio
```

**Smoothing**: whammy input is smoothed with a 5 ms exponential smoother. Dive-bombs generally change slowly enough that this is fine. For very fast whammy modulation (metal shred), the smoother allows fast tracking.

**Whammy mapping**: default CC 2 (breath) or user-mapped. MPE controllers with a Y-axis can be mapped here. A dedicated whammy pedal (like a Digitech Whammy) sends CC via MIDI; the plugin captures it.

**Spring resonance simulation (Floyd Rose mode)**: when whammy velocity exceeds a threshold and returns toward center, inject a short filtered noise burst representing the tremolo springs ringing. Very audible on records; often what people notice missing from cheap plugins.

**Whammy affects all strings unless user toggles "per-string whammy"** (rare, but MPE controllers can drive this).

## 9. Cable Simulation (module: `CableSim`)

Guitar cable is a lossy transmission line. A long cable + high-impedance pickup output = high-frequency roll-off.

**Model**: one-pole lowpass filter, cutoff depends on cable length (user-adjustable).
- 1 m cable: cutoff ~15 kHz (barely audible).
- 3 m cable: cutoff ~12 kHz.
- 6 m cable: cutoff ~9 kHz (noticeable).
- 10 m cable: cutoff ~6 kHz (obvious).

Add slight capacitive coupling (small mid-boost around 4 kHz) for realism.

## 10. Pre-Amp Effects Chain (module: `PreEffectsChain`)

Signal chain slots for pedals before the amp. Each slot holds one pedal or is empty (true bypass).

**Pedal types** (each is its own DSP module):
- **Compressor**: standard peak compressor with threshold, ratio, attack, release, makeup gain. Optical or FET character selectable.
- **Wah**: bandpass filter with sweep pedal control. Q, frequency range, and sweep curve adjustable.
- **Envelope filter**: bandpass filter modulated by input envelope.
- **Octaver**: pitch-shift down one octave, mixed with dry. Uses simple pitch tracking + waveform manipulation.
- **Pitch shifter**: PSOLA or WSOLA pitch shifting, ±12 semitones with mix.
- **Overdrive**: soft-clipping with pre-EQ, post-EQ, drive, level. Tube Screamer character (mid-focused, warm).
- **Distortion**: harder clipping with brighter EQ. DS-1 character.
- **Fuzz**: aggressive clipping, unstable, low input impedance simulation. Fuzz Face or Big Muff character.
- **Boost**: clean gain with slight EQ. Klon Centaur character.
- **Chorus/Vibrato**: modulation effects (also available post-amp).
- **Volume pedal**: gain modulation via expression pedal.

Up to 8 slots. Drag-and-drop reorder. Each slot has a bypass toggle (10 ms crossfade).

Every clipping/drive stage uses 4x oversampling.

## 11. Amp Engine (module: `AmpEngine`)

Simulates a guitar amplifier: preamp tube stages, tone stack, phase inverter, power amp, output transformer.

### 11.1 Amp models (user-selectable)

- **Fender Twin (blackface clean)**: two 12AX7 preamp stages, low gain, clean headroom, scooped mids.
- **Fender Tweed (Bassman)**: three 12AX7 stages, breakup at higher volumes, midrange.
- **Marshall Plexi 1959**: four 12AX7 stages, crunch, presence peak.
- **Marshall JCM800**: high-gain, tighter, more distortion.
- **Vox AC30 (Top Boost)**: EF86 or 12AX7 preamp, EL84 power, chime, sparkle.
- **Mesa Boogie Rectifier**: modern high-gain, tight low end, aggressive.
- **Bogner Ecstasy**: high-gain with clarity.
- **Diezel VH4**: modern high-gain metal amp.
- **Orange OR120**: British high-gain, thick.
- **Ampeg SVT (bass)**: high-power bass amp.
- **Custom**: user builds from scratch by selecting preamp count, power tube type, tone stack layout.

### 11.2 Preamp stage model

Each preamp stage:
1. Input gain (drive).
2. Waveshaper (tanh or asymmetric soft-clip; asymmetric produces even harmonics like a tube).
3. Cathode-follower bypass cap effect (slight low-mid boost).
4. Interstage coupling capacitor (highpass at ~10-30 Hz to remove DC).
5. Output gain.

Cascade multiple stages for higher gain. Between stages, a slight EQ shift models the tube's frequency response.

**Waveshaper function** (tube-like asymmetric):
```
y = 1.5 * tanh(x) - 0.5 * tanh(x - bias)
```
where `bias` is around 0.3 for asymmetry.

Use 4x oversampling around every waveshaper. Anti-alias with polyphase FIR.

### 11.3 Tone stack

Standard Fender/Marshall Bass-Mid-Treble tone stack. Model as a passive RC network.

For each amp, use the actual measured or derived tone stack coefficients from SPICE simulation or existing published models (David Yeh's PhD thesis is the standard reference). Implement as a coupled IIR filter (second or third order).

Presence: add a shelving EQ at ~4 kHz in the negative feedback loop of the power amp.

### 11.4 Phase inverter

Splits signal into two paths (positive and inverted) for driving push-pull power tubes. Model as a simple splitter with slight asymmetry (real phase inverters aren't perfectly balanced, which introduces even harmonics).

### 11.5 Power amp

Two power tubes in push-pull, class AB. Each tube is a waveshaper. Sum the two outputs with proper phase.

At high volume (master gain up), power tubes saturate first before preamp; this produces a different distortion character than preamp-only distortion.

Include **sag**: at high drive, the amp's power supply voltage droops slightly, causing dynamic response (compression under heavy playing). Model as slow envelope-controlled gain reduction on the power stage output.

### 11.6 Output transformer

Adds slight low-frequency saturation and high-frequency roll-off. Model as a slew-rate limiter + one-pole lowpass at ~7-10 kHz + slight even-harmonic saturation.

### 11.7 Standby switch

Muted output when in standby. Real amps warm up over 30 seconds; simulate a slow gain fade-in from standby to on.

## 12. Post-Amp Effects Chain (module: `PostEffectsChain`)

Modulation, delay, reverb pedals in the "effects loop" position (post-preamp, pre-power-amp on real amps, but placed here for typical use).

**Effect types**:
- **Chorus**: 2-4 voices, delay 15-30 ms, LFO-modulated, mix.
- **Phaser**: 4/6/8/12 allpass stages, LFO-modulated, feedback.
- **Flanger**: short delay (1-10 ms), LFO-modulated, high feedback for jet sound.
- **Tremolo**: amplitude modulation, sine/triangle/square LFO.
- **Rotary speaker**: dual rotor (horn + drum), Doppler effect, mic simulation.
- **Delay**: digital / tape / analog character, tempo-synced or free, feedback, mix, ping-pong option.
- **Reverb**: plate / hall / spring / room; size, decay, damping, pre-delay, mix.
- **Spring reverb** (specific model): three parallel delay lines with allpass diffusion, modulated to simulate the spring transducer.
- **EQ**: 5-band graphic EQ or parametric.

Up to 8 slots. Drag-and-drop reorder.

## 13. Cabinet + Microphone Engine (module: `CabinetEngine`)

**Purpose**: simulates the speaker cabinet and microphone.

### 13.1 Cabinet simulation
- Load a speaker IR (impulse response) for the selected cab + speaker + mic + position combination.
- Ship 200+ IRs covering common cabs (1x12, 2x12, 4x12, 1x15, 4x10, 8x10), speakers (Celestion Greenback, V30, G12H, Jensen C12, Alnico Blue, EVM-12L, etc.), and mic positions.
- Convolution via `dsp::Convolution`, partitioned FFT.
- IR length 100-300 ms.

### 13.2 Microphone options
Since IRs are baked with a specific mic+position, the "mic selector" in the UI just switches to a different IR. Users see: mic type, position (on-axis center / on-axis edge / off-axis / cap-edge), distance (close 1"/ medium 6"/ far 12").

### 13.3 Dual-mic support
User can load two IRs and blend them. Second IR panned to opposite side gives stereo cab response.

### 13.4 IR loading
Load asynchronously on preset change. During load, use a fallback zero-latency all-pass so the plugin never goes silent.

## 14. Room Engine (module: `RoomEngine`)

Simulates the room around the amp.

- Early reflections: 8-16 tapped delays with per-tap filtering, based on selected room preset (booth / small studio / large studio / live room / hall).
- Late reverb: FDN (feedback delay network) or Schroeder algorithm.
- Adjustable size, damping, mix.

## 15. Master Bus (module: `MasterBus`)

- Master gain (-60 dB to +12 dB).
- Master limiter fixed at -0.3 dBFS with fast release, safety only.
- Metering: peak + RMS + LUFS.
- DC blocker at 5 Hz (final safety).

## 16. State management

**Per-preset state**:
- Guitar type (string with all body/pickup/string parameters).
- Tuning (per-string frequencies and detune).
- Whammy configuration.
- Pre-effects chain state.
- Amp state.
- Post-effects chain state.
- Cabinet + mic state.
- Room state.
- Humanization state.
- MIDI mappings.

**Preset format**: JSON, extension `.luthierpreset`.

**State save/restore for host**: JUCE's `getStateInformation` / `setStateInformation`. Full JSON blob plus UI state (mode, view, A/B slots, history).

## 17. Threading model

- **Audio thread**: string engines (parallel via `juce::ThreadPool`? no, keep them serial to avoid sync overhead; parallelize only if profiler proves benefit), body convolution, pickup, whammy, pre-effects, amp, post-effects, cabinet convolution, room, master.
- **Message thread**: all UI, parameter changes from mouse.
- **Worker threads (2-4)**: IR loading, preset scanning, MIDI capture buffer flushing.

No allocations on audio thread. Communication via `AbstractFifo` for events, atomics for parameter values.

## 18. Latency reporting

Report total plugin latency to the host via `setLatencySamples()`:
- Sum of body convolution partition size + cabinet convolution partition size + oversampling delays + any lookahead in effects.
- Typical: 128 samples body + 128 samples cab + 64 samples oversampling = 320 samples at 48 kHz = 6.7 ms.

Failing to report latency causes tracks to drift against the beat when the DAW's plugin-delay compensation is on.

## 19. Testing (mandatory before ship)

**Unit tests per module**:
- `StringEngine`: pluck at various frequencies, verify pitch is correct (FFT the output, find peak); verify decay time; verify bend response.
- `TuningEngine`: verify all tunings produce correct frequencies within 0.1 cents.
- `TechniqueEngine`: verify each technique triggers correctly from given MIDI input.
- `PickupEngine`: verify comb filter position; verify pickup resonance frequency and Q.
- `WhammyEngine`: verify TransTrem maintains string intervals (chord frequencies scale together).
- `BodyEngine`: verify IR loads correctly; verify modal synthesis reproduces expected mode frequencies.
- `AmpEngine`: verify each amp stage produces expected harmonic content at test drive levels.
- `CouplingMatrix`: verify no feedback runaway with all strings ringing.

**Integration tests**:
- Play a chromatic scale through a preset; verify every note plays at correct pitch.
- Play a bend from A to B; verify smooth pitch transition without clicks.
- Play a fast slide across the fretboard; verify no denormals, no NaNs.
- Play 30 seconds of complex material at 96 kHz with all effects on; verify CPU stays under 15% on a mid-range machine.

**Round-trip test**:
- Load every factory preset, save it, reload, verify DSP output matches within 0.01 dB.

**Pluginval**:
- Level 10 clean on both platforms in CI.

**Host testing**:
- Ableton Live, FL Studio, Reaper, Studio One, Logic Pro, Cubase, Bitwig.
- 30 minutes of use per host without stuck UI, crashes, or CPU spikes.

**MIDI controller testing**:
- Standard MIDI keyboard.
- MPE controller (ROLI Seaboard, LinnStrument, Osmose).
- Guitar MIDI controller (Roland GK-3, Fishman TriplePlay).
- Verify each mode works end-to-end.

## 20. Common pitfalls Claude Code must avoid

1. **Do not use float for waveguide feedback state.** Use double. Float accumulates error too fast.
2. **Do not modulate delay-line length by writing directly; use fractional-delay interpolation.** Direct writes cause pitch glitches.
3. **Do not apply amp distortion without oversampling.** You will get aliasing that sounds like harsh digital garbage above 5 kHz.
4. **Do not forget the DC blocker on every recursive stage.** DC accumulates silently and destroys mixes on real speakers.
5. **Do not implement the sympathetic coupling matrix as a full N-to-N per-sample update.** Update coupling contributions per-block, not per-sample, and cap total coupling energy.
6. **Do not forget to reset filter state on transport start and preset load.** Otherwise you'll hear leftover state from the previous session/preset.
7. **Do not use `std::vector` inside the audio thread.** Use `juce::AudioBuffer` and fixed-size arrays.
8. **Do not use `std::cout` or `printf` for logging on the audio thread.** Use `juce::FileLogger` from a worker thread, feeding via a lock-free queue.
9. **Do not assume the sample rate stays constant.** Recompute all coefficients in `prepareToPlay`.
10. **Do not implement the amp tone stack as a naive cascaded IIR.** Use the coupled analog-modeled version; the naive version has wrong interaction between bass/mid/treble.
11. **Do not use linear interpolation for fractional delay in the waveguide.** It attenuates high frequencies too much. Use allpass interpolation.
12. **Do not model humbucker as "single-coil with extra bass".** Model as two coils summed with position offset. The comb filter between them is what makes it sound like a humbucker.
13. **Do not clamp pitch bend to whole semitones.** Fretless mode and continuous bends require continuous pitch.
14. **Do not skip the excitation shaping.** Pure white noise or a click doesn't sound like a plucked string. Use the shaped excitation described in section 5.5.
15. **Do not forget to handle voice stealing on mono strings.** Overlapping notes on the same string without proper crossfade cause clicks.
16. **Do not forget to update the delay-line length smoothly during pitch changes.** Instant jumps cause clicks.
17. **Do not forget MPE support.** Per-note pitch bend is table stakes for expressive playing.
18. **Do not tie feedback amount to unity or above.** Cap at 0.998 to prevent runaway.
19. **Do not skip mono-compatibility testing.** Some effects (chorus, wide reverb) can cancel out in mono. Every factory preset must pass a mono-compatibility check.
20. **Do not implement all this in one giant class.** Each module is its own class with a clear interface. Test each one independently before wiring them together.

## 21. Build order (do this in sequence, don't try to build it all at once)

1. **StringEngine** with fixed pitch (no bend, no vibrato). Test: play middle C, verify sine-plus-harmonics output at ~262 Hz.
2. **StringEngine with pitch modulation** (bends, vibrato, slides). Test: bend from A to B, verify smooth spectrum change.
3. **TuningEngine**. Test: verify all standard tunings and alternates produce correct frequencies.
4. **PickupEngine** (single-coil first). Test: verify comb filter at pickup position; verify resonant peak.
5. **BodyEngine** convolution mode. Test: load a body IR, verify output has body coloration.
6. **CouplingMatrix**. Test: pluck string 1, verify string 2 shows resonance at its own fundamental.
7. **MidiInterpreter** basic. Test: MIDI note on → string plucks.
8. **TechniqueEngine** basic (pluck + hammer-on + pull-off + slide). Test: rapid MIDI notes on same string produce legato.
9. **WhammyEngine**. Test: pitch offset applies to all strings; TransTrem mode maintains chord intervals.
10. **CableSim** (trivial; one-pole lowpass).
11. **PreEffectsChain** with compressor, wah, overdrive.
12. **AmpEngine** starting with clean Fender Twin. Test: verify tone stack behavior matches published response curves.
13. **AmpEngine** high-gain (Marshall Plexi, Mesa Rectifier). Test: distortion character.
14. **PostEffectsChain** with chorus, delay, reverb.
15. **CabinetEngine** with IR loading. Test: verify IR convolution matches offline convolution to sample accuracy.
16. **RoomEngine**.
17. **MasterBus**.
18. **Preset save/load**.
19. **UI Easy mode** with basic controls.
20. **UI Advanced mode** with full parameter access.
21. **MIDI Learn**.
22. **Export (audio and MIDI capture)**.
23. **Automated tests** for every module.
24. **Pluginval** at level 10 in CI.
25. **Host testing** matrix.

Ship only when every step passes.

## 22. Performance targets

- Single instance, 6 strings ringing, all effects on, 96 kHz, 128-sample buffer: under 8% CPU on a 2020 mid-range machine.
- 16 instances in a session at 48 kHz, 256-sample buffer: no glitches.
- Preset load: under 500 ms including IR loading (async, plugin remains responsive).
- MIDI latency: under 2 ms from MIDI-in to audio-out (plus reported plugin latency).

## 23. Final rule

**When in doubt, physically model.** Every design choice should be answerable with "because that's what a real guitar does." If you find yourself adding a parameter that doesn't correspond to a physical quantity, you're probably drifting from the plugin's mission. Fix it or remove it.

Build it in the order above. Don't skip steps. Test each module before wiring in the next. This engine is what makes Luthier convincing; get it right and everything else (UI, presets, marketing) is a wrapper around a great sound.