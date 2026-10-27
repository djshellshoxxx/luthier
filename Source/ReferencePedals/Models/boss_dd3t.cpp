// OWNER: WP14
#include "boss_dd3t.h"
#include <limits>
namespace luthier::reference::models
{
namespace
{
const char* const modeNames[]{"S (QT)","M (D8)","L (Q)","Short Loop"};
const char* const lawNames[]{"Linear ms","Hyperbolic"};
const char* const bitNames[]{"12","13","14","15","16","17","18","19","20","21","22","23","24"};
const char* const divNames[]{"1/4","1/8","1/8D","1/4T","1/16"};
const char* const variantNames[]{"DD-3T","Reserved (unavailable)"};
const char* const stereoModeNames[]{"Dual mono","Mono sum"};
const ParamDesc parameters[]{
    {"boss_dd3t_elevel","E.LEVEL","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.5f,1.f,0,nullptr,PF_Automatable,20.0f,16},
    {"boss_dd3t_feedback","FEEDBACK","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.4f,1.f,0,nullptr,PF_Automatable,20.0f,15},
    {"boss_dd3t_time","TIME","",ParamKind::Continuous,Taper::Linear,0.0f,1.0f,0.5f,1.f,0,nullptr,PF_Automatable | PF_OwnSmoothing,80.0f,14},
    {"boss_dd3t_mode","MODE","",ParamKind::Enum,Taper::Linear,0.0f,3.0f,2.0f,1.f,4,modeNames,PF_Automatable,20.0f,17},
    {"boss_dd3t_active","ON","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,1.0f,1.f,0,nullptr,PF_Engage,20.0f,85},
    {"boss_dd3t_loop_hold","SHORT LOOP HOLD","",ParamKind::Momentary,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Automatable,20.0f,87},
    {"boss_dd3t_tap","TAP","",ParamKind::Trigger,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Automatable,20.0f,86},
    {"boss_dd3t_tap_func","SWITCH TAP","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Ext,20.0f,-1},
    {"boss_dd3t_trails","TRAILS","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Automatable,20.0f,-1},
    {"boss_dd3t_time_law","TIME LAW","",ParamKind::Enum,Taper::Linear,0.0f,1.0f,1.0f,1.f,2,lawNames,PF_Automatable,20.0f,-1},
    {"boss_dd3t_bits","BITS","",ParamKind::Enum,Taper::Linear,0.0f,12.0f,0.0f,1.f,13,bitNames,PF_Config,20.0f,-1},
    {"boss_dd3t_conv_sim","CONVERTER SIM","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Config,20.0f,-1},
    {"boss_dd3t_sync","SYNC","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Ext,20.0f,-1},
    {"boss_dd3t_sync_div","DIVISION","",ParamKind::Enum,Taper::Linear,0.0f,4.0f,0.0f,1.f,5,divNames,PF_Ext,20.0f,-1},
    {"boss_dd3t_at_fb","AFTERTOUCH FEEDBACK","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Ext,20.0f,-1},
    {"boss_dd3t_mono_out_dup","MONO DUP","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,1.0f,1.f,0,nullptr,PF_Config,20.0f,-1},
    {"boss_dd3t_stereo_split","STEREO SPLIT","",ParamKind::Toggle,Taper::Linear,0.0f,1.0f,0.0f,1.f,0,nullptr,PF_Config,20.0f,-1},
    {"boss_dd3t_variant","VARIANT","",ParamKind::Enum,Taper::Linear,0.0f,1.0f,0.0f,1.f,2,variantNames,PF_Config,20.0f,-1},
    {"boss_dd3t_stereo_mode","STEREO MODE","",ParamKind::Enum,Taper::Linear,0,1,0,1,2,stereoModeNames,PF_Config,5,-1},
};
const PedalDesc description{"boss-dd3t","boss_dd3t_",1,"Quantized Digital Delay (prototype)","Delay",nullptr,ChannelMode::Mono,BypassMode::Buffered,19,parameters,0,true,false,false,0};
}
const PedalDesc& BossDd3t::descriptor() noexcept {return description;}
BossDd3t::BossDd3t() : PedalBase(description) {}
PreparedMemoryRequirement BossDd3t::prepareMemory(double rate,int maxBlock,int policy,const float*) noexcept
{
    // Match the pool's supported input range before converting buffer lengths.
    // These bounds also make every size conversion/addition below overflow-safe.
    if(!std::isfinite(rate) || rate<8000 || rate>1000000 || maxBlock<1 || policy<0 || policy>2)return {};
    // Fresh stereo delay/history/hold arrays. Ceil conservatively covers the
    // existing prepare() truncations; FractionalDelay adds16 beyond its argument.
    const auto longSamples=uint64_t(std::ceil(rate*.81)),holdSamples=uint64_t(std::ceil(rate*.8));
    const uint64_t payload=2*sizeof(double)*(longSamples+32+longSamples+16+holdSamples+8);
    // Filters and oversamplers are embedded. The immutable interpolation table
    // is shared, not a per-model allocation; no heap prepare scratch is owned.
    return {sizeof(BossDd3t)+payload,0};
}
uint64_t BossDd3t::getPreparedMemoryBytes() const noexcept
{
    uint64_t bytes=sizeof(*this);
    for(int c=0;c<2;++c)
        if(!addPreparedBytes(bytes,line[c].ownedMemoryBytes(),bytes)
            || !addPreparedBytes(bytes,uint64_t(history[c].capacity())*sizeof(double),bytes)
            || !addPreparedBytes(bytes,uint64_t(loop[c].capacity())*sizeof(double),bytes))return kUnknownPreparedMemoryBytes;
    return bytes;
}
void BossDd3t::prepare(double sampleRate,int,int)
{
    sr=sampleRate;timeCoefficient=1-std::exp(-1/(sr*.08));energyCoefficient=1-std::exp(-1/(sr*.05));prepareParams(sr);
    for(int c=0;c<2;++c)
    {
        line[c].prepare(int(sr*.81)+16);history[c].assign(size_t(sr*.81)+16,0);loop[c].assign(size_t(sr*.8)+8,0);
        highpass[c].setLowpass(sr,40);saturation[c].prepareFactor(sr,sr<=48000?2:1);
    }
    reset();
}
void BossDd3t::reset() noexcept
{
    for(int c=0;c<2;++c)
    {
        line[c].reset();highpass[c].reset();for(auto& f:pre[c])f.reset();for(auto& f:post[c])f.reset();saturation[c].reset();noise[c].reset(seed);
        std::fill(history[c].begin(),history[c].end(),0);feedback[c]=heldQuant[c]=previousQuant[c]=0;
    }
    historyHead=loopLength=0;loopPosition=0;holding=false;holdMix=0;advancingSync=false;
    engageRequest=downWasTap=false;samples=0;lastTap=downAt=-1;tapCount=tapIndex=0;energy=0;converterPhase=0;filterCountdown=0;lastCutoff=0;
    inputGain=wetGain=enabled?1:0;currentMs=cachedTargetMs=targetTime();lastLevel=lastFeedback=-1;lastBits=-1;
}
void BossDd3t::setParam(int idx,float norm) noexcept {PedalBase::setParam(idx,idx==17?0:norm);}
void BossDd3t::setExpression(int jack,float v) noexcept {if(jack==0)setParam(1,v);else if(jack==1)setParam(2,v);}
void BossDd3t::setTime(const TimeInfo& time) noexcept
{
    advancingSync=false;
    if(!time.valid || !std::isfinite(time.bpm) || time.bpm<=0 || !std::isfinite(time.ppq))return;
    const double period=60*sr/time.bpm;
    if(!std::isfinite(period) || period<=0)return;
    hostBpm=time.bpm;const double beat=std::floor(time.ppq);
    if(!hasSync || beat!=lastBeat)
    {
        syncMs=syncDelayMs();hasSync=true;lastBeat=beat;
    }
    cachedTargetMs=targetTime();
    const double nextBeat=beat+1,remaining=(nextBeat-time.ppq)*period;
    if(!time.playing || period<1 || nextBeat<=time.ppq || !std::isfinite(remaining))return;
    syncPeriodSamples=period;nextSyncBeat=nextBeat;
    // Snap only within floating-point error of an integer sample; cap this at
    // 1e-7 of a frame. This is not an audio/timing acceptance threshold.
    syncRoundoff=std::min(1.e-7,16*std::numeric_limits<double>::epsilon()*period*std::max(1.,std::abs(time.ppq)));
    const double nearest=std::round(remaining);
    syncSamplesRemaining=std::abs(remaining-nearest)<=syncRoundoff?nearest:remaining;
    advancingSync=true;
}
double BossDd3t::syncDelayMs() const noexcept
{
    constexpr double quarters[]{1,.5,.75,2.0/3,.25};
    double requested=60000/hostBpm*quarters[int(value(13))];
    while(requested>800)requested*=.5;
    return std::max(12.5,requested);
}
double BossDd3t::targetTime() const noexcept
{
    const int mode=int(value(3));
    if(mode!=3 && value(12)>.5f && hasSync)return syncMs;
    if(mode!=3 && value(7)>.5f && hasTapped)
    {
        // Normative IMPLEMENTATION_PLAN: QT/D8/Q use the overall delay range,
        // not the conflicting per-knob-range clamp in DD-3T section 9.
        const double factor=mode==0?2.0/3:mode==1?.75:1;
        return std::clamp(tapInterval*factor,12.5,800.);
    }
    const double minimum=mode==0?12.5:mode==1?50:200,maximum=minimum*4,t=value(2);
    return value(9)>.5f?minimum*maximum/(maximum-(maximum-minimum)*t):minimum+(maximum-minimum)*t;
}
void BossDd3t::tap() noexcept
{
    if(int(value(3))==3)return;
    if(lastTap>=0)
    {
        const double interval=1000*(samples-lastTap)/sr;
        if(interval>=133 && interval<=800)
        {
            tapIntervals[tapIndex]=interval;tapIndex=(tapIndex+1)%3;tapCount=std::min(3,tapCount+1);
            double sum=0;for(int k=0;k<tapCount;++k)sum+=tapIntervals[k];tapInterval=sum/tapCount;hasTapped=true;publishTap();
        }
        else if(interval>2000)tapCount=tapIndex=0;
    }
    lastTap=samples;cachedTargetMs=targetTime();
}
void BossDd3t::onTrigger(int index) noexcept {if(index==6)tap();}
void BossDd3t::beginHold() noexcept
{
    if(history[0].empty() || holding || int(value(3))!=3)return;
    loopLength=std::min(loop[0].size(),size_t(std::clamp(currentMs,200.,800.)*sr*.001));
    referenceMs=currentMs;loopPosition=0;
    for(int c=0;c<2;++c)for(size_t i=0;i<loopLength;++i)loop[c][i]=history[c][(historyHead+history[c].size()-loopLength+i)%history[c].size()];
    holding=true;
}
void BossDd3t::onParamChanged(int index,float v) noexcept
{
    if(index==5){if(v>.5f)beginHold();else holding=false;}
    if(index==3){downAt=-1;holdActionFired=false;if(int(v)!=3)holding=false;}
    cachedTargetMs=targetTime();
}
void BossDd3t::onFoot(const FootEvent& event) noexcept
{
    force=std::clamp(double(event.force),0.,1.);fadeSeconds=.040-.037*std::clamp(double(event.speed),0.,1.);
    if(int(value(3))==3){if(event.press)beginHold();else holding=false;return;}
    if(event.press){downAt=samples;holdActionFired=false;downWasTap=value(7)>.5f;if(downWasTap)tap();}
    else
    {
        if(downAt>=0 && !holdActionFired && !downWasTap)engageRequest=!engageRequest;
        downAt=-1;
    }
}
PedalUiMeters BossDd3t::readUiMeters() const noexcept
{
    PedalUiMeters meters;meters.available=UiTempo;
    meters.bpm=value(12)>.5f && hasSync?hostBpm:60000/tapInterval;
    if(holding && loopLength)
    {
        meters.available|=UiLoop|UiProgress;meters.loopLength=loopLength;
        meters.playhead=uint64_t(loopPosition);meters.progress=float(loopPosition/loopLength);
    }
    return meters;
}
float BossDd3t::getTailEnergyDb() const noexcept
{
    // A held buffer can emit again after a silent gap even when the underlying
    // delay has drained. Conservatively keep it scheduled until release/cut;
    // metering only the current sample cannot certify a sparse loop as silent.
    if(loopLength && (holding || holdMix>0) && wetGain>0)return 0;
    return float(10*std::log10(std::max(1e-20,energy)));
}
void BossDd3t::saveExtra(juce::var& v) const
{
    const double snapshot=publishedTap.load(std::memory_order_acquire),interval=std::abs(snapshot);
    if(!v.isObject())v=new juce::DynamicObject();v.getDynamicObject()->setProperty("tapIntervalMs",interval);v.getDynamicObject()->setProperty("tempoBpm",60000/interval);v.getDynamicObject()->setProperty("hasTapped",std::signbit(snapshot));
}
void BossDd3t::loadExtra(const juce::var& v)
{
    const double interval=double(v.getProperty("tapIntervalMs",500.));if(std::isfinite(interval))tapInterval=std::clamp(interval,133.,800.);hasTapped=bool(v.getProperty("hasTapped",false));publishTap();cachedTargetMs=targetTime();
}
void BossDd3t::process(double* left,double* right,int n) noexcept
{ render(left,right,nullptr,nullptr,n); }
bool BossDd3t::processWithWetResidual(double* left,double* right,double* residualL,double* residualR,int n) noexcept
{ render(left,right,residualL,residualR,n);return true; }
void BossDd3t::render(double* left,double* right,double* residualL,double* residualR,int n) noexcept
{
    for(int i=0;i<n;++i,++samples)
    {
        if(advancingSync && syncSamplesRemaining<=syncRoundoff)
        {
            // Discrete parameter events may follow setTime in this callback.
            syncMs=syncDelayMs();lastBeat=nextSyncBeat;nextSyncBeat+=1;
            if(nextSyncBeat<=lastBeat)advancingSync=false;
            syncSamplesRemaining+=syncPeriodSamples;cachedTargetMs=targetTime();
        }
        if(downAt>=0 && !holdActionFired && samples-downAt>=int64_t(2*sr)){setParam(7,value(7)>.5f?0:1);holdActionFired=true;}
        currentMs+=(cachedTargetMs-currentMs)*timeCoefficient;
        const double levelKnob=nextSmoothed(0),feedbackKnob=std::clamp(double(nextSmoothed(1))+(value(14)>.5f?force*.4:0),0.,1.);
        if(levelKnob!=lastLevel){lastLevel=levelKnob;cachedLevel=std::pow(levelKnob,1.6);}
        if(feedbackKnob!=lastFeedback){lastFeedback=feedbackKnob;cachedFeedback=.97*std::pow(feedbackKnob,1.2);}
        const double level=cachedLevel,fb=cachedFeedback;
        const double dry[2]{softGuard(left[i]),softGuard(right[i])},mono=(dry[0]+dry[1])*.5;
        inputGain+=std::clamp((enabled?1.:0.)-inputGain,-1/(sr*fadeSeconds),1/(sr*fadeSeconds));
        const double wetTarget=enabled || value(8)>.5f ? 1:0;wetGain+=std::clamp(wetTarget-wetGain,-1/(sr*fadeSeconds),1/(sr*fadeSeconds));
        const int mode=int(value(3));const double cutoff=mode==0?7000:mode==1?5500:7000-3500*std::clamp((currentMs-200)/600,0.,1.);
        if(--filterCountdown<=0)
        {
            filterCountdown=32;
            if(lastCutoff==0 || std::abs(cutoff-lastCutoff)>.01*lastCutoff)
            {
                for(int c=0;c<2;++c)for(auto* filters:{pre[c],post[c]}){filters[0].setLowpass(sr,cutoff,.541196100146197);filters[1].setLowpass(sr,cutoff,1.30656296487638);}lastCutoff=cutoff;
            }
        }
        bool converterTick=false;
        if(value(11)>.5f){converterPhase+=(32768/(currentMs*.001))/sr;if(converterPhase>=1){converterPhase-=std::floor(converterPhase);converterTick=true;}}
        holdMix+=std::clamp((holding?1.:0.)-holdMix,-1/(sr*.01),1/(sr*.01));
        const int bits=12+int(value(10));if(bits!=lastBits){lastBits=bits;quantScale=std::ldexp(1.,bits-1);inverseQuantScale=1/quantScale;}
        double wetEnergy=0;double* outputs[]{left,right};
        for(int c=0;c<2;++c)
        {
            const double input=value(18)>.5f?mono:dry[c];
            double x=highpass[c].highpass(input)*inputGain+feedback[c]*fb*wetGain;
            x=saturation[c].process(x,[](double v){return common::mildTanh(v);});for(auto& filter:pre[c])x=filter.process(x);
            if(bits<24 && std::abs(x)>1e-12)
                x=std::round(x*quantScale+.5*(noise[c].next()+noise[c].next()))*inverseQuantScale;
            if(value(11)>.5f)
            {
                if(converterTick){previousQuant[c]=heldQuant[c];heldQuant[c]=x;}
                x=previousQuant[c]+(heldQuant[c]-previousQuant[c])*converterPhase;
            }
            line[c].push(x);
            const double ratio=c==1 && value(16)>.5f?.75:1;
            const double delaySamples=std::max(1.,currentMs*ratio*sr*.001-saturation[c].latencySamples());
            double wet=line[c].read(delaySamples);for(auto& filter:post[c])wet=filter.process(wet);feedback[c]=wet;
            if(!history[c].empty())history[c][historyHead]=wet;
            double held=wet;
            if(loopLength && (holding || holdMix>0))
            {
                const size_t at=size_t(loopPosition),next=at+1==loopLength?0:at+1;const double frac=loopPosition-std::floor(loopPosition);
                held=loop[c][at]+(loop[c][next]-loop[c][at])*frac;
            }
            const double outWet=holdMix>=1?held:wet+(held-wet)*holdMix;
            const double dryOut=value(18)>.5f && value(15)>.5f?mono:dry[c];
            double* residual=c==0?residualL:residualR;
            if(residual)residual[i]=level*outWet*wetGain;
            outputs[c][i]=softGuard(dry[c]+inputGain*(dryOut-dry[c])+level*outWet*wetGain);
            wetEnergy+=wet*wet;
        }
        if(!history[0].empty() && ++historyHead==history[0].size())historyHead=0;
        if(loopLength && (holding || holdMix>0)){loopPosition+=referenceMs/currentMs;while(loopPosition>=loopLength)loopPosition-=loopLength;}
        energy+=(.5*wetEnergy-energy)*energyCoefficient;
        if(advancingSync)syncSamplesRemaining-=1;
    }
}
}
