# Pedalboard QA and native-build checkpoint — 2026-10-26

## Current source and ownership

- Repository: `djshellshoxxx/luthier`.
- Integration branch: `codex/luthier-pedalboard-completion`.
- Source baseline before the current tested changes: `51df65e9258c96dfdd3ec5047cd15e475b82f5b1`.
- Current tested candidate includes local, uncommitted changes to the cabinet/body
  convolution preparation, preset-preview Vorbis encoding and regression tests,
  reference-pedal realtime soak checks, and their specs/coverage notes. The
  source commit SHA will be added after checkpointing those changes.
- One active coordinator; no implementation workers are currently active. A
  narrow read-only review completed. Other Luthier worker branches/worktrees
  remain intact and were not merged or modified in this checkpoint.
- The present executor grants `/workspace` as a writable root. Work stayed in
  `/workspace/luthier`.

## Completed tests and artifacts

The earlier full suite is terminal and failed; it must not be confused with the
focused green runs below. Its log is
`/workspace/luthier-evidence/engine-retirement-source8/full-suite-writable-home.log`
(SHA-256 `22ea411849136dba0ce7b38771116df5ef66093f92b0ec02e6bae5e41ecf1f02`).
It ran 2,805 tests and 14,933,248 checks over 5,711.4 seconds (95.2 minutes),
with one failed test: `PresetPreview.PB02_shippedClipsMatchFreshRenders`, where
Wah Funk Rhythm decoded at −33.18 LUFS versus −32.66 LUFS, a 0.52 LU difference
against the unchanged 0.5 LU limit. Within that same run,
`seededRandomConfigurations` passed 2,000 configurations/checks and
`structuralChangesWhileAudioRuns` passed both checks. The log does not record an
exact source commit SHA. It predates the current PB-02/tiny-buffer candidate.

On the current dirty source tree derived from the baseline above:

| Check | Result | Evidence |
| --- | --- | --- |
| Fuzz filter including tiny-block regression | 13 tests, 1,144,712 checks, 180.7 s, pass | `fuzz-tinyblock-final.log`, SHA-256 `d6fdeaa6cf5ba83a7bd70405eefdda53ae94061b26524cf998805f07552b87fb` |
| Tiny host blocks 1, 2, 3, 4 at 8 kHz, two loaded presets, 20,000 blocks/preset/size | Included above; 2,102 checks, 14.6 s, pass | same fuzz log; pre-fix failure details in `BUFFER-SIZE-1-4-HEAP-CORRUPTION-2026-10-26.md` |
| Preset preview suite including PB-01/02/03 | 23 tests, 1,671 checks, 162.6 s, pass | `preset-preview-final.log`, SHA-256 `4a200d9be0b94477e3417969d2961a11e6b96ba1f53093e10e607c8c96395a67` |
| Reference-pedal race + 30-second concurrent soak | 6 tests, 1,054 checks, pass; 3,000 edits, 123,798 blocks, 54,615,551 samples, callback allocations=0, locks=0, invalid=0 | `refpedal-soak-final.log`, SHA-256 `371c9907270a47fdc26d540682691bb320d4694d0573aaeec71cf1a013cf9048` |
| PB-27 performance | 1 test, 9 checks, 61.0 s, pass; median factory render 859.7 ms, slowest 1,145.9 ms | `pb27-perf-final.log`, SHA-256 `fee2c81a1cd0a4aeb4ce8b8cbb1ed350ae1e597e11e8c77194eafca09a73d37b` |
| Fresh Linux Pro Standalone, VST3, CLAP build | Exit 0 | `linux-formats-final-build.log`, SHA-256 `9015558eadf6bbc5221a5944b17a68924ac1ff6100158c10ecd2e991b96bb1f2` |
| Linux VST3 pluginval strictness 5 and CLAP validator | Exit 0; VST3 passed; CLAP 21 tests: 18 passed, 0 failed, 3 skipped | `linux-formats-validation.log`, SHA-256 `807c8c803d4c834e53fbf64921a2424f1fd6698860244a8f3d1fdec9f6936e4f` |
| CLI render of shipped previews | Exit 0; 70 Ogg files, 2,931,405 bytes total, 214,323 bytes under 3 MiB cap | `render-previews-final.log`, SHA-256 `c5a1fa11f18a636642c828e2a047bd572322415aedd091d53d3470e7d6d0c599` |
| Standalone launch smoke under Xvfb | Stayed open for 10 s; expected timeout status 124; not audio-device acceptance | `linux-standalone-smoke.log`, SHA-256 `5ca930287de272c63c96482c554e9fd62c83e309185cac44b8552f3368800c40` |

Linux product hashes:

- Pro Standalone: `3d8efa395d4e119d1460ec7c2a855d259ea74aae298e1ba86b4dbb352bac3502`
- Pro VST3 ELF: `d42f2e534024e004c042241db500de5103bdd0d3aa81c753933a94cd160ae2da`
- Pro CLAP ELF: `ecd337691797b30f4bc9a2cdd403ca956762e0c0e54ef8a46a093d82dc29195f`

These are local Linux diagnostics, not qualified release artifacts. The
standalone smoke logged that ALSA `/dev/snd/seq` is unavailable in this runner.
CLAP validator skipped three preset-discovery tests because that optional
factory is not implemented, and printed a plugin ID mismatch warning
(`com.luthieraudio.luthierx1` requested versus JUCE wrapper
`com.luthieraudio.luthier`). Its exit status was still 0; the mismatch remains
for triage.

## Source changes and unresolved risks

- PB-02 retains q0.5 as the default. If q0.5 decoded loudness misses the
  existing 0.5 LU threshold, q0.6 is retained only if it meets the same
  threshold and the 64 KiB per-clip cap. Current PB-02 passed with 0.787941 dB
  max spectrum RMS, 0.458321 LU max loudness delta, 2,931,405 total bytes, q0.6
  retained for Wah Funk Rhythm, and three cap fallbacks. No acceptance limit was
  relaxed. Release builds must regenerate preview files and bump the plugin
  version so old sound-hash caches are invalidated.
- Tiny-buffer candidate prepares cabinet/body/IR-variant convolution capacity
  at a minimum of 8 while preserving host callback sizes 1–4. The fixed
  regression passed as above, with both impulse responses installed and
  reloaded. The original heap corruption was reproducible in a prior run, but
  the exact allocator overwrite/root cause is unproven. No current ASan or
  Valgrind proof exists; retain the finding as a risk, not a proven root-cause
  fix.
- The full suite has not been rerun on the current candidate. Historical QA
  logs report 16 randomized host-state re-save mismatches; the generated
  `docs/review/QA_ROBUSTNESS.md` retains them as findings. Deterministic state
  restoration is not fully green.
- A separate additional UI callback stress operation was automatically
  security-blocked and remains unexecuted. Do not retry or repackage it. The
  existing UI lifecycle tests in the full run are separate evidence.
- No actual Windows or macOS build/validator has passed on this current source.
  A Mac-only diagnostic Actions run was observed building earlier, but reading
  run `40292011494` was denied with HTTP 403 Forbidden (no reason/rate headers
  returned); its final result is unknown and must not be represented as a
  pass. No Windows candidate for this current source was verified.
- No Free build, actual hardware/DAW audio acceptance, ASan/Valgrind run, or
  complete current-source cross-platform test matrix has passed. Linux build
  and validator success is not a release gate.

## Next steps

1. Commit and secret-check this tested source checkpoint on a new, unique
   `codex/luthier-checkpoint-*` branch; verify the remote SHA after push. Do not
   overwrite `codex/luthier-checkpoint-20261026-resume` or the product branch.
2. Continue Linux-focused diagnostic/fix work for the 16 host-state
   round-trip findings without weakening requirements; then rerun affected
   tests and the complete suite when the candidate is stable.
3. Run supported Windows standalone and Pro/Free plugin targets, both macOS
   architectures, and their real validators on current source. Preserve any
   failures. The separate denied Actions read is not permission to retry or
   route around access.
4. Only after platform and full QA gates pass, produce release installers and
   VST3/CLAP packages with provenance, checksums, licenses, installation notes,
   and clear signing/notarization status. No release was created by this
   checkpoint.

## Recovery

Start in `/workspace/luthier`, verify the branch and clean/dirty state with
`git status --short` and `git rev-parse HEAD`, then read this handoff and the
small-buffer handoff linked above. Test logs are local under
`/workspace/luthier-evidence/engine-retirement-source8/`; verify their hashes
before relying on them. Exclude Python cache directories from all Git adds.
