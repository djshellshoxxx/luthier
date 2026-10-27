// OWNER: WP10
#include "ReverbSpring.h"
#include <cmath>
#include <algorithm>
namespace luthier::reference::generic
{
void ReverbSpring::prepare(double sampleRate) { sr=sampleRate; settingsValid=false; for(auto& d:delays)d.prepare(int(sr*.08)+8);reset();set(2.2,1,0,0); }
void ReverbSpring::reset() noexcept { dispersion={};for(auto& d:delays)d.reset();for(auto& f:lowpass)f.reset(); }
void ReverbSpring::set(double decay,double size,double tone,double frozen) noexcept
{
    const std::array<double,4> settings{decay,size,tone,frozen};
    if(settingsValid && settings==lastSettings)return;
    lastSettings=settings;settingsValid=true;
    freeze=std::clamp(frozen,0.,1.);
    for(int c=0;c<2;++c)
    {
        length[c]=std::round(sr*(c==0 ? .033:.041)*std::clamp(size,.3,1.7));
        gain[c]=std::pow(10,-3*length[c]/sr/std::clamp(decay,.2,12.));
        lowpass[c].setLowpass(sr,4000*std::pow(2,std::clamp(tone,-1.,1.)));
    }
}
std::array<double,2> ReverbSpring::process(double input) noexcept
{
    double voices[3]{};
    for(int v=0;v<3;++v)
    {
        double x=std::tanh(input);
        const double a=.62+.04*v;
        for(int k=0;k<24;++k) {const double y=a*x+dispersion[v][k];dispersion[v][k]=x-a*y;x=y;}
        voices[v]=x;
    }
    std::array<double,2> result{};
    for(int c=0;c<2;++c)
    {
        const double delayed=delays[c].read(length[c]-1);
        const double damped=lowpass[c].process(delayed);
        delays[c].push((voices[c]+voices[2])*.35+damped*gain[c]*(1-freeze)+delayed*freeze);
        result[c]=delayed;
    }
    return result;
}
}
