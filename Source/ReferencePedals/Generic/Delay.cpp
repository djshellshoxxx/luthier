// OWNER: WP10
#include "Delay.h"

namespace luthier::reference::generic
{
namespace
{
const char* const modeNames[]{"Digital","Analog","Tape"};
const char* const subNames[]{"1/4","1/8","1/8.","1/8T","1/16","1/2","1"};
const char* const syncNames[]{"Off","Host/Clock","Tap"};
const char* const pingNames[]{"Off","On","Wide"};
const char* const trailNames[]{"Cut","Spillover"};
const ParamDesc parameters[] {
    {"gen_delay_time","TIME","ms",ParamKind::Continuous,Taper::LogA,20.0f,2000.0f,380.0f,1.f,0,nullptr,PF_Automatable | PF_OwnSmoothing,60.0f,59},
    {"gen_delay_feedback","FEEDBACK","",ParamKind::Continuous,Taper::Linear,0.0f,1.1f,0.35f,1.f,0,nullptr,PF_Automatable,20.0f,60},
    {"gen_delay_mix","MIX","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.35f,1.f,0,nullptr,PF_Automatable,20.0f,61},
    {"gen_delay_tone","TONE","Hz",ParamKind::Continuous,Taper::LogA,1200.0f,12000.0f,5000.0f,1.f,0,nullptr,PF_Automatable,20.0f,62},
    {"gen_delay_mode","MODE","",ParamKind::Enum,Taper::Linear,0.0f,2.0f,0.0f,1.f,3,modeNames,PF_Automatable,20.0f,63},
    {"gen_delay_mod","MOD","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.15f,1.f,0,nullptr,PF_Automatable,20.0f,65},
    {"gen_delay_sub","SUBDIV","",ParamKind::Enum,Taper::Linear,0.0f,6.0f,0.0f,1.f,7,subNames,PF_Automatable,20.0f,66},
    {"gen_delay_sync","SYNC","",ParamKind::Enum,Taper::Linear,0.0f,2.0f,0.0f,1.f,3,syncNames,PF_Automatable,20.0f,67},
    {"gen_delay_pingpong","PING-PONG","",ParamKind::Enum,Taper::Linear,0.0f,2.0f,0.0f,1.f,3,pingNames,PF_Automatable,20.0f,68},
    {"gen_delay_spread","STEREO","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Automatable,20.0f,-1},
    {"gen_delay_trails","TRAILS","",ParamKind::Enum,Taper::Linear,0.0f,1.0f,1.0f,1.f,2,trailNames,PF_Automatable,20.0f,-1},
    {"gen_delay_hold","HOLD","",ParamKind::Momentary,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Automatable,20.0f,69},
    {"gen_delay_on","ON","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,1.0f,1.f,0,nullptr,PF_Engage,20.0f,70},
    {"gen_delay_tap","TAP","",ParamKind::Trigger,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Automatable,20.0f,85},
};
const PedalDesc description {"generic.delay","gen_delay_",1,"Delay","Time",nullptr,ChannelMode::TrueStereo,BypassMode::DSP,int(std::size(parameters)),parameters,0,true,false,false,0};
}
const PedalDesc& Delay::descriptor() noexcept { return description; }
Delay::Delay() : PedalBase(description) {}
void Delay::prepare(double sampleRate,int,int)
{
    sr=sampleRate; prepareParams(sr);
    for(auto& line:lines) line.prepare(int(sr*2.4)+8);
    wow.prepare(sr); wow.setRate(.6); flutter.prepare(sr); flutter.setRate(7);
    reset();
}
void Delay::reset() noexcept
{
    for(auto& line:lines) line.reset();
    for(int c=0;c<2;++c) { lowpass[c].reset(); highpass[c].reset(); }
    noise.reset(seed); drift=driftTarget=driftPhase=0;
    wow.setPhase(0); flutter.setPhase(.25); energy=0; samples=0; lastTap=-1;
    currentMs=targetMs=oldMs=desiredTime(); tapBlend=1;
    inputGain=enabled ? 1:0; tailGain=enabled || value(10)>.5f ? 1:0; holdBlend=0;
}
void Delay::setEnabled(bool state) noexcept { enabled=state; }
void Delay::setTime(const TimeInfo& t) noexcept
{
    // A lost clock retains the last valid tempo, without inventing a new one.
    if(t.valid && std::isfinite(t.bpm) && t.bpm>0 && (t.playing || t.clockLocked)) time=t;
}
double Delay::desiredTime() const noexcept
{
    constexpr double quarters[]{1,.5,.75,1.0/3,.25,2,4};
    double ms=value(0);
    if(int(value(7))==1 && time.valid) ms=60000/time.bpm*quarters[int(value(6))];
    if(int(value(7))==2) ms=tapMs*quarters[int(value(6))];
    while(ms>2000) ms*=.5;
    return std::clamp(ms,20.0,2000.0);
}
void Delay::onTrigger(int index) noexcept
{
    if(index!=13) return;
    if(lastTap>=0)
    {
        const double interval=1000*(samples-lastTap)/sr;
        if(interval>=60 && interval<=2000){tapMs=interval;publishedTapMs.store(tapMs,std::memory_order_release); }
    }
    lastTap=samples;
}
float Delay::getTailEnergyDb() const noexcept { return float(10*std::log10(std::max(1e-20,energy))); }
void Delay::saveExtra(juce::var& v) const
{
    if(!v.isObject()) v=new juce::DynamicObject();
    v.getDynamicObject()->setProperty("tapMs",publishedTapMs.load(std::memory_order_acquire));
}
void Delay::loadExtra(const juce::var& v)
{
    const double saved=double(v.getProperty("tapMs",500.));
    if(std::isfinite(saved)){tapMs=std::clamp(saved,60.0,2000.0);publishedTapMs.store(tapMs,std::memory_order_release); }
}
void Delay::process(double* left,double* right,int n) noexcept
{ render(left,right,nullptr,nullptr,n); }
bool Delay::processWithWetResidual(double* left,double* right,double* residualL,double* residualR,int n) noexcept
{ render(left,right,residualL,residualR,n);return true; }
void Delay::render(double* left,double* right,double* residualL,double* residualR,int n) noexcept
{
    constexpr double pi=3.14159265358979323846;
    for(int i=0;i<n;++i,++samples)
    {
        const double dry[2]{softGuard(left[i]),softGuard(right[i])};
        const int mode=int(value(4)),ping=int(value(8));
        const double mix=nextSmoothed(2),cutoff=nextSmoothed(3),mod=nextSmoothed(5),spread=nextSmoothed(9);
        const double feedback=std::min(.98,double(nextSmoothed(1)));
        const double desired=desiredTime();
        if(mode==0)
        {
            if(tapBlend>=1 && std::abs(desired-targetMs)>1e-6)
            {
                oldMs=targetMs; targetMs=desired; tapBlend=0;
            }
            tapBlend=std::min(1.0,tapBlend+1/(sr*.04));
            currentMs=targetMs;
        }
        else { currentMs+=(desired-currentMs)*(1-std::exp(-1/(sr*.060))); targetMs=oldMs=currentMs; tapBlend=1; }
        const double holdTarget=value(11)>.5f ? 1:0;
        holdBlend+=std::clamp(holdTarget-holdBlend,-1/(sr*.05),1/(sr*.005));
        const double inputTarget=enabled ? 1:0;
        inputGain+=std::clamp(inputTarget-inputGain,-1/(sr*.005),1/(sr*.005));
        const double tailTarget=enabled || value(10)>.5f ? 1:0;
        tailGain+=std::clamp(tailTarget-tailGain,-1/(sr*.020),1/(sr*.020));
        driftPhase+=.3/sr;
        if(driftPhase>=1) { driftPhase-=1; driftTarget=noise.next(); }
        drift+=(driftTarget-drift)*(1-std::exp(-2*pi*.3/sr));
        const double modulation=mode==0 ? 0: mode==1 ? mod*.8*wow.sine():mod*(1.2*wow.sine()+.15*flutter.sine()+.5*drift);
        double wet[2]{};
        for(int c=0;c<2;++c)
        {
            const double ratio=c==1 ? (ping==2 ? 1.15:ping==0 ? 1-.25*spread:1):1;
            const double haas=c==1 && ping==2 ? 2:0;
            // read precedes push: age 1 is two samples behind this input sample.
            const double delay=sr*.001*(currentMs*ratio+haas+modulation)-1;
            const double oldDelay=sr*.001*(oldMs*ratio+haas+modulation)-1;
            wet[c]=lines[c].read(delay)*tapBlend+lines[c].read(oldDelay)*(1-tapBlend);
        }
        for(int c=0;c<2;++c)
        {
            const double source=wet[ping ? 1-c:c];
            highpass[c].setLowpass(sr,mode==1 ? 150:mode==2 ? 80:20);
            lowpass[c].setLowpass(sr,mode==2 ? std::min(6000.,cutoff):cutoff);
            const double saturated=mode==1 ? std::tanh(1.2*source)/1.2:mode==2 ? std::tanh(.9*source):source;
            const double filtered=highpass[c].highpass(lowpass[c].process(saturated));
            const double loop=filtered*feedback*(1-holdBlend)+source*holdBlend;
            const double in=ping ? (c==0 ? (dry[0]+dry[1])*.5:0):dry[c];
            lines[c].push(softGuard(in*inputGain*(1-holdBlend)+loop*tailGain));
        }
        const double dryGain=1+inputGain*(std::cos(mix*pi*.5)-1);
        // E68: expose the real post-mix/trails wet term before guarded dry sum.
        if(residualL)residualL[i]=wet[0]*std::sin(mix*pi*.5)*tailGain;
        if(residualR)residualR[i]=wet[1]*std::sin(mix*pi*.5)*tailGain;
        left[i]=softGuard(dry[0]*dryGain+wet[0]*std::sin(mix*pi*.5)*tailGain);
        right[i]=softGuard(dry[1]*dryGain+wet[1]*std::sin(mix*pi*.5)*tailGain);
        energy+=(.5*(wet[0]*wet[0]+wet[1]*wet[1])-energy)*(1-std::exp(-1/(sr*.05)));
        wow.setRate(mode==1 ? .4:.6); wow.advance(); flutter.advance();
    }
}
}
