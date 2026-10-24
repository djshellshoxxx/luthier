// Reproducible actual-wrapper E66 acceptance measurements. O3 process CPU is
// reported as local core share, without pretending a calibrated 3 GHz machine.
#include "Source/ReferencePedals/Generic/Reverb.h"
#include <cstdio>
#include <vector>
#include <ctime>
using namespace luthier::reference;
static double cpu(){timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static void natural(generic::Reverb& p,int index,double x){p.setParam(index,float(p.desc().params[index].toNormalised(x)));}
int main()
{
 for(double sr:{44100.,48000.,88200.,96000.,192000.})
 {
  for(double decay:{1.,2.,4.,8.})
  {
   generic::Reverb p;natural(p,0,decay);natural(p,1,1);natural(p,3,1);natural(p,6,0);natural(p,7,20);p.prepare(sr,1,1);
   std::vector<double> energy(size_t(sr*decay*1.5));
   for(size_t i=0;i<energy.size();++i){double l=i==0?1:0,r=l;p.process(&l,&r,1);energy[i]=l*l+r*r;}
   for(size_t i=energy.size()-1;i>0;--i)energy[i-1]+=energy[i];
   size_t t5=0,t35=0;for(size_t i=0;i<energy.size();++i){const double db=10*std::log10(std::max(1e-30,energy[i]/energy[0]));if(!t5&&db<=-5)t5=i;if(db<=-35){t35=i;break;}}
   const double measured=2*double(t35-t5)/sr;std::printf("RT60 sr%.0f requested%.1f measured%.9f relative_error%.9f pass%d\n",sr,decay,measured,(measured-decay)/decay,std::abs(measured-decay)<=.1*decay);
  }
  for(uint32_t seed:{1u,73u,0xdeadbeefu})
  {
   generic::Reverb p;PedalServices services;services.instanceSeed=seed;p.setServices(services);natural(p,1,1);p.prepare(sr,1,1);
   for(int i=0;i<int(sr*.5);++i){double l=i==0?1:0,r=l;p.process(&l,&r,1);}p.setParam(9,1);
   double start[2]{},end[2]{};for(int i=0;i<int(sr*12);++i){double l=0,r=0;p.process(&l,&r,1);if(i>=int(sr)&&i<int(sr*2)){start[0]+=l*l;start[1]+=r*r;}if(i>=int(sr*11)){end[0]+=l*l;end[1]+=r*r;}}
   for(int c=0;c<2;++c){const double db=10*std::log10(end[c]/start[c]);std::printf("freeze sr%.0f seed%u channel%d change10seconds_db%.9f pass%d\n",sr,seed,c,db,std::abs(db)<=1);}
  }
  // Independent low/high band measurements after an impulse, dark vs bright.
  for(double tone:{-1.,1.})
  {
   generic::Reverb p;natural(p,1,1);natural(p,3,tone);natural(p,6,0);p.prepare(sr,1,1);
   common::Biquad low[2],high[2];for(int c=0;c<2;++c){low[c].setLowpass(sr,500);high[c].setHighpass(sr,6000);}
   double early[2]{},late[2]{};for(int i=0;i<int(sr*2);++i){double l=i==0?1:0,r=l;p.process(&l,&r,1);double x[]{l,r};for(int c=0;c<2;++c){const double lo=low[c].process(x[c]),hi=high[c].process(x[c]);if(i>=int(sr*.2)&&i<int(sr*.4)){early[0]+=lo*lo;early[1]+=hi*hi;}if(i>=int(sr*1.2)&&i<int(sr*1.4)){late[0]+=lo*lo;late[1]+=hi*hi;}}}
   std::printf("HF sr%.0f tone%.0f LF_decay_db%.9f HF_decay_db%.9f\n",sr,tone,10*std::log10(late[0]/early[0]),10*std::log10(late[1]/early[1]));
  }
 }

 for(int type:{1,2})for(int blockSize:{16,128,2048})for(bool silence:{false,true})
 {
  generic::Reverb p;natural(p,5,type);p.prepare(48000,blockSize,2);double l[2048]{},r[2048]{},elapsed=0,checksum=0;
  constexpr int frames=4800000;
  for(int at=0;at<frames;at+=blockSize){const int n=std::min(blockSize,frames-at);for(int i=0;i<n;++i){l[i]=silence?0:.1*std::sin((at+i)*.131);r[i]=silence?0:.1*std::cos((at+i)*.173);}const double begin=cpu();p.process(l,r,n);elapsed+=cpu()-begin;checksum+=l[0]+r[n-1];}
  std::printf("CPU type%d block%d silence%d seconds%.9f audio_seconds100 core_percent%.9f checksum%.9f\n",type,blockSize,silence,elapsed,elapsed,checksum);
 }
}
