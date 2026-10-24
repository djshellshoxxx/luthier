# Pedalboard preview packaging audit and compatibility fixes

This checkpoint changes packaging scripts and fixture tests only. It does not
publish a release, install production binaries, certify a platform or change DSP.

## Corrected paths and checksum contract

`package_beta_linux.sh` now selects the requested `LUTHIER_EDITION=PAID|FREE`
bundle (`Luthier Pro`/`Luthier Free`). It refuses an available wrong edition and
prefers the selected edition when both are staged. Historical unqualified
`Luthier.vst3` staging remains supported with its original neutral archive name;
it is never relabelled Pro/Free. Current archives use
`luthier-pro-<version>-<sha>-linux-x64` / `luthier-free-...`; legacy archives keep
`luthier-<version>-<sha>-linux-x64`. The package README and quoted copy commands
use the actual selected bundle name, including its spaces.

The packager refuses missing/empty standalone and VST3 binaries, a nonexecutable
standalone, empty factory Resources and an empty optional CLAP. A missing CLAP is
supported and explicitly described as absent. It emits an archive checksum and
checksums of the extracted standalone, optional CLAP and VST3 object. Extract the
archive beside its `*.SHA256SUMS` sidecar before verifying all those records.
The archive checksum protects the complete shipped content; the per-binary
records are additional checks, not an inventory of every extracted resource.

`verify_release.sh <directory> [public-key.asc]` accepts all existing manifest
families: `SHA256SUMS.txt`, `SHA256SUMS`, and `*.SHA256SUMS`. It checks every
recognized manifest, rejects empty/malformed/duplicate records and conflicting
hashes, handles spaces and literal regex characters in paths, and rejects
absolute paths, traversal and symlink targets. Every top-level payload, including
hidden files, must appear in a manifest. Present signature/hash sidecars remain
metadata exemptions. The legacy `release_manifest.sh` summary (`manifest.txt`) is accepted only when
its complete hash/byte-size/file set agrees with the verified checksum records.
Specifying a public key requires each checksum manifest signature, or for legacy
`SHA256SUMS`, the matching validated summary signature. Present Linux package
signatures are also verified in a temporary keyring.
Relative public-key paths resolve before changing into the release directory.
No-key runs explicitly report that signatures were not checked. Requested
signature verification fails if GPG is unavailable.

## Evidence

Command: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s scripts -p
 test_release_packaging_compat.py -v` (one shell line).

The original root `b5bec374` scripts fail **21 fixture assertions**; the corrected
scripts pass **12 tests**, including subcases for Pro, Free, legacy names,
wrong-edition refusal, both editions staged, missing CLAP, required-file failures,
repeatable archive bytes, all three manifest names, binary markers, CRLF, missing
and corrupted payloads, hidden/unlisted payloads, duplicate/conflicting manifests,
traversal and symlinks. A real temporary Ed25519 GPG key signs each manifest name;
valid signatures pass, missing signatures and modified signed manifests fail.
Legacy signed-summary authentication and mismatched/incomplete summary refusal
are also covered.
Test key material is generated and deleted only in a temporary fixture directory.
Synthetic payloads deliberately do not claim executable/host validation.

Existing isolated installer fixtures also pass: Linux install/upgrade/downgrade/
uninstall **27 checks**; macOS uninstall script logic **19 checks**. These operate
on temporary synthetic trees. They do not exercise a real macOS Installer.app,
Windows installer, current-source plugin scan or audio device.

## Remaining release gates

The configured build matrix is Linux x86-64 and Windows x64 (VST3, CLAP,
standalone), plus macOS Universal x86-64/arm64 (AU, VST3, CLAP, standalone), with
Pro and Free editions. Actual artifacts, complete test/validator results and host
smoke tests for the current frozen source are still required. No matching
current-source installer/checksum/provenance set was available in the local tree
at this audit; the coordinator's native build was still running.

For provenance, pass one immutable 40-character source SHA through every platform
job and preserve its logs. The general release workflow accepts a symbolic ref,
which can move between jobs; the coordinator owns that separate audit. The
existing beta2 workflow/notes are scoped to PR77 and explicitly exclude this
pedalboard, so they cannot establish its identity or feature claims. The beta
asset assembler validates binary architectures and presence of Resources, but
its current presence check alone is not a complete factory-file inventory.
Inspect actual archive contents and record hashes for the selected source.

Windows publisher signing and macOS Developer ID/notarization are optional in
existing scripts. Credential presence and actual signed/notarized results were
not inspected or inferred. Record the true status for each produced artifact.
Named-model acceptance, isolated CPU measurements and other open DSP gates remain
in `PEDALBOARD-TIMEFX.md`; packaging success cannot close them.
