// OWNER: WP10
#include "ReverbFdn.h"
#include <cmath>
#include <algorithm>
namespace luthier::reference::generic
{
void ReverbFdn::prepare(double sampleRate)
{
    sr=sampleRate; settingsValid=false;
    for(auto& line:lines) line.prepare(int(sr*.25)+16);
    for(auto& line:diffusion) line.prepare(int(sr*.03)+16);
    lfo.prepare(sr); lfo.setRate(.31); reset(); set(2.2,1,0,.25,1,0);
}
void ReverbFdn::reset() noexcept
{
    for(auto& line:lines) line.reset(); for(auto& line:diffusion) line.reset();
    for(auto& filter:damping) filter.reset(); lfo.setPhase(0);
}
void ReverbFdn::set(double decay,double size,double tone,double modulation,int family,double frozen) noexcept
{
    constexpr double room[]{.0127,.0179,.0233,.0299,.0317,.0371,.0419,.0449};
    constexpr double hall[]{.0301,.0373,.0439,.0533,.0613,.0719,.0893,.1097};
    constexpr double ambient[]{.0401,.0533,.0671,.0797,.0919,.1073,.1237,.1399};
    const std::array<double,7> settings{decay,size,tone,modulation,double(family),frozen,sr};
    if(settingsValid && settings==lastSettings)return;
    lastSettings=settings;settingsValid=true;
    const double* base=family==0 ? room:family==2 ? ambient:hall;
    mod=std::clamp(modulation,0.0,1.0); freeze=std::clamp(frozen,0.0,1.0);
    const double cutoff=2000*std::pow(8,(std::clamp(tone,-1.0,1.0)+1)*.5);
    for(int i=0;i<8;++i)
    {
        delays[i]=std::round(base[i]*std::clamp(size,.3,1.7)*sr);
        gains[i]=std::min(.9995,std::pow(10,-3*delays[i]/sr/std::clamp(decay,.2,12.0)));
        const double ratio=.15+.85*(std::clamp(tone,-1.0,1.0)+1)*.5;
        dampingAmount[i]=1-std::pow(gains[i],1/ratio-1);
        damping[i].setLowpass(sr,cutoff);
    }
}
std::array<double,2> ReverbFdn::process(double input) noexcept
{
    constexpr double diffuserMs[]{5.3,7.1,10.6,14.7};
    double x=input;
    for(int i=0;i<4;++i)
    {
        const double delayed=diffusion[i].read(std::round(sr*.001*diffuserMs[i])-1);
        const double y=delayed-.6*x;
        diffusion[i].push(x+.6*y); x=y;
    }
    std::array<double,8> taps{}; double sum=0;
    for(int i=0;i<8;++i)
    {
        const double modulation=i<2 ? (i==0 ? 1:-1)*sr*.004*mod*(1-freeze)*lfo.sine():0;
        taps[i]=lines[i].read(std::max(1.,delays[i]+modulation-1)); sum+=taps[i];
    }
    for(int i=0;i<8;++i)
    {
        // A = I - (2/N)11^T. Damping bypass makes freeze lossless at integer taps.
        const double reflected=taps[i]-.25*sum;
        const double low=damping[i].process(reflected);
        const double damped=reflected+dampingAmount[i]*(low-reflected);
        const double feedback=damped*gains[i]*(1-freeze)+reflected*freeze;
        lines[i].push(common::safe(x*.3535533905932738+feedback));
    }
    lfo.advance();
    return {(taps[0]+taps[2]-taps[4]-taps[6])*.5,(taps[1]+taps[3]-taps[5]-taps[7])*.5};
}
}
