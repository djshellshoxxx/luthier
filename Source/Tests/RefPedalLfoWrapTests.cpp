#include "TestFramework.h"
#include "../ReferencePedals/Common/Lfo.h"
#include <cstring>
using namespace luthier::reference;
namespace
{
class OriginalLfo
{
public:
 void prepare(double rate){sr=common::sampleRate(rate);setRate(hz);}
 void setRate(double rate){hz=std::clamp(common::safe(rate),0.,sr*.49);inc=hz/sr;}
 void setPhase(double x){p=common::safe(x);p-=std::floor(p);}
 double phase()const{return p;}double sine()const{return table(p);}double triangle()const{return 1-4*std::abs(p-.5);}
 void advance(){p+=inc;p-=std::floor(p);}
private:common::TableSin table;double sr=48000,hz=1,inc=1./48000,p=0;
};
uint64_t bits(double x){uint64_t b;std::memcpy(&b,&x,sizeof b);return b;}
}
LUTHIER_TEST(RefPedalLfoWrap, exactOriginalFloorOracleWithBoundaryAndHostileEvents)
{
 const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
 for(double sr:{8000.,44100.,48000.,88200.,96000.,192000.,0.,inf})
 {
  common::Lfo current;OriginalLfo old;current.prepare(sr);old.prepare(sr);uint32_t rng=73;
  const double phases[]{0.,1.,std::nextafter(1.,0.),std::nextafter(1.,2.),-1e-20,-1.,-1e300,1e300,nan,inf,-inf};
  for(double phase:phases)for(double rate:{0.,.1,3.56,100.,1e6,-1.,nan,inf})
  {
   current.setPhase(phase);old.setPhase(phase);current.setRate(rate);old.setRate(rate);
   for(int i=0;i<257;++i)
   {
    CHECK(bits(current.phase())==bits(old.phase()));CHECK(bits(current.sine())==bits(old.sine()));CHECK(bits(current.triangle())==bits(old.triangle()));
    current.advance();old.advance();
   }
  }
  for(int i=0;i<100000;++i)
  {
   if(i%113==0){rng=1664525u*rng+1013904223u;const double rate=double(rng)/4294967296.*100000;current.setRate(rate);old.setRate(rate);}
   if(i%173==0){rng=1664525u*rng+1013904223u;const double phase=double(rng)/2147483648.-1;current.setPhase(phase);old.setPhase(phase);}
   if(i==50000){current.prepare(96000);old.prepare(96000);}
   current.advance();old.advance();CHECK(bits(current.phase())==bits(old.phase()));
  }
 }
}
