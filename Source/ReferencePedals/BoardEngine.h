// OWNER: WP2
#pragma once
#include <array>
#include <atomic>
#include "BoardModel.h"
#include "TopologyCompiler.h"
#include "Routing/LatencyEqualiser.h"
#include "Routing/SendReturn.h"
#include "Routing/AbSwitch.h"
#include "Routing/Feedback.h"
#include "Rt/RetireList.h"
#include "Rt/SpscRing.h"
#include "WetDryMixer.h"
#include "Midi/MidiTypes.h"
#include "Midi/MidiRouter.h"
namespace luthier::reference
{
struct PedalNodeState
{
    PedalRuntime* runtime = nullptr;
    const SlotControl* control = nullptr;
    const midi::RouteOut* events = nullptr;
    const MidiEvent* raw = nullptr;
    int numRaw = 0, offset = 0;
    const double* sidechain[2] {};
    TimeInfo time;
    bool eventsEnabled = false, processWet = true;
};
    struct EngageEcho
    {
        InstanceId identity = InstanceId::null();
        uint8_t slot = 0;
        bool enabled = true, baseline = true;
    };
    struct ParamPulse
    {
        InstanceId identity = InstanceId::null();
        const char* modelId = nullptr;
        int param = 0;
        float value = 0;
    };
class BoardEngine
{
public:
    explicit BoardEngine (BoardModel&);
    ~BoardEngine();
    void prepare (double sampleRate, int maxBlock);
    void releaseResources(); // stopped-audio barrier
    bool setAssetStore (std::shared_ptr<IAssetStore> store);
    void requestReset() noexcept { resetRequested.store (true, std::memory_order_release); }
    void process (double*, double*, int, Placement) noexcept;
    void setProcessorLoad (double fraction, bool measuredWindow, double blockAudioSeconds) noexcept;
    void setTime (const TimeInfo& time) noexcept { blockTime = time; }
    void setAuxOutputs (const BoardAuxOutputs& outputs) noexcept
    {
        auxOutputs = outputs;
        const auto connected = [] (const BoardAuxOutputs::Stereo& bus) { return bus.left && bus.right && bus.samples > 0; };
        auxAvailable.store ((connected (outputs.pedalB) ? 1 : 0) | (connected (outputs.wet) ? 2 : 0) | (connected (outputs.dry) ? 4 : 0));
    }
    void setSidechain (const float* left, const float* right, int samples) noexcept
    {
        sidechain[0] = left; sidechain[1] = right; sidechainSamples = std::max (0, samples);
        sidechainConnected.store (left && right && samples > 0, std::memory_order_relaxed);
    }
    void setExtendedHostAutomation (const ExtendedHostAutomationSnapshot& snapshot) noexcept { extendedHost = snapshot; }
    void setHostAutomation (const HostAutomationSnapshot& snapshot) noexcept { hostAutomation = snapshot; }
    void syncRouting (midi::MidiRouter&) noexcept; // audio thread, retained model intent
    void beginRawMidi (const juce::MidiBuffer&, int samples) noexcept;
    int drainMidiOut (MidiEvent*, int capacity) noexcept;
    void beginEvents (const midi::RouteOut&) noexcept;
    void endEvents() noexcept { events = nullptr; sidechain[0] = sidechain[1] = nullptr; sidechainSamples = 0; auxOutputs = {}; }
    int reclaim() noexcept;
    void drainControlEcho(); // message thread, outside any BoardModel lock
    bool isActive() const noexcept { return targetActive.load() || running.load(); }
    int latency() const noexcept { return latencySamples.load(); }
    BoardMeters meters() const noexcept;
    PedalMeters readPedalMeters (int slot) const noexcept;
private:
    bool publish (const std::vector<PedalInstance>&, const BoardSettings&, TopologyInfo&, juce::String&, const BoardCommand* intent = nullptr, bool hostEcho = false);
    void acknowledge() noexcept;
    void publishPedalMeters (const RenderPlan&) noexcept;
    struct AtomicPedalMeters
    {
        std::atomic<uint32_t> sequence { 0 }, available { 0 }, faults { 0 };
        std::atomic<uint64_t> identity0 { 0 }, identity1 { 0 }, loopLength { 0 }, playhead { 0 };
        std::atomic<int> transport { 0 };
        std::atomic<bool> present { false }, enabled { false };
        std::atomic<float> tail { -120.f }, progress { 0.f };
        std::atomic<double> bpm { 0.0 };
    };
    std::array<AtomicPedalMeters, kMaxSlots> pedalMeters;
    struct Segment
    {
        const RenderPlan* current = nullptr;
        const RenderPlan* pending = nullptr;
        enum Phase { Idle, Out, In } phase = Idle;
        int ramp = 0;
        double gain = 1.0, rampStart = 1.0, mix = 1.0, enable = 1.0;
        std::array<DelayLine, 2> dry;
        std::array<routing::LatencyEqualiser, kMaxBranches> lanes;
        std::array<routing::SendReturnState, kMaxLoops> loops;
        routing::AbSwitchState ab;
    };
    SpscRing<ParamPulse, 256> paramPulses;
    std::array<ParamPulse, 256> pendingPulses;
    int numPendingPulses = 0;
    SpscRing<EngageEcho, 256> engageEcho;
    BoardModel& model;
    PedalPool pool;
    std::unique_ptr<RenderPlan> latest;
    std::vector<PedalInstance> releasedStored, releasedCaptured;
    RetireList<RenderPlan> retired;
    std::atomic<const RenderPlan*> live { nullptr };
    std::atomic<uint32_t> audioEpoch { 0 };
    std::atomic<bool> targetActive { false }, running { false }, resetRequested { false };
    std::atomic<int> latencySamples { 0 };
    std::atomic<float> inputPeak { -120.f }, outputPeak { -120.f };
    std::array<Segment, kNumPlacements> segments;
    std::array<std::array<std::array<double, kSubBlock>, 2>, kMaxBuses> buses {};
    routing::FeedbackState feedback;
    uint32_t feedbackRearmSeen = 0, feedbackKillSeen = 0;
    BoardAuxOutputs auxOutputs;
    std::atomic<uint8_t> auxAvailable { 0 };
    std::atomic<bool> feedbackFault { false }, feedbackTripped { false };
    const float* sidechain[2] {};
    int sidechainSamples = 0;
    std::atomic<bool> sidechainConnected { false }, assetStoreAvailable { false };
    std::array<std::array<double, kSubBlock>, 2> sidechainScratch {};
    WetDryMixer mixer;
    ExtendedHostAutomationSnapshot extendedHost;
    std::array<float, ExtendedHostAutomationSnapshot::count> extendedValues {};
    std::array<uint32_t, ExtendedHostAutomationSnapshot::count> extendedModelSerials {}, extendedHostSerials {};
    float extended (ExtendedHostField field) const noexcept { return extendedValues[static_cast<size_t> (field)]; }
    void applyExtended (const RenderPlan&) noexcept;
    void prepareEventOffsets (const RenderPlan&, int samples) noexcept;
    void applyCpuRelief(); // message thread only
    HostAutomationSnapshot hostAutomation;
    std::array<uint32_t, 3> hostBoardSerials {}, modelBoardSerials {};
    struct HostSlotSource { InstanceId identity = InstanceId::null(); const char* modelId = nullptr; };
    std::array<HostSlotSource, kMaxSlots> hostSlotSources, routingSlotSources;
    std::array<uint32_t, kMaxSlots * kMaxParams + 3> routingModelSerials {};
    std::array<uint32_t, HostAutomationSnapshot::count> routingHostSerials {};
    uint32_t routingRevision = 0;
    TimeInfo blockTime;
    midi::RouteOut blockEvents;
    bool eventOffsetsPrepared = false;
    const midi::RouteOut* events = nullptr;
    std::array<MidiEvent, 256> rawMidi {}, midiOutput {};
    int numRawMidi = 0, numMidiOutput = 0;
    std::atomic<uint32_t> rawDropped { 0 }, outputDropped { 0 };
    bool effectiveEnabled = false, modelEnabled = false, blockEnabled = false;
    float effectiveMix = 1.f, modelMix = 1.f, blockMix = 1.f;
    double effectiveTempo = 120, modelTempo = 120, blockTempo = 120;
    double rate = 48000, mixAlpha = 0.001;
    int block = 4096, dipSamples = 288;
    uint32_t version = 0;
    double cpuHotSeconds = 0; // audio thread only
    std::atomic<float> processorCpuPct { 0.f };
    std::atomic<bool> processorCpuAvailable { false }, cpuReliefRequested { false }, cpuGovernorTripped { false }, cpuEcoApplied { false }, cpuGovernorFailed { false };
};
}
