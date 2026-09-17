/*  The test runner.

    Runs every registered test, prints a report, and returns a non-zero exit code
    if anything failed - which is what CI needs.

        LuthierTests              run everything
        LuthierTests Tuning       run only suites whose name contains "Tuning"
        LuthierTests --list       list the tests without running them
*/

#include "TestFramework.h"
#include <juce_events/juce_events.h>

using namespace luthier::tests;

namespace
{
    juce::String pad (const juce::String& s, int width)
    {
        return s.paddedRight (' ', width);
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::StringArray filters;
    bool listOnly = false;

    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg (argv[i]);

        if (arg == "--list" || arg == "-l")
            listOnly = true;
        else if (! arg.startsWith ("-"))
            filters.add (arg);
    }

    const auto& entries = TestRegistry::get().getEntries();

    if (listOnly)
    {
        for (const auto& entry : entries)
            std::cout << entry.suite << " :: " << entry.name << std::endl;

        std::cout << entries.size() << " tests" << std::endl;
        return 0;
    }

    std::cout << "\n"
              << "================================================================\n"
              << " LUTHIER TEST SUITE\n"
              << "================================================================\n"
              << std::endl;

    int totalTests = 0;
    int failedTests = 0;
    int totalChecks = 0;
    int totalFailures = 0;

    juce::String currentSuite;

    const auto startTime = juce::Time::getMillisecondCounterHiRes();

    for (const auto& entry : entries)
    {
        if (filters.size() > 0)
        {
            bool matches = false;

            for (const auto& filter : filters)
                if (entry.suite.containsIgnoreCase (filter) || entry.name.containsIgnoreCase (filter))
                    matches = true;

            if (! matches)
                continue;
        }

        if (entry.suite != currentSuite)
        {
            currentSuite = entry.suite;
            std::cout << "\n" << currentSuite << std::endl;
            std::cout << juce::String::repeatedString ("-", 64) << std::endl;
        }

        TestContext ctx;
        ctx.currentTest = entry.name;

        const auto testStart = juce::Time::getMillisecondCounterHiRes();

        try
        {
            entry.fn (ctx);
        }
        catch (const std::exception& e)
        {
            ctx.fail (juce::String ("threw an exception: ") + e.what());
        }
        catch (...)
        {
            ctx.fail ("threw an unknown exception");
        }

        const auto elapsed = juce::Time::getMillisecondCounterHiRes() - testStart;

        ++totalTests;
        totalChecks += ctx.checks;
        totalFailures += ctx.failures;

        const bool passed = (ctx.failures == 0);

        if (! passed)
            ++failedTests;

        std::cout << "  " << (passed ? "[pass]" : "[FAIL]") << "  "
                  << pad (entry.name, 44)
                  << pad (juce::String (ctx.checks) + " checks", 14)
                  << juce::String (elapsed, 0) << " ms"
                  << std::endl;

        for (const auto& message : ctx.failureMessages)
            std::cout << message << std::endl;
    }

    const auto totalElapsed = juce::Time::getMillisecondCounterHiRes() - startTime;

    std::cout << "\n"
              << "================================================================\n";

    if (failedTests == 0)
    {
        std::cout << " ALL PASSED\n"
                  << " " << totalTests << " tests, " << totalChecks << " checks, "
                  << juce::String (totalElapsed / 1000.0, 1) << " s" << std::endl;
    }
    else
    {
        std::cout << " FAILED\n"
                  << " " << failedTests << " of " << totalTests << " tests failed ("
                  << totalFailures << " of " << totalChecks << " checks), "
                  << juce::String (totalElapsed / 1000.0, 1) << " s" << std::endl;
    }

    std::cout << "================================================================\n" << std::endl;

    return failedTests == 0 ? 0 : 1;
}
