// OWNER: WP15/WP16
#include "LooperStorage.h"
#include "../Rt/SpscRing.h"
#include "../Services/AssetStore.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <limits>
namespace luthier::reference::common
{
namespace
{
void integer(std::vector<uint8_t>& out,uint64_t n,int bytes)
{for(int i=0;i<bytes;++i)out.push_back(uint8_t(n>>(8*i)));}
bool number(const std::vector<uint8_t>& in,size_t& at,uint64_t& n,int bytes)
{if(at+size_t(bytes)>in.size())return false;n=0;for(int i=0;i<bytes;++i)n|=uint64_t(in[at++])<<(8*i);return true;}
uint64_t state(uint64_t epoch,uint64_t version) noexcept{return (epoch<<32)|uint64_t(uint32_t(version));}
uint64_t stateEpoch(uint64_t value) noexcept{return value>>32;}
uint64_t stateVersion(uint64_t value) noexcept{return uint32_t(value);}
struct AtomicHash
{
    std::array<std::atomic<uint64_t>,4> words{};
    void store(const AssetHash& hash) noexcept
    {for(size_t i=0;i<4;++i){uint64_t n;std::memcpy(&n,hash.bytes.data()+i*8,8);words[i].store(n,std::memory_order_relaxed);}}
    AssetHash load() const noexcept
    {AssetHash h;for(size_t i=0;i<4;++i){auto n=words[i].load(std::memory_order_relaxed);std::memcpy(h.bytes.data()+i*8,&n,8);}return h;}
};
}
struct LooperStorage::Impl
{
    Config config;IAssetStore& store;
    struct Page
    {
        std::vector<float> pcm;
        std::atomic<uint64_t> tag{0};
        uint64_t epoch=0,index=0,token=0,version=0; // audio owner; worker receives job copies
        int frames=0;
    };
    struct Entry
    {
        AtomicHash hash;
        std::atomic<uint64_t> ready{0},desired{0},mergedToken{0},mergedVersion{0};
        uint64_t residentEpoch=0;int resident=-1; // audio owner only
        int readSlot[2]{-1,-1}; // audio owner; validated against slot epoch/index
    };
    enum class Kind {Write,Read,Manifest,Barrier,Merge};
    struct Job {Kind kind;uint64_t epoch,index,token,frames;int buffer;uint64_t version;Impl* source;};
    std::unique_ptr<Page[]> writes,reads;
    std::unique_ptr<Entry[]> entries;
    size_t maxPages=0,bytesReserved=0;
    uint64_t diskReserved=0;
    SpscRing<Job,64> jobs;
    std::atomic<uint64_t> epoch{1},frames{0},manifestReady{0},barrierReady{0},phase{state(1,0)},hashSequence{0};
    AtomicHash manifestHash;
    std::atomic<uint32_t> flags{0};
    std::atomic<bool> stopping{false},workerExited{false};
    std::atomic<uint64_t> mergeBusy{0},mergeDone{0},mergeGeneration{0};
    std::atomic<uint32_t> borrowers{0};
    Impl* mergeSource=nullptr;uint64_t mergeSourceEpoch=0,mergeEpoch=0,mergeToken=0,mergeIndex=0;
    bool recording=false,sparse=false,finalized=false;int writeBuffer=-1;uint64_t writeIndex=0,nextToken=0;
    std::mutex mutex;std::condition_variable wake,done;
    struct Worker final : juce::Thread
    {
        Impl& owner;
        explicit Worker(Impl& p):juce::Thread("Looper asset pages",workerStackBytes),owner(p){}
        void run() override
        {
            owner.run();
            const std::lock_guard<std::mutex> lock(owner.mutex);
            owner.workerExited.store(true,std::memory_order_release);owner.done.notify_all();
        }
    } worker;
    Impl(IAssetStore& assetStore,Config cfg):config(cfg),store(assetStore),worker(*this)
    {
        maxPages=size_t((config.maxFrames+uint64_t(config.pageFrames)-1)/uint64_t(config.pageFrames));
        entries=std::make_unique<Entry[]>(maxPages);
        writes=std::make_unique<Page[]>(size_t(config.writePages));reads=std::make_unique<Page[]>(size_t(config.readPages)*2);
        for(int i=0;i<config.writePages;++i)writes[i].pcm.resize(size_t(config.pageFrames)*size_t(config.channels));
        for(int i=0;i<config.readPages*2;++i)reads[i].pcm.resize(size_t(config.pageFrames)*size_t(config.channels));
        bytesReserved=sizeof(Impl)+maxPages*sizeof(Entry)+size_t(config.writePages+config.readPages*2)*(sizeof(Page)+size_t(config.pageFrames)*size_t(config.channels)*sizeof(float))+maxPages*32+size_t(config.pageFrames)*size_t(config.channels)*20+65536+workerStackBytes;
        diskReserved=config.maxFrames*uint64_t(config.channels)*4+uint64_t(maxPages)*20+uint64_t(maxPages)*32+40;
        if(!worker.startThread())throw std::runtime_error("Cannot start looper page worker");
    }
    ~Impl()
    {
        stopping=true;wake.notify_one();
        {std::unique_lock<std::mutex> lock(mutex);done.wait(lock,[this]{return workerExited.load(std::memory_order_acquire);});}
        worker.waitForThreadToExit(-1);
        std::unique_lock<std::mutex> lock(mutex);done.wait(lock,[this]{return borrowers.load(std::memory_order_acquire)==0;});
    }
    void releaseBorrower()
    {
        const std::lock_guard<std::mutex> lock(mutex);
        borrowers.fetch_sub(1,std::memory_order_acq_rel);done.notify_all();
    }
    void releaseSource()
    {
        if(!mergeSource)return;
        auto* source=mergeSource;mergeSource=nullptr;
        source->releaseBorrower();
    }
    bool push(Job job) noexcept
    {return jobs.push(job);}
    void run()
    {
        for(;;)
        {
            Job job;
            if(jobs.pop(job)){try{work(job);}catch(...){if(job.epoch==epoch.load())flags.fetch_or(ioFailure);if(job.kind==Kind::Write)writes[job.buffer].tag.store(0,std::memory_order_release);notifyDone();}continue;}
            if(stopping.load()){releaseSource();return;}
            if(mergeSource){try{mergeStep();}catch(...){if(mergeEpoch==epoch.load())flags.fetch_or(ioFailure);mergeBusy=0;releaseSource();notifyDone();}continue;}
            std::unique_lock<std::mutex> lock(mutex);wake.wait_for(lock,std::chrono::milliseconds(1));
        }
    }
    void notifyDone(){ {const std::lock_guard<std::mutex> lock(mutex);} done.notify_all(); }
    std::vector<uint8_t> readAsset(const AssetHash& hash,size_t limit)
    {
        const auto path=store.getBlobPath(hash);if(path==juce::File())return {};
        juce::FileInputStream stream(path);const auto length=stream.getTotalLength();
        if(!stream.openedOk() || length<0 || uint64_t(length)>uint64_t(limit))return {};
        std::vector<uint8_t> bytes(size_t(length),uint8_t(0));
        for(size_t at=0;at<bytes.size();)
        {
            const auto count=int(std::min(size_t(65536),bytes.size()-at));
            if(stream.read(bytes.data()+at,count)!=count)return {};
            at+=size_t(count);
        }
        if(stream.getStatus().failed())return {};
        if(AssetStore::hashBlob(bytes.data(),bytes.size())!=hash)return {};
        return bytes;
    }
    void work(const Job& job)
    {
        if(job.kind==Kind::Merge)
        {
            if(job.epoch!=epoch.load(std::memory_order_acquire) || stopping.load())
            {job.source->releaseBorrower();mergeBusy=0;return;}
            mergeSource=job.source;mergeSourceEpoch=job.version;mergeEpoch=job.epoch;mergeToken=job.token;mergeIndex=0;return;
        }
        if(job.kind==Kind::Barrier){barrierReady.store(job.token,std::memory_order_release);notifyDone();return;}
        if(job.epoch!=epoch.load(std::memory_order_acquire))
        {if(job.kind==Kind::Write)writes[job.buffer].tag.store(0,std::memory_order_release);return;}
        if(job.kind==Kind::Write)
        {
            auto& page=writes[job.buffer];std::vector<uint8_t> bytes;
            bytes.reserve(20+size_t(job.frames)*size_t(config.channels)*4);
            integer(bytes,0x3150434c,4);integer(bytes,uint64_t(config.channels),4);integer(bytes,uint64_t(config.sampleRate),4);integer(bytes,job.frames,8);
            for(size_t i=0;i<size_t(job.frames)*size_t(config.channels);++i){uint32_t bits;std::memcpy(&bits,&page.pcm[i],4);integer(bytes,bits,4);}
            const auto hash=AssetStore::hashBlob(bytes.data(),bytes.size());
            if(store.putBlob(hash,bytes.data(),bytes.size()))
            {entries[job.index].ready.store(0,std::memory_order_release);entries[job.index].hash.store(hash);entries[job.index].ready.store(state(job.epoch,job.version),std::memory_order_release);}
            else if(job.epoch==epoch.load())flags.fetch_or(ioFailure);
            page.tag.store(0,std::memory_order_release);notifyDone();
        }
        else if(job.kind==Kind::Read)
        {
            auto& page=reads[job.buffer];bool ok=entries[job.index].ready.load(std::memory_order_acquire)==state(job.epoch,job.version);
            if(!ok){auto expected=(job.token<<2)|1;page.tag.compare_exchange_strong(expected,0,std::memory_order_release);return;}
            auto bytes=ok ? readAsset(entries[job.index].hash.load(),20+size_t(config.pageFrames)*size_t(config.channels)*4):std::vector<uint8_t>{};
            size_t at=0;uint64_t magic=0,ch=0,rate=0,count=0;
            ok=ok && number(bytes,at,magic,4) && magic==0x3150434c && number(bytes,at,ch,4) && ch==uint64_t(config.channels)
                && number(bytes,at,rate,4) && rate==uint64_t(config.sampleRate) && number(bytes,at,count,8)
                && count>0 && job.index*uint64_t(config.pageFrames)<job.frames
                && count==std::min(uint64_t(config.pageFrames),job.frames-job.index*uint64_t(config.pageFrames)) && at+size_t(count)*size_t(ch)*4==bytes.size();
            if(ok)for(size_t i=0;i<size_t(count)*size_t(ch);++i){uint64_t bits;number(bytes,at,bits,4);uint32_t word=uint32_t(bits);float value;std::memcpy(&value,&word,4);if(!std::isfinite(value)){ok=false;break;}page.pcm[i]=value;}
            if(!ok)if(job.epoch==epoch.load())flags.fetch_or(ioFailure);
            auto expected=(job.token<<2)|1;
            page.tag.compare_exchange_strong(expected,(job.token<<2)|(ok?2:3),std::memory_order_release);
            notifyDone();
        }
        else
        {
            std::vector<uint8_t> bytes;const size_t count=size_t((job.frames+uint64_t(config.pageFrames)-1)/uint64_t(config.pageFrames));
            bytes.reserve(32+count*32);integer(bytes,0x314d434c,4);integer(bytes,1,4);integer(bytes,uint64_t(config.channels),4);
            integer(bytes,uint64_t(config.sampleRate),4);integer(bytes,uint64_t(config.pageFrames),4);integer(bytes,job.frames,8);integer(bytes,uint64_t(count),4);
            for(size_t i=0;i<count;++i)
            {
                AssetHash hash;
                const auto wanted=entries[i].desired.load(std::memory_order_acquire);
                if(stateEpoch(wanted)==job.epoch)
                {
                    if(entries[i].ready.load(std::memory_order_acquire)!=wanted)
                    {if(job.epoch==epoch.load())flags.fetch_or(ioFailure);notifyDone();return;}
                    hash=entries[i].hash.load();
                }
                bytes.insert(bytes.end(),hash.bytes.begin(),hash.bytes.end());
            }
            const auto hash=AssetStore::hashBlob(bytes.data(),bytes.size());
            if(store.putBlob(hash,bytes.data(),bytes.size())){hashSequence.fetch_add(1,std::memory_order_acq_rel);manifestReady.store(0,std::memory_order_release);manifestHash.store(hash);manifestReady.store(job.epoch,std::memory_order_release);hashSequence.fetch_add(1,std::memory_order_release);}
            else if(job.epoch==epoch.load())flags.fetch_or(ioFailure);
            notifyDone();
        }
    }
    bool decodePage(Impl& owner,uint64_t index,uint64_t current,std::vector<float>& samples)
    {
        const auto count=std::min(uint64_t(config.pageFrames),frames.load()-index*uint64_t(config.pageFrames));
        samples.assign(size_t(count)*size_t(config.channels),0.f);
        const auto wanted=owner.entries[index].desired.load(std::memory_order_acquire);
        if(stateEpoch(wanted)!=current)return true;
        if(owner.entries[index].ready.load(std::memory_order_acquire)!=wanted)return false;
        const auto bytes=owner.readAsset(owner.entries[index].hash.load(),20+samples.size()*4);
        size_t at=0;uint64_t magic=0,ch=0,rate=0,n=0;
        if(!number(bytes,at,magic,4) || magic!=0x3150434c || !number(bytes,at,ch,4) || ch!=uint64_t(config.channels)
           || !number(bytes,at,rate,4) || rate!=uint64_t(config.sampleRate) || !number(bytes,at,n,8) || n!=count || at+samples.size()*4!=bytes.size())return false;
        for(auto& sample:samples){uint64_t bits=0;number(bytes,at,bits,4);const auto word=uint32_t(bits);std::memcpy(&sample,&word,4);if(!std::isfinite(sample))return false;}
        return true;
    }
    void mergeStep()
    {
        if(epoch.load(std::memory_order_acquire)!=mergeEpoch || mergeSource->epoch.load(std::memory_order_acquire)!=mergeSourceEpoch)
        {mergeBusy=0;releaseSource();notifyDone();return;}
        const auto count=(frames.load()+uint64_t(config.pageFrames)-1)/uint64_t(config.pageFrames);
        if(mergeIndex==count)
        {
            work({Kind::Manifest,mergeEpoch,0,0,frames.load(),0,0,nullptr});
            const bool ok=manifestReady.load(std::memory_order_acquire)==mergeEpoch && !(flags.load()&ioFailure);
            const auto token=mergeToken;releaseSource();if(ok)mergeDone.store(token,std::memory_order_release);else mergeBusy=0;notifyDone();return;
        }
        std::vector<float> base,layer;
        if(!decodePage(*this,mergeIndex,mergeEpoch,base) || !decodePage(*mergeSource,mergeIndex,mergeSourceEpoch,layer))
        {if(mergeEpoch==epoch.load())flags.fetch_or(ioFailure);mergeBusy=0;releaseSource();notifyDone();return;}
        std::vector<uint8_t> bytes;bytes.reserve(20+base.size()*4);
        integer(bytes,0x3150434c,4);integer(bytes,uint64_t(config.channels),4);integer(bytes,uint64_t(config.sampleRate),4);integer(bytes,base.size()/size_t(config.channels),8);
        for(size_t i=0;i<base.size();++i){const float sample=float(double(base[i])+double(layer[i]));if(!std::isfinite(sample)){if(mergeEpoch==epoch.load())flags.fetch_or(ioFailure);mergeBusy=0;releaseSource();notifyDone();return;}uint32_t word;std::memcpy(&word,&sample,4);integer(bytes,word,4);}
        const auto hash=AssetStore::hashBlob(bytes.data(),bytes.size());
        if(!store.putBlob(hash,bytes.data(),bytes.size())){if(mergeEpoch==epoch.load())flags.fetch_or(ioFailure);mergeBusy=0;releaseSource();notifyDone();return;}
        auto& entry=entries[mergeIndex];auto before=entry.desired.load(std::memory_order_acquire);
        if(epoch.load(std::memory_order_acquire)!=mergeEpoch){mergeBusy=0;releaseSource();notifyDone();return;}
        const auto version=stateEpoch(before)==mergeEpoch?stateVersion(before)+1:1;
        entry.ready=0;entry.hash.store(hash);entry.ready.store(state(mergeEpoch,version),std::memory_order_release);
        entry.mergedVersion.store(version,std::memory_order_relaxed);entry.mergedToken.store(mergeToken,std::memory_order_release);
        entry.desired.compare_exchange_strong(before,state(mergeEpoch,version),std::memory_order_release);
        ++mergeIndex;
    }
    int freeWrite() noexcept
    {
        // While recording, the slot holding the loop's first page is used only as
        // a last resort: it keeps the origin resident so playback can start the
        // moment the recording closes, instead of waiting for a disk read-back.
        int origin=-1;const auto current=epoch.load(std::memory_order_relaxed);
        for(int i=0;i<config.writePages;++i)if(writes[i].tag.load(std::memory_order_acquire)==0)
        {
            if(recording && !sparse && writes[i].epoch==current && writes[i].index==0){origin=i;continue;}
            return i;
        }
        return origin;
    }
    bool flush() noexcept
    {
        if(writeBuffer<0)return true;
        auto& page=writes[writeBuffer];
        page.tag.store(1,std::memory_order_release);
        if(!push({Kind::Write,page.epoch,page.index,0,uint64_t(page.frames),writeBuffer,page.version,nullptr}))
        {page.tag.store(0,std::memory_order_release);flags.fetch_or(writeOverrun);return false;}
        writeBuffer=-1;return true;
    }
    Page* cached(uint64_t pageIndex,uint64_t version,bool stale=false) noexcept
    {
        const auto current=epoch.load(std::memory_order_relaxed);auto& entry=entries[pageIndex];Page* found=nullptr;
        const auto token=mergeBusy.load(std::memory_order_acquire);
        stale=stale && token && entry.mergedToken.load(std::memory_order_acquire)==token && entry.mergedVersion.load(std::memory_order_acquire)==version;
        if(entry.residentEpoch==current && entry.resident>=0)
        {
            auto& page=writes[entry.resident];
            if(page.epoch==current && page.index==pageIndex && (page.version==version || (stale && page.version+1==version)))found=&page;
        }
        for(int way=0;way<2;++way)
        {
            const auto slot=entry.readSlot[way];if(slot<0)continue;
            auto& page=reads[slot];
            if(page.epoch==current && page.index==pageIndex && (page.version==version || (stale && page.version+1==version))
                && page.tag.load(std::memory_order_acquire)==((page.token<<2)|2) && (!found || page.version>found->version))found=&page;
        }
        return found;
    }
    void request(uint64_t pageIndex,uint64_t windowStart,uint64_t pageCount,Direction direction) noexcept
    {
        if(pageIndex>=maxPages)return;
        const auto current=epoch.load(std::memory_order_relaxed);auto& entry=entries[pageIndex];
        const auto wanted=entry.desired.load(std::memory_order_acquire);
        if(stateEpoch(wanted)!=current)return;
        const auto version=stateVersion(wanted);
        const auto way=int(version&1);auto slot=entry.readSlot[way];
        if(slot>=0)
        {
            const auto& page=reads[slot];
            if(page.epoch!=current || page.index!=pageIndex)slot=-1;
            else if(page.version==version && (page.tag.load(std::memory_order_acquire)&3)!=0)return;
        }
        if(entry.ready.load(std::memory_order_acquire)!=wanted)return;
        if(slot<0)
        {
            // Assign only on a miss, never during the per-sample cache lookup.
            // A ragged loop can wrap two same-version pages into one modulo
            // bucket. Protect the whole requested circular window instead.
            // Each parity lane has readPages slots, more than readAhead; the
            // other lane keeps the previous revision available for compaction.
            for(int i=way;i<config.readPages*2;i+=2)
            {
                const auto& candidate=reads[i];
                if(candidate.epoch!=current || candidate.tag.load(std::memory_order_acquire)==0)
                {slot=i;break;}
                const auto distance=direction==Direction::Reverse
                    ? (windowStart+pageCount-candidate.index)%pageCount
                    : (candidate.index+pageCount-windowStart)%pageCount;
                if(slot<0 && distance>uint64_t(config.readAhead))slot=i;
            }
            if(slot<0)return;
        }
        auto& page=reads[slot];entry.readSlot[way]=slot;
        page.epoch=current;page.index=pageIndex;page.token=++nextToken;page.version=version;
        page.frames=int(std::min(uint64_t(config.pageFrames),frames.load()-pageIndex*uint64_t(config.pageFrames)));
        page.tag.store((page.token<<2)|1,std::memory_order_release);
        if(!push({Kind::Read,current,pageIndex,page.token,frames.load(),int(slot),version,nullptr}))page.tag.store(0,std::memory_order_release);
    }
};
LooperStorage::LooperStorage()=default;LooperStorage::~LooperStorage()=default;
bool LooperStorage::reservation(const Config& config,size_t& ramOut,uint64_t& diskOut) noexcept
{
    if(!std::isfinite(config.sampleRate) || config.sampleRate<1 || config.sampleRate>192000 || std::floor(config.sampleRate)!=config.sampleRate
        || config.channels<1 || config.channels>2 || config.pageFrames<16 || config.pageFrames>65536 || config.writePages<2 || config.writePages>24
        || config.readPages<2 || config.readPages>24 || config.readAhead<0 || config.readAhead>=config.readPages || !config.maxFrames
        || config.maxFrames>uint64_t(192000)*5400)return false;
    const auto pages=(config.maxFrames+uint64_t(config.pageFrames)-1)/uint64_t(config.pageFrames);
    const auto ram=sizeof(Impl)+pages*sizeof(Impl::Entry)+uint64_t(config.writePages+config.readPages*2)*(sizeof(Impl::Page)+uint64_t(config.pageFrames)*uint64_t(config.channels)*4)+pages*32+uint64_t(config.pageFrames)*uint64_t(config.channels)*20+65536+workerStackBytes;
    const auto disk=config.maxFrames*uint64_t(config.channels)*4+pages*52+40;
    if(ram>std::numeric_limits<size_t>::max())return false;
    ramOut=size_t(ram);diskOut=disk;return true;
}
bool LooperStorage::prepare(IAssetStore& store,const Config& config)
{
    size_t ram=0;uint64_t disk=0;
    if(!reservation(config,ram,disk) || ram>config.ramBudget || disk>config.storageBudget)return false;
    try{auto next=std::make_unique<Impl>(store,config);impl=std::move(next);return true;}catch(const std::exception&){return false;}
}
uint64_t LooperStorage::capacity() const noexcept{return impl?impl->config.maxFrames:0;}
size_t LooperStorage::reservedBytes() const noexcept{return impl?impl->bytesReserved:0;}
uint64_t LooperStorage::reservedStorageBytes() const noexcept{return impl?impl->diskReserved:0;}
void LooperStorage::beginRecord() noexcept
{
    if(!impl)return;
    if(impl->borrowers.load(std::memory_order_acquire)){impl->flags.fetch_or(writeOverrun);return;}
    const auto completed=impl->mergeDone.load(std::memory_order_acquire);
    auto busy=impl->mergeBusy.load(std::memory_order_acquire);if(busy && busy==completed)impl->mergeBusy.compare_exchange_strong(busy,0,std::memory_order_acq_rel);
    ++impl->epoch;impl->frames=0;impl->flags=0;impl->recording=true;impl->sparse=false;impl->finalized=false;impl->writeBuffer=-1;impl->writeIndex=0;impl->phase.store(state(impl->epoch.load(),1),std::memory_order_release);
}
bool LooperStorage::append(double left,double right) noexcept
{
    if(!impl || !impl->recording || impl->sparse)return false;
    auto& p=*impl;auto n=p.frames.load(std::memory_order_relaxed);
    if(n>=p.config.maxFrames){p.flags.fetch_or(capacityReached);finishRecord();return false;}
    if(p.flags.load()&ioFailure){p.recording=false;return false;}
    if(p.writeBuffer<0)
    {
        p.writeBuffer=p.freeWrite();
        if(p.writeBuffer<0){p.flags.fetch_or(writeOverrun);finishRecord();return false;}
        auto& page=p.writes[p.writeBuffer];page.epoch=p.epoch.load();page.index=n/uint64_t(p.config.pageFrames);page.frames=0;page.version=1;p.entries[page.index].desired.store(state(page.epoch,1),std::memory_order_release);
        p.entries[page.index].residentEpoch=page.epoch;p.entries[page.index].resident=p.writeBuffer;
    }
    auto& page=p.writes[p.writeBuffer];const size_t at=size_t(page.frames)*size_t(p.config.channels);
    page.pcm[at]=std::isfinite(left)?float(std::clamp(left,-16.,16.)):0;
    if(p.config.channels==2)page.pcm[at+1]=std::isfinite(right)?float(std::clamp(right,-16.,16.)):0;
    ++page.frames;p.frames.store(n+1,std::memory_order_release);
    if(page.frames==p.config.pageFrames && !p.flush()){p.recording=false;return false;}
    return true;
}
bool LooperStorage::beginLayer(uint64_t frames) noexcept
{
    if(!impl || !frames || frames>impl->config.maxFrames)return false;
    if(impl->borrowers.load(std::memory_order_acquire))return false;
    beginRecord();impl->sparse=true;impl->frames=frames;return true;
}
bool LooperStorage::canAdd(uint64_t frame) const noexcept
{
    if(!impl || !impl->recording || !impl->sparse || frame>=impl->frames.load() || (impl->flags.load()&ioFailure))return false;
    auto& p=*impl;const auto index=frame/uint64_t(p.config.pageFrames);
    if(p.writeBuffer>=0 && p.writes[p.writeBuffer].index==index)return true;
    if(p.jobs.sizeApprox()>=p.jobs.capacity())return false;
    bool free=false;for(int i=0;i<p.config.writePages;++i)if(i!=p.writeBuffer && p.writes[i].tag.load(std::memory_order_acquire)==0){free=true;break;}
    if(!free)return false;
    auto& entry=p.entries[index];const auto wanted=entry.desired.load();return stateEpoch(wanted)!=p.epoch.load() || p.cached(index,stateVersion(wanted))!=nullptr;
}
bool LooperStorage::add(uint64_t frame,double left,double right) noexcept
{
    if(!impl || !impl->recording || !impl->sparse || frame>=impl->frames.load())return false;
    auto& p=*impl;const auto index=frame/uint64_t(p.config.pageFrames),offset=frame%uint64_t(p.config.pageFrames),current=p.epoch.load();
    if(p.flags.load()&ioFailure){finishRecord();return false;}
    if(p.writeBuffer>=0 && p.writes[p.writeBuffer].index!=index && !p.flush()){p.recording=false;return false;}
    if(p.writeBuffer<0)
    {
        auto& entry=p.entries[index];const auto wanted=entry.desired.load();const bool existing=stateEpoch(wanted)==current;const auto version=existing?stateVersion(wanted):0;
        const auto* source=existing?p.cached(index,version):nullptr;
        if(existing && !source){p.flags.fetch_or(readUnderrun|writeOverrun);prefetch(frame);finishRecord();return false;}
        const auto buffer=p.freeWrite();
        if(buffer<0){p.flags.fetch_or(writeOverrun);finishRecord();return false;}
        auto& page=p.writes[buffer];const auto count=int(std::min(uint64_t(p.config.pageFrames),p.frames.load()-index*uint64_t(p.config.pageFrames)));
        if(source && source!=&page)std::copy_n(source->pcm.data(),size_t(count)*size_t(p.config.channels),page.pcm.data());
        else if(!source)std::fill_n(page.pcm.data(),size_t(count)*size_t(p.config.channels),0.f);
        page.epoch=current;page.index=index;page.frames=count;page.version=version+1;p.writeBuffer=buffer;
        entry.desired.store(state(current,page.version),std::memory_order_release);entry.residentEpoch=current;entry.resident=buffer;
    }
    auto& page=p.writes[p.writeBuffer];const auto at=size_t(offset)*size_t(p.config.channels);
    const auto safe=[](double n){return std::isfinite(n)?float(std::clamp(n,-16.,16.)):0.f;};
    page.pcm[at]+=safe(left);if(p.config.channels==2)page.pcm[at+1]+=safe(right);
    prefetch(frame);return true;
}
bool LooperStorage::finishRecord() noexcept
{
    if(!impl || impl->finalized)return false;
    auto& p=*impl;p.recording=false;
    if(!p.flush())return false;
    if(!p.frames.load()){p.finalized=true;p.phase.store(state(p.epoch.load(),0),std::memory_order_release);return true;}
    if(!p.push({Impl::Kind::Manifest,p.epoch.load(),0,0,p.frames.load(),0,0,nullptr})){p.flags.fetch_or(writeOverrun);return false;}
    p.finalized=true;p.phase.store(state(p.epoch.load(),2),std::memory_order_release);return true;
}
void LooperStorage::prefetch(uint64_t frame,Direction direction) noexcept
{
    if(!impl || !impl->frames.load())return;
    auto& p=*impl;const auto count=(p.frames.load()+uint64_t(p.config.pageFrames)-1)/uint64_t(p.config.pageFrames);
    auto index=(frame%p.frames.load())/uint64_t(p.config.pageFrames);
    for(int i=0;i<=p.config.readAhead;++i)
    {
        const auto page=direction==Direction::Reverse
            ? (index+count-uint64_t(i)%count)%count : (index+uint64_t(i))%count;
        p.request(page,index,count,direction);
    }
}
bool LooperStorage::available(uint64_t frame,bool allowStale) const noexcept
{
    if(!impl || frame>=impl->frames.load())return false;
    auto& p=*impl;const auto index=frame/uint64_t(p.config.pageFrames);const auto wanted=p.entries[index].desired.load(std::memory_order_acquire);
    return stateEpoch(wanted)!=p.epoch.load() || p.cached(index,stateVersion(wanted),allowStale)!=nullptr;
}
bool LooperStorage::read(uint64_t frame,double& left,double& right,uint64_t* actualVersion,bool allowStale,Direction direction) noexcept
{
    left=right=0;if(actualVersion)*actualVersion=0;if(!impl || frame>=impl->frames.load())return false;
    auto& p=*impl;const auto epoch=p.epoch.load();const auto index=frame/uint64_t(p.config.pageFrames),offset=frame%uint64_t(p.config.pageFrames);
    const bool pageEntry=direction==Direction::Reverse
        ? offset+1==std::min(uint64_t(p.config.pageFrames),p.frames.load()-index*uint64_t(p.config.pageFrames)) : offset==0;
    auto& entry=p.entries[index];
    const auto desired=entry.desired.load(std::memory_order_acquire);
    if(stateEpoch(desired)!=epoch){if(pageEntry)prefetch(frame,direction);return true;}
    const auto wanted=stateVersion(desired);
    if(const auto* page=p.cached(index,wanted,allowStale))
    {
        if(offset<uint64_t(page->frames))
        {
            left=page->pcm[size_t(offset)*size_t(p.config.channels)];right=p.config.channels==2?page->pcm[size_t(offset)*2+1]:left;
            if(actualVersion)*actualVersion=page->version;
            if(pageEntry || page->version!=wanted)prefetch(frame,direction);
            return true;
        }
    }
    p.flags.fetch_or(readUnderrun);prefetch(frame,direction);return false;
}
uint64_t LooperStorage::mergeLayer(LooperStorage& layer) noexcept
{
    if(!impl || !layer.impl || impl.get()==layer.impl.get())return 0;
    auto& p=*impl;auto& source=*layer.impl;Capture base,delta;
    if(!capture(base) || !layer.capture(delta) || base.status!=CaptureStatus::Ready || delta.status!=CaptureStatus::Ready
       || base.frames!=delta.frames || p.config.channels!=source.config.channels || p.config.sampleRate!=source.config.sampleRate || p.config.pageFrames!=source.config.pageFrames
       || p.mergeBusy.load() || p.borrowers.load())return 0;
    const auto token=++p.nextToken;source.borrowers.fetch_add(1,std::memory_order_acq_rel);p.mergeGeneration.store(base.generation,std::memory_order_relaxed);p.mergeBusy.store(token,std::memory_order_release);
    if(!p.push({Impl::Kind::Merge,base.generation,0,token,base.frames,0,delta.generation,&source}))
    {p.mergeBusy=0;source.borrowers.fetch_sub(1,std::memory_order_acq_rel);return 0;}
    return token;
}
bool LooperStorage::mergeComplete(uint64_t token) const noexcept
{return impl && token && impl->mergeGeneration.load(std::memory_order_acquire)==impl->epoch.load(std::memory_order_acquire) && impl->mergeDone.load(std::memory_order_acquire)==token;}
bool LooperStorage::acknowledgeMerge(uint64_t token) noexcept
{
    if(!mergeComplete(token))return false;
    auto expected=token;return impl->mergeBusy.compare_exchange_strong(expected,0,std::memory_order_acq_rel);
}
bool LooperStorage::mergedAt(uint64_t frame,uint64_t actualVersion,uint64_t token) const noexcept
{
    if(!impl || !token || frame>=impl->frames.load() || impl->mergeGeneration.load()!=impl->epoch.load())return false;
    const auto& entry=impl->entries[frame/uint64_t(impl->config.pageFrames)];
    return entry.mergedToken.load(std::memory_order_acquire)==token && actualVersion>=entry.mergedVersion.load(std::memory_order_relaxed);
}
uint64_t LooperStorage::generation() const noexcept{return impl?impl->epoch.load(std::memory_order_acquire):0;}
uint64_t LooperStorage::length() const noexcept{return impl?impl->frames.load():0;}
uint32_t LooperStorage::errors() const noexcept{return impl?impl->flags.load():invalidAsset;}
bool LooperStorage::capture(Capture& out) const noexcept
{
    if(!impl)return false;
    auto& p=*impl;
    for(int attempt=0;attempt<8;++attempt)
    {
        const auto epoch=p.epoch.load(std::memory_order_acquire),phase=p.phase.load(std::memory_order_acquire);
        if(stateEpoch(phase)!=epoch)continue;
        const auto sequence=p.hashSequence.load(std::memory_order_acquire);if(sequence&1)continue;
        Capture next;next.generation=epoch;next.frames=p.frames.load(std::memory_order_acquire);next.faults=p.flags.load(std::memory_order_acquire);
        const auto ready=p.manifestReady.load(std::memory_order_acquire);
        if(next.faults&ioFailure)next.status=CaptureStatus::Failed;
        else if(stateVersion(phase)==1)next.status=CaptureStatus::Recording;
        else if(!next.frames)next.status=CaptureStatus::Empty;
        else if(ready==epoch && !p.mergeBusy.load(std::memory_order_acquire)){next.status=CaptureStatus::Ready;next.hash=p.manifestHash.load();}
        else next.status=CaptureStatus::Pending;
        std::atomic_thread_fence(std::memory_order_acquire);
        if(p.hashSequence.load(std::memory_order_relaxed)!=sequence || p.epoch.load(std::memory_order_acquire)!=epoch
           || p.phase.load(std::memory_order_acquire)!=phase || p.frames.load(std::memory_order_acquire)!=next.frames)continue;
        out=next;return true;
    }
    return false;
}
bool LooperStorage::manifest(AssetHash& hash,int timeoutMs) const
{
    if(!impl)return false;
    auto& p=*impl;const auto epoch=p.epoch.load();
    std::unique_lock<std::mutex> lock(p.mutex);
    p.done.wait_for(lock,std::chrono::milliseconds(std::clamp(timeoutMs,0,5000)),[&]{return (p.manifestReady.load()==epoch && !p.mergeBusy.load()) || (p.flags.load()&ioFailure)!=0;});
    Capture next;if(!capture(next) || next.generation!=epoch || next.status!=CaptureStatus::Ready)return false;hash=next.hash;return true;
}
bool LooperStorage::restore(const AssetHash& hash)
{
    if(!impl || impl->borrowers.load(std::memory_order_acquire) || impl->mergeBusy.load(std::memory_order_acquire))return false;
    auto& p=*impl;const auto bytes=p.readAsset(hash,32+p.maxPages*32);size_t at=0;
    uint64_t magic=0,version=0,channels=0,rate=0,pageFrames=0,frames=0,count=0;
    if(!number(bytes,at,magic,4) || magic!=0x314d434c || !number(bytes,at,version,4) || version!=1
        || !number(bytes,at,channels,4) || channels!=uint64_t(p.config.channels) || !number(bytes,at,rate,4) || rate!=uint64_t(p.config.sampleRate)
        || !number(bytes,at,pageFrames,4) || pageFrames!=uint64_t(p.config.pageFrames) || !number(bytes,at,frames,8) || !frames || frames>p.config.maxFrames
        || !number(bytes,at,count,4) || count!=(frames+pageFrames-1)/pageFrames || at+size_t(count)*32!=bytes.size())return false;
    // The producer transfers from stopped audio to this message-thread call.
    // Drain old jobs before reusing metadata/caches; a timeout changes no loop.
    const auto token=++p.nextToken;
    if(!p.push({Impl::Kind::Barrier,p.epoch.load(),0,token,0,0,0,nullptr}))return false;
    {std::unique_lock<std::mutex> lock(p.mutex);if(!p.done.wait_for(lock,std::chrono::milliseconds(1000),[&]{return p.barrierReady.load(std::memory_order_acquire)==token;}))return false;}
    const auto epoch=++p.epoch;
    for(int i=0;i<p.config.readPages*2;++i)p.reads[i].tag=0;
    for(size_t i=0;i<size_t(count);++i){AssetHash h;std::memcpy(h.bytes.data(),bytes.data()+at,32);at+=32;p.entries[i].ready=0;p.entries[i].hash.store(h);const auto wanted=h.isNull()?0:state(epoch,1);p.entries[i].desired=wanted;p.entries[i].ready.store(wanted,std::memory_order_release);p.entries[i].residentEpoch=0;}
    p.frames=frames;p.recording=false;p.sparse=false;p.finalized=true;p.writeBuffer=-1;p.flags=0;p.hashSequence.fetch_add(1,std::memory_order_acq_rel);p.manifestReady=0;p.manifestHash.store(hash);p.manifestReady.store(epoch,std::memory_order_release);p.hashSequence.fetch_add(1,std::memory_order_release);p.phase.store(state(epoch,2),std::memory_order_release);
    prefetch(0);return true;
}
}
