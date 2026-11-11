// SCRATCH PROBE - not committed. Measures wet-path and output THD of the CE-2 model.
#include "TestFramework.h"
#include "../ReferencePedals/Models/boss_ce2.h"
#include "../ReferencePedals/Common/BbdCore.h"
#include "../ReferencePedals/Common/OnePole.h"
#include <array>
#include <complex>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <vector>
using luthier::reference::models::BossCe2;
namespace
{
constexpr double probePi=3.14159265358979323846;
struct Harmonics
{
    std::array<std::complex<double>,10> h{};double dc=0;int n=0;
    void add(double v,double angle) noexcept
    {
        const std::complex<double> rotation(std::cos(angle),-std::sin(angle));auto phase=rotation;
        for(int k=1;k<10;++k){h[(size_t)k]+=v*phase;phase*=rotation;}
        dc+=v;++n;
    }
    double thd() const noexcept {double e=0;for(int k=2;k<10;++k)e+=std::norm(h[(size_t)k]);return std::sqrt(e)/std::abs(h[1]);}
    double rel(int k) const noexcept {return std::abs(h[(size_t)k])/std::abs(h[1]);}
    double amp(int k) const noexcept {return 2*std::abs(h[(size_t)k])/n;}
    double mean() const noexcept {return dc/n;}
};
}
LUTHIER_TEST(Ce2BbdProbe, baseline)
{
    using namespace luthier::reference;
    std::printf("PROBE baseline: db bbd rate | outTHD%% wetTHD%% | out1/in wet1/dry1 phase(wet/dry)deg | B=out/wetTHD out1/wet1 | wet h2%% h3%% out h2%% h3%% | dcOut reconErr\n");
    for(double db:{-12.,-3.})for(bool bbd:{true,false})for(double sr:{48000.,44100.,88200.,96000.,192000.})
    {
        BossCe2 pedal;pedal.setParam(1,0);pedal.setParam(4,bbd?1.f:0.f);pedal.setParam(5,0);pedal.prepare(sr,1,1);
        common::BbdCore core;core.prepare(sr,12,6500,5500);core.setClipOnset(.5);core.reset(1);
        common::OnePole hp;hp.setLowpass(sr,15);
        Harmonics out,wet,dry;double reconstruction=0;const double amplitude=std::pow(10.,db/20);
        for(int i=0;i<int(sr*2);++i)
        {
            const double angle=2*probePi*1000*i/sr,x=amplitude*std::sin(angle);
            double left=x,right=x;pedal.process(&left,&right,1);
            const double buffered=hp.highpass(x),w=core.process(buffered,3.4,0,bbd);
            reconstruction=std::max(reconstruction,std::abs(buffered+.9440608762859234*w-left));
            if(i>=int(sr)){out.add(left,angle);wet.add(w,angle);dry.add(buffered,angle);}
        }
        const auto ratio=wet.h[1]/dry.h[1];
        std::printf("PROBE %5.0f %d %6.0f | %.6f %.6f | %.5f %.5f %8.3f | %.4f %.5f | %.5f %.5f %.5f %.5f | %.3e %.3e\n",
            db,int(bbd),sr,out.thd()*100,wet.thd()*100,out.amp(1)/amplitude,std::abs(ratio),std::arg(ratio)*180/probePi,
            wet.thd()>0?out.thd()/wet.thd():0.,std::abs(out.h[1])/std::abs(wet.h[1]),
            wet.rel(2)*100,wet.rel(3)*100,out.rel(2)*100,out.rel(3)*100,out.mean(),reconstruction);
        CHECK(reconstruction<1e-12);
    }
}
#include "../ReferencePedals/Models/boss_ce2w.h"
namespace
{
struct Fnv
{
    uint64_t state=1469598103934665603ull;
    void add(double v) noexcept
    {
        unsigned char bytes[sizeof(double)];std::memcpy(bytes,&v,sizeof(double));
        for(unsigned char b:bytes){state^=b;state*=1099511628211ull;}
    }
};
struct ProbeInput
{
    uint32_t lcg=12345;double sr=48000;int i=0;
    void next(double& l,double& r) noexcept
    {
        lcg=lcg*1664525u+1013904223u;const double noise=double(lcg>>8)/8388608.0-1;
        const double t=i++/sr;
        l=.6*std::sin(2*probePi*220*t)+.3*std::sin(2*probePi*1570*t)+.2*noise;
        r=.5*std::sin(2*probePi*331*t)+.35*std::sin(2*probePi*2210*t)-.15*noise;
    }
};
template<class P,class Setup> uint64_t renderHash(double sr,Setup&& setup)
{
    P pedal;setup(pedal);pedal.prepare(sr,64,1);Fnv hash;ProbeInput input;input.sr=sr;
    std::array<double,64> l{},r{};
    for(int block=0;block<int(sr*.5/64);++block)
    {
        for(int k=0;k<64;++k)input.next(l[(size_t)k],r[(size_t)k]);
        pedal.process(l.data(),r.data(),64);
        for(int k=0;k<64;++k){hash.add(l[(size_t)k]);hash.add(r[(size_t)k]);}
    }
    return hash.state;
}
}
LUTHIER_TEST(Ce2BbdProbe, hashes)
{
    using namespace luthier::reference::models;
    for(double sr:{44100.,48000.,88200.,96000.,192000.})
    {
        for(int bbd=0;bbd<2;++bbd)for(int cfg=0;cfg<4;++cfg)
        {
            const auto h=renderHash<BossCe2>(sr,[&](BossCe2& p)
            {
                p.setParam(4,float(bbd));
                if(cfg==0){p.setParam(1,0);}
                if(cfg==1){p.setParam(1,1);p.setParam(0,1);}
                if(cfg==2){p.setParam(1,.5f);p.setParam(3,1);p.setParam(5,1);}
                if(cfg==3){p.setParam(1,.7f);p.setParam(13,1);p.setParam(12,0);}
            });
            std::printf("HASH ce2 sr=%.0f bbd=%d cfg=%d %016llx\n",sr,bbd,cfg,(unsigned long long)h);
        }
        for(int mode=0;mode<3;++mode)for(int stair=0;stair<2;++stair)for(int noise=0;noise<2;++noise)
        {
            const auto h=renderHash<BossCe2w>(sr,[&](BossCe2w& p)
            {
                p.setParam(2,mode*.5f);p.setParam(8,float(stair));p.setParam(7,float(noise));p.setParam(1,.8f);
            });
            std::printf("HASH ce2w sr=%.0f mode=%d stair=%d noise=%d %016llx\n",sr,mode,stair,noise,(unsigned long long)h);
        }
    }
    CHECK(true);
}
LUTHIER_TEST(Ce2BbdProbe, cpu)
{
    // Spec section 4 fixture shape: 48 kHz, 512-sample blocks, stereo, 480000 samples (10 s).
    // level .25 keeps peaks below the clip onset (guitar-like); level 1 drives the limiter.
    using namespace luthier::reference::models;
    constexpr int block=512,total=480000;
    std::vector<double> inL((size_t)total),inR((size_t)total);ProbeInput input;
    for(int i=0;i<total;++i){double l,r;input.next(l,r);inL[(size_t)i]=l;inR[(size_t)i]=r;}
    std::vector<double> l((size_t)block),r((size_t)block);
    for(int pass=0;pass<2;++pass)for(double level:{.25,1.})for(int bbd=0;bbd<2;++bbd)
    {
        double runs[3]{};
        for(int run=0;run<3;++run)
        {
            BossCe2 pedal;pedal.setParam(4,float(bbd));pedal.prepare(48000,block,1);double sink=0;
            const double start=luthier::tests::threadCpuTimeSeconds();
            for(int at=0;at+block<=total;at+=block)
            {
                for(int k=0;k<block;++k){l[(size_t)k]=level*inL[(size_t)(at+k)];r[(size_t)k]=level*inR[(size_t)(at+k)];}
                pedal.process(l.data(),r.data(),block);sink+=l[0]+r[(size_t)block-1];
            }
            runs[run]=(luthier::tests::threadCpuTimeSeconds()-start)/(total/48000.)*100;
            if(!std::isfinite(sink))runs[run]=-1;
        }
        std::printf("CPU ce2 pass=%d level=%.2f bbd=%d percent-of-one-core runs: %.4f %.4f %.4f (min %.4f)\n",pass,level,bbd,runs[0],runs[1],runs[2],
            std::min({runs[0],runs[1],runs[2]}));
    }
    CHECK(true);
}
LUTHIER_TEST(Ce2BbdProbe, levels)
{
    // After-change table: wet/out THD and h2/h3 at several levels, BBD on.
    using namespace luthier::reference;
    const double reference=std::sqrt(2.)*.78*9/15;
    for(double amplitude:{std::pow(10.,-18./20),std::pow(10.,-12./20),std::pow(10.,-6./20),reference,std::pow(10.,-3./20),1.,2.})
        for(double sr:{48000.,44100.,88200.,96000.,192000.})
    {
        BossCe2 pedal;pedal.setParam(1,0);pedal.setParam(4,1);pedal.setParam(5,0);pedal.prepare(sr,1,1);
        common::OnePole hp;hp.setLowpass(sr,15);Harmonics out,wet;double peak=0;
        for(int i=0;i<int(sr*2);++i)
        {
            const double angle=2*probePi*1000*i/sr,x=amplitude*std::sin(angle);
            double left=x,right=x;pedal.process(&left,&right,1);
            const double w=(left-hp.highpass(x))/.9440608762859234;
            if(i>=int(sr)){out.add(left,angle);wet.add(w,angle);peak=std::max(peak,std::abs(w));}
        }
        std::printf("LEVEL %8.3f dBFS %6.0f | wetTHD %.5f%% h2 %.5f%% h3 %.5f%% | outTHD %.5f%% h2 %.5f%% h3 %.5f%% h3/h2 %.4f | dcOut %.3e wetPeak %.5f\n",
            20*std::log10(amplitude),sr,wet.thd()*100,wet.rel(2)*100,wet.rel(3)*100,out.thd()*100,out.rel(2)*100,out.rel(3)*100,
            out.rel(2)>0?out.rel(3)/out.rel(2):0.,out.mean(),peak);
    }
    CHECK(true);
}
LUTHIER_TEST(Ce2BbdProbe, differential)
{
    // Curvature contribution isolated as curved minus plain clocked core, CE-2 configuration.
    using namespace luthier::reference;
    const double curvature=2*.005/(std::sqrt(2.)*.78*9/15);
    for(double hz:{5000.,1000.})for(double level:{1.,.25})for(int mode=0;mode<2;++mode)for(double sr:{44100.,48000.,88200.,96000.,192000.})
    {
        // mode 0: 3.4 ms (hold varies per sample); mode 1: clock at an integer multiple of the host rate (hold time-invariant).
        const double multiple=std::ceil(150588./sr),ms=mode==0?3.4:512000./(sr*multiple);
        common::BbdCore plain,curved;
        for(auto* core:{&plain,&curved}){core->prepare(sr,12,6500,5500);core->setClipOnset(.5);}
        curved.setTransferCurvature(curvature);
        const int lines=int(sr/2/hz);std::vector<std::complex<double>> projection((size_t)lines+1);
        std::complex<double> fundamental;double mean=0,power=0,plainPower=0;std::vector<std::complex<double>> plainProjection((size_t)lines+1);
        const int settle=int(sr),total=int(2*sr);
        for(int i=0;i<total;++i)
        {
            const double angle=2*probePi*hz*i/sr,x=level*std::sin(angle);
            const double p=plain.process(x,ms,0,true),difference=curved.process(x,ms,0,true)-p;
            if(i>=settle)
            {
                const std::complex<double> rotation(std::cos(angle),-std::sin(angle));std::complex<double> phase(1,0);
                for(int k=0;k<=lines;++k){projection[(size_t)k]+=difference*phase;plainProjection[(size_t)k]+=p*phase;phase*=rotation;}
                fundamental+=p*rotation;power+=difference*difference;mean+=difference;plainPower+=p*p;
            }
        }
        const double n=total-settle;
        double harmonicPower=std::norm(projection[0])/(n*n),plainHarmonic=std::norm(plainProjection[0])/(n*n);
        for(int k=1;k<=lines;++k){harmonicPower+=2*std::norm(projection[(size_t)k])/(n*n);plainHarmonic+=2*std::norm(plainProjection[(size_t)k])/(n*n);}
        const double amplitude=2*std::abs(fundamental)/n;
        const double residual=std::sqrt(std::max(0.,power/n-harmonicPower)),plainResidual=std::sqrt(std::max(0.,plainPower/n-plainHarmonic));
        std::printf("DIFF hz=%.0f level=%.2f mode=%d sr=%6.0f ms=%.6f | wetFund %.5f | diff mean %.3e rms %.3e | nonharmonic rms %.3e (%.1f dB re fund) | plain nonharmonic %.3e (%.1f dB)\n",
            hz,level,mode,sr,ms,amplitude,mean/n,std::sqrt(power/n),residual,20*std::log10(std::max(1e-300,residual/amplitude)),
            plainResidual,20*std::log10(std::max(1e-300,plainResidual/amplitude)));
    }
    CHECK(true);
}
