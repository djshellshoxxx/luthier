// Diagnostic only: GCC -fno-access-control exposes the meter state without a production test hook.
// Build against a frozen model snapshot; disable host FTZ/DAZ only on this thread.
#include "Source/ReferencePedals/Models/boss_dd3t.h"
#include <array>
#include <cfenv>
#include <cstdio>
#include <xmmintrin.h>
using luthier::reference::models::BossDd3t;
int main(){const unsigned saved=_mm_getcsr();_mm_setcsr(saved&~0x8040u);
 BossDd3t p;p.setParam(0,1);p.setParam(1,0);p.setParam(2,0);p.setParam(3,0);p.setParam(10,1);p.prepare(44100,512,1);
 std::array<double,512> l{},r{};l[0]=r[0]=.1;
 for(int i=0;i<3446;++i){p.process(l.data(),r.data(),512);l.fill(0);r.fill(0);}
 std::feclearexcept(FE_ALL_EXCEPT);p.process(l.data(),r.data(),512);
 const int underflow=std::fetestexcept(FE_UNDERFLOW),denormal=std::fpclassify(p.energy)==FP_SUBNORMAL;
 std::printf("frames=%d energy=%.17g tail_db=%.9g subnormal=%d underflow=%d audio=%.17g\n",3447*512,p.energy,p.getTailEnergyDb(),denormal,underflow,l[0]);
 _mm_setcsr(saved);return underflow||denormal;}
