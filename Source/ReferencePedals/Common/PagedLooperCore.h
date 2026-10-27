// OWNER: WP15/WP16
#pragma once
#include "LooperCore.h"
#include "LooperStorage.h"
#include <array>
#include <memory>
namespace luthier::reference::common
{
// Sample-clock transport over bounded, content-addressed PCM streams. All
// storage calls go through the IAssetStore provided by the owning pedal.
class PagedLooperCore
{
public:
    using State=LooperCore::State;
    using Action=LooperAction;
    using Command=LooperCommand;
    struct Config
    {
        double sampleRate=48000,seconds=300;
        int channels=2,undoDepth=1;
        // Aggregate across base, retained layers, scratch/retired streams,
        // caches, page metadata, queues and worker I/O scratch.
        size_t ramBudget=32*1024*1024;
        uint64_t storageBudget=32ull*1024*1024*1024;
        int pageFrames=4096,writePages=8,readPages=8,readAhead=3;
    };
    using CaptureStatus=LooperStorage::CaptureStatus;
    struct Graph
    {
        uint64_t generation=0,frames=0;
        CaptureStatus status=CaptureStatus::Empty;
        uint32_t faults=0;
        AssetHash base;
        std::array<AssetHash,8> layers{};
        int layerCount=0;
        std::array<AssetHash,2> retired{}; // audible, already outside undo depth
        int retiredCount=0;
    };
    PagedLooperCore();
    ~PagedLooperCore();
    bool prepare(IAssetStore&,const Config&); // stopped audio, off audio
    void reset() noexcept;
    bool enqueue(Command) noexcept; // one chronological producer
    void apply(Action) noexcept; // audio owner at exact boundary
    void process(double* left,double* right,int n,double loopGain=1,double dryGain=1) noexcept;
    void setSeamMilliseconds(double) noexcept;
    State state() const noexcept; // remaining getters are audio owner only
    uint64_t length() const noexcept;
    uint64_t playhead() const noexcept;
    uint64_t capacity() const noexcept;
    int64_t samplePosition() const noexcept;
    int undoDepth() const noexcept;
    bool canUndo() const noexcept; // audio owner only; current action eligibility
    bool canRedo() const noexcept;
    uint32_t faults() const noexcept;
    bool playbackReady() const noexcept; // audio owner; whole configured read-ahead window
    size_t reservedBytes() const noexcept;
    uint64_t reservedStorageBytes() const noexcept;
    // Any thread while this prepared processor remains alive; lifecycle calls
    // prepare/restore/destruction require the documented stopped owner barrier.
    // Fixed-size atomic publication, no live audio-buffer reads.
    // Ready resolves worker completion even without another audio callback.
    // Only Ready describes complete assets. Other states never relabel an old
    // manifest as current audio. Output is unchanged on contention.
    bool capture(Graph&) const noexcept;
    // Stopped off-audio lifecycle, normally before publishing a new processor.
    bool restore(const Graph&);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
