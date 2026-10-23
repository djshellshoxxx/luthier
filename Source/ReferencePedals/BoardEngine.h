// OWNER: WP2
#pragma once
#include <array>
#include <atomic>
#include "BoardModel.h"
#include "TopologyCompiler.h"
#include "Routing/LatencyEqualiser.h"
#include "Rt/RetireList.h"
#include "WetDryMixer.h"
namespace luthier::reference
{
struct PedalNodeState
{
    PedalRuntime* runtime = nullptr;
    const SlotControl* control = nullptr;
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
    int reclaim() noexcept;
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
    TimeInfo blockTime;
    double rate = 48000, mixAlpha = 0.001;
    int block = 4096, dipSamples = 288;
    uint32_t version = 0;
};
}
