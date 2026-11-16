#include "TestFramework.h"
#include "../DSP/Effects/EffectsChain.h"
#include <atomic>
#include <chrono>
#include <thread>

namespace luthier
{
// Test-only installation of a pedal that pauses inside a real virtual reader.
struct EffectsChainLifetimeTestAccess
{
    static void install (EffectsChain& chain, std::unique_ptr<Pedal> pedal)
    {
        chain.slots[0].pedal = std::move (pedal);
        chain.slots[0].type.store (PedalType::Boost);
        chain.slots[0].live.store (chain.slots[0].pedal.get());
    }
    static size_t retiredCount (EffectsChain& chain)
    {
        const juce::ScopedLock lock (chain.swapLock);
        return chain.retired.size();
    }
};
}

namespace
{
struct Pause
{
    std::atomic<bool> entered { false }, resume { false };
    std::atomic<int> destroyed { 0 };
    void wait() noexcept
    {
        entered.store (true);
        while (! resume.load())
            std::this_thread::yield();
    }
    bool awaitEntry()
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (2);
        while (! entered.load() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::yield();
        return entered.load();
    }
};

class PausingPedal final : public luthier::Pedal
{
public:
    explicit PausingPedal (Pause& p) : pause (p) {}
    ~PausingPedal() override { ++pause.destroyed; }
    luthier::PedalType getType() const noexcept override { return luthier::PedalType::Boost; }
    const char* getName() const noexcept override { return "Lifetime fixture"; }
    void prepare (double, int) override {}
    void reset() noexcept override {}
    void process (double*, double*, int) noexcept override {}
    int getNumParameters() const noexcept override { return 0; }
    const luthier::PedalParam& getParameterDescriptor (int) const noexcept override
    { static const luthier::PedalParam parameter; return parameter; }
    int getLatencySamples() const noexcept override { pause.wait(); return latency; }
    void setOversamplingFactor (int, int, bool) noexcept override { pause.wait(); latency = 23; }
private:
    void parameterChanged (int, double) override {}
    Pause& pause;
    int latency = 17;
};

void exerciseRetirement (luthier::tests::TestContext& ctx, bool clear, bool qualityReader)
{
    Pause pause;
    luthier::EffectsChain chain;
    luthier::EffectsChainLifetimeTestAccess::install (chain, std::make_unique<PausingPedal> (pause));
    int latency = 0;
    std::thread reader ([&]
    {
        if (qualityReader) chain.setOversamplingFactor (2);
        else latency = chain.getLatencySamples();
    });
    const bool entered = pause.awaitEntry();
    CHECK (entered);
    if (entered)
    {
        if (clear) chain.clear();
        else chain.setSlotType (0, luthier::PedalType::None);
        chain.reclaimRetired();
        CHECK (pause.destroyed.load() == 0);
        CHECK (luthier::EffectsChainLifetimeTestAccess::retiredCount (chain) == 1);
    }
    pause.resume.store (true);
    reader.join();
    if (! entered) return;
    if (! qualityReader) CHECK (latency == 17);
    // Ending a reader must never destroy the pedal on the audio thread.
    CHECK (pause.destroyed.load() == 0);
    chain.reclaimRetired();
    CHECK (pause.destroyed.load() == 1);
    CHECK (luthier::EffectsChainLifetimeTestAccess::retiredCount (chain) == 0);
    CHECK (chain.getLatencySamples() == 0);
}
}

LUTHIER_TEST (EffectsChainLifetime, replacementWaitsForLatencyReader)
{ exerciseRetirement (ctx, false, false); }
LUTHIER_TEST (EffectsChainLifetime, clearWaitsForLatencyReader)
{ exerciseRetirement (ctx, true, false); }
LUTHIER_TEST (EffectsChainLifetime, replacementWaitsForQualityReader)
{ exerciseRetirement (ctx, false, true); }
LUTHIER_TEST (EffectsChainLifetime, moveRepublishesWithoutDestroyingPedal)
{
    Pause pause;
    pause.resume.store (true);
    luthier::EffectsChain chain;
    luthier::EffectsChainLifetimeTestAccess::install (chain, std::make_unique<PausingPedal> (pause));
    auto* original = chain.getPedal (0);
    chain.moveSlot (0, 7);
    CHECK (chain.getPedal (7) == original);
    CHECK (chain.getPedal (0) == nullptr);
    CHECK (chain.getLatencySamples() == 17);
    chain.moveSlot (7, 0);
    CHECK (chain.getPedal (0) == original);
    CHECK (chain.getLatencySamples() == 17);
    CHECK (pause.destroyed.load() == 0);
    chain.clear();
    CHECK (pause.destroyed.load() == 1);
}
