# Independent release-cap review

Reviewed the frozen 280e182a baseline and DSP candidate in `/workspace/luthier-evidence/release-cap-025-280`. This is Linux native evidence from a one-production-object overlay, not full rebuilt release qualification or macOS qualification. No source changes were made during review.

## Source and math

The immutable StringEngine.cpp diff changes only the explanatory comment and the Released endpoint from 0.3 to 0.25 seconds. No assertion, measurement window, instrument-specific branch, source gain, tuning, note dispatch, or sustain coefficient changed. Existing test code is unchanged. The new endpoint is an authorized engineering choice within the stated 0.2–0.4 second estimate, not a measured hardware constant.

The loop multiplier is exp(-ln(1000) * loopSamples / (T60 * sampleRate)). With loopSamples = sampleRate / frequency, T60 seconds of round trips produces amplitude 0.001 (-60 dB) before other losses. The existing maximum loop-gain clamp can add loss; it cannot weaken the decay target. Full release chooses min(existing T60, 0.25); shorter decays are not lengthened. Intermediate release uses the existing geometric interpolation, so the ramp architecture is preserved although released waveforms intentionally change. The final T60 clamp remains 0.01–60 seconds.

StringEngine itself assumes its prepared sample rate is valid. The existing processor boundary calls HostClockGuard::validSampleRate before preparing the engine: finite positive input is accepted, otherwise the previous valid rate or 44100 is substituted. This was not introduced by the candidate. The candidate native binary passed the existing guard unit and invalid-host-prepare tests (2 tests, 12 checks). The first attempted Suite.name selector matched zero tests; that log is preserved separately and is not counted.

## Acceptance evidence independently checked

The additional probe keeps the original 30 ms before/after windows and the original 35 dB whole-output predicate (including existing floor/idle guards); it adds a strict >40 dB raw check. It requires the actual dispatched note-off to match the scheduled release. It does not apply the existing slow-CI 31 dB alternative. All 25 instrument choices at 44100, 48000, 88200, 96000 and 192000 Hz are present: 125 cases, 1000 checks. Candidate minimum measured raw drop is 59.924739 dB and minimum output drop is 42.298533 dB. Baseline output is below 35 dB for P-Style Bass at 44100 (34.452378 dB) and 48000 (34.738995 dB).

The unchanged original release/legato/pedal/ramp/stagger selection passed 9 tests/74 checks. The separate SustainDecay plus factory-normalization selection passed 16 tests/71 checks; these selections overlap at releaseRamp, releaseSag, and releaseRing and must not be summed as unique tests. Their exact logs remain in the DSP evidence directory. The test named legacyIsBitIdentical compares two neutral-shape configurations within the candidate; it does not assert that the intentionally changed released waveform equals the old baseline.

Direct StringEngine comparison: all 120 pre-release SHA256 hashes match; the full no-release/letRing traces match for all 60 cases (5 rates, 2 strings, 3 frets, 2 scenarios). This proves those direct scenarios, not actual MIDI pedal dispatch.

Across full-host initial sweeps, 124 of 125 pre-release traces match. Resonator at 88200 differs from frame 180, well before note-off; all four captured streams differ. A narrow repeat independently confirms baselineA = baselineB = candidate before release for all 25 instrument choices at 88200, while the original baseline alone differs at Resonator. All three repeat Resonator XML hashes match. The unexplained intermittent original baseline difference is preserved; no unconditional all-sweep bit-identity claim is supported.

Resource provenance is qualified: the private snapshot contains the same 1327 qualified resource files plus 66 ignored Factory Backup entries, with no changed or missing qualified file. All 70 active factory preset hashes match, and every actual private snapshot file matches its recorded hash.

## Fresh pinned generation3 follow-up

Generation3 pre-run hashes independently match all frozen source, command, object, and binary files. Fresh release rows retain the same raw/output minima and all 125 four-stream pre-release traces match exactly. Actual-host CC64/CC66 prefix hashes match for all 30 cases (5 rates × instrument choices 0/18/19 × 2 pedals), across raw, pre-cab, mic, and whole-output streams.

Generation3 overall is RED: its newly added pedal assertion incorrectly expects first StringActivity note-off at pedal-up. The real engine publishes note-off activity before honoring letRing and clears soundingNote, so that activity is key-up at 0.5 seconds. All 30 added assertions fail in both variants. The failed added probe is preserved. DSP will correct only this new probe to validate key-up activity and actual Open/Released damping state; original acceptance predicates remain untouched.

## Final generation4 pedal closure

Reviewed the corrected additional probe source. It now checks the actual key-up activity at 0.5 seconds, inspects the played-frequency string as Open while the pedal remains down at approximately 0.7 seconds, and observes it as Released after pedal-up. It retains nonzero held raw RMS and finite samples. No original acceptance test or predicate was modified.

Generation4 ran the corrected added pedal probe only. Baseline and candidate each pass 1 test/150 checks. Independent CSV comparison finds zero four-stream prefix-hash mismatches across all 30 actual CC64/CC66 cases and zero damping/key-up/held-energy state failures. All frozen pre-run source/object/binary hashes still match. Baseline probe SHA256 is ffe61e91b9129a6775b2dd64ee1c0575bc989cfa201b462ce0aa8240cc0f9b3d; candidate probe is 2b5ead074617c2909ca7aadc241091bc8f52c8f7c30a1feef960dd0aae5f78a1. Failed generation3 assertions are retained and not counted as a passing run.

The additional unchanged native StringEngine/Sustain/E-Bow selection passed 12 tests/39 checks; it overlaps the earlier legato selection, so counts are reported individually.

## Disposition and limits

No blocking production-code defect identified in the bounded 0.3-to-0.25 release-cap change. The authorized literal change is supported by unchanged release gates, corrected physical release behavior, direct and host waveform comparisons, valid-rate guarding, and the existing regressions. It is suitable for coordinator integration and fresh combined-source/platform qualification.

The original intermittent baseline Resonator waveform discrepancy is preserved as unexplained; later unchanged baselines and the fully pinned full sweep do not reproduce it. This is not a claim that every rendering context is deterministic. Initial generation1 output had no pre-run probe binary hash; it must not be retroactively attributed to later hashes. The fresh pinned generation3 and generation4 runs provide the current qualification evidence.

No full rebuilt release or macOS pass is claimed. The original Mac post-prepare exact-state failure remains unresolved pending actual Mac diagnostic evidence. Named looper and runtime-extra features absent from frozen source 280e remain outside this review. Root owns integration and final CI qualification.
