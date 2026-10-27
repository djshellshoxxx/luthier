// OWNER: WP15
#pragma once
#include "../Rt/SpscRing.h"
#include <array>
#include <vector>
#include <cstdint>
namespace luthier::reference::common
{
enum class LooperAction { Record, Play, Overdub, Stop, Clear, Undo, Redo, CancelOverdub };
struct LooperCommand { LooperAction action=LooperAction::Stop; int64_t sample=0; };
// Bounded RAM and undo history; the oldest accepted layer folds lazily into
// the base so overdubs remain unlimited. Disk paging is owned by storage.
class LooperCore
{
public:
    enum class State { Empty, Recording, Playing, Overdubbing, Stopped };
    using Action=LooperAction;
    using Command=LooperCommand;
    // Message thread, with audio stopped. Returns false if the requested budget
    // cannot hold one frame. Capacity is published only after this returns.
    bool prepare(double sampleRate,double seconds,int layers,size_t byteBudget);
    void reset() noexcept; // audio owner or stopped audio only; O(1)
    bool enqueue(Command command) noexcept { return commands.push(command); } // one producer, chronological timestamps
    void apply(Action action) noexcept; // audio thread, exact boundary
    void process(double* left,double* right,int n,double loopGain=1,double dryGain=1) noexcept;
    void setSeamMilliseconds(double ms) noexcept;
    State state() const noexcept { return current; } // audio-owner inspection only
    size_t length() const noexcept { return loopLength; }
    size_t capacity() const noexcept { return maxFrames; }
    size_t memoryBytes() const noexcept { return storageBytes; }
    int64_t samplePosition() const noexcept { return timeline; }
    int undoDepth() const noexcept { return activeLayers; }
    // Audio owner only: eligibility now, matching apply(Undo/Redo), not merely
    // retained history while recording/overdubbing/stopped.
    bool canUndo() const noexcept {return current==State::Playing && activeLayers>0;}
    bool canRedo() const noexcept {return current==State::Playing && redoLayers>0;}
    bool layerLimitReached() const noexcept { return layerLimit; }
    uint32_t droppedCommands() const noexcept { return commands.dropped(); }
private:
    struct Stereo { double l=0,r=0; };
    struct LayerSample { Stereo audio; uint32_t generation=0; };
    enum class EpochState : uint8_t { Free, Active, Retired, Discarded };
    // One epoch per distinct sample generation is a sufficient fixed upper
    // bound, plus handles for empty active layers. Refcounts recycle records
    // without allocation, including partially cancelled overdubs.
    struct Epoch { uint32_t references=0,next=0; uint64_t sequence=0; EpochState state=EpochState::Free; };
    std::vector<Epoch> epochs;
    uint32_t freeEpoch=0;
    uint64_t sequence=0,clearedThrough=0;
    uint32_t newEpoch() noexcept;
    void releaseEpoch(uint32_t) noexcept;
    void markEpoch(uint32_t,EpochState) noexcept;
    void discardIndex(size_t) noexcept;
    void foldIndex(size_t) noexcept;
    std::vector<Stereo> base;
    std::array<std::vector<LayerSample>,9> layer;
    std::array<uint32_t,9> generation{};
    SpscRing<Command,64> commands;
    Command pending{};
    bool hasPending=false,layerLimit=false,displacedRedo=false;
    State current=State::Empty;
    size_t maxFrames=0,loopLength=0,head=0,storageBytes=0;
    int maxLayers=1,activeLayers=0,redoLayers=0;
    int64_t timeline=0;
    double sr=48000,gain=0;
    size_t seam=0;
    Stereo playback(size_t index) const noexcept;
    void closeRecording() noexcept;
    void closeOverdub() noexcept;
};
}
