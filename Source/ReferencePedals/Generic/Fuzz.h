// OWNER: WP8
#pragma once
#include "../Common/DcBlock.h"
#include "../Common/EnvelopeFollower.h"
#include "../Common/OnePole.h"
#include "GenericACommon.h"
namespace luthier::reference::generic
{
class Fuzz final : public detail::Processor<8>
{
  public:
    Fuzz();
    static const PedalDesc &descriptor() noexcept;
    void prepare(double, int, int) override;
    void reset() noexcept override;
    void process(double *, double *, int) noexcept override;

  private:
    bool identicalHistory = true;
    common::CachedDbGain driveGain, levelGain, toneGain, thresholdGain;
    std::array<common::OnePole, 2> hp, germanium, tilt, eco;
    std::array<common::DcBlock, 2> dc, octave;
    std::array<common::EnvelopeFollower, 2> sag, gate;
    std::array<int, 2> hold{};
    std::array<common::LinearSmoother, 2> gateGain;
    int policy = 1;
    double previousType = -1;
};
} // namespace luthier::reference::generic
