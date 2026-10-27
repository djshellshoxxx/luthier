# Implementation decisions and contradictory numerical requirements

2026-10-24. This supplements the implementation plan; it does not lower its
acceptance limits. A candidate remains unaccepted while any applicable test
fails. Original criteria and failed measurements remain in the evidence.

## Audible behavior and numerical corrections

| Decision | Original requirements in conflict | Proposed correction | Review and acceptance |
|---|---|---|---|
| E37, candidate | Required legacy oversampler wrapper versus exact latency/bypass matching and flat boost response. The legacy allpass path has approximate DC group delay and measured boost response around−5.13dB at20kHz/48kHz/4x. | Reference-only linear-phase polyphase FIR; preserve the legacy engine oversampler unchanged. Current drive candidate uses257 taps in first2x stage and129 in upper stages, integer128/160/176-sample latency. | Timefx independent read-only review found no indexing/history/RT or latency-composition issue. Original alias and CPU gates are still failing; this candidate is **not integrated or accepted**. FIR flatness alone is insufficient. |
| E38, candidate | GENERIC_PEDALS§1 prescribes HP20Hz and flat±0.3dB down to20Hz; that pole alone gives−3.01dB at20Hz. §2 literal drive/compensation gives default output≈+12.12dB versus±3dB unity acceptance, and exceeds its6dB full-drive loudness claim. | HP2Hz preserves DC removal and original flatness criterion; it extends subbass and makes DC settling10times slower. Prepare-time reference loudness normalization preserves the waveshaper's spectral ratios and original level limits. | Material audible tradeoffs explicitly recorded. Independent reviewer requested both peak and RMS conventions be tested: amplitude0.1258925 means−18dBFS **peak**, not RMS. Calibration and quality evidence remain candidates, not hardware measurements. |
| E40, adopted helper | GENERIC_PEDALS§5 mandates four-point Lagrange interpolation and wet interpolation flat±0.5dB through14kHz/48kHz. At half-sample phase the prescribed cubic loses≈1.9dB at14kHz. | Add16-tap Blackman-windowed sinc read with257 prepared fractional phases and coefficient interpolation. Preserve existing cubic read unchanged. Coefficients initialize off the audio thread, no callback allocation. The chorus may select the bandlimited read; no numeric threshold is weakened. | Original helper regressions pass. Independent DSP dense DFT sweep:4rates×257phases×281frequencies, worst0.003349171dB through14kHz. Relevant tests: RefPedalBandlimitedDelay. Actual chain, CPU and sanitizer gates remain required. |
| WP14 tap range | DD-3T§6/algorithm and implementation-plan gate specify overall12.5–800ms plus halving. §9 instead clamps a500ms tap with QT to the S knob's50ms maximum. | Apply the plan's stated priority: QT yields333.3ms in the overall range. Keep the conflicting original§9 criterion visible. | Dedicated333.3ms regression in worker tests; no narrowed range or timing slack. |
| WP14 headroom, unresolved | Literal CE-2/CE-2W clipping constants conflict with their harmonic-distortion targets in coherent, depth0/noise0 fixtures. | No correction adopted. Verify reference units, signal level convention and real transfer path before proposing a change. | Worker observed CE-2 total THD2.26% at−12dBFS peak versus<0.3%, and CE-2W CE1 wet THD0.806% at0dBFS peak versus1–3%. Named models stay unregistered while gates fail. |

The separate short31-tap wet-path oversampler candidate is required for BBD
paths whose physical minimum delay is shorter than the drive filter latency.
Its2x roundtrip delay is15samples (4x extension23samples). Internal wet delay
must absorb that cost while physical timing, dry behavior and all applicable
alias limits remain unchanged. It must be reviewed and tested in the actual
BBD chain, not accepted from an impulse peak alone.

The antialiasing investigation is informed by the primary
[Parker, Zavalishin and Le Bivic DAFx2016 paper](https://dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf).
Its continuous-time integration approach motivates independent first/second
order experiments; those experiments do not certify a production algorithm.
The coordinator's ideal static-tanh8x oracle finds stronger rejection from
second order than from first-order delay compensation. Production composed
fuzz, parameter modulation, latency, aliasing and CPU still require validation.

## Additive contracts

### E51: CE-2 estimated headroom onset (peer reviewed; numerical gates unchanged)

The original `tanh(x)` headroom expression at a 1-unit ceiling distorts below
the stated onset above -6 dBFS. In the actual wet chain its THD at -12 dBFS
peak is 1.915–2.345%, conflicting with the original <0.3% requirement; at
-3 dBFS peak it is 11.24–12.94%, conflicting with the original 2–6% range.
The reviewed candidate is linear for |x|<=0.5 and otherwise
`sign(x)*(0.5+0.5*tanh((|x|-0.5)/0.5))`. It preserves the 1-unit ceiling,
unity slope and C2 continuity at onset. This is an estimated headroom model,
not a hardware measurement. DSP independently reviewed the proposal before
timefx adopted it in its isolated worktree.

At 44.1/48/96/192 kHz, coherent 1 kHz peak-amplitude probes measured effectively
zero low-level THD and 2.683/2.583/2.250/2.179% high-level THD. Original limits
remain unchanged. Actual-chain alias checks reported worst -114.903 dBc at
48 kHz (9th folded harmonic), versus -146 dBc for the original expression;
small-signal gain changed by <0.000103 dB and frequency response by <0.000003 dB.
Persistent all-rate regressions and raw original/revised results must accompany
the accepted implementation checkpoint. CE-2W CE-1 headroom remains a separate,
unresolved measurement; this correction does not authorize changing it.

All additions preserve existing state/parameter identities. Per-lane coverage
contains actual commit and test evidence; an approved interface alone does not
mean its full behavior is implemented or accepted.

| ID | Contract and ownership |
|---|---|
| E32 | IPedal borrowed double sidechain samples for the immediately following process call; null means unavailable, no internal-detector substitution. |
| E33 | MidiRouter message-thread map reclamation after audio readers release snapshots. |
| E34 | BoardModel cloned UI metadata setter, no undo or audio recompile. |
| E35 | BoardWindow non-owning shared BoardRoot reparenting; detach before owner destruction. |
| E36 | ParamWrite slot0xff denotes named board mix/tempo/enabled target. |
| E39 | Board OFF preserves legacy audio and suppresses pedal/raw/output MIDI; clock tracking and enabled binding remain live solely for reenable/sync. |
| E41 | Pedal footswitch engagement policy plus consumed engagement-toggle requests; loopers own record/play gestures, board owns bypass truth. |
| E43 | Validated selected-instance MIDI focus, persisted without undo or audio topology recompilation. |
| E44 | Fixed323-value/serial host snapshot, audio-only facade input, compiled descriptor mapping and persistent audio-owned overrides. Position remains structural. |
| E45 | Borrowed float host sidechain, bounded conversion to prepared double scratch, explicit clearing/availability; real PRE reamp path. |
| E46 | Audio-only host-enabled override for MIDI gating before message-thread mirror catches up. |
| E47 | Audio-only actual per-pedal UI meter POD; engine atomic publication, bounded UI read, instance identity, unavailable flags rather than invented progress. |
| E48 | Model-enabled intent revision enters MIDI before fresh host serials; same-value explicit writes remain observable. |
| E49 | Green Russian width69.9/depth114.3mm resolves inconsistent orientation using control coordinates; RC5 keeps physical73×129mm and external CTL controls live in inspector. Original geometry retained in UI ledger. |
| E50 | Host-origin model persistence has no undo/audio-intent revision; explicit UI writes keep intent revisions. Prevent stale timer echoes from defeating newer audio host writes. |
| E52 | Audio-only `Board::drainMidiOut(MidiEvent*, int capacity)` drains bounded short events without growing a caller's JUCE buffer. Deterministic overflow accounting and host output capacity remain explicit implementation gates. |
| E53 | `BoardModel::cancelTransaction()` restores the outer transaction baseline and republishes it, clears nesting and preserves the undo/redo history preceding the transaction. No active transaction is a rejected no-op. |
| E54 | Optional borrowed stereo Pedal B/Wet only/Dry only buffers; append buses after old indices, default disabled. Placement sends accumulate after host clearing; explicit availability and bounded writes. |
| E55 | Off-audio runtime-extra capture for persistence, retained UUID lifetime and race-safe published extra snapshots. Compiler/model publication snapshots remain separate so ordinary edits do not reset recorded loops. |
| E56 | Momentary feedback kill retains configured gain, ramps down in5ms, is serialized released; watchdog rearm requires explicit OFF then ON. |
| E57 | Append50 routing/board host controls after the original323 lanes, separate serial snapshot, exact table in ExtendedHostAutomation.h. -120dB host endpoint represents silence; lower preexisting finite state values remain preserved until a host write. Structural topology requests remain message-thread validated. |
| E59 | Stopped/off-audio shared asset-store injection and pinned runtime lifetime; persistent appdata default with explicit errors, tests use temporary injected stores. |

IDs E42 and other gaps are unassigned. A final spec reconciliation must link
the independent reviews, original/revised conditions, exact accepted source
commit and passing regressions in the release evidence.

E58's two-anchor Phase90 implementation is present in the integrated preview
source. Independent DSP review and retained original/revised oracle tests cover
its anchor equations, Schur stability, phase and explicit zero added latency.
This is not full named-model acceptance: registration remains gated, and the
literal raw24-bit bypass-step interpretation remains unresolved in the evidence.
The alternative2x FIR adds15samples and fails0.4%CPU; it was not adopted.


## Continuation contracts E60–E68

These additive contracts were agreed between the coordinator and disjoint
owners before their dependent implementations. Existing parameter identities
and all original numeric thresholds remain unchanged. Coverage files record
which parts are implemented and verified; a contract is not a release pass.

| ID | Original issue and agreed contract |
|---|---|
| E60 | Buffered bypass needs the actual model buffer, and time-based bypass must retain tails. `processBypass(L,R,n)` receives latency-matched dry, uses independent prepared state and adds no constant algorithmic latency. Buffered+hasTails follows the residual-tail contract. The original analytic dry subtraction is superseded by E68 where clipping makes it incorrect. |
| E61 | Host restore can publish maps off the message thread. Serialize only non-audio publishers/reclaimers with a mutex; the audio reader uses a bounded two-attempt seq_cst hazard protocol, with no locks, allocation or reclamation in process. Idle publication retention must remain bounded. |
| E62 | Append optional integer fsMode=-1 to SetSlotChannel, preserving existing three-field calls. Stored0/1/2 means latch/momentary/auto; -1 preserves. Clone existing MIDI metadata, reject malformed values atomically, and let an actual model fs_mode control take precedence. |
| E63 | Toggle/relative/pickup bindings share canonical audio-owned target values. Resolved host/model intent enters through a bounded setter with revision/generation; stale mirrors cannot defeat newer MIDI or a replacement slot UUID. |
| E64 | Looper undo-depth changes allocate off audio as structural Config+Ext changes. Capture/restore must preserve audible layers, available redo, transport and phase within the selected capacity. Failed preparation/asset restoration rejects the unpublished candidate and preserves the working model, audio and history. |
| E65 | Admission uses uint64 summed descriptor estimates and explicitly labels them estimated until calibrated. Runtime relief uses measured whole-Luthier callback load from CpuLoadMonitor, not host-wide or inferred board-only load: above85% continuously for2seconds latches an off-audio Eco transaction and notice, with no automatic reversion. Original measured performance budgets still apply. |
| E66 | Hall's original geometry failed the raw200ms NED criterion. The estimated replacement keeps8 Householder lines and4 input diffusers, uses shorter prime lengths inside30–110ms, adds lossless wet-only output allpasses and seeded random-walk modulation, and uses fixed signed calibration. Original geometry, all four sign candidates and raw failures remain executable evidence. Original canonical5-rate default-white±3dB and mixed-sine±1.5dB, NED, RT60 and freeze criteria pass unchanged in focused runs. Expanded-seed mixed gain reaches+1.643dB, an explicit open limitation, not a full-feature acceptance pass. |
| E67 | Discrete natural values were incorrectly treated as zero-based Enum indices. Discrete maps min..max snapped to its declared steps; Enum remains zero-based. No integrated descriptor used Discrete before the correction, so no shipped ordinal migration is implied. Upcoming undo-depth1..8 and memory1..99 defaults must map to natural1. |
| E68 | E60's analytic dry subtraction can reverse hot, trimmed dry when a model clamps its input/output. One-pass `processWithWetResidual(L,R,wetL,wetR,n)` exposes the actual post-mix/trails wet contribution before final dry sum/guard. Default calls process exactly once and returns false; faithful tail acceptance requires a real override. The board blends full output toward buffered dry plus this wet residual, never reconstructs dry by assuming linearity, and reports unavailable residual support. |
| Phase90 single-envelope correction (2026-10-27) | Phase90's original 5 ms equal-power audio law composes with Board Core's linear-in-time `mix_enable` control; applying both model and node envelopes squares the fade. Append `PedalDesc::bypassFade`, default Linear. Only a non-tail EqualPower descriptor opts into `IPedal::processForBoard`, which renders the active signal once without an internal engagement blend; the node applies the only audio envelope. Its default forwards to `process`. Phase90's direct `process` retains its own single equal-power envelope; enabled/LFO state is never forced. Existing linear arithmetic and E68 tail dispatch remain unchanged. This internal source-interface addition requires rebuilding consumers, changes no state/parameter identities or registration, and does not resolve or approve any reinterpretation of the separate 24-bit click criterion. |

Host state retains all373 APVTS intents, including vacant/unused slot lanes and
rejected structural combinations, while the validated model remains unchanged
on rejection and reports an error. Stored momentary intent is distinct from a
replayable action: feedback kill restores released in DSP even if host metadata
retains1, and only a fresh actual host action can fire it. A thread-local restore
notification guard must not suppress another thread's actual host writes.

## RC-5 history eligibility prerequisite (2026-10-27)

RAM `LooperCore` and `PagedLooperCore` add audio-owner-only `canUndo()` and
`canRedo()`, mirroring their existing apply guards: Playing and a nonzero accepted
undo/redo count. Unprepared returns false. These are eligibility now, not history
availability in other transport states. Cancel a provisional overdub first, then
query the restored Playing history. No processing/layout/state format or action
semantics change; no GUI-thread guarantee or named registration follows.

## RC-5 immediate press-cycle prerequisite (2026-10-27)

The existing boss-rc5 §2.1/2.2 cycle applies on a main pedal down edge:
Empty→Record, Recording→Play, Playing→Overdub, Overdubbing→Play,
Stopped→Play from loop origin. A small audio-owner Rc5PressCycle primitive
returns an optional LooperAction from the current real-core state; the caller
applies it immediately. Repeated down
notifications while held are ignored; up releases the guard without transport
change. reset clears only this physical-edge bookkeeping and never resets audio,
transport, history or the sample clock. The caller splits processing at the
sample boundary before calling down(state). Returning the action keeps it
interceptable by future double/hold handling. No queue, clock, allocation or I/O is added.

This is not a complete gesture decoder and is not connected to a registered
pedal. A future controller must intercept double/hold gestures, remember press
origin and cancel provisional overdub before history queries; it must not use
this primitive alone as the final RC-5 footswitch. The separate held second
press after double-Stop remains unresolved; no destructive Clear policy is
chosen here. This adds no parameter/state schema or registration.

## E64 internal available-redo graph preservation (2026-10-27)

PagedLooperCore::Graph carries available redo layers in the core's stack order,
separately from currently audible active/retired layers. activeCount+redoCount
must fit configured undoDepth (1..8); retired capacity stays2. Ready requires
complete hashes for every retained redo asset as well as audible assets. Pending,
recording, failed and empty snapshots cannot claim complete redo hashes.
Capture remains bounded atomic publication with no live audio-buffer reads;
restore remains stopped/off-audio on an unpublished processor. Invalid counts or
null hashes reject before clearing that candidate. Existing stopped/head0 restore
semantics do not change in this increment. sizeof(Impl) remains included in RAM
reservation. This closes only the internal graph prerequisite: JSON schema
migration, full transport/phase preservation and final wrapper integration remain
separate unfinished E64 gates; no JSON/preset compatibility claim is made here.

## Looper JSON schema2 redo migration (2026-10-27)

The codec writes schema2, adding mandatory ordered redo asset hashes; known
schema2 validates active+redo<=undoDepth and complete/non-null hashes only for
Ready. Other statuses carry an empty redo array. Legacy schema1 decodes with no
redo (the old format never represented it) and migrates on the next encode.
Schemas>2 remain opaque, preserved without DSP loading. Raising the schema is
intentional: old schema1-only readers already preserve schema2 as forward state
instead of loading it and losing history. Unknown unowned properties continue
being cloned; redo becomes a codec-owned key in schema2. Failure leaves Stored
or output untouched. This migration does not change transport/head semantics,
parameter identities, registration or final model acceptance.

## E64 runtime-only resume helper (2026-10-27)

Architecture separates persisted memory from per-session transport. Graph adds
an available runtime-only snapshot of Empty/Playing/Stopped state, head, current
5ms playback-ramp gain and sample clock, coherently published with assets.
Recording/Overdubbing snapshots are unavailable for resumption. The existing
restore defaults to Stopped, with an explicit ResumeCaptured policy for a stopped,
unpublished candidate; malformed/unavailable runtime metadata rejects before
clearing it. Resume primes read-ahead at the captured head; owner must confirm
playbackReady before activation. Schema2 intentionally does not store this
session metadata, so decoded memory cannot silently acquire resume intent.

This helper resumes the captured boundary, not a later live publication time.
Actual structural replacement still needs final-boundary handoff/revalidation
because the old core continues during off-audio preparation. No preset auto-play,
mid-record snapshot, registration or complete E64 acceptance is introduced.
The signed sample clock saturates at INT64_MAX rather than overflowing when
restored at the representable boundary; normal clock arithmetic is unchanged.

## RC-5 timed double-press prerequisite (2026-10-27)

Extend Rc5PressCycle with an optional timed down-edge overload returning a fixed
0..2 action batch. State mapping/held guard remain shared with the untimed API.
The caller supplies the actual boundary sample, prepared rate(1..192000) and
specified double-tap milliseconds(150..600); the inclusive down-to-down deadline
is floor(rate*ms/1000). First originPlaying emits provisionalOverdub; a qualifying
second while stillOverdubbing emits CancelOverdub thenStop without intervening
audio. First originOverdubbing emitsPlay, accepting that pass; second emitsStop.
Empty/Recording/Stopped origins perform the cycle and do not arm that pair.
An eligible pair with a failed/automatically closed first action mayStop, but
cannot cancel unless the provisionalOverdubbing state is actually still present.

Release clears only held; successful pair consumes its candidate. Expiration
itself emits nothing. Invalid/backward clocks/configuration disarm and reject
the timed edge; reset accompanies a deliberate clock reset and touches only
bookkeeping. Threshold/rate changes invalidate the pair. The caller MUST call
invalidateCandidate before external transport/history/media actions, even if
state later looks identical; matching state alone cannot prove ownership.
Untimed down invalidates any timed candidate. No hold/Clear behavior, including
the unresolved held second double-Stop policy, is silently introduced. This
prerequisite stays unregistered and is not a complete RC-5 gesture controller.

## RC-5 internal tap-tempo prerequisite (2026-10-27)

Per boss-rc5§4.4, use the arithmetic mean of the last four accepted adjacent-tap
intervals, not ClockSync's separate weighted-three board tap policy. A small
Rc5TapTempo prepares a finite rate1..192000 and returns an optional new full-
precision BPM for valid intervals200..1500ms inclusive, clamped40..300. First
tap anchors only. Every nonnegative monotonic tap updates the adjacent-tap anchor;
out-of-range intervals are excluded from the mean. A gap>=2seconds clears the
mean and makes the current tap the new anchor. Negative/backward events reset
history and emit no tempo; deliberate timeline reset calls reset. Rate changes
reset history; invalid prepare leaves prior valid state untouched. Integer frame
bounds use ceil(rate/5),floor(3*rate/2),ceil(2*rate), avoiding boundary rounding
ambiguity and signed arithmetic overflow. All storage is fixed-size/noalloc.

The result does not select host/MIDI/internal authority or move rhythm/loop phase.
The future wrapper applies it to internal tempo through its existing authority
rules, with0.1BPM only for UI; actual Rc5Rhythm keeps full precision and its own
phase-continuous setTempo. This prerequisite does not implement full RC5 controls,
clock master/slave, wrapper/registration or advanced audio playback.

## Forward page-window geometric coverage (2026-10-27)

A nominal pageFrames*readAhead/rate omits the partial last page and cursor offset.
Add a constructor-free minimumForwardWindowFrames(loopFrames,pageFrames,readAhead)
query: minimum consecutive frame count from any cursor, including the current
frame, covered by the configured circular forward page window. Invalid nonpositive
geometry returns0. If all loop pages fit in the window, return UINT64_MAX for
repeatable whole-loop coverage. Otherwise A=readAhead,P=pageFrames,R=last-page
length(in1..P): A0 returns1; A>=1 returns1+(A−1)*P+R. This is geometry, not proof
that assets are resident or I/O meets deadlines; readiness/performance stay separate.

The RC5 maximum-memory policy fixture must satisfy original>=2seconds using this
stronger minimum, including every possible partial-page length. A13 with the
existing14read slots and P8192/16384/32768 at<=48/96/192k preserves RAM while
covering the worst partial-page case. It does not silently reconfigure generic
loopers or implement the absent RC5 wrapper. Keep the old nominal assertion too;
record the stronger originalA12failure before changing that fixture policy.

## RC5 prerequisite: stopped/empty reverse selection (2026-10-27)

The storage direction API does not itself implement playback. Add a typed
audio-owner PagedLooperCore playback-direction getter/setter, defaultForward.
Accept a valid direction only while Empty or fully Stopped with gain exactly0;
reject unprepared, invalid values, Recording, Playing, Overdubbing and an audible
Stop fade without mutation. Success means selection accepted, not I/O ready.
For an existing loop anchor Reverse at N−1 or Forward at0, prime that window,
publish the coherent runtime tuple, and require playbackReady before rendering.
No direction event/command or active-play reversal is introduced in this slice.

Selection is a playback setting, like the existing seam setting: Record/Clear/
reset clear audio/transport but retain it. Recording always appends forward;
closing a reverse-selected recording plays from N−1. Reverse rejects Overdub
before any closeRecord/startOverdub side effect, with the private guard retained.
This follows the official manual's reverse/overdub restriction; it does not
choose whether to commit/cancel an existing overdub or choose a midplay anchor.

Every base/active/retired/scratch read, readiness window, initial/restore prime
and compaction replacement handoff uses the same direction. Reverse advances
head==0?N−1:head−1; stopped fade finishes in its current direction and then
anchors at that direction's origin. Asset PCM/order and forward writing do not
change. Preserve exact layers/Undo/Redo/merge version accounting and dry audio.

Append defaultForward direction to Graph::Runtime and its atomic publication
tuple; ResumeCaptured validates/restores it before priming, including arbitrary
heads/stop fades/empty selection. Invalid values reject before mutation. Existing
aggregate initializers retain Forward. RestorePolicy::Stopped explicitly starts
fresh Forward/head0 transport. JSON still deliberately omits the entire runtime;
this prerequisite does not persist a memory's reverse parameter or implement
the RC5 wrapper. A future wrapper must preserve its separate persistent setting.

The geometric minimum applies in either direction. Add a direction-neutral
query, retaining minimumForwardWindowFrames as a compatibility wrapper. Prove
reverse coverage by independent every-cursor enumeration; retain all original
forward bounds, >=2seconds and64,000,000bytes. New inline/publication state must
be charged by actual sizeof. Stopped reverse tests must cover partial tails,
fresh restore/Ready, recording, OD refusal, layers/Redo/compaction, coherent
capture/resume/JSON omission, faults and no callback allocations. Audible active
reversal, half-speed, preset selection, UI/MIDI and registration remain separate.

## RC5 prerequisite: 99-memory metadata index (2026-10-27)

Fill Models/rc5_storage's retained shell with a message-thread-only Rc5MemoryBank,
not an audio selector/file worker. Public numbers1..99 map to99fixed positional
entries; no second selected-memory authority or MIDI numbering policy. Each entry
holds LooperStateCodec::Stored plus its own cloned source/metadata. Return clones,
never mutable internal var aliases. No asset-store calls, audio copies or GC claim.

Typed per-memory settings follow RC5§2.4/§8: nameUnicode/defaultempty; loopLevel
finite0..200/default100; rhythmLevel0..100/default80; tempo40..300/default120,
fullprecision; patterninteger1..57/default1, variationinteger0..1/default0,
kitinteger1..7/default1; reversebool/defaultfalse. RhythmOn remains the separate
runtime/host intent: it is not invented as a saved memory-start policy. Preserve
unknown rhythmOn/playmode/countin/precount fields without adding typed behavior.

Index schema1 has exactly99positional entries. Empty/default entries may encode
compactly as null, avoiding a large default host-state blob; objects hold settings
and looper subtrees. Preserve unknown root/entry/settings/looper properties.
Future index schemas remain completely opaque/read-only; future nested looper
schemas remain opaque/unplayable. Never report successful replacement when the
underlying forward-schema codec would merely preserve its old tree unchanged.
All invalid decode/mutations/encode leave prior index/output unchanged. Metadata
edits do not modify audio. Explicit clear removes a known slot's audio/history
while preserving its settings/unknown properties; it cannot erase future-opaque
audio through an ordinary mutation. Known persisted loops must be Ready or Empty.

Saving audio requires an actual Ready capture and validates through existing
LooperStateCodec; Recording/Pending/Failed cannot replace a saved loop. Empty
requires the explicit clear operation. Preserve Ready fault bits, complete base/
active/retired/redo references and source-frame/rate identities; runtime transport
is deliberately omitted just as in the underlying codec. No preset-ready claim.

Enforce each frames<=5400*sourceRate and exact sum(frames/sourceRate)<=46800seconds.
Retain the existing codec's integral1..192000Hz acceptance. Use existing JUCE
BigInteger for bounded99-term exact rational comparisons, not rounded durations
or a floatingepsilon. Duplicate memories count separately; layers/history count
one logical timeline. Replacement excludes its old duration before checking.
Unknown future duration makes total capacity indeterminate; preserve such input
but reject new audio saves needing fullcap proof. Known-part overcapacity rejects
decode. Clearing known audio/metadata-only edits may reduce/preserve known usage.

Export known manifest roots deterministically in memory/base/active/retired/redo
order, optionally first-seen deduplicated, with explicit completeness status.
Opaque entries/index make it incomplete; IAssetStore supplies no pin/GC/recursive
page-closure facility. Do not claim physical retention, asset verification, disk
bytes, resampling, crash-safe file commit or complete restoration from this codec.
Future wrapper must enforce the whole board256KiB state limit, verify/prepare/
prime assets off audio, preserve persistent reverse/settings and publish safely.

Tests cover all99positions/invalidnumbers, exactmetadata types/boundaries/nonfinite,
Unicode/deep-clone isolation, nested schema1 migration/schema2 redo/future schemas,
transactional failure, exact90min/13h and one-frame excess, mixed/coprime rates,
replacement/clear accounting, runtime omission and complete ordered root reporting.
