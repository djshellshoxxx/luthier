// OWNER: WP15/WP16
#pragma once
#include "../Services/IAssetStore.h"
#include <memory>
namespace luthier::reference::common
{
// Bounded PCM page streaming. append/read/request are audio-owner calls: they
// neither allocate, lock, wait, nor touch files. The owned worker writes hashed
// pages and the hashed manifest through IAssetStore. State contains one hash.
class LooperStorage
{
public:
    struct Config
    {
        double sampleRate=48000;
        int channels=2,pageFrames=4096,writePages=8,readPages=8,readAhead=3;
        uint64_t maxFrames=48000*300ull;
        size_t ramBudget=32*1024*1024;
        uint64_t storageBudget=4ull*1024*1024*1024;
    };
    enum Error : uint32_t { none=0,writeOverrun=1,readUnderrun=2,ioFailure=4,capacityReached=8,invalidAsset=16 };
    enum class CaptureStatus : uint32_t { Empty, Recording, Pending, Ready, Failed };
    struct Capture
    {
        uint64_t generation=0,frames=0;
        AssetHash hash;
        CaptureStatus status=CaptureStatus::Empty;
        uint32_t faults=0;
    };
    // Any thread. Bounded atomic-only capture; unchanged output on contention.
    // Recording/Pending/Failed never expose an older generation's hash.
    bool capture(Capture&) const noexcept;
    static constexpr size_t workerStackBytes=256*1024;
    // Exact reservation formula including the requested worker stack, page
    // caches, metadata and bounded I/O scratch; no worker/allocation needed.
    static bool reservation(const Config&,size_t& ram,uint64_t& storage) noexcept;
    LooperStorage();
    ~LooperStorage();
    // Stopped audio only. Refuses a budget that cannot represent maxFrames;
    // never silently shortens the configured loop to fit RAM.
    bool prepare(IAssetStore&,const Config&);
    uint64_t capacity() const noexcept;
    size_t reservedBytes() const noexcept;
    uint64_t reservedStorageBytes() const noexcept;
    void beginRecord() noexcept;
    bool append(double left,double right) noexcept;
    // Sparse, wrapping overdub delta with a fixed loop length. Unwritten pages
    // are implicit zero. Existing page data must be resident/read-ahead ready;
    // a miss fails immediately with explicit underrun/overrun flags.
    bool beginLayer(uint64_t frames) noexcept;
    bool add(uint64_t frame,double left,double right) noexcept;
    bool canAdd(uint64_t frame) const noexcept;
    bool finishRecord() noexcept;
    bool read(uint64_t frame,double& left,double& right,uint64_t* version=nullptr,bool allowStale=false) noexcept;
    void prefetch(uint64_t frame) noexcept;
    // Stopped owner off audio only: wait for requested read-ahead, never on
    // the callback. Used before publishing a restored processor.
    bool waitAvailable(uint64_t frame,int timeoutMs=1000) const;
    bool available(uint64_t frame,bool allowStale=false) const noexcept; // audio owner, no fault side effects
    // Audio-owner call. Both loops must be finalized and have ready manifests.
    // Source remains immutable until completion; its lifetime is held internally.
    // Worker merges one page between queued realtime I/O jobs. Zero refuses.
    uint64_t mergeLayer(LooperStorage& immutableLayer) noexcept;
    bool mergeComplete(uint64_t token) const noexcept;
    // Audio owner after removing the retired layer from its playback/state
    // graph. Until acknowledged, capture stays Pending and old cached page
    // revisions may be combined with that layer using mergedAt().
    bool acknowledgeMerge(uint64_t token) noexcept;
    // During playback, omit the retired layer only if the actual returned base
    // page revision already includes this token. Old cached revisions remain
    // usable while their replacement is being read ahead.
    bool mergedAt(uint64_t frame,uint64_t actualVersion,uint64_t token) const noexcept;
    uint64_t generation() const noexcept; // atomic, any thread
    uint64_t length() const noexcept;
    uint32_t errors() const noexcept;
    // Message/worker thread; bounded wait for page writes already queued by
    // finishRecord(). Atomic failure leaves the previous hash unchanged.
    bool manifest(AssetHash&,int timeoutMs=1000) const;
    // Stopped audio only. Validates the external manifest off audio, then starts
    // read-ahead. Audio pages remain external. No absolute paths in the result.
    bool restore(const AssetHash&);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
