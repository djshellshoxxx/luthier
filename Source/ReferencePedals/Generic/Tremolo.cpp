// OWNER: WP9
#include "Tremolo.h"

namespace luthier::reference::generic
{
namespace
{
const char* const waveNames[]{"Sine","Triangle","Square","Opto","Harmonic"};
const char* const stereoNames[]{"Mono","Pan","Dual-phase"};
const char* const syncNames[]{"Off","1/4","1/4T","1/8","1/8.","1/8T","1/16","1/2","1 bar","2 bars"};
const ParamDesc parameters[] {
    {"gen_trem_rate","RATE","Hz",ParamKind::Continuous,Taper::LogA,0.2f,16.0f,4.5f,1.f,0,nullptr,PF_Automatable,20.0f,104},
    {"gen_trem_depth","DEPTH","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.6f,1.f,0,nullptr,PF_Automatable,20.0f,105},
    {"gen_trem_wave","WAVE","",ParamKind::Enum,Taper::Linear,0.0f,4.0f,0.0f,1.f,5,waveNames,PF_Automatable,20.0f,106},
    {"gen_trem_shape","SHAPE","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.5f,1.f,0,nullptr,PF_Automatable,20.0f,107},
    {"gen_trem_stereo","STEREO","",ParamKind::Enum,Taper::Linear,0.0f,2.0f,0.0f,1.f,3,stereoNames,PF_Automatable,20.0f,108},
    {"gen_trem_phase","PHASE","deg",ParamKind::Continuous,Taper::Linear,0.0f,180.0f,90.0f,1.f,0,nullptr,PF_Automatable,20.0f,109},
    {"gen_trem_sync","SYNC","",ParamKind::Enum,Taper::Linear,0.0f,9.0f,0.0f,1.f,10,syncNames,PF_Automatable,20.0f,110},
    {"gen_trem_level","LEVEL","dB",ParamKind::Continuous,Taper::Linear,-12.0f,6.0f,0.0f,1.f,0,nullptr,PF_Automatable,20.0f,111},
    {"gen_trem_tap","TAP","",ParamKind::Trigger,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Automatable,20.0f,112},
    {"gen_trem_on","ON","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,1.0f,1.f,0,nullptr,PF_Engage,20.0f,113},
};
const PedalDesc description {"generic.tremolo","gen_trem_",1,"Tremolo","Modulation",nullptr,ChannelMode::Stereo,BypassMode::DSP,int(std::size(parameters)),parameters,0,false,false,false,0};
}
const PedalDesc& Tremolo::descriptor() noexcept { return description; }
Tremolo::Tremolo() : PedalBase(description) {}
void Tremolo::prepare(double sampleRate,int,int)
{
    sr=sampleRate; prepareParams(sr); clock.prepare(sr);
    for(int c=0;c<2;++c) { low[c].setLowpass(sr,700,.5); high[c].setHighpass(sr,700,.5); }
    reset();
}
void Tremolo::reset() noexcept
{
    clock.reset(); for(int c=0;c<2;++c) { low[c].reset(); high[c].reset(); opto[c]=0; }
    samples=0; lastTap=-1; intervalCount=intervalIndex=0;
    wave=oldWave=int(value(2)); waveBlend=1;
}
void Tremolo::setTime(const TimeInfo& t) noexcept { time=t; clock.time(t,detail::tremoloQuarters(int(value(6)))); }
void Tremolo::onTrigger(int index) noexcept
{
    if(index!=8) return;
    if(lastTap>=0)
    {
        const double interval=(samples-lastTap)/sr;
        if(interval>=1.0/16 && interval<=5)
        {
            intervals[intervalIndex]=interval; intervalIndex=(intervalIndex+1)%3;
            intervalCount=std::min(3,intervalCount+1);
            double sum=0; for(int k=0;k<intervalCount;++k) sum+=intervals[k];
            setParam(0,float(description.params[0].toNormalised(intervalCount/sum)));
        }
        else intervalCount=intervalIndex=0;
    }
    lastTap=samples;
}
void Tremolo::process(double* left,double* right,int n) noexcept
{
    for(int i=0;i<n;++i,++samples)
    {
        const double depth=nextSmoothed(1), shape=nextSmoothed(3), offset=nextSmoothed(5)/360;
        const double levelDb=nextSmoothed(7);
        const double level=std::abs(levelDb)<1e-6 ? 1:std::pow(10,levelDb/20);
        const int requested=int(value(2));
        if(requested!=wave) { oldWave=wave; wave=requested; waveBlend=0; }
        waveBlend=std::min(1.0,waveBlend+1/(.005*sr));
        const int stereo=int(value(4));
        double* outputs[]{left,right};
        for(int c=0;c<2;++c)
        {
            const double phase=clock.phase()+(stereo==2 ? c*offset:0);
            const double sinus=detail::wave(phase,false);
            const double target=(1-sinus)*.5;
            opto[c]+=(target-opto[c])*(1-std::exp(-1/(sr*(target>opto[c] ? .008:.040))));
            auto modulation=[&](int selected) noexcept
            {
                if(selected==1) return (1-detail::wave(phase+.25,true))*.5;
                if(selected==2) { const double k=1+40*shape; return (1-std::tanh(k*(sinus-std::cos(detail::pi*std::clamp(shape,.01,.99)))))*.5; }
                if(selected==3) return std::pow(opto[c],1.5);
                return target;
            };
            const double u=modulation(oldWave)*(1-waveBlend)+modulation(wave)*waveBlend;
            const double dry=softGuard(outputs[c][i]);
            const double lo=low[c].process(dry), hi=-high[c].process(dry);
            const double harmonic=lo*(1-depth*u)+hi*(1-depth*(1-u));
            double ordinary=dry*(1-depth*u);
            if(stereo==1)
            {
                const double angle=detail::pi*.25+detail::pi*.25*depth*(2*u-1);
                ordinary=dry*(c==0 ? std::cos(angle):std::sin(angle));
            }
            const double old=oldWave==4 ? harmonic:ordinary, current=wave==4 ? harmonic:ordinary;
            outputs[c][i]=softGuard((old+(current-old)*waveBlend)*level);
        }
        clock.advance(detail::tempoRate(time,detail::tremoloQuarters(int(value(6))),nextSmoothed(0)));
    }
}
}
