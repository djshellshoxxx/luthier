// No argument measures the actual product. Optional argument is the HISTORICAL
// external-output-diffusion prototype, only valid on a tank WITHOUT E66 stages.
// The final product already contains E66; do not pass an argument for acceptance.
#include "Source/ReferencePedals/Generic/Reverb.h"
#include <cstdio>
#include <vector>
using namespace luthier::reference;
int main(int argc,char**)
{
    const bool outputDiffusion=argc>1;
    double worstAt200=2,worstWindow=2;int failedAt200=0,failedWindow=0;
    for(double sr:{44100.,48000.,88200.,96000.,192000.})for(uint32_t seed:{1u,73u,0xdeadbeefu})for(double preroll:{0.,.37,1.13})
    {
        generic::Reverb p;PedalServices services;services.instanceSeed=seed;p.setServices(services);p.setParam(1,1);p.prepare(sr,1,1);
        common::FractionalDelay diffusion[2][2];for(auto& channel:diffusion)for(auto& line:channel)line.prepare(int(sr*.012)+16);
        const double delay[2][2]{{std::round(sr*.0031),std::round(sr*.0073)},{std::round(sr*.0047),std::round(sr*.0111)}};
        auto render=[&](double& l,double& r){p.process(&l,&r,1);if(outputDiffusion){double* out[]{&l,&r};for(int c=0;c<2;++c)for(int k=0;k<2;++k){auto& line=diffusion[c][k];const double x=*out[c],y=line.read(delay[c][k]-1)-.6*x;line.push(x+.6*y);*out[c]=y;}}};
        for(int i=0;i<int(sr*preroll);++i){double l=0,r=0;render(l,r);}
        std::vector<double> out[2];for(auto& v:out)v.resize(size_t(sr*.3));
        for(int i=0;i<int(out[0].size());++i){double l=i==0?1:0,r=l;render(l,r);out[0][size_t(i)]=l;out[1][size_t(i)]=r;}
        for(double at:{.19,.20,.21})for(int c=0;c<2;++c)
        {
            const int first=int((at-.01)*sr),last=int((at+.01)*sr);double mean=0,variance=0;
            for(int i=first;i<last;++i)mean+=out[c][size_t(i)];mean/=last-first;
            for(int i=first;i<last;++i){const double x=out[c][size_t(i)]-mean;variance+=x*x;}
            const double sd=std::sqrt(variance/(last-first));int above=0;
            for(int i=first;i<last;++i)if(std::abs(out[c][size_t(i)]-mean)>sd)++above;
            const double ned=double(above)/(last-first)/.317310507862914;
            worstWindow=std::min(worstWindow,ned);if(ned<=.9)++failedWindow;
            if(at==.20){worstAt200=std::min(worstAt200,ned);if(ned<=.9)++failedAt200;}
            std::printf("sr%.0f seed%u preroll%.2f window_center%.3f channel%d raw_NED%.9f\n",sr,seed,preroll,at,c,ned);
        }
    }
    std::printf("outputDiffusion%d min200ms %.9f failures%d/90 minAllWindows %.9f failures%d/270\n",outputDiffusion,worstAt200,failedAt200,worstWindow,failedWindow);
}
