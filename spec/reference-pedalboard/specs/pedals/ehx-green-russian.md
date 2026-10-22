# ehx-green-russian — EHX Green Russian Big Muff Pi (inspired-by) — DEV SPEC v0.1

> **RECONCILED v1.1 (2026-10-21) - normative, overrides conflicting text below** (rules: IMPLEMENTATION_PLAN.md sec 1; API: BOARD_CORE.md sec 11).
> - `PedalDesc`: modelId `ehx-green-russian`, **paramPrefix `ehx_green_russian_`** (every ParamDesc.id starts with it), namespace `luthier::reference::models`, code `Source/ReferencePedals/Models/ehx_green_russian.{h,cpp}`, face `Source/UI/ReferencePedals/Faces/ehx_green_russian_face.cpp` (procedural `juce::Path`, no .svg/bitmap), test `Source/Tests/ReferencePedals_ehx_green_russianTests.cpp`.
> - Engage/footswitch params (PF_Engage, alias of slot `enabled` / `ref_s{nn}_enabled`, 1 = effect engaged, polarity of any "bypass"/"on=0" wording is normalised to this): `ehx_green_russian_bypass`.
> - REMOVED from the pedal ParamDesc list (handled by slot/board layer): slot trims `ehx_green_russian_input_trim_db` -> `ref_s{nn}_in_db/out_db`; footswitch-mode params `ehx_green_russian_fsw_mode` -> FootController mode (MIDI_EXPRESSION 9.2); per-pedal MIDI/softpress params (none) -> `midi.slotChan[]` / FootController `softPress`.
> - PF_Config (structural, not lane-mapped): `ehx_green_russian_stereo_mode`, `ehx_green_russian_os_factor`, `ehx_green_russian_input_load`.  Momentary/Trigger (gesture, not lane-mapped): (none).  PF_Ext (virtual extension, state + MIDI-learn only): `ehx_green_russian_at_to_sustain`.
> - Host lanes `ref_s{nn}_p01..` in this order: `ehx_green_russian_sustain`, `ehx_green_russian_tone`, `ehx_green_russian_volume`.
> - CC numbers in this spec are the pedal-LOCAL profile (MIDI_EXPRESSION 9): never global, may overlap other pedals. Tempo = `TimeInfo` from `ClockSync`; board tempo param is `ref_board_tempo_bpm` (no per-pedal tempo param unless the hardware has one). Footprint in mm renders at 1.6 px/mm.

model_id `ehx-green-russian` | category Fuzz | namespace `ReferencePedals::Models` | files `Source/ReferencePedals/Models/ehx_green_russian.{h,cpp}`, test `Source/Tests/ReferencePedals_ehx_green_russianTests.cpp`.
Evidence tags: DOCUMENTED (retailer/mfr text or published circuit data; URL given; 'secondary' = reached only via WebSearch snippet of a third-party page, primary page not opened), MEASURED (none), ESTIMATED (derived with stated derivation), OMIT (not modellable; plugin does nothing).
Research limit (re-attempted 2026-10-22): WebFetch and curl cannot resolve ANY external host (ENOTFOUND / no route; ehx.com, electrosmash.com, bigmuffpage.com, kitrae.net, tagboardeffects, generalguitargadgets, premierguitar, web.archive.org, wikipedia all failed). Only WebSearch snippets were reachable. Values upgraded in section 11 come from those snippets (secondary). Everything else stays ESTIMATED from the public Big Muff Pi topology until confirmed against: EHX manual (ehx.com/products/green-russian-big-muff-pi/), Big Muff Pi Page schematics (bigmuffpage.com, "Big_Muff_Pi_versions_schematics_part4"), ElectroSmash "Big Muff Pi Analysis". Wording is original; no copied text/photos.

## 1 Identity / variants
- Product: reissue of Sovtek-era tall-font "Green Russian" Big Muff Pi, EHX, analog, mono, true bypass (DOCUMENTED: retailers). Controls: VOLUME, TONE, SUSTAIN. Power: 9V battery (inside bottom plate, 4 screws) or 9VDC center-negative 2.1mm; optional EHX9.6DC-200 adapter (9.6VDC 200mA, not included), battery included, current draw 3 mA, dims 4.5 x 2.75 x 2.1 in (DOCUMENTED, EHX/retail via search snippet: https://www.ehx.com/greenrussianbigmuffpi , https://shop.ehx.com/item/rus%20bm/). 9.6 V is the adapter's unloaded-ish rating; model Vcc = 9.0 V (design choice, battery nominal).
- Voicing: fuller bass/low-mids and less mid-scoop than NYC/Ram's-head; reissue reported less scooped than original Russian (DOCUMENTED forum/review, subjective).
- Reissue facts: released Aug 2017, engineer Gino Maisano, PCB EC-D155 Rev. C, true bypass (DOCUMENTED secondary, kitrae.net history page via search snippet).
- Variants in plugin: single voicing only. No `revision` parameter is shipped. `sovtek-black-v8` / `nyc-ramshead` variants: OMIT (no verified component data; plugin does nothing).
- Legal: name shown only as "inspired by"; enclosure art is original vector (section 7).

## 2 Controls
| ctl | type | range | taper | default | behaviour |
|---|---|---|---|---|---|
| SUSTAIN | rotary pot | 0..10 (0-1 norm) | audio (log, A100k; norm->R = 100k*(10^(2n)-1)/99) ESTIMATED (Big Muff family uses 100k pots; reissue value unverified) | 0.5 | variable gain between stage 1 and 2 input (pot as divider/feedback R). Min = clean-ish fuzz-lite, max = full saturation, sustain/compression |
| TONE | rotary pot | 0..10 | linear B100k ESTIMATED | 0.5 | passive pan LP<->HP; 0 = dark/bassy LP, 10 = thin/bright HP, noon = deepest mid notch (~1.16 kHz, ESTIMATED: 1/(2*pi*sqrt(R1*R2*C1*C2)) with 22k/22k/3.9n/10n) |
| VOLUME | rotary pot | 0..10 | audio (log) | 0.5 | output level after recovery stage, 0 = silent, max ~ +10..+15 dB over unity ESTIMATED |
| FOOTSWITCH | 3PDT stomp | on/off | - | off | true bypass; LED on when engaged |
| INPUT | 1/4" TS | - | - | - | mono; Zin ~ 100 kΩ nominal ESTIMATED (no published figure found; Q1 base bias network). Default model: input_load off |
| OUTPUT | 1/4" TS | - | - | - | Zout ~ 10 kΩ nominal ESTIMATED |
| DC | 2.1mm center-neg | 9V | - | - | no model of battery sag in v1 (option `psu_sag` later) |
No other jacks/switches. No MIDI/expression on hardware.

## 3 Signal path / circuit model (ESTIMATED topology, 4 transistor stages, Vcc 9 V, bias ~4.5 V)
1. Input HP: Cin 100 nF into ~100k bias -> fc ≈ 16 Hz (ESTIMATED).
2. Stage Q1 (input boost, common-emitter, silicon NPN, high-hFE (~500) BC549C-class per secondary source, DOCUMENTED-secondary; 2N5088/2N5089 are modern substitutes): gain ~ +20..26 dB fixed, mild asymmetric soft clip.
3. Interstage HP: C ≈ 100 nF + SUSTAIN pot load -> fc 10..100 Hz depending on sustain (bass retention is the Russian trait).
4. SUSTAIN: pot sets gain of clip stage 1 input (stage gain ≈ 1+Rf/Re region), range ≈ 0..+40 dB.
5. Clipping stage Q2: CE amp with back-to-back silicon diodes (1N914 per secondary source, DOCUMENTED-secondary; Is≈2.5 nA, n≈1.9 ESTIMATED from 1N914/1N4148 datasheet class) in feedback with Cf = 470 pF (DOCUMENTED-secondary: 'later green and black Russian' feedback cap, kitrae.net via search snippet) -> symmetric soft clip at ±0.6..0.7 V; feedback-C lowpass ≈ 1/(2πRfCf).
6. Coupling C: value not found. ESTIMATED 100 nF (BMP-family typical interstage value) -> HP fc ≈ 16 Hz into ~100k. Reissue-vs-original cap difference claim (forum) is unverified: OMIT as a modelled difference.
7. Clipping stage Q3: identical to Q2 (second diode pair, same Cf). Cascade yields high-order odd harmonics (University of Illinois report: mostly odd harmonics, DOCUMENTED).
8. Tone stack (passive, drives Q4): R1=22 kΩ, C1=3.9 nF, R2=22 kΩ, C2=10 nF (DOCUMENTED-secondary: Big Muff mods page via search snippet lists this for 'Reissue version'; same page lists Green Russian as R1=20k, C1=3.9n, R2=22k, C2=10n; 22k used here as the reissue). LP leg R1/C1: fc = 1/(2*pi*22k*3.9n) = 1.85 kHz; HP leg C2/R2: fc = 1/(2*pi*22k*10n) = 723 Hz (derived). TONE pot wiper (100k, ESTIMATED) crossfades; mid notch at noon ~1.16 kHz (derived), insertion loss ~ -20 dB at notch (ESTIMATED), recovered by Q4. (Earlier 39k/3.9n/22k values replaced.)
9. Recovery stage Q4: CE gain ~ +20 dB, makeup for tone-stack loss, feeds VOLUME pot.
10. Output: VOLUME pot -> output coupling 100 nF into 1 MΩ-ish load.
- Latency: 0 samples algorithmic from circuit; oversampling filters add latency (section 4); report via `getLatencySamples()` and compensate dry on bypass crossfade only (true bypass = no latency in hardware).
- Bypass: true bypass (hardware relay-free 3PDT, mechanical, NO click-suppression). Plugin: 5 ms equal-power crossfade dry<->wet to avoid click (documented deviation, `bypass_xfade_ms`). When bypassed, wet path keeps running 50 ms then idles (flush denormals, clear states) to avoid CPU.
- Analog level calibration (design choice, not hardware data): 0 dBFS = 1.0 V peak default; guitar nominal -20 dBFS peak 0.1 V; user trim `input_trim_db` ±12.

## 4 Algorithm
Block: in -> inTrim -> HP1 -> Q1 soft-clip -> HP2 -> gainStage(sustain) -> CLIP2 -> HP3 -> CLIP3 -> tone stack (2 biquads + mix) -> Q4 gain -> volume -> out -> bypass xfade.
- Oversampling: 4x polyphase (halfband IIR/FIR cascade, 2 stages) for all nonlinear blocks (Q1 .. Q4); 2x when CPU budget mode "eco"; 8x offline render option. Aliasing target: alias products ≥ 60 dB below fundamental for 1 kHz input at SUSTAIN 100%, 96 kHz-equiv rate check.
- Clipper per stage (per-sample, at OS rate): solve diode feedback op-style: `y = Vt*n*asinh(x_g/(2*Is*Rf))`-type closed form (Wright-omega / asinh approximation) for symmetric pair; with feedback cap Cf as one-pole LP (trapezoidal/ZDF): `s += 2*g*(v - s)` with g = tan(pi*fc/fs_os)/(1+tan(...))`. Max Newton iterations 0 (closed form) -> deterministic.
- Q1/Q4 soft clip: `y = tanh(k*x + b) - tanh(b)` with bias b≈0.15 (asymmetry), k set by gain; DC-block after (1-pole HP 10 Hz).
- Tone stack: LP biquad (bilinear prewarped at 1.05 kHz scaled by pot position via R mapping) + HP; implement as ZDF state-space recomputed every 32 samples with control smoothing; mix `out = (1-t)*LP(x) + t*HP(x)` then loss factor from table (-20 dB at t=0.5 ... -9 dB at ends, ESTIMATED).
- Smoothing: sustain, tone, volume with 1-pole (30 ms, linear gain in log domain for volume/sustain). Footswitch xfade 5 ms.
- Denormals: `juce::ScopedNoDenormals` + add 1e-18 DC-offset-free alternate-sign dither on filter states; flush states on bypass idle.
- CPU budget: ≤ 1.5% of one core @48 kHz stereo-summed mono 4x OS on reference x86 (ESTIMATED target), ≤ 6 MFLOP/block of 512. No alloc/locks/IO on audio thread; all coefficient tables precomputed in `prepare()`; params via `std::atomic<float>` snapshot.

## 5 I/O & routing
- Hardware mono in / mono out. Plugin: mono-sum or dual-mono (default dual-mono: two independent state sets, stereo in -> stereo out; `stereo_mode` enum {dual_mono, sum_to_mono}).
- No extra jacks. Board-level tricks available through Board routing (not pedal features): put in FX-loop of another pedal for series order; parallel (split/mix) with dry via board wet/dry; feedback: board loop send/return into a delay then back into Muff input (limit gain: feedback loop must be soft-limited, loop gain clamp ≤ 0.95 and DC-block); stereo split: L->Muff, R->clean; high-gain stacking before/after other drive (Muff into amp-style EQ).
- Placement allowed: PRE, LOOP (per ARCHITECTURE.md). Input impedance interaction: `input_load` option models 100k loading on previous pedal (optional, default off).

## 6 MIDI / sync / expression (no hardware analog; emulation of board-level functions)
Hardware has none, so provide a generic mapping layer per MIDI_EXPRESSION.md:
- Hardware has NO MIDI implementation (no chart exists). Default CC numbers CC14..CC18 in the earlier draft were invented: OMIT as 'documented'; they are board-layer pedal-LOCAL defaults only (CC14 SUSTAIN, CC15 TONE, CC16 VOLUME, CC17 engage >=64, per MIDI_EXPRESSION.md), MIDI learn overrides. Input trim CC removed (slot-level). PC: ignore.
- Expression target: any of the 3 knobs; heel=min toe=max; curve linear or audio; 20 ms smoothing.
- Tap tempo/MIDI clock: not applicable (no time-based effect); do NOT sync.
- Virtual extensions (not hardware features, OMIT as authenticity claims): aftertouch can modulate SUSTAIN (+0..30%, `at_to_sustain`, depth 0..1). Velocity->xfade speed and latch/momentary mode are handled by the FootController layer (`fsw_mode` removed from pedal).
- Sliders/knobs: all params MIDI-learn, 14-bit via CC pair optional.

## 7 Visuals (original vector art; ESTIMATED dimensions)
- Enclosure: portrait pedal, 114.3 x 69.9 mm footprint (DOCUMENTED: 4.5 x 2.75 in, EHX/retail; renders at 1.6 px/mm per reconciled header, the earlier '3 px/mm' and '117 mm' are superseded). Colour: military/olive green powder coat (#4F6B3A base, #3C5230 shade; ESTIMATED artistic choice), light-gray/white text (#E8E6DA), font-neutral sans (do not copy logo).
- Layout (origin top-left of face, mm): 3 knobs in a row, centres y=16: VOLUME x=18, TONE x=35, SUSTAIN x=52 (knob dia 14 mm, black skirted w/ white pointer line, 270° sweep, 7 o'clock to 5 o'clock). Labels 4 mm below... above knobs: "VOLUME" "TONE" "SUSTAIN", numerals 0..10 arc. LED: red 3 mm at x=35 y=36. Footswitch: 12 mm dia chrome, centre x=35 y=60 (large, lower half). Jacks on sides (INPUT right/left per convention), DC jack top. Wordmark area y=42..50 with original lettering "Big Muff Pi style" avoided: use text "GREEN RUSSIAN FUZZ (inspired by)".
- Vector notes: rounded rect r=3 mm, 1 px inner highlight top, drop shadow offset (0,2 mm) blur 3 mm, knob radial gradient, LED glow radius 6 mm when on, footswitch press animation 1 mm travel/60 ms. Dark/light-safe.

## 8 Parameter table
prefix `ehx_green_russian_`.
| id | name | min | max | default | unit | skew | smooth ms | auto |
|---|---|---|---|---|---|---|---|---|
| ehx_green_russian_sustain | Sustain | 0 | 1 | 0.5 | norm | log (audio) | 30 | yes |
| ehx_green_russian_tone | Tone | 0 | 1 | 0.5 | norm | linear | 30 | yes |
| ehx_green_russian_volume | Volume | 0 | 1 | 0.5 | norm | log (audio) | 30 | yes |
| ehx_green_russian_bypass | Engaged (1=on) | 0 | 1 | 1 | bool | step | 5 xfade | yes |
| ~~ehx_green_russian_input_trim_db~~ | REMOVED (slot in_db/out_db) | - | - | - | - | - | - | - |
| ehx_green_russian_stereo_mode | Stereo mode | 0 | 1 | 0 | enum | step | - | no |
| ehx_green_russian_os_factor | Oversampling | 0 | 2 | 1 | enum(2x,4x,8x) | step | - | no |
| ehx_green_russian_at_to_sustain | Aftertouch->Sustain | 0 | 1 | 0 | norm | linear | 20 | yes |
| ~~ehx_green_russian_fsw_mode~~ | REMOVED (FootController) | - | - | - | - | - | - | - |
| ehx_green_russian_input_load | Model 100k load | 0 | 1 | 0 | bool | step | - | no |
Param IDs append-only; never renumber.

## 9 Tests / acceptance (thresholds PROVISIONAL until hardware capture)
- Silence in -> |out| < -120 dBFS, DC < 1e-4 after 1 s.
- Bypass null: bypass on, wet disabled => bit-exact (after 5 ms xfade settle).
- Freq response (small-signal -40 dBFS sine, SUSTAIN=0): TONE=0.5 notch depth 15..25 dB below 200 Hz level near 0.9–1.4 kHz; TONE=0 LP -3 dB between 1.2–2.5 kHz (derived 1.85 kHz); TONE=1 HP -3 dB between 0.5–1.0 kHz (derived 723 Hz). Tol ±3 dB, replace with measured grid.
- THD (1 kHz, -20 dBFS peak in): SUSTAIN 0.25 -> 3..12%; 0.5 -> 15..40%; 1.0 -> >40% and dominated by odd harmonics (3rd > 2nd by ≥ 6 dB).
- Level: volume 0.5 gain unity ±6 dB at SUSTAIN 0.5.
- Aliasing: 5 kHz sine, SUSTAIN 1.0, 4x OS: alias components ≤ -60 dBc.
- Sweep/impulse/step, two-tone IMD, sample rates 44.1/48/96 same response ±0.5 dB, block sizes 1/4/16/64/512, determinism (noise off), param jump no click (> -60 dB step spike), 30 min random soak no NaN/denormal CPU spike (<2x baseline), save/restore equality, CPU ≤ budget.
- Null-test vs reference curves: error RMS target ≤ -30 dB spectral envelope once hardware capture exists.

## 10 Known inaccuracies / open items
- Remaining ESTIMATED: pot values/tapers, coupling/input/output caps, bias resistors, impedances, gains, insertion loss. Tone stack R/C, feedback cap, diode type, transistor class, power, size are DOCUMENTED-secondary (section 11).
- Reissue vs original coupling-cap differences: unverified, not modelled (OMIT).
- No transistor-level hFE/temperature variation, no battery sag, no power-supply noise, no mechanical switch pop.
- Bypass crossfade deviates from true-bypass hardware by design.
- Oversampling latency not present in hardware.
- Do not claim "authentic"/"identical" before verified capture.
- Loopers section N/A (not a looper).

Sources (links, content not fully fetched): https://www.ehx.com/products/green-russian-big-muff-pi/ ; https://www.bigmuffpage.com/Big_Muff_Pi_versions_schematics_part4.html ; https://www.electrosmash.com/big-muff-pi-analysis ; https://www.premierguitar.com/gear/electro-harmonix-russian-big-muff-pi-pedal-mods ; https://courses.physics.illinois.edu/phys406/sp2017/Student_Projects/Spring15/Jon_Liang_Physics_406_Final_Report_Sp15.pdf ; https://forum.seymourduncan.com/threads/so-hows-the-new-russian-big-muff.320280

## 11 Verification log
Access: all direct fetches failed (ENOTFOUND / no route) for ehx.com, electrosmash.com, bigmuffpage.com, kitrae.net, tagboardeffects, generalguitargadgets, premierguitar, archive.org, wikipedia. Reached only WebSearch snippets. 'secondary' = snippet of third-party page, primary not opened; re-verify when network allows.
| # | item | old -> new | status | source |
|---|---|---|---|---|
| 1 | tone stack LP leg | 39k / 3.9n -> R1=22k, C1=3.9n (fc 1.85 kHz) | DOCUMENTED (secondary) | https://www.coda-effects.com/2015/11/big-muff-mods-and-tweaks.html , https://www.kitrae.net/music/Big_Muff_Mods_and_Tweaks_Page.html |
| 2 | tone stack HP leg | C 3.9n + 22k+pot -> C2=10n, R2=22k (fc 723 Hz) | DOCUMENTED (secondary) | same |
| 3 | tone notch freq | ~1 kHz -> ~1.16 kHz | ESTIMATED (derived from #1,#2) | formula in section 3 |
| 4 | feedback cap | 470 pF (may be 560) -> 470 pF | DOCUMENTED (secondary) | https://www.kitrae.net/music/big_muff_guts.html |
| 5 | clip diodes | 1N914/1N4148 class -> 1N914 | DOCUMENTED (secondary) | kitrae.net / search snippet |
| 6 | transistor | 2N5088/BC239 class -> BC549C-class, hFE~500 | DOCUMENTED (secondary, forum-grade; 2N5088 = substitute) | kitrae.net snippet |
| 7 | power | ~9.6V/200mA, 3 mA -> 9V battery incl.; optional EHX9.6DC-200; 3 mA | DOCUMENTED | https://www.ehx.com/greenrussianbigmuffpi , https://shop.ehx.com/item/rus%20bm/ |
| 8 | model Vcc | 9.0 V | ESTIMATED (battery nominal; design choice) | - |
| 9 | enclosure size | 117x70 -> 114.3x69.9 mm | DOCUMENTED | EHX/retail 4.5x2.75x2.1 in |
| 10 | reissue ID | none -> Aug 2017, Gino Maisano, PCB EC-D155 Rev C | DOCUMENTED (secondary) | https://www.kitrae.net/music/big_muff_history2B.html |
| 11 | coupling cap | '100n-1u?' / 470n -> 100 nF | ESTIMATED (BMP typical) | - |
| 12 | reissue vs original cap difference | forum claim -> not modelled | OMIT | https://forum.seymourduncan.com/threads/so-hows-the-new-russian-big-muff.320280 (unverified) |
| 13 | pot values/tapers (100k A / B) | unchanged | ESTIMATED (family norm) | - |
| 14 | Zin 100k, Zout 10k | unchanged | ESTIMATED; no published figure found | EHX manual PDF not opened |
| 15 | Q1/Q4 gain, insertion loss, THD targets | unchanged | ESTIMATED | - |
| 16 | MIDI CC14..18 | 'default map' -> pedal-local virtual defaults; hardware has no MIDI | OMIT (as hardware fact) | - |
| 17 | variants sovtek-black-v8 / nyc-ramshead / `revision` enum | reserved -> removed | OMIT | - |
| 18 | at_to_sustain, velocity xfade | unchanged | OMIT (virtual extension, not hardware) | - |
| 19 | inconsistencies fixed | `Bypass(on=0)` -> Engaged(1=on); removed input_trim_db/fsw_mode rows; px scale conflict; '+4 dBu? NO' and '470nF?' messy text; dup 3 px/mm vs 1.6 px/mm | n/a | header v1.1 |
