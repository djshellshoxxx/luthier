# RC-5 tap-tempo integration handoff (proposal, 2026-10-28)

Source inspected: `a8f878d509ed52163331d899b7e77b9545ae9cf2` (PR 83).
At inspection the remote PR 83 branch still pointed to that SHA. This is
read-only preparation, not an implementation or acceptance claim. Confirm the
external owner's current branch, unpushed changes and file ownership before
editing `Models/boss_rc5.*` or its tests. UI/native work has a separate owner.

## Existing contract and gap

`spec/reference-pedalboard/specs/pedals/boss-rc5.md` §2 names a RHYTHM TEMPO
tap button; §4.4 requires the mean of the last four accepted intervals,
200–1500 ms inclusive, a two-second timeout and 40–300 BPM; §9.8 requires
eight 120 BPM taps to yield 120.0 ±0.5 BPM. Rhythm settings, including tempo,
are described as per-memory. The source evidence labels the interval algorithm
as a plugin choice, not a hardware capture.

`Common/Rc5TapTempo.h` implements the estimator with audio-owner sample
timestamps, fixed storage and no phase mutation. Its existing tests cover the
rate matrix, interval boundaries, rejected intervals, timeout, allocation and
actual rhythm phase/partition behavior. The `BossRc5` wrapper does not use it:
`updateRhythm()` selects the host clock in AUTO when valid and playing, else
`value(Tempo)`. The three existing pedal momentaries drive loop transport;
`FootEvent` has no control identity and must not be repurposed as TEMPO.

## Smallest proposed event route

The existing specified TEMPO button can be represented by an **appended**
`boss_rc5_tap` `ParamKind::Trigger` descriptor and a rising-edge handler. This
is a proposed mechanical interface, not a new product feature or an approved
patch. `BOARD_CORE.md` §9.1/§11 excludes trigger/momentary parameters from
fixed host lanes, and permits discrete `ParamWrite` events through `setParam`.
`PedalBase::setParam` invokes `onTrigger` only on a rising edge. `PedalView`
already displays trigger descriptors as momentary actions and posts down/up;
`BoardEngine` carries trigger pulses, and MIDI mappings can target descriptors.
Verify those end-to-end paths rather than assuming delivery or exact timing:
UI commands are asynchronous and bounded queues may drop events, while MIDI
event splitting has its documented limits.

The RC-5 reconciled header currently lists exactly three momentaries and its
descriptor test asserts 23 parameters. Append the trigger after existing
parameters, retain their indices and all 13 lane mappings, and document a
scoped model-spec erratum before implementing. Confirm registry descriptor
limits and state migration behavior. Do not overload `pedal1`, CTL1/CTL2,
host tempo automation or raw MIDI with a new meaning. No IPedal interface
change appears necessary for this route, subject to the integration tests.

An audio-owner monotonic processed-frame counter is a candidate timestamp;
use the same clock consistently for successive taps, including block splits,
and reset/re-anchor deliberately on prepare, reset, discontinuity or overflow.
`TimeInfo.sample` can represent host position and seek, and UI pulse timing is
not necessarily the original physical arrival time; do not claim greater
accuracy than the delivery path supports. Test disabled/suspended processing
and transport seeks explicitly before choosing counter semantics.

## Tempo authority and persistence: design work still needed

Tap estimates must not be overwritten on the next `updateRhythm()` call by
the unchanged tempo parameter. Conversely, changing a private runtime value
alone would leave the board/model and host-visible parameter stale and could
lose the tempo on state save or memory changes. Define and test the existing
board's audio-to-model/host synchronization route before coding; do not write
message-thread state or notify a host from the callback. Preserve an explicit
manual/host-automation write as a meaningful authority change. Preserve AUTO's
current host-clock precedence and the internal value for fallback; specify
whether taps during external lock update that retained internal value and make
this visible in tests. Do not silently claim memory persistence while the
99-memory audio/metadata handoff is still unfinished.

These are engineering integration choices grounded in the existing button and
clock contract, not a request for a new user-facing control. Any genuinely
ambiguous user-facing clock/automation precedence that cannot be derived from
the specs should be recorded for owner review; no threshold waiver is implied.

## First objective fixtures for the owner

1. RED wrapper test via the proposed event: at 44.1, 48, 88.2, 96 and 192 kHz,
   eight presses at 0.5-second intervals produce 120 BPM ±0.5; compare blocks
   of 1, 16, 64, 512 and 2048 frames, including event boundaries. First tap
   alone must not replace tempo. Hold and repeated high values fire once;
   release then press fires again. Confirm original estimator tests remain green.
2. Exercise 200/1500 ms inclusive and adjacent rejected intervals, two-second
   timeout, reset/reprepare and the chosen seek/suspend policy through the
   wrapper. Cover actual rhythm output/phase and no allocation in the callback.
3. Assert AUTO with valid running host clock, INTERNAL, host stop/fallback,
   tap under lock, and a later explicit tempo automation write according to the
   documented authority decision. Test save/reload and memory selection only
   when their real persistence paths are wired; keep those gates open otherwise.
4. Exercise actual board/UI and MIDI trigger delivery, ordered press/release,
   queue drop reporting and slot identity. Verify unchanged host lane IDs/order,
   old state migration, and no replay of a held trigger after preset load.
   Distinguish sample-offset MIDI from asynchronous UI arrival.

Run focused Pro/Free and applicable sanitizer tests on the exact resulting
source, preserving any RED. Register no RC-5 model until its remaining safety,
audio, functionality and native/host gates pass. No build or test was run for
this proposal; CE-2, Phase90, DD-3T and Dyna open gates remain unchanged.
