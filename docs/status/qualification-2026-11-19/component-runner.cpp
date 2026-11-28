#include "Tests/TestFramework.h"
#include "Tests/TestSelection.h"
#include <chrono>
#include <iostream>

int main(int argc, char** argv)
{
    std::vector<std::string> arguments;
    for (int i=1;i<argc;++i) arguments.emplace_back(argv[i]);
    const luthier::tests::TestSelection selection(arguments);
    if (!selection.valid) return 2;
    int count=0, checks=0, failures=0, ordinal=0;
    for (const auto& test : luthier::tests::TestRegistry::get().getEntries())
    {
        if (!selection.includes(test.suite.toStdString(), test.name.toStdString()) || !selection.inShard(ordinal++)) continue;
        std::cout << test.suite << "." << test.name << std::endl;
        if (selection.listOnly) { ++count; continue; }
        luthier::tests::TestContext ctx;
        ctx.currentTest=test.suite+"."+test.name;
        const auto start=std::chrono::steady_clock::now();
        try { test.fn(ctx); }
        catch(const std::exception& e) { ctx.fail(juce::String("exception: ")+e.what()); }
        catch(...) { ctx.fail("unknown exception"); }
        const double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        for(const auto& message:ctx.failureMessages) std::cerr << message << std::endl;
        std::cout << (ctx.failures?"[FAIL]":"[pass]") << " checks=" << ctx.checks << " failures=" << ctx.failures << " wall_seconds=" << elapsed << std::endl;
        ++count; checks+=ctx.checks; failures+=ctx.failures;
    }
    std::cout << "TOTAL tests=" << count << " checks=" << checks << " failures=" << failures << std::endl;
    return count>0 && failures==0 ? 0 : 1;
}
