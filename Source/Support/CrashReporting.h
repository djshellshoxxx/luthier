#pragma once

/*  CrashReporting: optional, opt-in crash reporting via sentry-native.

    OFF by default. This is a scaffold with two independent gates, BOTH of which
    must be satisfied before a single crash is ever sent:

      1. Build gate    - the CMake option LUTHIER_ENABLE_SENTRY (default OFF).
                         When OFF, LUTHIER_ENABLE_SENTRY is not defined and every
                         function here compiles to a no-op; sentry-native is not
                         linked and no header from it is included. The normal
                         build is completely unaffected.

      2. Runtime gate  - the user must explicitly opt in. init() does nothing
                         unless userOptedIn is true, so even a build with the
                         option ON reports nothing until the user consents.

    The DSN is never hardcoded. When the build option is ON it is read from the
    LUTHIER_SENTRY_DSN compile definition, which CMake fills from the SENTRY_DSN
    environment variable / cache entry at configure time (see CMakeLists.txt and
    docs/infra/SETTINGS_CHECKLIST.md). An empty DSN also disables reporting.

    To wire sentry-native fully: install/vendor sentry-native, turn the option
    ON with -DLUTHIER_ENABLE_SENTRY=ON -DSENTRY_DSN=..., and fill in the two
    #ifdef LUTHIER_ENABLE_SENTRY blocks in CrashReporting.cpp with the actual
    sentry_init / sentry_close calls.
*/

namespace luthier
{

class CrashReporting
{
public:
    /*  Arms crash reporting. No-op unless the build option is ON, the user has
        opted in, AND a non-empty DSN was configured. Safe to call when any of
        those is false. Not called anywhere by default. */
    static void init (bool userOptedIn);

    /*  Flushes and shuts down the crash reporter. No-op when never initialised. */
    static void shutdown();

    /*  Whether this build was compiled with sentry support (the CMake option).
        Runtime opt-in and a DSN are still required for reporting to happen. */
    static bool isCompiledIn() noexcept;
};

} // namespace luthier
