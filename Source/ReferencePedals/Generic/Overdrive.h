// OWNER: WP8
#pragma once
#include "../Common/DcBlock.h"
#include "../Common/OnePole.h"
#include "GenericACommon.h"
namespace luthier::reference::generic
{
class Overdrive final : public detail::Processor<7>
{
  public:
    Overdrive();
    static const PedalDesc &descriptor() noexcept;
    // Audited: all prepared DSP state is embedded; calibration scratch is stack-only.
    static PreparedMemoryRequirement prepareMemory(double sr, int block, int policy,
                                                   const float *params) noexcept
    {
        return fixedPreparedMemory<Overdrive>(sr, block, policy, params);
    }
    uint64_t getPreparedMemoryBytes() const noexcept override { return sizeof(*this); }
    void prepare(double, int, int) override;
    void reset() noexcept override;
    void process(double *, double *, int) noexcept override;

  private:
    bool identicalHistory = true;
    common::CachedDbGain driveGain, levelGain;
    double previousTone = -1, previousBass = -1;
    std::array<double, 101> calibration{};
    void prepareCalibration() noexcept;
    double calibrationGain(double db) const noexcept;
    std::array<common::OnePole, 2> bass, tone;
    std::array<common::DcBlock, 2> dc;
};
} // namespace luthier::reference::generic
