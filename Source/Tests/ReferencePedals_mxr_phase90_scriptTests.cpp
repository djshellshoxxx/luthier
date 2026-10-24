#include "TestFramework.h"
#include "../ReferencePedals/Models/mxr_phase90_script.h"
#include <array>
using luthier::reference::models::MxrPhase90Script;
LUTHIER_TEST(RefPedalPhase90, analyticStaticNotchesAndBypass)
{
    for(double sr:{44100.,48000.,88200.,96000.,192000.})
    {
        MxrPhase90Script p;p.setEnabled(true);p.setParam(3,0);p.setParam(5,0);p.prepare(sr,1,1);
        constexpr double pi=3.14159265358979323846;const double center=330*std::sqrt(10.);
        double real[2]{},imaginary[2]{};
        double frequency[]{sr/pi*std::atan(std::tan(pi*center/sr)*std::tan(pi/8)),sr/pi*std::atan(std::tan(pi*center/sr)*std::tan(3*pi/8))};
        for(int i=0;i<int(sr*1.5);++i)
        {
            double l=i==0?.001:0,r=l;p.process(&l,&r,1);
            for(int k=0;k<2;++k){real[k]+=l*std::cos(2*pi*frequency[k]*i/sr);imaginary[k]-=l*std::sin(2*pi*frequency[k]*i/sr);}
        }
        for(int k=0;k<2;++k)CHECK_MSG(std::hypot(real[k],imaginary[k])<1e-7,juce::String(std::hypot(real[k],imaginary[k]),12));
        p.setEnabled(false);for(int i=0;i<int(sr*.01);++i){double l=.1,r=-.1;p.process(&l,&r,1);}
        double l=.125,r=-.25;p.process(&l,&r,1);CHECK_NEAR(l,.125,1e-15);CHECK_NEAR(r,-.25,1e-15);
    }
}
LUTHIER_TEST(RefPedalPhase90, blockPartitionAndSeededReset)
{
    MxrPhase90Script a,b;a.setEnabled(true);b.setEnabled(true);a.prepare(48000,2048,1);b.prepare(48000,2048,1);
    std::array<double,2048> l{},r{},x{},y{};
    for(int i=0;i<2048;++i)l[i]=x[i]=.1*std::sin(i*.1),r[i]=y[i]=-l[i];
    a.process(l.data(),r.data(),2048);for(int i=0;i<2048;++i)b.process(x.data()+i,y.data()+i,1);
    CHECK(l==x);CHECK(r==y);
    a.setParam(11,1);b.setParam(11,1);a.reset();b.reset();l.fill(0);r.fill(0);x.fill(0);y.fill(0);
    a.process(l.data(),r.data(),2048);b.process(x.data(),y.data(),2048);CHECK(l==x);CHECK(r==y);
}
LUTHIER_TEST(RefPedalPhase90, lfoRateAndSixtySecondFeedbackStability)
{
    for(float setting:{0.f,1.f})
    {
        MxrPhase90Script p;p.setEnabled(true);p.setParam(0,setting);p.prepare(48000,1,1);double phase=0;int wraps=0;
        for(int i=0;i<48000*4;++i){double l=0,r=0;p.process(&l,&r,1);if(p.modulationPhase()<phase)++wraps;phase=p.modulationPhase();}
        CHECK_NEAR((wraps+phase)/4,.3*std::pow(25,setting),1e-9);
    }
    MxrPhase90Script p;p.setEnabled(true);p.setParam(2,1);p.prepare(48000,2048,1);std::array<double,2048> l{},r{};l[0]=r[0]=1;bool stable=true;
    for(int block=0;block<1407;++block){p.process(l.data(),r.data(),2048);for(int i=0;i<2048;++i)stable=stable && std::isfinite(l[i]) && std::isfinite(r[i]) && std::abs(l[i])<4 && std::abs(r[i])<4;l.fill(0);r.fill(0);}
    CHECK(stable);
}
LUTHIER_TEST(RefPedalPhase90, independentPointSixFeedbackStabilityBound)
{
    // Acceptance-only numerical probe. The hardware-facing Block variant stays
    // at its specified0.35; no parameter or model law is changed for this test.
    constexpr double pi=3.14159265358979323846;
    for(double sr:{44100.,48000.,88200.,96000.,192000.})
    {
        double states[4]{},feedback=0,phase=0,triangle=.5,a=0,step=0,peak=0,tail=0;
        const double coefficient=1-std::exp(-1/(sr*.02));bool finite=true;
        for(int i=0;i<int(sr*60);++i)
        {
            const double tri=phase<.65?phase/.65:(1-phase)/.35;
            triangle+=(tri-triangle)*coefficient;
            if((i&7)==0){const double g=std::tan(pi*330*std::pow(10.,triangle)/sr);step=((g-1)/(g+1)-a)/8;}
            a+=step;double x=(i==0?1.:0.)+.6*std::tanh(feedback);
            for(auto& state:states)
            {
                const double y=a*x+state;state=x-a*y;if(std::abs(state)<1e-30)state=0;
                x=y/std::sqrt(1+.04*y*y);
            }
            feedback=x;finite=finite&&std::isfinite(x);peak=std::max(peak,std::abs(x));
            if(i>=int(sr*59))tail+=x*x;
            phase+=16/sr;if(phase>=1)phase-=1;
        }
        CHECK(finite);CHECK(peak<=16);CHECK(tail/sr<1e-12);
    }
}
