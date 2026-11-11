# Effects reference

This page lists effect controls in two separate places. They are different subsystems
with different controls.

1. **Pedalboard models** (the reference pedalboard, see [PEDALBOARD.md](PEDALBOARD.md)):
   13 registered models, 10 generic effects and 3 prototypes. These are the models you
   can add from the board's browser.
2. **Legacy rack pedals**: the 23 pedal types in the two eight-slot racks of the
   Advanced panel (one before the amp, one after).

Ranges, units and defaults were read from the descriptors in the source at commit
`ca14934e`: `Source/ReferencePedals/Generic/*.cpp` and
`Source/ReferencePedals/Models/PrototypePedals.cpp` for the board, and
`Source/DSP/Effects/PedalsDrive.cpp` and `PedalsMod.cpp` for the racks. Nothing here is
a measured sound or performance result.

The six named models planned for the first pedalboard release (a phaser, two
bucket-brigade choruses, a digital delay, a compressor and a looper) are **not**
registered, so they are not listed in section 1 and cannot be added.

---

## 1. Pedalboard models

### Reading the tables

- **Type**: knob (continuous), choice (a list), switch (on or off), footswitch (the
  pedal's on/off, stored with the board slot), button (momentary or trigger).
- **Range**: for knobs, the minimum and maximum in the stated unit. Many knobs are
  unitless 0 to 1 amounts. For choices, the options in order.
- **Default**: the value a newly added pedal starts with.
- **Scale**: how knob travel maps to the range. Linear, log (geometric, used for
  frequencies and times) or skew (a power curve that gives more travel to the low end).
- **Id**: the control's identifier in saved state and MIDI learn. Ids of the prototypes
  are omitted.
- A QUALITY choice (Eco, Normal, High) is a setting and has no host automation lane.
- "Bypass" is how the board treats the pedal when it is switched off.

Each pedal also has board-level settings (input trim, output trim, placement, lane,
MIDI channel); see [PEDALBOARD.md](PEDALBOARD.md).


### Clean Boost

Family: Boost. Channels: stereo; bypass: true bypass.

| Control | Id | Type | Range | Unit | Default | Scale | Notes |
|---|---|---|---|---|---|---|---|
| GAIN | `gen_boost_gain` | knob | 0 to 30 | dB | 12 | linear | - |
| TONE | `gen_boost_tone` | knob | -6 to 6 | dB | 0 | linear | - |
| LEVEL | `gen_boost_level` | knob | -24 to 6 | dB | -6 | linear | - |
| MODE | `gen_boost_mode` | choice | Clean / Warm / Fat | - | Clean | - | - |
| QUALITY | `gen_boost_quality` | choice | Eco / Normal / High | - | Normal | - | setting (no automation lane) |
| ON | `gen_boost_on` | footswitch | off / on | - | on | - | - |

### Overdrive

Family: Overdrive. Channels: stereo; bypass: true bypass.

| Control | Id | Type | Range | Unit | Default | Scale | Notes |
|---|---|---|---|---|---|---|---|
| DRIVE | `gen_overdrive_drive` | knob | 0 to 40 | dB | 16 | linear | - |
| TONE | `gen_overdrive_tone` | knob | 500 to 9000 | Hz | 3000 | log | - |
| LEVEL | `gen_overdrive_level` | knob | -60 to 12 | dB | 0 | skew | - |
| CLIP | `gen_overdrive_clip` | choice | Soft / Asym / Hard | - | Soft | - | - |
| BASS CUT | `gen_overdrive_bass` | choice | Off / 300 Hz / 700 Hz | - | 300 Hz | - | - |
| QUALITY | `gen_overdrive_quality` | choice | Eco / Normal / High | - | Normal | - | setting (no automation lane) |
| ON | `gen_overdrive_on` | footswitch | off / on | - | on | - | - |

### Fuzz

Family: Fuzz. Channels: stereo; bypass: true bypass.

| Control | Id | Type | Range | Unit | Default | Scale | Notes |
|---|---|---|---|---|---|---|---|
| FUZZ | `gen_fuzz_fuzz` | knob | 6 to 60 | dB | 44 | linear | - |
| TONE | `gen_fuzz_tone` | knob | -12 to 12 | dB | 0 | linear | - |
| VOLUME | `gen_fuzz_volume` | knob | -60 to 12 | dB | -6 | skew | - |
| BIAS | `gen_fuzz_bias` | knob | 0 to 1 | - | 0.5 | linear | - |
| TYPE | `gen_fuzz_type` | choice | Silicon / Germanium / Octave-ish | - | Silicon | - | - |
| GATE | `gen_fuzz_gate` | knob | 0 to 1 | - | 0 | linear | - |
| QUALITY | `gen_fuzz_quality` | choice | Eco / Normal / High | - | Normal | - | setting (no automation lane) |
| ON | `gen_fuzz_on` | footswitch | off / on | - | on | - | - |

### Compressor

Family: Dynamics. Channels: stereo; bypass: true bypass.

| Control | Id | Type | Range | Unit | Default | Scale | Notes |
|---|---|---|---|---|---|---|---|
| SUSTAIN | `gen_comp_sustain` | knob | 0 to 1 | - | 0.5 | linear | - |
| ATTACK | `gen_comp_attack` | knob | 0.5 to 60 | ms | 10 | log | - |
| RELEASE | `gen_comp_release` | knob | 40 to 1200 | ms | 350 | log | - |
| LEVEL | `gen_comp_level` | knob | -12 to 24 | dB | 6 | linear | - |
| BLEND | `gen_comp_blend` | knob | 0 to 1 | - | 1 | linear | - |
| MODE | `gen_comp_mode` | choice | Sustain / Manual | - | Sustain | - | - |
| THRESH | `gen_comp_thresh` | knob | -60 to 0 | dB | -24 | linear | - |
| RATIO | `gen_comp_ratio` | knob | 1.5 to 20 | :1 | 4 | log | - |
| KNEE | `gen_comp_knee` | knob | 0 to 12 | dB | 6 | linear | - |
| ON | `gen_comp_on` | footswitch | off / on | - | on | - | - |
| SIDECHAIN | `gen_comp_sc` | choice | Internal / Sidechain bus | - | Internal | - | setting (no automation lane) |

### Chorus

Family: Modulation. Channels: mono in, stereo out; bypass: true bypass.

| Control | Id | Type | Range | Unit | Default | Scale | Notes |
|---|---|---|---|---|---|---|---|
| RATE | `gen_chorus_rate` | knob | 0.1 to 8 | Hz | 0.8 | log | - |
| DEPTH | `gen_chorus_depth` | knob | 0 to 1 | - | 0.5 | linear | - |
| MIX | `gen_chorus_mix` | knob | 0 to 1 | - | 0.5 | linear | - |
| TONE | `gen_chorus_tone` | knob | 1500 to 12000 | Hz | 6000 | log | - |
| MODE | `gen_chorus_mode` | choice | Chorus / Vibrato / Dimension | - | Chorus | - | - |
| WAVE | `gen_chorus_wave` | choice | Sine / Triangle | - | Sine | - | - |
| SPREAD | `gen_chorus_spread` | knob | 0 to 1 | - | 0.8 | linear | - |
| SYNC | `gen_chorus_sync` | choice | Off / 1/4 / 1/8 / 1/8. / 1/16 / 1/2 / 1 bar | - | Off | - | - |
| ON | `gen_chorus_on` | footswitch | off / on | - | on | - | - |

### Tremolo

Family: Modulation. Channels: stereo; bypass: DSP bypass.

| Control | Id | Type | Range | Unit | Default | Scale | Notes |
|---|---|---|---|---|---|---|---|
| RATE | `gen_trem_rate` | knob | 0.2 to 16 | Hz | 4.5 | log | - |
| DEPTH | `gen_trem_depth` | knob | 0 to 1 | - | 0.6 | linear | - |
| WAVE | `gen_trem_wave` | choice | Sine / Triangle / Square / Opto / Harmonic | - | Sine | - | - |
| SHAPE | `gen_trem_shape` | knob | 0 to 1 | - | 0.5 | linear | - |
| STEREO | `gen_trem_stereo` | choice | Mono / Pan / Dual-phase | - | Mono | - | - |
| PHASE | `gen_trem_phase` | knob | 0 to 180 | deg | 90 | linear | - |
| SYNC | `gen_trem_sync` | choice | Off / 1/4 / 1/4T / 1/8 / 1/8. / 1/8T / 1/16 / 1/2 / 1 bar / 2 bars | - | Off | - | - |
| LEVEL | `gen_trem_level` | knob | -12 to 6 | dB | 0 | linear | - |
| TAP | `gen_trem_tap` | button (trigger) | press | - | - | - | - |
| ON | `gen_trem_on` | footswitch | off / on | - | on | - | - |

### Wah

Family: Filter. Channels: stereo; bypass: true bypass.

| Control | Id | Type | Range | Unit | Default | Scale | Notes |
|---|---|---|---|---|---|---|---|
| PEDAL | `gen_wah_pedal` | knob | 0 to 1 | - | 0.5 | linear | - |
| FREQ LOW | `gen_wah_range_lo` | knob | 250 to 600 | Hz | 350 | log | - |
| FREQ HIGH | `gen_wah_range_hi` | knob | 1200 to 3200 | Hz | 2200 | log | - |
| Q | `gen_wah_q` | knob | 2 to 12 | - | 6 | log | - |
| MODE | `gen_wah_mode` | choice | Treadle / Auto / LFO | - | Treadle | - | - |
| SENS | `gen_wah_sens` | knob | 0 to 1 | - | 0.5 | linear | - |
| RATE | `gen_wah_rate` | knob | 0.2 to 10 | Hz | 1.5 | log | - |
| MIX | `gen_wah_mix` | knob | 0 to 1 | - | 1 | linear | - |
| TOE SWITCH | `gen_wah_toe` | choice | Off / On | - | Off | - | - |
| ON | `gen_wah_on` | footswitch | off / on | - | on | - | - |

### Phaser

Family: Modulation. Channels: mono in, stereo out; bypass: true bypass.

| Control | Id | Type | Range | Unit | Default | Scale | Notes |
|---|---|---|---|---|---|---|---|
| RATE | `gen_phaser_rate` | knob | 0.05 to 8 | Hz | 0.5 | log | - |
| DEPTH | `gen_phaser_depth` | knob | 0 to 1 | - | 0.8 | linear | - |
| FEEDBACK | `gen_phaser_feedback` | knob | -0.9 to 0.9 | - | 0.3 | linear | - |
| STAGES | `gen_phaser_stages` | choice | 2 / 4 / 6 / 8 / 12 | - | 4 | - | - |
| MIX | `gen_phaser_mix` | knob | 0 to 1 | - | 0.5 | linear | - |
| CENTER | `gen_phaser_center` | knob | 200 to 2000 | Hz | 700 | log | - |
| SYNC | `gen_phaser_sync` | choice | Off / 1/4 / 1/8 / 1/8. / 1/16 / 1/2 / 1 bar | - | Off | - | - |
| ON | `gen_phaser_on` | footswitch | off / on | - | on | - | - |

### Delay

Family: Time. Channels: true stereo; bypass: DSP bypass.

| Control | Id | Type | Range | Unit | Default | Scale | Notes |
|---|---|---|---|---|---|---|---|
| TIME | `gen_delay_time` | knob | 20 to 2000 | ms | 380 | log | own smoothing |
| FEEDBACK | `gen_delay_feedback` | knob | 0 to 1.1 | - | 0.35 | linear | - |
| MIX | `gen_delay_mix` | knob | 0 to 1 | - | 0.35 | linear | - |
| TONE | `gen_delay_tone` | knob | 1200 to 12000 | Hz | 5000 | log | - |
| MODE | `gen_delay_mode` | choice | Digital / Analog / Tape | - | Digital | - | - |
| MOD | `gen_delay_mod` | knob | 0 to 1 | - | 0.15 | linear | - |
| SUBDIV | `gen_delay_sub` | choice | 1/4 / 1/8 / 1/8. / 1/8T / 1/16 / 1/2 / 1 | - | 1/4 | - | - |
| SYNC | `gen_delay_sync` | choice | Off / Host/Clock / Tap | - | Off | - | - |
| PING-PONG | `gen_delay_pingpong` | choice | Off / On / Wide | - | Off | - | - |
| STEREO | `gen_delay_spread` | knob | 0 to 1 | - | 0 | linear | - |
| TRAILS | `gen_delay_trails` | choice | Cut / Spillover | - | Spillover | - | - |
| HOLD | `gen_delay_hold` | button (momentary) | press | - | - | - | - |
| ON | `gen_delay_on` | footswitch | off / on | - | on | - | - |
| TAP | `gen_delay_tap` | button (trigger) | press | - | - | - | - |

### Reverb

Family: Time. Channels: mono in, stereo out; bypass: DSP bypass.

| Control | Id | Type | Range | Unit | Default | Scale | Notes |
|---|---|---|---|---|---|---|---|
| DECAY | `gen_reverb_decay` | knob | 0.2 to 12 | s | 2.2 | log | - |
| MIX | `gen_reverb_mix` | knob | 0 to 1 | - | 0.3 | linear | - |
| PRE-DELAY | `gen_reverb_predelay` | knob | 0 to 250 | ms | 15 | linear | - |
| TONE | `gen_reverb_tone` | knob | -1 to 1 | - | 0 | linear | - |
| SIZE | `gen_reverb_size` | knob | 0.3 to 1.7 | - | 1 | linear | - |
| TYPE | `gen_reverb_type` | choice | Room / Hall / Plate / Spring / Ambient | - | Hall | - | - |
| MOD | `gen_reverb_mod` | knob | 0 to 1 | - | 0.25 | linear | - |
| LOW CUT | `gen_reverb_lowcut` | knob | 20 to 800 | Hz | 120 | log | - |
| TRAILS | `gen_reverb_trails` | choice | Cut / Spillover | - | Spillover | - | - |
| FREEZE | `gen_reverb_freeze` | button (momentary) | press | - | - | - | - |
| ON | `gen_reverb_on` | footswitch | off / on | - | on | - | - |

### Overdrive prototype

Family: prototype. Channels: mono; bypass: true bypass. It is registered and appears in the browser (with a name that refers to the design it approximates). It is a simple three-control approximation, not a detailed model.

| Control | Id | Type | Range | Unit | Default | Scale | Notes |
|---|---|---|---|---|---|---|---|
| Drive | - | knob | 0 to 1 | - | 0.5 | linear | - |
| Tone | - | knob | 0 to 1 | - | 0.5 | linear | - |
| Level | - | knob | 0 to 1 | - | 0.5 | linear | - |

### Distortion prototype

Family: prototype. Channels: mono; bypass: true bypass. It is registered and appears in the browser (with a name that refers to the design it approximates). It is a simple three-control approximation, not a detailed model.

| Control | Id | Type | Range | Unit | Default | Scale | Notes |
|---|---|---|---|---|---|---|---|
| Distortion | - | knob | 0 to 1 | - | 0.5 | linear | - |
| Tone | - | knob | 0 to 1 | - | 0.5 | linear | - |
| Level | - | knob | 0 to 1 | - | 0.5 | linear | - |

### Phaser prototype

Family: prototype. Channels: mono; bypass: true bypass. It is registered and appears in the browser (with a name that refers to the design it approximates). It is a simple three-control approximation, not a detailed model.

Two of its three controls are labelled "Reserved" in the descriptor.

| Control | Id | Type | Range | Unit | Default | Scale | Notes |
|---|---|---|---|---|---|---|---|
| Speed | - | knob | 0 to 1 | - | 0.5 | linear | - |
| Reserved | - | knob | 0 to 1 | - | 0.5 | linear | - |
| Reserved | - | knob | 0 to 1 | - | 0.5 | linear | - |

---

## 2. Legacy rack pedals

These are the 23 pedal types of `PedalType` (`Source/DSP/Effects/Pedal.h`). They live in
the two eight-slot racks of the Advanced panel and are not the pedalboard.
In the source they are grouped as pre-amp pedals (compressor to volume pedal) and
post-amp or loop pedals (chorus to parametric EQ); the doubler and the gater were
appended later so that saved slot indices keep their meaning. Each pedal has up to 10
controls.

The Skew column is the descriptor's skew: 1 is linear; a value below 1 gives more
travel to the low end of the range. Choice controls show their options in order and the
default option. The Free edition limits which of these pedals are available
(`Source/Edition.h`); which ones is not listed here.


### Compressor (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Threshold | -48 to 0 | dB | -18 | 1 |
| Ratio | 1 to 20 | :1 | 4 | 0.6 |
| Attack | 0.1 to 100 | ms | 8 | 0.4 |
| Release | 10 to 1000 | ms | 120 | 0.4 |
| Makeup | 0 to 24 | dB | 0 | 1 |
| Character | Optical / FET | - | Optical | 1 |

### Noise Gate (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Threshold | -90 to -20 | dB | -60 | 1 |
| Attack | 0.1 to 50 | ms | 1 | 0.4 |
| Hold | 0 to 500 | ms | 40 | 0.5 |
| Release | 10 to 2000 | ms | 120 | 0.4 |

### Wah (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Low | 200 to 1200 | Hz | 400 | 0.5 |
| High | 1000 to 4000 | Hz | 2200 | 0.5 |
| Q | 1 to 12 | - | 4 | 0.6 |
| Level | 0 to 2 | - | 1 | 1 |
| Mode | Pedal / Auto | - | Pedal | 1 |

### Envelope Filter (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Sense | 0 to 1 | - | 0.5 | 1 |
| Base | 80 to 1000 | Hz | 250 | 0.5 |
| Range | 0.5 to 5 | oct | 3 | 1 |
| Q | 1 to 10 | - | 3.5 | 0.6 |
| Attack | 1 to 100 | ms | 10 | 0.4 |
| Direction | Up / Down | - | Up | 1 |

### Octaver (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Sub 1 | 0 to 1 | - | 0.6 | 1 |
| Sub 2 | 0 to 1 | - | 0 | 1 |
| Direct | 0 to 1 | - | 1 | 1 |
| Tone | 500 to 6000 | Hz | 2500 | 0.5 |

### Pitch Shifter (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Pitch | -12 to 12 | st | -12 | 1 |
| Fine | -50 to 50 | cent | 0 | 1 |
| Mix | 0 to 1 | - | 0.5 | 1 |
| Level | 0 to 2 | - | 1 | 1 |

### Overdrive (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Drive | 0 to 1 | - | 0.4 | 1 |
| Tone | 0 to 1 | - | 0.5 | 1 |
| Level | 0 to 1 | - | 0.5 | 1 |
| Presence | 0 to 1 | - | 0.5 | 1 |
| Voicing | Warm / Bright / Fat | - | Warm | 1 |

### Distortion (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Drive | 0 to 1 | - | 0.55 | 1 |
| Tone | 0 to 1 | - | 0.5 | 1 |
| Level | 0 to 1 | - | 0.45 | 1 |
| Presence | 0 to 1 | - | 0.55 | 1 |
| Voicing | Classic / Modern / Scooped | - | Classic | 1 |

### Fuzz (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Fuzz | 0 to 1 | - | 0.7 | 1 |
| Tone | 0 to 1 | - | 0.5 | 1 |
| Level | 0 to 1 | - | 0.4 | 1 |
| Bias | 0 to 1 | - | 0.5 | 1 |
| Voicing | Face / Muff / Tone Bender | - | Face | 1 |

### Boost (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Gain | 0 to 1 | - | 0.35 | 1 |
| Tone | 0 to 1 | - | 0.6 | 1 |
| Level | 0 to 1 | - | 0.6 | 1 |
| Treble | 0 to 1 | - | 0.5 | 1 |
| Voicing | Clean / Treble / Full | - | Clean | 1 |

### Volume Pedal (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Min | 0 to 1 | - | 0 | 1 |
| Max | 0 to 2 | - | 1 | 1 |
| Taper | Log / Linear | - | Log | 1 |

### Chorus (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Rate | 0.05 to 8 | Hz | 0.6 | 0.5 |
| Depth | 0.1 to 12 | ms | 3.5 | 0.7 |
| Delay | 5 to 40 | ms | 18 | 1 |
| Voices | 1 to 4 (4 steps) | - | 3 | 1 |
| Mix | 0 to 1 | - | 0.45 | 1 |

### Phaser (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Rate | 0.02 to 8 | Hz | 0.4 | 0.5 |
| Depth | 0 to 1 | - | 0.7 | 1 |
| Feedback | 0 to 0.9 | - | 0.35 | 1 |
| Stages | 4 / 6 / 8 / 12 | - | 4 | 1 |
| Mix | 0 to 1 | - | 0.5 | 1 |

### Flanger (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Rate | 0.02 to 6 | Hz | 0.25 | 0.5 |
| Depth | 0.1 to 8 | ms | 3 | 0.8 |
| Manual | 0.3 to 10 | ms | 4 | 1 |
| Feedback | -0.95 to 0.95 | - | 0.6 | 1 |
| Mix | 0 to 1 | - | 0.5 | 1 |

### Tremolo (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Rate | 0.1 to 20 | Hz | 5 | 0.5 |
| Depth | 0 to 1 | - | 0.6 | 1 |
| Shape | Sine / Triangle / Square / Random | - | Sine | 1 |
| Stereo | 0 to 180 | deg | 0 | 1 |
| Sync | Free / 1/1 / 1/2 / 1/4 / 1/4. / 1/8 / 1/8. / 1/8T / 1/16 | - | Free | 1 |

### Rotary (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Speed | Slow / Fast | - | Fast | 1 |
| Horn | 0.5 to 10 | Hz | 6 | 1 |
| Drum | 0.2 to 5 | Hz | 0.8 | 1 |
| Distance | 0 to 1 | - | 0.5 | 1 |
| Mix | 0 to 1 | - | 0.7 | 1 |

### Delay (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Time | 20 to 2000 | ms | 375 | 0.5 |
| Feedback | 0 to 0.95 | - | 0.35 | 1 |
| Mix | 0 to 1 | - | 0.3 | 1 |
| Tone | 800 to 16000 | Hz | 6000 | 0.5 |
| Character | Digital / Analogue / Tape | - | Digital | 1 |
| Sync | Free / 1/1 / 1/2 / 1/4 / 1/4. / 1/8 / 1/8. / 1/8T / 1/16 | - | Free | 1 |
| Ping Pong | Off / On | - | Off | 1 |

### Reverb (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Size | 0 to 1 | - | 0.5 | 1 |
| Decay | 0.2 to 15 | s | 2 | 0.6 |
| Damping | 0 to 1 | - | 0.5 | 1 |
| Pre-Delay | 0 to 200 | ms | 20 | 0.7 |
| Character | Plate / Hall / Room / Chamber | - | Plate | 1 |
| Mix | 0 to 1 | - | 0.25 | 1 |

### Spring Reverb (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Decay | 0 to 1 | - | 0.5 | 1 |
| Tension | 0 to 1 | - | 0.5 | 1 |
| Drip | 0 to 1 | - | 0.5 | 1 |
| Tone | 800 to 8000 | Hz | 4500 | 0.5 |
| Mix | 0 to 1 | - | 0.3 | 1 |

### Graphic EQ (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| 100 Hz | -15 to 15 | dB | 0 | 1 |
| 250 Hz | -15 to 15 | dB | 0 | 1 |
| 630 Hz | -15 to 15 | dB | 0 | 1 |
| 1.6 kHz | -15 to 15 | dB | 0 | 1 |
| 4 kHz | -15 to 15 | dB | 0 | 1 |
| 8 kHz | -15 to 15 | dB | 0 | 1 |
| Level | -18 to 18 | dB | 0 | 1 |

### Parametric EQ (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Low Freq | 40 to 400 | Hz | 120 | 0.5 |
| Low Gain | -18 to 18 | dB | 0 | 1 |
| Mid Freq | 200 to 5000 | Hz | 800 | 0.5 |
| Mid Gain | -18 to 18 | dB | 0 | 1 |
| Mid Q | 0.2 to 10 | - | 1 | 0.6 |
| High Freq | 1500 to 16000 | Hz | 4000 | 0.5 |
| High Gain | -18 to 18 | dB | 0 | 1 |
| Highpass | 20 to 1000 | Hz | 20 | 0.4 |
| Lowpass | 1000 to 20000 | Hz | 20000 | 0.4 |

### Doubler (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Delay | 5 to 40 | ms | 22 | 1 |
| Pitch | -25 to 25 | cent | -8 | 1 |
| Pan | -1 to 1 | - | -0.7 | 1 |
| Width | Mono / Stereo | - | Stereo | 1 |
| Mix | 0 to 100 | % | 40 | 1 |
| HP | 20 to 500 | Hz | 100 | 0.5 |
| LP | 2000 to 20000 | Hz | 8000 | 0.5 |

### Gater (legacy rack)

| Control | Range or options | Unit | Default | Skew |
|---|---|---|---|---|
| Rate | Free / 1/1 / 1/2 / 1/4 / 1/4. / 1/8 / 1/8. / 1/8T / 1/16 | - | 1/8 | 1 |
| Size | 1 to 99 | % | 50 | 1 |
| Shape | 0 to 1 | - | 0.2 | 1 |
| Frequency | 0.5 to 20 | Hz | 4 | 0.5 |
