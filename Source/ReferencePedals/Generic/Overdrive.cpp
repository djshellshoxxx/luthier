// OWNER: WP8
#include "Overdrive.h"
#include "../Common/SoftClip.h"
namespace luthier::reference::generic {
namespace {
const char* const clips[]={"Soft","Asym","Hard"};const char* const basses[]={"Off","300 Hz","700 Hz"};
const ParamDesc params[]={detail::continuous("gen_overdrive_drive","DRIVE","dB",0,40,16,26),detail::continuous("gen_overdrive_tone","TONE","Hz",500,9000,3000,27,Taper::LogA),detail::continuous("gen_overdrive_level","LEVEL","dB",-60,12,0,28,Taper::Skew,2),detail::choice("gen_overdrive_clip","CLIP",clips,3,0,29),detail::choice("gen_overdrive_bass","BASS CUT",basses,3,1,30),detail::choice("gen_overdrive_quality","QUALITY",detail::qualityNames,3,1,-1,PF_Config),detail::engage("gen_overdrive_on",31)};
const PedalDesc description={"generic.overdrive","gen_overdrive_",1,"Overdrive","Overdrive",nullptr,ChannelMode::Stereo,BypassMode::True,7,params,6,false,true,false,1000};
}
const PedalDesc& Overdrive::descriptor() noexcept{return description;}
Overdrive::Overdrive():Processor(description){}
void Overdrive::prepare(double rate,int,int quality){prepareCommon(rate,quality,true,5);for(auto& f:dc)f.prepare(sr,5);reset();}
void Overdrive::reset() noexcept {resetCommon();for(int c=0;c<2;++c){bass[c].reset();tone[c].reset();dc[c].reset();}}
void Overdrive::process(double* l,double* r,int n) noexcept {
 if(!l||!r)return;for(int i=0;i<n;++i){tick();if((clock&7)==0)for(int c=0;c<2;++c){bass[c].setLowpass(sr,detail::modeBlend(p[4],20,300,700));tone[c].setLowpass(sr,p[1]);}
 const double g=common::dbGain(p[0]),level=p[2]<=-60?0:common::dbGain(p[2]),comp=1/(1+.1*std::sqrt(g));double* channels[]={l,r};
 for(int c=0;c<2;++c){double x=input(channels[c][i]),v=(x-.7*bass[c].process(x))*g;double y=os[c].process(v,[&](double z){return detail::modeBlend(p[3],std::tanh(z),common::asymmetricClip(z),common::diodeApprox(z));});channels[c][i]=mix(c,x,dc[c].process(tone[c].process(y)*comp*level));}advance();}
}
}
