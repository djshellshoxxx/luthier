#include "Source/Tests/TestFramework.h"
#include "Source/ReferencePedals/Models/mxr_phase90_script.h"
#include <array>
#include "soak-template-exact.inc"
#include <chrono>
#include <cstdio>
LUTHIER_TEST(QualificationPhase90Soak, randomizedThirtyMinutesAtEveryRate)
{ soak<luthier::reference::models::MxrPhase90Script>(ctx); }
int main(){
 using namespace luthier::tests;
 if(!perfRunRequested()){std::fprintf(stderr,"LUTHIER_PERF=1 required\n");return 2;}
 std::printf("Exact51fd4de677fd0d4a056ded5ee2e8845b9e81584d; original soak template, phaser only;1800s at44100/48000/96000/192000Hz;120min total offline audio.\n");std::fflush(stdout);
 int count=0,checks=0,failures=0;auto start=std::chrono::steady_clock::now();double cpu=threadCpuTimeSeconds();
 for(auto& t:TestRegistry::get().getEntries())if(t.suite=="QualificationPhase90Soak"){
 TestContext ctx;ctx.currentTest=t.suite+"."+t.name;t.fn(ctx);++count;checks+=ctx.checks;failures+=ctx.failures;
 for(auto& e:ctx.failureMessages)std::fprintf(stderr,"%s\n",e.toRawUTF8());
 }
 std::printf("%d tests,%d checks,%d failures;wall%.6fs;threadCPU%.6fs\n",count,checks,failures,std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(),threadCpuTimeSeconds()-cpu);
 return count==1&&checks==8&&failures==0?0:1;
}
