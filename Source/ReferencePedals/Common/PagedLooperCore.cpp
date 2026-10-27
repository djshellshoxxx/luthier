// OWNER: WP15/WP16
#include "PagedLooperCore.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
namespace luthier::reference::common
{
struct PagedLooperCore::Impl
{
    Config config;
    std::array<std::unique_ptr<LooperStorage>,11> streams;
    std::array<int,8> active{},redo{};
    std::array<int,10> retired{};
    std::array<bool,11> used{};
    int count=0,activeCount=0,redoCount=0,retiredCount=0,scratch=-1,merging=-1;
    AssetHash mergeBaseHash;
    uint64_t mergeToken=0,frames=0,head=0,maxFrames=0,generation=0;
    int64_t timeline=0;
    size_t ram=0;uint64_t disk=0;
    uint32_t flags=0;
    State current=State::Empty;
    double gain=0;uint64_t seam=0;
    SpscRing<Command,64> commands;Command pending{};bool hasPending=false;
    struct References
    {
        bool resolve=false,frozen=false;
        AssetHash frozenBase;
        int layerCount=0,redoCount=0,retiredCount=0;
        std::array<int,11> ids{};
        std::array<uint64_t,11> epochs{};
    };
    struct Publication
    {
        std::atomic<uint64_t> sequence{0},generation{0},frames{0};
        std::atomic<uint32_t> status{uint32_t(CaptureStatus::Empty)},faults{0},count{0},redoCount{0},retiredCount{0};
        std::atomic<bool> resolve{false},frozen{false};
        std::atomic<int> sourceCount{0},sourceRedoCount{0},sourceRetiredCount{0};
        std::array<std::atomic<int>,11> ids{};
        std::array<std::atomic<uint64_t>,11> epochs{};
        std::array<std::atomic<uint64_t>,80> hashes{};
        void store(const Graph& graph,const References& refs) noexcept
        {
            sequence.fetch_add(1,std::memory_order_acq_rel);
            generation.store(graph.generation,std::memory_order_relaxed);frames.store(graph.frames,std::memory_order_relaxed);
            status.store(uint32_t(graph.status),std::memory_order_relaxed);faults.store(graph.faults,std::memory_order_relaxed);count.store(uint32_t(graph.layerCount),std::memory_order_relaxed);retiredCount.store(uint32_t(graph.retiredCount),std::memory_order_relaxed);
            redoCount.store(uint32_t(graph.redoCount),std::memory_order_relaxed);
            resolve.store(refs.resolve,std::memory_order_relaxed);frozen.store(refs.frozen,std::memory_order_relaxed);sourceCount.store(refs.layerCount,std::memory_order_relaxed);sourceRetiredCount.store(refs.retiredCount,std::memory_order_relaxed);
            sourceRedoCount.store(refs.redoCount,std::memory_order_relaxed);
            for(size_t i=0;i<11;++i){ids[i].store(refs.ids[i],std::memory_order_relaxed);epochs[i].store(refs.epochs[i],std::memory_order_relaxed);}
            for(size_t h=0;h<20;++h){const auto& hash=h==0?graph.base:h<=8?graph.layers[h-1]:h<=16?graph.redo[h-9]:h<=18?graph.retired[h-17]:refs.frozenBase;for(size_t i=0;i<4;++i){uint64_t word;std::memcpy(&word,hash.bytes.data()+i*8,8);hashes[h*4+i].store(word,std::memory_order_relaxed);}}
            sequence.fetch_add(1,std::memory_order_release);
        }
        bool load(Graph& out,References& refs,uint64_t& sequenceOut) const noexcept
        {
            for(int attempt=0;attempt<8;++attempt)
            {
                const auto before=sequence.load(std::memory_order_acquire);if(before&1)continue;
                Graph next;References nextRefs;next.generation=generation.load(std::memory_order_relaxed);next.frames=frames.load(std::memory_order_relaxed);
                next.status=CaptureStatus(status.load(std::memory_order_relaxed));next.faults=faults.load(std::memory_order_relaxed);next.layerCount=int(count.load(std::memory_order_relaxed));next.retiredCount=int(retiredCount.load(std::memory_order_relaxed));
                next.redoCount=int(redoCount.load(std::memory_order_relaxed));
                nextRefs.resolve=resolve.load(std::memory_order_relaxed);nextRefs.frozen=frozen.load(std::memory_order_relaxed);nextRefs.layerCount=sourceCount.load(std::memory_order_relaxed);nextRefs.retiredCount=sourceRetiredCount.load(std::memory_order_relaxed);
                nextRefs.redoCount=sourceRedoCount.load(std::memory_order_relaxed);
                for(size_t i=0;i<11;++i){nextRefs.ids[i]=ids[i].load(std::memory_order_relaxed);nextRefs.epochs[i]=epochs[i].load(std::memory_order_relaxed);}
                for(size_t h=0;h<20;++h){auto& hash=h==0?next.base:h<=8?next.layers[h-1]:h<=16?next.redo[h-9]:h<=18?next.retired[h-17]:nextRefs.frozenBase;for(size_t i=0;i<4;++i){const auto word=hashes[h*4+i].load(std::memory_order_relaxed);std::memcpy(hash.bytes.data()+i*8,&word,8);}}
                std::atomic_thread_fence(std::memory_order_acquire);if(sequence.load(std::memory_order_relaxed)!=before)continue;
                out=next;refs=nextRefs;sequenceOut=before;return true;
            }
            return false;
        }
    } published;
    explicit Impl(Config c):config(c){}
    void publish() noexcept
    {
        Graph graph;References refs;graph.generation=generation;graph.frames=frames;graph.faults=flags|(commands.dropped()?uint32_t(256):0);
        if(flags&LooperStorage::ioFailure)graph.status=CaptureStatus::Failed;
        else if(current==State::Recording)graph.status=CaptureStatus::Recording;
        else if(!frames)graph.status=CaptureStatus::Empty;
        else
        {
            graph.status=CaptureStatus::Pending;
            if(current!=State::Overdubbing)
            {
                refs.resolve=true;refs.layerCount=activeCount;refs.redoCount=redoCount;refs.retiredCount=retiredCount;refs.frozen=merging>=0;refs.frozenBase=mergeBaseHash;
                refs.ids[0]=0;refs.epochs[0]=streams[0]->generation();
                for(int i=0;i<activeCount;++i){const auto slot=size_t(i+1);refs.ids[slot]=active[size_t(i)];refs.epochs[slot]=streams[size_t(active[size_t(i)])]->generation();}
                for(int i=0;i<redoCount;++i){const auto slot=size_t(activeCount+i+1);refs.ids[slot]=redo[size_t(i)];refs.epochs[slot]=streams[size_t(redo[size_t(i)])]->generation();}
                for(int i=0;i<retiredCount;++i){const auto slot=size_t(i+9);refs.ids[slot]=retired[size_t(i)];refs.epochs[slot]=streams[size_t(retired[size_t(i)])]->generation();}
            }
        }
        published.store(graph,refs);
    }
    void clear() noexcept
    {
        used.fill(false);used[0]=true;activeCount=redoCount=retiredCount=0;scratch=merging=-1;mergeToken=0;mergeBaseHash={};
        frames=head=0;gain=0;flags=0;current=State::Empty;++generation;
        streams[0]->beginRecord();
    }
    void closeRecord() noexcept
    {
        frames=streams[0]->length();streams[0]->finishRecord();head=0;gain=0;current=frames?State::Playing:State::Empty;++generation;
    }
    void discardRedo() noexcept
    {for(int i=0;i<redoCount;++i)used[size_t(redo[size_t(i)])]=false;redoCount=0;}
    bool startOverdub() noexcept
    {
        if(!frames)return false;
        for(int i=1;i<count;++i)if(!used[size_t(i)] && streams[size_t(i)]->beginLayer(frames))
        {scratch=i;used[size_t(i)]=true;current=State::Overdubbing;++generation;return true;}
        flags|=LooperStorage::writeOverrun;return false;
    }
    void closeOverdub() noexcept
    {
        if(scratch<0)return;
        streams[size_t(scratch)]->finishRecord();
        discardRedo(); // A provisional press/hold may still cancel and redo.
        if(activeCount==config.undoDepth)
        {
            retired[size_t(retiredCount++)]=active[0];
            for(int i=1;i<activeCount;++i)active[size_t(i-1)]=active[size_t(i)];
            --activeCount;
        }
        active[size_t(activeCount++)]=scratch;scratch=-1;current=State::Playing;++generation;
    }
    void cancelOverdub() noexcept
    {if(scratch>=0){used[size_t(scratch)]=false;scratch=-1;++generation;}current=frames?State::Playing:State::Empty;}
    bool windowReady(int id,bool stale=false) const noexcept
    {
        if(!frames)return true;
        const auto pages=(frames+uint64_t(config.pageFrames)-1)/uint64_t(config.pageFrames);
        for(uint64_t ahead=0;ahead<std::min(pages,uint64_t(config.readAhead+1));++ahead)
        {
            const auto frame=((head/uint64_t(config.pageFrames)+ahead)%pages)*uint64_t(config.pageFrames);
            if(!streams[size_t(id)]->available(frame,stale))return false;
        }
        return true;
    }
    void service() noexcept
    {
        if(frames && current!=State::Recording)
        {
            streams[0]->prefetch(head);
            for(int i=0;i<activeCount;++i)streams[size_t(active[size_t(i)])]->prefetch(head);
            for(int i=0;i<retiredCount;++i)streams[size_t(retired[size_t(i)])]->prefetch(head);
            if(scratch>=0)streams[size_t(scratch)]->prefetch(head);
        }
        if(merging>=0 && streams[0]->mergeComplete(mergeToken))
        {
            streams[0]->prefetch(head);
            if(windowReady(0))
            {
                used[size_t(merging)]=false;
                for(int i=1;i<retiredCount;++i)retired[size_t(i-1)]=retired[size_t(i)];
                --retiredCount;
                streams[0]->acknowledgeMerge(mergeToken);merging=-1;mergeToken=0;mergeBaseHash={};++generation;
            }
        }
        if(merging<0 && retiredCount)
        {
            LooperStorage::Capture before;
            if(streams[0]->capture(before) && before.status==CaptureStatus::Ready)
            {
                const auto token=streams[0]->mergeLayer(*streams[size_t(retired[0])]);
                if(token){merging=retired[0];mergeToken=token;mergeBaseHash=before.hash;}
            }
        }
        for(int i=0;i<count;++i)if(used[size_t(i)])flags|=streams[size_t(i)]->errors();
        if(flags&LooperStorage::ioFailure){current=frames?State::Stopped:State::Empty;gain=0;}
    }
    bool playback(uint64_t index,double& l,double& r) noexcept
    {
        uint64_t version=0;bool ok=streams[0]->read(index,l,r,&version,merging>=0);
        for(int i=0;i<retiredCount;++i)
        {
            const auto id=retired[size_t(i)];if(id==merging && streams[0]->mergedAt(index,version,mergeToken))continue;
            double dl=0,dr=0;ok=streams[size_t(id)]->read(index,dl,dr)&&ok;l+=dl;r+=dr;
        }
        for(int i=0;i<activeCount;++i){double dl=0,dr=0;ok=streams[size_t(active[size_t(i)])]->read(index,dl,dr)&&ok;l+=dl;r+=dr;}
        if(scratch>=0){double dl=0,dr=0;ok=streams[size_t(scratch)]->read(index,dl,dr)&&ok;l+=dl;r+=dr;}
        if(!ok){l=r=0;flags|=LooperStorage::readUnderrun;}
        return ok;
    }
};
PagedLooperCore::PagedLooperCore()=default;PagedLooperCore::~PagedLooperCore()=default;
bool PagedLooperCore::prepare(IAssetStore& store,const Config& config)
{
    if(!std::isfinite(config.sampleRate) || !std::isfinite(config.seconds) || config.sampleRate<1 || config.sampleRate>192000
       || config.seconds<=0 || config.seconds>5400 || config.channels<1 || config.channels>2 || config.undoDepth<1 || config.undoDepth>8)return false;
    LooperStorage::Config stream;stream.sampleRate=config.sampleRate;stream.channels=config.channels;stream.pageFrames=config.pageFrames;
    stream.writePages=config.writePages;stream.readPages=config.readPages;stream.readAhead=config.readAhead;
    stream.maxFrames=uint64_t(config.sampleRate*config.seconds);stream.ramBudget=config.ramBudget;stream.storageBudget=config.storageBudget;
    size_t ram=0;uint64_t disk=0;if(!LooperStorage::reservation(stream,ram,disk))return false;
    const auto count=uint64_t(config.undoDepth+3);
    if(ram>std::numeric_limits<size_t>::max()/count || disk>std::numeric_limits<uint64_t>::max()/count)return false;
    const auto aggregateRam=sizeof(Impl)+count*(ram+sizeof(LooperStorage));const auto aggregateDisk=count*disk;
    if(aggregateRam>config.ramBudget || aggregateDisk>config.storageBudget)return false;
    try
    {
        auto next=std::make_unique<Impl>(config);next->count=int(count);next->ram=size_t(aggregateRam);next->disk=aggregateDisk;next->maxFrames=stream.maxFrames;
        for(int i=0;i<next->count;++i){next->streams[size_t(i)]=std::make_unique<LooperStorage>();if(!next->streams[size_t(i)]->prepare(store,stream))return false;}
        next->clear();next->publish();impl=std::move(next);return true;
    }
    catch(const std::exception&){return false;}
}
void PagedLooperCore::reset() noexcept
{
    if(!impl)return;
    auto& p=*impl;p.clear();p.timeline=0;p.hasPending=false;Command discarded;for(int i=0;i<64 && p.commands.pop(discarded);++i){}p.publish();
}
bool PagedLooperCore::enqueue(Command command) noexcept{return impl && impl->commands.push(command);}
void PagedLooperCore::apply(Action action) noexcept
{
    if(!impl)return;
    auto& p=*impl;
    switch(action)
    {
        case Action::Record:p.clear();p.current=State::Recording;break;
        case Action::Clear:p.clear();break;
        case Action::Play:if(p.current==State::Recording)p.closeRecord();else if(p.current==State::Overdubbing)p.closeOverdub();else if(p.frames){if(p.current==State::Stopped)p.head=0;p.current=State::Playing;}break;
        case Action::Overdub:if(p.current==State::Recording)p.closeRecord();if(p.current!=State::Overdubbing)p.startOverdub();break;
        case Action::CancelOverdub:if(p.current==State::Overdubbing)p.cancelOverdub();break;
        case Action::Stop:if(p.current==State::Recording)p.closeRecord();else if(p.current==State::Overdubbing)p.closeOverdub();if(p.frames)p.current=State::Stopped;break;
        case Action::Undo:
            if(p.current==State::Playing && p.activeCount){p.redo[size_t(p.redoCount++)]=p.active[size_t(--p.activeCount)];++p.generation;}
            break;
        case Action::Redo:if(p.current==State::Playing && p.redoCount){p.active[size_t(p.activeCount++)]=p.redo[size_t(--p.redoCount)];++p.generation;}break;
    }
    p.service();p.publish();
}
bool PagedLooperCore::canUndo() const noexcept
{return impl && impl->current==State::Playing && impl->activeCount>0;}
bool PagedLooperCore::canRedo() const noexcept
{return impl && impl->current==State::Playing && impl->redoCount>0;}
void PagedLooperCore::process(double* left,double* right,int n,double loopGain,double dryGain) noexcept
{
    if(!impl || n<0 || (n && (!left || !right)))return;
    auto& p=*impl;p.service();
    loopGain=std::isfinite(loopGain)?loopGain:0;dryGain=std::isfinite(dryGain)?dryGain:0;
    for(int i=0;i<n;++i,++p.timeline)
    {
        if(!p.hasPending)p.hasPending=p.commands.pop(p.pending);
        for(int dispatched=0;p.hasPending && p.pending.sample<=p.timeline && dispatched<64;++dispatched)
        {apply(p.pending.action);p.hasPending=p.commands.pop(p.pending);}
        const auto inputL=left[i],inputR=right[i];double wetL=0,wetR=0;
        if(p.current==State::Recording)
        {
            const auto recordL=p.config.channels==1?.5*(inputL+inputR):inputL;
            if(!p.streams[0]->append(recordL,inputR) || p.streams[0]->length()==p.maxFrames)p.closeRecord();
            else p.frames=p.streams[0]->length();
        }
        else if(p.frames)
        {
            const bool playing=p.current==State::Playing || p.current==State::Overdubbing;
            const double step=1./std::max(1.,p.config.sampleRate*.005);p.gain=std::clamp(p.gain+(playing?step:-step),0.,1.);
            if(!(p.flags&LooperStorage::ioFailure) && (playing || p.gain>0))
            {
                p.playback(p.head,wetL,wetR);
                if(p.current==State::Overdubbing && p.scratch>=0)
                {
                    const auto recordL=p.config.channels==1?.5*(inputL+inputR):inputL;
                    if(!p.streams[size_t(p.scratch)]->add(p.head,recordL,inputR))p.closeOverdub();
                }
                double edge=1;
                const auto seam=std::min(p.seam,p.frames/2);
                if(seam){if(p.head<seam)edge=double(p.head)/double(seam);else if(p.head>=p.frames-seam)edge=double(p.frames-1-p.head)/double(seam);}
                wetL*=p.gain*loopGain*edge;wetR*=p.gain*loopGain*edge;p.head=(p.head+1)%p.frames;
            }
        }
        if(p.current==State::Stopped && p.gain==0)p.head=0;
        left[i]=inputL*dryGain+wetL;right[i]=inputR*dryGain+wetR;
    }
    p.service();p.publish();
}
void PagedLooperCore::setSeamMilliseconds(double ms) noexcept{if(impl)impl->seam=uint64_t(impl->config.sampleRate*.001*(std::isfinite(ms)?std::clamp(ms,0.,10.):0.));}
PagedLooperCore::State PagedLooperCore::state() const noexcept{return impl?impl->current:State::Empty;}
uint64_t PagedLooperCore::length() const noexcept{return impl?impl->frames:0;}
uint64_t PagedLooperCore::playhead() const noexcept{return impl?impl->head:0;}
uint64_t PagedLooperCore::capacity() const noexcept{return impl?impl->maxFrames:0;}
int64_t PagedLooperCore::samplePosition() const noexcept{return impl?impl->timeline:0;}
int PagedLooperCore::undoDepth() const noexcept{return impl?impl->activeCount:0;}
uint32_t PagedLooperCore::faults() const noexcept{return impl?impl->flags:LooperStorage::invalidAsset;}
bool PagedLooperCore::playbackReady() const noexcept
{
    if(!impl)return false;
    const auto& p=*impl;if(p.flags&LooperStorage::ioFailure)return false;
    if(!p.frames)return true;
    if(!p.windowReady(0,p.merging>=0))return false;
    for(int i=0;i<p.activeCount;++i)if(!p.windowReady(p.active[size_t(i)]))return false;
    for(int i=0;i<p.retiredCount;++i)if(!p.windowReady(p.retired[size_t(i)]))return false;
    return p.scratch<0 || p.windowReady(p.scratch);
}
size_t PagedLooperCore::reservedBytes() const noexcept{return impl?impl->ram:0;}
uint64_t PagedLooperCore::reservedStorageBytes() const noexcept{return impl?impl->disk:0;}
bool PagedLooperCore::capture(Graph& out) const noexcept
{
    if(!impl)return false;
    const auto& p=*impl;
    for(int attempt=0;attempt<8;++attempt)
    {
        Graph next;Impl::References refs;uint64_t sequence=0;if(!p.published.load(next,refs,sequence))continue;
        bool stable=true;
        if(refs.resolve)
        {
            bool ready=true,failed=false;std::array<LooperStorage::Capture,11> sources;
            for(size_t slot=0;slot<11;++slot)
            {
                if(slot>0 && !(slot<=size_t(refs.layerCount+refs.redoCount) || (slot>=9 && slot<size_t(9+refs.retiredCount))))continue;
                if(!p.streams[size_t(refs.ids[slot])]->capture(sources[slot]) || sources[slot].generation!=refs.epochs[slot] || sources[slot].frames!=next.frames){stable=false;break;}
                next.faults|=sources[slot].faults;
                failed=failed || sources[slot].status==CaptureStatus::Failed;
                if(!(slot==0 && refs.frozen))ready=ready && sources[slot].status==CaptureStatus::Ready;
            }
            if(failed)next.status=CaptureStatus::Failed;
            else if(stable && ready)
            {
                next.status=CaptureStatus::Ready;next.base=refs.frozen?refs.frozenBase:sources[0].hash;next.layerCount=refs.layerCount;next.retiredCount=refs.retiredCount;
                next.redoCount=refs.redoCount;
                for(int i=0;i<next.layerCount;++i)next.layers[size_t(i)]=sources[size_t(i+1)].hash;
                for(int i=0;i<next.redoCount;++i)next.redo[size_t(i)]=sources[size_t(next.layerCount+i+1)].hash;
                for(int i=0;i<next.retiredCount;++i)next.retired[size_t(i)]=sources[size_t(i+9)].hash;
            }
        }
        std::atomic_thread_fence(std::memory_order_acquire);
        if(!stable || p.published.sequence.load(std::memory_order_relaxed)!=sequence)continue;
        out=next;return true;
    }
    return false;
}
bool PagedLooperCore::restore(const Graph& graph)
{
    if(!impl || (graph.status!=CaptureStatus::Ready && graph.status!=CaptureStatus::Empty) || graph.layerCount<0 || graph.layerCount>impl->config.undoDepth || graph.redoCount<0 || graph.redoCount>8 || graph.layerCount+graph.redoCount>impl->config.undoDepth || graph.retiredCount<0 || graph.retiredCount>2
       || graph.frames>impl->maxFrames || (graph.status==CaptureStatus::Ready && (!graph.frames || graph.base.isNull())))return false;
    auto& p=*impl;
    if(graph.status==CaptureStatus::Empty)
    {
        if(graph.frames || graph.layerCount || graph.redoCount || graph.retiredCount || !graph.base.isNull())return false;
        p.clear();p.publish();return true;
    }
    for(int i=0;i<graph.layerCount;++i)if(graph.layers[size_t(i)].isNull())return false;
    for(int i=0;i<graph.retiredCount;++i)if(graph.retired[size_t(i)].isNull())return false;
    for(int i=0;i<graph.redoCount;++i)if(graph.redo[size_t(i)].isNull())return false;
    // Called on an unpublished processor during its stopped lifecycle. Store
    // failures are explicit; a partially restored processor is never Ready.
    p.clear();
    if(!p.streams[0]->restore(graph.base)){p.flags|=LooperStorage::ioFailure;p.publish();return false;}
    for(int i=0;i<graph.layerCount;++i)
    {
        const auto id=i+1;
        if(!p.streams[size_t(id)]->restore(graph.layers[size_t(i)]) || p.streams[size_t(id)]->length()!=graph.frames)
        {p.flags|=LooperStorage::ioFailure;p.publish();return false;}
        p.active[size_t(p.activeCount++)]=id;p.used[size_t(id)]=true;
    }
    for(int i=0;i<graph.redoCount;++i)
    {
        const auto id=graph.layerCount+i+1;
        if(!p.streams[size_t(id)]->restore(graph.redo[size_t(i)]) || p.streams[size_t(id)]->length()!=graph.frames)
        {p.flags|=LooperStorage::ioFailure;p.publish();return false;}
        p.redo[size_t(p.redoCount++)]=id;p.used[size_t(id)]=true;
    }
    if(p.streams[0]->length()!=graph.frames){p.flags|=LooperStorage::ioFailure;p.publish();return false;}
    for(int i=0;i<graph.retiredCount;++i)
    {
        const auto id=graph.layerCount+graph.redoCount+i+1;
        if(graph.retired[size_t(i)].isNull() || !p.streams[size_t(id)]->restore(graph.retired[size_t(i)]) || p.streams[size_t(id)]->length()!=graph.frames)
        {p.flags|=LooperStorage::ioFailure;p.publish();return false;}
        p.retired[size_t(p.retiredCount++)]=id;p.used[size_t(id)]=true;
    }
    p.frames=graph.frames;p.current=State::Stopped;p.head=0;p.gain=0;p.publish();return true;
}
}
