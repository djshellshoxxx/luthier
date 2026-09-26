# Effects / technique DSP accuracy audit

Scope: the accuracy of the **effects and technique** DSP only — `Source/DSP/Slap`,
`Source/DSP/Noise` (Scrape, FretBuzz, PlayingNoise), `Source/DSP/Slide`,
`Source/DSP/String/Harmonics.h` and the harmonic-contact path in `StringEngine`,
`Source/DSP/Effects` (`PedalsDrive`, `PedalsMod`, `Pedal`, `EffectsChain`),
`Source/DSP/Amp` (`AmpEngine`, `ToneStack`, `RoomEngine`), `Source/DSP/Feedback`
(`FeedbackLoop`), and `Source/Modulation` (`ModMatrix`). Instrument bodies,
strings and pickups are the other audit's territory and are not re-reviewed
here except where an effect leans on one of their outputs.

Method: read every technique/effect spec in the brief against the source that
implements it, line by line, checking the physical/acoustic reasoning the code
gives for itself (most files cite the exact spec section and several carry
explicit build notes — `REALISM-B`, `FIX-CROSS` — recording where a draft
number was corrected against a measured build). Web-searched independent
references for the handful of claims worth checking against real hardware or
real string construction. Built `LuthierTests` (Release, Ninja/clang,
`scripts/setup_linux.sh`) and ran the full suite headless
(`xvfb-run build/LuthierTests_artefacts/Release/LuthierTests`) to confirm the
audit's read of the code against actual behaviour, not just the source text.

## Summary

This is, effect for effect, the most carefully physically-grounded DSP I have
reviewed under this kind of audit. Every engine below either matches its spec
and real-world acoustic behaviour, or has an explicit, documented reason for
where it diverges. **No "wrong approach" findings** — nothing here fakes a
physical effect with an unrelated shortcut (no sample-triggered "squeak
layer," no naive shelving-filter tone stack, no delay-line "reverb" standing
in for a diffuse tail). The two items below are the worst of what I found,
and neither is a wrong model — one is a spec-vs-implementation numeric
inconsistency, the other is an incomplete control layer over an otherwise
sound physical mechanism.

The task's specific worry — **the Shred Lead OD's historical oversampler
pitch-drift in the feedback tests** — is resolved. See the dedicated section
below.

| # | Effect / technique | Modelled behaviour | Expected behaviour + source | Verdict | Recommended fix |
|---|---|---|---|---|---|
| 1 | String squeak / pick chirp / scrape catch-rate | `windingPitchPerMm = 1 / ((diameterMm − coreDiameterMm) / 2)`, derived from `StringMaterials`' per-material `coreRatio` (e.g. 0.45 for nickel-plated steel). For a .046″ wound low E this computes to **≈3.1 wraps/mm**. | `string-squeak.md` §1's own worked example: *"A 0.046″ wound low E has roughly **6.5 wraps/mm**... A brisk position shift moves the hand around 400 mm/s. That gives about 2.6 kHz."* Real roundwound low-E wrap wire is commonly ~0.012″–0.014″ (~0.30–0.36 mm), which gives **~2.8–3.3 wraps/mm** — close to what the code computes, not to the spec's figure. | **Inconsistent.** The spec's own illustrative number looks physically too fine by roughly 2×; the code's geometric derivation is more plausible but not verified against real manufacturer wire gauges for every material/gauge row in `StringMaterials.cpp`. | Not pushed: this needs real string-gauge reference data (D'Addario/Ernie Ball spec sheets or similar) to settle which side is off, then the three worked examples in `string-squeak.md`/`pick-noise.md`/`string-scraping.md` need updating together, since they all cite the same 6.5 figure. **Proposal, not a fix** — see below. |
| 2 | Two-hand tapping | Hammer-on/pull-off/tap are auto-detected in `TechniqueEngine` (a legato note below a velocity threshold, within 150 ms) and rendered through `Excitation::Kind::HammerOn/PullOff/Tap` with **fixed** gain/length scaling (0.28 / 0.24 / 0.42 of a normal pluck). | `two-hand-tapping.md` specifies a dedicated `TapGesture` struct and a `TapEngine` module, plus seven-plus user controls: tap strength curve, lateral-flick amount (0–1), auto-pull-off toggle (default on), fret-snap toggle (default on), multi-finger max-concurrent (default 2), tap duration default (200 ms), MIDI-channel-2 trigger source. | **Incomplete, not wrong.** Grepped the whole tree for `lateralFlick`, `pullOffAfter`, `autoPullOff`, `fretSnap`, `multiFinger` and matching parameter names: none exist. What *is* implemented (a hammer/pull/tap excitation with a fixed gain) is physically reasonable as far as it goes, and the underlying mechanism it would need (a boundary-condition contact that shortens the sounding length) already exists for harmonics (finding below) and could be reused. | Not pushed: this is a feature build-out (a new engine plus ~7 parameters and their GUI), not a small correctness patch. **Proposal.** |
| 3 | Slap (thumb / pop / palm / body tap) | `SlapEngine` builds two things per strike: the string excitation (`Excitation::Kind::Slap`, contact point from `positionFraction`) and a fret-contact buzz event handed to the shared `FretBuzz` generator, scaled by `slap_fret_contact`/`snap_back` against the same `excess`-over-clearance model a bad setup uses. Ghost notes reuse `Damping::Chuck`. Double-thump timing and its brighter, quieter up-stroke (`velocityForLevelRatio`) match `string-slap-technique.md`/`bass-techniques.md` exactly. | `string-slap-technique.md`, `bass-techniques.md`. | **Accurate.** | None. |
| 4 | String scraping | Per-winding impulse train: `catches = travel_distance × windings_per_mm`, each catch an impulse into the string's excitation at the pick's instantaneous position, run through the same bridge-position comb a pluck uses so a catch near the 12th fret sounds different from one near the saddle. Deterministic per-winding irregularity (hashed seed), tension-load pitch bump under pressure, plain strings silently produce zero catches (not a special case — `windingsPerMm` returns 0). | `string-scraping.md`. | **Accurate** (module itself). Its one input, winding pitch, is finding 1 above. | None. |
| 5 | Fret buzz | Setup geometry (`SetupGeometry::clearanceMm`) models the truss-rod relief as a parabola peaking at fret 7, nut-to-saddle interpolation, and "only frets ahead of the fretted position can buzz." The buzz test evaluates displacement as a 3-mode weighted sine sum at the fret position, matching how a pluck-position comb really would excite each mode. Sitar mode's continuous grazing contact and the heatmap are wired to the same `sense()` used for the audio. | `fret-buzz.md`. | **Accurate.** | None. |
| 6 | Slide guitar | Bar-contact damping distinguishes full damping behind the bar (lap steel/dobro) from partial (bottleneck with a finger behind it); bar mass and material set both the clank spectrum and how much energy the bar itself absorbs; a rattling bar (low pressure) produces a genuine periodic re-contact clank rather than a fixed "rattle" sample. | `slide-guitar.md`, `slide-technique-controls.md`. | **Accurate.** | None. |
| 7 | Harmonics (natural/artificial/pinch/tapped) | Implements exactly the *n*-tap node-comb boundary condition `harmonic-realism.md` specifies (not a band-pass "harmonic mode" — a real touch/damper on the waveguide). Analytic node search (`findNode`, `n` in 2–8 maximising `e/√n`), the REALISM-B super-Gaussian pad-efficiency correction, tab-fret reading that resolves `<4>`/`<9>`/`<16>` to the true 3.86/8.84/15.87 nodes, and the sounding-pitch locator for keyboard players are all present, matching the spec's own numbered build-note corrections (`kOffNodeEfficiency = 0.1`, exponent 4, pinch graze at `0.5g`, etc). | `harmonic-realism.md` and its "Build notes (REALISM-B)". | **Accurate.** All 19 `HarmonicRealism` (`HR01`–`HR19`) tests pass. | None. |
| 8 | Overdrive / Distortion / Fuzz / Boost | Shared oversampled `DrivePedalBase`. Overdrive's default pre-clip high-pass is **720 Hz**, which is the exact corner frequency of a real TS9/TS808's clipping-stage high-pass (stock 4.7 kΩ/0.047 µF); Fuzz's three voicings use the right topology per pedal (Fuzz Face's starved-diode-like exponential curve, Big Muff's cascaded symmetric clipping, Tone Bender's harder-positive/splattier-negative asymmetry); Boost is a gentle near-clean tilt (Klon-ish). | Real hardware behaviour — [ElectroSmash Tube Screamer analysis](https://www.electrosmash.com/tube-screamer-analysis), [geofex "Technology of the Tube Screamer"](http://www.geofex.com/article_folders/tstech/tsxtech.htm). | **Accurate**, and the 720 Hz match in particular is a direct hit on the real circuit, not a coincidence of a round number. | None. |
| 9 | OD/Shred Lead oversampler + feedback-loop pitch drift | See dedicated section below. | | **Resolved.** | None. |
| 10 | Amp preamp / tone stack | Cascaded tube stages with a push-pull recombination and a small class-AB crossover notch (`0.004 × compression`, deliberately small — "a well-biased amp barely shows it"). `ToneStack` is a true 3-pot passive RC network (Fender/Marshall/Vox/Rectifier component sets) via bilinear-transformed nodal analysis, not a cascade of shelving filters — the code explicitly calls out "pitfall 10" (naive EQ chains get the bass/mid/treble *interaction* wrong) and avoids it. | `engine.md` pitfall 10; D. Yeh's published Bassman tone-stack analysis (cited in-code). | **Accurate.** `ReviewRegression::theToneStackIsPassiveAtEverySetting` and `Amp::neutralToneStackIsFlatWithin1dB` pass. | None. |
| 11 | Reverb (FDN) / Spring reverb | 8-line FDN with mutually-prime delay lengths and 4-stage input diffusion (avoids the small-FDN "metallic ring" failure mode); spring reverb's characteristic "boing" comes from a genuine all-pass dispersion cascade (frequency-dependent group delay), not an EQ curve pretending to be one. | Standard FDN/dispersive-line reverb theory; the code's own comments name the failure modes each design choice avoids. | **Accurate.** | None. |
| 12 | Compressor / Gate / Wah / Envelope filter / Octaver / Pitch shifter / Chorus / Doubler / Phaser / Flanger / Tremolo / Rotary / Delay | Each cites and reproduces a specific real behaviour: optical vs. FET compressor timing ratio (2.2× vs. 0.55× the same knob), a wah as a resonant peak *added to* the dry signal rather than a pure notch/bandpass, a monophonic octave-down built from zero-crossing flip-flops that **intentionally** glitches on chords (matching real analogue octave pedals, not a bug), a two-mic rotary speaker with independent Doppler delay-modulation and AM per horn/drum plus a realistic spin-up/spin-down time constant, tape delay's wow (slow sine) + flutter (fast random) + saturation on hot repeats. | Each pedal's own doc comment names the hardware it reproduces; general effects-pedal literature. | **Accurate.** | None. |
| 13 | Modulation matrix | Control rate = `max(128, blockSize/32)`, additive sum-and-clamp over the base value, matches `modulation-matrix.md` §0.1 and §3 exactly. | `modulation-matrix.md`. | **Accurate** (this one isn't really an acoustic-realism question — it's a routing/control system — so it got a lighter pass than the others). | None. |

## The Shred Lead OD oversampler / feedback pitch-drift history — resolved

The brief specifically asked me to check this. It is fixed, and thoroughly so:

**Two oversampler bugs, both fixed with regression tests** (`Source/DSP/Common/Oversampler.h`):
1. Re-sending the same oversampling factor used to clear the half-band
   filters unconditionally, clicking the amp on every structural-parameter
   change even when nothing changed (`Oversampler::setFactor` now early-outs
   when the factor is unchanged). Covered by
   `ReviewRegression::resendingTheSameOversamplingFactorDoesNotReset`.
2. The half-band all-pass branches read `x[n-2]/y[n-2]` instead of
   `x[n-1]/y[n-1]` at the base rate — i.e. `A(z^4)` at the oversampled rate
   instead of the complementary `A(z^2)` pair the polyphase half-band design
   needs. This weakened image rejection by 11–14 dB, drooped the passband
   (−4 dB at 16.8 kHz for a 2× stage at 48 kHz), and reported roughly half
   the true group delay (6.4/9.5/11.1 samples measured against 3/5/6
   reported). Fixed; covered by
   `ReviewRegression::theOversamplerDelaysWhatItReports`.

**The feedback loop's phase-at-the-note bug** (`docs/coverage/FIX-CROSS.md`,
`Source/DSP/Feedback/FeedbackLoop.cpp`): the loop's only "phase" was whatever
the block size, the plugin's processing latency (which includes the drive
pedals' and the amp's oversamplers) and the air happened to add up to. Since
each string only hears a narrow band around its own partial, a delay that
isn't a whole number of periods puts the note somewhere on the partial's
phase circle at random — at 0.5 m, Shred Lead's E4 landed at 174°, almost
exactly anti-phase, and never took over regardless of gain. This also meant
the octave-bias lock (`feedbackOctaveBias`) moved between merges purely
because the *oversampling factor* changed, nothing else. Fixed by padding the
total delay (block + air + processing latency) up to the **next whole period
of the string's own target partial** (`FeedbackLoop::updateReadDelay`,
cubic-interpolated read) so the loop is always in phase on the partial it's
supposed to reinforce, at any block size, quality level or distance.
`kInjectionGain` and `kResonantShare` were recalibrated afterward, since their
old values had partly been compensating for the accidental phase.

**Verified, not just read**: built `LuthierTests` fresh and ran
`Feedback::eachStringHearsItsOwnNote` (octave bias 0 and 1, asserts within 5
cents of the string's own partial) and
`Feedback::aLoudRigTakesOverAndACleanOneDoesNot` (Shred Lead at 100%/0.5 m
must take over; Clean Double-Cut Funk at 20% must not) — **both pass**, along
with the two oversampler regression tests above and
`Latency::oversamplerReportsItsGroupDelay`. This is resolved and guarded
against recurrence.

## Data gaps

- **Winding-pitch calibration** (finding 1): needs real string-gauge
  reference data to determine whether `StringMaterials`' `coreRatio` table or
  `string-squeak.md`'s worked example is the one that's off, since they
  currently disagree by roughly a factor of 2 and both can't be right.
- **Two-hand tapping's control layer** (finding 2): a whole spec's worth of
  user-facing controls (7+ parameters, a dedicated `TapEngine`) has no
  implementation to check the accuracy of yet.
- **`FeedbackLoop::setBodyCoupling`** carries the code's own
  `MODEL-GAPS (TODO 2k)` marker: how much more feedback-prone a hollow or
  chambered body should be is currently a hand-picked `sqrt()` relationship
  against `part-acoustics.md`'s table, "left at the reference" for acoustic
  guitars because this loop models a magnetic pickup hearing the amp, which
  an acoustic doesn't have. This sits right at the edge of this audit's scope
  (it's a feedback *parameter*, but what it scales is a body-acoustics
  question) — noting it here since it touches the feedback loop, but the fix
  belongs with whoever owns body/part acoustics.

## Fixes applied

**None pushed.** Both real findings (1 and 2) need either independent
reference data or a genuine feature build-out, not a small, obviously-correct
patch — per the brief's instruction to leave anything larger than that as a
proposal. Everything else reviewed already matches its spec and the real
acoustic behaviour it claims to reproduce, including the specific
oversampler/feedback-loop history the brief flagged, which is resolved and
covered by its own regression tests.

## Tests run

Built `LuthierTests` (Release/Ninja/clang via `scripts/setup_linux.sh`) and
ran the full suite headless under `xvfb-run`. All effects/technique-relevant
suites pass: `Feedback` (6/6), `Buzz`, `Scrape`, `Slap`, `Squeak`, `PickNoise`,
`NoisePool`, `Slide`, `HarmonicRealism` (`HR01`–`HR19`, 19/19), `Effects`,
`Amp`, `ReviewRegression` (incl. both oversampler regressions), `Latency`.
The only failure in the full run, `FacesIntegration::theAdvancedAmpSectionHasItsControlsOnTheFace`,
is a GUI knob-size layout check unrelated to audio DSP (pre-existing on this
branch, not touched by anything in this audit's scope) — see
`build/logs/full_run.log`.
