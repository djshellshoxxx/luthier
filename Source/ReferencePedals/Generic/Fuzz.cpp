// OWNER: WP8
#include "Fuzz.h"
namespace luthier::reference::generic {
namespace {
const char* const types[]={"Silicon","Germanium","Octave-ish"};
// CC32 is reserved bank-select: omit the published conflicting local assignment.
const ParamDesc params[]={detail::continuous("gen_fuzz_fuzz","FUZZ","dB",6,60,44,-1),detail::continuous("gen_fuzz_tone","TONE","dB",-12,12,0,33),detail::continuous("gen_fuzz_volume","VOLUME","dB",-60,12,-6,34,Taper::Skew,2),detail::continuous("gen_fuzz_bias","BIAS","",0,1,.5,35),detail::choice("gen_fuzz_type","TYPE",types,3,0,36),detail::continuous("gen_fuzz_gate","GATE","",0,1,0,37),detail::choice("gen_fuzz_quality","QUALITY",detail::qualityNames,3,1,-1,PF_Config),detail::engage("gen_fuzz_on",38)};
const PedalDesc description={"generic.fuzz","gen_fuzz_",1,"Fuzz","Fuzz",nullptr,ChannelMode::Stereo,BypassMode::True,8,params,6,false,true,false,1200};
double silicon(double x) noexcept {return x>=0?std::tanh(1.3*x):1.1*std::tanh(.9*x);}
double shape(double x,double type,double bias) noexcept {double si=silicon(x),b=.2*(1-bias),ge=std::tanh(1.8*(x+b))-std::tanh(1.8*b);return detail::modeBlend(type,si,ge,si);}
}
const PedalDesc& Fuzz::descriptor() noexcept{return description;}
Fuzz::Fuzz():Processor(description){}
void Fuzz::prepare(double rate,int,int quality){prepareCommon(rate,quality,true,6);policy=std::min(quality,(int)value(6));for(int c=0;c<2;++c){hp[c].setLowpass(sr,60);germanium[c].setLowpass(sr,9000);tilt[c].setLowpass(sr,1000);eco[c].setLowpass(sr,14000);dc[c].prepare(sr,10);octave[c].prepare(os[c].rate(),80);sag[c].prepare(sr,5,120);gate[c].prepare(sr,1,80);gateGain[c].prepare(sr,1);}reset();}
void Fuzz::reset() noexcept {resetCommon();for(int c=0;c<2;++c){hp[c].reset();germanium[c].reset();tilt[c].reset();eco[c].reset();dc[c].reset();octave[c].reset();sag[c].reset();gate[c].reset();gateGain[c].reset(1);hold[c]=0;}}
void Fuzz::process(double* l,double* r,int n) noexcept {
 if(!l||!r)return;for(int i=0;i<n;++i){tick();const double g=std::sqrt(common::dbGain(p[0])),t=common::dbGain(p[1]),volume=p[2]<=-60?0:common::dbGain(p[2]);double* channels[]={l,r};
 if((clock&7)==0)for(auto& f:hp)f.setLowpass(sr,detail::modeBlend(p[4],60,90,60));
 for(int c=0;c<2;++c){double x=input(channels[c][i]),v=hp[c].highpass(x),ge=germanium[c].process(v);double geWeight=std::max(0.0,1-std::abs(p[4]-1));v+=(ge-v)*geWeight;const double env=std::min(1.0,sag[c].process(x)),gain=g*std::sqrt(1-.2*env*geWeight);
 double y=os[c].process(v,[&](double z){double y1=shape(gain*z,p[4],p[3]),y2=shape(gain*y1+(.5-p[3])*.6,p[4],p[3]);double starve=std::clamp((std::abs(y1)+.1*p[3])/.1,0.0,1.0);y2*=starve*starve*(3-2*starve);double oct=octave[c].process(.6*y2+.4*std::abs(y2));return y2+(oct-y2)*std::max(0.0,p[4]-1);});
 y=dc[c].process(y);double low=tilt[c].process(y);y=low/t+(y-low)*t;
 double detected=gate[c].process(x),threshold=common::dbGain(-90+50*p[5]),target=1;
 if(p[5]>0){if(detected>=threshold)hold[c]=(int)(sr*.03);else if(hold[c]>0)--hold[c];else target=std::max(.001,std::pow(detected/threshold,3));}
 gateGain[c].setTarget(target);y*=gateGain[c].next()*volume;if(policy==0)y=eco[c].process(y);channels[c][i]=mix(c,x,std::clamp(y,-common::dbGain(12),common::dbGain(12)));}advance();}
}
}
