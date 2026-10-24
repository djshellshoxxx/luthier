// OWNER: WP10
#include "Reverb.h"
#include "MemorySupport.h"

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
// ESTIMATED admission budget from GENERIC_PEDALS; highest documented runtime mode.
const PedalDesc description {"generic.reverb","gen_reverb_",1,"Reverb","Time",nullptr,ChannelMode::MonoInStereoOut,BypassMode::DSP,int(std::size(parameters)),parameters,0,true,false,false,3000,1000};
}
PreparedMemoryRequirement Reverb::prepareMemory(double sr,int,int,const float*) noexcept
{
    generic::detail::TimeFxMemoryEstimate memory(sr,sizeof(Reverb));
    memory.fdn(3);
    memory.delay(.04,8,4);memory.delay(.07,8,2);memory.delay(.16,8,2);memory.delay(.27,8,2);memory.delay(.23,8,2);
    memory.delay(.08,8,2);memory.delay(.251,8);
    return memory.result();
}
uint64_t Reverb::getPreparedMemoryBytes() const noexcept
{
    uint64_t bytes=sizeof(*this);
    if(!addPreparedBytes(bytes,fdn[0].ownedMemoryBytes(),bytes))return kUnknownPreparedMemoryBytes;
    if(!addPreparedBytes(bytes,fdn[1].ownedMemoryBytes(),bytes))return kUnknownPreparedMemoryBytes;
    if(!addPreparedBytes(bytes,fdn[2].ownedMemoryBytes(),bytes))return kUnknownPreparedMemoryBytes;
    if(!addPreparedBytes(bytes,plate.ownedMemoryBytes(),bytes))return kUnknownPreparedMemoryBytes;
    if(!addPreparedBytes(bytes,spring.ownedMemoryBytes(),bytes))return kUnknownPreparedMemoryBytes;
    if(!addPreparedBytes(bytes,predelay.ownedMemoryBytes(),bytes))return kUnknownPreparedMemoryBytes;
    return bytes;
}
const PedalDesc& Reverb::descriptor() noexcept { return description; }
Reverb::Reverb() : PedalBase(description) {}
void Reverb::prepare(double sampleRate,int,int)
{
    sr=sampleRate;energyCoefficient=1-std::exp(-1/(sr*.05));typeStep=1/(sr*.025);
    prepareParams(sr);for(int i=0;i<3;++i)fdn[i].prepare(sr,seed+uint32_t(i)*0x9e3779b9u);
    plate.prepare(sr,seed^0xa511e9b3u);spring.prepare(sr);predelay.prepare(int(sr*.251)+8);reset();
}
void Reverb::reset() noexcept
{
    for(auto& tank:fdn)tank.reset();plate.reset();spring.reset();predelay.reset();lowcut.reset();
    type=int(value(5));typeBlend=1;freeze=0;energy=0;lastCut=-1;
    inputGain=enabled ? 1:0;tailGain=enabled || value(8)>.5f ? 1:0;
}
float Reverb::getTailEnergyDb() const noexcept { return float(10*std::log10(std::max(1e-20,energy))); }
void Reverb::process(double* left,double* right,int n) noexcept
{ render(left,right,nullptr,nullptr,n); }
bool Reverb::processWithWetResidual(double* left,double* right,double* residualL,double* residualR,int n) noexcept
{ render(left,right,residualL,residualR,n);return true; }
void Reverb::render(double* left,double* right,double* residualL,double* residualR,int n) noexcept
{
    constexpr double pi=3.14159265358979323846;
    for(int i=0;i<n;++i)
    {
        const double dry[2]{softGuard(left[i]),softGuard(right[i])};
        const double decay=nextSmoothed(0),mix=nextSmoothed(1),pre=nextSmoothed(2),tone=nextSmoothed(3),size=nextSmoothed(4),mod=nextSmoothed(6),cut=nextSmoothed(7);
        const int requested=int(value(5));
        // A 25 ms down / 25 ms up dip handles repeated automation without
        // discontinuously replacing a partly audible crossfade voice. Reset
        // the selected tank only at zero gain; delay resets are O(1).
        if(requested!=type)
        {
            typeBlend=std::max(0.,typeBlend-typeStep);
            if(typeBlend==0)
            {
                type=requested;
                if(type==2)plate.reset();else if(type==3)spring.reset();
                else fdn[type==4 ? 2:type].reset();
            }
        }
        else typeBlend=std::min(1.,typeBlend+typeStep);
        const double hold=value(9)>.5f ? 1:0;
        freeze+=std::clamp(hold-freeze,-1/(sr*.1),1/(sr*.02));
        inputGain+=std::clamp((enabled?1.:0.)-inputGain,-1/(sr*.005),1/(sr*.005));
        const double tailTarget=enabled || value(8)>.5f ? 1:0;
        tailGain+=std::clamp(tailTarget-tailGain,-1/(sr*.02),1/(sr*.02));
        if(cut!=lastCut){lowcut.setHighpass(sr,cut);lastCut=cut;}
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
        // Fixed estimated calibration, without dynamic normalization. E66 Hall
        // reverses BOTH wet tap signs: wet energy is unchanged, while the
        // dry/wet cross term changes. Other types retain their original signs.
        constexpr double calibration[]{1.25,-1.47,1.45,.9,1.9};
        const double wetL=current[0]*typeBlend*calibration[type],wetR=current[1]*typeBlend*calibration[type];
        const double dryGain=1+inputGain*(std::cos(mix*pi*.5)-1),wetGain=std::sin(mix*pi*.5)*tailGain;
        if(residualL)residualL[i]=wetL*wetGain;
        if(residualR)residualR[i]=wetR*wetGain;
        left[i]=softGuard(dry[0]*dryGain+wetL*wetGain);right[i]=softGuard(dry[1]*dryGain+wetR*wetGain);
        energy+=(.5*(wetL*wetL+wetR*wetR)-energy)*energyCoefficient;
    }
}
}
