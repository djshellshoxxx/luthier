#include "Source/ReferencePedals/Common/BbdCore.h"
#include "Source/ReferencePedals/Models/boss_ce2.h"
#include "Source/ReferencePedals/Models/boss_ce2w.h"
#include "Source/ReferencePedals/Models/boss_dd3t.h"
#include "Source/ReferencePedals/Models/mxr_phase90_script.h"
#include <ctime>
#include <cstdio>
#include <complex>
using namespace luthier::reference;
constexpr double pi=3.14159265358979323846;
template<class P> void cpu(const char* name,int block)
{
    P p;p.setEnabled(true);p.prepare(48000,block,1);
    std::vector<double> left(480000),right(480000);
    for(int i=0;i<480000;++i)left[i]=right[i]=.1*std::sin(i*.13);
    double sink=0;timespec start{},finish{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID,&start);
    for(int at=0;at+block<=480000;at+=block){p.process(left.data()+at,right.data()+at,block);sink+=left[at];}
    clock_gettime(CLOCK_THREAD_CPUTIME_ID,&finish);
    const double seconds=finish.tv_sec-start.tv_sec+1e-9*(finish.tv_nsec-start.tv_nsec);
    std::printf("CPU %s %.5f%% sink%g\n",name,seconds*10,sink);
}
int main(){
for(double sr:{44100.,48000.,96000.,192000.}){
 for(double f:{1000.,5000.,6500.,10000.,14000.}){common::BbdCore p;p.prepare(sr,12,6500,5500);p.setClipOnset(.5);std::complex<double> a{};for(int i=0;i<int(sr*2);++i){double y=p.process(.001*std::sin(2*pi*f*i/sr),3.4,0,false);if(i>=int(sr))a+=y*std::exp(std::complex<double>(0,-2*pi*f*i/sr));}printf("BBD gain sr%.0f f%.0f %.7fdB\n",sr,f,20*log10(abs(a)*2/sr/.001));}
 common::BbdCore p;p.prepare(sr,12,6500,5500);p.setClipOnset(.5);std::complex<double> bins[12]{};for(int i=0;i<int(sr*2);++i){double y=p.process(std::sin(2*pi*5000*i/sr),3.4,0,false);if(i>=int(sr))for(int h=1;h<12;h+=2)bins[h]+=y*std::exp(std::complex<double>(0,-2*pi*5000*h*i/sr));}for(int h=3;h<12;h+=2)if(h*5000>sr/2)printf("BBD alias sr%.0f harmonic%d %.3fdBc\n",sr,h,20*log10(abs(bins[h])/abs(bins[1])));
}
cpu<models::BossCe2>("CE2",512);cpu<models::BossCe2w>("CE2W",128);cpu<models::BossDd3t>("DD3T",512);cpu<models::MxrPhase90Script>("Phase90",128);
}
