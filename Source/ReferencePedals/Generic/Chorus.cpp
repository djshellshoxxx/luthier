// OWNER: WP9
#include "Chorus.h"
#include "MemorySupport.h"

namespace luthier::reference::generic
{
namespace
{
const char* const syncNames[] {"Off","1/4","1/8","1/8.","1/16","1/2","1 bar"};
const char* const modeNames[] {"Chorus","Vibrato","Dimension"};
const char* const waveNames[] {"Sine","Triangle"};
const ParamDesc parameters[] {
    {"gen_chorus_rate","RATE","Hz",ParamKind::Continuous,Taper::LogA,0.1f,8.0f,0.8f,1.f,0,nullptr,PF_Automatable,20.0f,50},
    {"gen_chorus_depth","DEPTH","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.5f,1.f,0,nullptr,PF_Automatable,20.0f,51},
    {"gen_chorus_mix","MIX","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.5f,1.f,0,nullptr,PF_Automatable,20.0f,52},
    {"gen_chorus_tone","TONE","Hz",ParamKind::Continuous,Taper::LogA,1500.0f,12000.0f,6000.0f,1.f,0,nullptr,PF_Automatable,20.0f,53},
    {"gen_chorus_mode","MODE","",ParamKind::Enum,Taper::Linear,0.0f,2.0f,0.0f,1.f,3,modeNames,PF_Automatable,20.0f,54},
    {"gen_chorus_wave","WAVE","",ParamKind::Enum,Taper::Linear,0.0f,1.0f,0.0f,1.f,2,waveNames,PF_Automatable,20.0f,55},
    {"gen_chorus_spread","SPREAD","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.8f,1.f,0,nullptr,PF_Automatable,20.0f,56},
    {"gen_chorus_sync","SYNC","",ParamKind::Enum,Taper::Linear,0.0f,6.0f,0.0f,1.f,7,syncNames,PF_Automatable,20.0f,57},
    {"gen_chorus_on","ON","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,1.0f,1.f,0,nullptr,PF_Engage,20.0f,58},
};
// ESTIMATED admission budget from GENERIC_PEDALS; highest documented runtime mode.
const PedalDesc description {"generic.chorus","gen_chorus_",1,"Chorus","Modulation",nullptr,ChannelMode::MonoInStereoOut,BypassMode::True,int(std::size(parameters)),parameters,0,false,false,false,800,1000};
}
PreparedMemoryRequirement Chorus::prepareMemory(double sr,int,int,const float*) noexcept
{
    generic::detail::TimeFxMemoryEstimate memory(sr,sizeof(Chorus));
    memory.delay(.025,8);
    return memory.result();
}
uint64_t Chorus::getPreparedMemoryBytes() const noexcept
{
    uint64_t bytes=sizeof(*this);
    if(!addPreparedBytes(bytes,delay.ownedMemoryBytes(),bytes))return kUnknownPreparedMemoryBytes;
    return bytes;
}
const PedalDesc& Chorus::descriptor() noexcept { return description; }
Chorus::Chorus() : PedalBase(description) {}
void Chorus::prepare(double sampleRate,int,int)
{
    sr=sampleRate; prepareParams(sr); delay.prepare(int(sr*0.025)+8);
    highpass.setLowpass(sr,100); clock.prepare(sr); reset();
}
void Chorus::reset() noexcept
{
    delay.reset(); highpass.reset(); for(auto& f:tone) f.reset(); clock.reset();
    mode=previousMode=int(value(4)); modeBlend=1;
}
void Chorus::setTime(const TimeInfo& t) noexcept
{
    time=t; clock.time(t,detail::modulationQuarters(int(value(7))));
}
void Chorus::process(double* left,double* right,int n) noexcept
{
    for(int i=0;i<n;++i)
    {
        const double dry[2] {softGuard(left[i]),softGuard(right[i])};
        const double depth=nextSmoothed(1), mix=nextSmoothed(2), cutoff=nextSmoothed(3), spread=nextSmoothed(6);
        const int requested=int(value(4));
        if(requested!=mode) { previousMode=mode; mode=requested; modeBlend=0; }
        modeBlend=std::min(1.0,modeBlend+1/(sr*.005));
        delay.push(highpass.highpass((dry[0]+dry[1])*.5));
        auto render=[&](int selected,int channel) noexcept
        {
            const double d=selected==2 ? .35 : depth;
            const double base=selected==2 ? 4 : selected==1 ? 5 : 7;
            const double modulation=d*4.5*(selected==1 ? .6 : 1);
            const double phase=clock.phase()+channel*spread*.5;
            const double wet=delay.readBandlimited(sr*.001*(base+modulation*detail::wave(phase,value(5)>.5f)));
            return (selected==2 && channel==1 ? -1.0 : 1.0)*std::tanh(.7*wet)/.7*.9;
        };
        double* outputs[] {left,right};
        for(int c=0;c<2;++c)
        {
            tone[c].setLowpass(sr,cutoff);
            const double wet=tone[c].process(render(previousMode,c)*(1-modeBlend)+render(mode,c)*modeBlend);
            const double m=(previousMode==1 ? 1:mix)*(1-modeBlend)+(mode==1 ? 1:mix)*modeBlend;
            outputs[c][i]=softGuard(dry[c]*std::cos(m*detail::pi*.5)+wet*std::sin(m*detail::pi*.5));
        }
        const double rate=nextSmoothed(0);
        clock.advance(mode==2 ? .4 : detail::tempoRate(time,detail::modulationQuarters(int(value(7))),rate));
    }
}
}
