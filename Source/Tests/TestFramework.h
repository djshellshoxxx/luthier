#pragma once

/*  A minimal test framework.

    Deliberately tiny: a dependency-free runner is one less thing that can break
    the build, and these tests need to run in CI on three platforms without any
    package management.

    Failures print the file, the line, the expression and - where it helps - the
    actual and expected values, because a DSP test that says only "failed" is
    almost useless.
*/

#include <juce_dsp/juce_dsp.h>
#include <functional>
#include <vector>
#include <algorithm>
#include <ctime>
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>   // GetThreadTimes for threadCpuTimeSeconds()
#endif

namespace luthier::tests
{

//==============================================================================
struct TestContext
{
    juce::String currentTest;
    int checks = 0;
    int failures = 0;
    juce::StringArray failureMessages;

    void fail (const juce::String& message)
    {
        ++failures;
        failureMessages.add ("    " + message);
    }
};

//==============================================================================
class TestRegistry
{
public:
    using TestFn = std::function<void (TestContext&)>;

    struct Entry
    {
        juce::String suite;
        juce::String name;
        TestFn fn;
    };

    static TestRegistry& get()
    {
        static TestRegistry instance;
        return instance;
    }

    void add (const juce::String& suite, const juce::String& name, TestFn fn)
    {
        entries.push_back ({ suite, name, std::move (fn) });
    }

    const std::vector<Entry>& getEntries() const { return entries; }

private:
    std::vector<Entry> entries;
};

//==============================================================================
struct TestRegistrar
{
    TestRegistrar (const char* suite, const char* name, TestRegistry::TestFn fn)
    {
        TestRegistry::get().add (suite, name, std::move (fn));
    }
};

#define LUTHIER_TEST(suite, name)                                                        \
    static void test_##suite##_##name (luthier::tests::TestContext& ctx);                \
    static luthier::tests::TestRegistrar registrar_##suite##_##name                      \
        (#suite, #name, test_##suite##_##name);                                          \
    static void test_##suite##_##name (luthier::tests::TestContext& ctx)

//==============================================================================
#define CHECK(expr)                                                                      \
    do {                                                                                 \
        ++ctx.checks;                                                                    \
        if (! (expr))                                                                    \
            ctx.fail (juce::String ("line ") + juce::String (__LINE__)                   \
                      + ": expected " + #expr);                                          \
    } while (false)

#define CHECK_MSG(expr, message)                                                         \
    do {                                                                                 \
        ++ctx.checks;                                                                    \
        if (! (expr))                                                                    \
            ctx.fail (juce::String ("line ") + juce::String (__LINE__) + ": "            \
                      + juce::String (message));                                         \
    } while (false)

#define CHECK_NEAR(actual, expected, tolerance)                                          \
    do {                                                                                 \
        ++ctx.checks;                                                                    \
        const double a_ = (double) (actual);                                             \
        const double e_ = (double) (expected);                                           \
        const double t_ = (double) (tolerance);                                          \
        if (! (std::abs (a_ - e_) <= t_))                                                \
            ctx.fail (juce::String ("line ") + juce::String (__LINE__) + ": "            \
                      + #actual + " = " + juce::String (a_, 6)                           \
                      + ", expected " + juce::String (e_, 6)                             \
                      + " +/- " + juce::String (t_, 6));                                 \
    } while (false)

#define CHECK_FINITE(buffer, count)                                                      \
    do {                                                                                 \
        ++ctx.checks;                                                                    \
        for (int i_ = 0; i_ < (count); ++i_) {                                           \
            if (! std::isfinite ((buffer)[i_])) {                                        \
                ctx.fail (juce::String ("line ") + juce::String (__LINE__)               \
                          + ": non-finite sample at index " + juce::String (i_));        \
                break;                                                                   \
            }                                                                            \
        }                                                                                \
    } while (false)

//==============================================================================
/** Finds the strongest spectral peak in a signal, in Hz, by parabolic
    interpolation around the largest FFT bin. Accurate to a fraction of a bin,
    which is what pitch verification needs. */
double findPeakFrequency (const double* samples, int numSamples, double sampleRate,
                          double minHz = 20.0, double maxHz = 20000.0);

/** Measures the frequency of the nth spectral peak above `minHz`. Used to check
    partial stretching. */
std::vector<double> findPartials (const double* samples, int numSamples, double sampleRate,
                                  int howMany, double fundamentalHz);

/** RMS of a buffer. */
double rms (const double* samples, int numSamples);

/** Peak absolute value. */
double peak (const double* samples, int numSamples);

/** Time in seconds for a decaying signal to fall by `db` from its peak.
    Returns -1 if it never does. */
double measureDecayTime (const double* samples, int numSamples, double sampleRate, double db);

/** Total harmonic distortion, as a fraction, for a signal known to be a sine at
    `fundamentalHz`. */
double measureThd (const double* samples, int numSamples, double sampleRate, double fundamentalHz);

//==============================================================================
/*  performance-budget.md 1 / 10: CPU "units" are percent of one core at the
    measured audio duration. Measured on the calling thread's CPU clock rather
    than a stopwatch, so another process competing for the core does not count
    against the code (see Modulation.thousandRouteStressTest for the history).
    The best of `repeats`: noise only ever adds. */
inline double threadCpuTimeSeconds() noexcept
{
   #if JUCE_WINDOWS
    // The thread's kernel + user time, in 100 ns ticks.
    FILETIME creation {}, exit {}, kernel {}, user {};

    if (! GetThreadTimes (GetCurrentThread(), &creation, &exit, &kernel, &user))
        return 0.0;

    const auto ticks = [] (const FILETIME& t) { return ((juce::uint64) t.dwHighDateTime << 32) | t.dwLowDateTime; };
    return (double) (ticks (kernel) + ticks (user)) * 1.0e-7;
   #else
    timespec ts {};
    if (clock_gettime (CLOCK_THREAD_CPUTIME_ID, &ts) != 0)
        return 0.0;
    return (double) ts.tv_sec + (double) ts.tv_nsec * 1.0e-9;
   #endif
}

template <typename RenderFn>
double cpuUnits (RenderFn&& render, double audioSeconds, int repeats = 3)
{
    double best = 1.0e30;

    for (int r = 0; r < repeats; ++r)
    {
        const double start = threadCpuTimeSeconds();
        render();
        best = std::min (best, threadCpuTimeSeconds() - start);
    }

    return 100.0 * best / std::max (1.0e-9, audioSeconds);
}

/** LUTHIER_PERF=1: the machine-relative performance tests (CPU units, scaling
    curves, the long memory session) run; otherwise they skip. */
inline bool perfRunRequested()
{
    return juce::SystemStats::getEnvironmentVariable ("LUTHIER_PERF", {}) == "1";
}

} // namespace luthier::tests
