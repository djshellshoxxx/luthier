// OWNER: WP2
#pragma once
#include <array>
#include <memory>
#include <vector>
#include "BoardTypes.h"
#include "DelayLine.h"
#include "IPedal.h"
#include "FootController.h"
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
    double enableMix = 0.0, bypassSamples = 240.0;
    FootController foot;
    bool enabled = true, modelEnabled = true;
    std::array<float, kMaxParams> modelParams {};
    std::array<uint32_t, kMaxParams> modelParamSerials {};
    uint32_t modelEnabledSerial = 0, modelInputSerial = 0, modelOutputSerial = 0;
    std::array<uint32_t, 19> hostSerials {};
    bool hostInitialised = false;
    float inputGain = 1.f, outputGain = 1.f, modelInputGain = 1.f, modelOutputGain = 1.f;
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
