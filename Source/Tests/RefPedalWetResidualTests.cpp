#include "TestFramework.h"
#include "../ReferencePedals/Generic/Delay.h"
#include "../ReferencePedals/Generic/Reverb.h"
#include "../ReferencePedals/Models/boss_dd3t.h"
#include <array>
using namespace luthier::reference;
namespace
{
template<class P> void identicalSinglePass(luthier::tests::TestContext& ctx)
{
 for(double sr:{44100.,48000.,88200.,96000.,192000.})
 {
  P ordinary,residual;ordinary.prepare(sr,2048,2);residual.prepare(sr,2048,2);
  std::array<double,2048> l{},r{},x{},y{},wl{},wr{};
  uint32_t rng=73;auto draw=[&](){rng=1664525u*rng+1013904223u;return rng;};
  for(int block=0;block<96;++block)
  {
   if(block%3==0){const int index=draw()%ordinary.desc().numParams;const float value=float(draw()>>8)/16777215;ordinary.setParam(index,value);residual.setParam(index,value);}
   if(block%11==0){ordinary.setEnabled(block%22==0);residual.setEnabled(block%22==0);}
   const int sizes[]{1,4,16,64,512,2048};const int n=sizes[block%6];
   for(int i=0;i<n;++i){l[i]=x[i]=(block%4==0?32.:.2)*std::sin((block*2048+i)*.13);r[i]=y[i]=(block%4==0?-32.:.1)*std::cos((block*2048+i)*.17);wl[i]=wr[i]=std::numeric_limits<double>::quiet_NaN();}
   ordinary.process(l.data(),r.data(),n);CHECK(residual.processWithWetResidual(x.data(),y.data(),wl.data(),wr.data(),n));
   for(int i=0;i<n;++i){CHECK(l[i]==x[i]);CHECK(r[i]==y[i]);CHECK(std::isfinite(wl[i]));CHECK(std::isfinite(wr[i]));}
  }
 }
}
template<class P> void hotBypassMatchesIndependentTail(luthier::tests::TestContext& ctx,int trailsIndex)
{
 for(double sr:{44100.,48000.,88200.,96000.,192000.})
 {
  P hot,cold;hot.setParam(trailsIndex,1);cold.setParam(trailsIndex,1);hot.prepare(sr,1,2);cold.prepare(sr,1,2);
  for(int i=0;i<int(sr);++i){double l=.1*std::sin(i*.13),r=.1*std::cos(i*.17),x=l,y=r;hot.process(&l,&r,1);cold.process(&x,&y,1);}
  hot.setEnabled(false);cold.setEnabled(false);
  for(int i=0;i<int(sr*.1);++i){double l=0,r=0,x=0,y=0;hot.process(&l,&r,1);cold.process(&x,&y,1);}
  double tailEnergy=0,inferredError=0;
  for(int i=0;i<int(sr*.1);++i)
  {
   const double inputL=i%2?32:-32,inputR=-inputL;double l=inputL,r=inputR,x=0,y=0,wl=0,wr=0;
   CHECK(hot.processWithWetResidual(&l,&r,&wl,&wr,1));cold.process(&x,&y,1);
   CHECK(wl==x);CHECK(wr==y);CHECK(std::abs(l)<=16);CHECK(std::abs(r)<=16);
   tailEnergy+=wl*wl+wr*wr;inferredError+=std::abs((l-inputL)-wl)+std::abs((r-inputR)-wr);
  }
  CHECK(tailEnergy>1e-12);CHECK(inferredError>sr);
  hot.setParam(trailsIndex,0);cold.setParam(trailsIndex,0);
  double lastWet=0;
  for(int i=0;i<int(sr*.1);++i){double l=32,r=-32,wl=0,wr=0;hot.processWithWetResidual(&l,&r,&wl,&wr,1);if(i>int(sr*.05))lastWet+=std::abs(wl)+std::abs(wr);}
  CHECK(lastWet==0);
 }
}
}
LUTHIER_TEST(RefPedalWetResidual, normalOutputAndStateAdvanceExactlyOnce)
{
 identicalSinglePass<generic::Delay>(ctx);identicalSinglePass<generic::Reverb>(ctx);identicalSinglePass<models::BossDd3t>(ctx);
}
LUTHIER_TEST(RefPedalWetResidual, guardedHotDryCannotCorruptSpilloverPolarity)
{
 hotBypassMatchesIndependentTail<generic::Delay>(ctx,10);hotBypassMatchesIndependentTail<generic::Reverb>(ctx,8);hotBypassMatchesIndependentTail<models::BossDd3t>(ctx,8);
}
