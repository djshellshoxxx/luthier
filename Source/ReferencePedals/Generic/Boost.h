// OWNER: WP8
#pragma once
#include "../Common/Biquad.h"
#include "../Common/OnePole.h"
#include "GenericACommon.h"
namespace luthier::reference::generic
{
class Boost final : public detail::Processor<6>
{
  public:
    Boost();
    static const PedalDesc &descriptor() noexcept;
    // Audited: all prepared DSP state is embedded; calibration scratch is stack-only.
    static PreparedMemoryRequirement prepareMemory(double sr, int block, int policy,
                                                   const float *params) noexcept
    {
        return fixedPreparedMemory<Boost>(sr, block, policy, params);
    }
    uint64_t getPreparedMemoryBytes() const noexcept override { return sizeof(*this); }
    void prepare(double, int, int) override;
    void reset() noexcept override;
    void process(double *, double *, int) noexcept override;

  private:
    bool identicalHistory = true;
    common::CachedDbGain driveGain, levelGain;
    std::array<common::OnePole, 2> hp, eco;
    std::array<common::Biquad, 2> tone, warm, fat1;
    std::array<common::FirstOrderShelf, 2> fat2;
    int policy = 1;
    double previousTone = 100;
};
} // namespace luthier::reference::generic
