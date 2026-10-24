#include "Source/ReferencePedals/Models/mxr_phase90_script.h"
#include <complex>
#include <cstdio>
#include <ctime>
#include <vector>
using namespace luthier::reference;
constexpr double pi=3.14159265358979323846;
void freezeAt(models::MxrPhase90Script& p,double position,TimeInfo& time)
{
    time.valid=time.playing=true;time.bpm=120;time.ppq=position*.65;
    p.setEnabled(true);p.setParam(5,0);p.setParam(6,1);p.setTime(time);
}
std::complex<double> oracle(double sr,double fc,double frequency)
{
    const double u=std::tan(pi*fc*std::tan(pi/8)/sr),v=std::tan(pi*fc*std::tan(3*pi/8)/sr);
    const double s=v-u,p=u*v,d=1+s+p,a1=2*(p-1)/d,a2=(1-s+p)/d;
    const auto z=std::exp(std::complex<double>(0,-2*pi*frequency/sr));
    const auto ap=(a2+a1*z+z*z)/(1.+a1*z+a2*z*z);
    const double g=std::tan(pi*1.6/sr),a=(g-1)/(g+1),b=g/(g+1);
    return .5*(1.+ap*ap)*(1.-b*(1.+z)/(1.+a*z));
}
int main()
{
    for(double sr:{44100.,48000.,88200.,96000.,192000.})for(double position:{0.,.5,1.})
    {
        models::MxrPhase90Script p;TimeInfo time;freezeAt(p,position,time);p.prepare(sr,1,1);
        for(int i=0;i<int(sr);++i){double l=0,r=0;p.setTime(time);p.process(&l,&r,1);}
        const double fc=330*std::pow(10.,position);
        std::vector<double> frequencies{fc*std::tan(pi/8),fc*std::tan(3*pi/8),20,40,80,160,320,640,1000,2000,4000,6500,10000,14000};
        std::vector<std::complex<double>> sums(frequencies.size()),phase(frequencies.size(),1),step;
        for(double f:frequencies)step.push_back(std::exp(std::complex<double>(0,-2*pi*f/sr)));
        double rightDifference=0;
        for(int i=0;i<int(sr*2);++i){double l=i==0?1e-6:0,r=l;p.setTime(time);p.process(&l,&r,1);rightDifference=std::max(rightDifference,std::abs(l-r));for(size_t k=0;k<frequencies.size();++k){sums[k]+=l*phase[k];phase[k]*=step[k];}}
        double maxDb=0,maxComplex=0;for(size_t k=2;k<frequencies.size();++k){const auto expected=oracle(sr,fc,frequencies[k]),actual=sums[k]/1e-6;maxDb=std::max(maxDb,std::abs(20*std::log10(std::abs(actual)/std::abs(expected))));maxComplex=std::max(maxComplex,std::abs(actual-expected));}
        std::printf("FR sr%.0f fc%.9f notch_residual %.9g %.9g max_db_error %.9g complex_error %.9g stereo_difference %.9g latency%d\n",sr,fc,std::abs(sums[0])/1e-6,std::abs(sums[1])/1e-6,maxDb,maxComplex,rightDifference,p.getLatencySamples());
    }
    for(double sr:{44100.,48000.,88200.,96000.,192000.})for(double dbv:{-20.,-6.})
    {
        models::MxrPhase90Script p;TimeInfo time;freezeAt(p,.5,time);p.prepare(sr,1,1);
        const double peak=std::sqrt(2.)*std::pow(10.,(dbv-6.218487496)/20.);
        std::complex<double> sums[10]{},phases[10],steps[10];for(int k=0;k<10;++k){phases[k]=1;steps[k]=std::exp(std::complex<double>(0,-2*pi*1000*(k+1)/sr));}
        for(int i=0;i<int(sr*2);++i){double l=peak*std::sin(2*pi*1000*i/sr),r=l;p.setTime(time);p.process(&l,&r,1);for(int k=0;k<10;++k){if(i>=int(sr))sums[k]+=l*phases[k];phases[k]*=steps[k];}}
        double harmonics=0;for(int k=1;k<10;++k)harmonics+=std::norm(sums[k]);std::printf("THD sr%.0f dbv%.0f peak%.12f percent%.9f\n",sr,dbv,peak,100*std::sqrt(harmonics/std::norm(sums[0])));
    }
    for(int trial=0;trial<5;++trial)for(float stereo:{0.f,1.f})
    {
        models::MxrPhase90Script p;p.setEnabled(true);p.setParam(8,stereo);p.prepare(48000,128,1);
        std::vector<double> left(480000),right(left.size());for(int i=0;i<int(left.size());++i){left[i]=.1*std::sin(i*.13);right[i]=.1*std::cos(i*.17);}
        timespec a{},b{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&a);for(int i=0;i<int(left.size());i+=128)p.process(left.data()+i,right.data()+i,std::min(128,int(left.size())-i));clock_gettime(CLOCK_THREAD_CPUTIME_ID,&b);
        const double seconds=b.tv_sec-a.tv_sec+(b.tv_nsec-a.tv_nsec)*1e-9;std::printf("CPU trial%d stereo_phase%.0f %.9f%% checksum%.12g\n",trial,stereo*180,seconds*10,left.back()+right.back());
    }
}
