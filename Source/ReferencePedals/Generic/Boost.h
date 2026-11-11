// OWNER: WP8
#pragma once
#include "GenericACommon.h"
#include "../Common/Biquad.h"
#include "../Common/OnePole.h"
namespace luthier::reference::generic {
class Boost final : public detail::Processor<6> {
public: Boost();static const PedalDesc& descriptor() noexcept;
 // E69 audit: legacy allpass DSP, filters, controls and sidechain pointers are embedded.
 static PreparedMemoryRequirement prepareMemory(double sr,int block,int policy,const float* params) noexcept {return fixedPreparedMemory<Boost>(sr,block,policy,params);}
 uint64_t getPreparedMemoryBytes() const noexcept override {return sizeof(*this);}
 void prepare(double,int,int) override;void reset() noexcept override;void process(double*,double*,int) noexcept override;
private:std::array<common::OnePole,2> hp,eco;std::array<common::Biquad,2> tone,warm,fat1,fat2;int policy=1;
};
}
