// OWNER: WP8
#pragma once
#include "GenericACommon.h"
#include "../Common/OnePole.h"
#include "../Common/DcBlock.h"
namespace luthier::reference::generic {
class Overdrive final : public detail::Processor<7> {
public: Overdrive();static const PedalDesc& descriptor() noexcept;void prepare(double,int,int) override;void reset() noexcept override;void process(double*,double*,int) noexcept override;
private:std::array<common::OnePole,2> bass,tone;std::array<common::DcBlock,2> dc;
};
}
