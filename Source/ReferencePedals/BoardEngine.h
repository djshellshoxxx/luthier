// OWNER: WP2
#pragma once
#include <array>
#include <atomic>
#include "BoardModel.h"
#include "TopologyCompiler.h"
#include "Routing/LatencyEqualiser.h"
#include "Rt/RetireList.h"
#include "Rt/SpscRing.h"
#include "WetDryMixer.h"
#include "Midi/MidiTypes.h"
namespace luthier::reference
{
struct PedalNodeState
{
    PedalRuntime* runtime = nullptr;
    const SlotControl* control = nullptr;
};
    struct EngageEcho
    {
        InstanceId identity = InstanceId::null();
        uint8_t slot = 0;
        bool enabled = true, baseline = true;
    };
class BoardEngine
{
public:
    explicit BoardEngine (BoardModel&);
    ~BoardEngine();
    void prepare (double sampleRate, int maxBlock);
    void releaseResources(); // stopped-audio barrier
    void requestReset() noexcept { resetRequested.store (true, std::memory_order_release); }
    void process (double*, double*, int, Placement) noexcept;
    void setTime (const TimeInfo& time) noexcept { blockTime = time; }
    void setHostAutomation (const HostAutomationSnapshot& snapshot) noexcept { hostAutomation = snapshot; }
    void beginEvents (const midi::RouteOut&) noexcept;
    void endEvents() noexcept { events = nullptr; }
    int reclaim() noexcept;
    void drainControlEcho(); // message thread, outside any BoardModel lock
    bool isActive() const noexcept { return targetActive.load() || running.load(); }
    int latency() const noexcept { return latencySamples.load(); }
    BoardMeters meters() const noexcept;
private:
    bool publish (const std::vector<PedalInstance>&, const BoardSettings&, TopologyInfo&, juce::String&);
    void acknowledge() noexcept;
    struct Segment
    {
        const RenderPlan* current = nullptr;
        const RenderPlan* pending = nullptr;
        enum Phase { Idle, Out, In } phase = Idle;
        int ramp = 0;
        double gain = 1.0, rampStart = 1.0, mix = 1.0, enable = 1.0;
        std::array<DelayLine, 2> dry;
        std::array<routing::LatencyEqualiser, kMaxBranches> lanes;
    };
    SpscRing<EngageEcho, 256> engageEcho;
    BoardModel& model;
    PedalPool pool;
    std::unique_ptr<RenderPlan> latest;
    RetireList<RenderPlan> retired;
    std::atomic<const RenderPlan*> live { nullptr };
    std::atomic<uint32_t> audioEpoch { 0 };
    std::atomic<bool> targetActive { false }, running { false }, resetRequested { false };
    std::atomic<int> latencySamples { 0 };
    std::atomic<float> inputPeak { -120.f }, outputPeak { -120.f };
    std::array<Segment, kNumPlacements> segments;
    std::array<std::array<std::array<double, kSubBlock>, 2>, kMaxBuses> buses {};
    WetDryMixer mixer;
    HostAutomationSnapshot hostAutomation;
    std::array<uint32_t, 3> hostBoardSerials {};
    struct HostSlotSource { InstanceId identity = InstanceId::null(); const char* modelId = nullptr; };
    std::array<HostSlotSource, kMaxSlots> hostSlotSources;
    TimeInfo blockTime;
    const midi::RouteOut* events = nullptr;
    bool effectiveEnabled = false, modelEnabled = false, blockEnabled = false;
    float effectiveMix = 1.f, modelMix = 1.f, blockMix = 1.f;
    double effectiveTempo = 120, modelTempo = 120, blockTempo = 120;
    double rate = 48000, mixAlpha = 0.001;
    int block = 4096, dipSamples = 288;
    uint32_t version = 0;
};
}
