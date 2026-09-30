# Crash reporting (sentry-native) — opt-in, OFF by default

Luthier ships with an **optional** crash-reporting scaffold built on
[sentry-native](https://github.com/getsentry/sentry-native). It is **disabled by
default** and reports nothing unless two independent gates are both satisfied.

## The two gates

1. **Build gate — CMake option `LUTHIER_ENABLE_SENTRY` (default `OFF`).**
   When OFF, `Source/Support/CrashReporting.{h,cpp}` compiles to no-ops, no
   sentry-native header is included, and nothing is linked. The normal build is
   unaffected.

2. **Runtime gate — user opt-in.** `luthier::CrashReporting::init(userOptedIn)`
   does nothing unless `userOptedIn` is `true`. Even a build with the option ON
   stays silent until the user consents.

## The DSN is never hardcoded

The Sentry DSN is read from the `SENTRY_DSN` environment variable / CMake cache
entry **at configure time, only when the option is ON**:

```sh
cmake -B build -G Ninja \
      -DLUTHIER_ENABLE_SENTRY=ON \
      -DSENTRY_DSN="$SENTRY_DSN"
```

CMake passes it through to the code as the `LUTHIER_SENTRY_DSN` compile
definition on the `Luthier` target. An empty DSN keeps reporting disabled. The
repository secrets `SENTRY_DSN` and `SENTRY_AUTH_TOKEN` already exist for CI; the
DSN must never be committed to source.

## Finishing the wiring

The scaffold intentionally stops short of calling into sentry-native. To finish:

1. Vendor or install sentry-native.
2. In `CMakeLists.txt`, inside the `if (LUTHIER_ENABLE_SENTRY)` block, uncomment
   the `find_package(sentry ...)` / `target_link_libraries(... sentry::sentry)`
   lines.
3. In `Source/Support/CrashReporting.cpp`, fill the `#ifdef LUTHIER_ENABLE_SENTRY`
   blocks with the real `sentry_init` / `sentry_close` calls and `#include <sentry.h>`.
4. Call `CrashReporting::init(userConsent)` once at startup and
   `CrashReporting::shutdown()` at teardown.

Uploading debug symbols with `sentry-cli` uses the `SENTRY_AUTH_TOKEN` secret.
