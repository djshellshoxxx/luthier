#include "TestFramework.h"
#include "../ReferencePedals/Models/mxr_phase90_script.h"
#include <array>
#include <complex>
using luthier::reference::models::MxrPhase90Script;
LUTHIER_TEST(RefPedalPhase90, revisedTwoAnchorStaticNotchesAndBypass)
{
    for(double sr:{44100.,48000.,88200.,96000.,192000.})
    {
        MxrPhase90Script p;p.setEnabled(true);p.setParam(3,0);p.setParam(5,0);p.prepare(sr,1,1);
        constexpr double pi=3.14159265358979323846;const double center=330*std::sqrt(10.);
        double real[2]{},imaginary[2]{};
        double frequency[]{center*std::tan(pi/8),center*std::tan(3*pi/8)};
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
LUTHIER_TEST(RefPedalPhase90, originalLiteralBilinearOracleRetained)
{
    // Historical literal formula remains an independent executable oracle.
    // E58 deliberately changes this transfer to restore the specified analog
    // notch anchors; it does not assert equality to the old digital response.
    constexpr double pi=3.14159265358979323846;
    for(double sr:{44100.,48000.,88200.,96000.,192000.})for(double fc:{330.,330*std::sqrt(10.),3300.})
    {
        const double g=std::tan(pi*fc/sr),a=(g-1)/(g+1);
        for(double tangent:{std::tan(pi/8),std::tan(3*pi/8)})
        {
            const double oldNotch=sr/pi*std::atan(g*tangent);
            const auto z=std::exp(std::complex<double>(0,-2*pi*oldNotch/sr));
            const auto stage=(a+z)/(1.+a*z);
            CHECK(std::abs(.5*(1.+stage*stage*stage*stage))<1e-12);
        }
        if(fc==3300 && sr<=48000)
        {
            const double oldHigh=sr/pi*std::atan(g*std::tan(3*pi/8));
            CHECK(std::abs(oldHigh/(fc*std::tan(3*pi/8))-1)>.03);
        }
    }
}
LUTHIER_TEST(RefPedalPhase90, revisedSweepEndpointsAndZeroExplicitDelay)
{
    constexpr double pi=3.14159265358979323846;
    for(double sr:{44100.,48000.,88200.,96000.,192000.})for(double position:{0.,.5,1.})
    {
        MxrPhase90Script p;p.setEnabled(true);p.setParam(5,0);p.setParam(6,1);p.prepare(sr,1,1);
        luthier::reference::TimeInfo t;t.valid=t.playing=true;t.bpm=120;t.ppq=position*.65;
        for(int i=0;i<int(sr);++i){double l=0,r=0;p.setTime(t);p.process(&l,&r,1);}
        const double fc=330*std::pow(10.,position),frequencies[]{fc*std::tan(pi/8),fc*std::tan(3*pi/8)};
        std::complex<double> bins[2]{},phase[]{1.,1.},step[]{std::exp(std::complex<double>(0,-2*pi*frequencies[0]/sr)),std::exp(std::complex<double>(0,-2*pi*frequencies[1]/sr))};
        for(int i=0;i<int(sr*2);++i)
        {
            double l=i==0?1e-6:0,r=l;p.setTime(t);p.process(&l,&r,1);
            if(i==0)CHECK(std::abs(l)>1e-8); // Direct IIR response; no hidden buffering.
            for(int k=0;k<2;++k){bins[k]+=l*phase[k];phase[k]*=step[k];}
        }
        CHECK(p.getLatencySamples()==0);
        for(auto bin:bins)CHECK(std::abs(bin)/1e-6<1e-7);
    }
}
LUTHIER_TEST(RefPedalPhase90, allRateBlockExtremesAndStereoPhase)
{
    for(double sr:{44100.,48000.,88200.,96000.,192000.})
    {
        MxrPhase90Script a,b;
        for(auto* p:{&a,&b}){p->setEnabled(true);p->setParam(2,1);p->setParam(5,1);p->setParam(8,1);p->prepare(sr,2048,58);}
        int frame=0;std::array<double,2048> l{},r{},x{},y{};
        for(int n:{1,4,16,64,512,2048})
        {
            for(int i=0;i<n;++i,++frame){l[i]=x[i]=.1*std::sin(frame*.1);r[i]=y[i]=.1*std::cos(frame*.13);}
            a.process(l.data(),r.data(),n);for(int i=0;i<n;++i)b.process(x.data()+i,y.data()+i,1);
            for(int i=0;i<n;++i){CHECK(l[i]==x[i]);CHECK(r[i]==y[i]);CHECK(std::isfinite(l[i]));CHECK(std::isfinite(r[i]));}
        }
        a.reset();double peak=0;
        for(int i=0;i<int(sr*.1);++i){double l0=0,r0=0;a.process(&l0,&r0,1);peak=std::max(peak,std::max(std::abs(l0),std::abs(r0)));}
        CHECK(peak==0);
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
