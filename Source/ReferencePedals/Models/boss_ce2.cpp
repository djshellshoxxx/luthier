// OWNER: WP14
#include "boss_ce2.h"
#include "../Common/TempoSync.h"
namespace luthier::reference::models
{
namespace
{
const char* const variantNames[]{"Standard","Silver screw (estimated)"};
const char* const divNames[]{"1/1","1/2","1/4","1/8","1/4T","1/8T","1/16"};
const char* const stereoModeNames[]{"Dual mono","Mono sum"};
const ParamDesc parameters[]{
    {"boss_ce2_rate","RATE","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.4f,1.f,0,nullptr,PF_Automatable,20.0f,14},
    {"boss_ce2_depth","DEPTH","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.5f,1.f,0,nullptr,PF_Automatable,20.0f,15},
    {"boss_ce2_bypass","ON","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,1.0f,1.f,0,nullptr,PF_Engage,5.0f,85},
    {"boss_ce2_variant","VARIANT","",ParamKind::Enum,Taper::Linear,0.0f,1.0f,0.0f,1.f,2,variantNames,PF_Config,20.0f,-1},
    {"boss_ce2_bbd_mode","BBD SIM","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Automatable,20.0f,-1},
    {"boss_ce2_noise","NOISE","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Config,50.0f,-1},
    {"boss_ce2_sync","SYNC","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Ext,20.0f,-1},
    {"boss_ce2_sync_div","DIVISION","",ParamKind::Enum,Taper::Linear,0.0f,6.0f,2.0f,1.f,7,divNames,PF_Ext,20.0f,16},
    {"boss_ce2_sync_reset","RESET ON START","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,1.0f,1.f,0,nullptr,PF_Ext,20.0f,-1},
    {"boss_ce2_tap_enable","TAP ENABLE","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Ext,20.0f,-1},
    {"boss_ce2_at_depth","AFTERTOUCH DEPTH","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Ext,20.0f,-1},
    {"boss_ce2_lfo_phase_offset","LFO PHASE","deg",ParamKind::Continuous,Taper::Linear,0.0f,360.0f,0.0f,1.f,0,nullptr,PF_Ext,20.0f,-1},
    {"boss_ce2_mono_out_dup","MONO DUP","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,1.0f,1.f,0,nullptr,PF_Config,20.0f,-1},
    {"boss_ce2_stereo_mode","STEREO MODE","",ParamKind::Enum,Taper::Linear,0,1,0,1,2,stereoModeNames,PF_Config,5,-1},
};
const PedalDesc description{"boss-ce2","boss_ce2_",1,"Vintage Bucket Chorus (prototype)","Chorus",nullptr,ChannelMode::Mono,BypassMode::Buffered,14,parameters,0,false,false,false,0};
}
const PedalDesc& BossCe2::descriptor() noexcept {return description;}
BossCe2::BossCe2() : PedalBase(description) {}
void BossCe2::prepare(double sampleRate,int,int)
{
    sr=sampleRate;triangleCoefficient=1-std::exp(-6.283185307179586*72/sr);prepareParams(sr);variant=int(value(3));
    for(int c=0;c<2;++c){wet[c].prepare(sr,12,variant?7500:6500,variant?6000:5500);wet[c].setClipOnset(.5);inputHp[c].setLowpass(sr,15);bypassHp[c].setLowpass(sr,15);}
    lfo.prepare(sr);reset();
}
void BossCe2::reset() noexcept
{
    for(int c=0;c<2;++c){wet[c].reset(seed);inputHp[c].reset();bypassHp[c].reset();}
    lfo.setPhase(0);triangle=0;lastRate=-1;force=0;samples=0;lastTap=-1;tapIndex=tapCount=0;
}
void BossCe2::setTime(const TimeInfo& t) noexcept
{
    time=t;constexpr double quarters[]{4,2,1,.5,2.0/3,1.0/3,.25};
    if(value(6)>.5f && t.valid && t.playing && std::isfinite(t.ppq))lfo.setPhase(common::TempoSync::phase(t.ppq,quarters[int(value(7))]));
}
void BossCe2::onTransport(TransportEvent event,int) noexcept
{
    if(value(8)>.5f && event==TransportEvent::Start)lfo.setPhase(0);
}
void BossCe2::setExpression(int jack,float v) noexcept {if(jack==0)setParam(1,v);else if(jack==1)setParam(0,v);}
void BossCe2::onFoot(const FootEvent& event) noexcept
{
    force=std::clamp(double(event.force),0.,1.);
    if(!event.press || value(9)<.5f)return;
    if(lastTap>=0)
    {
        const double interval=(samples-lastTap)/sr;
        if(interval>=.2 && interval<=.5)
        {
            taps[tapIndex]=interval;tapIndex=(tapIndex+1)%3;tapCount=std::min(3,tapCount+1);
            double sum=0;for(int k=0;k<tapCount;++k)sum+=taps[k];
            const double hz=std::clamp(tapCount/sum,.3,3.56);setParam(0,float(std::log(hz/.3)/std::log(3.56/.3)));
        }
        else if(interval>2)tapIndex=tapCount=0;
    }
    lastTap=samples;
}
void BossCe2::process(double* left,double* right,int n) noexcept
{
    constexpr double quarters[]{4,2,1,.5,2.0/3,1.0/3,.25};
    for(int i=0;i<n;++i,++samples)
    {
        if(variant!=int(value(3))) {variant=int(value(3));for(auto& b:wet)b.setFilters(variant?7500:6500,variant?6000:5500);}
        const double knob=nextSmoothed(0);if(knob!=lastRate){lastRate=knob;cachedRate=.3*std::pow(3.56/.3,knob);}const double rate=cachedRate;
        const double depth=std::clamp(double(nextSmoothed(1))+(value(10)>.5f ? force*.5:0),0.,1.);
        double phase=lfo.phase()+nextSmoothed(11)/360;phase-=std::floor(phase);
        const double tri=4*std::abs(phase-.5)-1;
        triangle+=(tri-triangle)*triangleCoefficient;
        const double delayMs=3.4+depth*triangle*(triangle>=0?1.1:1.2),noise=nextSmoothed(5);
        const double dry[2]{softGuard(left[i]),softGuard(right[i])};double* outputs[]{left,right};
        const double mono=(dry[0]+dry[1])*.5;
        for(int c=0;c<2;++c)
        {
            const double input=value(13)>.5f?mono:dry[c];
            const double buffered=inputHp[c].highpass(input);
            const double dryPath=value(13)>.5f && value(12)<.5f?dry[c]:buffered;
            const double processed=dryPath+.9440608762859234*wet[c].process(buffered,delayMs,noise,value(4)>.5f);
            outputs[c][i]=softGuard(processed); // E60: board owns buffered engagement.
        }
        lfo.setRate(value(6)>.5f && time.valid ? common::TempoSync::rate(time.bpm,quarters[int(value(7))]):rate);lfo.advance();
    }
}
void BossCe2::processBypass(double* left,double* right,int n) noexcept
{
    for(int i=0;i<n;++i){left[i]=softGuard(bypassHp[0].highpass(softGuard(left[i])));right[i]=softGuard(bypassHp[1].highpass(softGuard(right[i])));}
}
}
