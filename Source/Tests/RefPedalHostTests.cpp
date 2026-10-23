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
