// Standalone E58 experiment. No production model or registered descriptor changes.
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <ctime>
#include <vector>
constexpr double pi=3.14159265358979323846;
using Coeff=std::array<double,4>;
Coeff coefficients(double sr,double fc,bool revised,bool grouped=false)
{
    if(!revised){double g=std::tan(pi*fc/sr),a=(g-1)/(g+1);return {a,a,a,a};}
    const double u=std::tan(pi*fc*std::tan(pi/8)/sr),v=std::tan(pi*fc*std::tan(3*pi/8)/sr);
    const double s=v-u,p=u*v,d=std::sqrt(std::max(0.,s*s-4*p));
    const double r=(s+d)*.5,t=p/r; // stable smaller-root evaluation
    const double a=(r-1)/(r+1),b=(t-1)/(t+1);
    return grouped ? Coeff{a,a,b,b}:Coeff{a,b,a,b};
}
std::complex<double> transfer(const Coeff& a,double f,double sr)
{
    const auto z=std::exp(std::complex<double>(0,-2*pi*f/sr));std::complex<double> h=1;
    for(double c:a)h*=(c+z)/(1.+c*z);return h;
}
struct Core
{
    Coeff a{},step{},z{};double sr=48000,phase=0,tri=.5,tc=0,feedback=0,amount=0,rate=1.5;
    int clock=0;bool revised=false,grouped=false,moving=false;
    void prepare(double fs,double fc,bool revision,bool order=false){sr=fs;revised=revision;grouped=order;a=coefficients(sr,fc,revised,grouped);tc=1-std::exp(-1/(sr*.02));}
    double process(double input)
    {
        if(moving){tri+=((phase<.65?phase/.65:(1-phase)/.35)-tri)*tc;
            if(!clock){auto target=coefficients(sr,330*std::pow(10.,tri),revised,grouped);for(int k=0;k<4;++k)step[k]=(target[k]-a[k])*.125;}
            for(int k=0;k<4;++k)a[k]+=step[k];clock=(clock+1)&7;phase+=rate/sr;if(phase>=1)phase-=1;}
        double x=input+amount*std::tanh(feedback);
        for(int k=0;k<4;++k){double y=a[k]*x+z[k];z[k]=x-a[k]*y;if(std::abs(z[k])<1e-30)z[k]=0;x=y/std::sqrt(1+.04*y*y);}
        feedback=x;return .5*(input+x);
    }
};
double thd(double sr,double fc,bool revised,bool grouped,double peak)
{
    Core c;c.prepare(sr,fc,revised,grouped);std::array<std::complex<double>,10> bins{};
    for(int i=0;i<int(2*sr);++i){double y=c.process(peak*std::sin(2*pi*1000*i/sr));if(i>=int(sr))for(int h=1;h<=10;++h)bins[h-1]+=y*std::exp(std::complex<double>(0,-2*pi*1000*h*i/sr));}
    double sum=0;for(int h=1;h<10;++h)sum+=std::norm(bins[h]);return 100*std::sqrt(sum/std::norm(bins[0]));
}
int main(int argc,char** argv)
{
    FILE* curves=argc>1?std::fopen(argv[1],"w"):nullptr;
    if(curves)std::fprintf(curves,"sr,fc,hz,old_phase_deg,new_phase_deg,old_mix_db,new_mix_db\n");
    for(double sr:{44100.,48000.,88200.,96000.,192000.})for(double fc:{330.,330*std::sqrt(10.),3300.})
    {
        const auto old=coefficients(sr,fc,false),next=coefficients(sr,fc,true);
        double maxMagnitude=0,maxPhase=0,previousOld=0,previousNew=0,unwrappedOld=0,unwrappedNew=0;
        for(int i=0;i<=2000;++i){double f=20*std::pow(700.,i/2000.);auto a=transfer(old,f,sr),b=transfer(next,f,sr);
            double pa=std::arg(a),pb=std::arg(b);if(!i){unwrappedOld=pa;unwrappedNew=pb;}else{unwrappedOld+=std::remainder(pa-previousOld,2*pi);unwrappedNew+=std::remainder(pb-previousNew,2*pi);}previousOld=pa;previousNew=pb;
            maxMagnitude=std::max(maxMagnitude,std::abs(std::abs(b)-1));maxPhase=std::max(maxPhase,std::abs(unwrappedOld-unwrappedNew)*180/pi);
            if(curves)std::fprintf(curves,"%.0f,%.9f,%.9f,%.12f,%.12f,%.12f,%.12f\n",sr,fc,f,unwrappedOld*180/pi,unwrappedNew*180/pi,20*std::log10(std::abs(.5*(1.+a))),20*std::log10(std::abs(.5*(1.+b))));}
        std::printf("sr %.0f fc %.6f old_a %.12g new_a %.12g %.12g wet_mag_error %.3g max_phase_delta_deg %.6f\n",sr,fc,old[0],next[0],next[1],maxMagnitude,maxPhase);
        for(double t:{std::tan(pi/8),std::tan(3*pi/8)}){double target=fc*t,prior=sr/pi*std::atan(std::tan(pi*fc/sr)*t);std::printf(" target %.9f old %.9f old_error_pct %.6f new_residual %.3g\n",target,prior,100*(prior/target-1),std::abs(.5*(1.+transfer(next,target,sr))));}
        if(fc==3300.)for(double dbv:{-20.,-6.}){double peak=std::sqrt(2.)*std::pow(10.,(dbv-6.218487496)/20.);std::printf(" thd dbv %.0f peak %.12f old %.9f new_alternating %.9f new_grouped %.9f percent\n",dbv,peak,thd(sr,fc,false,false,peak),thd(sr,fc,true,false,peak),thd(sr,fc,true,true,peak));}
    }
    if(curves)std::fclose(curves);
    for(double sr:{44100.,48000.,88200.,96000.,192000.})for(bool revision:{false,true}){Core c;c.prepare(sr,1044,revision);c.moving=true;c.amount=.6;c.rate=16;double peak=0,tail=0;bool finite=true;for(int i=0;i<int(sr*60);++i){double y=c.process(i==0?1.:0.);finite=finite&&std::isfinite(y);peak=std::max(peak,std::abs(y));if(i>int(sr*59))tail+=y*y;}std::printf("stability sr%.0f revised%d finite%d peak%.12g tail%.12g\n",sr,revision,finite,peak,tail);}
    std::vector<double> inputs(480000);for(int i=0;i<int(inputs.size());++i)inputs[i]=.1*std::sin(i*.13);
    for(bool revision:{false,true}){Core c[2];for(auto& v:c){v.prepare(48000,1044,revision);v.moving=true;}timespec start{},end{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&start);double sum=0;for(double x:inputs)for(auto& v:c)sum+=v.process(x);clock_gettime(CLOCK_THREAD_CPUTIME_ID,&end);double seconds=end.tv_sec-start.tv_sec+(end.tv_nsec-start.tv_nsec)*1e-9;std::printf("core stereo revised%d CPU %.6f%% checksum %.12g\n",revision,seconds*10,sum);}
}
