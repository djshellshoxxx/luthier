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
    // Destroy pedal (including its workers) before releasing the borrowed service owner.
    std::shared_ptr<IAssetStore> assetOwner;
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
    bool tailResidualAvailable = false; // E68: last rendered explicit wet-output capability
    std::array<float, kMaxParams> modelParams {};
    std::array<uint32_t, kMaxParams> modelParamSerials {};
    uint32_t modelEnabledSerial = 0, modelInputSerial = 0, modelOutputSerial = 0;
    std::array<uint32_t, 19> hostSerials {};
    bool hostInitialised = false;
    float inputGain = 1.f, outputGain = 1.f, modelInputGain = 1.f, modelOutputGain = 1.f;
    struct ParamRamp { double value = 0, target = 0, coefficient = 0; };
    std::array<ParamRamp, kMaxParams> ramps {};
    uint32_t rampMask = 0;
    double sampleRate = 48000;
    std::array<MidiEvent, 64> renderedMidi {};
    int numRenderedMidi = 0;
    uint32_t midiDropped = 0;
    void writeParam (int index, float value, float smoothMs = 0) noexcept;
    void advanceParams() noexcept;
    void collectMidi (int sampleOffset, int samples) noexcept;
    void reset() noexcept;
};
class PedalPool
{
public:
    bool setAssetStore (std::shared_ptr<IAssetStore>); // stopped; rejects while any runtime is retained
    bool hasAssetStore() const noexcept { return static_cast<bool> (assets); }
    void prepare (double sampleRate, int maxBlock, const std::vector<PedalInstance>& captured = {});
    std::shared_ptr<PedalRuntime> acquire (const PedalInstance&, juce::String& error, const PedalRuntime* active = nullptr, bool restoreExtras = false);
    void reclaim();
private:
    std::shared_ptr<IAssetStore> assets;
    bool assetStoreConfigured = false;
    double rate = 48000.0;
    int block = 4096;
    std::vector<std::weak_ptr<PedalRuntime>> objects;
    std::vector<PedalInstance> preparedExtras;
};
}
