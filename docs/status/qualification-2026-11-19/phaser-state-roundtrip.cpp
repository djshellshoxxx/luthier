// Supplemental native qualification for candidate 51fd4de677fd0d4a056ded5ee2e8845b9e81584d.
// This harness adds evidence only; production sources and committed tests are unchanged.
// Scope: real Board/BoardState/phaser paths, not a DAW project or full processor.
#include "Tests/TestFramework.h"
#include "ReferencePedals/Board.h"
#include "ReferencePedals/BoardModel.h"
#include "ReferencePedals/BoardState.h"
#include "ReferencePedals/PedalRegistry.h"
#include <array>
#include <iostream>
#include <memory>
#include <cstring>

namespace luthier::tests { long allocationsOnThisThread() noexcept; }
using namespace luthier::reference;
namespace
{
struct Fixture
{
    std::vector<PedalInstance> pedals;
    BoardSettings settings;
    juce::String json;
    bool valid = false;
};
Fixture authoredFixture(luthier::tests::TestContext& ctx, bool synced)
{
    PedalRegistry::registerAll();
    BoardModel model;
    bool good = true;
    const auto apply = [&](const BoardCommand& command)
    {
        juce::String error;
        const bool ok = model.apply(command, &error);
        CHECK_MSG(ok, error); good = good && ok;
    };
    const InstanceId first("70100000123412341234123456789001");
    const InstanceId second("70100000123412341234123456789002");
    apply(cmd::Add{"mxr-phase90-script", -1, Placement::beforeExistingPre, 0, first});
    apply(cmd::Add{"mxr-phase90-script", -1, Placement::beforeExistingPre, 0, second});
    for (int pedal = 0; pedal < 2; ++pedal)
    {
        const auto id = pedal == 0 ? first : second;
        apply(cmd::SetEnabled{id, true});
        apply(cmd::SetParam{id, 0, pedal == 0 ? .27f : .73f});
        apply(cmd::SetParam{id, 3, pedal == 0 ? .63f : .41f});
        apply(cmd::SetParam{id, 4, pedal == 0 ? .71f : .56f});
        apply(cmd::SetParam{id, 5, pedal == 0 ? .37f : .62f});
        apply(cmd::SetParam{id, 6, synced ? 1.f : 0.f});
        apply(cmd::SetParam{id, 7, pedal == 0 ? .2f : .8f});
        apply(cmd::SetParam{id, 8, pedal == 0 ? .31f : .67f});
        apply(cmd::SetParam{id, 10, 1.f});
        apply(cmd::SetParam{id, 12, 0.f});
        apply(cmd::SetTrim{id, pedal == 0 ? -1.5f : .5f, pedal == 0 ? .75f : -.5f});
    }
    apply(cmd::Move{second, 0});
    apply(cmd::SetMix{.73f});
    apply(cmd::SetBoardParam{"tempoSource", double(TempoSource::Host)});
    apply(cmd::SetBoardParam{"tempoBpm", 109.});
    apply(cmd::SetBoardEnabled{true});
    Fixture fixture;
    model.snapshot(fixture.pedals, fixture.settings);
    juce::String error;
    const bool saved = BoardState::save(model, fixture.json, &error);
    CHECK_MSG(saved, error);
    const auto parsed = juce::JSON::parse(fixture.json);
    CHECK(int(parsed["schema"]) == kSchemaVersion);
    CHECK(parsed["instances"].size() == 2);
    CHECK(parsed["order"].size() == 2);
    CHECK(parsed["order"][0].toString() == second.toString());
    CHECK(parsed["order"][1].toString() == first.toString());
    fixture.valid = good && saved && fixture.pedals.size() == 2;
    return fixture;
}
std::unique_ptr<Board> freshBoard(luthier::tests::TestContext& ctx, const Fixture& fixture,
                                  double rate, int block, bool encoded, double previousRate)
{
    auto board = std::make_unique<Board>();
    CHECK(board->setAssetStore(nullptr));
    board->prepare(previousRate, 64);
    juce::String error;
    const bool restored = encoded ? BoardState::restore(board->model(), fixture.json, &error)
                                  : board->model().replaceAll(fixture.pedals, fixture.settings, &error);
    CHECK_MSG(restored, error);
    board->prepare(rate, block);
    CHECK_MSG(board->getPreparationError().isEmpty(), board->getPreparationError());
    CHECK(board->isActive());
    return board;
}
void checkStoredState(luthier::tests::TestContext& ctx, Board& board, const Fixture& fixture)
{
    std::vector<PedalInstance> actual;
    BoardSettings settings;
    board.model().snapshot(actual, settings);
    CHECK(actual.size() == fixture.pedals.size());
    if (actual.size() != fixture.pedals.size()) return;
    CHECK(settings.enabled);
    CHECK(settings.mode == fixture.settings.mode);
    CHECK(settings.mix == fixture.settings.mix);
    CHECK(settings.tempoSource == TempoSource::Host);
    CHECK(settings.tempoBpm == 109.);
    const auto order = board.model().getOrder();
    CHECK(order.size() == fixture.pedals.size());
    for (size_t i = 0; i < actual.size(); ++i)
    {
        const auto& a = actual[i]; const auto& e = fixture.pedals[i];
        CHECK(a.instanceId == e.instanceId);
        CHECK(i < order.size() && order[i] == e.instanceId);
        CHECK(a.modelId == "mxr-phase90-script");
        CHECK(a.modelRevision == e.modelRevision);
        CHECK(!a.missing && a.enabled);
        CHECK(a.slot == e.slot);
        CHECK(a.placement == e.placement && a.lane == e.lane);
        CHECK(a.inDb == e.inDb && a.outDb == e.outDb);
        const auto* descriptor = PedalRegistry::find(a.modelId.c_str());
        CHECK(descriptor != nullptr);
        if (descriptor != nullptr)
            for (int parameter = 0; parameter < descriptor->numParams; ++parameter)
                CHECK(a.params[size_t(parameter)] == e.params[size_t(parameter)]);
    }
    juce::String saved, error;
    CHECK_MSG(BoardState::save(board.model(), saved, &error), error);
    CHECK(saved == fixture.json);
}
uint64_t bits(double value)
{
    uint64_t result = 0; std::memcpy(&result, &value, sizeof(value)); return result;
}
}

LUTHIER_TEST(PhaserStateQualification, nonDefaultParametersAndOrderSurviveSampleRateChanges)
{
    for (bool sync : {false, true})
    {
        const auto fixture = authoredFixture(ctx, sync);
        if (!fixture.valid) return;
        for (double rate : {44100., 48000., 88200., 96000., 192000.})
            for (int block : {1, 37, 64, 512})
            {
                auto board = freshBoard(ctx, fixture, rate, block, true, 96000.);
                checkStoredState(ctx, *board, fixture);
            }
    }
}

LUTHIER_TEST(PhaserStateQualification, freshRestoredAudioMatchesDirectStateAndFollowsHostTempo)
{
    constexpr double twoPi = 6.283185307179586476925286766559;
    for (bool sync : {false, true})
    {
        const auto fixture = authoredFixture(ctx, sync);
        if (!fixture.valid) return;
        auto zeroWetTrimFixture = fixture;
        // Preserve all slot trims, routing and other model settings. Only the
        // phasers' wet-trim parameter differs from the restored-state board.
        for (auto& pedal : zeroWetTrimFixture.pedals)
            pedal.params[4] = 0.f;
        for (double rate : {44100., 48000., 88200., 96000., 192000.})
            for (int block : {1, 37, 64, 512})
            {
                auto direct = freshBoard(ctx, fixture, rate, block, false, 48000.);
                auto restored = freshBoard(ctx, fixture, rate, block, true, 96000.);
                auto fixedTempo = freshBoard(ctx, fixture, rate, block, true, 44100.);
                auto zeroWetTrim = freshBoard(ctx, zeroWetTrimFixture, rate, block, false, 96000.);
                checkStoredState(ctx, *restored, fixture);
                CHECK(direct->getLatencySamples() == restored->getLatencySamples());
                std::array<std::array<double, 512>, 8> audio{};
                std::array<std::array<double, 512>, 2> input{};
                juce::MidiBuffer incoming, outgoing;
                incoming.ensureSize(64); outgoing.ensureSize(64);
                const int total = int(rate * .25);
                double ppq = .375, controlPpq = .375, maximumError = 0, wetTrimDifference = 0, tempoDifference = 0;
                bool exact = true, finite = true;
                const auto allocations = luthier::tests::allocationsOnThisThread();
                for (int at = 0; at < total;)
                {
                    const int n = std::min(block, total - at);
                    for (int i = 0; i < n; ++i)
                    {
                        const double time = (at + i) / rate;
                        input[0][size_t(i)] = .071 * std::sin(twoPi * 317 * time) + .019 * std::sin(twoPi * 1103 * time);
                        input[1][size_t(i)] = .061 * std::cos(twoPi * 233 * time) + .017 * std::sin(twoPi * 811 * time);
                        for (int pair = 0; pair < 4; ++pair)
                            for (int channel = 0; channel < 2; ++channel)
                                audio[size_t(pair * 2 + channel)][size_t(i)] = input[size_t(channel)][size_t(i)];
                    }
                    const double bpm = at < total / 2 ? 113. : 171.;
                    HostTransport host{true, true, bpm, ppq, 4, 4};
                    HostTransport control{true, true, 113., controlPpq, 4, 4};
                    Board* boards[]{direct.get(), restored.get(), fixedTempo.get(), zeroWetTrim.get()};
                    for (int pair = 0; pair < 4; ++pair)
                    {
                        boards[pair]->beginBlock(incoming, pair == 2 ? control : host, n);
                        for (int placement = 0; placement < kNumPlacements; ++placement)
                            boards[pair]->processStereo(audio[size_t(pair * 2)].data(), audio[size_t(pair * 2 + 1)].data(), n, Placement(placement));
                        boards[pair]->endBlock(outgoing);
                    }
                    for (int i = 0; i < n; ++i)
                        for (int channel = 0; channel < 2; ++channel)
                        {
                            const double a = audio[size_t(channel)][size_t(i)];
                            const double b = audio[size_t(channel + 2)][size_t(i)];
                            const double c = audio[size_t(channel + 4)][size_t(i)];
                            const double d = audio[size_t(channel + 6)][size_t(i)];
                            exact = exact && bits(a) == bits(b);
                            finite = finite && std::isfinite(a) && std::isfinite(b) && std::isfinite(c) && std::isfinite(d);
                            maximumError = std::max(maximumError, std::abs(a - b));
                            wetTrimDifference = std::max(wetTrimDifference, std::abs(b - d));
                            if (at + i > total * 3 / 4) tempoDifference = std::max(tempoDifference, std::abs(b - c));
                        }
                    ppq += n * bpm / (60 * rate); controlPpq += n * 113. / (60 * rate); at += n;
                }
                const auto allocationDelta = luthier::tests::allocationsOnThisThread() - allocations;
                const auto label = juce::String(rate) + "/" + juce::String(block) + (sync ? "/sync" : "/free");
                CHECK_MSG(exact, label + " direct versus restored error=" + juce::String(maximumError, 15));
                CHECK(finite);
                CHECK_MSG(wetTrimDifference > 1.e-6, label + " phaser wet-trim control did not change output");
                CHECK(allocationDelta == 0);
                if (sync) CHECK_MSG(tempoDifference > 1.e-6, label + " host tempo did not affect synced phaser");
                else CHECK_MSG(tempoDifference == 0, label + " free-running phaser followed host tempo");
                std::cout << "    " << label << " restored_max_error=" << maximumError
                          << " wet_trim_control_delta=" << wetTrimDifference << " tempo_delta=" << tempoDifference
                          << " callback_allocations=" << allocationDelta << '\n';
            }
    }
}
