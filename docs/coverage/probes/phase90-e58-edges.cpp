#include "Source/ReferencePedals/Models/mxr_phase90_script.h"
#include <complex>
#include <cstdio>
using namespace luthier::reference::models;
constexpr double pi=3.14159265358979323846;
int main()
{
    for(double sr:{44100.,48000.,88200.,96000.,192000.})for(double peak:{.069117914080,.346410161520,1.})
    {
        MxrPhase90Script p;p.setEnabled(true);p.setParam(3,0);p.setParam(5,0);p.prepare(sr,1,1);
        std::complex<double> bins[32]{},phases[32],steps[32];for(int h=1;h<32;h+=2){phases[h]=1;steps[h]=std::exp(std::complex<double>(0,-2*pi*5000*h/sr));}
        for(int i=0;i<int(sr*2);++i){double l=peak*std::sin(2*pi*5000*i/sr),r=l;p.process(&l,&r,1);for(int h=1;h<32;h+=2){if(i>=int(sr))bins[h]+=l*phases[h];phases[h]*=steps[h];}}
        double largest=0;int harmonic=0;for(int h=3;h<32;h+=2)if(h*5000>sr*.5&&std::abs(bins[h])>largest){largest=std::abs(bins[h]);harmonic=h;}
        std::printf("alias sr%.0f peak%.12f worst_folded_odd%d %.9fdBc\n",sr,peak,harmonic,20*std::log10(std::max(1e-30,largest)/std::abs(bins[1])));
    }
    for(double sr:{44100.,48000.,88200.,96000.,192000.})
    {
        MxrPhase90Script p,wet;for(auto* q:{&p,&wet}){q->setParam(3,0);q->setParam(5,0);q->prepare(sr,1,1);}wet.setEnabled(true);
        double firstDelta=0,expectedError=0,bypassError=0,oldActual=0,oldWet=0,oldDry=0,maxStep=0,maxNatural=0;
        for(int i=0;i<int(sr*1.1);++i){if(i==int(sr))p.setEnabled(true);double input=.1*std::sin(2*pi*1000*i/sr),l=input,r=input,a=input,b=input;p.process(&l,&r,1);wet.process(&a,&b,1);
            if(i>=int(sr)){maxStep=std::max(maxStep,std::abs(l-oldActual));maxNatural=std::max(maxNatural,std::max(std::abs(a-oldWet),std::abs(input-oldDry)));}oldActual=l;oldWet=a;oldDry=input;
            if(i<int(sr))bypassError=std::max(bypassError,std::abs(l-input));else{double fade=std::min(1.,(i-int(sr)+1)/(sr*.005));expectedError=std::max(expectedError,std::abs(l-(input+(a-input)*fade)));if(i==int(sr))firstDelta=std::abs(l-input);}}
        std::printf("bypass sr%.0f off_error%.12g ramp_residual%.12g first_fade_step%.12g original_24bit_LSB%.12g A_PED_step_ratio%.9f\n",sr,bypassError,expectedError,firstDelta,std::ldexp(1.,-23),maxStep/maxNatural);
    }
}
