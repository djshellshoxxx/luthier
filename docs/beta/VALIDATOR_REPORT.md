# Luthier beta — validator & packaging report

**Version:** 1.0.0 (git `30ba998`, branch `codex/luthier-beta`)
**Platform:** Linux x86-64
**Build type:** Release (Ninja, Clang 18.1.3)
**Date:** 2026-09-30

This report records the pluginval and clap-validator runs, the standalone
launch check, and the contents and checksums of the Linux beta package.
Validator tools and versions match `scripts/ci_build.sh` exactly
(pluginval `v1.0.4`, clap-validator `0.3.2`).

## Summary

| Check | Result |
|-------|--------|
| Build (Standalone + VST3 + CLAP, Release) | **PASS** — `ninja` exit 0, all three formats produced |
| pluginval VST3 — strictness 5 | **PASS** (SUCCESS, exit 0, 0 warnings) |
| pluginval VST3 — strictness 10 | **PASS** (SUCCESS, exit 0, 0 warnings) |
| clap-validator CLAP | **PASS** (18 passed, 0 failed, 3 skipped, 0 warnings) |
| clap STATE reproducibility (previously fixed) | **PASS** — all four state tests pass |
| Standalone launch under xvfb | **PASS** — launched and ran, no crash |
| Package `dist/luthier-1.0.0-30ba998-linux-x64.tar.gz` | **PASS** — built, checksummed, verified |

No blocking issues. One informational portability note (glibc floor) and one
benign validator diagnostic are recorded below; neither fails validation.

## 1. Build

```
scripts/setup_linux.sh          # deps + JUCE 8.0.10 + clap-juce-extensions, configure build/
ninja -C build Luthier_Standalone Luthier_VST3 Luthier_CLAP
# BUILD_EXIT=0  (356/356 targets)
```

Artifacts (`build/Luthier_artefacts/Release/`):

- `Standalone/Luthier` — ELF 64-bit PIE executable, x86-64
- `VST3/Luthier.vst3/Contents/x86_64-linux/Luthier.so` — ELF 64-bit shared object
- `CLAP/Luthier.clap` — ELF 64-bit shared object

## 2. pluginval (VST3)

Command (from `ci_build.sh`, run under `xvfb-run`):

```
xvfb-run -a -s "-screen 0 1920x1080x24" pluginval \
  --strictness-level <5|10> --validate-in-process --timeout-ms 600000 \
  --output-dir build/logs --validate build/Luthier_artefacts/Release/VST3/Luthier.vst3
```

**Strictness 5:** `SUCCESS`, exit 0. No warnings, no errors.
**Strictness 10:** `SUCCESS`, exit 0. No warnings, no errors.

Plugin info reported by pluginval:

```
Plugin name: Luthier
Alternative names: Luthier
SupportsDoublePrecision: no
Reported latency: 0
Reported taillength: 12
Main bus: 0 in / 2 out
Input layouts:  Mono, Stereo, Discrete #1
Output layouts: Stereo / Mono variants + Discrete #1
```

Test sections exercised at strictness 10 (all passed):

```
Scan for plugins, Open plugin (cold), Open plugin (warm), Plugin info,
Plugin programs, Parameters, Automatable Parameters, Automation,
Editor Automation, Editor, Open editor whilst processing, Audio processing,
Non-releasing audio processing, Background thread state,
Parameter thread safety, Plugin state, Plugin state restoration,
Basic bus, Listing available buses, Enabling all buses,
Disabling non-main busses, Restoring default layout, Fuzz parameters,
vst3 validator (skipped: external validator path not set), auval (n/a on Linux)
```

Full logs: `build/logs/pluginval-s5.log`, `build/logs/pluginval-s10.log`
(under the gitignored `build/` tree; not committed).

## 3. clap-validator (CLAP)

Command (from `ci_build.sh`, run under `xvfb-run`):

```
xvfb-run -a -s "-screen 0 1920x1080x24" clap-validator validate \
  build/Luthier_artefacts/Release/CLAP/Luthier.clap
```

**Result:** `21 tests run, 18 passed, 0 failed, 3 skipped, 0 warnings` (exit 0).

STATE reproducibility (the area a prior fix addressed) — **all pass**:

- `state-buffered-streams` — PASSED
- `state-invalid` — PASSED
- `state-reproducibility-basic` — PASSED
- `state-reproducibility-flush` — PASSED
- `state-reproducibility-null-cookies` — PASSED

Other passing tests: `descriptor-consistency`, `features-categories`,
`features-duplicates`, `param-conversions`, `param-fuzz-basic`,
`param-set-wrong-namespace`, `process-audio-out-of-place-basic`,
`process-note-inconsistent`, `process-note-out-of-place-basic`,
`scan-rtld-now`, `scan-time`, `create-id-with-trailing-garbage`,
`query-factory-nonexistent`.

Skipped (3): the `clap.preset-discovery-factory` tests
(`preset-discovery-crawl`, `preset-discovery-descriptor-consistency`,
`preset-discovery-load`) — the plugin does not implement the optional preset
discovery factory, so the validator skips them. Not a failure.

### Benign diagnostic (no action needed)

clap-validator prints one stderr line before the run:

```
Warning: CLAP asked for plugin_id 'com.luthieraudio.luthierx1' and
JuceCLAPWrapper ID is 'com.luthieraudio.luthier'
```

This is emitted **by the validator's own negative test**
`create-id-with-trailing-garbage`, which deliberately asks the factory to
instantiate a bogus id (the real id `com.luthieraudio.luthier` with `x1`
appended) and asserts the factory returns null. The wrapper correctly rejected
it and that test **PASSED**. The configured `CLAP_ID` in `CMakeLists.txt` is
`com.luthieraudio.luthier` and matches the wrapper — there is no id
misconfiguration. No change made.

Full log: `build/logs/clap-validator.log` (gitignored; not committed).

## 4. Standalone launch (xvfb)

The standalone was launched headless and left running ~12 s, then signalled to
quit:

```
xvfb-run -a -s "-screen 0 1920x1080x24" build/Luthier_artefacts/Release/Standalone/Luthier
# Process stayed alive (window + audio device initialised); no crash. Clean exit on TERM.
```

The only stderr output is a benign ALSA message because the container has no
MIDI sequencer device:

```
ALSA lib seq_hw.c:528:(snd_seq_hw_open) open /dev/snd/seq failed: No such file or directory
```

This is environmental (no `/dev/snd/seq` in the CI container), not a plugin
fault. Launch result: **PASS**.

## 5. Package

Built with `scripts/package_beta_linux.sh` (added in this branch), which stages
via `scripts/ci_build.sh stage` and wraps the products into the beta archive.

**Archive:** `dist/luthier-1.0.0-30ba998-linux-x64.tar.gz`
**Size:** 39,679,120 bytes (38 MiB)
**Entries:** 1649

Top-level layout (deterministic tar — sorted, fixed owner/mtime):

```
luthier-1.0.0-30ba998-linux-x64/
├── Luthier.vst3/            VST3 bundle (Contents/x86_64-linux/Luthier.so)
├── Luthier.clap             CLAP plugin
├── luthier                  Standalone (ELF x86-64)
├── Resources/               Factory content shared by all three formats
│   ├── BodyIRs/  CabIRs/  Examples/  Fonts/  Guitars/
│   └── Parts/  Presets/  Riffs/  Tunes/
├── INSTALL.md               Install paths + verify + uninstall
└── VERSION                  1.0.0-30ba998
```

The per-bundle duplicate content dirs (`Contents/Resources/{BodyIRs,CabIRs,…}`)
are stripped by the stage step; every format reads the single shared
`Resources/` tree, matching how the installers lay it out.

`INSTALL.md` documents: VST3 → `~/.vst3`, CLAP → `~/.clap`, factory content →
`~/.local/share/luthier/Resources`, standalone → `~/.local/bin/luthier` (or run
in place); plus the system-wide paths and checksum verification.

### Checksums

`dist/luthier-1.0.0-30ba998-linux-x64.SHA256SUMS` (paths relative to the
directory holding the archive; verify with
`cd dist && tar xzf … && sha256sum -c …`):

```
e05535f5296178ec7a0824feb2cde5188daf7d7b8c70f1c222be232b958e47e7  luthier-1.0.0-30ba998-linux-x64.tar.gz
53ccea479b0b07d68b6ac9eff5d8c08998ba360c7f3d2f189fe386cf768059a8  luthier-1.0.0-30ba998-linux-x64/luthier
41f0aa9bef947646493c358fa352a0b4d97effe3b03ebee923a9b64a71b25d96  luthier-1.0.0-30ba998-linux-x64/Luthier.clap
6efa39f517e0efe5a9fe860861221a69510531b9aa719bdcc5170697a28a5703  luthier-1.0.0-30ba998-linux-x64/Luthier.vst3/Contents/x86_64-linux/Luthier.so
```

Verification was run in a clean directory: **all 4 lines `OK`**.

> The archive and checksums live under `dist/` (gitignored) and are **not**
> committed. The hashes above are the record of what the beta build produced;
> re-running the build from the same commit reproduces the same archive
> (deterministic tar + `SOURCE_DATE_EPOCH` from the last commit).

## 6. Notes flagged for others (not fixed here)

Per the fixing policy, packaging/config issues were fixed in this branch; the
following is **informational** and not a bug in scope for me to change:

- **glibc floor 2.38.** The Standalone and VST3 `.so` reference `GLIBC_2.38`
  as their highest symbol version, so this beta build only runs on
  distributions with glibc ≥ 2.38 (Ubuntu 24.04 / Fedora 39 era and newer). It
  will **not** load on Ubuntu 22.04 (glibc 2.35) or similar. This is a property
  of the build host, not a code bug. If broader compatibility is required for
  the beta, the release build should be produced on an older base image (or in
  a manylinux-style container). Routing this to whoever owns the release build
  matrix. No DSP/parameter/state/threading issues were observed by either
  validator.
