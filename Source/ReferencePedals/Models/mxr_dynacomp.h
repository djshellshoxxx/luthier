// OWNER: WP12
#pragma once
#include "../Common/DcBlock.h"
#include "../Common/FractionalDelay.h"
#include "../Common/LowLatencyOversampler.h"
#include "../Common/NoiseGen.h"
#include "../Common/OnePole.h"
#include "../Common/PedalDsp.h"
#include <atomic>
namespace luthier::reference::models
{
// ESTIMATED feedback OTA model specified by mxr-dynacomp.md §4.
// No hardware capture/calibration claim; circuit component spread is not modelled.
class MxrDynaComp final : public common::Processor<10>
{
  public:
    MxrDynaComp();
    static const PedalDesc &descriptor() noexcept;
    static PreparedMemoryRequirement prepareMemory(double, int, int, const float *) noexcept;
    uint64_t getPreparedMemoryBytes() const noexcept override;
    void prepare(double, int, int) override;
    int getLatencySamples() const noexcept override { return otaOs[0].latencySamples(); }
    void reset() noexcept override;
    void process(double *, double *, int) noexcept override;
    void setServices(const PedalServices &s) override { seed = s.instanceSeed ? s.instanceSeed : 1; }
    void setExpression(int jack, float value) noexcept override;
    double gainReductionDb() const noexcept { return reduction.load(std::memory_order_relaxed); }

  private:
    std::array<common::LowLatencyOversampler, 2> otaOs;
    std::array<common::OnePole, 2> hp, lp;
    std::array<common::DcBlock, 2> dc;
    std::array<common::NoiseGen, 2> noise;
    std::array<common::FractionalDelay, 2> dry;
    std::array<double, 2> env{};
    std::atomic<double> reduction{0};
    uint32_t seed = 1;
    double releaseAtZero = 0, releaseAtFloor = 0;
    double attack = 0, previousAttack = -1, previousOutput = -1, outputGain = 0;
    // Exact-value caches: each reuses a result only for an identical operand.
    std::array<double, 2> lastReleaseMs{{-1, -1}}, lastRelease{};
    double previousNoiseSens = -1, noiseGain = 0;
};
} // namespace luthier::reference::models
