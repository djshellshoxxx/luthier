// OWNER: WP2/WP5 publication safety regression
#include "TestFramework.h"
#include "AllocationFailureTestHook.h"
#include "../ReferencePedals/Midi/MidiRouter.h"
#include "../ReferencePedals/Midi/MidiMap.h"
#include <new>
#include <atomic>
#include <thread>
using namespace luthier::reference;
using namespace luthier::reference::midi;
using namespace luthier::tests;
LUTHIER_TEST (RefPedalMidiPublication, retirementCannotAllocateAfterPublication)
{
    MidiRouter router; router.prepare (48000, 64);
    auto first = std::make_unique<MapSnap>();
    Binding binding; binding.srcNum = 37; binding.target = 0;
    CHECK (first->add (binding)); router.publishMap (std::move (first));
    auto next = std::make_unique<MapSnap>(); binding.target = 1;
    CHECK (next->add (binding));
    bool threw = false;
    // Old publication allocated its first retired unique_ptr AFTER publishing.
    // Construction must now secure both possible single-reader retirement slots.
    failNextAllocationOfSizeOnThisThread (sizeof (std::unique_ptr<MapSnap>));
    const auto before = allocationsOnThisThread();
    try { router.publishMap (std::move (next)); } catch (const std::bad_alloc&) { threw = true; }
    const auto after = allocationsOnThisThread(); clearAllocationFailureOnThisThread();
    CHECK (! allocationFailureTriggeredOnThisThread()); CHECK (! threw); CHECK (after == before);
    juce::MidiBuffer input; input.addEvent (juce::MidiMessage::controllerEvent (1, 37, 127), 3);
    RouteOut output; router.process (input, {}, 0, 64, output);
    CHECK (output.nWrites == 1);
    if (output.nWrites == 1) { CHECK (output.writes[0].paramIdx == 1); CHECK (output.writes[0].offset == 3); }
    CHECK (router.reclaim() <= 2);
}

LUTHIER_TEST (RefPedalMidiPublication, concurrentStormKeepsSingleReaderRetirementBound)
{
    MidiRouter router; router.prepare (48000, 64);
    auto initial = std::make_unique<MapSnap>(); Binding binding; binding.srcNum = 37;
    CHECK (initial->add (binding)); router.publishMap (std::move (initial));
    juce::MidiBuffer input; input.addEvent (juce::MidiMessage::controllerEvent (1, 37, 127), 3);
    std::atomic<bool> started { false }, stop { false }; std::atomic<int> failures { 0 };
    std::thread reader ([&]
    {
        RouteOut output; int64_t sample = 0;
        started.store (true);
        while (! stop.load())
        {
            const auto before = allocationsOnThisThread();
            router.process (input, {}, sample, 64, output); sample += 64;
            if (allocationsOnThisThread() != before || output.nWrites > 1
                || (output.nWrites == 1 && (output.writes[0].paramIdx > 1 || output.writes[0].offset != 3))) ++failures;
        }
    });
    while (! started.load()) std::this_thread::yield();
    for (int i = 0; i < 1000; ++i)
    {
        auto map = std::make_unique<MapSnap>(); binding.target = static_cast<uint16_t> (i & 1);
        CHECK (map->add (binding)); const auto before = allocationsOnThisThread(); router.publishMap (std::move (map));
        CHECK (allocationsOnThisThread() == before); CHECK (router.reclaim() <= 2);
    }
    stop.store (true); reader.join(); CHECK (failures.load() == 0); CHECK (router.reclaim() <= 2);
}
