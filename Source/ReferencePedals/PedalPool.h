// OWNER: WP2
#pragma once
#include <array>
#include <memory>
#include <vector>
#include "BoardTypes.h"
#include "DelayLine.h"
#include "IPedal.h"
namespace luthier::reference
{
// Shared ownership is ONLY manipulated on the message thread. The audio thread
// borrows a raw pointer while its published plan remains protected by the epoch.
struct PedalRuntime
{
    std::unique_ptr<IPedal> pedal;
    InstanceId identity;
    std::string model;
    uint16_t revision = 1;
    juce::String extrasKey;
    std::array<float, kMaxParams> config {};
    std::array<DelayLine, 2> dry;
    double enableMix = 0.0;
    bool enabled = true;
    void reset() noexcept;
};
class PedalPool
{
public:
    void prepare (double sampleRate, int maxBlock);
    std::shared_ptr<PedalRuntime> acquire (const PedalInstance&, juce::String& error);
    void reclaim();
private:
    double rate = 48000.0;
    int block = 4096;
    std::vector<std::weak_ptr<PedalRuntime>> objects;
};
}
