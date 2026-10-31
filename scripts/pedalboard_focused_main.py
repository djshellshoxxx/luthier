"""Generate a headless runner using the product tests' original allocation hooks."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
source = (root / "Source/Tests/CircuitTests.cpp").read_text()
start = source.index("namespace\n{\n    thread_local long threadAllocationCount")
end = source.index("\nnamespace\n{\n    /**", start)
hooks = source[start:end]
main = r'''
int main(int argc, char** argv)
{
    int tests=0, checks=0, failures=0;
    for(const auto& entry:luthier::tests::TestRegistry::get().getEntries())
    {
        const auto name=entry.suite+"."+entry.name;
        bool selected=argc==1;
        for(int i=1;i<argc;++i)selected=selected || name.containsIgnoreCase(argv[i]);
        if(!selected)continue;
        luthier::tests::TestContext ctx;ctx.currentTest=name;
        try { entry.fn(ctx); }
        catch(const std::exception& error) { ctx.fail(juce::String("Exception: ")+error.what()); }
        catch(...) { ctx.fail("Unknown exception"); }
        ++tests;checks+=ctx.checks;failures+=ctx.failures;
        std::printf("%s checks=%d failures=%d\n", name.toRawUTF8(),ctx.checks,ctx.failures);
        for(const auto& message:ctx.failureMessages)std::printf("%s\n",message.toRawUTF8());
        std::fflush(stdout);
    }
    std::printf("TOTAL tests=%d checks=%d failures=%d\n",tests,checks,failures);
    return tests>0 && failures==0 ? 0:1;
}
'''
Path(sys.argv[1]).write_text("#include \"Tests/TestFramework.h\"\n#include <cstdio>\n#include <cstdlib>\n#include <new>\n"+hooks+main)
