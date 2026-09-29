## licensing.md

Licensing is a design waiting for the editions split (§0: "Nothing here is implemented yet"). This checkout has only the placeholder the spec replaces: `License` in `Source/Updates/Telemetry.h:301`, with states unlicensed/activated/grace/expired, `activate`/`revalidate`/`deactivate`, and an offline challenge that accepts any 16-character response. Its UI points are the grace countdown in `PluginEditor.cpp:834` and the HELP tab licence line in `HelpTab.cpp:164-173`, both covered by `Telemetry::licenceActivationAndGrace`. There is no Options -> Licence page yet. None of the Ed25519 / fingerprint / server / demo-mode design exists, so all rows are OWNED by the release helper (deferred).

| Req | Summary | Engine location | GUI location | Test | Status |
|---|---|---|---|---|---|
| LIC-0/1 (§0, §1) | Goals, non-goals, threat model T1-T10, L-1 targets — design only | - | n/a | - | OWNED |
| LIC-2 (§2) | Architecture: store webhook -> licence server with KMS private key -> signed licence verified offline with embedded public key — not built | - | n/a | - | OWNED |
| LIC-3 (§3.1-3.4) | Ed25519 via vendored Monocypher, signed canonical JSON licence document + `{licence, sig}` file, verify-before-parse, `kid` key rotation (current + next embedded), base-32 licence keys with checksum — not built (placeholder stores key hash + dates in plain JSON) | - | n/a | - | OWNED |
| LIC-4 (§4.1-4.3) | Five salted-hash fingerprint components per OS, 3-of-5 tolerant matching with silent refresh, per-user + machine-wide licence store, atomic writes — not built (placeholder uses `getUniqueDeviceID`, binds nothing) | - | n/a | - | OWNED |
| LIC-5 (§5) | 3 seats, online activate, one-click deactivate with queued retry, self-service portal rate limit, revocation — placeholder `activate/deactivate` only | `Updates/Telemetry.h:License` (placeholder) | - | `Telemetry::licenceActivationAndGrace` (placeholder only) | OWNED |
| LIC-6 (§6.1-6.5) | Server endpoints via existing `Transport` on a worker (privacy log), signed errors, serverless or vendor implementation, 30-day revalidation sending only id+fp, offline challenge/response (base-32/QR/file, 1-year validity), 30-day trial licence — not built (placeholder offline response accepts any 16 chars) | - | - | - | OWNED |
| LIC-7 (§7) | Integrity: multiple independent re-verifying checks, deferred consequences, runtime code-signature/Authenticode/Linux manifest checks, per-release variation, light obfuscation, never on audio thread, no anti-debug — not built | - | n/a | - | OWNED |
| LIC-8 (§8) | State machine Activated / Revalidation due (7-day header note) / Grace (banner) / Unlicensed demo mode (1 s faded silence per 60 s, exports disabled, never switches during playback), clock-rollback guard — placeholder grace countdown only | - | Header grace countdown `PluginEditor.cpp:834` (placeholder) | `Telemetry::licenceActivationAndGrace` (placeholder) | OWNED |
| LIC-9 (§9) | Privacy: no personal data, hashed ids, network-log entries category "licence", privacy dashboard view + "Deactivate and delete licence", enterprise relay URL — not built | - | - | - | OWNED |
| LIC-10 (§10) | Vendor choice (L-10 Keygen.sh recommended) — owner decision | - | n/a | - | OWNED |
| LIC-11 (§11) | Replace placeholder with `LicenceFile`, `MachineFingerprint`, `LicenceClient`, `LicenceState` in a Pro-only file; keep header countdown, HELP licence line, Options -> Licence; map `License::State` — not done (no Options Licence page exists) | - | HELP tab `HelpTab.cpp:164-173` (placeholder line) | - | OWNED |
| LIC-T (§11 Tests) | Signature (valid/tampered/wrong kid/truncated), fingerprint tolerance 2-5 of 5, state machine with mocked clock, click-free demo fade never during playback, no audio-thread licensing (`ThreadProbe`), privacy log entries — none | - | - | - | OWNED |
| LIC-Q (§12) | Open questions Q-L1..Q-L9 — undecided | - | n/a | - | OWNED |

<!-- counts DONE=0 NO-GUI=0 NO-TEST=0 PARTIAL=0 MISSING=0 OWNED=13 -->
