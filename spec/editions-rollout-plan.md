# EDITIONS + LICENSING ROLLOUT PLAN

Phased implementation plan for the Free/Pro fork (`editions.md`) and the
Pro licensing system (`licensing.md`). This document is the **order of
work**: it does not re-specify the design (the two specs above own that),
it sequences it into shippable phases, each with its files, tests,
acceptance criteria, and the owner actions that gate it.

Status: plan for the coordinator and the product owner. Nothing here is
implemented yet. The design decisions it depends on are `editions.md`
**D-1..D-6** and `licensing.md` **L-1..L-10**; the open questions it is
blocked on are `editions.md` **Q-1..Q-8** and `licensing.md`
**Q-L1..Q-L9**.

## 0. Where this fits and what is already true

`docs/audit/REMAINING.md` records editions + licensing as deferred by
design into their own specs (Editions: 4 items; Licensing: 4 items;
`updates-telemetry` UT-20). This plan discharges those items. Read it
against:

- `editions.md` §7 (how the split is implemented), §9 (the fork step),
  §10 (tests).
- `licensing.md` §8 (unlicensed behaviour), §11 (integration with
  existing code).

### 0.1 Scaffolding already in the tree (build on it, do not re-invent)

The split has **not** happened, but transitional edition seams already
exist and the plan reuses them:

| In tree today | What it is | Fate in this plan |
|---|---|---|
| `Source/Support/Edition.h/.cpp` | A **runtime** process-wide flag (`luthier::Editions::current()`, a relaxed atomic defaulting to `pro`, audio-thread-safe) with `isPro()` and `kProSuffix " (Pro)"`. Used by assist-rules gating in `Parameters.cpp` and by tests that call `Editions::set()`. | Phase 1 reconciles this with the **compile-time** `Source/Edition.h` that `editions.md` §7.2 specifies. See §0.2. |
| `Source/Jam/JamEdition.h` | A **compile-time** edition table (`JamEdition::kIsFree`, `nearestFree(...)`) already wired in `Parameters.cpp` (`if constexpr (JamEdition::kIsFree)`) and `JamPanel.cpp`. | Becomes the first consumer pattern the rest of the Feature table follows. |
| `Source/Updates/Telemetry.{h,cpp}` | The placeholder `License` class (`licensing.md` §11): accepts any HTTP 2xx as activation and any 16-char string as an offline response, binds nothing, stores a key hash + dates. UI-only, no protection. | Phases 2–3 replace it and split it into Pro-only files. |
| assist-rules Free path | `Parameters.cpp` already appends `kProSuffix` and forces the neutral rules value when `edition == free`. | The template for the §7.3 "identical parameter layout, Pro params non-automatable + suffixed in Free" rule. |

No `Source/Edition.h`, `Source/Presets/ProFeatureGuard.*`,
`Source/UI/ProLockedPanel.*`, `cmake/Editions.cmake`, or
`packaging/content/free.txt` exists yet — those are Phase 1 / Phase 4 / Phase 5
deliverables named in `editions.md` §9.

### 0.2 Two edition concepts, kept distinct

The single most important reconciliation this plan makes explicit, because
a runtime flag and a compile-time flag both live in the tree today:

- **Edition = Free vs Pro** is **compile-time** (`editions.md` D-5, rule
  0.5): two binaries, the Free link omitting every Pro `.cpp`
  (`cmake/Editions.cmake` `LUTHIER_PRO_ONLY_REGEX`). The source of truth
  after the fork is `constexpr bool edition::isPro` in `Source/Edition.h`.
- **Licence state = demo vs activated** (`licensing.md` §8) is
  **runtime**, and exists **only inside the Pro binary**. The Free binary
  contains no licensing code at all (`editions.md` 2.5, Q-8).

The existing runtime `Source/Support/Edition.h` atomic is a *pre-fork
simulation seam* only: it lets today's single (Pro) build pretend to be
Free for tests before the compile-time split exists. Phase 1 **D-R1**:
keep the runtime atomic as a test-only override behind the compile-time
default — `Editions::current()` returns `edition::isPro ? pro : free`
unless a test explicitly calls `set()` — so existing call sites and tests
keep compiling, while the compile-time constant drives real builds.
Remove the setter from non-test code. (Do not grow a second runtime
edition concept; `licensing.md` state is the only runtime gate, and it
gates demo-vs-activated, never Free-vs-Pro.)

### 0.3 "Seven premium features" — reconciliation

The brief references "seven premium features". No such list exists in the
specs; the canonical, gating list is the **eight headline groups H1–H8**
in `editions.md` §1 (Workshop, Tune Builder, Techniques tab, Tone Match,
Notation/MIDI export, Advanced ranges, Studio routing, Deep realism
panels). The number "seven" in the product copy refers to the **Techniques
tab's seven sub-tabs** (`docs/PRODUCT_DESCRIPTION.md`: SCRAPE, SLIDE,
SLAP, MUTE, TAP, BEND, CASCADE), which are one headline group (**H3**).
This plan gates on the eight groups H1–H8; see owner action **O-0** to
confirm the count and naming (ties to `editions.md` Q-2).

## 1. Sequencing overview

Six phases, built in order; each merges to the integration branch on its
own before the next starts. Phases 1–4 are per `editions.md` §9 (the fork)
plus `licensing.md` §11; Phases 2–3 are the licensing core; Phases 5–6 are
release and commerce.

```
Phase 1  Gating scaffolding          (editions.md §7, §9)      ── no behaviour change in Pro
Phase 2  Licence verify core         (licensing.md §3,§4,§8)   ── offline, no server
Phase 3  Trial + activation          (licensing.md §5,§6)      ── needs server + store
Phase 4  Edition UI (locks, upsell)  (editions.md §4; lic §11) ── user-visible Free
Phase 5  Packaging two editions      (editions.md §6,§7.6)     ── two artefact sets
Phase 6  Store + upgrade flow        (licensing.md §10; ed §5) ── commerce live
```

Rationale for this order:

1. **Gating first, inert.** Phase 1 lands `Edition.h`, the `Feature`
   enum, `edition::limits`, `ProFeatureGuard`, and the CMake option with
   `LUTHIER_EDITION=PAID` as the only built edition, so the whole tree can
   *ask* about edition without any Free binary existing yet. Pro behaviour
   is byte-identical before and after. This de-risks everything else.
2. **Licence core before activation.** Phase 2 can verify a signed licence
   file (test-key signed) entirely offline with no server, replacing the
   placeholder's "any 2xx is valid" with real Ed25519 verification and the
   §8 state machine + demo mode. It is testable with fixtures alone.
3. **Activation needs infrastructure.** Phase 3 is the first phase blocked
   on an owner action (a Keygen account / server), so it comes after the
   client can already verify.
4. **UI after the model.** The locked-feature UX (Phase 4) consumes the
   `Feature`/`limits` model (Phase 1) and the `LicenceState` (Phase 2).
5. **Packaging after a Free binary can exist.** Phase 5 flips on the
   `LUTHIER_EDITION=FREE` link-exclusion and content manifest, which only
   makes sense once Phases 1 & 4 define what Free omits and shows.
6. **Commerce last.** Phase 6 is store webhooks, pricing, upgrade/crossgrade
   — business wiring that depends on the whole technical chain working.

A single release tag at the end builds **six** installers (Pro + Free ×
win/mac/linux) per `editions.md` §6, §7.6.

---

## Phase 1 — Gating scaffolding (inert in Pro)

**Goal:** the tree can ask `edition::has(Feature::x)` and read
`edition::limits` everywhere, with `PAID` the only edition that builds and
Pro output unchanged. No Free binary, no licensing yet.

**Design refs:** `editions.md` §7.1–§7.5, §9 (files), §0.2 above.

### Files

| File | Action | Content |
|---|---|---|
| `Source/Edition.h` | **new** | `editions.md` §7.2 verbatim: `constexpr bool isPro`, `enum class Feature` (one entry per Pro row of §2), `constexpr bool has(Feature)`, `struct Limits` + `constexpr Limits limits`, `isFreeAmp/isFreePedal`, `productName`, `contentFolder`. |
| `Source/Support/Edition.{h,cpp}` | **change** | Per **D-R1**: `current()` defaults from the compile-time `edition::isPro`; `set()` becomes a test-only override; keep `kProSuffix`. Migrate the assist-rules + Jam call sites to the `edition::has/limits` API where natural, leaving `JamEdition.h` as the per-subsystem table it already is. |
| `cmake/Editions.cmake` | **new** | The §6 identity table as CMake vars + `LUTHIER_PRO_ONLY_REGEX` (seeded empty / Pro-only list stubbed; populated as features land — `editions.md` §9). |
| `CMakeLists.txt` | **change** | Add `LUTHIER_EDITION` cache option (PAID\|FREE), `include(cmake/Editions.cmake)`, `target_compile_definitions(... LUTHIER_PRO=$<BOOL:${LUTHIER_IS_PRO}>)`. PAID path must be identical to today. |
| `Source/Presets/ProFeatureGuard.{h,cpp}` | **new** | Detection + effective-values + banner text (`editions.md` §5.1). In Pro it reports "no Pro features bypassed" (inert). |
| `Source/Tests/EditionTests.cpp` | **new** | Phase-1 subset of `editions.md` §10 (see below). |

### Tests

- Parameter layout is identical with `LUTHIER_EDITION=PAID` before/after
  the change: same IDs, order, ranges (`editions.md` §10.7). A golden
  hash over the parameter tree, diffed in CI.
- `edition::has(f)` returns `true` for all `f` when `isPro`;
  `edition::limits` equals the "full" struct.
- `ProFeatureGuard` on every factory preset in Pro reports zero bypassed
  features and returns the state unchanged (round-trip identity).
- Build matrix still green; `LuthierTests`, pluginval (strictness 10),
  clap-validator unchanged.

### Acceptance criteria

- `cmake -DLUTHIER_EDITION=PAID` produces a binary null-identical in
  audio and parameter schema to the pre-Phase-1 build (−∞ dB null on the
  factory-preset render set).
- Grep shows no call site still using the old runtime `Editions::set()`
  outside `Source/Tests/`.
- `LUTHIER_EDITION=FREE` **configures** (CMake succeeds) but is not yet
  required to build cleanly — Phase 5 owns the Free link. Document it as
  "reserved" in `cmake/Editions.cmake`.

### Owner actions

- **O-0** Confirm the eight headline groups H1–H8 and the Free/Pro split
  of `editions.md` §2 (ties Q-2, Q-3). Nothing user-facing ships this
  phase, but the `Feature` enum encodes the decision.

---

## Phase 2 — Licence verify core (offline, no server)

**Goal:** inside the Pro build, replace the placeholder `License` with
real offline verification and the full unlicensed-behaviour state machine,
using test keys and fixture licences. No network, no server yet.

**Design refs:** `licensing.md` §3 (format + crypto), §4 (fingerprint),
§7 (integrity), §8 (states + demo mode), §9 (privacy), §11 (integration).

### Files

| File | Action | Content |
|---|---|---|
| `ThirdParty/monocypher/` | **new** | Vendored Monocypher (public domain) for Ed25519 verify (`licensing.md` L-3). CMake target, added to `THIRD_PARTY_LICENCES.txt`. |
| `Source/Licence/LicenceFile.{h,cpp}` | **new** | Parse `{"licence","sig"}`, canonical payload, Ed25519 verify against embedded public key(s), `kid` lookup, verify-then-parse order (`licensing.md` §3.2). |
| `Source/Licence/MachineFingerprint.{h,cpp}` | **new** | The five salted-hash components per OS (`licensing.md` §4.1), tolerant 3-of-5 match (§4.2). |
| `Source/Licence/LicenceState.{h,cpp}` | **new** | The §8 state machine (activated / revalidation-due / grace / unlicensed→demo), clock-rollback guard (monotonic last-seen), exposed to UI and as one atomic to the engine for demo fades. |
| `Source/Licence/LicenceStore.{h,cpp}` | **new** | Atomic read/write of `licence.json` (per-user + machine-wide), §4.3. |
| `Source/Updates/Telemetry.{h,cpp}` | **change** | Remove placeholder `License`; keep the UI-facing `License::State` shape mapped onto `LicenceState` (`licensing.md` §11). These files are marked Pro-only for Phase 5's link exclusion. |
| `Source/<engine demo fade>` | **change** | Demo-mode fade (20 ms out/in, 1 s per 60 s) driven by the atomic licence state; evaluated at load/editor-open, never engaging mid-playback (`licensing.md` §8, L-9). RT-safe: no alloc/lock in `processBlock`. |
| `Source/Tests/LicenceTests.cpp` | **new** | See below. |
| `Resources/keys/` or embedded header | **new** | The **public** key(s) only (`kid` → key). Private key never in-repo (`licensing.md` §3.1, T3). |

### Tests (`licensing.md` §11)

- Signature verification with test keys: valid, tampered payload, wrong
  `kid`, truncated signature all behave correctly (only valid passes).
- Fingerprint tolerance: 2/5 fails, 3/5, 4/5, 5/5 pass; drift triggers a
  silent refresh request flag (no crash when offline).
- State-machine transitions with a **mocked clock**: activated →
  revalidation-due → grace → demo; grace window per Q-L3.
- Clock-rollback (T7): a current time >2 days before last-seen is treated
  as revalidation-due, not trusted.
- Demo fade is click-free, never engages during playback, and produces
  finite audio (`ThreadProbe` asserts no licence work on the audio
  thread).
- Privacy: the licence file contents are exactly `licensing.md` §3.2
  fields; no personal data; a `savedBy`-style audit never leaks identity.

### Acceptance criteria

- With a valid fixture licence present, Pro is full-featured, no UI nags.
- With no licence / a tampered licence, Pro enters demo mode: project
  still loads and saves, exports disabled, periodic fade, non-modal
  "Activate" note — and audio **never stops mid-session**.
- `ThreadProbe` shows zero licence/verify calls on the audio thread
  (`licensing.md` §7.6, `engine.md` 0).

### Owner actions

- **O-1** Approve Ed25519 + Monocypher (L-2, L-3) and the demo-mode
  behaviour (Q-L4: silence intervals recommended).
- **O-2** Decide where the **private** signing key lives now (cloud KMS
  vs the vendor of Phase 3). Only the public key is needed to finish
  Phase 2; the private key is needed to *sign* fixtures — a dev-only test
  keypair is fine for this phase and MUST NOT be a production key.

---

## Phase 3 — Trial + activation (server + store)

**Goal:** the Pro client talks to a real licence server: online
activation, revalidation, deactivation, offline (air-gapped) activation,
and a 30-day full-Pro trial.

**Design refs:** `licensing.md` §5 (activation/seats/deactivation), §6
(server + trial), §3.4 (licence keys), §9 (privacy).

### Files

| File | Action | Content |
|---|---|---|
| `Source/Licence/LicenceClient.{h,cpp}` | **new** | The §6.1 calls (`/v1/activate`, `/revalidate`, `/deactivate`, offline response verify) over the existing `Source/Updates` `Transport`, worker thread only, logged in the privacy network log. |
| `Source/Licence/OfflineActivation.{h,cpp}` | **new** | Challenge encode (base-32 + QR + save-file) and response verify (`licensing.md` §6.4). Replaces the placeholder's "any 16-char string". |
| `Source/Licence/DeactivationQueue.{h,cpp}` | **new** | Delete-local-immediately + retry-on-network semantics (`licensing.md` §5). |
| `Source/Licence/LicenceState.cpp` | **change** | Wire revalidation-due → background retry; trial issuance/expiry. |
| `Source/Tests/LicenceClientTests.cpp` | **new** | Mocked `Transport`: activate, seats_exhausted, invalid_key, revoked, signed-error handling (T6), revalidation, deactivation queue drain, trial one-per-fingerprint. |

### Tests

- Mocked-server activation returns a signed licence that the Phase-2 core
  accepts; `seats_exhausted`/`invalid_key`/`revoked` surface as the right
  `LicenceState` with no crash.
- Signed error responses (T6): an unsigned "revoked" is rejected.
- Deactivation deletes the local file even when the network fails, and
  the queued request drains on reconnect.
- Offline challenge/response round-trips a fixture licence end-to-end.
- Trial: one trial per fingerprint set, runs full Pro, expires to demo.
- All server calls appear in the `updates-telemetry.md` network log with
  category "licence"; none on the audio thread.

### Acceptance criteria

- Against a staging server (or Keygen sandbox), a real key activates,
  revalidates after the window, and deactivates; a second machine beyond
  the seat limit is refused with a clear message.
- Air-gapped activation works with no network on the plugin machine.

### Owner actions (gating)

- **O-3** Choose the licence vendor/server (Q-L1: **Keygen.sh**
  recommended) and create the account; provision the production KMS/HSM
  Ed25519 key (L-4) — **GitHub secret**, never in-repo.
- **O-4** Seats (Q-L2: 3), offline window + grace (Q-L3), trial length
  (Q-L5: 30-day full Pro), account-vs-key activation (Q-L9).
- **O-5** Server endpoints (`licensing.md` §6.1) stood up (vendor-hosted
  or the small serverless service of §6.2) with the store webhook stub.

---

## Phase 4 — Edition UI: locks, upsell, banners

**Goal:** the user-visible Free experience — locked tabs/panels/pills with
the single upsell panel, the "N Pro features bypassed" preset banner, the
licence/activation screen, trial countdown.

**Design refs:** `editions.md` §4 (locked-feature UX), §5.1 (preset
banner), §7.4 (UI), §9 (files); `licensing.md` §11 (UI points).

### Files

| File | Action | Content |
|---|---|---|
| `Source/UI/ProLockedPanel.{h,cpp}` | **new** | Lock glyph drawing (4.1) + the one non-modal upsell side panel (4.2): title, two sentences, 20 s looping preview, "Learn more", "Close". |
| `Source/UI/<panel factories>` | **change** | `if constexpr`-selected `ProLockedPanel` substitution for each Pro panel (Workshop, Tune, Tone Match, Techniques, …); lists (racks, pedals, amps, snapshots, mod routes) built from `edition::limits` — the Pro lists filtered, never a second copy (§7.4). |
| `Source/UI/<preset load path>` | **change** | `ProFeatureGuard` banner (info priority, `error-recovery.md`), "Don't show again for this preset" persisted in `config/plugin.json` (§4.5). |
| `Source/UI/LicencePanel.{h,cpp}` | **new / from placeholder** | Options → Licence: key entry, machine list, deactivate, offline activation, trial countdown (`licensing.md` §5, §11). Pro-only. |
| `Source/PluginEditor.cpp`, `Source/UI/HelpTab.cpp` | **change** | Header grace/trial countdown; HELP licence line map onto `LicenceState` (`licensing.md` §11). |
| `Source/PluginEditorOnboarding.cpp` | **change** | Free "What Luthier Pro adds" end-of-tour page, once, skippable (`editions.md` §4.4). |
| `Resources/Demos/` + `scripts/render_demos.sh` | **new** | Pre-rendered locked-preset audio demos + upsell images (`editions.md` §4.3), generated at release time by `luthier-render`. |
| `Source/Tests/EditionUiTests.cpp` | **new** | `editions.md` §10.6. |

### Tests (`editions.md` §10.6)

- Every locked control has the tooltip "Available in Luthier Pro", the
  accessible description "locked, available in Luthier Pro", and opens the
  upsell panel.
- Nothing opens the upsell unprompted during a 10-minute soak with the
  transport running; nothing on a timer; nothing modal.
- The preset banner lists the correct bypassed features and persists the
  per-preset dismissal.
- Accessibility parity: screen reader, keyboard nav, palettes identical in
  both editions (`editions.md` rule 0.3) — a11y tests pass in Free config.

### Acceptance criteria

- Running the Pro binary with the runtime Free simulation seam (D-R1)
  shows the complete locked UX without a separate build yet.
- Licence panel performs activate/deactivate/offline against the Phase-3
  staging server.

### Owner actions

- **O-6** Upsell copy + the product web page URL ("Learn more"); demo
  audio size budget (Q-6, ~6 MB); trial/upsell telemetry opt-in (Q-7).

---

## Phase 5 — Packaging two editions

**Goal:** actually build, validate, and package **both** editions — the
Free link omits every Pro `.cpp`, ships the Free content manifest, and
carries the Free identity; CI builds a 2×3 matrix.

**Design refs:** `editions.md` §3 (content), §6 (identity), §7.1/§7.6
(build/CI), §9 (files), §10 (tests).

### Files

| File | Action | Content |
|---|---|---|
| `cmake/Editions.cmake` | **change** | Populate `LUTHIER_PRO_ONLY_REGEX` with the real Pro-only `.cpp` set (`editions.md` §9 list: `Source/Workshop/*`, Tune editor `.cpp`, `Source/ToneMatch/*`, `Source/Export/*`, `Source/Notation/NotationExport.cpp`, `Source/Presets/PresetMorph.cpp`, the technique panels/engines, `Source/Licence/*` + the Pro-only Telemetry split, and the `LuthierRender` target). Re-derive from §2 at fork time. |
| `packaging/content/free.txt` | **new** | The Free content manifest (`editions.md` §3): 6 guitars, 16 presets, 8 kits, ~130 IRs, 6 example tunes (playback), 6 backing tracks, 8 MIDI clips. |
| `scripts/ci_build.sh`, `scripts/ci_build.ps1` | **change** | Accept `EDITION`/`-Edition`, pass `-DLUTHIER_EDITION`, read product name for artefact paths (today they assume `Luthier.vst3`); `stage` copies `free.txt` for FREE, all of `Resources/` for PAID (§7.6). |
| `.github/workflows/build.yml` | **change** | Add the `edition: [PAID, FREE]` matrix axis to the existing platform matrix (today it has no edition axis); upload six artefact sets; run pluginval + clap-validator on both editions; run the Pro-preset guard test on Free. |
| `packaging/windows/*.iss`, `packaging/macos/*`, `packaging/linux/*` | **change** | Per-edition names, IDs, AppId (new GUID for Free), content folder (`editions.md` §6). Pro keeps today's codes/IDs (D-4). |
| `Source/Tests/EditionTests.cpp` | **change** | Add §10.2 (Free binary contains no Pro symbols — CI greps Free binaries for Pro class names + licence strings), §10.3–§10.5. |
| `docs/EDITIONS.md`, `docs/RELEASING.md` | **change/new** | User-facing comparison table generated from §2; release steps for six installers. |

### Tests (`editions.md` §10)

- §10.1 Both editions build and pass `LuthierTests`, pluginval (10),
  clap-validator.
- §10.2 Free binaries contain **none** of the Pro-only symbols or licence
  strings (CI symbol grep) — proves rule 0.5 and that no licensing code is
  in Free.
- §10.3 Every Pro factory preset loads in Free: no assertion, finite
  audio, correct banner, round-trips Pro→Free→Pro identical except
  `savedBy`.
- §10.4 Every Free preset renders within −90 dBFS null of Pro (same
  engine).
- §10.5 Both editions install side by side on each OS; a host scan lists
  both; both load in one session (D-3, rule 0.6).

### Acceptance criteria

- A single release tag yields six installers (Pro+Free × win/mac/linux).
- The Free binary is ~40 % of the Pro installer (§3) and demonstrably
  carries no Pro/licensing code (§10.2).
- Shared user-data folder (`~/Documents/Luthier`) works from both; a Free
  preset opens unchanged in Pro (rule 0.7, upgrade path).

### Owner actions

- **O-7** Confirm product names (Q-1), the Free guitar/amp lists (Q-3),
  and whether Free plays Pro-built `.luthierguitar` (Q-4, recommended
  yes). Allocate the new Windows AppId GUID + code-signing certs for both
  editions (**GitHub secrets**).

---

## Phase 6 — Store + upgrade flow

**Goal:** commerce is live — the store creates licence keys via webhook,
customers buy/trial/upgrade, and the self-service seat portal works.

**Design refs:** `licensing.md` §5 (portal, revocation), §6 (server
webhook), §10 (vendor), §12; `editions.md` §5.4 (host sessions), Q-5
(upgrade/crossgrade).

### Work (mostly server/business, not plugin code)

| Item | Where | Notes |
|---|---|---|
| Store → licence-server webhook | vendor / serverless (`licensing.md` §6.2) | Purchase creates a key; refund/chargeback revokes (T8, §5). |
| Self-service portal | web | Machine list by chosen name, rate-limited seat release (§5). |
| Revocation list delivery | server | Takes effect at next revalidation (§5, §6.3). |
| Pricing / VAT | store (Paddle/FastSpring/Stripe/Moonbase) | Q-L8. |
| Trial funnel | store + `/v1/activate` trial | Q-L5. |
| Upgrade / crossgrade for Free users | store + `majorVersions` | Q-5, Q-L6. |
| Enterprise / education / on-prem relay | server URL policy (`updates-telemetry.md` 7) | Q-L7. |

### Tests / verification

- End-to-end: a real purchase in the (sandbox) store issues a key that
  activates Pro; a refund revokes it and demo mode returns at the next
  revalidation.
- Seat portal releases a dead machine's seat; rate limit blocks abuse.
- `majorVersions` gate: a v1 licence runs v1 Pro; a v2 build without a v2
  entitlement enters demo (upgrade path exercised).

### Acceptance criteria

- A customer can buy, receive a key, activate within the seat limit,
  move a seat themselves, and (for refunds) be revoked — with no manual
  vendor intervention.

### Owner actions (gating, business)

- **O-8** Store choice + pricing + VAT (Q-L8, no price in the plugin).
- **O-9** Upgrade/crossgrade policy for Free→Pro and v1→v2 (Q-5, Q-L6).
- **O-10** Education/volume/floating licensing at launch? (Q-L7).

---

## 2. Owner action items (consolidated)

Blocking actions, by the phase they gate. Items marked **secret** are
credentials that live in the provider + **GitHub secrets**, never in the
repo (`licensing.md` §3.1: the binary embeds only a **public** key).

| # | Decision / action | Gates | Spec Q |
|---|---|---|---|
| O-0 | Confirm H1–H8 split + "eight headline, not seven" | 1 | ed Q-2 |
| O-1 | Approve Ed25519/Monocypher + demo-mode behaviour | 2 | lic Q-L4 |
| O-2 | Dev test keypair now; decide prod key home | 2→3 | lic L-4 |
| O-3 | **Vendor = Keygen.sh**; prod KMS signing key (**secret**) | 3 | lic Q-L1 |
| O-4 | Seats / offline window / trial length / account-vs-key | 3 | lic Q-L2,3,5,9 |
| O-5 | Stand up server endpoints + store webhook | 3 | lic §6 |
| O-6 | Upsell copy + web URL; demo size; upsell telemetry | 4 | ed Q-6,7 |
| O-7 | Product names; Free guitar/amp lists; AppId + signing certs (**secret**) | 5 | ed Q-1,3,4 |
| O-8 | Store + pricing + VAT | 6 | lic Q-L8 |
| O-9 | Upgrade/crossgrade (Free→Pro, v1→v2) | 6 | ed Q-5, lic Q-L6 |
| O-10 | Education / volume / floating licences | 6 | lic Q-L7 |

## 3. Cross-cutting invariants (hold in every phase)

1. **Audio never stops mid-session** for licensing, and no licence/edition
   check runs on the audio thread (`licensing.md` §7.6, §8; `editions.md`
   §7.5; `engine.md` 0).
2. **No secret ships.** Only the public key is embedded; private keys and
   store tokens live in the provider + GitHub secrets (`licensing.md`
   §3.1).
3. **User data is never broken.** Free loads any Pro file, bypasses what
   it cannot do transparently, writes Pro data back verbatim with an added
   `savedBy` (`editions.md` rule 0.4, §5).
4. **Accessibility/safety/privacy never gated** (`editions.md` rule 0.3).
5. **One schema, one tree.** Parameter layout identical across editions;
   one codebase with a CMake option, no long-lived per-edition branch
   (`editions.md` D-5, D-6).
6. **Every new feature declares its edition** in its spec header and adds
   its §2 row; a feature without a row does not merge (`editions.md` §8).
