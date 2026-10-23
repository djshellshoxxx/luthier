// OWNER: WP10
#include "Reverb.h"

namespace luthier::reference::generic
{
namespace
{
const char* const typeNames[]{"Room","Hall","Plate","Spring","Ambient"};
const char* const trailNames[]{"Cut","Spillover"};
const ParamDesc parameters[] {
    {"gen_reverb_decay","DECAY","s",ParamKind::Continuous,Taper::LogA,0.2f,12.0f,2.2f,1.f,0,nullptr,PF_Automatable,20.0f,71},
    {"gen_reverb_mix","MIX","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.3f,1.f,0,nullptr,PF_Automatable,20.0f,72},
    {"gen_reverb_predelay","PRE-DELAY","ms",ParamKind::Continuous,Taper::Linear,0.0f,250.0f,15.0f,1.f,0,nullptr,PF_Automatable,20.0f,73},
    {"gen_reverb_tone","TONE","",ParamKind::Continuous,Taper::Linear,-1.0f,1.0f,0.0f,1.f,0,nullptr,PF_Automatable,20.0f,74},
    {"gen_reverb_size","SIZE","",ParamKind::Continuous,Taper::Linear,0.3f,1.7f,1.0f,1.f,0,nullptr,PF_Automatable,20.0f,75},
    {"gen_reverb_type","TYPE","",ParamKind::Enum,Taper::Linear,0.0f,4.0f,1.0f,1.f,5,typeNames,PF_Automatable,20.0f,76},
    {"gen_reverb_mod","MOD","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.25f,1.f,0,nullptr,PF_Automatable,20.0f,77},
    {"gen_reverb_lowcut","LOW CUT","Hz",ParamKind::Continuous,Taper::LogA,20.0f,800.0f,120.0f,1.f,0,nullptr,PF_Automatable,20.0f,78},
    {"gen_reverb_trails","TRAILS","",ParamKind::Enum,Taper::Linear,0.0f,1.0f,1.0f,1.f,2,trailNames,PF_Automatable,20.0f,79},
    {"gen_reverb_freeze","FREEZE","",ParamKind::Momentary,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Automatable,20.0f,102},
    {"gen_reverb_on","ON","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,1.0f,1.f,0,nullptr,PF_Engage,20.0f,103},
};
const PedalDesc description {"generic.reverb","gen_reverb_",1,"Reverb","Time",nullptr,ChannelMode::MonoInStereoOut,BypassMode::DSP,int(std::size(parameters)),parameters,0,true,false,false,0};
}
const PedalDesc& Reverb::descriptor() noexcept { return description; }
Reverb::Reverb() : PedalBase(description) {}
void Reverb::prepare(double sampleRate,int,int)
{
    sr=sampleRate;prepareParams(sr);for(auto& tank:fdn)tank.prepare(sr);
    plate.prepare(sr);spring.prepare(sr);predelay.prepare(int(sr*.251)+8);reset();
}
void Reverb::reset() noexcept
{
    for(auto& tank:fdn)tank.reset();plate.reset();spring.reset();predelay.reset();lowcut.reset();
    type=oldType=int(value(5));typeBlend=1;freeze=0;energy=0;
    inputGain=enabled ? 1:0;tailGain=enabled || value(8)>.5f ? 1:0;
}
float Reverb::getTailEnergyDb() const noexcept { return float(10*std::log10(std::max(1e-20,energy))); }
void Reverb::process(double* left,double* right,int n) noexcept
{
    constexpr double pi=3.14159265358979323846;
    for(int i=0;i<n;++i)
    {
        const double dry[2]{softGuard(left[i]),softGuard(right[i])};
        const double decay=nextSmoothed(0),mix=nextSmoothed(1),pre=nextSmoothed(2),tone=nextSmoothed(3),size=nextSmoothed(4),mod=nextSmoothed(6),cut=nextSmoothed(7);
        const int requested=int(value(5));
        if(requested!=type) { oldType=type;type=requested;typeBlend=0; }
        typeBlend=std::min(1.,typeBlend+1/(sr*.05));
        const double hold=value(9)>.5f ? 1:0;
        freeze+=std::clamp(hold-freeze,-1/(sr*.1),1/(sr*.02));
        inputGain+=std::clamp((enabled?1.:0.)-inputGain,-1/(sr*.005),1/(sr*.005));
        const double tailTarget=enabled || value(8)>.5f ? 1:0;
        tailGain+=std::clamp(tailTarget-tailGain,-1/(sr*.02),1/(sr*.02));
        lowcut.setHighpass(sr,cut);
        const double input=lowcut.process((dry[0]+dry[1])*.5)*inputGain*(1-freeze);
        predelay.push(input);
        const double x=pre<1e-6 ? input:predelay.read(std::max(1.,pre*sr*.001));
        auto render=[&](int selected) noexcept
        {
            if(selected==2) { plate.set(decay,size,tone,mod,freeze);return plate.process(x); }
            if(selected==3) { spring.set(decay,size,tone,freeze);return spring.process(x); }
            const int family=selected==4 ? 2:selected;
            fdn[family].set(decay,size,tone,mod,family,freeze); return fdn[family].process(x);
        };
        const auto current=render(type);
        const auto old=oldType!=type && typeBlend<1 ? render(oldType):current;
        const double wetL=old[0]+(current[0]-old[0])*typeBlend,wetR=old[1]+(current[1]-old[1])*typeBlend;
        const double dryGain=1+inputGain*(std::cos(mix*pi*.5)-1),wetGain=std::sin(mix*pi*.5)*tailGain;
        left[i]=softGuard(dry[0]*dryGain+wetL*wetGain);right[i]=softGuard(dry[1]*dryGain+wetR*wetGain);
        energy+=(.5*(wetL*wetL+wetR*wetR)-energy)*(1-std::exp(-1/(sr*.05)));
    }
}
}
