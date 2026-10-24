// OWNER: WP15
#include "LooperCore.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace luthier::reference::common
{
bool LooperCore::prepare(double sampleRate,double seconds,int layers,size_t budget)
{
    sr=std::isfinite(sampleRate)?std::clamp(sampleRate,44100.,192000.):48000;
    maxLayers=std::clamp(layers,1,8);
    const auto storageLayers=size_t(maxLayers+1);
    const size_t fixedBytes=(storageLayers+2)*sizeof(Epoch);
    const size_t bytesPerFrame=sizeof(Stereo)+storageLayers*(sizeof(LayerSample)+sizeof(Epoch));
    const double requested=sr*(std::isfinite(seconds)?std::clamp(seconds,0.,5400.):0.);
    maxFrames=std::min({size_t(requested),budget>fixedBytes ? (budget-fixedBytes)/bytesPerFrame : size_t(0),
                        (size_t(std::numeric_limits<uint32_t>::max())-(storageLayers+2))/storageLayers});
    if(!maxFrames)
    {
        std::vector<Stereo>().swap(base);
        for(auto& samples:layer)std::vector<LayerSample>().swap(samples);
        std::vector<Epoch>().swap(epochs);freeEpoch=0;generation.fill(0);sequence=clearedThrough=0;
        storageBytes=0;reset();return false;
    }
    base=std::vector<Stereo>(maxFrames);
    for(int i=0;i<9;++i) { layer[i]=std::vector<LayerSample>(i<=maxLayers ? maxFrames:0); }
    epochs=std::vector<Epoch>(maxFrames*storageLayers+storageLayers+2);
    for(size_t i=1;i<epochs.size();++i)epochs[i].next=i+1<epochs.size() ? uint32_t(i+1):0;
    freeEpoch=1;sequence=clearedThrough=0;generation.fill(0);activeLayers=redoLayers=0;
    storageBytes=maxFrames*bytesPerFrame+fixedBytes;reset();setSeamMilliseconds(5);return maxFrames>0;
}
void LooperCore::reset() noexcept
{
    apply(Action::Clear);timeline=0;hasPending=false;
    Command discarded;for(int i=0;i<64 && commands.pop(discarded);++i){}
}
uint32_t LooperCore::newEpoch() noexcept
{
    const auto id=freeEpoch;if(!id)return 0;
    auto& e=epochs[id];freeEpoch=e.next;e.references=0;e.sequence=++sequence;e.state=EpochState::Active;return id;
}
void LooperCore::releaseEpoch(uint32_t id) noexcept
{
    if(!id)return;
    auto& e=epochs[id];
    if(--e.references==0 && e.state!=EpochState::Active){e.state=EpochState::Free;e.next=freeEpoch;freeEpoch=id;}
}
void LooperCore::markEpoch(uint32_t id,EpochState state) noexcept
{
    if(!id)return;
    auto& e=epochs[id];e.state=state;
    if(e.references==0){e.state=EpochState::Free;e.next=freeEpoch;freeEpoch=id;}
}
void LooperCore::discardIndex(size_t index) noexcept
{
    for(int k=0;k<=maxLayers;++k)
    {auto& s=layer[k][index];releaseEpoch(s.generation);s.generation=0;}
}
void LooperCore::foldIndex(size_t index) noexcept
{
    for(int k=0;k<=maxLayers;++k)
    {
        auto& s=layer[k][index];
        if(s.generation && (epochs[s.generation].sequence<=clearedThrough || epochs[s.generation].state==EpochState::Discarded))
        {releaseEpoch(s.generation);s.generation=0;}
    }
    // Fold in chronological order for exactly the same addition order as
    // playback, even after many rapid partial gestures before revisiting here.
    for(int pass=0;pass<=maxLayers;++pass)
    {
        int oldest=-1;uint64_t first=std::numeric_limits<uint64_t>::max();
        for(int k=0;k<=maxLayers;++k)
        {
            const auto id=layer[k][index].generation;
            if(id && epochs[id].state==EpochState::Retired && epochs[id].sequence<first)
            {first=epochs[id].sequence;oldest=k;}
        }
        if(oldest<0)break;
        auto& s=layer[oldest][index];base[index].l+=s.audio.l;base[index].r+=s.audio.r;
        releaseEpoch(s.generation);s.generation=0;
    }
}
void LooperCore::setSeamMilliseconds(double ms) noexcept
{
    seam=size_t(sr*.001*(std::isfinite(ms)?std::clamp(ms,0.,10.):0.));
}
void LooperCore::closeRecording() noexcept
{
    loopLength=head;head=0;current=loopLength ? State::Playing:State::Empty;gain=0;
}
void LooperCore::closeOverdub() noexcept
{
    ++activeLayers;redoLayers=0;current=State::Playing;
    if(activeLayers>maxLayers)
    {
        markEpoch(generation[0],EpochState::Retired);
        std::rotate(layer.begin(),layer.begin()+1,layer.begin()+maxLayers+1);
        std::rotate(generation.begin(),generation.begin()+1,generation.begin()+maxLayers+1);
        generation[--activeLayers]=0;
    }
}
void LooperCore::apply(Action action) noexcept
{
    switch(action)
    {
        case Action::Clear:
            for(auto id:generation)if(id)markEpoch(id,EpochState::Discarded);
            generation.fill(0);clearedThrough=sequence;
            current=State::Empty;loopLength=head=0;activeLayers=redoLayers=0;gain=0;layerLimit=false;break;
        case Action::Record:
            if(current==State::Empty && maxFrames) { current=State::Recording;head=0; }
            else if(current==State::Recording)closeRecording();
            break;
        case Action::Play:
            if(current==State::Recording)closeRecording();
            else if(current==State::Overdubbing)closeOverdub();
            else if(loopLength) { if(current==State::Stopped)head=0;current=State::Playing; }
            break;
        case Action::Overdub:
            if(current==State::Playing)
            {
                for(int i=activeLayers;i<activeLayers+redoLayers;++i)
                {markEpoch(generation[i],EpochState::Discarded);generation[i]=0;}
                redoLayers=0;
                const auto id=newEpoch();
                if(id){generation[activeLayers]=id;current=State::Overdubbing;layerLimit=false;}
                else layerLimit=true; // pool exhaustion is an invariant failure, never a count limit
            }
            break;
        case Action::Stop:
            if(current==State::Recording)closeRecording();
            if(current==State::Overdubbing)closeOverdub();
            if(loopLength)current=State::Stopped;
            break;
        case Action::Undo:
            if(current==State::Playing && activeLayers>0){--activeLayers;++redoLayers;}
            break;
        case Action::Redo:
            if(current==State::Playing && redoLayers>0){++activeLayers;--redoLayers;}
            break;
        case Action::CancelOverdub:
            if(current==State::Overdubbing)
            {markEpoch(generation[activeLayers],EpochState::Discarded);generation[activeLayers]=0;current=State::Playing;}
            break;
    }
}
LooperCore::Stereo LooperCore::playback(size_t index) const noexcept
{
    Stereo result=base[index];
    const int count=activeLayers+(current==State::Overdubbing ? 1:0);
    for(int k=0;k<count;++k)
    {
        const auto& sample=layer[k][index];
        if(sample.generation==generation[k]){result.l+=sample.audio.l;result.r+=sample.audio.r;}
    }
    return result;
}
void LooperCore::process(double* left,double* right,int n,double loopGain,double dryGain) noexcept
{
    auto finite=[](double v) noexcept {return std::isfinite(v)?v:0.;};
    loopGain=finite(loopGain);dryGain=finite(dryGain);
    for(int i=0;i<n;++i,++timeline)
    {
        for(int c=0;c<64;++c)
        {
            if(!hasPending)hasPending=commands.pop(pending);
            if(!hasPending || pending.sample>timeline)break;
            apply(pending.action);hasPending=false;
        }
        const Stereo input{finite(left[i]),finite(right[i])};Stereo wet{};
        if(current==State::Recording)
        {
            discardIndex(head);base[head++]=input;
            if(head==maxFrames)closeRecording();
        }
        else if(loopLength && (current==State::Playing || current==State::Overdubbing || gain>0))
        {
            foldIndex(head);
            if(current==State::Overdubbing)
            {
                auto& sample=layer[activeLayers][head];
                if(sample.generation!=generation[activeLayers])
                {
                    releaseEpoch(sample.generation);sample.audio={};sample.generation=generation[activeLayers];
                    ++epochs[sample.generation].references;
                }
                sample.audio.l+=input.l;sample.audio.r+=input.r;
            }
            wet=playback(head);
            const double target=current==State::Stopped ? 0:1;
            gain+=std::clamp(target-gain,-1/(sr*.005),1/(sr*.005));
            double seamGain=1;
            const size_t edge=std::min(seam,loopLength/2);
            if(edge>0)
            {
                const size_t distance=std::min(head,loopLength-1-head);
                if(distance<edge)seamGain=.5-.5*std::cos(3.14159265358979323846*double(distance)/edge);
            }
            wet.l*=gain*seamGain;wet.r*=gain*seamGain;
            if(++head==loopLength)head=0;
        }
        left[i]=finite(input.l*dryGain+wet.l*loopGain);right[i]=finite(input.r*dryGain+wet.r*loopGain);
    }
}
}
