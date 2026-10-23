// WP24: exercise the actual registered library; missing models remain a separate release gate.
#include "TestFramework.h"
#include "../ReferencePedals/PedalRegistry.h"
#include <array>
#include <limits>

using namespace luthier::reference;
namespace luthier::tests { long allocationsOnThisThread() noexcept; }

namespace
{
    constexpr int length = 4097;
    using Signal = std::array<double, length>;
    void input (Signal& left, Signal& right)
    {
        for (int i = 0; i < length; ++i)
        {
            left[(size_t) i] = .12 * std::sin (i * .071) + .03 * std::cos (i * .313);
            right[(size_t) i] = .09 * std::cos (i * .117);
        }
    }
    void render (IPedal& pedal, Signal& left, Signal& right, int block)
    {
        for (int offset = 0; offset < length; offset += block)
            pedal.process (left.data() + offset, right.data() + offset, std::min (block, length - offset));
    }
}

LUTHIER_TEST (RefPedalSweep, registeredModelsAreDeterministicAcrossRatesAndPartitions)
{
    PedalRegistry::registerAll();
    CHECK (! PedalRegistry::all().empty());
    for (const auto* descriptor : PedalRegistry::all())
        for (double rate : { 44100., 48000., 88200., 96000., 192000. })
            for (int block : { 1, 16, 37, 64, 481, 2048 })
            {
                auto reference = PedalRegistry::make (descriptor->modelId);
                auto candidate = PedalRegistry::make (descriptor->modelId);
                CHECK (reference != nullptr && candidate != nullptr);
                if (! reference || ! candidate) continue;
                const PedalServices services { nullptr, 0x51a7u, 2048, rate };
                reference->setServices (services);
                candidate->setServices (services);
                reference->prepare (rate, 2048, 1);
                candidate->prepare (rate, 2048, 1);
                Signal left, right, actualLeft, actualRight;
                input (left, right);
                actualLeft = left; actualRight = right;
                const auto allocations = luthier::tests::allocationsOnThisThread();
                render (*reference, left, right, 37);
                render (*candidate, actualLeft, actualRight, block);
                CHECK (luthier::tests::allocationsOnThisThread() == allocations);
                double error = 0;
                bool finite = true;
                for (int i = 0; i < length; ++i)
                {
                    finite = finite && std::isfinite (actualLeft[(size_t) i]) && std::isfinite (actualRight[(size_t) i]);
                    error = std::max ({ error, std::abs (left[(size_t) i] - actualLeft[(size_t) i]),
                                       std::abs (right[(size_t) i] - actualRight[(size_t) i]) });
                }
                CHECK_MSG (finite, juce::String (descriptor->modelId) + " nonfinite output");
                CHECK_MSG (error <= 1.e-9, juce::String (descriptor->modelId) + " partition error " + juce::String (error, 12));
            }
}

LUTHIER_TEST (RefPedalSweep, allRegisteredControlsRecoverFromHostileInput)
{
    PedalRegistry::registerAll();
    for (const auto* descriptor : PedalRegistry::all())
    {
        auto pedal = PedalRegistry::make (descriptor->modelId);
        CHECK (pedal != nullptr);
        if (! pedal) continue;
        for (double rate : { 44100., 48000., 88200., 96000., 192000. })
        {
            pedal->prepare (rate, 64, 1);
            for (int parameter = 0; parameter < descriptor->numParams; ++parameter)
                for (float setting : { 0.f, .25f, .5f, .75f, 1.f })
                {
                    pedal->reset();
                    pedal->setParam (parameter, setting);
                    std::array<double, 64> left {}, right {};
                    const double hostile[] { std::numeric_limits<double>::quiet_NaN(),
                        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                        1.e6, -1.e6, 1.e-310, -1.e-310, .1 };
                    for (int i = 0; i < 64; ++i) left[(size_t) i] = right[(size_t) i] = hostile[i % 8];
                    const auto allocations = luthier::tests::allocationsOnThisThread();
                    pedal->process (left.data(), right.data(), 64);
                    CHECK (luthier::tests::allocationsOnThisThread() == allocations);
                    bool bounded = true;
                    for (int i = 0; i < 64; ++i)
                        bounded = bounded && std::isfinite (left[(size_t) i]) && std::isfinite (right[(size_t) i])
                            && std::abs (left[(size_t) i]) <= 16. && std::abs (right[(size_t) i]) <= 16.;
                    CHECK_MSG (bounded, juce::String (descriptor->modelId) + " hostile-input bound, parameter " + juce::String (parameter));
                }
        }
    }
}
