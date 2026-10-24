#include "Source/ReferencePedals/Models/boss_ce2.h"
#include "Source/ReferencePedals/Models/boss_ce2w.h"
#include "Source/ReferencePedals/Models/boss_dd3t.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <type_traits>
using namespace luthier::reference;
static double cpu(){timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static void hashValue(uint64_t& h,double x){uint64_t bits;std::memcpy(&bits,&x,8);h^=bits;h*=1099511628211ull;}
template<class P>void trace(const char* name)
{
 P p;uint32_t rng=73;auto draw=[&](){rng=1664525u*rng+1013904223u;return rng;};
 for(double sr:{44100.,48000.,88200.,96000.,192000.})
 {
  PedalServices services;services.instanceSeed=73;p.setServices(services);p.prepare(sr,2048,2);uint64_t hash=1469598103934665603ull;int frames=0;
  for(int block=0;block<600;++block)
  {
   if(block%3==0){const int index=draw()%p.desc().numParams;const float value=float(draw()>>8)/16777215;p.setParam(index,value);}
   if(block%40==0){if constexpr(std::is_same_v<P,models::BossDd3t>)p.setParam(3,float((block/40)%4)/3);else if constexpr(std::is_same_v<P,models::BossCe2w>)p.setParam(2,float((block/40)%3)/2);else p.setParam(3,float((block/40)%2));}
   TimeInfo t;t.valid=t.playing=true;t.bpm=40+draw()%200;t.ppq=frames/sr*t.bpm/60;p.setTime(t);
   if(block%19==0){p.onFoot({true,.75f,.7f,0});p.setExpression(0,float(draw()>>8)/16777215);}
   if(block%19==3)p.onFoot({false,.75f,.7f,0});
   if(block%29==0){p.onTransport(TransportEvent::Start,0);p.setEnabled(block%58==0);}
   if(block%37==0){juce::var extra(new juce::DynamicObject());extra.getDynamicObject()->setProperty("tapIntervalMs",133+int(draw()%668));extra.getDynamicObject()->setProperty("hasTapped",true);p.loadExtra(extra);}
   if(block==211)p.prepare(sr==48000?96000:48000,2048,2);if(block==233)p.prepare(sr,2048,2);if(block==401)p.reset();
   const int sizes[]{1,4,16,64,512,2048};const int n=sizes[block%6];double l[2048],r[2048],wl[2048],wr[2048];
   for(int i=0;i<n;++i,++frames){l[i]=.31*std::sin(frames*.131);r[i]=.19*std::cos(frames*.173);}
   if constexpr(std::is_same_v<P,models::BossDd3t>){p.processWithWetResidual(l,r,wl,wr,n);for(int i=0;i<n;++i){hashValue(hash,wl[i]);hashValue(hash,wr[i]);}}
   else p.process(l,r,n);
   for(int i=0;i<n;++i){hashValue(hash,l[i]);hashValue(hash,r[i]);}
   if(block%5==0){for(int i=0;i<n;++i)l[i]=.07*std::sin(i*.13),r[i]=-.03;p.processBypass(l,r,n);for(int i=0;i<n;++i){hashValue(hash,l[i]);hashValue(hash,r[i]);}}
   for(int k=0;k<p.desc().numParams;++k)hashValue(hash,p.getParam(k));hashValue(hash,p.getTailEnergyDb());
   if constexpr(std::is_same_v<P,models::BossDd3t>){hashValue(hash,p.delayMilliseconds());hashValue(hash,p.shortLoopActive());const auto m=p.readUiMeters();hashValue(hash,m.bpm);hashValue(hash,m.playhead);hashValue(hash,m.loopLength);hashValue(hash,m.progress);}
   else hashValue(hash,p.modulationPhase());
   juce::var extra;p.saveExtra(extra);const auto json=juce::JSON::toString(extra);for(auto ch:json){hash^=uint64_t(ch);hash*=1099511628211ull;}
  }
  std::printf("trace %s sr%.0f frames%d final_rng%u output_and_public_state_hash%llu\n",name,sr,frames,rng,(unsigned long long)hash);
 }
}
template<class P>void bench(const char* name,int blockSize,int modeIndex,int variant)
{
 for(int trial=0;trial<5;++trial)
 {
  P p;if(modeIndex>=0)p.setParam(modeIndex,float(variant));p.prepare(48000,blockSize,2);double l[512],r[512],elapsed=0,checksum=0;
  const int frames=48000*100;
  for(int at=0;at<frames;at+=blockSize){const int n=std::min(blockSize,frames-at);for(int i=0;i<n;++i){l[i]=.1*std::sin((at+i)*.131);r[i]=.1*std::cos((at+i)*.173);}const double begin=cpu();p.process(l,r,n);elapsed+=cpu()-begin;checksum+=l[0]+r[n-1];}
  std::printf("CPU %s block%d variant%d trial%d core_percent%.9f checksum%.17g\n",name,blockSize,variant,trial,elapsed,checksum);
 }
}
int main(){trace<models::BossCe2>("CE2");trace<models::BossCe2w>("CE2W");trace<models::BossDd3t>("DD3T");bench<models::BossCe2>("CE2",512,4,0);bench<models::BossCe2w>("CE2W",128,-1,0);bench<models::BossDd3t>("DD3T",512,11,0);}
