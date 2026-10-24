#include "../../../Source/ReferencePedals/Models/ibanez_ts808.h"
#include "../../../Source/ReferencePedals/Models/boss_ds1.h"
#include "../../../Source/ReferencePedals/Models/boss_sd1.h"
#include <complex>
#include <cstdio>
#include <fstream>
#include <ctime>
using namespace luthier::reference;
void measure(IPedal& p)
{
    p.setParam(7,0);
    for(double sr:{44100.,48000.,96000.,192000.})for(double drive:{0.,.2,.5,1.})
    {
        p.setParam(0,drive);p.prepare(sr,128,1);
        std::complex<double> bins[10]{};double mean=0,energy=0;
        for(int i=0;i<int(2*sr);++i)
        {
            double x=.1*std::sin(2*common::pi*1000*i/sr),r=x;p.process(&x,&r,1);
            if(i>=int(sr)){mean+=x;energy+=x*x;for(int h=1;h<=10;++h)if(h*1000<sr/2)bins[h-1]+=x*std::exp(std::complex<double>(0,-2*common::pi*1000*h*i/sr));}
        }
        double harmonics=0;for(int h=1;h<10;++h)harmonics+=std::norm(bins[h]);
        std::printf("%s sr%.0f drive%.2f input -20dBFS peak: outRMS %.9fdBFS gain %.9fdB THD %.9f%% H2/H3 %.9fdB mean %.12g latency %d\n",p.desc().modelId,sr,drive,10*std::log10(energy/sr),10*std::log10(energy/sr/.005),100*std::sqrt(harmonics/std::norm(bins[0])),20*std::log10(std::abs(bins[1])/std::abs(bins[2])),mean/sr,p.getLatencySamples());
    }
    p.setParam(0,1);p.prepare(48000,1,1);std::ofstream raw(std::string("/tmp/")+p.desc().modelId+"-5k.raw",std::ios::binary);
    for(int i=0;i<96000;++i){double x=std::sin(2*common::pi*5000*i/48000),r=x;p.process(&x,&r,1);if(i>=48000)raw.write((char*)&x,sizeof x);}
    p.setParam(0,.5);p.prepare(48000,128,1);double l[128]{},r[128]{};timespec a{},b{};double sum=0;
    // Exclude oscillator generation and checksum work from measured callback time.
    for(int i=0;i<128;++i){l[i]=.1*std::sin(i*.137);r[i]=.07*std::cos(i*.173);}
    double seconds=0;
    for(int j=0;j<750;++j){double x[128],y[128];std::copy(l,l+128,x);std::copy(r,r+128,y);clock_gettime(CLOCK_THREAD_CPUTIME_ID,&a);p.process(x,y,128);clock_gettime(CLOCK_THREAD_CPUTIME_ID,&b);seconds+=b.tv_sec-a.tv_sec+(b.tv_nsec-a.tv_nsec)*1e-9;sum+=x[127]+y[127];}
    std::printf("%s 48k stereo block128 normal4x process-only threadCPU %.6f%% checksum %.12g\n",p.desc().modelId,seconds*50,sum);
}
int main(){models::IbanezTs808 ts;models::BossDs1 ds;models::BossSd1 sd;measure(ts);measure(ds);measure(sd);}
