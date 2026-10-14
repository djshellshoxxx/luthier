# Reference Pedal Library — research catalog (2026-10-14)

Scope: separate user-addable, chainable **Reference Pedals** slide-out; legacy `PedalRack` and both pre/post `EffectsChain` instances unchanged. No claim of authentic sonic reproduction until reference capture and tests pass. Entries below are **research-backed build specifications**, not shipped pedal models.

**Image policy:** The `reference_photo_page` is the product manufacturer's model-specific page (or official image library). It is a verified source for a photograph of the correct product, **not a locally copied or redistributable image**. Obtain license/permission and image asset checksum before bundling. Where a vintage circuit is documented but the modern model photographed differs, the model file explicitly says so.

**Evidence grades:** A = full schematic and component analysis adequate to implement a topology-specific approximation; B = documented exact external controls/behavior but hidden circuitry or converter details; C = effect-family analogy only, not suitable for branded model claim. Even grade A needs hardware measurements to attain audio-reference validation (waveform, FRF, dynamics, noise, latency, bypass, headroom).

Model | Family | Grade | Specs | Official photo source
---|---|---|---|---
[BOSS DS-1 (modern op-amp revision)](models/boss-ds1.md) | Distortion | A | [spec](models/boss-ds1.md) | [model-identifiable source](https://www.boss.info/us/support/by_product/ds-1/product_images/)
[Ibanez TS808 Tube Screamer (original topology)](models/ibanez-ts808.md) | Overdrive | A | [spec](models/ibanez-ts808.md) | [model-identifiable source](https://www.ibanez.com/eu/products/detail/ts808_99.html)
[Electro-Harmonix NYC Big Muff Pi (documented transistor variant)](models/ehx-big-muff-nyc.md) | Fuzz / sustainer | A | [spec](models/ehx-big-muff-nyc.md) | [model-identifiable source](https://www.ehx.com/products/big-muff-pi/)
[Pro Co RAT (LM308 classic)](models/proco-rat.md) | Distortion | A | [spec](models/proco-rat.md) | [model-identifiable source](https://www.ratdistortion.com/)
[MXR Phase 90 (vintage Script circuit)](models/mxr-phase90-script.md) | Phaser | A | [spec](models/mxr-phase90-script.md) | [model-identifiable source](https://www.jimdunlop.com/mxr-phase-90/)
[BOSS CE-2 (vintage mono chorus)](models/boss-ce2.md) | Chorus | A | [spec](models/boss-ce2.md) | [model-identifiable source](https://www.boss.info/global/products/ce-2w/)
[MXR Dyna Comp (classic OTA circuit)](models/mxr-dynacomp.md) | Compressor | A | [spec](models/mxr-dynacomp.md) | [model-identifiable source](https://www.jimdunlop.com/mxr-dyna-comp-compressor/)
[MXR Distortion+ (M104 topology)](models/mxr-distortion-plus.md) | Distortion | A | [spec](models/mxr-distortion-plus.md) | [model-identifiable source](https://www.jimdunlop.com/mxr-distortion/)
[BOSS SD-1 Super OverDrive](models/boss-sd1.md) | Overdrive | B | [spec](models/boss-sd1.md) | [model-identifiable source](https://www.boss.info/global/products/sd-1/)
[BOSS DD-3T Digital Delay](models/boss-dd3t.md) | Delay / short hold looper | B | [spec](models/boss-dd3t.md) | [model-identifiable source](https://www.boss.info/ca/products/dd-3t/)
[BOSS RC-1 Loop Station](models/boss-rc1.md) | Stereo phrase looper | B | [spec](models/boss-rc1.md) | [model-identifiable source](https://www.boss.info/global/products/rc-1/)
[BOSS RC-5 Loop Station](models/boss-rc5.md) | Stereo looper / rhythm | B | [spec](models/boss-rc5.md) | [model-identifiable source](https://www.boss.info/global/products/rc-5/)
[TC Electronic Ditto Looper (original mono)](models/tc-ditto.md) | Phrase looper | B | [spec](models/tc-ditto.md) | [model-identifiable source](https://www.tcelectronic.com/en/products/0709-AFB)
[Electro-Harmonix Nano Looper 360](models/ehx-nano-360.md) | Mono phrase looper | B | [spec](models/ehx-nano-360.md) | [model-identifiable source](https://www.ehx.com/products/nano-looper-360/)
[EHX Big Muff Pi with Tone Wicker (2008 version)](models/ehx-muff-wicker.md) | Fuzz / sustainer | B | [spec](models/ehx-muff-wicker.md) | [model-identifiable source](https://www.ehx.com/products/big-muff-pi-with-tone-wicker/)
[BOSS CE-2W (three-mode reissue)](models/boss-ce2w.md) | Analog BBD chorus / vibrato | B | [spec](models/boss-ce2w.md) | [model-identifiable source](https://www.boss.info/global/products/ce-2w/)
[Electro-Harmonix Deluxe Big Muff Pi](models/ehx-deluxe-muff.md) | Fuzz / gate / parametric mid | B | [spec](models/ehx-deluxe-muff.md) | [model-identifiable source](https://www.ehx.com/products/deluxe-big-muff-pi/)
[EHX Green Russian Big Muff Pi reissue](models/ehx-green-russian.md) | Fuzz | B | [spec](models/ehx-green-russian.md) | [model-identifiable source](https://www.ehx.com/products/green-russian-big-muff-pi/)

Other families for generic (non-brand-matched) modelling: clean boost, buffer, tuner, wah, auto-wah, filter, octave-down analog divider, harmonizer, ring modulation, tremolo, vibrato, harmonic tremolo, rotary, flanger, univibe, compressor, limiter, envelope gate, parametric/graphic EQ, pitch bend, volume, delay (analog BBD/tape/digital/reverse), reverb (spring/plate/room), granular/stutter, freeze, synth, multi-track looper. These are **not** considered reference-pedal variants without sources and physical validation.

## Build order

1. Master `ARCHITECTURE.md` and repeatable `skills/reference-pedal-modeler/SKILL.md`.
2. Initially prove bypass, chain ordering, serial IDs and state with a **non-branded** reference board and 1-2 model adapters; the default new board must be OFF.
3. Start with TS808 / DS-1 / Phase 90 / Big Muff / Dyna Comp and a basic Ditto-style looper; these span nonlinear, time variant and state machine DSP.
4. Add named variants only after completing each model's pass/fail test plan and recording evidence. Keep descriptions honest.
5. More advanced RC-5 memory/reverse/MIDI/rhythm require independent worker and storage specs; do not conflate with the existing main Luthier looper.

## Sources and evidence

The linked product pages and ElectroSmash analyses in each model spec are the traceable evidence. Component analysis is published with different circuit revisions. Record exact revision, BOM values, pot taper, tolerance, power rail, A/D converters for digital models, and the manufacturer revision date before claiming faithful audio.
