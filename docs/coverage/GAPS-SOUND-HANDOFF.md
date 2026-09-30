# GAPS-SOUND handoff

Context cap reached (long session: full spec reading + multiple build/test
cycles + a full-suite run). Rows completed and their tests are in
`docs/coverage/GAPS-SOUND.md`; this file is the "what's left" map for the
next helper, built from a full read of all 11 spec sections against
`docs/audit/SPEC_SWEEP.md` (fetch `origin/claude/luthier-spec-sweep` and
`git show ...:docs/audit/SPEC_SWEEP.md` for row text/work-list detail - it is
long, ~6300 lines, read one section at a time with `grep -n "^## " ` then
`Read` the offset range).

## Done this session (see GAPS-SOUND.md for the full table)

part-acoustics PA-6, PA-13, PA-24, PA-27, PA-28, PA-29, PA-33, PA-34, PA-48
(all tests; PA-33 also a small `mapSpec` fallback-table addition); engine
EN-16, EN-52, EN-90 (EN-90 also a real fix: FDN feedback cap 0.9985 -> 0.998
in RoomEngine and PedalsMod's ReverbPedal); fret-buzz FB-8; modulation-matrix
MM-1, MM-41; routing-io RIO-4, RIO-13, RIO-14; string-squeak SQ-16, SQ-17;
slide-guitar SG-12 (formula-level only, see note below).

**PA-40 and PN-9 are DEFERRED** - read the notes in GAPS-SOUND.md before
touching either. Short version: PN-9's spec-correct default (0.73 mm instead
of 1.07 mm) silently changes every factory preset's audio because none of
them set `pick_thickness` explicitly, which breaks the normalization golden
hashes and factory table (owned by another workstream). Fixing it for real
needs a coordinated regeneration of that data, not just a parameter edit.

**Full suite**: ran once clean (before the PN-9 revert), 9/1390 failed. After
reverting PN-9, only ON02's `p06_gown_phrase`/`p06_gown_chord` mismatch
remains from that batch, and it is pre-existing (fails on `origin/claude/
luthier-cloud-session-5lzlix` independent of anything in this branch - verify
again before assuming so, but that was true when checked). Other failures
seen in that run and NOT this branch's problem: `everyFactoryPresetPlaysEveryPhrase`
/ `snapshotsAndPresetMorph` (factory preset #10 "P-Bass Flatwound" renders
silent), `everyAutomatableParameterHasAVisibleControl`, `ON27_Cache`,
`CQ10_aRingingNoteKeepsItsStagesUntilReExcited`,
`CQ22_everyTimerDrivenUiClassIsRegisteredOrAllowListed`. Worth flagging to
the coordinator if nobody else has.

## Cross-spec opportunity

MM-10 (write the mod matrix into `.luthierpreset`) and RIO-30/31/32 (write
routing into `.luthierpreset`) both want the same `captureExtraBlocks` hook
on `PresetManager`/`LuthierAudioProcessor` (see the routing-io work list for
RIO-30's description - it explicitly says the same hook should carry
`modulation`, `snapshots` and `rhythmEngine`). Worth doing once for all of
them rather than three times.

## Remaining rows by spec (priority-ish order within each; effort from the
sweep's work list, not re-verified)

### part-acoustics (largest remaining chunk)
- PA-9 (M) neck wood/density -> neck mass + dead-spot frequency
- PA-35 (M) bridge.piezo -> hasPiezo
- PA-46 (M) wiring.switching -> PickupSelector topology
- PA-49 (M) strings.core round/hex
- PA-51 (S) tension_kg[] override
- PA-52 (M) wound mu from core diameter + wrap geometry
- PA-60 (M) loss-domain damping composition (1/Q sums instead of multiplying sustainScale)
- PA-14 (M) consume airResonanceHz/Q in BodyEngine; PA-15 (S) consume bodyGainDb; PA-56 (S) consume finishDampingDb
- PA-7 (M) body wood table actually drives modes (resonanceTrim wiring); PA-8 (M) tanδ -> Q lossScale
- PA-23 (M) fretboard.radius_mm -> buzz clearance (needs FretBuzz too)
- PA-54 (S) pickguard top damping; PA-50 (S) PlayingNoise reads d.windingPitchPerMm instead of recomputing
- PA-20 DEFER (fold into PA-9); PA-5 (S) comment the fitted constants
- NO-TEST only, fast: PA-4, PA-16, PA-18, PA-22, PA-36, PA-37, PA-41, PA-57, PA-58 (covered by PA-36's test per work list)
- PA-T1/PA-T7 (M) upgrade to rendered-audio comparisons once the fields above land

### modulation-matrix
- MM-10 (M) preset file carries the matrix (see cross-spec note) - unlocks MM-6, MM-T3
- MM-2 (M) true linear ramp instead of one-pole step
- MM-32 (L) per-string virtual destinations; MM-35 (M) snapshot_morph param; MM-34 (S) output_pan param; MM-33 (M) rhythm_density/hand_position params
- MM-48 (M) per-source colour tags; MM-39 (M) Custom curve; MM-49 (S) snapshot "includes modulation" flag; MM-50 (S) unknown-destination warning banner
- GUI-only, no test needed first: MM-12, MM-14, MM-18, MM-19, MM-20, MM-22, MM-25, MM-40 (route table Offset column), MM-28 (macro naming)
- NO-TEST only, fast: MM-7, MM-8, MM-9, MM-15, MM-16, MM-23 (also needs a small internal-rate/tap-sync feature), MM-26, MM-27, MM-29, MM-44, MM-47, MM-51
- MM-43 DEFER (document in spec/DECISIONS.md, per work list)

### tone-match (nothing done yet - real functional gaps, start here for impact)
- TM-6 (M) actually process the body IR slot (currently never called)
- TM-7 (M) cab slots should replace mic1/mic2 IR, not convolve main out in series
- TM-28 (L) EQ-match result needs its own filter stage + position choice, not dumped into cab slot 2
- TM-17 (M) play the cab-match test signal out of Aux 1
- TM-5 (M) move analysis off the message thread
- Everything else in the spec's work list is NO-TEST/NO-GUI and smaller (TM-1,2,3,8,9,11,15,16,21,23,25,30,31,32,33,34,37,38,39,41,42,43,44,45)

### engine
- EN-72 (M) route UI writes (detune, panic) through the processor instead of direct audio-thread calls from the message thread
- EN-59 (M) Custom amp stage controls; EN-21/EN-23/EN-27 (M) custom temperament/tuning/per-string intonation editors; EN-17 (M) CC map editor; EN-18/EN-54 (S) aftertouch-target and per-string-whammy params
- EN-25 (S) re-roll detune button; EN-63 (S) 8s->30s warmup or a DECISIONS note
- NO-TEST only: EN-49, EN-50, EN-65, EN-70, EN-75, EN-77, EN-91, EN-94, EN-95
- EN-45/EN-57 DEFER (already noted in the sweep); EN-86/EN-87 are manual QA doc entries, not code

### input-routing (nothing done - all effort M/L, needs care with MIDI plumbing)
IR-3 (S, consuming MIDI Learn) is the easiest starting point; IR-4, IR-5,
IR-7, IR-9, IR-11, IR-12, IR-14, IR-15, IR-16, IR-17, IR-18, IR-19, IR-20,
IR-24, IR-25, IR-26, IR-27, IR-28 follow; IR-29/IR-30 are new test-suite
scaffolding once several of the above land. Read input-routing's work list
in the sweep in full before starting - the consumer order matters and
several rows share infrastructure (a `Tests/InputRouting/` suite, IR-9's
practice FIFO also serves IR-27).

### routing-io
- RIO-11 (M) sidechain-compressor pedal type
- RIO-30/31/32 (M, shared with MM-10, see cross-spec note)
- NO-TEST only: RIO-6, RIO-27
- RIO-25 (S) layout selector becomes a real (informational) list

### advanced-ranges
- AR-15 (M) modulation family setter clamps (LFO/env/seq/follower)
- AR-10 (M) route changeRanges through the structural command queue
- AR-21/AR-T10 (S) snapshots store plain values and clamp on recall instead of re-mapping normalised ones
- NO-TEST only, fast: AR-6, AR-11, AR-12, AR-22, AR-24, AR-28, AR-30, AR-31, AR-36, AR-T2
- AR-34 (S) telemetry `advanced_ranges_used` boolean

### string-squeak
- SQ-8 (M) route ChordVoicer revoice + imported SLIDE events through onShift
- SQ-24 (M) winding selector mirrors the Workshop string part
- SQ-27 (S) fret wear raises squeak
- NO-TEST only: SQ-9/SQ-T7 (bend/vibrato don't squeak), SQ-25 (squeak+buzz coexist, shared with FB-25), SQ-T1 (factory sweep), SQ-T10 (no-alloc)

### fret-buzz
- FB-21 (M) fret wear moves the buzz; FB-26 (M) bend moves the buzz up the neck
- FB-9 (M) fret-material brightness field feeding FretBuzz's spectrum (distinct from part-acoustics' fretBrightness - PA already has the material table, this wires it into `SetupGeometry`/`FretBuzz::process`)
- NO-TEST only: FB-13 (tried and backed off - needs a careful render-vs-note-RMS measurement, see below), FB-18 (heatmap staleness - needs a settable clock; `BuzzHeatmap`/`NoiseEventStrip` both hard-code `juce::Time`), FB-25 (shared with SQ-25), FB-T9, FB-T10

### slide-guitar
- SG-6 (M) bar length/diameter audible effect (blocked on the Workshop bar part actually reaching the engine - check whether `visual`/`techniques` landed `setSlidePart -> setSlideBar` yet)
- SG-24 (M) tuning popover shows continuous pitch in Slide Mode
- SG-10 (S) pressure choke term; SG-14 (S) friction-follows-material-and-speed test; SG-17 (S) low-setup-buzzes-under-bar test
- SG-26 (M) slant into capture + Generic-profile pitch-bend rendering
- SG-T10 (S) no-alloc test
- SG-12 is PARTIAL: the formula (`vibratoCents`) has a test now; an engine-level test through `vibrato_depth` and the damped-segment sustain scale is still open

### pick-noise
- PN-17 (M) Easy-mode rake gesture button
- PN-15 (S) decay/wear/position shape test; PN-6/PN-T5 (S) excitation-vs-surface and noise-rides-the-instrument tests
- PN-4, PN-8 are DEFER candidates per the sweep's own work list (texture-per-material, Ultex/Tortex/Stone) - read spec/DECISIONS.md first, they may already be settled

## Notes on things tried and backed off

- **FB-13** (light buzz 30-40 dB under the note): `FretBuzz::levelFor` is a
  fixed -22 dB scale times an excess-ratio clamp, not empirically 30-40 dB
  under a rendered note; a real render-vs-note-RMS test is do-able (same
  pattern as `PickNoise::aClickSitsAboutThirtyDecibelsUnderTheNote` in
  NoiseTests.cpp) but takes real engine-level care to land in-band
  reliably - didn't want to ship something flaky. Worth another attempt.
- **FB-18** (heatmap staleness): `BuzzHeatmap::isStale()` and
  `NoiseEventStrip::isStale()` both read `juce::Time::getMillisecondCounterHiRes()`
  directly with no injectable clock, so testing the 2-second staleness
  transition means either sleeping for real (slow, avoided) or adding a
  small test-only clock hook to both classes (reasonable, just didn't have
  the budget left this session).
- Ran the **full suite once** (see above) - do it again near the end of the
  next batch too, per the token rules.
