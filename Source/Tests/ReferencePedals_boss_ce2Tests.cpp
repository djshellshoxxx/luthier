#include "TestFramework.h"
#include "../ReferencePedals/Models/boss_ce2.h"
#include <array>
#include <complex>
using luthier::reference::models::BossCe2;
LUTHIER_TEST(RefPedalCe2, impulseTimingSilenceAndBypass)
{
    for(double sr:{44100.,48000.,96000.,192000.})
    {
        BossCe2 p;p.setParam(1,0);p.prepare(sr,1,1);int at=0;double peak=0;
        for(int i=0;i<int(sr*.012);++i){double l=i==0?.01:0,r=l;p.process(&l,&r,1);if(i>int(sr*.001) && std::abs(l)>peak)peak=std::abs(l),at=i;}
        CHECK_NEAR(at,sr*.0034,sr*.0003);CHECK(peak>1e-5);
        p.reset();double energy=0;for(int i=0;i<1000;++i){double l=0,r=0;p.process(&l,&r,1);energy+=l*l+r*r;}CHECK_NEAR(energy,0,1e-25);
        p.setEnabled(false);for(int i=0;i<int(sr);++i){double l=.1,r=-.2;p.processBypass(&l,&r,1);}
        double l=.125,r=-.25;p.processBypass(&l,&r,1);const double gain=1/(1+std::tan(3.14159265358979323846*15/sr));
        CHECK_NEAR(l,.025*gain,1e-12);CHECK_NEAR(r,-.05*gain,1e-12);
    }
}
LUTHIER_TEST(RefPedalCe2, expressionAndBlockPartition)
{
    BossCe2 a,b;a.prepare(48000,2048,1);b.prepare(48000,2048,1);a.setExpression(0,.8f);b.setExpression(0,.8f);
    CHECK_NEAR(a.getParam(1),.8,1e-6);std::array<double,2048> l{},r{},x{},y{};
    for(int i=0;i<2048;++i)l[i]=r[i]=x[i]=y[i]=.1*std::sin(i*.1);
    a.process(l.data(),r.data(),2048);for(int i=0;i<2048;++i)b.process(x.data()+i,y.data()+i,1);CHECK(l==x);CHECK(r==y);CHECK(l==r);
}
LUTHIER_TEST(RefPedalCe2, lfoRateAndSync)
{
    for(float setting:{0.f,.5f,1.f})
    {
        BossCe2 p;p.setParam(0,setting);p.prepare(48000,1,1);double phase=0;int wraps=0;
        for(int i=0;i<48000*4;++i){double l=0,r=0;p.process(&l,&r,1);if(p.modulationPhase()<phase)++wraps;phase=p.modulationPhase();}
        CHECK_NEAR((wraps+phase)/4,.3*std::pow(3.56/.3,setting),1e-9);
    }
    BossCe2 p;p.setParam(6,1);p.prepare(48000,1,1);luthier::reference::TimeInfo t;t.valid=t.playing=true;t.bpm=120;t.ppq=.25;p.setTime(t);
    CHECK_NEAR(p.modulationPhase(),.25,1e-12);
}

LUTHIER_TEST(RefPedalCe2, originalDistortionGatesWithEstimatedHeadroom)
{
    constexpr double pi=3.14159265358979323846;
    for(double sr:{44100.,48000.,96000.,192000.})for(double db:{-12.,-3.})
    {
        BossCe2 pedal;pedal.setParam(1,0);pedal.prepare(sr,1,1);
        std::array<std::complex<double>,10> harmonics{};const double amplitude=std::pow(10.,db/20);
        for(int i=0;i<int(sr*2);++i)
        {
            const double angle=2*pi*1000*i/sr;double left=amplitude*std::sin(angle),right=left;pedal.process(&left,&right,1);
            if(i>=int(sr))
            {
                const std::complex<double> rotation(std::cos(angle),-std::sin(angle));auto phase=rotation;
                for(int h=1;h<10;++h){harmonics[h]+=left*phase;phase*=rotation;}
            }
        }
        double energy=0;for(int h=2;h<10;++h)energy+=std::norm(harmonics[h]);
        const double thd=std::sqrt(energy)/std::abs(harmonics[1]);
        if(db==-12)CHECK(thd<.003);else{CHECK(thd>=.02);CHECK(thd<=.06);}
    }
}

// Spec section 9: THD at 1 kHz, -12 dBFS with BBD sim ON must be 0.5-3%
// (ratio 0.005 <= thd <= 0.03). Same coherent fixture as the test above:
// two seconds, measured over the last second, harmonics 2..9 over the
// fundamental. Parameters: depth (param1)=0, BBD sim (param4)=1, noise
// (param5)=0. Rate 48000 is measured first, then the specified set.
LUTHIER_TEST(RefPedalCe2, bbdOnOriginalThdGate)
{
    constexpr double pi=3.14159265358979323846;
    for(double sr:{48000.,44100.,88200.,96000.,192000.})
    {
        BossCe2 pedal;pedal.setParam(1,0);pedal.setParam(4,1);pedal.setParam(5,0);pedal.prepare(sr,1,1);
        std::array<std::complex<double>,10> harmonics{};const double amplitude=std::pow(10.,-12./20);
        bool finiteOutput=true;
        for(int i=0;i<int(sr*2);++i)
        {
            const double angle=2*pi*1000*i/sr;double left=amplitude*std::sin(angle),right=left;pedal.process(&left,&right,1);
            if(!std::isfinite(left))finiteOutput=false;
            if(i>=int(sr))
            {
                const std::complex<double> rotation(std::cos(angle),-std::sin(angle));auto phase=rotation;
                for(int h=1;h<10;++h){harmonics[h]+=left*phase;phase*=rotation;}
            }
        }
        double energy=0;for(int h=2;h<10;++h)energy+=std::norm(harmonics[h]);
        const double fundamental=std::abs(harmonics[1]);
        const bool usable=finiteOutput && std::isfinite(fundamental) && fundamental>0;
        CHECK_MSG(usable,"BBD-on THD rate "+juce::String(sr,1)+" Hz: non-finite output or zero fundamental (fundamental="+juce::String(fundamental,9)+")");
        if(!usable)continue;
        const double thd=std::sqrt(energy)/fundamental;
        const juce::String detail="BBD-on THD rate "+juce::String(sr,1)+" Hz: measured ratio "+juce::String(thd,6)
            +" ("+juce::String(thd*100,4)+"%), required 0.005..0.03 (0.5..3%)";
        CHECK_MSG(thd>=.005,detail);
        CHECK_MSG(thd<=.03,detail);
    }
}

// Spec section 9's -3 dBFS clause (2-6%, 3rd harmonic dominant) states no BBD
// mode; the test above covers BBD off. Same coherent fixture with BBD sim on.
LUTHIER_TEST(RefPedalCe2, bbdOnHeadroomGateKeepsThirdHarmonicDominant)
{
    constexpr double pi=3.14159265358979323846;
    for(double sr:{44100.,48000.,88200.,96000.,192000.})
    {
        BossCe2 pedal;pedal.setParam(1,0);pedal.setParam(4,1);pedal.setParam(5,0);pedal.prepare(sr,1,1);
        std::array<std::complex<double>,10> harmonics{};const double amplitude=std::pow(10.,-3./20);
        for(int i=0;i<int(sr*2);++i)
        {
            const double angle=2*pi*1000*i/sr;double left=amplitude*std::sin(angle),right=left;pedal.process(&left,&right,1);
            if(i>=int(sr))
            {
                const std::complex<double> rotation(std::cos(angle),-std::sin(angle));auto phase=rotation;
                for(int h=1;h<10;++h){harmonics[h]+=left*phase;phase*=rotation;}
            }
        }
        double energy=0;for(int h=2;h<10;++h)energy+=std::norm(harmonics[h]);
        const double fundamental=std::abs(harmonics[1]),thd=std::sqrt(energy)/fundamental;
        const juce::String detail="BBD-on -3 dBFS rate "+juce::String(sr,1)+" Hz: THD "+juce::String(thd*100,4)
            +"%, h2 "+juce::String(std::abs(harmonics[2])/fundamental*100,4)+"%, h3 "+juce::String(std::abs(harmonics[3])/fundamental*100,4)+"%";
        CHECK_MSG(thd>=.02,detail);CHECK_MSG(thd<=.06,detail);
        CHECK_MSG(std::abs(harmonics[3])>std::abs(harmonics[2]),detail);
    }
}
