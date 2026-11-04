// Fixed Hall calibration feasibility. Each actual wet render yields both
// wet energy and dry/wet cross correlation, hence exact mix-RMS quadratic.
#include "Source/ReferencePedals/Generic/Reverb.h"
#include <cstdio>
#include <vector>
using namespace luthier::reference;
constexpr double pi=3.14159265358979323846;
int main()
{
 for(double sr:{44100.,48000.,88200.,96000.,192000.})for(int stimulus=0;stimulus<3;++stimulus)
 {
  std::vector<double> input(size_t(sr*8));common::Biquad low;low.setLowpass(sr,4000);uint32_t rng=42;double inputEnergy=0;
  for(int i=0;i<int(input.size());++i){double x=0;if(stimulus==0){rng=1664525u*rng+1013904223u;x=low.process(double(rng)/4294967296.-.5);}else if(stimulus==1){for(double f:{82.406889,110.,146.832384,196.,246.941651,329.627557})for(int h=1;h<=8;++h)x+=std::sin(2*pi*f*h*i/sr)/(h*h);}else x=std::sin(2*pi*1000*i/sr);input[size_t(i)]=x;if(i>=int(sr*5))inputEnergy+=x*x;}
  const double scale=std::pow(10.,-18./20)/std::sqrt(inputEnergy/(sr*3));for(auto& x:input)x*=scale;
  for(uint32_t seed:{1u,73u,0xdeadbeefu})for(double preroll:{0.,.37,1.13})
  {
   generic::Reverb p;PedalServices services;services.instanceSeed=seed;p.setServices(services);p.setParam(1,1);p.prepare(sr,1,1);
   for(int i=0;i<int(sr*preroll);++i){double l=0,r=0;p.process(&l,&r,1);}
   double dry=0,wet=0,cross=0,left=0,sum=0;
   for(int i=0;i<int(input.size());++i){double x=input[size_t(i)],l=x,r=x;p.process(&l,&r,1);if(i>=int(sr*5)){dry+=x*x;wet+=(l*l+r*r)*.5;cross+=x*(l+r)*.5;left+=l*l;sum+=(l+r)*(l+r);}}
   // g below is relative to the rendered wet signal. The archived sign grid
   // explicitly pins positive1.4; actual-product runs use their recorded gain.
   const double a=std::pow(std::sin(.3*pi*.5),2)*wet/dry,b=2*std::cos(.3*pi*.5)*std::sin(.3*pi*.5)*cross/dry,c=std::pow(std::cos(.3*pi*.5),2);
   std::printf("sr%.0f stimulus%d seed%u preroll%.2f wetRatio%.12g mixA%.12g mixB%.12g mixC%.12g sumVsL_db%.9f\n",sr,stimulus,seed,preroll,wet/dry,a,b,c,10*std::log10(sum/left));
  }
 }
}
