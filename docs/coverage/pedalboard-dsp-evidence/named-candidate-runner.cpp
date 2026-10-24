#include "../../../Source/Tests/TestFramework.h"
#include <cstdio>
int main(){int bad=0,checks=0;for(auto& t:luthier::tests::TestRegistry::get().getEntries()){luthier::tests::TestContext ctx;t.fn(ctx);checks+=ctx.checks;bad+=ctx.failures;std::printf("%s::%s %d checks %d failures\n",t.suite.toRawUTF8(),t.name.toRawUTF8(),ctx.checks,ctx.failures);for(auto& message:ctx.failureMessages)std::puts(message.toRawUTF8());}std::printf("TOTAL %d checks %d failures\n",checks,bad);return bad?1:0;}
