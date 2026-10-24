// OWNER: WP9
#include "Wah.h"

namespace luthier::reference::generic
{
namespace
{
const char* const modeNames[]{"Treadle","Auto","LFO"};
const char* const toeNames[]{"Off","On"};
const ParamDesc parameters[] {
    {"gen_wah_pedal","PEDAL","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.5f,1.f,0,nullptr,PF_Automatable | PF_ExprTarget,8.0f,11},
    {"gen_wah_range_lo","FREQ LOW","Hz",ParamKind::Continuous,Taper::LogA,250.0f,600.0f,350.0f,1.f,0,nullptr,PF_Automatable,20.0f,-1},
    {"gen_wah_range_hi","FREQ HIGH","Hz",ParamKind::Continuous,Taper::LogA,1200.0f,3200.0f,2200.0f,1.f,0,nullptr,PF_Automatable,20.0f,-1},
    {"gen_wah_q","Q","",ParamKind::Continuous,Taper::LogA,2.0f,12.0f,6.0f,1.f,0,nullptr,PF_Automatable,20.0f,114},
    {"gen_wah_mode","MODE","",ParamKind::Enum,Taper::Linear,0.0f,2.0f,0.0f,1.f,3,modeNames,PF_Automatable,20.0f,115},
    {"gen_wah_sens","SENS","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.5f,1.f,0,nullptr,PF_Automatable,20.0f,116},
    {"gen_wah_rate","RATE","Hz",ParamKind::Continuous,Taper::LogA,0.2f,10.0f,1.5f,1.f,0,nullptr,PF_Automatable,20.0f,117},
    {"gen_wah_mix","MIX","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,1.0f,1.f,0,nullptr,PF_Automatable,20.0f,118},
    {"gen_wah_toe","TOE SWITCH","",ParamKind::Enum,Taper::Linear,0.0f,1.0f,0.0f,1.f,2,toeNames,PF_Automatable,20.0f,-1},
    {"gen_wah_on","ON","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,1.0f,1.f,0,nullptr,PF_Engage,20.0f,119},
};
const PedalDesc description {"generic.wah","gen_wah_",1,"Wah","Filter",nullptr,ChannelMode::Stereo,BypassMode::True,int(std::size(parameters)),parameters,0,false,false,false,0};
}
const PedalDesc& Wah::descriptor() noexcept { return description; }
Wah::Wah() : PedalBase(description) {}
void Wah::prepare(double sampleRate,int,int)
{
    sr=sampleRate; prepareParams(sr); for(auto& f:filter) f.prepare(sr);
    envelope.prepare(sr,2,80); clock.prepare(sr); reset();
}
void Wah::reset() noexcept { for(auto& f:filter) f.reset(); envelope.reset(); clock.reset(); position=value(0);toeArmed=position<.97;toeRequest=false; }
void Wah::setExpression(int jack,float v) noexcept { if(jack==0) setParam(0,v); }
void Wah::process(double* left,double* right,int n) noexcept
{
    for(int i=0;i<n;++i)
    {
        const double dry[2]{softGuard(left[i]),softGuard(right[i])};
        const double treadle=nextSmoothed(0),lo=nextSmoothed(1),hi=nextSmoothed(2),q=nextSmoothed(3);
        const double env=envelope.process(std::max(std::abs(dry[0]),std::abs(dry[1])));
        const double sensitivity=nextSmoothed(5),rate=nextSmoothed(6),mix=nextSmoothed(7);
        const int mode=int(value(4));
        const double target=mode==1 ? std::clamp(env*sensitivity*8,0.0,1.0):mode==2 ? (detail::wave(clock.phase(),false)+1)*.5:treadle;
        position+=(target-position)*(1-std::exp(-1/(.008*sr)));
        if(value(8)>.5f)
        {
            if(treadle<=.9)toeArmed=true;
            if(toeArmed && treadle>=.97){toeRequest=!toeRequest;toeArmed=false;}
        }
        else {toeArmed=treadle<.97;toeRequest=false;}
        const double frequency=lo*std::pow(hi/lo,position);
        double* outputs[]{left,right};
        for(int c=0;c<2;++c)
        {
            filter[c].set(frequency,q);
            // TPT bandpass peak is Q, with 0.5 input gain: normalise to +12 dB.
            const double wet=filter[c].process(std::tanh(.5*dry[c])).band*8/q;
            outputs[c][i]=softGuard(dry[c]*(1-mix)+wet*mix);
        }
        clock.advance(rate);
    }
}
}
