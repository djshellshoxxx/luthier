// OWNER: WP23
#include "TestFramework.h"
#include "../PluginProcessor.h"
#include "../ReferencePedals/BoardModel.h"
#include "../ReferencePedals/BoardState.h"
using namespace luthier;
using namespace luthier::reference;
LUTHIER_TEST (RefPedalHost, appendOnlyLaneIdsAndDefaults)
{
    auto p = std::make_unique<LuthierAudioProcessor>();
    const auto& parameters = p->getParameters();
    CHECK (parameters.size() >= 323);
    CHECK (dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[parameters.size() - 323])->paramID == "ref_board_enabled");
    CHECK (dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters.getLast())->paramID == "ref_s16_pos");
    CHECK (! p->getEngine().getReferencePedals().isBoardEnabled());
    for (int slot = 0; slot < kMaxSlots; ++slot)
    {
        CHECK (p->getState().getParameter (ParamIDs::refSlot (slot, "enabled")) != nullptr);
        CHECK (p->getState().getParameter (ParamIDs::refSlot (slot, "p16")) != nullptr);
        CHECK (p->getState().getParameter (ParamIDs::refSlot (slot, "pos")) != nullptr);
    }
}
LUTHIER_TEST (RefPedalHost, boardStateSurvivesHostReloadAndRateChange)
{
    auto source = std::make_unique<LuthierAudioProcessor>();
    auto& board = source->getEngine().getReferencePedals();
    CHECK (board.model().apply (cmd::SetMix { .37f }));
    CHECK (board.model().apply (cmd::SetBoardParam { "tempoBpm", 143. }));
    juce::MemoryBlock state; source->getStateInformation (state);
    auto target = std::make_unique<LuthierAudioProcessor>();
    target->setStateInformation (state.getData(), (int) state.getSize());
    target->prepareToPlay (96000, 64);
    const auto settings = target->getEngine().getReferencePedals().model().getBoardSettings();
    CHECK_NEAR (settings.mix, .37f, 1e-6); CHECK_NEAR (settings.tempoBpm, 143., 1e-9);
    CHECK (! settings.enabled);
    target->releaseResources();
}
LUTHIER_TEST (RefPedalHost, missingBoardBlockRestoresLegacyDefault)
{
    auto target = std::make_unique<LuthierAudioProcessor>();
    target->getEngine().getReferencePedals().model().apply (cmd::SetBoardEnabled { true });
    const juce::String old = "{\"formatVersion\":1}";
    target->setStateInformation (old.toRawUTF8(), (int) old.getNumBytesAsUTF8());
    CHECK (! target->getEngine().getReferencePedals().model().getBoardSettings().enabled);
    CHECK (target->getEngine().getReferencePedals().model().getInstanceCount() == 0);
}
LUTHIER_TEST (RefPedalHost, repeatedRateAndSmallBlockLifecycle)
{
    auto p = std::make_unique<LuthierAudioProcessor>();
    const double rates[] { 44100, 48000, 96000 };
    const int sizes[] { 1, 37, 64 };
    for (double rate : rates)
        for (int size : sizes)
        {
            p->prepareToPlay (rate, size);
            juce::AudioBuffer<float> audio (2, size * 2 + 1); juce::MidiBuffer midi;
            audio.clear(); p->processBlock (audio, midi);
            bool finite = true;
            for (int c = 0; c < 2; ++c) for (int i = 0; i < audio.getNumSamples(); ++i) finite &= std::isfinite (audio.getSample (c, i));
            CHECK (finite); p->releaseResources();
        }
}
LUTHIER_TEST (RefPedalHost, forwardBoardSectionPreservesRawBytes)
{
    const juce::String future = "{ \"schema\" : 999, \"enabled\" : true, \"vendor\" : { \"gain\" : 0.12345678901234567 } }";
    const auto host = "{\"formatVersion\":1,\"referencePedals\":" + future + "}";
    auto p = std::make_unique<LuthierAudioProcessor>();
    p->setStateInformation (host.toRawUTF8(), (int) host.getNumBytesAsUTF8());
    CHECK (! p->getEngine().getReferencePedals().model().getBoardSettings().enabled);
    juce::MemoryBlock saved; p->getStateInformation (saved);
    const auto json = juce::String::fromUTF8 ((const char*) saved.getData(), (int) saved.getSize());
    juce::String raw; CHECK (BoardState::extractFromHostJson (json, raw)); CHECK (raw == future);
}

#include "../ReferencePedals/PedalRegistry.h"
#include <atomic>
namespace
{
std::atomic<float> probeValue { -1.f };
const ParamDesc probeParams[] {
    { "host_probe_gain", "Gain", "", ParamKind::Continuous, Taper::Linear, 0, 1, .2f, 1, 0, nullptr, PF_Automatable, 0, -1 }
};
const PedalDesc probeDesc { "test.host-automation", "host_probe_", 1, "Host automation probe", "Test", nullptr,
    ChannelMode::Stereo, BypassMode::DSP, 1, probeParams, 0, false, false, false, 1 };
class HostProbe final : public IPedal
{
public:
    const PedalDesc& desc() const noexcept override { return probeDesc; }
    void prepare (double, int, int) override {}
    void reset() noexcept override {}
    void setParam (int, float value) noexcept override { gain = value; probeValue.store (value, std::memory_order_relaxed); }
    float getParam (int) const noexcept override { return gain; }
    void process (double* l, double* r, int n) noexcept override { for (int i = 0; i < n; ++i) l[i] = r[i] = gain; }
private: float gain = .2f;
};
std::unique_ptr<IPedal> makeHostProbe() { return std::make_unique<HostProbe>(); }
}
LUTHIER_TEST (RefPedalHost, automationWithoutMessagePump)
{
    PedalRegistry::TestScope scope;
    CHECK (PedalRegistry::add (probeDesc, makeHostProbe));
    auto p = std::make_unique<LuthierAudioProcessor>();
    auto& model = p->getEngine().getReferencePedals().model();
    CHECK (model.apply (cmd::Add { "test.host-automation" })); CHECK (model.apply (cmd::SetBoardEnabled { true }));
    p->prepareToPlay (96000, 64);
    auto* lane = p->getState().getParameter ("ref_s01_p01"); CHECK (lane != nullptr);
    lane->setValueNotifyingHost (.8f);
    juce::AudioBuffer<float> audio (2, 64); juce::MidiBuffer midi;
    p->processBlock (audio, midi);
    CHECK_NEAR (probeValue.load(), .8f, 1e-6);
    // Persistence/echo has not needed a timer to deliver the audible update.
    CHECK_NEAR (model.getInstance (model.getOrder()[0])->params[0], .2f, 1e-6);
    p->releaseResources();
}
LUTHIER_TEST (RefPedalHost, automationThousandWritesPerSecond)
{
    PedalRegistry::TestScope scope; CHECK (PedalRegistry::add (probeDesc, makeHostProbe));
    auto p = std::make_unique<LuthierAudioProcessor>();
    auto& model = p->getEngine().getReferencePedals().model();
    CHECK (model.apply (cmd::Add { "test.host-automation" })); CHECK (model.apply (cmd::SetBoardEnabled { true }));
    p->prepareToPlay (96000, 64); auto* lane = p->getState().getParameter ("ref_s01_p01");
    juce::AudioBuffer<float> audio (2, 64); juce::MidiBuffer midi;
    int nextWrite = 0, writes = 0; float expected = .2f;
    for (int start = 0; start < 96000; start += 64)
    {
        while (nextWrite < start + 64)
        { expected = writes % 2 ? .75f : .25f; lane->setValueNotifyingHost (expected); ++writes; nextWrite += 96; }
        p->processBlock (audio, midi); CHECK_NEAR (probeValue.load(), expected, 1e-6);
    }
    CHECK (writes == 1000); p->releaseResources();
}
LUTHIER_TEST (RefPedalHost, hostPersistenceEchoDoesNotCreateUndo)
{
    PedalRegistry::TestScope scope; CHECK (PedalRegistry::add (probeDesc, makeHostProbe));
    auto p = std::make_unique<LuthierAudioProcessor>(); auto& model = p->getEngine().getReferencePedals().model();
    CHECK (model.apply (cmd::Add { "test.host-automation" })); CHECK (model.apply (cmd::SetBoardEnabled { true }));
    p->prepareToPlay (96000, 64);
    std::vector<PedalInstance> instances; BoardSettings settings; model.snapshot (instances, settings);
    CHECK (model.replaceAll (instances, settings)); CHECK (! model.canUndo());
    p->getState().getParameter ("ref_s01_p01")->setValueNotifyingHost (.8f);
    juce::AudioBuffer<float> audio (2, 64); juce::MidiBuffer midi; p->processBlock (audio, midi);
    juce::MemoryBlock saved; p->getStateInformation (saved);
    CHECK_NEAR (model.getInstance (model.getOrder()[0])->params[0], .8f, 1e-6); CHECK (! model.canUndo());
    p->releaseResources();
}
