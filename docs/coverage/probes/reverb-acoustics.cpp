#include "Source/ReferencePedals/Generic/Reverb.h"
#include <cstdio>
#include <vector>
using namespace luthier::reference;
constexpr double pi=3.14159265358979323846;
int main()
{
    // Calibration is already in the actual wrapper, never applied twice.
    constexpr double correction[]{1,1,1,1,1};
    for(double sr:{44100.,48000.,88200.,96000.,192000.})for(int stimulus=0;stimulus<3;++stimulus)
    {
        std::vector<double> input(size_t(sr*8));common::Biquad low;low.setLowpass(sr,4000);uint32_t rng=42;double inputEnergy=0;
        for(int i=0;i<int(input.size());++i)
        {
            double x=0;
            if(stimulus==0){rng=1664525u*rng+1013904223u;x=low.process(double(rng)/4294967296.-.5);}
            else if(stimulus==1){for(double f:{82.406889,110.,146.832384,196.,246.941651,329.627557})for(int h=1;h<=8;++h)x+=std::sin(2*pi*f*h*i/sr)/(h*h);}
            else x=std::sin(2*pi*1000*i/sr);
            input[size_t(i)]=x;if(i>=int(sr*5))inputEnergy+=x*x;
        }
        const double scale=std::pow(10.,-18./20)/std::sqrt(inputEnergy/(sr*3));for(auto& x:input)x*=scale;
        for(int type=0;type<5;++type)
        {
            generic::Reverb p;p.setParam(5,type*.25f);p.setParam(1,1);p.prepare(sr,1,1);
            double dry=0,wet=0,mono=0,left=0,defaultMix=0;
            for(int i=0;i<int(input.size());++i){double x=input[size_t(i)],l=x,r=x;p.process(&l,&r,1);if(i>=int(sr*5)){dry+=x*x;wet+=(l*l+r*r)*.5;mono+=(l+r)*(l+r);left+=l*l;const double a=std::cos(.3*pi*.5)*x+std::sin(.3*pi*.5)*correction[type]*l,b=std::cos(.3*pi*.5)*x+std::sin(.3*pi*.5)*correction[type]*r;defaultMix+=(a*a+b*b)*.5;}}
            std::printf("level sr%.0f stimulus%d type%d actual_wet_db%.6f actual_wet_db_check%.6f actual_default_mix_db%.6f literal_sum_vs_L_db%.6f\n",sr,stimulus,type,10*std::log10(wet/dry),10*std::log10(wet/dry)+20*std::log10(correction[type]),10*std::log10(defaultMix/dry),10*std::log10(mono/left));
        }
    }
    for(double sr:{44100.,48000.,88200.,96000.,192000.})for(bool bandlimited:{false,true})
    {
        generic::Reverb p;p.setParam(1,1);p.prepare(sr,1,1);common::Biquad low[2];low[0].setLowpass(sr,20000,.541196100146197);low[1].setLowpass(sr,20000,1.30656296487638);
        std::vector<double> output[2];for(auto& x:output)x.resize(size_t(sr*.3));
        for(int i=0;i<int(output[0].size());++i){double x=i==0?1:0;if(bandlimited)for(auto& f:low)x=f.process(x);double l=x,r=x;p.process(&l,&r,1);output[0][size_t(i)]=l;output[1][size_t(i)]=r;}
        for(double time:{.1,.15,.2,.25})
        {
            const int first=int((time-.01)*sr),last=int((time+.01)*sr);double ned[2]{};
            for(int c=0;c<2;++c){double mean=0,variance=0;for(int i=first;i<last;++i)mean+=output[c][size_t(i)];mean/=last-first;for(int i=first;i<last;++i)variance+=std::pow(output[c][size_t(i)]-mean,2);const double deviation=std::sqrt(variance/(last-first));int above=0;for(int i=first;i<last;++i)if(std::abs(output[c][size_t(i)]-mean)>deviation)++above;ned[c]=double(above)/(last-first)/.317310507862914;}
            std::printf("density sr%.0f bandlimit20k%d time%.3f L%.9f R%.9f\n",sr,bandlimited,time,ned[0],ned[1]);
        }
    }
}
