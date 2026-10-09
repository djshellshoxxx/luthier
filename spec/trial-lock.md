# TRIAL LOCK: the 60-day Pro trial with an unlock code

Owner decision 2026-10-09: the next version locks the Pro features after a
60-day trial. Entering an unlock code re-ups a fresh 60-day window, and may be
repeated. This is the **beta-grade** gate: simple, offline, no server. It is
deliberately *not* the full signed-licence anti-piracy system designed in
`spec/licensing.md` (Ed25519 + machine fingerprint + activation server), which
remains the future "real" path and is partly built but unwired under
`Source/Licence/`. This document specifies only what ships now.

## 0. Goals

1. **Never hurt a paying customer.** An activated (paid) licence is *never*
   governed by the trial clock — the 60-day lock only ever touches an
   unlicensed Pro build. (Carried over from `licensing.md` goal 1.)
2. After 60 days with no valid unlock, **the Pro features lock**: the plugin
   keeps running and projects still load and save, but it behaves as the Free
   edition until the trial is re-upped.
3. **The owner is never stranded.** There is always a working unlock path (the
   code), so a legitimate owner or tester can keep any build unlocked.
4. Winding the system clock back must not extend the trial.

## 1. States

The live `luthier::License` class (`Source/Updates/Telemetry.*`, Pro-only) owns
the state. Its `State` enum gains `trial`:

| State | Condition | Pro features |
|---|---|---|
| `activated` | a paid licence, inside the revalidation window | unlocked |
| `grace` | a paid licence, inside the 14-day post-revalidation grace | unlocked |
| `trial` | unlicensed, `effectiveNow < trialExpiresAt` | unlocked |
| `expired` | unlicensed trial has ended (or a paid licence past grace) | **locked → Free** |
| `unlicensed` | no licence and no trial started yet | locked → Free |

`proFeaturesUnlocked()` is true for `activated`, `grace` and `trial`.

## 2. The trial clock

- First run of an unlicensed Pro build: `startTrialIfNeeded()` sets
  `trialStartedAt = now`, `trialExpiresAt = now + 60 days`, persists them.
- `getTrialDaysLeft()` reports whole days remaining (rounded up, so day one of a
  fresh trial reads "60 days").
- Persisted in the existing licence store
  (`userApplicationDataDirectory/Luthier/license.json`) alongside the paid-licence
  fields. An older file with no trial fields upgrades cleanly: the missing fields
  read as zero, so the next load starts a trial.

### Clock-rollback guard (threat T7)

`lastSeen` is a monotonic wall-clock marker that only ever moves forward. Every
expiry decision uses `effectiveNow = max(now, lastSeen)`. Setting the system
clock back therefore cannot revive or extend a trial — `effectiveNow` stays at
the latest time the machine has ever seen.

## 3. The unlock code

- Entering the correct code calls `enterUnlockCode(code)`, which sets
  `trialExpiresAt = effectiveNow + 60 days` and persists. Repeatable any number
  of times ("on repeat"): each correct entry starts a fresh 60-day window from
  the moment it is entered.
- The code is checked against a **salted SHA-256 digest** embedded in the binary;
  the plaintext never appears in the source tree or in the shipped binary's
  strings. Salt: `luthier-trial-unlock-v1`. A casual `strings` dump reveals only
  the digest, not the word.
- This is obfuscation, not cryptographic protection: the digest is a shared
  secret and anyone who reverse-engineers the binary (or clears the app-data
  folder, which resets the trial) can bypass it. That is the accepted limit of
  the beta-grade gate (`spec/licensing.md` threat T5 — made a little costlier,
  not prevented). The signed-licence system is what raises that bar later.
- Tests substitute a known digest via `setUnlockHashForTesting()` so the real
  code is never written into test source either.

## 4. Enforcement — "degrade to Free"

A single runtime authority decides the live edition:

- `luthier::Editions` (`Source/Support/Edition.*`) gains `setProUnlocked(bool)` /
  `isProUnlocked()`. `Editions::current()` returns Free when the build is Pro but
  `proUnlocked` is false (and no test override is set). It defaults to **true**,
  so nothing changes until the processor drives it from the licence state — a
  fresh trial is active, so a new install behaves exactly as before.
- `LuthierAudioProcessor` drives it: on load it calls `license.load()`,
  `startTrialIfNeeded()`, then `Editions::setProUnlocked(license.proFeaturesUnlocked())`.
  `tryUnlockProTrial(code)` enters a code and refreshes the authority.

### What honours the authority

- **Increment 1 (this change):** the runtime authority, the trial/unlock engine,
  persistence, the rollback guard, and the processor wiring — with full unit
  tests. No existing behaviour changes, because a fresh trial keeps Pro unlocked.
- **Increment 2 (next, verified against a green build):** make the lock visibly
  and audibly bite —
  - `EditionLocks` consults the runtime edition, so the Pro guitars/amps/pedals
    disable in their combo boxes when locked;
  - the engine's `effectiveGuitarIndex/AmpIndex/PedalIndex` substitution runs at
    runtime, so a stored Pro choice *plays* as its nearest Free model when locked;
  - the Pro-only feature tabs/exports gate on `proFeaturesUnlocked()`;
  - the UI surfaces the trial countdown and an unlock field (Options → Licence /
    Help), and a non-modal "Trial ended — enter your unlock code" banner.

## 5. Tests (`Source/Tests/TrialLockTests.cpp`)

- a fresh unlicensed Pro build starts a 60-day trial (`trial`, ~60 days left);
- an expired trial reports `expired` and `! proFeaturesUnlocked()`;
- the runtime authority: `Editions::current()` is Free when `proUnlocked` is
  false and Pro when true (restored after each case so sibling tests are unaffected);
- a wrong unlock code is rejected; a correct one (a test digest) re-ups 60 days
  and returns to `trial`;
- the rollback guard: a `lastSeen` in the future keeps an otherwise-live trial
  `expired` — the clock cannot be wound back to revive it;
- a paid (activated) licence is unaffected by the trial clock.
