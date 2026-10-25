// OWNER: WP15
#include "tc_ditto.h"
#include "../Common/PagedLooperCore.h"
#include "../Common/LooperFootGesture.h"
#include "../Common/LooperStateCodec.h"
#include <array>
#include <atomic>
#include <cmath>
#include <stdexcept>
namespace luthier::reference::models
{
namespace
{
using Core=common::PagedLooperCore;
using Gesture=common::LooperFootGesture;
using Codec=common::LooperStateCodec;
const char* const quantChoices[]{"Off","Beat","Bar"};
const ParamDesc parameters[]{
    {"tc_ditto_loop_level","LOOP LEVEL","",ParamKind::Continuous,Taper::Linear,0,1,.909f,1,0,nullptr,PF_Automatable|PF_OwnSmoothing,10,-1},
    {"tc_ditto_dry_level","Dry Level","",ParamKind::Continuous,Taper::Linear,0,1,1,1,0,nullptr,PF_Automatable|PF_OwnSmoothing,10,-1},
    {"tc_ditto_fs","FOOTSWITCH","",ParamKind::Momentary,Taper::Linear,0,1,0,1,0,nullptr,PF_None,0,-1},
    {"tc_ditto_undo_hold_ms","Undo Hold","ms",ParamKind::Continuous,Taper::Linear,800,2500,1500,1,0,nullptr,PF_Automatable,0,-1},
    {"tc_ditto_undo_depth","Undo Depth","",ParamKind::Discrete,Taper::Linear,1,8,1,1,8,nullptr,PF_Ext|PF_Config,0,-1},
    {"tc_ditto_rec_to_overdub","Record to Overdub","",ParamKind::Toggle,Taper::Linear,0,1,0,1,0,nullptr,PF_Automatable,0,-1},
    {"tc_ditto_stop_at_end","Stop at End","",ParamKind::Toggle,Taper::Linear,0,1,0,1,0,nullptr,PF_Ext,0,-1},
    {"tc_ditto_seam_xfade_ms","Seam Fade","ms",ParamKind::Continuous,Taper::Linear,0,10,0,1,0,nullptr,PF_Automatable,0,-1},
    {"tc_ditto_quantize","Quantize","",ParamKind::Enum,Taper::Linear,0,2,0,1,3,quantChoices,PF_Ext,0,-1},
    {"tc_ditto_follow_transport","Follow Transport","",ParamKind::Toggle,Taper::Linear,0,1,0,1,0,nullptr,PF_Ext,0,-1},
    {"tc_ditto_conv_latency_samples","Converter Latency","samples",ParamKind::Continuous,Taper::Linear,0,256,0,1,0,nullptr,PF_Config,0,-1},
    {"tc_ditto_send_return_mode","Feedback Tap","",ParamKind::Toggle,Taper::Linear,0,1,0,1,0,nullptr,PF_Ext,0,-1}
};
const PedalDesc description{"tc-ditto","tc_ditto_",1,"Ditto Looper","Looper","TC Electronic Ditto Looper",ChannelMode::Mono,BypassMode::True,12,parameters,0,false,false,false,300,1000};
class TcDitto final : public IPedal
{
public:
    TcDitto(){for(size_t i=0;i<values.size();++i)values[i]=parameters[i].defaultNormalised();}
    const PedalDesc& desc() const noexcept override{return description;}
    static PreparedMemoryRequirement prepareMemory(double sampleRate,int maxBlock,int,const float*) noexcept
    {
        if(!std::isfinite(sampleRate) || sampleRate<1 || sampleRate>192000 || maxBlock<1)return {};
        return {sizeof(TcDitto)+sizeof(Core),0};
    }
    uint64_t getPreparedMemoryBytes() const noexcept override
    {
        // E69: pimpl/streams/workers/caches are explicitly covered by the
        // separate looper-media reservation. JSON heap is project state.
        return sizeof(TcDitto)+(core?sizeof(Core):0);
    }
    void setServices(const PedalServices& value) override{services=value;}
    void prepare(double sampleRate,int,int) override
    {
        if(!services.assets)throw std::runtime_error("Ditto needs the board asset store");
        rate=sampleRate;preparedConfig={};preparedConfig.sampleRate=rate;preparedConfig.seconds=300;preparedConfig.channels=1;
        preparedConfig.undoDepth=int(std::lround(parameters[4].fromNormalised(values[4])));
        // Include the fixed wrapper/converter ring plus a JSON state-template
        // allowance in the per-instance 32MiB reservation.
        constexpr size_t wrapperAllowance=sizeof(TcDitto)+256*1024;
        preparedConfig.ramBudget=32*1024*1024-wrapperAllowance;preparedConfig.storageBudget=4ull*1024*1024*1024;
        core=std::make_unique<Core>();if(!core->prepare(*services.assets,preparedConfig))throw std::runtime_error("Ditto full-duration RAM/storage budget failed");
        prepared=true;clock=timeClock=0;time={};gesture.configure(rate,timing());gesture.reset();pendingStart=pendingEnd=pendingStop=false;undone=false;++delayEpoch;
        delaySamples=std::clamp(int(std::lround(parameters[10].fromNormalised(values[10]))),0,256);delayHead=0;
        loopGain=targetLoopGain=levelGain(values[0]);dryGain=values[1];smooth=1.-std::exp(-1./std::max(1.,rate*.010));
        core->setSeamMilliseconds(parameters[7].fromNormalised(values[7]));restoreSaved();
    }
    void reset() noexcept override
    {
        if(core)core->apply(Core::Action::Stop);
        gesture.reset();pendingStart=pendingEnd=pendingStop=false;++delayEpoch;
    }
    void setEnabled(bool value) noexcept override{enabled=value;if(!value)gesture.reset();}
    bool footControlsEngagement() const noexcept override{return false;}
    bool outputsDualMono() const noexcept override{return false;} // dry remains true stereo
    void setParam(int index,float norm) noexcept override
    {
        if(index<0 || index>=int(values.size()))return;
        norm=std::isfinite(norm)?std::clamp(norm,0.f,1.f):0.f;
        const auto before=values[size_t(index)];values[size_t(index)]=norm;
        if(index==0)targetLoopGain=levelGain(norm);
        if(index==2 && (before>=.5f)!=(norm>=.5f))foot(norm>=.5f);
        if(prepared && index==3)gesture.configure(rate,timing());
        if(core && index==11 && norm>=.5f)
        {
            pendingStart=pendingEnd=false;
            if(core->state()==Core::State::Overdubbing)core->apply(Core::Action::CancelOverdub);
            else if(core->state()==Core::State::Recording)core->apply(Core::Action::Stop);
        }
        if(core && index==7)core->setSeamMilliseconds(parameters[7].fromNormalised(norm));
    }
    float getParam(int index) const noexcept override{return index>=0 && index<int(values.size())?values[size_t(index)]:0;}
    void onFoot(const FootEvent& event) noexcept override{values[2]=event.press?1.f:0.f;foot(event.press);}
    void setTime(const TimeInfo& value) noexcept override{time=value;timeClock=clock;}
    void onTransport(TransportEvent event,int) noexcept override
    {
        if(!core || !prepared || values[9]<.5f || stored.forward)return;
        pendingStart=pendingEnd=pendingStop=false;
        if(event==TransportEvent::Start || event==TransportEvent::Continue){core->apply(Core::Action::Play);core->rewind();}
        else if(event==TransportEvent::Stop || event==TransportEvent::Locate)core->apply(Core::Action::Stop);
    }
    void process(double* left,double* right,int n) noexcept override
    {
        if(!prepared || !core || !enabled || stored.forward || n<0 || (n && (!left || !right)))return;
        core->beginBlock();double wetPeak=0;
        for(int i=0;i<n;++i,++clock)
        {
            if(pendingStart && clock>=startAt){pendingStart=false;core->apply(Core::Action::Record);recordAt=clock;undone=false;++delayEpoch;}
            if(pendingEnd && clock>=endAt){pendingEnd=false;core->apply(values[5]>=.5f?Core::Action::Overdub:Core::Action::Play);undone=false;}
            if(pendingStop && core->playhead()==0){pendingStop=false;core->apply(Core::Action::Stop);}
            Gesture::Event event;if(gesture.advance(clock,event))handle(event);
            const double inputL=left[i],inputR=right[i];double wetL=inputL,wetR=inputR;
            core->processFrame(wetL,wetR,1,0);
            loopGain+=(targetLoopGain-loopGain)*smooth;dryGain+=(double(values[1])-dryGain)*smooth;
            auto& write=delay[size_t(delayHead)];write={wetL,wetR,delayEpoch};
            const int read=(delayHead+257-delaySamples)%257;const auto& output=delay[size_t(read)];
            if(output.epoch!=delayEpoch)wetL=wetR=0;else{wetL=output.left;wetR=output.right;}
            delayHead=(delayHead+1)%257;const auto outputL=wetL*loopGain,outputR=wetR*loopGain;wetPeak=std::max(wetPeak,std::max(std::abs(outputL),std::abs(outputR)));left[i]=inputL*dryGain+outputL;right[i]=inputR*dryGain+outputR;
        }
        core->endBlock();if(n)tailDb=float(20.*std::log10(std::max(wetPeak,1e-6)));
    }
    int getLatencySamples() const noexcept override{return 0;} // analog dry is never delayed
    float getTailEnergyDb() const noexcept override
    {return prepared && enabled && !stored.forward?tailDb:-120.f;}
    PedalUiMeters readUiMeters() const noexcept override
    {
        PedalUiMeters result;result.available=UiTransport|UiLoop|UiFaults;
        if(core){result.transport=int(core->state());result.loopLength=core->length();result.playhead=core->playhead();result.faults=core->faults();}
        result.faults|=stored.forward?uint32_t(common::LooperStorage::invalidAsset):0;
        // This extension needs an actual board return-tap service. It never
        // substitutes an invented internal signal while that service is absent.
        if(values[11]>=.5f)result.faults|=1u<<20;
        if(pendingStart && startAt>armedAt){result.available|=UiProgress;result.progress=float(std::clamp(double(clock-armedAt)/double(startAt-armedAt),0.,1.));}
        return result;
    }
    void saveExtra(juce::var& out) const override
    {
        Core::Graph graph;if(!core || !core->capture(graph)){graph.status=Core::CaptureStatus::Pending;graph.faults=common::LooperStorage::writeOverrun;}
        juce::var state;juce::String error;
        if(!Codec::encode(stored,graph,preparedConfig,state,&error))throw std::runtime_error(error.toStdString());
        juce::var owned(new juce::DynamicObject());owned.getDynamicObject()->setProperty("looper",state);out=std::move(owned);
    }
    void loadExtra(const juce::var& extra) override
    {
        Codec::Stored next;juce::String error;if(!Codec::decode(extra["looper"],next,&error))throw std::runtime_error(error.toStdString());
        stored=std::move(next);if(prepared)restoreSaved();
    }
private:
    struct Delay {double left=0,right=0;uint64_t epoch=0;};
    std::array<float,12> values{};float tailDb=-120;
    PedalServices services;Core::Config preparedConfig;Codec::Stored stored;std::unique_ptr<Core> core;
    std::array<Delay,257> delay{};uint64_t delayEpoch=1;int delayHead=0,delaySamples=0;
    Gesture gesture;TimeInfo time;double rate=48000,loopGain=1,targetLoopGain=1,dryGain=1,smooth=1;
    int64_t clock=0,timeClock=0,armedAt=0,startAt=0,endAt=0,recordAt=0,quantPeriod=0;
    bool prepared=false,enabled=true,undone=false,pendingStart=false,pendingEnd=false,pendingStop=false;
    static double levelGain(double value) noexcept{return value<.01?0:std::pow(10.,(-60.+66.*value)/20.);}
    Gesture::Timing timing() const noexcept{Gesture::Timing value;value.undoHoldMs=std::round(parameters[3].fromNormalised(values[3]));return value;}
    void restoreSaved()
    {
        if(!prepared || stored.forward)return;
        if(stored.graph.status==Core::CaptureStatus::Empty)return;
        if(stored.graph.status!=Core::CaptureStatus::Ready)throw std::runtime_error("Ditto state contains unfinished recording or asset I/O");
        if(stored.config.sampleRate!=rate || stored.config.channels!=1 || stored.config.pageFrames!=preparedConfig.pageFrames)
            throw std::runtime_error("Ditto asset format needs offline rate conversion");
        const auto needed=std::min(8,stored.graph.layerCount+stored.graph.redoCount+stored.graph.retiredCount);
        if(needed>std::max(preparedConfig.undoDepth,preparedConfig.historyCapacity))
        {
            preparedConfig.historyCapacity=needed;
            core.reset();core=std::make_unique<Core>();
            if(!core->prepare(*services.assets,preparedConfig))throw std::runtime_error("Ditto restored history exceeds the RAM/storage budget");
            core->setSeamMilliseconds(parameters[7].fromNormalised(values[7]));
        }
        if(!core->restore(stored.graph))throw std::runtime_error("Ditto audio assets are missing, invalid or unavailable");
        undone=stored.graph.redoCount>0;
    }
    void foot(bool press) noexcept
    {
        if(!prepared || !core || stored.forward)return;
        Gesture::Event event;if(!enabled && press)return;
        if(gesture.edge(press,clock,core->state(),event))handle(event);
    }
    bool quantized() const noexcept{return values[8]>=.25f && time.valid && std::isfinite(time.bpm) && time.bpm>=20 && time.bpm<=300 && std::isfinite(time.ppq);}
    void startRecording() noexcept
    {
        if(values[11]>=.5f)return; // unavailable return source must not record the ordinary input instead
        undone=false;++delayEpoch;
        if(!quantized()){pendingStart=false;quantPeriod=0;recordAt=clock;core->apply(Core::Action::Record);return;}
        const auto mode=int(std::lround(parameters[8].fromNormalised(values[8])));const double grid=mode==2?4.:1.;
        const double beat=rate*60./time.bpm,ppq=time.ppq+double(clock-timeClock)/beat;
        double phase=std::fmod(ppq,grid);if(phase<0)phase+=grid;
        const auto wait=phase<1e-10?int64_t(0):int64_t(std::ceil((grid-phase)*beat));
        quantPeriod=std::max(int64_t(1),int64_t(std::llround(grid*beat)));armedAt=clock;startAt=clock+wait;pendingStart=true;
    }
    void endRecording() noexcept
    {
        if(quantized() && quantPeriod>0){const auto elapsed=std::max(int64_t(1),clock-recordAt);endAt=recordAt+std::max(int64_t(1),(elapsed+quantPeriod-1)/quantPeriod)*quantPeriod;pendingEnd=true;return;}
        core->apply(values[5]>=.5f?Core::Action::Overdub:Core::Action::Play);undone=false;
    }
    void handle(const Gesture::Event& event) noexcept
    {
        switch(event.kind)
        {
            case Gesture::Kind::None:break;
            case Gesture::Kind::Down:
                if(pendingStart){pendingStart=false;break;}
                switch(event.began)
                {
                    case Core::State::Empty:startRecording();break;
                    case Core::State::Recording:endRecording();break;
                    case Core::State::Playing:if(values[11]<.5f)core->apply(Core::Action::Overdub);break;
                    case Core::State::Overdubbing:core->apply(Core::Action::Play);undone=false;break;
                    case Core::State::Stopped:core->apply(Core::Action::Play);break;
                }
                break;
            case Gesture::Kind::DoubleDown:
                pendingStart=pendingEnd=false;
                if(core->state()==Core::State::Overdubbing)core->apply(Core::Action::CancelOverdub);
                if(values[6]>=.5f && core->length() && core->state()==Core::State::Playing)pendingStop=true;
                else core->apply(Core::Action::Stop);
                break;
            case Gesture::Kind::UndoRedoHold:
            {
                if(core->state()==Core::State::Overdubbing)core->apply(Core::Action::CancelOverdub);
                const auto before=core->undoDepth();core->apply(undone?Core::Action::Redo:Core::Action::Undo);
                if(core->undoDepth()<before)undone=true;else if(core->undoDepth()>before)undone=false;
                break;
            }
            case Gesture::Kind::ClearHold:
                pendingStart=pendingEnd=pendingStop=false;core->apply(Core::Action::Clear);undone=false;++delayEpoch;break;
        }
    }
};
}
const PedalDesc& tcDittoDescription() noexcept{return description;}
std::unique_ptr<IPedal> makeTcDitto(){return std::make_unique<TcDitto>();}
PreparedMemoryRequirement tcDittoPreparedMemory(double sr,int block,int policy,const float* values) noexcept{return TcDitto::prepareMemory(sr,block,policy,values);}
}
