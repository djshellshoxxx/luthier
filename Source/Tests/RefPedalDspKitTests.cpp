#include "TestFramework.h"
#include "../ReferencePedals/Common/Biquad.h"
#include "../ReferencePedals/Common/DcBlock.h"
#include "../ReferencePedals/Common/TptSvf.h"
#include "../ReferencePedals/Common/FractionalDelay.h"
#include "../ReferencePedals/Common/EnvelopeFollower.h"
#include "../ReferencePedals/Common/Lfo.h"
#include "../ReferencePedals/Common/TempoSync.h"
#include "../ReferencePedals/Common/NoiseGen.h"
#include "../ReferencePedals/Common/NewtonClipper.h"
#include "../ReferencePedals/Common/OversamplerAdapter.h"
#include <limits>
using namespace luthier::reference::common;
LUTHIER_TEST(RefPedalDspKit, bilinearCutoffAndDc)
{
 for(double sr:{44100.,48000.,88200.,96000.,192000.}) {
  OnePole lp;lp.setLowpass(sr,1000);double in=0,out=0;
  for(int i=0;i<(int)sr;++i){double x=std::sin(2*pi*1000*i/sr),y=lp.process(x);if(i>(int)sr/2){in+=x*x;out+=y*y;}}
  CHECK_NEAR(out/in,.5,.0002);
  DcBlock dc;dc.prepare(sr);double y=0;for(int i=0;i<(int)sr;++i)y=dc.process(1);CHECK_NEAR(y,0,1e-10);
 }
}
LUTHIER_TEST(RefPedalDspKit, fractionalDelayAndSmoothing)
{
 FractionalDelay d;d.prepare(100);
 for(int i=0;i<80;++i)d.push((double)i*i*i);
 CHECK_NEAR(d.read(4.25),std::pow(79-4.25,3),1e-8);
 d.reset();CHECK(d.read(4)==0);
 LinearSmoother s;s.prepare(48000,20);s.reset(0);s.setTarget(1);
 for(int i=0;i<959;++i)s.next();CHECK(s.value()<1);CHECK(s.next()==1);
}
LUTHIER_TEST(RefPedalDspKit, newtonResidualAndBound)
{
 NewtonClipper c;
 for(int i=-1600;i<=1600;++i){double x=i*.01,y=c.process(x);CHECK(std::abs(c.residual(y,x))<1e-9);CHECK(c.lastIterations()<=8);}
}
LUTHIER_TEST(RefPedalDspKit, phaseEnvelopeNoiseAndFiniteRecovery)
{
 Lfo l;l.prepare(48000);l.setRate(2);for(int i=0;i<6000;++i)l.advance();CHECK_NEAR(l.phase(),.25,1e-12);CHECK_NEAR(l.sine(),1,1e-6);
 CHECK_NEAR(TempoSync::rate(120,.5),4,1e-12);CHECK_NEAR(TempoSync::phase(2.25,1),.25,1e-12);
 EnvelopeFollower e;e.prepare(48000,10,100);double y=0;for(int i=0;i<480;++i)y=e.process(1);CHECK_NEAR(y,1-std::exp(-1),1e-12);
 NoiseGen a(42),b(42);for(int i=0;i<1000;++i)CHECK(a.next()==b.next());
 Biquad q; q.setLowpass(48000,2000);TptSvf sv;sv.prepare(48000);OnePole lp;lp.setLowpass(48000,2000);
 for(double x:{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),1e-310,1.0,0.0}){CHECK_FINITE(q.process(x));CHECK_FINITE(sv.process(x).low);CHECK_FINITE(lp.process(x));}
 CHECK(safe(1e-310)==0);
}
LUTHIER_TEST(RefPedalDspKit, oversamplerPolicyDeterminism)
{
 for(int policy=0;policy<3;++policy){OversamplerAdapter a,b;a.prepare(48000,policy);b.prepare(48000,policy);CHECK(a.factor()==(2<<policy));CHECK(a.latencySamples()==(policy==0?3:policy==1?5:6));
 for(int i=0;i<10000;++i){const double x=std::sin(i*.13);CHECK(a.process(x,[](double v){return std::tanh(v);})==b.process(x,[](double v){return std::tanh(v);}));}}
}
