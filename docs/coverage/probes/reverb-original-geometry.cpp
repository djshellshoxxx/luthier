// Frozen original geometry and sine-modulated algorithm oracle from19320228.
// This is evidence only, not linked into product code. Source paths/class renamed.
// OWNER: WP10

#include "Source/ReferencePedals/Common/FractionalDelay.h"
#include "Source/ReferencePedals/Common/OnePole.h"
#include "Source/ReferencePedals/Common/Lfo.h"
#include <array>
namespace luthier::reference::generic
{
// Eight-line orthogonal Householder FDN; Room/Hall/Ambient families only.
class OriginalGeometryFdn
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void set(double decaySeconds,double size,double tone,double modulation,int family,double freeze) noexcept;
    std::array<double,2> process(double input) noexcept;
    std::array<double,8> delayLengths() const noexcept {return delays;}
private:
    std::array<double,7> lastSettings{};
    bool settingsValid=false;
    std::array<common::FractionalDelay,8> lines;
    std::array<common::FractionalDelay,4> diffusion;
    std::array<common::OnePole,8> damping;
    std::array<double,8> delays{},gains{},dampingAmount{};
    std::array<double,4> diffuserDelays{};
    std::vector<int> primeLookup;
    common::Lfo lfo;
    double sr=48000,mod=0,freeze=0;
};
}
// OWNER: WP10

#include <cmath>
#include <algorithm>
namespace luthier::reference::generic
{
void OriginalGeometryFdn::prepare(double sampleRate)
{
    sr=sampleRate; settingsValid=false;
    // Build a nearest-prime lookup off the callback. SIZE automation then
    // performs eight bounded indexed reads, never a search/allocation/sieve.
    const int limit=int(sr*.25)+16;
    std::vector<bool> prime(size_t(limit*2+1),true);prime[0]=prime[1]=false;
    for(int p=2;p*p<=limit*2;++p)if(prime[size_t(p)])
        for(int n=p*p;n<=limit*2;n+=p)prime[size_t(n)]=false;
    primeLookup.resize(size_t(limit+1));int lower=2;
    for(int n=0;n<=limit;++n){if(prime[size_t(n)])lower=n;primeLookup[size_t(n)]=lower;}
    int upper=limit*2;
    for(int n=limit*2;n>=0;--n)
    {
        if(prime[size_t(n)])upper=n;
        if(n<=limit && upper-n<=std::abs(n-primeLookup[size_t(n)]))primeLookup[size_t(n)]=upper;
    }
    for(auto& line:lines) line.prepare(int(sr*.25)+16);
    for(auto& line:diffusion) line.prepare(int(sr*.03)+16);
    lfo.prepare(sr); lfo.setRate(.31); reset(); set(2.2,1,0,.25,1,0);
}
void OriginalGeometryFdn::reset() noexcept
{
    for(auto& line:lines) line.reset(); for(auto& line:diffusion) line.reset();
    for(auto& filter:damping) filter.reset(); lfo.setPhase(0);
}
void OriginalGeometryFdn::set(double decay,double size,double tone,double modulation,int family,double frozen) noexcept
{
    constexpr double room[]{.0127,.0179,.0233,.0299,.0317,.0371,.0419,.0449};
    constexpr double hall[]{.0301,.0373,.0439,.0533,.0613,.0719,.0893,.1097};
    constexpr double ambient[]{.0401,.0533,.0671,.0797,.0919,.1073,.1237,.1399};
    const std::array<double,7> settings{decay,size,tone,modulation,double(family),frozen,sr};
    if(settingsValid && settings==lastSettings)return;
    lastSettings=settings;settingsValid=true;
    const double* base=family==0 ? room:family==2 ? ambient:hall;
    constexpr double diffuserMs[]{5.3,7.1,10.6,14.7};
    const double scale=std::clamp(size,.3,1.7);
    for(int i=0;i<4;++i)diffuserDelays[i]=std::round(sr*.001*diffuserMs[i]*scale);
    mod=std::clamp(modulation,0.0,1.0); freeze=std::clamp(frozen,0.0,1.0);
    const double cutoff=2000*std::pow(8,(std::clamp(tone,-1.0,1.0)+1)*.5);
    for(int i=0;i<8;++i)
    {
        delays[i]=primeLookup[size_t(std::lround(base[i]*scale*sr))];
        gains[i]=std::min(.9995,std::pow(10,-3*delays[i]/sr/std::clamp(decay,.2,12.0)));
        const double ratio=.15+.85*(std::clamp(tone,-1.0,1.0)+1)*.5;
        dampingAmount[i]=1-std::pow(gains[i],1/ratio-1);
        damping[i].setLowpass(sr,cutoff);
    }
}
std::array<double,2> OriginalGeometryFdn::process(double input) noexcept
{
    double x=input;
    for(int i=0;i<4;++i)
    {
        const double delayed=diffusion[i].read(diffuserDelays[i]-1);
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

#include "Source/ReferencePedals/Common/Biquad.h"
#include <cstdio>
int main()
{
 using namespace luthier::reference;
 for(double sr:{44100.,48000.,88200.,96000.,192000.})for(double preroll:{0.,.37,1.13})
 {
  generic::OriginalGeometryFdn tank;tank.prepare(sr);tank.set(2.2,1,0,.25,1,0);
  common::Biquad lowcut;lowcut.setHighpass(sr,120);common::FractionalDelay pre;pre.prepare(int(sr*.251)+8);
  auto render=[&](double x){pre.push(lowcut.process(x));return tank.process(pre.read(sr*.015));};
  for(int i=0;i<int(sr*preroll);++i)render(0);
  std::vector<double> output[2];for(auto& x:output)x.resize(size_t(sr*.3));
  for(int i=0;i<int(output[0].size());++i){auto y=render(i==0?1:0);output[0][i]=y[0];output[1][i]=y[1];}
  for(double at:{.19,.20,.21})for(int c=0;c<2;++c){int first=int((at-.01)*sr),last=int((at+.01)*sr);double mean=0,var=0;for(int i=first;i<last;++i)mean+=output[c][i];mean/=last-first;for(int i=first;i<last;++i)var+=std::pow(output[c][i]-mean,2);double sd=std::sqrt(var/(last-first));int above=0;for(int i=first;i<last;++i)if(std::abs(output[c][i]-mean)>sd)++above;std::printf("original19320228 sr%.0f preroll%.2f center%.2f channel%d raw_NED%.9f\n",sr,preroll,at,c,double(above)/(last-first)/.317310507862914);}
 }
}
