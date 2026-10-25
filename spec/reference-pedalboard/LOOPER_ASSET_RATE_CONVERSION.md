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
  chronological history, generations and unknown state fields. No stale source
  hash may be labeled as converted audio. Same-rate compatible assets stay exact.
- Use a periodic 64-tap Blackman-windowed sinc, normalized per output sample,
  with cutoff 0.45 * min(1, newRate/oldRate) in source cycles/sample. This is an
  offline engineering filter for rate ratios 1/8 through 8, covering all supported
  44.1–192k host-rate pairs; wider ratios reject without mutation. It is not
  claimed hardware converter coloration. Tests
  require constant/DC preservation, low-tone amplitude/pitch preservation and
  rejection of a source tone beyond the destination Nyquist frequency.
- Bound memory by two manifest byte vectors, at most five source PCM pages,
  one encoded source page, one encoded output page, fixed kernel/control state
  and a 64KiB I/O/control allowance. Validate lengths and reservation before
  allocation. Charge this against the existing prepared core's remaining
  aggregate 32MiB budget. Prepare the replacement core after conversion scratch
  is released and charge the simultaneous old+new core reservation. Insufficient
  capacity rejects the restore; do not shorten duration or silently drop history.
  Shared IAssetStore internals and allocator bookkeeping remain separately owned.
- Reserve all converted graph streams' active external page/manifest payload
  within the configured storage budget before writing, including the existing
  core plus replacement reservation while both are alive. Old content-addressed
  assets and partial orphan blobs on failure remain historical store data;
  global garbage collection is outside this increment.
- Any missing/corrupt/nonfinite asset, store failure, unsupported channel format
  or insufficient budget leaves the previous wrapper/core/state usable. New
  assets become visible only through a fully restored replacement core. No path
  enters host JSON. Error messages contain categories, not filesystem paths.

Acceptance: original cross-rate native restore must fail before implementation;
then test 44.1/48/96k duration/phase, mono/stereo primitive fidelity and alias
rejection, preserved undo/redo graph and unknowns, corrupted/missing/nonfinite
assets, store failures, exact reservation boundaries, atomic old-instance
retention, and the complete owned storage/core/state/gesture/Ditto regressions.
Native and sanitizer results must distinguish instrumented code from reused JUCE.
