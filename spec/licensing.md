# LICENSING SPEC: COPY PROTECTION FOR LUTHIER PRO

Design for licensing the paid edition (`editions.md`). Nothing here is
implemented yet; it replaces the placeholder `License` class in
`Source/Updates/Telemetry.*` when the coordinator performs the editions
split. It extends `updates-telemetry.md` 5 and obeys its privacy rules.

Status: recommendation for the product owner. Decisions are marked
**L-n** with a reason; section 12 lists what the owner must decide.

## 0. Goals and non-goals

Goals, in priority order:

1. **Never hurt a paying customer.** No audio interruption mid-session,
   no failure on a gig without network, no lost work, no licence loss on
   a new hard drive or OS update, fast self-service seat moves.
2. **Keep honest people honest** with no friction: activation takes one
   paste of a key or one sign-in, works offline for months.
3. **Make casual sharing useless**: a copied licence file does not work
   on another machine; a posted serial number burns itself (activation
   limit, revocation).
4. **Raise the cost of cracking** enough that a release is later, less
   stable and needs redoing for each version, without chasing the
   impossible goal of stopping it.
5. **Minimal cost and infrastructure** for a small company.

Non-goals: defeating a determined reverse engineer permanently (nobody,
including iLok, achieves this); DRM on presets or audio output; any
protection that phones home during use.

## 1. Threat model

| Threat | Who | Likelihood | Impact | Mitigation (section) |
|---|---|---|---|---|
| T1 Sharing a serial number with friends / online | casual | high | medium | activation limit, revocation, per-customer keys (3, 5) |
| T2 Copying the activated licence file to another machine | casual | high | medium | machine binding (4) |
| T3 A key generator | cracker | medium | high | asymmetric signatures: the binary holds only a public key, so no keygen is possible without the vendor's private key (3) |
| T4 Editing the licence file (dates, seat count, edition) | casual / cracker | medium | medium | the whole licence is signed; any change invalidates it (3) |
| T5 Patching the binary to skip the check | cracker | medium (for a $200 plugin, near-certain eventually) | high | several independent verifications, integrity checks, per-release variation (7); accept residual risk (0 non-goal) |
| T6 Emulating the activation server | cracker | low | high | server responses are signed; the server's signing key never ships (3, 6) |
| T7 Clock rollback to extend a grace or trial | casual | medium | low | monotonic "last seen" time in the licence store (8) |
| T8 Chargeback fraud / stolen cards | fraudster | low | medium | revocation list delivered with revalidation (6.3) |
| T9 Leaked private signing key | insider / breach | very low | critical | key in an HSM or a vendor service; key rotation with versioned key IDs (3.3) |
| T10 Privacy harm from licensing telemetry | us | — | high (trust) | no identifiers beyond the licence, hashed fingerprints, no usage data (9) |

**L-1** Target: T1, T2, T3, T4, T6, T7 fully mitigated; T5 made costly.
Reason: the first group is where almost all lost revenue comes from for
an indie plugin; T5 cannot be prevented, only delayed, and every hour
spent beyond "costly" returns little while risking false positives that
hurt customers (goal 1).

## 2. Architecture overview

```
 customer ── buys ──> store (Paddle / FastSpring / Stripe)
                          │ webhook
                          v
                   licence server (6)  ── holds the Ed25519 PRIVATE key (in an HSM / KMS)
                          │  signed licence (JSON + signature)
                          v
 Luthier Pro ── activation request (key, hashed fingerprint) ──> server
            <── signed machine licence ─────────────────────────
            stores it in the licence store (4.3), verifies OFFLINE
            with the embedded PUBLIC key on every load
```

Everything the plugin trusts is signed by the server. The plugin can
verify, never mint. The network is needed once per activation and once
per revalidation window; everything else is offline.

## 3. Licence format and cryptography

### 3.1 Keys

- **Ed25519** (RFC 8032) signatures: small (64-byte signatures, 32-byte
  public keys), fast to verify, deterministic, no parameter choices to
  get wrong. **L-2** Reason: RSA works too, but Ed25519 has simpler,
  constant-time implementations and tiny keys to embed.
- Library: **libsodium** (ISC licence) or **Monocypher** (public domain /
  CC0, a single C file, easy to vendor under `ThirdParty/`). JUCE's own
  `juce_cryptography` RSA is not recommended (no constant-time guarantee,
  no Ed25519). **L-3** Monocypher, for zero build-system impact.
- The binary embeds the **public** key(s) only. No secret that can mint
  a licence ever ships (rule of T3).

### 3.2 Licence document

A UTF-8 JSON payload, canonicalised (sorted keys, no whitespace) before
signing, stored with its signature:

```json
{
  "v": 1,
  "kid": "2026-01",
  "product": "com.luthieraudio.luthier",
  "edition": "pro",
  "licenseId": "lic_8f3c...",
  "customer": "a3f1c9...",
  "seat": 2,
  "seats": 3,
  "machine": { "fp": ["h1...", "h2...", "h3...", "h4...", "h5..."], "min": 3 },
  "issued": "2026-09-24T10:00:00Z",
  "validUntil": "2026-12-23T10:00:00Z",
  "revalidateAfter": "2026-10-24T10:00:00Z",
  "majorVersions": [1],
  "features": ["all"],
  "trial": false
}
```

- `customer` is an opaque hash, not a name or e-mail (section 9).
- `validUntil` is the end of the offline window (30-day revalidation +
  60-day grace, section 8) for a subscription-free perpetual licence;
  an online revalidation simply issues a new document.
- `majorVersions` lets a v2 paid upgrade exist without a new scheme.
- `features` is reserved for future bundles; Pro uses `["all"]`.

The file on disk is `{"licence": "<base64 payload>", "sig": "<base64>"}`.
The client verifies the signature over the exact payload bytes and only
then parses the JSON (never trust parsed-then-reserialised data).

### 3.3 Key rotation

`kid` selects the public key. The binary embeds the current key and the
next one. If a private key is ever suspected compromised, the server
starts signing with the next key and a release removes the old one;
licences signed with the old key are re-issued transparently at the next
revalidation. **L-4** The private key lives in a cloud KMS / HSM that can
sign but never export (AWS KMS and Google Cloud KMS both support
Ed25519 signing today), or with the licensing vendor of section 10.

### 3.4 Licence keys (what the customer types)

A random 128-bit identifier encoded as 5 groups of 5 base-32 characters
(`LTHR-7K2QD-...`) with a checksum character. It is a lookup key for the
server, not itself a signed licence: an unused key is worthless offline,
a used one is subject to the seat limit. Customers who prefer can sign
in with their account e-mail instead (the server finds their keys).

## 4. Machine binding

### 4.1 Fingerprint

A set of **five** independent components, each hashed with a per-product
salt (SHA-256, truncated to 16 bytes) so the raw values never leave the
machine and cannot be correlated across vendors:

| # | Windows | macOS | Linux |
|---|---|---|---|
| 1 | `MachineGuid` (registry) | `IOPlatformUUID` | `/etc/machine-id` |
| 2 | system volume serial | boot volume UUID | root filesystem UUID |
| 3 | CPU brand string + core count | CPU brand + core count | CPU model + core count |
| 4 | primary network adapter MAC (first physical) | built-in Ethernet / Wi-Fi MAC | first physical interface MAC |
| 5 | computer name | computer name | hostname |

JUCE's `SystemStats::getUniqueDeviceID()` (used by today's placeholder)
covers part of this; the recommended implementation computes the five
separately so matching can be tolerant.

### 4.2 Tolerant matching

The licence stores all five hashes and `min` (3). The licence is valid
on a machine when **at least 3 of 5** components match. **L-5** Reason:
a new network card, a renamed computer and a replaced disk are normal
events; requiring an exact match is the single largest source of support
tickets with fingerprint schemes. When 3 or 4 match, the plugin silently
requests a refreshed licence at the next online moment so drift never
accumulates.

Virtual machines and cloud desktops: allowed (they count as one seat
each); some components are unstable there, and the offline
challenge / response of 6.4 covers the rest.

### 4.3 Licence store

- Per user: `userApplicationDataDirectory/Luthier/licence.json`
  (`%APPDATA%`, `~/Library/Application Support`, `~/.config`), as today.
- Also readable machine-wide (`commonApplicationDataDirectory`) so a
  studio admin can activate once for all users of a machine.
- Written atomically (temp file + rename), per `file-formats.md`.
- The file is not secret. It is signed, not encrypted (T2 and T4 are
  handled by the signature and the fingerprint).

## 5. Activation, limits and deactivation

- **Seats**: 3 simultaneous machines per licence by default
  (**L-6**: the industry norm is 2-3; 3 covers studio, laptop and a
  spare, avoiding most support mail).
- **Activate** (online): the plugin sends licence key, the five
  fingerprint hashes, product, version, OS family. The server checks the
  key, the seat count, the revocation list, and returns a signed
  licence (3.2).
- **Deactivate**: one click in Options -> Licence (`updates-telemetry.md`
  5: "one click and immediate"). The plugin holds no secret to sign
  with, so the request carries the licence id and the fingerprint
  hashes, and the server frees the seat only for a machine whose
  fingerprint matches that activation. The local
  file is deleted immediately regardless of the network result; if the
  server was unreachable, the deactivation is queued and retried.
- **Self-service portal** (web): the customer sees their machines (by
  the computer name they chose to show, never raw identifiers) and can
  release a seat for a machine that died. Rate-limited (e.g. 5 releases
  per 30 days) so it cannot become a seat-sharing tool.
- **Revocation**: refunded or fraudulent licences are revoked on the
  server; the revocation takes effect at the next revalidation.

## 6. The licence server

### 6.1 Interface

HTTPS + JSON, versioned path. The plugin talks only to these endpoints,
through the existing `Transport` abstraction in `Source/Updates` (so the
privacy log of `updates-telemetry.md` 0.3 records each call) on a worker
thread, never the audio thread.

| Method | Path | Request | Response |
|---|---|---|---|
| POST | `/v1/activate` | `{key, product, version, os, fp[5], machineName?}` | `{licence, sig}` or `{error: "seats_exhausted" | "invalid_key" | "revoked" | ...}` |
| POST | `/v1/revalidate` | `{licenseId, fp[5]}` | fresh `{licence, sig}` or `{error}` |
| POST | `/v1/deactivate` | `{licenseId, fp[5]}` | `{ok}` |
| POST | `/v1/offline/response` | (web form, not the plugin) challenge text | response text |
| GET | `/v1/keys` | — | current public keys by `kid` (informational; the plugin trusts only embedded keys) |

Error responses are also signed so a fake server cannot, for example,
tell a customer their licence is revoked (T6).

### 6.2 Implementation options

Either a vendor (section 10) or a small service: a serverless function
(Cloudflare Workers, AWS Lambda) with a database table of keys, seats and
activations, a KMS key for signing, and the store's webhook creating
keys. Estimated: a few hundred lines, a few dollars a month at indie
volume.

### 6.3 Revalidation

Every 30 days (`updates-telemetry.md` 5), in the background, on plugin
load, only when a network is available. It returns a new licence with a
new `validUntil`, or an error that the plugin handles per section 8. It
sends only the licence id and fingerprint hashes, never the key
(`updates-telemetry.md` 5: "revalidation sends only a signed proof").

### 6.4 Offline (air-gapped) activation

1. The plugin shows a challenge: base-32 text encoding
   `{key, fp[5], product, version, nonce}` (short enough to type or
   photograph, also offered as a QR code and a "Save challenge file"
   button).
2. On any other device, the customer pastes it into the web form
   (`/v1/offline/response`), signed in to their account.
3. The response is the signed licence itself (base-32 or a small file to
   carry over on a USB stick). The plugin verifies it exactly like an
   online one. Offline licences get a long `validUntil` (1 year), since
   the machine may never be online; they renew the same way.

This replaces today's placeholder, which accepts any 16-character
string as a response.

### 6.5 Trial

A 30-day trial is a licence with `trial: true`, bound to the machine,
issued by `/v1/activate` with a trial request (one per fingerprint set).
It runs the Pro edition in full. **L-7** Reason: a full Pro trial is a
better sales tool than the Free edition alone and keeps Pro code out of
the Free binary (`editions.md` Q-8).

## 7. Integrity and tamper resistance (layered, proportionate)

The aim is to make patching expensive and fragile, not impossible, and
to never risk a false positive against a paying customer.

1. **Several independent checks.** The licence is verified in more than
   one place, by separately compiled code paths (e.g. at plugin load, at
   editor open, when a preset is saved, when the Workshop or Tune tab is
   first opened, when an export is written), so there is no single
   branch whose removal unlocks everything. Each check re-verifies the
   signature rather than reading a cached boolean.
2. **Deferred, non-obvious consequences.** A failed check does not show
   an error at the point of the check; it sets state that the ordinary
   unlicensed behaviour of section 8 acts on later. This makes it harder
   to find the check from the symptom.
3. **Binary integrity.** On macOS the code signature is verified at
   runtime (`SecStaticCodeCheckValidity` on the bundle); on Windows the
   Authenticode signature (`WinVerifyTrust`). A modified binary fails
   these, which is then treated as "unlicensed" (never as a crash, and
   never as an accusation in the UI). Linux: a hash of the plugin file
   compared against a signed manifest shipped beside it.
4. **Per-release variation.** Check placement and the embedded key
   encoding change a little with every release, so a crack for one
   version does not transfer mechanically to the next.
5. **Obfuscation, lightly.** String literals for the licence UI and the
   server URL are not stored in plain text; the licence code is built
   with symbols stripped and without RTTI names that describe it. No
   commercial virtualising obfuscator (**L-8**: they cause antivirus
   false positives and host-compatibility problems in audio plugins,
   which directly violates goal 1).
6. **Never** in the audio thread: every verification runs on the message
   thread or a worker, and its result reaches the audio thread only as
   the existing atomic parameter-style state. No check ever blocks,
   allocates or locks in `processBlock` (`engine.md` 0).
7. **No anti-debugging or kernel tricks.** They break legitimate
   debugging of host crashes, upset antivirus software and hosts, and
   are the first thing crackers remove anyway.

## 8. What happens when the licence is not valid

Rule: **audio never stops mid-session because of licensing**, and a
customer with a valid purchase is never inconvenienced before the grace
period ends.

| State | Condition | Behaviour |
|---|---|---|
| Activated | valid signature, fingerprint matches, before `revalidateAfter` | Full product, no UI |
| Revalidation due | after `revalidateAfter`, before `validUntil` | Full product; silent background retries; after 7 days a small header note "Connect to the internet to refresh your licence (N days left)" |
| Grace | `validUntil` passed by less than 14 days (`updates-telemetry.md` 5) | Full product; a dismissible header banner each session |
| Unlicensed (never activated, expired, revoked, tampered) | otherwise | **Demo mode** below |

Demo mode (the unlicensed Pro binary):

- Everything works and the project **loads and saves normally** (never
  hold a customer's session hostage).
- Every 60 seconds of playback, 1 second of output is faded to silence
  (a gentle 20 ms fade out and in, never a click), and a small,
  non-modal "Luthier Pro is not activated" note with an "Activate" button
  sits in the header.
- Exports (audio, MIDI, notation) are disabled with the same note.
- The state change from licensed to demo never happens during
  playback: it is evaluated at plugin load and at editor open, and a
  change found at another time waits for the transport to stop.
- Clock rollback (T7): the licence store keeps the latest time it has
  seen (signed by nothing, but monotonic); a current time more than two
  days before it is treated as "revalidation due" rather than trusted.

**L-9** Reason: silence intervals are the audio-plugin industry's
accepted unlicensed behaviour; they make an unlicensed copy unusable for
finished work while letting a customer whose licence has a problem keep
working and open their project.

## 9. Privacy (`updates-telemetry.md` 0 and 5)

- Licensing is the one network feature not opt-in, because the customer
  initiates activation; it sends only what section 6.1 lists.
- No personal data in the licence file or requests: the customer id is a
  hash, fingerprints are salted hashes, the machine name is optional and
  chosen by the user for the portal.
- No usage data, no preset names, no file names, no IP retention beyond
  30 days of server logs for abuse handling.
- Every licensing call appears in the local network log with destination,
  size, time and category "licence".
- The privacy dashboard (`updates-telemetry.md` 6) shows what the licence
  file contains, in plain text, and a "Deactivate and delete licence"
  button.
- Enterprise policy (`updates-telemetry.md` 7): the licence server URL
  can be pointed at an on-premises relay; offline activation needs no
  network at all.

## 10. Commercial alternatives

| Option | What it is | Cost (indicative, verify before deciding) | Pros | Cons |
|---|---|---|---|---|
| **iLok / PACE (iLok License Manager + Eden)** | Industry-standard dongle/cloud/machine licensing plus binary wrapping | Setup fee plus per-licence fees; wrapping requires a PACE agreement | Strongest practical protection; users of pro studios already have iLok | Cost; iLok account required (a real friction and a frequent complaint); wrapping can delay releases; Linux support limited |
| **Keygen.sh** | Licensing API with Ed25519-signed licence files, machine activation, offline licences | Free tier, then monthly plans by active licences | Very close to this design; well documented; self-hostable (Keygen CE) | Still need client code and a store webhook; SaaS dependency unless self-hosted |
| **Moonbase** | Store + licensing for audio software, JUCE client module | Revenue share / plans | Audio-focused, handles payments, tax, offline activation, JUCE integration | Newer, smaller vendor; tied to their store |
| **KeyZy** | Licensing for audio plugins with a JUCE SDK | Per-licence / plan pricing | Audio-focused, simple integration | Smaller vendor; less flexibility |
| **Cryptlex** | General licensing SDK (LexActivator) with node-locking, floating, trials | Monthly plans | Mature, many platforms, floating licences for schools | Closed-source native library to ship in the plugin; generic, not audio-aware |
| **Home-grown (this spec)** | Section 2-8 on a small serverless service | Development time (about 1-2 weeks) and small hosting costs | Full control, no fees, no third-party library in the audio plugin | We own security, uptime and support |

**L-10** Recommendation: implement the client exactly as specified here
(Ed25519 via Monocypher, tolerant fingerprints, grace, demo mode), and
use **Keygen.sh** (hosted, or self-hosted Keygen CE later) as the
server, since its data model (licences, machines, signed offline
licence files, Ed25519) matches sections 3-6 closely. Moonbase is the
alternative if the owner also wants the store handled. iLok is not
recommended at launch: its protection is stronger but its cost and
customer friction do not fit a $200 indie product whose free edition
already competes for goodwill; it can be reconsidered if piracy data
justifies it.

## 11. Integration with the existing code

- `Source/Updates/Telemetry.*` holds a placeholder `License` class. It
  accepts any successful HTTP response as an activation and any
  16-character string as an offline response, stores only a key hash and
  dates in plain JSON, and binds nothing to the machine. It is fine as a
  UI placeholder; it provides no protection. At the split it moves into
  its own Pro-only file (`editions.md` 9) and is replaced by:
  - `LicenceFile` (parse, canonical payload, Ed25519 verify, kid lookup),
  - `MachineFingerprint` (the five components, tolerant match),
  - `LicenceClient` (the section 6.1 calls over the existing
    `Transport`, worker thread only),
  - `LicenceState` (the section 8 state machine, exposed to the UI and,
    as one atomic, to the engine for the demo-mode fades).
- The existing UI points stay: the header grace countdown
  (`PluginEditor.cpp`), the HELP tab licence line (`HelpTab.cpp`),
  Options -> Licence. The public shape of `License::State` (unlicensed,
  activated, grace, expired) maps onto section 8's states.
- Tests: signature verification with test keys (valid, tampered payload,
  wrong kid, truncated), fingerprint tolerance (2, 3, 4, 5 of 5
  matching), state machine transitions with a mocked clock, the demo
  fade is click-free and never engages during playback, no licensing
  call on the audio thread (`ThreadProbe`), privacy log entries.

## 12. Open questions for the product owner

- **Q-L1** Vendor: Keygen.sh (recommended), Moonbase, KeyZy, Cryptlex,
  iLok, or self-hosted?
- **Q-L2** Seats per licence: 3 (recommended) or 2?
- **Q-L3** Offline window: 30-day revalidation + 14-day grace
  (`updates-telemetry.md` 5) or longer (60 + 14) for touring musicians?
- **Q-L4** Unlicensed behaviour: silence intervals (recommended), noise
  bursts, or a hard block on load?
- **Q-L5** Trial: 30-day full Pro trial (recommended), or none (Free
  edition as the only try-before-you-buy)?
- **Q-L6** Upgrade policy: free updates within major version 1,
  paid upgrade for v2 (`majorVersions`)?
- **Q-L7** Education / volume licensing and floating licences for
  schools: needed at launch?
- **Q-L8** Which store (Paddle, FastSpring, Stripe, Moonbase) handles
  payments and VAT?
- **Q-L9** Is a customer account (e-mail sign-in) acceptable, or must
  activation work with a key alone?
