// OWNER: WP8
#include "Boost.h"
#include "../Common/SoftClip.h"
namespace luthier::reference::generic {
namespace {
const char* const modes[]={"Clean","Warm","Fat"};
const ParamDesc params[]={detail::continuous("gen_boost_gain","GAIN","dB",0,30,12,20),detail::continuous("gen_boost_tone","TONE","dB",-6,6,0,21),detail::continuous("gen_boost_level","LEVEL","dB",-24,6,-6,22),detail::choice("gen_boost_mode","MODE",modes,3,0,23),detail::choice("gen_boost_quality","QUALITY",detail::qualityNames,3,1,24,PF_Config),detail::engage("gen_boost_on",25)};
const PedalDesc description={"generic.boost","gen_boost_",1,"Clean Boost","Boost",nullptr,ChannelMode::Stereo,BypassMode::True,6,params,6,false,true,false,300};
}
const PedalDesc& Boost::descriptor() noexcept {return description;}
Boost::Boost():Processor(description){}
void Boost::prepare(double rate,int,int quality){prepareCommon(rate,quality,true,4);policy=std::min(quality,(int)value(4));for(int c=0;c<2;++c){hp[c].setLowpass(sr,20);eco[c].setLowpass(os[c].rate(),18000);warm[c].setShelf(os[c].rate(),100,3,false);fat1[c].setShelf(os[c].rate(),120,4,false);fat2[c].setShelf(os[c].rate(),120,2,false);}reset();}
void Boost::reset() noexcept {resetCommon();for(int c=0;c<2;++c){hp[c].reset();eco[c].reset();tone[c].reset();warm[c].reset();fat1[c].reset();fat2[c].reset();}}
void Boost::process(double* l,double* r,int n) noexcept {
 if(!l||!r)return;
 for(int i=0;i<n;++i){tick();if((clock&7)==0)for(int c=0;c<2;++c)tone[c].setShelf(os[c].rate(),3000,p[1],true);
  const double gain=common::dbGain(p[0]),level=common::dbGain(p[2]);double* channels[]={l,r};
  for(int c=0;c<2;++c){double x=input(channels[c][i]);double y=os[c].process(hp[c].highpass(x)*gain,[&](double v){v=tone[c].process(v);double a=common::cleanClip(v),b=warm[c].process(v),f=fat2[c].process(fat1[c].process(v));b=.5*b+.5*std::tanh(b);double z=detail::modeBlend(p[3],a,b,std::tanh(1.4*f));return policy==0?eco[c].process(z):z;});channels[c][i]=mix(c,x,y*level);}
  advance();}
}
}
