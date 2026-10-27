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
    CHECK (parameters.size() >= 373);
    CHECK (dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[parameters.size() - 373])->paramID == "ref_board_enabled");
    CHECK (dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters[parameters.size() - 51])->paramID == "ref_s16_pos");
    CHECK (dynamic_cast<juce::AudioProcessorParameterWithID*> (parameters.getLast())->paramID == "ref_board_fb_kill");
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
std::atomic<double> probeBpm { 0.0 };
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
    uint64_t getPreparedMemoryBytes() const noexcept override { return sizeof (*this); }
    void reset() noexcept override {}
    void setTime (const TimeInfo& time) noexcept override { probeBpm.store (time.bpm, std::memory_order_relaxed); }
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
    CHECK (PedalRegistry::add (probeDesc, makeHostProbe, fixedPreparedMemory<HostProbe>));
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
    PedalRegistry::TestScope scope; CHECK (PedalRegistry::add (probeDesc, makeHostProbe, fixedPreparedMemory<HostProbe>));
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
    PedalRegistry::TestScope scope; CHECK (PedalRegistry::add (probeDesc, makeHostProbe, fixedPreparedMemory<HostProbe>));
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

LUTHIER_TEST (RefPedalHost, appendedAuxBusesPreserveLayoutAndRenderSlicedSends)
{
    PedalRegistry::TestScope scope; CHECK (PedalRegistry::add (probeDesc, makeHostProbe, fixedPreparedMemory<HostProbe>));
    auto p = std::make_unique<LuthierAudioProcessor>();
    const int first = p->getBusCount (false) - 3;
    CHECK (p->getBus (false, first)->getName() == "Pedal B");
    CHECK (p->getBus (false, first + 1)->getName() == "Wet only");
    CHECK (p->getBus (false, first + 2)->getName() == "Dry only");
    auto layout = p->getBusesLayout();
    for (int i = 0; i < 3; ++i) { CHECK (layout.outputBuses[first + i].isDisabled()); layout.outputBuses.set (first + i, juce::AudioChannelSet::stereo()); }
    CHECK (p->setBusesLayout (layout));
    auto& model = p->getEngine().getReferencePedals().model();
    CHECK (model.apply (cmd::Add { "test.host-automation" })); CHECK (model.apply (cmd::SetBoardEnabled { true }));
    p->prepareToPlay (48000, 37);
    juce::AudioBuffer<float> audio (p->getTotalNumOutputChannels(), 81); juce::MidiBuffer midi;
    audio.clear(); p->processBlock (audio, midi);
    auto wet = p->getBusBuffer (audio, false, first + 1);
    bool signal = false, finite = true;
    for (int i = 0; i < wet.getNumSamples(); ++i) { signal |= std::abs (wet.getSample (0, i)) > .01f; finite &= std::isfinite (wet.getSample (0, i)); }
    CHECK (signal); CHECK (finite);
    CHECK (model.apply (cmd::SetBoardEnabled { false }));
    audio.clear(); p->processBlock (audio, midi);
    for (int bus = first; bus < first + 3; ++bus)
    { auto out = p->getBusBuffer (audio, false, bus); CHECK (out.getMagnitude (0, out.getNumSamples()) == 0.f); }
    p->releaseResources();
}

#include "../Routing/MidiOutRouter.h"
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
LUTHIER_TEST (RefPedalHost, outputRouterBoundsCombinedSources)
{
    luthier::MidiOutRouter router; router.prepare (48000, 64);
    juce::MidiBuffer input, output; output.ensureSize (2048);
    for (int i = 0; i < 400; ++i) input.addEvent (juce::MidiMessage::controllerEvent (1, 20, i % 128), i % 64);
    router.captureInput (input);
    luthier::MidiOutConfig config; config.enabled = true; config.passThrough = true;
    luthier::StringActivityQueue activity;
    const auto before = luthier::tests::allocationsOnThisThread();
    router.emitBounded (output, config, activity, 64, 2048);
    const auto after = luthier::tests::allocationsOnThisThread();
    CHECK (before == after); CHECK (output.data.size() <= 2048); CHECK (! output.isEmpty()); CHECK (router.getOverflowCount() > 0);
}

namespace
{
class FixedHostClock final : public juce::AudioPlayHead
{
    juce::Optional<PositionInfo> getPosition() const override
    { PositionInfo p; p.setBpm (180); p.setPpqPosition (20); p.setIsPlaying (true); return p; }
};
}
LUTHIER_TEST (RefPedalHost, extendedTempoSourceAndBpmWithoutMessagePump)
{
    PedalRegistry::TestScope scope; CHECK (PedalRegistry::add (probeDesc, makeHostProbe, fixedPreparedMemory<HostProbe>));
    auto p = std::make_unique<LuthierAudioProcessor>(); auto& model = p->getEngine().getReferencePedals().model();
    CHECK (model.apply (cmd::Add { "test.host-automation" })); CHECK (model.apply (cmd::SetBoardEnabled { true }));
    FixedHostClock clock; p->setPlayHead (&clock); p->prepareToPlay (48000,64);
    auto* source = p->getState().getParameter ("ref_board_tempo_source"); auto* tempo = p->getState().getParameter ("ref_board_tempo_bpm");
    CHECK (source && tempo); source->setValueNotifyingHost (0); tempo->setValueNotifyingHost (tempo->convertTo0to1 (240));
    juce::AudioBuffer<float> audio (2,64); juce::MidiBuffer midi;
    for (int i = 0; i < 40; ++i) { audio.clear(); p->processBlock(audio,midi); }
    CHECK_NEAR (probeBpm.load(),240,1e-6); CHECK (model.getBoardSettings().tempoSource == TempoSource::Auto); CHECK_NEAR (model.getBoardSettings().tempoBpm,120,1e-9);
    source->setValueNotifyingHost (source->convertTo0to1 (1));
    for (int i = 0; i < 40; ++i) { audio.clear(); p->processBlock(audio,midi); }
    CHECK_NEAR (probeBpm.load(),180,1e-6);
    p->releaseResources(); p->setPlayHead (nullptr);
}
LUTHIER_TEST (RefPedalHost, extendedParameterDefaultsNamesAndLowGainPreservation)
{
    PedalRegistry::TestScope scope; CHECK (PedalRegistry::add (probeDesc,makeHostProbe, fixedPreparedMemory<HostProbe>));
    auto p = std::make_unique<LuthierAudioProcessor>(); auto& model = p->getEngine().getReferencePedals().model();
    for (const auto& d : extendedHostParameters)
    {
        auto* parameter = p->getState().getParameter (d.id); CHECK (parameter != nullptr);
        CHECK_NEAR (parameter->convertFrom0to1(parameter->getDefaultValue()), d.defaultValue,1e-3);
    }
    auto* lane = p->getState().getParameter("ref_s01_p01"); CHECK (lane->getName(128) == "-"); CHECK (!lane->isAutomatable());
    CHECK (model.apply(cmd::Add{"test.host-automation"})); CHECK (model.apply(cmd::SetBoardParam{"reampGainDb",-200}));
    juce::MemoryBlock saved; p->getStateInformation(saved); // message-thread metadata/persistence service
    CHECK (lane->getName(128).contains("Gain")); CHECK (lane->isAutomatable());
    p->getStateInformation(saved); CHECK_NEAR(model.getBoardSettings().reampGainDb,-200,1e-6);
    auto* reamp = p->getState().getParameter("ref_board_reamp_gain"); reamp->setValueNotifyingHost(0); p->getStateInformation(saved);
    CHECK_NEAR(model.getBoardSettings().reampGainDb,-120,1e-6);
}
LUTHIER_TEST (RefPedalHost, vacantSlotHostValuesSurviveSaveRestoreAndService)
{
    auto p=std::make_unique<LuthierAudioProcessor>();
    const char* ids[]{"ref_s16_in_db","ref_s16_out_db","ref_s16_p16","ref_s16_enabled","ref_s16_pos"};
    const float physical[]{-3.375f,7.125f,.37f,0.f,4.f};
    for (int i=0;i<5;++i)
    { auto* parameter=p->getState().getParameter(ids[i]); CHECK(parameter); parameter->setValueNotifyingHost(parameter->convertTo0to1(physical[i])); }
    juce::MemoryBlock saved; p->getStateInformation(saved);
    for (int i=0;i<5;++i) CHECK_NEAR(p->getState().getRawParameterValue(ids[i])->load(),physical[i],1e-5);
    auto restored=std::make_unique<LuthierAudioProcessor>(); restored->setStateInformation(saved.getData(),(int)saved.getSize());
    restored->prepareToPlay(48000,64); juce::MemoryBlock resaved; restored->getStateInformation(resaved);
    for (int i=0;i<5;++i) CHECK_NEAR(restored->getState().getRawParameterValue(ids[i])->load(),physical[i],1e-5);
    CHECK(restored->getEngine().getReferencePedals().model().getInstanceCount()==0);
    restored->releaseResources();
}
LUTHIER_TEST (RefPedalHost, unusedPopulatedSlotLanePreservesHostValueWithoutChangingPedal)
{
    PedalRegistry::TestScope scope; CHECK(PedalRegistry::add(probeDesc,makeHostProbe, fixedPreparedMemory<HostProbe>));
    auto p=std::make_unique<LuthierAudioProcessor>(); auto& model=p->getEngine().getReferencePedals().model();
    CHECK(model.apply(cmd::Add{"test.host-automation"}));
    auto* unused=p->getState().getParameter("ref_s01_p16"); unused->setValueNotifyingHost(.63f);
    juce::MemoryBlock saved; p->getStateInformation(saved);
    CHECK_NEAR(unused->getValue(),.63f,1e-6); CHECK_NEAR(model.getInstance(model.getOrder()[0])->params[0],.2f,1e-6);
    auto restored=std::make_unique<LuthierAudioProcessor>(); restored->setStateInformation(saved.getData(),(int)saved.getSize());
    juce::MemoryBlock resaved; restored->getStateInformation(resaved);
    CHECK_NEAR(restored->getState().getParameter("ref_s01_p16")->getValue(),.63f,1e-6);
    auto& restoredModel=restored->getEngine().getReferencePedals().model(); CHECK_NEAR(restoredModel.getInstance(restoredModel.getOrder()[0])->params[0],.2f,1e-6);
}
LUTHIER_TEST (RefPedalHost, allReferenceHostIntentSurvivesRejectedStructuralRequests)
{
    auto p=std::make_unique<LuthierAudioProcessor>(); const auto& parameters=p->getParameters(); int base=-1;
    for(int i=0;i<parameters.size();++i) if(auto* withId=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameters[i]))
        if(withId->paramID=="ref_board_enabled") { base=i; break; }
    CHECK(base>=0); juce::StringArray ids; std::array<float,373> expected{};
    for(int i=0;i<373;++i)
    {
        auto* parameter=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameters[base+i]); ids.add(parameter->paramID);
        parameter->setValueNotifyingHost(.63f); expected[(size_t)i]=parameter->getValue();
    }
    juce::MemoryBlock saved; p->getStateInformation(saved);
    for(int i=0;i<373;++i) CHECK_MSG(std::abs(p->getState().getParameter(ids[i])->getValue()-expected[(size_t)i])<1e-5,ids[i]+" changed during save/service");
    auto restored=std::make_unique<LuthierAudioProcessor>(); restored->setStateInformation(saved.getData(),(int)saved.getSize());
    juce::MemoryBlock resaved; restored->getStateInformation(resaved);
    for(int i=0;i<373;++i) CHECK_MSG(std::abs(restored->getState().getParameter(ids[i])->getValue()-expected[(size_t)i])<1e-5,ids[i]+" changed after restore/service");
    CHECK(restored->getEngine().getReferencePedals().model().getInstanceCount()==0);
    const auto settings=restored->getEngine().getReferencePedals().model().getBoardSettings();
    CHECK(!settings.fb.on); CHECK(!settings.loops[0].used); CHECK(!settings.loops[1].used);
}
namespace
{
const PedalDesc feedbackProbeDesc { "test.host-feedback", "host_fb_", 1, "Host feedback probe", "Test", nullptr,
    ChannelMode::Stereo, BypassMode::DSP, 0, nullptr, 0, false, false, false, 1 };
class HostFeedbackProbe final : public IPedal
{
public:
    const PedalDesc& desc() const noexcept override { return feedbackProbeDesc; }
    uint64_t getPreparedMemoryBytes() const noexcept override { return sizeof (*this); }
    void prepare(double,int,int) override {} void reset() noexcept override {}
    void setParam(int,float) noexcept override {} float getParam(int) const noexcept override { return 0; }
    void process(double*,double*,int) noexcept override {}
};
std::unique_ptr<IPedal> makeHostFeedbackProbe() { return std::make_unique<HostFeedbackProbe>(); }
}
LUTHIER_TEST (RefPedalHost, restoredKillIntentStaysReleasedUntilFreshHostEdge)
{
    PedalRegistry::TestScope scope; CHECK(PedalRegistry::add(feedbackProbeDesc,makeHostFeedbackProbe, fixedPreparedMemory<HostFeedbackProbe>));
    auto source=std::make_unique<LuthierAudioProcessor>(); auto& model=source->getEngine().getReferencePedals().model();
    CHECK(model.apply(cmd::Add{"test.host-feedback"})); const auto id=model.getOrder()[0];
    reference::FeedbackSettings feedback; feedback.on=true; feedback.src=feedback.dst=id; feedback.gain=.5f;
    CHECK(model.apply(cmd::SetFeedback{feedback})); CHECK(model.apply(cmd::SetBoardEnabled{true}));
    CHECK(model.apply(cmd::SetBoardParam{"reampEnable",1})); CHECK(model.apply(cmd::SetBoardParam{"reampReplace",1}));
    juce::MemoryBlock releasedState,heldIntentState; source->getStateInformation(releasedState);
    source->getState().getParameter("ref_board_fb_kill")->setValueNotifyingHost(1); source->getStateInformation(heldIntentState);
    auto a=std::make_unique<LuthierAudioProcessor>(),b=std::make_unique<LuthierAudioProcessor>();
    a->setStateInformation(heldIntentState.getData(),(int)heldIntentState.getSize()); b->setStateInformation(releasedState.getData(),(int)releasedState.getSize());
    CHECK(a->getState().getParameter("ref_board_fb_kill")->getValue()==1.f);
    CHECK(!a->getEngine().getReferencePedals().model().getBoardSettings().fb.kill);
    auto prepare=[&](LuthierAudioProcessor& processor)
    {
        auto layout=processor.getBusesLayout(); layout.inputBuses.set(0,juce::AudioChannelSet::stereo());
        layout.outputBuses.set(processor.getBusCount(false)-2,juce::AudioChannelSet::stereo()); CHECK(processor.setBusesLayout(layout)); processor.prepareToPlay(48000,64);
    };
    prepare(*a); prepare(*b); juce::AudioBuffer<float> audioA(a->getTotalNumOutputChannels(),64),audioB(b->getTotalNumOutputChannels(),64); juce::MidiBuffer midiA,midiB;
    auto difference=[&]
    {
        float difference=0;
        for(int block=0;block<8;++block)
        {
            audioA.clear(); audioB.clear(); audioA.setSample(0,0,.1f); audioA.setSample(1,0,.1f); audioB.setSample(0,0,.1f); audioB.setSample(1,0,.1f);
            a->processBlock(audioA,midiA); b->processBlock(audioB,midiB);
            auto wetA=a->getBusBuffer(audioA,false,a->getBusCount(false)-2),wetB=b->getBusBuffer(audioB,false,b->getBusCount(false)-2);
            for(int i=0;i<64;++i) difference=std::max(difference,std::abs(wetA.getSample(0,i)-wetB.getSample(0,i)));
        }
        return difference;
    };
    CHECK_NEAR(difference(),0,1e-7); // restored APVTS1 must not recreate the kill pulse
    auto* kill=a->getState().getParameter("ref_board_fb_kill"); kill->setValueNotifyingHost(0); kill->setValueNotifyingHost(1);
    CHECK(difference()>1e-5f); // a fresh actual edge still reaches feedback DSP
    a->releaseResources(); b->releaseResources();
}

LUTHIER_TEST (RefPedalHost, processorFeedsMeasuredLoadIntoBoardWithoutMessagePump)
{
    auto p=std::make_unique<LuthierAudioProcessor>(); p->prepareToPlay(48000,64);
    auto& monitor=p->getCpuLoadMonitorForTesting();
    for(int bucket=0;bucket<20;++bucket) monitor.addBlock(.009,.010);
    CHECK(monitor.hasShortWindowOnAudioThread());
    juce::AudioBuffer<float> audio(2,64); audio.clear(); juce::MidiBuffer midi; p->processBlock(audio,midi);
    const auto meters=p->getEngine().getReferencePedals().readMeters();
    CHECK(meters.cpuAvailable); CHECK(meters.cpuPct>80.f);
    CHECK_NEAR(meters.cpuPct,100*monitor.getShortMeanOnAudioThread(),1e-3);
    p->releaseResources();
}
LUTHIER_TEST (RefPedalHost, positionGestureDoesNotPublishPreviewOrCancelledValues)
{
    PedalRegistry::TestScope scope; CHECK(PedalRegistry::add(probeDesc,makeHostProbe, fixedPreparedMemory<HostProbe>));
    auto p=std::make_unique<LuthierAudioProcessor>(); auto& model=p->getEngine().getReferencePedals().model();
    CHECK(model.apply(cmd::Add{"test.host-automation"})); CHECK(model.apply(cmd::Add{"test.host-automation"}));
    const auto first=model.getOrder()[0],second=model.getOrder()[1]; juce::MemoryBlock state; p->getStateInformation(state);
    auto* positionA=p->getState().getParameter("ref_s01_pos"),*positionB=p->getState().getParameter("ref_s02_pos");
    struct ValueObserver final : juce::AudioProcessorParameter::Listener
    { int changes=0; void parameterValueChanged(int,float) override { ++changes; } void parameterGestureChanged(int,bool) override {} } observer;
    positionA->addListener(&observer); positionB->addListener(&observer); const float original=positionB->getValue();
    positionB->beginChangeGesture(); CHECK(model.apply(cmd::Move{second,0}));
    p->getStateInformation(state); p->getStateInformation(state);
    CHECK(model.getOrder()[0]==second); CHECK_NEAR(positionB->getValue(),original,1e-7); CHECK(observer.changes==0);
    model.apply(cmd::Move{second,1}); p->getStateInformation(state); positionB->endChangeGesture(); p->getStateInformation(state);
    CHECK(model.getOrder()[0]==first); CHECK_NEAR(positionB->getValue(),original,1e-7); CHECK(observer.changes==0);
    positionA->removeListener(&observer); positionB->removeListener(&observer);
}
LUTHIER_TEST (RefPedalHost, acceptedPositionGestureRecordsFinalValueWithOneModelMove)
{
    PedalRegistry::TestScope scope; CHECK(PedalRegistry::add(probeDesc,makeHostProbe, fixedPreparedMemory<HostProbe>));
    auto p=std::make_unique<LuthierAudioProcessor>(); auto& model=p->getEngine().getReferencePedals().model();
    CHECK(model.apply(cmd::Add{"test.host-automation"})); CHECK(model.apply(cmd::Add{"test.host-automation"}));
    const auto first=model.getOrder()[0],second=model.getOrder()[1]; juce::MemoryBlock state; p->getStateInformation(state);
    auto* position=p->getState().getParameter("ref_s02_pos"); position->beginChangeGesture(); CHECK(model.apply(cmd::Move{second,0}));
    p->getStateInformation(state); CHECK_NEAR(position->getValue(),1./15,1e-7);
    position->setValueNotifyingHost(position->convertTo0to1(0)); position->endChangeGesture(); p->getStateInformation(state);
    CHECK(model.getOrder()[0]==second); CHECK_NEAR(p->getState().getRawParameterValue("ref_s01_pos")->load(),1,1e-7);
    CHECK_NEAR(p->getState().getRawParameterValue("ref_s02_pos")->load(),0,1e-7);
    CHECK(model.undo()); CHECK(model.getOrder()[0]==first); CHECK(model.getInstanceCount()==2);
    p->getStateInformation(state); CHECK_NEAR(position->convertFrom0to1(position->getValue()),1,1e-7);
}
