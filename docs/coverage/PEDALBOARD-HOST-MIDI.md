# Reference pedalboard host / MIDI coverage ledger

2026-10-24. WP5/WP6/WP23. This ledger records implemented behavior, actual evidence and finite missing gates. It does not claim full MIDI_EXPRESSION acceptance.

## Verification

- Focused native JUCE8.0.10 harness: **32 cases /337 checks /0 failures**, AddressSanitizer+UndefinedBehaviorSanitizer. Own board/router/model/test sources are instrumented; reused JUCE module objects are uninstrumented. Tests cover routing offsets/reserved CCs, bounded flood/clock priority, learn acceptance/suppression, no callback allocation,200 concurrent map publications/reclamations, board targets/OFF gating, pickup/precedence, held-key reorder release, host STOP,4 sample rates x5 partitions, callback capture/echo isolation, model/host intent including wrap and stale echoes, CC14 history, UUID/focus/local maps, analytic/custom curves, capacity contracts, malformed data, virtual-expression fanout, canonical tempo glide/phase, AUTO arbitration/lock/loss, UI feedback and slider/velocity routes. Gesture tests cover thresholds/double/hold, momentary/auto/debounce and one-pole step response.
- Earlier **13 cases /64 checks** passed ThreadSanitizer. A separate100,000-write capture/echo/snapshot ThreadSanitizer stress passed. These are earlier snapshots, not a claim that every later source change ran under TSan.
- Coordinator native Linux Pro at source05bbdb8e: **206 cases /5,649,492 checks /0 failures** (40.1s). Seven host cases actually executed, including no-messagepump automation and1000 writes/sec at96kHz/block64. Evidence: `/workspace/luthier-evidence/native-pedalboard-checkpoint2.log`.
- Current Parameters.cpp/PluginProcessor.cpp/LuthierEngine.cpp/HostStateFormat.cpp/Routing sources and RefPedalHostTests.cpp pass native compiler syntax checks. New E50 no-undo, bounded-output, sliced auxiliary-bus and E57 extended host tests await the next integrated native run. Compiler checks are not execution.
- JUCE reservation patch:4 isolated Python tests pass; coordinator CMake guard applies/verifies the exact pinned source before juce_audio_utils compilation. Shared JUCE was changed only by coordinator after source freeze ended.

## Implemented host behavior

Original323 reference params (3 board +16x20 slot fields) retain their indices. E57 appends50 descriptors from ExtendedHostAutomation.h, with a separate integration-test count term. Base discovery uses the stable ref_board_enabled ID. LaneMap maps physical descriptor parameters to host lanes; unused lanes are no-op, named "-" and nonautomatable. Message-thread metadata publication gives populated lanes their model/control names and notifies host metadata changes. Metadata reads never consult BoardModel/registry.

Actual callback payload+serial capture supplies audio-owned snapshots every processor slice. Serial0 is unwritten; wrap skips0. Thread-local echo scopes suppress only synchronous service-thread notifications. Runtime scalar controls do not depend on the30Hz timer; persistence uses applyHostEcho without undo or new audio intent. Explicit model intent, including same-value writes, precedes fresh host serials. Structural mode/endpoint/kind/channel/layout requests remain validated message-thread commands. Endpoint choices0none/1..16storage slot resolve stable UUIDs; empty nonzero selections reject. Exp1/2 feed real virtual sources, and the feedback-kill rising pulse survives an intervening host release before the next audio block.

PRE/LOOP/POST render sites, begin/end/reclaim/prepare/release, board latency, validated borrowed host transport and actual sidechain slices are wired. Pedal B/Wet only/Dry only are appended optional stereo buses after every prior output index, defaultdisabled; each slice clears them, forwards borrowed pointers and preserves them through legacy output routing.

Board state uses the state worker's exact String codec and raw referencePedals extraction, preserving future raw sections and opaque double precision. Missing board state restores empty/OFF. Existing lower finite lane/reamp gain values survive message service/serialization until a fresh host write. The normalized -120dB endpoint is SILENCE (UI -infinity), serialized as-120; reamp maximum remains12dB.

## Bounded MIDI and sources

Immutable maps resolve instance UUID+paramId, including dashed UUID normalization, retain unresolved/unknown state and compile fixed lookup/curve tables off audio. Layer order is explicit > selected focus > local profile; exact channel precedes omni. Reserved CC guards apply. Routing supports CC/CC14, notes/velocity, pitch bend, channel/poly pressure, PC, absolute/toggle/momentary/trigger/two's-complement relative/step, pickup crossing, priority hold and fixed fanout. Adaptive256-node LUTs meet tested1e-4 analytic/PCHIP error sweeps, preserve exact step edges and validate custom5-point ranges/monotonicity.

OFF keeps audio bit-identical and gates pedal events/output while clock and board-enable binding remain live. Held notes retain original storage-slot targets across reorder. Malformed data bytes reject before indexing. Active sensing cannot occupy clock priority. CC14 orphan LSBs are ignored and prepare clears controller history.

VirtualExpression is appended source14 (old ordinals unchanged): JSON src.type="virtualExpression",num1/2,chan0. Host/UI source packets produce real mapped writes; focus Exp1 defaults to first PF_ExprTarget. Unbound CC11/CC4 feed jack1/2 at their offsets; explicit CC mappings win. Latest MIDI source value survives map replacement; OFF defers pending host expression until reenable. UiSlider-origin controller-shaped events route their7-bit source value at next-block0; KeyboardVelocity number0 latches positive note-on velocity. Fixed UI ring reports activity/tempo/lock/loss/recovery/overload.

Clock tracks transport/SPP, sample-rate-independent period estimation and host priority. Canonical host/model BPM+source intent applies without a timer; unrelated maps cannot undo it. Internal tempo changes integrate a phase-continuous50ms linear BPM glide. AUTO MIDI adoption waits max(two beats,1s), playing HOST wins, loss500ms falls back; lock requires48 intervals with<1% interval error. Accepted MIDI tempo updates the internal oscillator at its event offset; an external selected master still wins.

## Bounded host output contract

Owned processor MIDI/SysEx staging and the engine board-output stream reserve65536 before callbacks. E52 drains at most256 sorted pedal events per board beginBlock; engine adds outer offsets. Legacy output sources use emitBounded and all final merges use BoundedMidi release-priority overflow handling. Board output is independent of the legacy MIDI-out toggle.

Final host copy uses a guaranteed lower bound captured before clear: pinned VST3/AU/AUv3/CLAP wrapper reserve2048, patched standalone reserve65536, explicitly registered direct-host storage, or the entry buffer's used bytes. No host-storage swap, allocator/layout introspection or first-callback reserve occurs. Unknown empty unregistered direct hosts have zero guaranteed output capacity and drop outgoing MIDI. Direct hosts/tests call reserveMidiOutputBuffer while stopped, retaining buffer lifetime/storage. The pinned JUCE constructor patch is hash-checked/idempotent and configured via scripts/patch_juce_midi_capacity.py.

## Approved interface additions

- E33 MidiRouter::reclaim(): message-thread destruction after two completed router epochs; prepare/destruction require stopped audio.
- E36 ParamWrite slot0xff addresses BoardTarget Mix0/Tempo1/Enabled2; tempo normalized20..300.
- E39 OFF semantics above.
- E43 focus metadata setter, E44 original HostAutomationSnapshot323, E46 host-enabled serial gate, E48 explicit model-enabled revisions, E50 applyHostEcho, E52 bounded output drain and E54 borrowed auxiliary buses are integrated with the engine worker.
- E57 ExtendedHostAutomationSnapshot50, physical descriptor/enum table, source/capture helpers, Board::setExtendedHostAutomation and real virtual-expression source input. Audio-only router setHost/ModelTempoSource and setHost/ModelTempoBpm use the same serial/revision order. updateReferenceLaneNames is message-thread metadata publication. New helpers add no GUI dependency to the engine.

## Missing acceptance gates

1. Execute new host regressions after coherent integration; rerun full native suite, then ASan host lifecycle/automation stress and actual macOS AU96kHz/block64/subblock32 reproduction. Existing AU Bus error10 has not been diagnosed by these Linux checks. Cross-platform validator/standalone-device output capacity tests remain open.
2. Pedals receive one canonical TimeInfo per board block. During a glide, intra-block PPQ extrapolation error forN<=G is bounded by |deltaBPM|*N*N/(120*sampleRate*G), G=round(.05*sampleRate); forN>G a conservative bound is |deltaBPM|*(N-G/2)/(60*sampleRate). No per-offset clock query exists. MIDI period estimation is not yet the full specified phase PLL; jitter/phase budgets, integer snap, clock output and tap external-master rejection remain open.
3. Learn currently accepts the first qualifying two-value/span candidate, with generation isolation and15s message controller timeout. Most-active500ms competition, first8 candidate message FSM, ambiguous-type preference, duplicate/import conflict workflow and learn-all are not complete.
4. Full expression assignment/calibration, floating-source macros, heel/toe switch, bipolar/latch-PB user configuration and continuous held-key pressure modes are not integrated. Gesture/ExprEngine helpers are tested, but do not imply all consumers use them. Release velocity is routed; model-specific force/pressure/bounce behavior still needs integration.
5. Binding smoothMs is carried; engine per-target one-pole consumption remains a separate engine gate. NRPN, scene/PC snapshot recall+ramping, bank handling, feedback-kill/looper-stop panic semantics and the remaining MIDI_EXPRESSION10 configuration controls are incomplete. Do not show these as working functionality.
6. Map retirement is safe under the tested concurrent publisher/audio usage; idle publication without completed epochs can retain intermediate maps until stopped prepare. Tight idle-memory bounding remains open.
7. Required60s bounded-flood timing, long jitter/loss/source-switch soak,30min malformed MIDI fuzz, complete split-equivalence/S05 phase gates and latest whole-module TSan are not run. UI screenshots/accessibility/automation gesture evidence and every appended control's visible attachment belong to the UI worker's acceptance, not inferred from parameter creation.
