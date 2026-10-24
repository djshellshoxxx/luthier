# Prepared pedal memory admission proposal

Status: design proposal only; no64MiB enforcement is implemented or claimed by this document. Public contract and scope require coordinator approval before model-owner integration.

BOARD_CORE§12 bounds prepared pedal buffers and explicitly treats looper storage separately. A64MiB admission limit must count prepared instances retained for audio transitions as well as the candidate plan; limiting only the final active16 slots can temporarily double usage and is insufficient. This is an accounted allocation-payload budget, not a total process-RSS guarantee.

## Actual fresh model payloads

Timefx owner inventory includes actual vector sizing, interpolation guards and the three simultaneously prepared reverb FDNs. Values below exclude inline object storage and allocator overhead, which a final admission contract must identify explicitly.

| Model |48kHz bytes |192kHz bytes |
| --- | ---: | ---: |
| Generic Delay |1,843,584 |7,373,184 |
| Generic Reverb |3,436,812 |13,701,132 |
| CE-2 |9,600 |37,248 |
| CE-2W |46,656 |184,896 |
| DD-3T |1,859,456 |7,435,136 |

Generic Delay prepares two `(floor(sr*2.4)+8+16)`-double histories. Thus16 delays at192kHz consume117,970,944 bytes (112.505MiB) before their objects and board buffers. Five reverbs exceed64MiB before those additions. DD-3T's short-loop/history buffers are ordinary pedal-owned state, not separately exempted paged-looper media. FractionalDelay's immutable sinc kernel is32,896 bytes shared once. FDN preparation also uses a transient sieve. Vector assign/resize may retain larger capacity after reprepare; actual capacity, not current logical length, is the accounting basis.

## Proposed admission contract

1. Registry registration supplies a pure off-audio preflight callback taking sample rate, block size, oversampling policy and normalized parameter values. It returns supported/unsupported plus retained owned bytes, peak temporary prepare bytes, and separately identified media bytes. It includes sizeof(model), owned dynamic capacities and per-instance supporting allocations. A static callback permits reservation before factory construction; an instance-only estimator would miss constructor/inline allocation peaks.
2. IPedal supplies a post-prepare actual capacity report. The compiler rejects a candidate whose report is unknown or exceeds its reservation. Missing reports are never silently treated as zero; every accepted registered model needs an audited implementation before universal enforcement is claimed.
3. PedalPool reserves peak bytes before constructing/preparing/loading a new runtime, then reduces to actual retained bytes after successful preparation. A reservation follows each shared runtime until its final off-audio destruction, covering RCU-retired plans and persistence pins. Shared runtimes are counted once. Candidate failure releases only its reservation and preserves the old model, history and working audio.
4. A configuration replacement can fail for lack of transition headroom even if its final state alone would fit. Report that explicitly; do not evict or reset an audio-owned old runtime to conceal the peak. Rapid edits cannot bypass the limit by accumulating retired processors. Stopped lifecycle preparation/release must obey the same accounting or report its separate lifecycle guarantee.
5. Charge each runtime's dry compensation buffers. The coordinator must decide whether the fixed board routing/dry scratch is charged as a shared baseline inside64MiB or separately reported; no claim should hide it. Shared immutable tables need one documented charge. Heap metadata/fragmentation is outside a payload claim unless a bounded allocator is introduced.
6. Paged-looper PCM, caches and media workers remain under their explicitly separate aggregate reservation. Report that budget beside the DSP budget. State-owner figures include RC-5 roughly54.4MB of paged aggregate at its192kHz maximum; excluding it from this pool must never be described as a64MiB total-RSS bound.

## Required evidence before acceptance

- Boundary/exact-fit/one-byte-over tests with checked64-bit arithmetic; invalid/unreported requirements reject before factory preparation.
- Actual Delay/Reverb high-rate boards reject above the cap before excessive allocation; approved lower-rate configurations still work.
- Failed add/config/restore preserves audio output, topology version, history and all prior reservations.
- Concurrent audio transitions plus rapid edits retain the old reservation until safe reclamation; shared runtimes are not double-counted, and failed candidates do not leak reservations.
- Peak temporary prepare scratch and explicit media exclusion are exercised; lower-rate reprepare accounts retained capacity honestly.
- No reservation query, allocation, reclamation or mutex enters the audio callback.
