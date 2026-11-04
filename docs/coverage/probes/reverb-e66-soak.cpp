// 60 seconds per type/rate, bounded deterministic controls and block extremes.
// Reports finite bounds, allocation counts and actual one-second tail energy
// variation rather than defining monotonicity by backward integration itself.
#include "Source/ReferencePedals/Generic/Reverb.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>
using namespace luthier::reference;
static thread_local uint64_t allocations=0;
void* operator new(std::size_t n){++allocations;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
int main()
{
 for(double sr:{44100.,48000.,88200.,96000.,192000.})for(int type=0;type<5;++type)
 {
  generic::Reverb p;PedalServices services;services.instanceSeed=73;p.setServices(services);p.setParam(5,type*.25f);p.setParam(1,1);p.prepare(sr,2048,2);
  uint32_t rng=73;auto draw=[&](){rng=1664525u*rng+1013904223u;return float(rng>>8)/16777215;};
  constexpr int sizes[]{1,4,16,64,512,2048};uint64_t frames=0,block=0,bad=0,before=allocations;double peak=0,energy[60]{};double l[2048],r[2048];
  const uint64_t total=uint64_t(sr*60);
  while(frames<total)
  {
   if(block%31==0){for(int index:{0,2,3,4,6,7})p.setParam(index,draw());}
   const int n=int(std::min<uint64_t>(sizes[block%6],total-frames));
   for(int i=0;i<n;++i){const uint64_t at=frames+i;const double x=at<uint64_t(sr*.1)?.05*std::sin(at*.371):0;l[i]=r[i]=x;}
   p.process(l,r,n);
   for(int i=0;i<n;++i){if(!std::isfinite(l[i])||!std::isfinite(r[i]))++bad;peak=std::max(peak,std::max(std::abs(l[i]),std::abs(r[i])));energy[size_t((frames+i)/uint64_t(sr))]+=l[i]*l[i]+r[i]*r[i];}
   frames+=n;++block;
  }
  const uint64_t allocated=allocations-before;double largestRise=0;int rises=0;
  for(int second=2;second<60;++second)if(energy[second-1]>1e-24){const double rise=10*std::log10(std::max(1e-300,energy[second]/energy[second-1]));largestRise=std::max(largestRise,rise);if(rise>0)++rises;}
  std::printf("sr%.0f type%d frames%llu blocks%llu seed73 finalrng%u nonfinite%llu allocations%llu peak%.9g lastEnergy%.9g positiveWindowSteps%d largestRise_db%.9f\n",sr,type,(unsigned long long)frames,(unsigned long long)block,rng,(unsigned long long)bad,(unsigned long long)allocated,peak,energy[59],rises,largestRise);
 }
}
