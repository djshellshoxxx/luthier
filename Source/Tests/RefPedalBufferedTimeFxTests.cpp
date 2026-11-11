#include "TestFramework.h"
#include "../ReferencePedals/Models/boss_ce2.h"
#include "../ReferencePedals/Models/boss_ce2w.h"
#include <array>
#include <complex>
namespace
{
template<class P> void bufferOracle(luthier::tests::TestContext& ctx,double cutoff)
{
    constexpr double pi=3.14159265358979323846;
    for(double sr:{44100.,48000.,88200.,96000.,192000.})for(double hz:{10.,15.,1000.})
    {
        P pedal;pedal.prepare(sr,1,1);std::complex<double> sum{},phase=1,step=std::exp(std::complex<double>(0,-2*pi*hz/sr));double rightEnergy=0;
        for(int i=0;i<int(sr*2);++i){double l=.1*std::sin(2*pi*hz*i/sr),r=0;pedal.processBypass(&l,&r,1);if(i>=int(sr))sum+=l*phase;phase*=step;rightEnergy+=r*r;}
        const auto z=step;const double g=std::tan(pi*cutoff/sr),a=(g-1)/(g+1),b=g/(g+1);
        const auto expected=1.-b*(1.+z)/(1.+a*z),actual=sum*std::complex<double>(0,2/(.1*sr));
        CHECK(std::abs(actual-expected)<1e-8);CHECK(rightEnergy==0);CHECK(pedal.getLatencySamples()==0);
        pedal.reset();double l=.001,r=0;pedal.processBypass(&l,&r,1);CHECK(l>.0009);CHECK(r==0);
    }
}
template<class P> void independentState(luthier::tests::TestContext& ctx)
{
    P a,b;a.prepare(48000,512,1);b.prepare(48000,512,1);
    std::array<double,512> l{},r{},x{},y{},bypassL{},bypassR{};
    for(int block=0;block<40;++block)
    {
        a.setEnabled(block%2);b.setEnabled(true);
        for(int i=0;i<512;++i){l[i]=x[i]=.1*std::sin((block*512+i)*.13);r[i]=y[i]=.1*std::cos((block*512+i)*.17);bypassL[i]=.3;bypassR[i]=-.5;}
        a.processBypass(bypassL.data(),bypassR.data(),512); // Deliberately absent on b.
        a.process(l.data(),r.data(),512);b.process(x.data(),y.data(),512);
        CHECK(l==x);CHECK(r==y);
    }
}
}
LUTHIER_TEST(RefPedalBufferedTimeFx, analyticBufferColourAndStereoIsolation)
{
    bufferOracle<luthier::reference::models::BossCe2>(ctx,15);
    bufferOracle<luthier::reference::models::BossCe2w>(ctx,10);
}
LUTHIER_TEST(RefPedalBufferedTimeFx, boardOwnsFadeAndBypassHistoryIsIndependent)
{
    independentState<luthier::reference::models::BossCe2>(ctx);
    independentState<luthier::reference::models::BossCe2w>(ctx);
}
