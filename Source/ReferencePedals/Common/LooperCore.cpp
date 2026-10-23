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
    const size_t bytesPerFrame=sizeof(Stereo)+size_t(maxLayers)*sizeof(LayerSample);
    const double requested=sr*(std::isfinite(seconds)?std::clamp(seconds,0.,5400.):0.);
    maxFrames=std::min(size_t(requested),budget/bytesPerFrame);
    base=std::vector<Stereo>(maxFrames);
    for(int i=0;i<8;++i) { layer[i]=std::vector<LayerSample>(i<maxLayers ? maxFrames:0); }
    storageBytes=maxFrames*bytesPerFrame;reset();setSeamMilliseconds(5);return maxFrames>0;
}
void LooperCore::reset() noexcept
{
    current=State::Empty;loopLength=head=0;activeLayers=redoLayers=0;gain=0;layerLimit=false;
    ++epoch;timeline=0;hasPending=false;commands.clear();
}
void LooperCore::setSeamMilliseconds(double ms) noexcept
{
    seam=size_t(sr*.001*(std::isfinite(ms)?std::clamp(ms,0.,10.):0.));
}
void LooperCore::closeRecording() noexcept
{
    loopLength=head;head=0;current=loopLength ? State::Playing:State::Empty;gain=0;
}
void LooperCore::apply(Action action) noexcept
{
    switch(action)
    {
        case Action::Clear:
            current=State::Empty;loopLength=head=0;activeLayers=redoLayers=0;gain=0;layerLimit=false;++epoch;break;
        case Action::Record:
            if(current==State::Empty && maxFrames) { current=State::Recording;head=0; }
            else if(current==State::Recording)closeRecording();
            break;
        case Action::Play:
            if(current==State::Recording)closeRecording();
            else if(current==State::Overdubbing) {++activeLayers;redoLayers=0;current=State::Playing;}
            else if(loopLength) { if(current==State::Stopped)head=0;current=State::Playing; }
            break;
        case Action::Overdub:
            if(current==State::Playing && activeLayers<maxLayers)
            {
                generation[activeLayers]=++epoch;current=State::Overdubbing;redoLayers=0;layerLimit=false;
            }
            else if(current==State::Playing)layerLimit=true;
            break;
        case Action::Stop:
            if(current==State::Recording)closeRecording();
            if(current==State::Overdubbing){++activeLayers;redoLayers=0;}
            if(loopLength)current=State::Stopped;
            break;
        case Action::Undo:
            if(current==State::Playing && activeLayers>0){--activeLayers;++redoLayers;}
            break;
        case Action::Redo:
            if(current==State::Playing && redoLayers>0){++activeLayers;--redoLayers;}
            break;
        case Action::CancelOverdub:
            if(current==State::Overdubbing)current=State::Playing;
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
            base[head++]=input;
            if(head==maxFrames)closeRecording();
        }
        else if(loopLength && (current==State::Playing || current==State::Overdubbing || gain>0))
        {
            if(current==State::Overdubbing)
            {
                auto& sample=layer[activeLayers][head];
                if(sample.generation!=generation[activeLayers]) {sample.audio={};sample.generation=generation[activeLayers];}
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
