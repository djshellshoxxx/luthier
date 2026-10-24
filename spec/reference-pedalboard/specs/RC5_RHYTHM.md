# RC5 rhythm module contract

2026-10-24. Coordinator owns only Common/Rc5Rhythm.{h,cpp},
Models/rc5_patterns.inc and RefPedalRc5RhythmTests.cpp. The state worker owns
paged transport and the named RC5 wrapper. This module does not register a pedal.

The [official product page](https://www.boss.info/global/products/rc-5/) confirms
57 rhythms, A/B variations and seven kits. The [Reference Manual](https://static.roland.com/assets/media/pdf/RC-5_reference_eng01_W.pdf), pages6,10,17–18,
confirms tempo40–300, variation changes at a measure or loop boundary, optional
MIDI clock/start/stop, and per-memory rhythm settings. These sources establish
behavior and counts, not the proprietary pattern notes or samples. This module
uses original grids and synthesized sounds; all sonic details remain estimates.
The manual also suggests recorded-track tempo behavior beyond the older
spec's unsupported no-time-stretch estimate. That separate product behavior is
unresolved and is not changed by this rhythm module.

## Finite implementation

- Exactly57 authored nonempty patterns, each with two distinct variations,
  16 steps per4/4 bar and a2/4-bar phrase. Seven original sample-free kits.
- Eight bounded voices: kick, snare, closed/open hat, clap, rim, tom, crash.
  Procedural pitched/noise transients, per-kit prepared coefficients; no audio,
  copyrighted samples, files, allocation, locks or lazy initialization in process.
- Off-audio prepare(rate,seed), audio reset(), setTempo(bpm), setLevel(percent),
  select(pattern1to57,variation0or1,kit1to7), setRunning(bool), locate(ppq), process(L,R,n).
  Select applies immediately stopped, or at the next bar boundary running.
  Starting restarts the phrase. Stopping stops new hits and smooths output down.
  The final beat of each2/4-bar phrase uses its paired variation as an original fill.
  Existing voices retain their triggered kit through a kit change. When phrase
  length changes, the current bar is reduced modulo the new phrase length,
  including selection committed by an explicit transport seek.
- A double sample-clock accumulator preserves phase through tempo changes.
  The wrapper supplies canonical TimeInfo tempo and transport; full precision is
  retained internally, while the UI tempo parameter uses0.1BPM steps. Explicit
  transport seeks call locate after starting; ordinary tempo changes keep phase. This helper does
  not arbitrate host/MIDI clocks, change recorded loop speed or emit MIDI.
- Rhythm level0–100 uses squared gain and20ms one-pole smoothing. Rhythm is
  centered and added equally to both channels. State is fixed-size/double.
- Invalid selectors clamp to valid values; nonfinite tempo/level use safe
  defaults. Zero/negative lengths and null channels are no-ops. Output remains
  finite and bounded; envelope/filter denormals are cleared.

## Acceptance before integration

Every114 grid is distinct/nonempty, all seven kits produce distinct deterministic
audio, partition equivalence at44.1/48/88.2/96/192k and1/16/37/64/481/2048,
exact bar-boundary selection and tempo-phase continuity, hostile controls,
stop/reset silence, zero callback allocations, long randomized processing,
ASan/UBSan and independent review. Report measured CPU separately; RC5's total
0.8% budget includes the future looper and is not certified by helper timing.
