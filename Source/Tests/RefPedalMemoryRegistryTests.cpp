// OWNER: WP2
#include "TestFramework.h"
#include "../ReferencePedals/PedalRegistry.h"
#include "../ReferencePedals/PedalPool.h"
#include <array>
using namespace luthier::reference;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }
LUTHIER_TEST (RefPedalMemoryRegistry, everyRegisteredModelHasAllocationFreePreflightAndBoundedActualCapacity)
{
    PedalRegistry::TestScope registry;
    PedalRegistry::registerAll();
    CHECK (PedalRegistry::all().size() >= 13);
    for (const auto* descriptor : PedalRegistry::all())
        for (double rate : { 8000., 44100., 48000., 88200., 96000., 192000. })
            for (int policy : { 0, 1, 2 })
                for (int setting : { -1, 0, 1 })
                {
                    std::array<float, kMaxParams> params {};
                    for (int p = 0; p < descriptor->numParams; ++p)
                        params[static_cast<size_t> (p)] = setting < 0 ? static_cast<float> (descriptor->params[p].defaultNormalised()) : static_cast<float> (setting);
                    const auto before = luthier::tests::allocationsOnThisThread();
                    const auto estimate = PedalRegistry::memoryRequirement (descriptor->modelId, rate, 2048, policy, params.data());
                    CHECK (luthier::tests::allocationsOnThisThread() == before);
                    CHECK_MSG (estimate.ownedBytes != kUnknownPreparedMemoryBytes, descriptor->modelId);
                    auto pedal = PedalRegistry::make (descriptor->modelId);
                    CHECK (pedal != nullptr); if (! pedal) continue;
                    for (int p = 0; p < descriptor->numParams; ++p) pedal->setParam (p, params[static_cast<size_t> (p)]);
                    pedal->prepare (rate, 2048, policy);
                    const auto reportBefore = luthier::tests::allocationsOnThisThread();
                    const auto actual = pedal->getPreparedMemoryBytes();
                    CHECK (luthier::tests::allocationsOnThisThread() == reportBefore);
                    CHECK_MSG (actual >= sizeof (IPedal) && actual <= estimate.ownedBytes, descriptor->modelId);
                    pedal->reset(); CHECK (pedal->getPreparedMemoryBytes() == actual);
                }
}
LUTHIER_TEST (RefPedalMemoryRegistry, eachRegisteredRuntimeAccountsItsObjectAndDryBuffers)
{
    PedalRegistry::TestScope registry;
    PedalRegistry::registerAll();
    for (const auto* descriptor : PedalRegistry::all())
        for (double rate : { 44100., 192000. })
        {
            PedalPool pool; CHECK (pool.setAssetStore (nullptr)); pool.prepare (rate, 2048);
            PedalInstance instance; instance.modelId = descriptor->modelId;
            for (int p = 0; p < descriptor->numParams; ++p)
                instance.params[static_cast<size_t> (p)] = static_cast<float> (descriptor->params[p].defaultNormalised());
            juce::String error; auto runtime = pool.acquire (instance, error);
            CHECK_MSG (runtime != nullptr, juce::String (descriptor->modelId) + ": " + error); if (! runtime) continue;
            const auto actual = sizeof (PedalRuntime) + runtime->pedal->getPreparedMemoryBytes()
                + runtime->dry[0].preparedMemoryBytes() + runtime->dry[1].preparedMemoryBytes();
            CHECK (pool.preparedBytes() == actual); CHECK (actual <= kPreparedPedalMemoryLimit);
            runtime.reset(); CHECK (pool.preparedBytes() == 0);
        }
}
