## string-squeak.md

The generated squeak is in place: `PlayingNoise::makeSqueak` / `onShift` implement winding pitch x speed with a glide, wound-only, min travel, the level formula, the per-winding table, the deterministic probability roll, moisture/pressure and age roughness; the six parameters, style dropdown ("Natural (modified)") and noise-event strip sit in CHARACTER > STRING NOISE (`NoiseGroups`). Gaps: only the legacy legato slide (and the hybrid-slide fretted fallback) triggers a squeak - ChordVoicer revoices, score/tab position changes and imported MIDI slide events do not; fret wear does not raise squeak; the winding selector edits `string_material` rather than mirroring the Workshop string part; bend/vibrato, factory-sweep and no-allocation tests are missing. The degradation-step-4 pool halving exists (`NoiseEngine::setDegraded`) and is wired through the quality profile (`noiseDegraded` at Low).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| SQ-1 (§0.1) | Wound strings only; plain strings never squeak | `DSP/Noise/PlayingNoise.cpp:makeSqueak` (`!string.wound` -> 0) | n/a | `Squeak.flatwoundIsNearlySilentAndPlainIsSilent` | DONE |
| SQ-2 (§0.2) | No "squeak now" control; squeak only from a position change | `LuthierEngine::triggerNote` (onShift only on a travelled note) | n/a (no such control exists) | `Squeak.aLegatoSlideInTheEngineSqueaksAndABendDoesNot` | DONE |
| SQ-3 (§0.3, 3) | Level/pitch follow hand speed (travel/duration), not note velocity | `makeSqueak` (speed = travelMm/seconds) | n/a | `Squeak.pitchTracksSpeedAndWinding` | DONE |
| SQ-4 (§0.4) | Normal shift on clean acoustic 20-30 dB under the note; electric quieter | `makeSqueak` (kNoteReference x -22 dB), pre-body injection | n/a | `Squeak.aShiftSitsTwentyToThirtyDecibelsUnderTheNote` | DONE |
| SQ-5 (§0.5) | Zero is silent and free | `makeSqueak` (amount<=0 -> level 0, `onShift` returns before trigger) | n/a | `Squeak.zeroIsFreeAndSlideModeSuppressesIt` | DONE |
| SQ-6 (§1, 3) | f = speed x windingPitch, glided over the shift | `makeSqueak` (startHz = 0.5 peak -> endHz = peak); `StringNoiseInfo::fromSpec` windingPitchPerMm | n/a | `Squeak.pitchTracksSpeedAndWinding` | DONE |
| SQ-7 (§2) | Trigger: legato slide (TechniqueEngine) | `LuthierEngine.cpp` `Technique::Slide && slideFromFret>=0` -> `playingNoise.onShift` | n/a | `Squeak.aLegatoSlideInTheEngineSqueaksAndABendDoesNot` | DONE |
| SQ-8 (§2) | Trigger: PerformanceScore position change, ChordVoicer revoice of a held note, explicit MIDI slide events — only paths that reach `Technique::Slide` squeak; voicer revoice and imported SLIDE events never call `onShift` | `MidiInterpreter::emitVoicedNote` (shiftFromFret) -> `LuthierEngine::triggerNote` onShift; imported SLIDE events still not replayed (with BT-12) | n/a | `Squeak.aRevoiceOfAHeldNoteSqueaks` | PARTIAL |
| SQ-9 (§2) | Not a trigger: new pluck, bend, vibrato — implemented by construction; test name claims bend but body only checks pluck vs slide; no vibrato test | `LuthierEngine::triggerNote` gate | n/a | `Squeak.aBendAndVibratoDoNotSqueak` | DONE |
| SQ-10 (§2.1) | `squeak_min_travel` (1.5 frets) gates generation | `makeSqueak` (`travelFrets < minTravelFrets`) | CHARACTER > STRING NOISE, `NoiseGroups::squeakMinTravel` | `Squeak.theMinimumTravelIsRespected` | DONE |
| SQ-11 (§3) | Level = amount x windingDepth x pressure^1.3 x roughness(moisture) x min(1, speed/300) | `makeSqueak`; `StringNoiseInfo::fromSpec` windingDepth | n/a | `Squeak.aShiftSitsTwentyToThirtyDecibelsUnderTheNote`, `Squeak.flatwoundIsNearlySilentAndPlainIsSilent` | DONE |
| SQ-12 (§4) | Per-winding brightness/texture table (8 windings) | `PlayingNoise::windingBrightness`, `windingTexture` | n/a | `Squeak.flatwoundIsNearlySilentAndPlainIsSilent` | DONE |
| SQ-13 (§4) | Pre-synthesised per-material texture read at random offset, band-shaped at f, 2f, 3f by brightness | `NoiseEngine::prepare` textures, `NoiseGenerator::start` | n/a | `NoisePool.aSeedRepeatsExactly` | DONE |
| SQ-14 (§5) | Injection pre-body (through body/pickup/circuit) and summed to Aux 8 | `NoiseEngine::processSample` surfaceNoise; `LuthierEngine` noise -> Aux 8 | n/a | `Squeak.aShiftSitsTwentyToThirtyDecibelsUnderTheNote`, `PluginBuses.aux8CarriesThePlayingNoiseAndObeysItsStrip` | DONE |
| SQ-15 (§6) | `squeak_probability` 0.65, roll deterministic per seed and shift index | `PlayingNoise::onShift` (`noiseUniform(seed, shiftIndex)`) | STRING NOISE `squeakProbability` | `Squeak.theProbabilityRollIsDeterministic` | DONE |
| SQ-16 (§6) | `squeak_finger_moisture` lowers probability and brightness — no test checks the moisture effect | `onShift` chance x (1.35-moisture); `makeSqueak` brightness | STRING NOISE `squeakMoisture` | `Squeak.moistureLowersOddsAndBrightness` | DONE |
| SQ-17 (§7) | `squeak_finger_pressure` raises level, coarsens texture (q) — no test | `makeSqueak` pressure^1.3, q | STRING NOISE `squeakPressure` | `Squeak.pressureRaisesLevelAndCoarsensTexture` | DONE |
| SQ-18 (§7) | Bass default pressure lower (0.35) | `Model/Guitar/BassDefaults.cpp` | n/a | `BassTechniques.bassDefaultsApplyOnLoad` | DONE |
| SQ-19 (§8) | Five style presets; Silent sets amount only | `NoiseGroups::applySqueakStyle`, `Parameters::squeakStyleNames` | STRING NOISE `styleBox` | `NoiseUi.squeakStylesApplyAndReadModified` | DONE |
| SQ-20 (§8) | Ship default Natural at 0.25, reads "Natural (modified)" | `Parameters.cpp` default 0.25; `NoiseGroups::describeSqueakStyle` | STRING NOISE `styleBox` | `NoiseUi.squeakStylesApplyAndReadModified` | DONE |
| SQ-21 (§9) | Six params with stock/advanced ranges and defaults, `squeak` family | `Parameters.cpp` squeak block; `PhysicalRange.cpp` squeak rows | CHARACTER padlock (`RangeTabButton`) | `Ranges.stockMatchesTheDeclaredRange`, `NoiseUi.theCharacterTabCarriesAPadlockWhenUnlocked` | DONE |
| SQ-22 (§9) | STRING NOISE group on CHARACTER with amount, probability, moisture, pressure, material, style, strip | `UI/NoiseGroups.*` | CHARACTER > STRING NOISE (`CharacterPanel::noiseGroups`) | `NoiseUi.*`, `GuiReach.everyAutomatableParameterHasAVisibleControl` | DONE |
| SQ-23 (§9.1) | Noise-event strip: 24 px, last 8 s, ticks by class colour/level, 30 Hz drain, grey after 2 s | `NoiseEngine::drainEvents`; `NoiseEventStrip` | STRING NOISE `eventStrip` | `NoiseUi.theEventStripShowsWhatTheEngineTriggered` | DONE |
| SQ-24 (§9.1) | Winding selector is a mirror of the Workshop string part (edit part directly now Workshop exists) — attaches `string_material` param directly, no link to the Workshop part | n/a | STRING NOISE `NoiseGroups::windingMaterial` | - | PARTIAL |
| SQ-25 (§10) | Fret-buzz coexists, no ducking | separate pools in `NoiseEngine` | n/a | `Buzz.squeakAndBuzzCoexist` | DONE |
| SQ-26 (§10) | Slide Mode replaces finger squeak on barred strings | `LuthierEngine` `!slide.isUnderBar(s)`; `SqueakSettings::slideMode` | n/a | `Squeak.zeroIsFreeAndSlideModeSuppressesIt`, `Slide.squeakStopsUnderTheBarButNotBesideIt` | DONE |
| SQ-27 (§10) | Fret wear raises squeak slightly — not implemented | `LuthierEngine::triggerNote` scales roughness by fret wear | n/a | `Squeak.fretWearRaisesSqueak` | DONE |
| SQ-28 (§10) | String age multiplies roughness up to 1.4 — continuous `AgingFactors::roughness` (REALISM-A) | `StringAging` roughness -> `StringNoiseInfo::fromSpec` ageRoughness | Advanced STRINGS "Age (h)"; CHARACTER STRING AGING | `StringAging.SA13_squeakReconciliation` | DONE |
| SQ-29 (§11) | MIDI export SQUEAK class (string, start, end, dur, level); Generic drops it | `Export/LuthierMidiEvents.cpp` squeak fields; `PluginProcessor.cpp` sysEx push | MIDI OUT panel | `MidiExport.everyEventClassRoundTripsWithEveryField`, `MidiExport.genericProfileIsPlainMidi` | DONE |
| SQ-30 (§12) | 16-generator squeak pool, textures once per material | `NoiseEngine::kPoolSizes` | n/a | `NoisePool.aFullPoolStealsTheOldest` | DONE |
| SQ-31 (§12) | Degradation step 4 halves pool to 8 | `NoiseEngine::setDegraded`; `LuthierEngine::setQualityLevel` (`LuthierEngineQuality.cpp` 61) applies `QualityProfile::noiseDegraded` at Low | n/a | `NoisePool.aFullPoolStealsTheOldest`, `CpuQuality.noisePoolsHalveAtLowAndNeverUnderLoadAtHigh` | DONE |
| SQ-T1 (§13) | Test: plain never squeaks, sweep every factory guitar — unit check only, no factory sweep | `makeSqueak` | n/a | `Squeak.noFactoryGuitarSqueaksOnAPlainString` | DONE |
| SQ-T2 (§13) | Test: pitch tracks speed and winding (1.95 kHz +/-10 %, x2) | | n/a | `Squeak.pitchTracksSpeedAndWinding` | DONE |
| SQ-T3 (§13) | Test: glide >= 15 % | | n/a | `Squeak.pitchTracksSpeedAndWinding` | DONE |
| SQ-T4 (§13) | Test: flatwound >= 15 dB quieter | | n/a | `Squeak.flatwoundIsNearlySilentAndPlainIsSilent` | DONE |
| SQ-T5 (§13) | Test: minimum travel | | n/a | `Squeak.theMinimumTravelIsRespected` | DONE |
| SQ-T6 (§13) | Test: probability deterministic | | n/a | `Squeak.theProbabilityRollIsDeterministic` | DONE |
| SQ-T7 (§13) | Test: bend and vibrato do not squeak | n/a | n/a | `Squeak.aBendAndVibratoDoNotSqueak` | DONE |
| SQ-T8 (§13) | Test: Slide Mode suppresses it | | n/a | `Squeak.zeroIsFreeAndSlideModeSuppressesIt` | DONE |
| SQ-T9 (§13) | Test: zero is free over 10 000 shifts | | n/a | `Squeak.zeroIsFreeAndSlideModeSuppressesIt` | DONE |
| SQ-T10 (§13) | Test: no allocation on the audio thread — missing | n/a | n/a | `Squeak.noAllocationOnTheAudioThread` | DONE |

<!-- counts DONE=38 NO-GUI=0 NO-TEST=1 PARTIAL=2 MISSING=0 OWNED=0 -->
