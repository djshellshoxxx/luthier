#include "TestFramework.h"
#include "../ReferencePedals/Board.h"
#include "../ReferencePedals/BoardModel.h"
#include "../ReferencePedals/PedalRegistry.h"
#include "../ReferencePedals/PreparedMemory.h"
#include "../ReferencePedals/Midi/HostAutomationCapture.h"

using namespace luthier::tests;
using namespace luthier::reference;

namespace
{
const PedalDesc description {"test.bypassdelay", "test_bypassdelay_", 1, "Bypass delay probe", "Test", nullptr,
    ChannelMode::Stereo, BypassMode::True, 0, nullptr, 32, false, false, false, 0};
class DelayProbe final : public IPedal
{
public:
    const PedalDesc& desc() const noexcept override { return description; }
    void prepare (double, int, int) override { reset(); }
    void reset() noexcept override { history = {}; position = 0; }
    void setParam (int, float) noexcept override {}
    float getParam (int) const noexcept override { return 0; }
    int getLatencySamples() const noexcept override { return 32; }
    uint64_t getPreparedMemoryBytes() const noexcept override { return sizeof (*this); }
    void process (double* l, double* r, int n) noexcept override
    {
        for (int i = 0; i < n; ++i)
        {
            std::swap (l[i], history[0][position]);
            std::swap (r[i], history[1][position]);
            position = (position + 1) % 32;
        }
    }
private:
    std::array<std::array<double, 32>, 2> history {};
    size_t position = 0;
};
std::unique_ptr<IPedal> makeDelay() { return std::make_unique<DelayProbe>(); }

void checkBypass (TestContext& ctx, Placement placement, bool hostBypass)
{
    Board board;
    CHECK (board.setAssetStore (nullptr));
    board.prepare (48000.0, 64);
    CHECK (board.model().apply (cmd::Add {description.modelId, -1, placement}));
    CHECK (board.model().apply (cmd::SetBoardEnabled {true}));
    CHECK (board.model().apply (cmd::SetBoardParam {"limiterOn", 0}));
    juce::MidiBuffer midi;
    const auto process = [&] (double* l, double* r)
    {
        board.beginBlock (midi, {}, 64);
        for (auto p : {Placement::beforeExistingPre, Placement::afterExistingPost, Placement::postCab})
            board.processStereo (l, r, 64, p);
        board.endBlock (midi);
    };
    const auto settle = [&]
    {
        for (int block = 0; block < 40; ++block)
        {
            double l[64] {}, r[64] {};
            process (l, r);
        }
    };
    settle();
    CHECK (board.getLatencySamples() == 32);
    uint32_t serial = 0;
    for (bool enabled : {false, true, false})
    {
        if (hostBypass)
        {
            HostAutomationSnapshot host;
            host.values[0] = enabled ? 1.f : 0.f;
            host.serials[0] = ++serial;
            board.setHostAutomation (host);
        }
        else
            CHECK (board.model().apply (cmd::SetBoardEnabled {enabled}));
        settle();
        CHECK (board.getLatencySamples() == 32);
        CHECK_MSG (board.isActive(), "a delayed dry path still requires processing");
        double l[64] {}, r[64] {};
        l[0] = .25; r[0] = -.125;
        process (l, r);
        for (int i = 0; i < 64; ++i)
        {
            CHECK_NEAR (l[i], i == 32 ? .25 : 0.0, 1.e-9);
            CHECK_NEAR (r[i], i == 32 ? -.125 : 0.0, 1.e-9);
        }
    }
}
}

LUTHIER_TEST (RefPedalBypassLatency, modelAndHostBypassRetainMeasuredDelayAtEveryPlacement)
{
    PedalRegistry::TestScope registry;
    CHECK (PedalRegistry::add (description, makeDelay, &fixedPreparedMemory<DelayProbe>));
    for (auto placement : {Placement::beforeExistingPre, Placement::afterExistingPost, Placement::postCab})
        for (bool host : {false, true})
            checkBypass (ctx, placement, host);
}
