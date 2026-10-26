// OWNER: WP24
#include "TestFramework.h"
#include "../ReferencePedals/Board.h"
#include "../ReferencePedals/BoardModel.h"
#include "../ReferencePedals/PedalRegistry.h"
#include "../Support/ThreadProbe.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <thread>

namespace luthier::tests { long allocationsOnThisThread() noexcept; }
using namespace luthier::reference;

LUTHIER_TEST (RefPedalSoak, concurrentEditsAndProcessingRemainFiniteAndAllocationFree)
{
    PedalRegistry::registerAll();
    Board board;
    board.prepare (48000., 2048);
    std::vector<juce::String> models;
    for (const auto* descriptor : PedalRegistry::all())
        if (juce::String (descriptor->modelId).startsWith ("generic."))
            models.emplace_back (descriptor->modelId);
    CHECK (! models.empty());
    if (models.empty()) return;
    CHECK (board.model().apply (cmd::Add { models.front().toStdString() }));
    CHECK (board.model().apply (cmd::SetBoardEnabled { true }));
    CHECK (board.isActive());

    int seconds = std::getenv ("LUTHIER_SOAK") != nullptr ? 1800 : 30;
    if (const char* overrideSeconds = std::getenv ("LUTHIER_SOAK_SECONDS"))
        seconds = std::clamp (std::atoi (overrideSeconds), 30, 7200);
    std::atomic<bool> stop { false };
    std::atomic<uint64_t> blocks { 0 }, samples { 0 }, allocations { 0 }, locks { 0 }, invalid { 0 };
    std::thread audio ([&]
    {
        std::array<double, 2048> left {}, right {};
        juce::MidiBuffer incoming, outgoing;
        incoming.ensureSize (4096); outgoing.ensureSize (4096);
        constexpr int sizes[] { 1, 16, 37, 64, 481, 2048 };
        uint64_t cursor = 0, count = 0, bad = 0, allocated = 0;
        HostTransport host { true, true, 123., 0., 4, 4 };
        luthier::ThreadProbe::markAsAudioThread();
        const auto locksBefore = luthier::ThreadProbe::audioThreadLocks.load();
        while (! stop.load (std::memory_order_relaxed))
        {
            const int n = sizes[count % std::size (sizes)];
            for (int i = 0; i < n; ++i)
            {
                left[(size_t) i] = .1 * std::sin ((cursor + (uint64_t) i) * .071);
                right[(size_t) i] = .1 * std::cos ((cursor + (uint64_t) i) * .137);
            }
            host.ppq = double (cursor) / 48000. * host.bpm / 60.;
            const auto before = luthier::tests::allocationsOnThisThread();
            board.beginBlock (incoming, host, n);
            for (int placement = 0; placement < kNumPlacements; ++placement)
                board.processStereo (left.data(), right.data(), n, (Placement) placement);
            board.endBlock (outgoing);
            allocated += (uint64_t) (luthier::tests::allocationsOnThisThread() - before);
            outgoing.clear();
            for (int i = 0; i < n; ++i)
                if (! std::isfinite (left[(size_t) i]) || ! std::isfinite (right[(size_t) i])
                    || std::abs (left[(size_t) i]) > 16. || std::abs (right[(size_t) i]) > 16.) ++bad;
            cursor += (uint64_t) n;
            ++count;
        }
        const auto blockingLocks = (uint64_t) (luthier::ThreadProbe::audioThreadLocks.load() - locksBefore);
        blocks.store (count); samples.store (cursor); allocations.store (allocated); invalid.store (bad);
        locks.store (blockingLocks);
        luthier::ThreadProbe::markAsAudioThread (false);
    });

    std::mt19937 random (0x51a7u);
    const auto started = std::chrono::steady_clock::now();
    const auto deadline = started + std::chrono::seconds (seconds);
    uint64_t edits = 0, rejected = 0;
    while (std::chrono::steady_clock::now() < deadline)
    {
        const auto order = board.model().getOrder();
        juce::String reason;
        bool accepted = false;
        switch (random() % 7)
        {
            case 0: accepted = board.model().apply (cmd::SetMix { float (random() % 101) / 100.f }, &reason); break;
            case 1: if (order.size() < 8) accepted = board.model().apply (cmd::Add { models[random() % models.size()].toStdString() }, &reason); break;
            case 2: if (order.size() > 1) accepted = board.model().apply (cmd::Remove { order[random() % order.size()] }, &reason); break;
            case 3: if (! order.empty()) accepted = board.model().apply (cmd::Move { order[random() % order.size()], int (random() % order.size()) }, &reason); break;
            case 4: if (! order.empty()) accepted = board.model().apply (cmd::SetEnabled { order[random() % order.size()], (random() & 1) != 0 }, &reason); break;
            case 5: if (! order.empty()) accepted = board.model().apply (cmd::SetTrim { order[random() % order.size()], float (int (random() % 25) - 12), 0 }, &reason); break;
            case 6: accepted = (random() & 1) != 0 ? board.model().undo() : board.model().redo(); break;
        }
        ++edits;
        if (! accepted) ++rejected;
        board.reclaim();
        // 100 structural commands/sec while the audio thread runs freely.
        std::this_thread::sleep_until (started + std::chrono::milliseconds (10 * edits));
    }
    stop.store (true, std::memory_order_relaxed);
    audio.join();
    board.reclaim();
    std::cout << "    seed=0x51a7 seconds=" << seconds << " edits=" << edits << " rejected=" << rejected
              << " blocks=" << blocks.load() << " samples=" << samples.load()
              << " callback_new=" << allocations.load() << " callback_locks=" << locks.load()
              << " invalid=" << invalid.load() << std::endl;
    CHECK (blocks.load() > 1000);
    CHECK (edits >= (uint64_t) seconds * 80);
    CHECK (allocations.load() == 0);
    CHECK (locks.load() == 0);
    CHECK (invalid.load() == 0);
}
