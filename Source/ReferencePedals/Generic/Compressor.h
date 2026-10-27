// OWNER: WP8
#pragma once
#include "GenericACommon.h"
#include "../Common/OnePole.h"
#include <atomic>
namespace luthier::reference::generic {
class Compressor final : public detail::Processor<11> {
public: Compressor();static const PedalDesc& descriptor() noexcept;
 // E69 audit: legacy allpass DSP, filters, controls and sidechain pointers are embedded.
 static PreparedMemoryRequirement prepareMemory(double sr,int block,int policy,const float* params) noexcept {return fixedPreparedMemory<Compressor>(sr,block,policy,params);}
 uint64_t getPreparedMemoryBytes() const noexcept override {return sizeof(*this);}
 void prepare(double,int,int) override;void reset() noexcept override;void process(double*,double*,int) noexcept override;
 static double gainReductionDb(double inputDb,double threshold,double ratio,double knee) noexcept;
 void setSidechain(const double* l,const double* r,int n) noexcept override {externalL=l;externalR=r;externalCount=std::max(0,n);}
 bool sidechainAvailable() const noexcept {return available.load(std::memory_order_relaxed);}
 float gainReductionMeterDb() const noexcept {return meter.load(std::memory_order_relaxed);}
private:const double* externalL=nullptr;const double* externalR=nullptr;int externalCount=0;std::atomic<bool> available{false};std::array<common::OnePole,2> sidechain;double reduction=0,attack=0,release=0;std::atomic<float> meter{0};
};
}
