#include "CrashReporting.h"

/*  See CrashReporting.h. Everything of substance is behind LUTHIER_ENABLE_SENTRY,
    which is defined only when the CMake option of the same name is ON. With the
    option OFF (the default) this file compiles to empty no-op functions and pulls
    in no sentry-native headers. */

#if defined(LUTHIER_ENABLE_SENTRY)
 // #include <sentry.h>   // provided by sentry-native when the option is ON
 #include <atomic>
 #ifndef LUTHIER_SENTRY_DSN
  #define LUTHIER_SENTRY_DSN ""   // CMake normally fills this from $SENTRY_DSN
 #endif
#endif

namespace luthier
{

#if defined(LUTHIER_ENABLE_SENTRY)
namespace
{
    std::atomic<bool> gInitialised { false };
}
#endif

void CrashReporting::init ([[maybe_unused]] bool userOptedIn)
{
#if defined(LUTHIER_ENABLE_SENTRY)
    if (! userOptedIn || gInitialised.load())
        return;

    const char* dsn = LUTHIER_SENTRY_DSN;
    if (dsn == nullptr || dsn[0] == '\0')
        return;   // no DSN configured -> stay disabled, never hardcode one

    // --- sentry-native wiring goes here, e.g.:
    //   sentry_options_t* opts = sentry_options_new();
    //   sentry_options_set_dsn (opts, dsn);
    //   sentry_options_set_release (opts, "luthier@" LUTHIER_BUILD_STRING);
    //   sentry_init (opts);
    gInitialised.store (true);
#endif
}

void CrashReporting::shutdown()
{
#if defined(LUTHIER_ENABLE_SENTRY)
    if (! gInitialised.exchange (false))
        return;
    // sentry_close();
#endif
}

bool CrashReporting::isCompiledIn() noexcept
{
#if defined(LUTHIER_ENABLE_SENTRY)
    return true;
#else
    return false;
#endif
}

} // namespace luthier
