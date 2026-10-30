// OWNER: WP16
#include "boss_rc5.h"
#include <chrono>
#include <thread>

namespace luthier::reference::models
{
namespace
{
using Action=common::LooperAction;
using State=common::PagedLooperCore::State;
using Direction=common::PagedLooperCore::Direction;
const char* const variationNames[]{"A","B"};
const char* const syncNames[]{"AUTO","INTERNAL"};
const char* const expTargets[]{"LOOP LEVEL","RHYTHM LEVEL","TEMPO"};
// Spec sec 8 order. Host lanes (non-excluded kinds) follow the reconciled header:
// loop_level, rhythm_level, rhythm_on, tempo, pattern, variation, kit, memory,
// reverse, hold_ms, dtap_ms, dry_mute, half_speed.
const ParamDesc parameters[]{
    {"boss_rc5_loop_level","LOOP LEVEL","%",ParamKind::Continuous,Taper::Linear,0,200,100,1,0,nullptr,PF_Automatable|PF_ExprTarget,20,-1},
    {"boss_rc5_rhythm_level","RHYTHM LEVEL","%",ParamKind::Continuous,Taper::Linear,0,100,80,1,0,nullptr,PF_Automatable|PF_ExprTarget,20,-1},
    {"boss_rc5_rhythm_on","RHYTHM","",ParamKind::Toggle,Taper::Linear,0,1,0,1,0,nullptr,PF_Automatable,0,-1},
    {"boss_rc5_tempo","TEMPO","BPM",ParamKind::Continuous,Taper::Linear,40,300,120,1,0,nullptr,PF_Automatable|PF_ExprTarget,50,-1},
    {"boss_rc5_pattern","PATTERN","",ParamKind::Discrete,Taper::Linear,1,57,1,1,57,nullptr,PF_Automatable,0,-1},
    {"boss_rc5_variation","VARIATION","",ParamKind::Enum,Taper::Linear,0,1,0,1,2,variationNames,PF_Automatable,0,-1},
    {"boss_rc5_kit","KIT","",ParamKind::Discrete,Taper::Linear,1,7,1,1,7,nullptr,PF_Automatable,0,-1},
    {"boss_rc5_memory","MEMORY","",ParamKind::Discrete,Taper::Linear,1,99,1,1,99,nullptr,PF_Automatable,0,-1},
    {"boss_rc5_reverse","REVERSE","",ParamKind::Toggle,Taper::Linear,0,1,0,1,0,nullptr,PF_Automatable,5,-1},
    {"boss_rc5_pedal1","PEDAL","",ParamKind::Momentary,Taper::Linear,0,1,0,1,0,nullptr,PF_Automatable,0,-1},
    {"boss_rc5_pedal2","CTL1","",ParamKind::Momentary,Taper::Linear,0,1,0,1,0,nullptr,PF_Automatable,0,-1},
    {"boss_rc5_pedal3","CTL2","",ParamKind::Momentary,Taper::Linear,0,1,0,1,0,nullptr,PF_Automatable,0,-1},
    {"boss_rc5_sync_clock","SYNC CLOCK","",ParamKind::Enum,Taper::Linear,0,1,0,1,2,syncNames,PF_Ext,0,-1},
    {"boss_rc5_quantize","LOOP QUANTIZE","",ParamKind::Toggle,Taper::Linear,0,1,0,1,0,nullptr,PF_Ext,0,-1},
    {"boss_rc5_exp_target","EXP TARGET","",ParamKind::Enum,Taper::Linear,0,2,0,1,3,expTargets,PF_Ext,0,-1},
    {"boss_rc5_exp","EXP","",ParamKind::Continuous,Taper::Linear,0,1,0,1,0,nullptr,PF_Ext,10,-1},
    {"boss_rc5_hold_ms","HOLD TIME","ms",ParamKind::Continuous,Taper::Linear,800,3000,2000,1,0,nullptr,PF_None,0,-1},
    {"boss_rc5_dtap_ms","DOUBLE-TAP WINDOW","ms",ParamKind::Continuous,Taper::Linear,150,600,400,1,0,nullptr,PF_None,0,-1},
    {"boss_rc5_lofi","CONVERTER MODEL","",ParamKind::Toggle,Taper::Linear,0,1,0,1,0,nullptr,PF_Config,0,-1},
    {"boss_rc5_dry_mute","DRY MUTE (EXT)","",ParamKind::Toggle,Taper::Linear,0,1,0,1,0,nullptr,PF_Automatable,20,-1},
    {"boss_rc5_half_speed","HALF SPEED (EXT)","",ParamKind::Toggle,Taper::Linear,0,1,0,1,0,nullptr,PF_Automatable,20,-1},
    {"boss_rc5_ram_cap_mb","RAM CAP","MB",ParamKind::Continuous,Taper::Linear,64,2048,256,1,0,nullptr,PF_Config,0,-1},
    {"boss_rc5_clock_out","MIDI CLOCK OUT (EXT)","",ParamKind::Toggle,Taper::Linear,0,1,0,1,0,nullptr,PF_Ext|PF_Hidden,0,-1},
    {"boss_rc5_tap","TEMPO TAP","",ParamKind::Trigger,Taper::Linear,0,1,0,1,0,nullptr,PF_None,0,-1},
};
const PedalDesc description{"boss-rc5","boss_rc5_",1,"Loop Station (prototype)","Looper",nullptr,
    ChannelMode::Stereo,BypassMode::Buffered,24,parameters,0,false,false,false,800,1000};
// Spec 4.2/1: one stereo track of 1.5 h; worst-case undo layer; >=2 s read-ahead.
common::PagedLooperCore::Config looperConfig(double rate,double ramMb) noexcept
{
    common::PagedLooperCore::Config c;
    c.sampleRate=rate;c.seconds=5400;c.channels=2;c.undoDepth=1;
    c.pageFrames=rate<=48000?8192:rate<=96000?16384:32768;c.writePages=4;c.readPages=14;c.readAhead=13;
    c.ramBudget=size_t(std::clamp(ramMb,64.,2048.)*1024*1024);c.storageBudget=32ull*1024*1024*1024;
    return c;
}
}
const PedalDesc& BossRc5::descriptor() noexcept {return description;}
BossRc5::BossRc5() : PedalBase(description) {}
PreparedMemoryRequirement BossRc5::prepareMemory(double rate,int block,int policy,const float*) noexcept
{
    if(!std::isfinite(rate) || rate<8000 || rate>192000 || block<1 || policy<0 || policy>2)return {};
    return {sizeof(BossRc5),0};
}
void BossRc5::prepare(double sampleRate,int,int)
{
    // Re-preparing replaces the core. Keep any completed loop so it can be
    // restored (same rate) or at least re-saved unchanged; never drop it.
    if(looperReady && !storedPending && core.state()!=State::Empty)
    {
        common::PagedLooperCore::Graph graph;bool ready=false;
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        do
        {
            core.process(nullptr,nullptr,0);
            ready=core.capture(graph) && graph.status==common::PagedLooperCore::CaptureStatus::Ready;
            if(!ready)std::this_thread::yield();
        }while(!ready && std::chrono::steady_clock::now()<until);
        juce::var encoded;
        if(ready && common::LooperStateCodec::encode(common::LooperStateCodec::Stored{},graph,config,encoded))
        {stored.source=encoded;stored.graph=graph;stored.config=config;stored.forward=false;storedPending=true;}
    }
    sr=common::sampleRate(sampleRate);prepareParams(sr);
    rhythm.prepare(sr,services.instanceSeed?services.instanceSeed:0x51a7u);
    tapTempo.prepare(sr);
    for(int c=0;c<2;++c){lofiHp[c].setLowpass(sr,20);lofiLp[c].setLowpass(sr,std::min(20000.,sr*.45));}
    config=looperConfig(sr,value(RamCapMb));
    looperReady=services.assets!=nullptr && core.prepare(*services.assets,config);
    if(looperReady)core.setSeamMilliseconds(2); // spec 3: 2 ms boundary fades (ESTIMATED)
    prepared=true;
    rhythmPattern=rhythmVariation=rhythmKit=-1;
    reset();
    restoreStored();
}
void BossRc5::reset() noexcept
{
    // A loop is user data: reset stops transport and gestures but keeps audio.
    if(looperReady)core.apply(Action::Stop);
    cycle.reset();rhythm.reset();rhythmPattern=rhythmVariation=rhythmKit=-1;
    tapTempo.reset();tappedTempoActive=false;
    for(auto& f:lofiHp)f.reset();for(auto& f:lofiLp)f.reset();
    mainHeld=holdArmed=provisionalCut=false;footDown={};dryGain=value(DryMute)>.5f?0:1;
}
bool BossRc5::restoreStored() noexcept
{
    if(!storedPending || !looperReady || stored.forward)return false;
    if(stored.graph.status==common::PagedLooperCore::CaptureStatus::Empty){storedPending=false;return false;}
    if(stored.graph.status!=common::PagedLooperCore::CaptureStatus::Ready)return false;
    if(!core.restore(stored.graph,common::PagedLooperCore::RestorePolicy::Stopped))return false;
    storedPending=false;
    return true;
}
void BossRc5::onParamChanged(int index,float v) noexcept
{
    if(index==Tempo){tappedTempoActive=false;return;}
    if(index<Pedal1 || index>Pedal3)return;
    const bool down=v>=.5f;auto& was=footDown[size_t(index-Pedal1)];
    if(down==was)return;
    was=down;
    if(!prepared)return;
    if(index==Pedal1){if(down)mainDown();else mainUp();return;}
    if(!down)return;
    cycle.invalidateCandidate();
    if(!looperReady)return;
    if(index==Pedal2)
    {
        // CTL1: TRK PLY/STP (plugin default, ESTIMATED).
        const auto s=core.state();
        if(s==State::Playing || s==State::Overdubbing || s==State::Recording)core.apply(Action::Stop);
        else if(s==State::Stopped)core.apply(Action::Play);
    }
    else undoOrRedo(); // CTL2: TRK UND/RED (plugin default, ESTIMATED).
}
void BossRc5::onTrigger(int index) noexcept
{
    if(index!=TapTempo || !prepared)return;
    if(const auto measured=tapTempo.tap(samples))
    {tappedTempo=*measured;tappedTempoActive=true;}
}
void BossRc5::onFoot(const FootEvent& event) noexcept
{
    if(event.press)mainDown();else mainUp();
}
void BossRc5::mainDown() noexcept
{
    if(mainHeld)return;
    mainHeld=true;
    if(!looperReady)return;
    const auto before=core.state();
    const auto batch=cycle.down(before,samples,sr,double(value(DtapMs)));
    for(int k=0;k<batch.count;++k)core.apply(batch.actions[size_t(k)]);
    // A double press stops; it never arms the destructive STOP-hold clear.
    const bool stopped=batch.count>0 && batch.actions[size_t(batch.count-1)]==Action::Stop;
    holdArmed=batch.count>0 && !stopped;holdStart=samples;holdOrigin=before;provisionalCut=false;
}
void BossRc5::mainUp() noexcept
{
    if(!mainHeld)return;
    // A long press that never reached hold_ms still means "overdub": resume
    // it if its provisional pass was cut at a wrap.
    if(holdArmed && provisionalCut && looperReady && core.state()==State::Playing)core.apply(Action::Overdub);
    mainHeld=holdArmed=provisionalCut=false;cycle.up();
}
void BossRc5::undoOrRedo() noexcept
{
    if(core.canUndo())core.apply(Action::Undo);
    else if(core.canRedo())core.apply(Action::Redo);
}
void BossRc5::fireHold() noexcept
{
    // Spec 2.2: hold >= hold_ms in PLAY/OVERDUB = undo, again = redo; in STOP = clear.
    // The down edge already acted, so its provisional work is intercepted here.
    holdArmed=provisionalCut=false;cycle.invalidateCandidate();
    switch(holdOrigin)
    {
        case State::Playing:
            if(core.state()==State::Overdubbing)core.apply(Action::CancelOverdub);
            undoOrRedo();break;
        case State::Overdubbing:undoOrRedo();break;
        case State::Stopped:core.apply(Action::Stop);core.apply(Action::Clear);break;
        case State::Empty:case State::Recording:break;
    }
}
void BossRc5::updateRhythm() noexcept
{
    const int pattern=int(value(Pattern)),variation=int(value(Variation)),kit=int(value(Kit));
    if(pattern!=rhythmPattern || variation!=rhythmVariation || kit!=rhythmKit)
    {rhythm.select(pattern,variation,kit);rhythmPattern=pattern;rhythmVariation=variation;rhythmKit=kit;}
    // SYNC CLOCK AUTO follows a running host clock, else the internal tempo.
    const bool follow=value(SyncClock)<.5f && time.valid && time.playing && std::isfinite(time.bpm) && time.bpm>0;
    tempo=std::clamp(follow?time.bpm:(tappedTempoActive?tappedTempo:double(value(Tempo))),40.,300.);
    rhythm.setTempo(tempo);rhythm.setLevel(value(RhythmLevel));rhythm.setRunning(value(RhythmOn)>.5f);
}
void BossRc5::process(double* left,double* right,int n) noexcept
{
    // Zero frames only services the core (publishes completed worker I/O).
    if(n==0){if(looperReady)core.process(nullptr,nullptr,0);return;}
    if(!left || !right || n<0)return;
    for(int i=0;i<n;++i){left[i]=softGuard(left[i]);right[i]=softGuard(right[i]);}
    if(looperReady)
    {
        // The core accepts a direction change only while Empty/Stopped; a
        // request made during playback applies at the next stop.
        const auto want=value(Reverse)>.5f?Direction::Reverse:Direction::Forward;
        if(core.playbackDirection()!=want)core.setPlaybackDirection(want);
    }
    const double dryTarget=value(DryMute)>.5f?0:1,dryStep=1/(sr*.02);
    for(int done=0;done<n;)
    {
        int chunk=std::min(n-done,32);
        if(holdArmed)
        {
            const int64_t remaining=holdStart+holdSamples()-samples;
            if(remaining<=0){fireHold();continue;}
            chunk=int(std::min<int64_t>(chunk,remaining));
        }
        // A paged layer is written once per pass, so continuous overdub closes
        // each pass at the wrap and opens the next at that exact sample. Undo
        // therefore removes the last pass (spec 2.2, ESTIMATED single level).
        // While a hold from PLAY is pending, its provisional pass is cancelled at
        // the wrap instead, so it can never consume the single undo level.
        const bool wraps=looperReady && core.state()==State::Overdubbing && core.length()>0
            && core.playbackDirection()==Direction::Forward;
        const bool provisional=holdArmed && holdOrigin==State::Playing;
        if(wraps)chunk=int(std::min<uint64_t>(uint64_t(chunk),core.length()-core.playhead()));
        // Spec 4.3: LOOP LEVEL 0..200 maps linearly to gain 0..2 (100 = unity).
        const double loopGain=smoothedNow(LoopLevel)*.01;skipSmoothed(LoopLevel,chunk);
        dryGain+=std::clamp(dryTarget-dryGain,-dryStep*chunk,dryStep*chunk);
        if(looperReady)core.process(left+done,right+done,chunk,loopGain,dryGain);
        else for(int i=done;i<done+chunk;++i){left[i]*=dryGain;right[i]*=dryGain;}
        samples+=chunk;done+=chunk;
        if(wraps && core.playhead()==0 && core.state()==State::Overdubbing)
        {
            if(provisional){core.apply(Action::CancelOverdub);provisionalCut=true;}
            else{core.apply(Action::Play);core.apply(Action::Overdub);}
        }
    }
    // A hold reaching hold_ms exactly at this block's end fires at that boundary.
    if(holdArmed && samples-holdStart>=holdSamples())fireHold();
    updateRhythm();rhythm.process(left,right,n);
    const bool lofi=value(Lofi)>.5f;
    for(int i=0;i<n;++i)
    {
        if(lofi){left[i]=lofiLp[0].process(lofiHp[0].highpass(left[i]));right[i]=lofiLp[1].process(lofiHp[1].highpass(right[i]));}
        left[i]=softGuard(left[i]);right[i]=softGuard(right[i]);
    }
}
void BossRc5::setExpression(int jack,float v) noexcept
{
    if(jack!=0 || !std::isfinite(v))return;
    v=std::clamp(v,0.f,1.f);setParam(Exp,v);
    const int target=int(value(ExpTarget));
    setParam(target==1?RhythmLevel:target==2?Tempo:LoopLevel,v);
}
PedalUiMeters BossRc5::readUiMeters() const noexcept
{
    PedalUiMeters m;
    m.available=UiTransport|UiLoop|UiTempo|UiProgress|UiFaults;
    m.transport=int(transportState());m.bpm=tempo;
    if(looperReady)
    {
        m.loopLength=core.length();m.playhead=core.playhead();m.faults=core.faults();
        m.progress=m.loopLength?float(double(m.playhead)/double(m.loopLength)):0.f;
    }
    else m.faults=noAssetStore;
    return m;
}
void BossRc5::saveExtra(juce::var& extra) const
{
    juce::var looper;
    common::PagedLooperCore::Graph graph;
    const bool captured=looperReady && core.capture(graph);
    const auto status=graph.status;
    // A live Ready/Empty capture is authoritative, except an untouched core that
    // still holds a pending (not restorable here) saved loop.
    const bool live=captured && (status==common::PagedLooperCore::CaptureStatus::Ready
        || (status==common::PagedLooperCore::CaptureStatus::Empty && !storedPending));
    // An opaque future-schema prior would make encode re-emit it; a live loop
    // recorded here replaces it, so encode against a fresh prior in that case.
    if(live){if(!common::LooperStateCodec::encode(stored.forward?common::LooperStateCodec::Stored{}:stored,graph,config,looper))return;}
    else if(stored.source.getDynamicObject())looper=stored.source.clone(); // incomplete capture: keep last saved loop
    else return;
    if(!extra.getDynamicObject())extra=juce::var(new juce::DynamicObject());
    extra.getDynamicObject()->setProperty("looper",looper);
}
void BossRc5::loadExtra(const juce::var& extra)
{
    if(!extra.hasProperty("looper"))return;
    common::LooperStateCodec::Stored next;
    if(!common::LooperStateCodec::decode(extra["looper"],next))return; // keep previous on failure
    stored=next;
    storedPending=stored.forward || stored.graph.status==common::PagedLooperCore::CaptureStatus::Ready;
    if(looperReady && storedPending && core.state()!=State::Empty)
    {
        // Loading a state replaces the current loop only when it can be restored.
        core.apply(Action::Stop);
    }
    restoreStored();
}
}
