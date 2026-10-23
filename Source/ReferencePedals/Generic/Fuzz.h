// OWNER: WP8
#pragma once
#include "GenericACommon.h"
#include "../Common/OnePole.h"
#include "../Common/DcBlock.h"
#include "../Common/EnvelopeFollower.h"
namespace luthier::reference::generic {
class Fuzz final : public detail::Processor<8> {
public: Fuzz();static const PedalDesc& descriptor() noexcept;void prepare(double,int,int) override;void reset() noexcept override;void process(double*,double*,int) noexcept override;
private:std::array<common::OnePole,2> hp,germanium,tilt,eco;std::array<common::DcBlock,2> dc,octave;std::array<common::EnvelopeFollower,2> sag,gate;std::array<int,2> hold{};std::array<common::LinearSmoother,2> gateGain;int policy=1;
};
}
