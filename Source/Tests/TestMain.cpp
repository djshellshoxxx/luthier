/*  The test runner.

    Runs every registered test, prints a report, and returns a non-zero exit code
    if anything failed - which is what CI needs.

        LuthierTests              run everything
        LuthierTests Tuning       run only suites whose name contains "Tuning"
        LuthierTests --list       list the tests without running them
        LuthierTests --skip=Combo exclude an exact suite name (case-insensitive)
        LuthierTests --shard=2/4  run the 2nd of 4 interleaved slices of the selection
*/

#include "TestFramework.h"
#include "TestSelection.h"
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <typeinfo>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#if JUCE_MAC
 #include <CoreFoundation/CoreFoundation.h>
#else
 namespace juce::detail { bool dispatchNextMessageOnSystemQueue (bool returnIfNoPendingMessages); }
#endif
#include "../UI/FirstRun.h"
#include "../UI/Onboarding.h"
#include "../Support/QualityController.h"

using namespace luthier::tests;

namespace
{
    juce::String pad (const juce::String& s, int width)
    {
        return s.paddedRight (' ', width);
    }

    // Test isolation: an asynchronous popup (menu, call-out, message box) a test
    // opened and never closed stays modal and would steal keys and modal checks
    // from whichever test runs next, so results would depend on test order and
    // on the --shard slice. Report the leak against the test that caused it,
    // then close it so the next test starts from a clean UI state.
    void closeLeftoverModals (const juce::String& testName)
    {
        auto* modal = juce::ModalComponentManager::getInstanceWithoutCreating();
        const int count = modal != nullptr ? modal->getNumModalComponents() : 0;
        if (count == 0)
            return;

        juce::StringArray kinds;
        for (int i = 0; i < count; ++i)
            if (auto* component = modal->getModalComponent (i))
                kinds.add (typeid (*component).name());

        std::cout << "  [note]  " << testName << " left " << count << " modal component(s) open"
                  << (kinds.isEmpty() ? juce::String() : " (" + kinds.joinIntoString (", ") + ")")
                  << "; closed before the next test" << std::endl;
        juce::PopupMenu::dismissAllActiveMenus();
        if (modal != nullptr)
            modal->cancelAllModalComponents();
    }

    // Test isolation: deliver what a test left in JUCE's message queue before
    // the next test starts. Otherwise a later test inherits the backlog (in
    // the --shard order a light UI test waited 2 s behind it on macOS, which
    // delivers a few messages per run-loop pass). A marker posted now runs
    // after everything already queued; the deadline only bounds a test that
    // keeps re-posting forever, and is reported against that test.
    void drainMessageQueue (const juce::String& testName)
    {
        const auto delivered = std::make_shared<std::atomic<bool>> (false);
        if (! juce::MessageManager::callAsync ([delivered] { delivered->store (true, std::memory_order_release); }))
            return;

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (30);
        while (! delivered->load (std::memory_order_acquire))
        {
            if (std::chrono::steady_clock::now() >= deadline)
            {
                std::cout << "  [note]  " << testName << " left messages that did not drain in 30 s" << std::endl;
                return;
            }
           #if JUCE_MAC
            const bool dispatched = CFRunLoopRunInMode (kCFRunLoopDefaultMode, 0, true) == kCFRunLoopRunHandledSource;
           #else
            const bool dispatched = juce::detail::dispatchNextMessageOnSystemQueue (true);
           #endif
            if (! dispatched)
                std::this_thread::yield();
        }
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // onboarding.md 5 (TUNE-HELP-ONBOARDING): an editor built by any test must not
    // apply this machine's OS preferences to the real settings files mid-run.
    luthier::FirstRun::setStateForTesting (true, false);
    luthier::Onboarding::setAutomaticForTesting (false);

    // cpu-quality-modes 7 (superseding performance-budget.md 8): a busy test
    // machine must not trigger the governor (dropped strings, a frozen
    // audition) inside unrelated tests.
    luthier::QualityController::setGovernorEnabledGlobally (false);

    std::vector<std::string> arguments;
    for (int i = 1; i < argc; ++i)
        arguments.emplace_back (argv[i]);
    const TestSelection selection (arguments);
    if (! selection.valid)
    {
        std::cerr << "Invalid arguments: --shard expects i/n with 1 <= i <= n" << std::endl;
        return 2;
    }

    const auto& entries = TestRegistry::get().getEntries();

    if (selection.listOnly)
    {
        int listed = 0;
        int ordinal = 0;
        for (const auto& entry : entries)
            if (selection.includes (entry.suite.toStdString(), entry.name.toStdString())
                && selection.inShard (ordinal++))
            {
                std::cout << entry.suite << " :: " << entry.name << std::endl;
                ++listed;
            }

        std::cout << listed << " tests" << std::endl;
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

    int selectedOrdinal = 0;
    for (const auto& entry : entries)
    {
        if (! selection.includes (entry.suite.toStdString(), entry.name.toStdString()))
            continue;
        if (! selection.inShard (selectedOrdinal++))
            continue;

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
        closeLeftoverModals (entry.suite + "." + entry.name);
        drainMessageQueue (entry.suite + "." + entry.name);

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
