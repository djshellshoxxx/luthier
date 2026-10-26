#include "TestFramework.h"
#include "../ReferencePedals/Models/boss_dd3t.h"
#include "../ReferencePedals/PedalPool.h"
#include "../ReferencePedals/PedalRegistry.h"
#include <limits>

using namespace luthier::reference;
using models::BossDd3t;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }

LUTHIER_TEST (RefPedalDd3tMemory, actualReportIncludesAllSixPreparedBuffers)
{
    for (double rate : { 44100., 48000., 88200., 96000., 192000. })
    {
        BossDd3t pedal;
        pedal.prepare (rate, 512, 2);
        const auto before = luthier::tests::allocationsOnThisThread();
        const auto bytes = pedal.getPreparedMemoryBytes();
        CHECK (luthier::tests::allocationsOnThisThread() == before);
        CHECK_MSG (bytes != kUnknownPreparedMemoryBytes, "Actual DD-3T prepared memory is unknown at " + juce::String (rate));
        // Independent lower bound from the six requested arrays; capacity may
        // exceed size. Inline filters/oversamplers are already in sizeof(model).
        const uint64_t payload = 2 * sizeof (double) *
            (uint64_t (rate * .81) + 32 + uint64_t (rate * .81) + 16 + uint64_t (rate * .8) + 8);
        if (bytes != kUnknownPreparedMemoryBytes) CHECK (bytes >= sizeof (BossDd3t) + payload);
        pedal.reset();
        CHECK (pedal.getPreparedMemoryBytes() == bytes);
    }
}

LUTHIER_TEST (RefPedalDd3tMemory, lowerRateReprepareReportsRetainedCapacity)
{
    BossDd3t pedal;
    pedal.prepare (192000, 2048, 2);
    const auto high = pedal.getPreparedMemoryBytes();
    CHECK (high != kUnknownPreparedMemoryBytes);
    pedal.prepare (48000, 1, 0);
    const auto retained = pedal.getPreparedMemoryBytes();
    CHECK (retained != kUnknownPreparedMemoryBytes);
    // Existing prepare uses vector::assign, preserving allocated capacity when
    // the smaller request fits. Report owned capacities, not the new rate's size.
    CHECK (retained == high);
}

LUTHIER_TEST (RefPedalDd3tMemory, freshPreflightBoundsActualCapacityWithoutAllocating)
{
    for (double rate : { 8000., 44100., 48000., 88200., 96000., 192000., 1000000. })
        for (int policy : { 0, 1, 2 })
        {
            const auto before = luthier::tests::allocationsOnThisThread();
            const auto bound = BossDd3t::prepareMemory (rate, 2048, policy, nullptr);
            CHECK (luthier::tests::allocationsOnThisThread() == before);
            CHECK (bound.ownedBytes != kUnknownPreparedMemoryBytes);
            CHECK (bound.temporaryBytes == 0);
            BossDd3t pedal;
            pedal.setParam (3, policy == 2 ? 1.f : 0.f); // Short Loop versus delay.
            pedal.setParam (10, policy == 1 ? 1.f : 0.f); // 24 versus12bits.
            pedal.setParam (11, policy == 1 ? 1.f : 0.f); // Converter simulation.
            pedal.prepare (rate, 2048, policy);
            const auto actual = pedal.getPreparedMemoryBytes();
            CHECK (actual != kUnknownPreparedMemoryBytes && actual >= sizeof (BossDd3t));
            CHECK (actual <= bound.ownedBytes);
            const auto smallBlock = BossDd3t::prepareMemory (rate, 1, policy, nullptr);
            CHECK (smallBlock.ownedBytes == bound.ownedBytes);
        }
}

LUTHIER_TEST (RefPedalDd3tMemory, invalidInputsHaveNoUsableReservation)
{
    for (double rate : { -1., 0., 7999., 1000001., std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::max() })
        CHECK (BossDd3t::prepareMemory (rate, 512, 2, nullptr).ownedBytes == kUnknownPreparedMemoryBytes);
    for (int block : { -1, 0 })
        CHECK (BossDd3t::prepareMemory (48000, block, 2, nullptr).ownedBytes == kUnknownPreparedMemoryBytes);
    for (int policy : { -1, 3 })
        CHECK (BossDd3t::prepareMemory (48000, 512, policy, nullptr).ownedBytes == kUnknownPreparedMemoryBytes);
}

namespace
{
int dd3tFactories = 0;
std::unique_ptr<IPedal> makeMemoryDd3t()
{
    ++dd3tFactories;
    return std::make_unique<BossDd3t>();
}
PedalInstance memoryDd3t(int slot)
{
    PedalInstance result;
    const auto& descriptor = BossDd3t::descriptor();
    result.modelId = descriptor.modelId; result.slot = slot;
    for (int p = 0; p < descriptor.numParams; ++p)
        result.params[static_cast<size_t> (p)] = static_cast<float> (descriptor.params[p].defaultNormalised());
    return result;
}
}

LUTHIER_TEST (RefPedalDd3tMemory, poolRejectsBeforeConstructionAndTracksSharedPins)
{
    // Test-only registry; the production named-model catalogue stays unchanged.
    PedalRegistry::TestScope registry;
    CHECK (PedalRegistry::add (BossDd3t::descriptor(), makeMemoryDd3t, &BossDd3t::prepareMemory));
    PedalPool pool; CHECK (pool.setAssetStore (nullptr)); pool.prepare (192000, 2048);
    std::vector<PedalInstance> instances;
    for (int slot = 0; slot < 9; ++slot) instances.push_back (memoryDd3t (slot));
    dd3tFactories = 0; juce::String error;
    CHECK (kPreparedPedalMemoryLimit == 64000000);
    CHECK (!pool.preflightFresh (instances, error));
    CHECK (error.contains ("memory")); CHECK (dd3tFactories == 0); CHECK (pool.preparedBytes() == 0);
    auto fitting = instances; fitting.pop_back(); error.clear();
    CHECK (pool.preflightFresh (fitting, error)); CHECK (dd3tFactories == 0);
    std::vector<std::shared_ptr<PedalRuntime>> retained;
    uint64_t total = 0;
    for (const auto& instance : fitting)
    {
        auto runtime = pool.acquire (instance, error);
        CHECK_MSG (runtime != nullptr, error); if (!runtime) return;
        total += sizeof (PedalRuntime) + runtime->pedal->getPreparedMemoryBytes()
            + runtime->dry[0].preparedMemoryBytes() + runtime->dry[1].preparedMemoryBytes();
        retained.push_back (std::move (runtime));
    }
    CHECK (dd3tFactories == 8); CHECK (pool.preparedBytes() == total);
    CHECK (total <= kPreparedPedalMemoryLimit);
    auto same = pool.acquire (fitting.front(), error, retained.front().get());
    CHECK (same == retained.front()); CHECK (dd3tFactories == 8); CHECK (pool.preparedBytes() == total);
    CHECK (!pool.acquire (instances.back(), error));
    CHECK (error.contains ("memory")); CHECK (dd3tFactories == 8); CHECK (pool.preparedBytes() == total);
    const auto firstBytes = sizeof (PedalRuntime) + same->pedal->getPreparedMemoryBytes()
        + same->dry[0].preparedMemoryBytes() + same->dry[1].preparedMemoryBytes();
    retained.clear(); CHECK (pool.preparedBytes() == firstBytes);
    same.reset(); CHECK (pool.preparedBytes() == 0);
    pool.prepare (7999, 512); CHECK (!pool.acquire (instances.front(), error)); CHECK (dd3tFactories == 8);
    CHECK (pool.preparedBytes() == 0);
}
