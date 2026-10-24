// OWNER: WP14
#include "mxr_phase90_script.h"
#include "../Common/TempoSync.h"
namespace luthier::reference::models
{
namespace
{
const char* const variantNames[]{"Script","Block (estimated)"};
const char* const syncNames[]{"Off","Clock"};
const char* const divNames[]{"1/1","1/2","1/4","1/8","1/8T","1/16"};
const char* const stereoModeNames[]{"Dual mono","Mono sum"};
const ParamDesc parameters[]{
    {"mxr_phase90_script_speed","SPEED","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.5f,1.f,0,nullptr,PF_Automatable,50.0f,-1},
    {"mxr_phase90_script_bypass","ON","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Engage,20.0f,-1},
    {"mxr_phase90_script_variant","VARIANT","",ParamKind::Enum,Taper::Linear,0.0f,1.0f,0.0f,1.f,2,variantNames,PF_Config,20.0f,-1},
    {"mxr_phase90_script_depth_trim","SWEEP DEPTH","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,1.0f,1.f,0,nullptr,PF_Automatable,20.0f,-1},
    {"mxr_phase90_script_mix_trim","WET TRIM","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,1.0f,1.f,0,nullptr,PF_Automatable,20.0f,-1},
    {"mxr_phase90_script_stage_mismatch","MISMATCH","",ParamKind::Continuous,Taper::Linear,0.0f,0.1f,0.02f,1.f,0,nullptr,PF_Ext,20.0f,-1},
    {"mxr_phase90_script_sync","SYNC","",ParamKind::Enum,Taper::Linear,0.0f,1.0f,0.0f,1.f,2,syncNames,PF_Ext,20.0f,-1},
    {"mxr_phase90_script_sync_div","DIVISION","",ParamKind::Enum,Taper::Linear,0.0f,5.0f,2.0f,1.f,6,divNames,PF_Ext,20.0f,-1},
    {"mxr_phase90_script_stereo_phase","R PHASE","deg",ParamKind::Continuous,Taper::Linear,0.0f,180.0f,0.0f,1.f,0,nullptr,PF_Automatable,20.0f,-1},
    {"mxr_phase90_script_ext_speed","EXT SPEED","CC",ParamKind::Continuous,Taper::Linear,0.0f,127.0f,64.0f,1.f,0,nullptr,PF_Automatable,5.0f,-1},
    {"mxr_phase90_script_reset_on_stomp","RESET ON STOMP","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Ext,20.0f,-1},
    {"mxr_phase90_script_noise","HISS","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Config,20.0f,-1},
    {"mxr_phase90_script_stereo_mode","STEREO MODE","",ParamKind::Enum,Taper::Linear,0,1,0,1,2,stereoModeNames,PF_Config,5,-1},
};
const PedalDesc description{"mxr-phase90-script","mxr_phase90_script_",1,"Phase 90 Script (prototype)","Phaser","MXR Phase 90 Script",ChannelMode::Mono,BypassMode::True,13,parameters,0,false,false,false,0};
}
const PedalDesc& MxrPhase90Script::descriptor() noexcept {return description;}
MxrPhase90Script::MxrPhase90Script() : PedalBase(description) {}
void MxrPhase90Script::prepare(double sampleRate,int,int)
{
    sr=sampleRate;triangleCoefficient=1-std::exp(-1/(sr*.02));fadeStep=1/(sr*.005);prepareParams(sr);oscillator.prepare(sr);noise.reset(seed);
    for(auto& v:mismatch)v=noise.next();for(auto& f:highpass)f.setLowpass(sr,1.6);reset();
}
void MxrPhase90Script::reset() noexcept
{
    states={};feedback[0]=feedback[1]=0;triangle[0]=triangle[1]=.5;
    oscillator.setPhase(0);noise.reset(seed);for(auto& f:highpass)f.reset();engage=enabled ? 1:0;
    coefficientClock=0;lastSpeed=lastVariant=-1;
    for(int c=0;c<2;++c)for(int k=0;k<4;++k)
    {
        const double g=std::tan(common::pi*330*std::sqrt(10.)*(1+mismatch[k]*value(5))/sr);
        allpass[c][k]=(g-1)/(g+1);allpassStep[c][k]=0;
    }
}
void MxrPhase90Script::setEnabled(bool e) noexcept
{
    if(e && !enabled && value(10)>.5f)oscillator.setPhase(0);
    enabled=e;
}
void MxrPhase90Script::setTime(const TimeInfo& t) noexcept
{
    time=t;
    constexpr double quarters[]{4,2,1,.5,1.0/3,.25};
    if(value(6)>.5f && t.valid && t.playing && std::isfinite(t.ppq))oscillator.setPhase(common::TempoSync::phase(t.ppq,quarters[int(value(7))]));
}
void MxrPhase90Script::onTransport(TransportEvent event,int) noexcept
{
    if(value(6)>.5f && (event==TransportEvent::Start || event==TransportEvent::Continue))oscillator.setPhase(0);
}
void MxrPhase90Script::setExpression(int jack,float v) noexcept {if(jack==0)setParam(0,v);}
void MxrPhase90Script::onParamChanged(int index,float v) noexcept {if(index==9)setParam(0,v/127);}
void MxrPhase90Script::process(double* left,double* right,int n) noexcept
{
    constexpr double pi=3.14159265358979323846,quarters[]{4,2,1,.5,1.0/3,.25};
    for(int i=0;i<n;++i)
    {
        const double speed=nextSmoothed(0),depth=nextSmoothed(3),mix=nextSmoothed(4),offset=nextSmoothed(8)/360;
        if(speed!=lastSpeed || value(2)!=lastVariant){lastSpeed=speed;lastVariant=value(2);cachedRate=.3*std::pow(lastVariant>.5f ? 16/.3:25,speed);}
        const double rate=value(6)>.5f && time.valid ? std::clamp(common::TempoSync::rate(time.bpm,quarters[int(value(7))]),.1,16.):cachedRate;
        engage+=std::clamp((enabled?1.:0.)-engage,-fadeStep,fadeStep);
        const double source[2]{softGuard(left[i]),softGuard(right[i])};
        double* output[]{left,right};
        for(int c=0;c<2;++c)
        {
            double phase=oscillator.phase()+c*offset;phase-=std::floor(phase);
            const double tri=phase<.65 ? phase/.65:(1-phase)/.35;
            triangle[c]+=(tri-triangle[c])*triangleCoefficient;
            if(coefficientClock==0)
            {
                const double frequency=330*std::pow(10,depth*(triangle[c]-.5)+.5);
                for(int k=0;k<4;++k)
                {
                    const double g=std::tan(pi*frequency*(1+mismatch[k]*value(5))/sr);
                    allpassStep[c][k]=((g-1)/(g+1)-allpass[c][k])*.125;
                }
            }
            const double dry=source[c],signal=value(12)>.5f ? (source[0]+source[1])*.5:dry,input=highpass[c].highpass(signal);
            double x=input+(value(2)>.5f ? .35*std::tanh(feedback[c]):0);
            for(int k=0;k<4;++k)
            {
                const double a=allpass[c][k]+=allpassStep[c][k];
                const double y=a*x+states[c][k];states[c][k]=x-a*y;
                if(std::abs(states[c][k])<1e-30)states[c][k]=0;
                x=y/std::sqrt(1+.04*y*y); // Estimated mild per-stage JFET law.
            }
            feedback[c]=x;
            const double hiss=value(11)>.5f ? noise.next()*1.4e-5:0;
            const double wet=.5*(input+x*mix)+hiss;
            output[c][i]=softGuard(dry+(wet-dry)*engage);
        }
        coefficientClock=(coefficientClock+1)&7;
        oscillator.setRate(rate);if(enabled)oscillator.advance();
    }
}
}
