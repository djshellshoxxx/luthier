// OWNER: WP14
#include "boss_ce2w.h"
#include "../Common/TempoSync.h"
namespace luthier::reference::models
{
namespace
{
const char* const modeNames[]{"Standard","CE-1 Chorus","CE-1 Vibrato"};
const char* const outputNames[]{"A only","Stereo"};
const char* const divNames[]{"1/1","1/2","1/4","1/8","1/4T","1/16"};
const ParamDesc parameters[]{
    {"boss_ce2w_rate","RATE","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.35f,1.f,0,nullptr,PF_Automatable,20.0f,14},
    {"boss_ce2w_depth","DEPTH","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.5f,1.f,0,nullptr,PF_Automatable,20.0f,15},
    {"boss_ce2w_mode","MODE","",ParamKind::Enum,Taper::Linear,0.0f,2.0f,0.0f,1.f,3,modeNames,PF_Automatable,20.0f,16},
    {"boss_ce2w_bypass","ON","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,1.0f,1.f,0,nullptr,PF_Engage,5.0f,17},
    {"boss_ce2w_out_mode","OUTPUT","",ParamKind::Enum,Taper::Linear,0.0f,1.0f,1.0f,1.f,2,outputNames,PF_Automatable,20.0f,18},
    {"boss_ce2w_sync","SYNC","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Ext,20.0f,-1},
    {"boss_ce2w_sync_div","DIVISION","",ParamKind::Enum,Taper::Linear,0.0f,5.0f,1.0f,1.f,6,divNames,PF_Ext,20.0f,-1},
    {"boss_ce2w_noise","NOISE","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,1.0f,1.f,0,nullptr,PF_Config,20.0f,-1},
    {"boss_ce2w_hq","BBD STAIRCASE","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Config,20.0f,-1},
};
const PedalDesc description{"boss-ce2w","boss_ce2w_",1,"BossCe2w (prototype)","Chorus","BossCe2w",ChannelMode::MonoInStereoOut,BypassMode::Buffered,9,parameters,0,false,false,false,0};
}
const PedalDesc& BossCe2w::descriptor() noexcept {return description;}
BossCe2w::BossCe2w() : PedalBase(description) {}
void BossCe2w::prepare(double sampleRate,int,int)
{
    sr=sampleRate;prepareParams(sr);
    for(int m=0;m<3;++m){wet[m].prepare(sr,40,6590,6930);wet[m].setClipLevel(4.5);emphasis[m].setShelf(sr,1000,6,true);deemphasis[m].setShelf(sr,1000,-6,true);}
    inputHp.setLowpass(sr,10);for(auto& filter:bypassHp)filter.setLowpass(sr,10);lfo.prepare(sr);reset();
}
void BossCe2w::reset() noexcept
{
    for(int m=0;m<3;++m){wet[m].reset(seed);emphasis[m].reset();deemphasis[m].reset();}
    inputHp.reset();for(auto& filter:bypassHp)filter.reset();lfo.setPhase(0);lastRate=-1;mode=oldMode=int(value(2));modeBlend=1;stereoBlend=value(4);
}
void BossCe2w::setTime(const TimeInfo& t) noexcept
{
    time=t;constexpr double quarters[]{4,2,1,.5,2.0/3,.25};
    if(value(5)>.5f && t.valid && t.playing && std::isfinite(t.ppq))lfo.setPhase(common::TempoSync::phase(t.ppq,quarters[int(value(6))]));
}
void BossCe2w::onTransport(TransportEvent event,int) noexcept {if(value(5)>.5f && event==TransportEvent::Start)lfo.setPhase(0);}
void BossCe2w::setExpression(int jack,float v) noexcept {if(jack==0)setParam(1,v);else if(jack==1)setParam(0,v);}
void BossCe2w::process(double* left,double* right,int n) noexcept
{
    constexpr double quarters[]{4,2,1,.5,2.0/3,.25},centres[]{6.5,7,4.5},depths[]{4,3.5,2.5};
    for(int i=0;i<n;++i)
    {
        const double dry[2]{softGuard(left[i]),softGuard(right[i])};
        const double rateKnob=nextSmoothed(0);if(rateKnob!=lastRate){lastRate=rateKnob;cachedRate=.3*std::pow(4/.3,rateKnob);}
        const double input=inputHp.highpass((dry[0]+dry[1])*.5),rate=cachedRate,depth=nextSmoothed(1);
        if(mode!=int(value(2))){oldMode=mode;mode=int(value(2));modeBlend=0;}
        modeBlend=std::min(1.,modeBlend+1/(sr*.01));
        stereoBlend+=std::clamp(double(value(4))-stereoBlend,-1/(sr*.01),1/(sr*.01));
        auto render=[&](int m) noexcept
        {
            double coloured=input;
            if(m>0)coloured=4.5*(std::tanh(input/4.5+.025)-std::tanh(.025));
            const double sample=emphasis[m].process(coloured);
            const double delayed=wet[m].process(sample,centres[m]+depths[m]*depth*lfo.triangle(),value(7)>.5f ? .4:0,value(8)>.5f);
            const double w=deemphasis[m].process(delayed),d=m==2?0:input;
            return std::array<double,2>{d+w,d-w};
        };
        // Keep every delay/filter history current while its mode is inaudible.
        // Returning to a mode must never replay audio from its last activation.
        const std::array<std::array<double,2>,3> voices{render(0),render(1),render(2)};
        const auto& current=voices[mode];const auto& old=voices[oldMode];
        const double a=old[0]+(current[0]-old[0])*modeBlend,b=old[1]+(current[1]-old[1])*modeBlend;
        left[i]=softGuard(a); // E60: board owns buffered engagement.
        right[i]=softGuard(a+(b-a)*stereoBlend);
        lfo.setRate(value(5)>.5f && time.valid ? std::min(10.,common::TempoSync::rate(time.bpm,quarters[int(value(6))])):rate);lfo.advance();
    }
}
void BossCe2w::processBypass(double* left,double* right,int n) noexcept
{
    for(int i=0;i<n;++i){left[i]=softGuard(bypassHp[0].highpass(softGuard(left[i])));right[i]=softGuard(bypassHp[1].highpass(softGuard(right[i])));}
}
}
