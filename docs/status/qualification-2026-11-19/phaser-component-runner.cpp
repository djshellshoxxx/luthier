#include "Tests/TestFramework.h"
#include <iostream>
#include <chrono>
int main(int argc,char** argv)
{
    luthier::tests::TestContext context;
    int count=0;
    const juce::String filter=argc>1?argv[1]:"RefPedalPhase90";
    for(const auto& test:luthier::tests::TestRegistry::get().getEntries())
    {
        context.currentTest=test.suite+"."+test.name;
        if(!context.currentTest.startsWith(filter)) continue;
        const int beforeChecks=context.checks,beforeFailures=context.failures;
        const auto started=std::chrono::steady_clock::now();
        std::cout<<"RUN "<<context.currentTest<<std::endl;
        test.fn(context);++count;
        std::cout<<context.currentTest<<": "<<context.checks-beforeChecks<<" checks, "<<context.failures-beforeFailures
                 <<" failures, "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<" wall seconds"<<std::endl;
    }
    for(const auto& failure:context.failureMessages)std::cerr<<failure<<'\n';
    std::cout<<count<<" tests, "<<context.checks<<" checks, "<<context.failures<<" failures"<<std::endl;
    return count>0&&context.failures==0?0:1;
}
