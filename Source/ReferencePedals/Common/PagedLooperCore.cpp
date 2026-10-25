// OWNER: WP15/WP16
#include "PagedLooperCore.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <chrono>
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
        std::array<int,27> ids{};
        std::array<uint64_t,27> epochs{};
    };
    struct Publication
    {
        static_assert(std::atomic<uint64_t>::is_always_lock_free,"Loop state needs lock-free atomic words");
        static constexpr size_t slots=27,wordCount=14+slots*2+(slots+1)*4;
        std::atomic<uint64_t> sequence{0};
        std::array<std::atomic<uint64_t>,wordCount> words{};
        static const AssetHash& hashAt(const Graph& graph,size_t slot) noexcept
        {return slot==0?graph.base:slot<=8?graph.layers[slot-1]:slot<=16?graph.redo[slot-9]:graph.retired[slot-17];}
        static AssetHash& hashAt(Graph& graph,size_t slot) noexcept
        {return slot==0?graph.base:slot<=8?graph.layers[slot-1]:slot<=16?graph.redo[slot-9]:graph.retired[slot-17];}
        void store(const Graph& graph,const References& refs) noexcept
        {
            std::array<uint64_t,wordCount> packed{};size_t at=0;
            for(auto value:{graph.generation,graph.frames,graph.playhead,uint64_t(graph.transport),uint64_t(graph.status),uint64_t(graph.faults),
                uint64_t(graph.layerCount),uint64_t(graph.redoCount),uint64_t(graph.retiredCount),uint64_t(refs.resolve),uint64_t(refs.frozen),
                uint64_t(refs.layerCount),uint64_t(refs.redoCount),uint64_t(refs.retiredCount)})packed[at++]=value;
            for(auto id:refs.ids)packed[at++]=uint64_t(id);
            for(auto epoch:refs.epochs)packed[at++]=epoch;
            for(size_t h=0;h<=slots;++h)
            {
                const auto& hash=h==slots?refs.frozenBase:hashAt(graph,h);
                for(size_t i=0;i<4;++i){uint64_t word;std::memcpy(&word,hash.bytes.data()+i*8,8);packed[at++]=word;}
            }
            sequence.fetch_add(1,std::memory_order_acq_rel);
            for(size_t i=0;i<wordCount;++i)words[i].store(packed[i],std::memory_order_relaxed);
            sequence.fetch_add(1,std::memory_order_release);
        }
        bool load(Graph& out,References& refs,uint64_t& sequenceOut) const noexcept
        {
            for(int attempt=0;attempt<8;++attempt)
            {
                const auto before=sequence.load(std::memory_order_acquire);if(before&1)continue;
                std::array<uint64_t,wordCount> packed{};for(size_t i=0;i<wordCount;++i)packed[i]=words[i].load(std::memory_order_relaxed);
                std::atomic_thread_fence(std::memory_order_acquire);if(sequence.load(std::memory_order_relaxed)!=before)continue;
                Graph next;References nextRefs;size_t at=0;
                next.generation=packed[at++];next.frames=packed[at++];next.playhead=packed[at++];next.transport=State(packed[at++]);next.status=CaptureStatus(packed[at++]);next.faults=uint32_t(packed[at++]);
                next.layerCount=int(packed[at++]);next.redoCount=int(packed[at++]);next.retiredCount=int(packed[at++]);
                nextRefs.resolve=packed[at++]!=0;nextRefs.frozen=packed[at++]!=0;nextRefs.layerCount=int(packed[at++]);nextRefs.redoCount=int(packed[at++]);nextRefs.retiredCount=int(packed[at++]);
                for(auto& id:nextRefs.ids)id=int(packed[at++]);
                for(auto& epoch:nextRefs.epochs)epoch=packed[at++];
                for(size_t h=0;h<=slots;++h)
                {
                    auto& hash=h==slots?nextRefs.frozenBase:hashAt(next,h);
                    for(size_t i=0;i<4;++i){const auto word=packed[at++];std::memcpy(hash.bytes.data()+i*8,&word,8);}
                }
                out=next;refs=nextRefs;sequenceOut=before;return true;
            }
            return false;
        }
    } published;
    explicit Impl(Config c):config(c){}
    void publish() noexcept
    {
        Graph graph;References refs;graph.generation=generation;graph.frames=frames;graph.transport=current;graph.playhead=head;graph.faults=flags|(commands.dropped()?uint32_t(256):0);
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
                for(int i=0;i<redoCount;++i){const auto slot=size_t(i+9);refs.ids[slot]=redo[size_t(i)];refs.epochs[slot]=streams[size_t(redo[size_t(i)])]->generation();}
                for(int i=0;i<retiredCount;++i){const auto slot=size_t(i+17);refs.ids[slot]=retired[size_t(i)];refs.epochs[slot]=streams[size_t(retired[size_t(i)])]->generation();}
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
            for(int i=0;i<redoCount;++i)streams[size_t(redo[size_t(i)])]->prefetch(head);
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
       || config.seconds<=0 || config.seconds>5400 || config.channels<1 || config.channels>2 || config.undoDepth<1 || config.undoDepth>8 || config.historyCapacity<0 || config.historyCapacity>8)return false;
    LooperStorage::Config stream;stream.sampleRate=config.sampleRate;stream.channels=config.channels;stream.pageFrames=config.pageFrames;
    stream.writePages=config.writePages;stream.readPages=config.readPages;stream.readAhead=config.readAhead;
    stream.maxFrames=uint64_t(config.sampleRate*config.seconds);stream.ramBudget=config.ramBudget;stream.storageBudget=config.storageBudget;
    size_t ram=0;uint64_t disk=0;if(!LooperStorage::reservation(stream,ram,disk))return false;
    const auto count=uint64_t(std::max(config.undoDepth,config.historyCapacity)+3);
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
        case Action::Play:if(p.current==State::Recording)p.closeRecord();else if(p.current==State::Overdubbing)p.closeOverdub();else if(p.frames)p.current=State::Playing;break;
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
void PagedLooperCore::beginBlock() noexcept {if(impl)impl->service();}
void PagedLooperCore::endBlock() noexcept {if(impl){impl->service();impl->publish();}}
void PagedLooperCore::processFrame(double& left,double& right,double loopGain,double dryGain) noexcept
{
    if(!impl)return;
    auto& p=*impl;
    loopGain=std::isfinite(loopGain)?loopGain:0;dryGain=std::isfinite(dryGain)?dryGain:0;
    if(!p.hasPending)p.hasPending=p.commands.pop(p.pending);
    for(int dispatched=0;p.hasPending && p.pending.sample<=p.timeline && dispatched<64;++dispatched)
    {apply(p.pending.action);p.hasPending=p.commands.pop(p.pending);}
    const auto inputL=left,inputR=right;double wetL=0,wetR=0;
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
            const auto envelope=p.gain<=0?0:p.gain>=1?1:.5-.5*std::cos(3.14159265358979323846*p.gain);
            wetL*=envelope*loopGain*edge;wetR*=envelope*loopGain*edge;if(++p.head==p.frames)p.head=0;
        }
    }
    if(p.current==State::Stopped && p.gain==0)p.head=0;
    left=inputL*dryGain+wetL;right=inputR*dryGain+wetR;
    ++p.timeline;
}
void PagedLooperCore::process(double* left,double* right,int n,double loopGain,double dryGain) noexcept
{
    if(!impl || n<0 || (n && (!left || !right)))return;
    beginBlock();for(int i=0;i<n;++i)processFrame(left[i],right[i],loopGain,dryGain);endBlock();
}
void PagedLooperCore::rewind() noexcept {if(impl){impl->head=0;impl->publish();}}
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
    for(int i=0;i<p.redoCount;++i)if(!p.windowReady(p.redo[size_t(i)]))return false;
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
            bool ready=true,failed=false;std::array<LooperStorage::Capture,27> sources;
            for(size_t slot=0;slot<27;++slot)
            {
                if(slot>0 && !(slot<=size_t(refs.layerCount) || (slot>=9 && slot<size_t(9+refs.redoCount)) || (slot>=17 && slot<size_t(17+refs.retiredCount))))continue;
                if(!p.streams[size_t(refs.ids[slot])]->capture(sources[slot]) || sources[slot].generation!=refs.epochs[slot] || sources[slot].frames!=next.frames){stable=false;break;}
                next.faults|=sources[slot].faults;
                failed=failed || sources[slot].status==CaptureStatus::Failed;
                if(!(slot==0 && refs.frozen))ready=ready && sources[slot].status==CaptureStatus::Ready;
            }
            if(failed)next.status=CaptureStatus::Failed;
            else if(stable && ready)
            {
                next.status=CaptureStatus::Ready;next.base=refs.frozen?refs.frozenBase:sources[0].hash;next.layerCount=refs.layerCount;next.redoCount=refs.redoCount;next.retiredCount=refs.retiredCount;
                for(int i=0;i<next.layerCount;++i)next.layers[size_t(i)]=sources[size_t(i+1)].hash;
                for(int i=0;i<next.redoCount;++i)next.redo[size_t(i)]=sources[size_t(i+9)].hash;
                for(int i=0;i<next.retiredCount;++i)next.retired[size_t(i)]=sources[size_t(i+17)].hash;
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
    if(!impl || (graph.status!=CaptureStatus::Ready && graph.status!=CaptureStatus::Empty)
       || graph.layerCount<0 || graph.layerCount>8 || graph.redoCount<0 || graph.redoCount>8 || graph.layerCount+graph.redoCount>8
       || graph.retiredCount<0 || graph.retiredCount>10 || graph.layerCount+graph.redoCount+graph.retiredCount>10
       || graph.frames>impl->maxFrames || graph.transport>State::Stopped
       || (graph.status==CaptureStatus::Ready && (!graph.frames || graph.base.isNull() || graph.playhead>=graph.frames
           || (graph.transport!=State::Playing && graph.transport!=State::Stopped))))return false;
    auto& p=*impl;
    if(graph.status==CaptureStatus::Empty)
    {
        if(graph.frames || graph.layerCount || graph.redoCount || graph.retiredCount || !graph.base.isNull() || graph.playhead || graph.transport!=State::Empty)return false;
        p.clear();p.publish();return true;
    }
    for(int i=0;i<graph.layerCount;++i)if(graph.layers[size_t(i)].isNull())return false;
    for(int i=0;i<graph.redoCount;++i)if(graph.redo[size_t(i)].isNull())return false;
    for(int i=0;i<graph.retiredCount;++i)if(graph.retired[size_t(i)].isNull())return false;
    // Keep the nearest redo entries first, then the newest audible undo
    // entries within the selected depth. Older audible layers remain in the
    // playback sum until the worker folds them, never discarded or truncated.
    const int keepRedo=std::min(graph.redoCount,p.config.undoDepth);
    const int keepActive=std::min(graph.layerCount,p.config.undoDepth-keepRedo);
    if(graph.retiredCount+graph.layerCount+keepRedo>p.count-1)return false;
    p.clear();
    if(!p.streams[0]->restore(graph.base)){p.flags|=LooperStorage::ioFailure;p.publish();return false;}
    int nextId=1;
    const auto load=[&](const AssetHash& hash,int& id)
    {
        id=nextId++;
        if(!p.streams[size_t(id)]->restore(hash) || p.streams[size_t(id)]->length()!=graph.frames)return false;
        p.used[size_t(id)]=true;return true;
    };
    for(int i=0;i<graph.retiredCount;++i)
    {
        int id=0;if(!load(graph.retired[size_t(i)],id)){p.flags|=LooperStorage::ioFailure;p.publish();return false;}
        p.retired[size_t(p.retiredCount++)]=id;
    }
    for(int i=0;i<graph.layerCount;++i)
    {
        int id=0;if(!load(graph.layers[size_t(i)],id)){p.flags|=LooperStorage::ioFailure;p.publish();return false;}
        if(i<graph.layerCount-keepActive)p.retired[size_t(p.retiredCount++)]=id;
        else p.active[size_t(p.activeCount++)]=id;
    }
    for(int i=graph.redoCount-keepRedo;i<graph.redoCount;++i)
    {
        int id=0;if(!load(graph.redo[size_t(i)],id)){p.flags|=LooperStorage::ioFailure;p.publish();return false;}
        p.redo[size_t(p.redoCount++)]=id;
    }
    if(p.streams[0]->length()!=graph.frames){p.flags|=LooperStorage::ioFailure;p.publish();return false;}
    p.frames=graph.frames;p.current=graph.transport;p.head=graph.playhead;p.gain=p.current==State::Playing?1:0;
    // Prime the actual configured window before publishing the replacement.
    // This stopped off-audio wait is bounded across the whole stream pool.
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
    const auto pages=(p.frames+uint64_t(p.config.pageFrames)-1)/uint64_t(p.config.pageFrames);
    for(int id=0;id<p.count;++id)if(p.used[size_t(id)])p.streams[size_t(id)]->prefetch(p.head);
    for(int id=0;id<p.count;++id)if(p.used[size_t(id)])
        for(uint64_t ahead=0;ahead<std::min(pages,uint64_t(p.config.readAhead+1));++ahead)
        {
            const auto frame=((p.head/uint64_t(p.config.pageFrames)+ahead)%pages)*uint64_t(p.config.pageFrames);
            const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
            if(!p.streams[size_t(id)]->waitAvailable(frame,int(std::clamp(int64_t(remaining),int64_t(0),int64_t(1000)))))
            {p.flags|=LooperStorage::ioFailure;p.current=State::Stopped;p.gain=0;p.publish();return false;}
        }
    p.publish();return true;
}
}
