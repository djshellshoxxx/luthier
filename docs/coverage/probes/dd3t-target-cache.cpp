#include "Source/ReferencePedals/Models/boss_dd3t.h"
#include <cstdio>
#include <cstring>
#include <ctime>
using namespace luthier::reference;
static double cpu(){timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main()
{
 for(double sr:{44100.,48000.,88200.,96000.,192000.})
 {
  models::BossDd3t p;p.prepare(sr,2048,2);uint32_t rng=73;uint64_t hash=1469598103934665603ull;
  auto draw=[&](){rng=1664525u*rng+1013904223u;return rng;};
  int frame=0;
  for(int block=0;block<1000;++block)
  {
   if(block%3==0){int index=draw()%19;float value=float(draw()>>8)/16777215;p.setParam(index,value);}
   if(block%7==0){TimeInfo t;t.valid=true;t.bpm=60+draw()%160;t.ppq=block*.137;p.setTime(t);}
   if(block%17==0){juce::var state(new juce::DynamicObject());state.getDynamicObject()->setProperty("tapIntervalMs",133+int(draw()%668));state.getDynamicObject()->setProperty("hasTapped",bool(draw()&1));p.loadExtra(state);}
   if(block%113==0)p.reset();
   int n=block%4==0?1:block%4==1?16:block%4==2?512:2048;double l[2048],r[2048];
   for(int i=0;i<n;++i,++frame){l[i]=.1*std::sin(frame*.131);r[i]=.1*std::cos(frame*.173);}p.process(l,r,n);
   for(int i=0;i<n;++i)for(double x:{l[i],r[i]}){uint64_t bits;std::memcpy(&bits,&x,8);hash^=bits;hash*=1099511628211ull;}
  }
  std::printf("oracle sr%.0f frames%d rng%u hash%llu currentMs%.17g\n",sr,frame,rng,(unsigned long long)hash,p.delayMilliseconds());
 }
 for(int trial=0;trial<5;++trial)
 {
  models::BossDd3t p;p.prepare(48000,128,2);double l[128],r[128],elapsed=0,checksum=0;
  for(int block=0;block<37500;++block){for(int i=0;i<128;++i){l[i]=.1*std::sin((block*128+i)*.131);r[i]=.1*std::cos((block*128+i)*.173);}double begin=cpu();p.process(l,r,128);elapsed+=cpu()-begin;checksum+=l[0]+r[127];}
  std::printf("CPU trial%d core_percent%.9f checksum%.17g\n",trial,elapsed,checksum);
 }
}
