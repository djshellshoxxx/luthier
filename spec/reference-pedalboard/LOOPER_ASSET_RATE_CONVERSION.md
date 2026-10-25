# Off-audio looper asset rate conversion

This bounded increment closes the native Ditto factory's rejection of completed
saved loops whose sample rate differs from the current host rate. It does not
register the model or change the unfinished-take, return-tap or seam contracts.

- Conversion runs synchronously only during stopped, unpublished restore. It
  reads/writes content-addressed assets through the supplied IAssetStore; the
  callback never touches files, resamples a saved loop, or allocates.
- Convert each referenced base, active, redo and retired stream independently.
  Sparse missing pages mean zero. Verify every manifest/page hash, format,
  finite PCM sample and exact length before publishing the replacement graph.
- New length is round(oldFrames * newRate / oldRate), minimum one frame; new
  playhead uses the same time scaling, modulo the new length. Keep transport,
  chronological history and unknown state fields. The converter copies graph
  generation; newly restored runtimes establish their own publication epochs.
  No stale source
  hash may be labeled as converted audio. Same-rate compatible assets stay exact.
- Preserve the original Ditto T8 limits: loop response within ±0.01 dB from
  20 Hz through 20 kHz and THD+N below −140 dBFS. The original 64-tap Blackman/.45
  prototype failed actual asset migration at 18–20 kHz; retain that RED evidence.
  It is superseded by a periodic Kaiser-windowed sinc with beta 19. For supported
  audio host rates, pass edge=20 kHz, stop edge=min(source,target)/2, cutoff is
  centered between them. Below 44.1 kHz fixture rates use pass edge=.45*minimum.
  Use odd centered support with half-length
  ceil(0.5*1.25*(180−8)/(2.285*2*pi*(stop−pass)/sourceRate)); 180 dB is an empirical
  design input with 25% length margin, not a measured universal rejection claim.
  Samples outside the Kaiser radius have zero weight; normalize each phase.
- Reduce integral rates to P/Q by GCD and precompute Q coefficient rows. Sample
  n uses integer quotient/remainder of n*P divided by Q, avoiding accumulated
  phase error on long recordings. Unsupported ratios outside 1/8..8 and tables
  exceeding the remaining budget reject before allocation. All 25 supported
  44.1/48/88.2/96/192k pairs fit at default depth. Same-rate compatible assets
  remain hash-exact after verification.
- Decode source pages into a bounded cache of
  min(sourcePageCount,ceil(kernelTaps/pageFrames)+2) pages. Walk contiguous
  fragments through the periodic boundary, never reading stale partial-page
  padding. One encoded input page, one encoded output page and two manifest
  byte vectors are retained; no whole recording is resident. Charge coefficient
  tables, decoded cache capacity/control, fixed state and a 64 KiB I/O allowance
  before allocating. This is logical payload reservation, not RSS.
- Charge scratch against the existing core's remaining aggregate 32 MiB budget.
  Release it before allocating the replacement, and charge simultaneous old+new
  core pools. The E69 nonmedia estimate also includes one temporary Core facade.
  Insufficient capacity rejects restore; never shorten duration/drop history.
  Shared IAssetStore internals and allocator bookkeeping remain separately owned.
- Reserve converted active external page/manifest payload within the 4 GiB
  storage budget before writing, including simultaneous old/new reservations.
  Historical/orphan content-addressed blobs remain outside active reservation;
  garbage collection is outside this increment. Full-duration tests additionally
  measure actual committed file payload and filesystem allocation, with an
  explicit single-atomic-temp-blob bound.
- Any missing/corrupt/nonfinite asset, store failure, unsupported channel format
  or insufficient budget leaves the previous wrapper/core/state usable. New
  assets become visible only through a fully restored replacement core. No path
  enters host JSON. Error messages contain categories, not filesystem paths.

Acceptance: original cross-rate native restore must fail before implementation;
then test all 25 supported host-rate pairs and 500 coherent full-band tone cases
against unchanged T8, near-Nyquist diagnostics without inventing a new threshold,
full 300 s exact capacity/autoclose at each supported rate, full 300 s 44.1→192k base
plus undo conversion, mono/stereo fidelity, preserved undo/redo graph and unknowns,
corrupted/missing/nonfinite
assets, store failures, exact reservation boundaries, atomic old-instance
retention, and the complete owned storage/core/state/gesture/Ditto regressions.
Native and sanitizer results must distinguish instrumented code from reused JUCE.
