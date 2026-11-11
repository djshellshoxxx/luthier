#include "TestFramework.h"
#include "../ReferencePedals/Generic/Reverb.h"
using namespace luthier::reference;
LUTHIER_TEST(RefPedalReverbDensity, actualHallRawDensityAt200msAcrossSeedsAndPhases)
{
 for(double sr:{44100.,48000.,88200.,96000.,192000.})for(uint32_t seed:{1u,73u,0xdeadbeefu})for(double preroll:{0.,.37,1.13})
 {
  generic::Reverb p;PedalServices services;services.instanceSeed=seed;p.setServices(services);p.setParam(1,1);p.prepare(sr,1,1);
  for(int i=0;i<int(sr*preroll);++i){double l=0,r=0;p.process(&l,&r,1);}
  std::vector<double> out[2];for(auto& v:out)v.resize(size_t(sr*.22));
  for(int i=0;i<int(out[0].size());++i){double l=i==0?1:0,r=l;p.process(&l,&r,1);out[0][i]=l;out[1][i]=r;}
  const int first=int(.19*sr),last=int(.21*sr);
  for(int c=0;c<2;++c)
  {
   double mean=0,variance=0;for(int i=first;i<last;++i)mean+=out[c][i];mean/=last-first;
   for(int i=first;i<last;++i)variance+=std::pow(out[c][i]-mean,2);
   const double sd=std::sqrt(variance/(last-first));int above=0;
   for(int i=first;i<last;++i)if(std::abs(out[c][i]-mean)>sd)++above;
   CHECK(double(above)/(last-first)/.317310507862914>.9);
  }
 }
}
LUTHIER_TEST(RefPedalReverbDensity, seededHallAndPlateResetAndPartition)
{
 for(double sr:{44100.,48000.,88200.,96000.,192000.})for(int type:{1,2})
 {
  generic::Reverb a,b;PedalServices services;services.instanceSeed=0xdeadbeef;
  a.setServices(services);b.setServices(services);a.setParam(5,type*.25f);b.setParam(5,type*.25f);
  a.prepare(sr,2048,2);b.prepare(sr,2048,2);
  std::array<double,2048> l{},r{},x{},y{};
  for(int cycle=0;cycle<2;++cycle)
  {
   for(int block=0;block<32;++block)
   {
    const int n=block%4==0?1:block%4==1?16:block%4==2?512:2048;
    for(int i=0;i<n;++i)l[i]=x[i]=.1*std::sin((block*2048+i)*.13),r[i]=y[i]=.1*std::cos((block*2048+i)*.17);
    a.process(l.data(),r.data(),n);for(int i=0;i<n;++i)b.process(x.data()+i,y.data()+i,1);
    for(int i=0;i<n;++i){CHECK(l[i]==x[i]);CHECK(r[i]==y[i]);CHECK(std::isfinite(l[i]));CHECK(std::isfinite(r[i]));}
   }
   a.reset();b.reset();
  }
 }
}

LUTHIER_TEST(RefPedalReverbDensity, originalWhiteWetAndDefaultSineLevelFixtures)
{
 constexpr double pi=3.14159265358979323846;
 for(double sr:{44100.,48000.,88200.,96000.,192000.})
 {
  generic::Reverb white,sine;white.setParam(1,1);white.prepare(sr,1,1);sine.prepare(sr,1,1);
  uint32_t rng=42;double whiteDry=0,whiteWet=0,sineDry=0,sineWet=0;
  for(int i=0;i<int(sr*10);++i)
  {
   rng=1664525u*rng+1013904223u;
   const double noise=(double(rng)/4294967296.-.5)*std::sqrt(12.)*std::pow(10.,-18./20);
   const double wave=std::sqrt(2.)*std::pow(10.,-18./20)*std::sin(2*pi*1000*i/sr);
   double wl=noise,wr=noise,sl=wave,srOut=wave;white.process(&wl,&wr,1);sine.process(&sl,&srOut,1);
   if(i>=int(sr*7)){whiteDry+=noise*noise;whiteWet+=(wl*wl+wr*wr)*.5;sineDry+=wave*wave;sineWet+=(sl*sl+srOut*srOut)*.5;}
  }
  CHECK(std::abs(10*std::log10(whiteWet/whiteDry))<=3);
  CHECK(std::abs(10*std::log10(sineWet/sineDry))<=1.5);
 }
}
