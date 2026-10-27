// OWNER: WP24
#include "TestFramework.h"
#include "../ReferencePedals/Board.h"
#include "../ReferencePedals/BoardState.h"
#include <array>
#include <random>

using namespace luthier::reference;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }

LUTHIER_TEST (RefPedalIntegration, thousandMalformedLoadsPreserveModelAndLiveAudio)
{
    Board board;
    board.prepare (48000, 64);
    const InstanceId id;
    CHECK (board.model().apply (cmd::Add { "generic.boost", -1, Placement::beforeExistingPre, 0, id }));
    CHECK (board.model().apply (cmd::SetBoardEnabled { true }));
    juce::String baseline;
    CHECK (BoardState::save (board.model(), baseline));
    const auto base = juce::JSON::parse (baseline);
    std::mt19937 random (0x54a7ef01u);
    juce::MidiBuffer midi;
    HostTransport transport;
    int rejected = 0;
    for (int iteration = 0; iteration < 1000; ++iteration)
    {
        auto mutation = base.clone();
        auto* root = mutation.getDynamicObject();
        juce::String candidate;
        switch (iteration % 8)
        {
            case 0: root->setProperty ("mix", 2. + (double) (random() % 10000)); break;
            case 1: root->setProperty ("schema", 1. + (double) (1 + random() % 999) / 1000.); break;
            case 2: root->setProperty ("enabled", "true"); break;
            case 3: root->setProperty ("instances", (int) (random() % 1000)); break;
            case 4:
            {
                juce::Array<juce::var> duplicate;
                duplicate.add (id.toString()); duplicate.add (id.toString());
                root->setProperty ("order", duplicate); break;
            }
            case 5: candidate = juce::String::charToString ((juce::juce_wchar) (1 + random() % 8)) + baseline; break;
            case 6: candidate = baseline.substring (0, 1 + (int) (random() % (baseline.length() - 2))); break;
            case 7: candidate = baseline + " invalid-token-" + juce::String ((int) random()); break;
        }
        if (candidate.isEmpty()) candidate = juce::JSON::toString (mutation, true);
        juce::String before, after, error;
        CHECK (BoardState::save (board.model(), before));
        const bool accepted = BoardState::restore (board.model(), candidate, &error);
        if (! accepted) ++rejected;
        const auto label = "malformed state seed0x54a7ef01 iteration" + juce::String (iteration);
        CHECK_MSG (! accepted, label + " accepted");
        CHECK_MSG (error.isNotEmpty(), label + " lacks error");
        CHECK (BoardState::save (board.model(), after));
        CHECK_MSG (after == before, label + " changed state");
        std::array<double, 64> left {}, right {};
        for (int i = 0; i < 64; ++i) left[(size_t) i] = right[(size_t) i] = .1 * std::sin ((iteration * 64. + i) * .071);
        const auto allocations = luthier::tests::allocationsOnThisThread();
        board.beginBlock (midi, transport, 64);
        for (auto placement : { Placement::beforeExistingPre, Placement::afterExistingPost, Placement::postCab })
            board.processStereo (left.data(), right.data(), 64, placement);
        board.endBlock (midi);
        const auto added = luthier::tests::allocationsOnThisThread() - allocations;
        bool finite = true;
        for (int i = 0; i < 64; ++i)
            finite = finite && std::isfinite (left[(size_t) i]) && std::isfinite (right[(size_t) i])
                && std::abs (left[(size_t) i]) <= 16 && std::abs (right[(size_t) i]) <= 16;
        CHECK_MSG (added == 0, label + " audio allocation");
        CHECK_MSG (finite, label + " invalid live audio");
        board.reclaim();
    }
    CHECK (rejected == 1000);
}
