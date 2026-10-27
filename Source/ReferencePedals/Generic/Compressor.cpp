// OWNER: WP8
#include "Compressor.h"
#include "../Common/EnvelopeFollower.h"
namespace luthier::reference::generic {
namespace {
const char* const modes[]={"Sustain","Manual"};const char* const sources[]={"Internal","Sidechain bus"};
const ParamDesc params[]={detail::continuous("gen_comp_sustain","SUSTAIN","",0,1,.5,40),detail::continuous("gen_comp_attack","ATTACK","ms",.5,60,10,41,Taper::LogA),detail::continuous("gen_comp_release","RELEASE","ms",40,1200,350,42,Taper::LogA),detail::continuous("gen_comp_level","LEVEL","dB",-12,24,6,43),detail::continuous("gen_comp_blend","BLEND","",0,1,1,44),detail::choice("gen_comp_mode","MODE",modes,2,0,45),detail::continuous("gen_comp_thresh","THRESH","dB",-60,0,-24,46),detail::continuous("gen_comp_ratio","RATIO",":1",1.5,20,4,47,Taper::LogA),detail::continuous("gen_comp_knee","KNEE","dB",0,12,6,48),detail::engage("gen_comp_on",49),detail::choice("gen_comp_sc","SIDECHAIN",sources,2,0,-1,PF_Config)};
const PedalDesc description={"generic.compressor","gen_comp_",1,"Compressor","Dynamics",nullptr,ChannelMode::Stereo,BypassMode::True,11,params,0,false,false,false,300,1000};
double safety(double x) noexcept {double a=std::abs(x);return a<=.95?x:std::copysign(.95+.05*std::tanh((a-.95)/.05),x);}
}
const PedalDesc& Compressor::descriptor() noexcept{return description;}
Compressor::Compressor():Processor(description){}
void Compressor::prepare(double rate,int,int quality){prepareCommon(rate,quality,false);for(auto& f:sidechain)f.setLowpass(sr,80);reset();}
void Compressor::reset() noexcept {resetCommon();for(auto& f:sidechain)f.reset();reduction=0;externalL=externalR=nullptr;externalCount=0;available.store(false,std::memory_order_relaxed);meter.store(0,std::memory_order_relaxed);}
double Compressor::gainReductionDb(double x,double t,double r,double w) noexcept {
 const double delta=x-t,slope=1/r-1;
 if(w>0&&std::abs(2*delta)<=w)return slope*(delta+w*.5)*(delta+w*.5)/(2*w);
 return delta>0?slope*delta:0;
}
void Compressor::process(double* l,double* r,int n) noexcept {
 const bool present=externalL&&externalR&&externalCount>=n&&n>0;
 available.store(present,std::memory_order_relaxed);
 if(!l||!r){externalL=externalR=nullptr;externalCount=0;return;}
 for(int i=0;i<n;++i){tick();if((clock&7)==0){attack=common::EnvelopeFollower::coefficient(sr,p[1]);release=common::EnvelopeFollower::coefficient(sr,p[2]);}
 double x=input(l[i]),y=input(r[i]);
 const bool external=value(10)>=.5;
 if(external&&!present){l[i]=mix(0,x,x);r[i]=mix(1,y,y);advance();continue;}
 double env=std::max(std::abs(sidechain[0].highpass(external?input(externalL[i]):x)),std::abs(sidechain[1].highpass(external?input(externalR[i]):y)));
 double linkedT=-10-35*p[0],linkedR=2+10*p[0],t=linkedT+(p[6]-linkedT)*p[5],ratio=linkedR+(p[7]-linkedR)*p[5];
 double wanted=gainReductionDb(20*std::log10(env+1e-9),t,ratio,p[8]);double c=wanted<reduction?attack:release;reduction=common::safe(wanted+c*(reduction-wanted));
 double makeup=p[3]+(1-p[5])*(-linkedT*(1-1/linkedR)*.5),gain=common::dbGain(reduction+makeup);
 double a=x+(safety(x*gain)-x)*p[4],b=y+(safety(y*gain)-y)*p[4];l[i]=mix(0,x,a);r[i]=mix(1,y,b);advance();}
 externalL=externalR=nullptr;externalCount=0;
 meter.store((float)-reduction,std::memory_order_relaxed);
}
}
