// OWNER: WP16
#pragma once
#include "../PedalBase.h"
#include "../Common/PagedLooperCore.h"
#include "../Common/LooperStateCodec.h"
#include "../Common/Rc5PressCycle.h"
#include "../Common/Rc5Rhythm.h"
#include "../Common/OnePole.h"
#include <array>

namespace luthier::reference::models
{
// RC-5-style stereo looper wrapper (specs/pedals/boss-rc5.md). One paged stereo
// track (1.5 h, undo depth 1) on the board-provided asset store, the original
// synthesized rhythm, and the main-pedal tap/double/hold grammar. Without an
// asset store the looper is unavailable: dry and rhythm still run and the
// fault bit noAssetStore is reported; gestures are ignored.
class BossRc5 final : public PedalBase<23>
{
public:
    enum Param
    {
        LoopLevel, RhythmLevel, RhythmOn, Tempo, Pattern, Variation, Kit, Memory, Reverse,
        Pedal1, Pedal2, Pedal3, SyncClock, Quantize, ExpTarget, Exp, HoldMs, DtapMs, Lofi,
        DryMute, HalfSpeed, RamCapMb, ClockOut
    };
    using State=common::PagedLooperCore::State;
    static constexpr uint32_t noAssetStore=1u<<31; // above LooperStorage::Error bits

    BossRc5();
    static const PedalDesc& descriptor() noexcept;
    static PreparedMemoryRequirement prepareMemory(double,int,int,const float*) noexcept;
    // Paged media is budgeted by the core's own RAM/storage limits (PreparedMemory.h).
    uint64_t getPreparedMemoryBytes() const noexcept override {return sizeof(*this);}
    void setServices(const PedalServices& s) override {services=s;}
    void prepare(double,int,int) override;
    void reset() noexcept override;
    void process(double*,double*,int) noexcept override;
    void setTime(const TimeInfo& t) noexcept override {time=t;}
    void onFoot(const FootEvent&) noexcept override;
    bool footControlsEngagement() const noexcept override {return false;}
    PedalUiMeters readUiMeters() const noexcept override;
    void setExpression(int,float) noexcept override;
    void saveExtra(juce::var&) const override;
    void loadExtra(const juce::var&) override;

    // Audio-owner inspection (tests, UI meters).
    State transportState() const noexcept {return looperReady ? core.state() : State::Empty;}
    bool looperAvailable() const noexcept {return looperReady;}
    uint64_t loopLength() const noexcept {return looperReady ? core.length() : 0;}
    bool playbackReady() const noexcept {return looperReady && core.playbackReady();}
    double rhythmTempo() const noexcept {return tempo;}
    bool captureLoop(common::PagedLooperCore::Graph& g) const noexcept {return looperReady && core.capture(g);}

private:
    void onParamChanged(int,float) noexcept override;
    void mainDown() noexcept;
    void mainUp() noexcept;
    void fireHold() noexcept;
    void undoOrRedo() noexcept;
    void updateRhythm() noexcept;
    bool restoreStored() noexcept;
    int64_t holdSamples() const noexcept {return int64_t(std::llround(sr*value(HoldMs)*.001));}

    PedalServices services;
    common::PagedLooperCore core;
    common::PagedLooperCore::Config config;
    common::LooperStateCodec::Stored stored; // last loaded/preserved loop (message thread)
    bool storedPending=false;                 // stored loop not yet in the core
    common::Rc5PressCycle cycle;
    common::Rc5Rhythm rhythm;
    common::OnePole lofiHp[2],lofiLp[2];
    TimeInfo time;
    double sr=48000,tempo=120,dryGain=1;
    int64_t samples=0,holdStart=0;
    State holdOrigin=State::Empty;
    bool looperReady=false,prepared=false,mainHeld=false,holdArmed=false,provisionalCut=false;
    std::array<bool,3> footDown{};
    int rhythmPattern=-1,rhythmVariation=-1,rhythmKit=-1;
};
}
