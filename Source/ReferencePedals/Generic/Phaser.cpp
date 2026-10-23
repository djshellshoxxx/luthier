// OWNER: WP9
#include "Phaser.h"

namespace luthier::reference::generic
{
namespace
{
const char* const syncNames[] {"Off","1/4","1/8","1/8.","1/16","1/2","1 bar"};
const char* const stageNames[] {"2","4","6","8","12"};
const ParamDesc parameters[] {
    {"gen_phaser_rate","RATE","Hz",ParamKind::Continuous,Taper::LogA,0.05f,8.0f,0.5f,1.f,0,nullptr,PF_Automatable,20.0f,14},
    {"gen_phaser_depth","DEPTH","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.8f,1.f,0,nullptr,PF_Automatable,20.0f,15},
    {"gen_phaser_feedback","FEEDBACK","",ParamKind::Continuous,Taper::Linear,-0.9f,0.9f,0.3f,1.f,0,nullptr,PF_Automatable,20.0f,16},
    {"gen_phaser_stages","STAGES","",ParamKind::Enum,Taper::Linear,0.0f,4.0f,1.0f,1.f,5,stageNames,PF_Automatable,20.0f,17},
    {"gen_phaser_mix","MIX","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.5f,1.f,0,nullptr,PF_Automatable,20.0f,18},
    {"gen_phaser_center","CENTER","Hz",ParamKind::Continuous,Taper::LogA,200.0f,2000.0f,700.0f,1.f,0,nullptr,PF_Automatable,20.0f,19},
    {"gen_phaser_sync","SYNC","",ParamKind::Enum,Taper::Linear,0.0f,6.0f,0.0f,1.f,7,syncNames,PF_Automatable,20.0f,3},
    {"gen_phaser_on","ON","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,1.0f,1.f,0,nullptr,PF_Engage,20.0f,9},
};
const PedalDesc description {"generic.phaser","gen_phaser_",1,"Phaser","Modulation",nullptr,ChannelMode::MonoInStereoOut,BypassMode::True,int(std::size(parameters)),parameters,0,false,false,false,0};
}
const PedalDesc& Phaser::descriptor() noexcept { return description; }
Phaser::Phaser() : PedalBase(description) {}
void Phaser::prepare(double sampleRate,int,int) { sr=sampleRate; prepareParams(sr); clock.prepare(sr); reset(); }
void Phaser::reset() noexcept { state={}; feedback={}; clock.reset(); constexpr int counts[]{2,4,6,8,12}; stages=previousStages=counts[int(value(3))]; stageBlend=1; }
void Phaser::setTime(const TimeInfo& t) noexcept { time=t; clock.time(t,detail::modulationQuarters(int(value(6)))); }
void Phaser::process(double* left,double* right,int n) noexcept
{
    constexpr int counts[]{2,4,6,8,12};
    for(int i=0;i<n;++i)
    {
        const int requested=counts[int(value(3))];
        if(stages!=requested) { previousStages=stages; stages=requested; stageBlend=0; }
        stageBlend=std::min(1.0,stageBlend+1/(.005*sr));
        const double depth=nextSmoothed(1), fb=nextSmoothed(2), mix=nextSmoothed(4), center=nextSmoothed(5);
        double* outputs[]{left,right};
        for(int c=0;c<2;++c)
        {
            const double dry=softGuard(outputs[c][i]);
            const double freq=std::clamp(center*std::pow(2,depth*2.5*detail::wave(clock.phase()+c*.25,false)),20.0,sr*.45);
            const double t=std::tan(detail::pi*freq/sr), a=(t-1)/(t+1);
            double x=dry+fb*std::tanh(.9*feedback[c]), oldTap=0,newTap=0;
            for(int s=0;s<12;++s)
            {
                const double y=a*x+state[c][s]; state[c][s]=x-a*y; x=y;
                if(s+1==previousStages) oldTap=y;
                if(s+1==stages) newTap=y;
            }
            const double wet=oldTap+(newTap-oldTap)*stageBlend;
            feedback[c]=wet;
            outputs[c][i]=softGuard(dry*(1-mix)+wet*mix);
        }
        clock.advance(detail::tempoRate(time,detail::modulationQuarters(int(value(6))),nextSmoothed(0)));
    }
}
}
