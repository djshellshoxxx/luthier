#include "Tests/TestFramework.h"
#include <iostream>

int main()
{
    luthier::tests::TestContext context;
    int count = 0;
    for (const auto& test : luthier::tests::TestRegistry::get().getEntries())
    {
        context.currentTest = test.suite + "." + test.name;
        std::cout << context.currentTest << std::endl;
        test.fn (context);
        ++count;
    }
    for (const auto& failure : context.failureMessages)
        std::cerr << failure << '\n';
    std::cout << count << " tests, " << context.checks << " checks, "
              << context.failures << " failures\n";
    return count > 0 && context.failures == 0 ? 0 : 1;
}
