#include "Source/ReferencePedals/Generic/Delay.h"
#include <cstdio>
#include <cstring>
#include <ctime>
using namespace luthier::reference;
static double cpu(){timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main()
{
 for(double sr:{44100.,48000.,88200.,96000.,192000.})for(int mode=0;mode<3;++mode)
 {
  generic::Delay p;p.setParam(4,mode*.5f);p.prepare(sr,2048,2);uint32_t rng=73;uint64_t hash=1469598103934665603ull;int frame=0;
  auto draw=[&](){rng=1664525u*rng+1013904223u;return rng;};
  for(int block=0;block<600;++block)
  {
   if(block%3==0){const int index=draw()%14;const float value=float(draw()>>8)/16777215;p.setParam(index,value);}
   if(block%7==0){TimeInfo t;t.valid=t.playing=true;t.bpm=60+draw()%160;t.ppq=block*.137;p.setTime(t);}
   if(block%17==0){juce::var state(new juce::DynamicObject());state.getDynamicObject()->setProperty("tapMs",60+int(draw()%1941));p.loadExtra(state);}
   if(block%113==0)p.reset();if(block%29==0)p.setEnabled(block%58==0);
   const int sizes[]{1,4,16,64,512,2048};const int n=sizes[block%6];double l[2048],r[2048],wl[2048],wr[2048];
   for(int i=0;i<n;++i,++frame){l[i]=.1*std::sin(frame*.131);r[i]=.1*std::cos(frame*.173);}
   p.processWithWetResidual(l,r,wl,wr,n);
   for(int i=0;i<n;++i)for(double x:{l[i],r[i],wl[i],wr[i]}){uint64_t bits;std::memcpy(&bits,&x,8);hash^=bits;hash*=1099511628211ull;}
  }
  std::printf("oracle sr%.0f mode%d frames%d rng%u hash%llu\n",sr,mode,frame,rng,(unsigned long long)hash);
 }
 for(int mode=0;mode<3;++mode)for(int trial=0;trial<3;++trial)
 {
  generic::Delay p;p.setParam(4,mode*.5f);p.prepare(48000,128,2);double l[128],r[128],wl[128],wr[128],elapsed=0,checksum=0;
  for(int block=0;block<37500;++block){for(int i=0;i<128;++i){l[i]=.1*std::sin((block*128+i)*.131);r[i]=.1*std::cos((block*128+i)*.173);}double begin=cpu();p.processWithWetResidual(l,r,wl,wr,128);elapsed+=cpu()-begin;checksum+=l[0]+r[127]+wl[17]+wr[72];}
  std::printf("CPU mode%d trial%d core_percent%.9f checksum%.17g\n",mode,trial,elapsed,checksum);
 }
}
