// OWNER: WP14
#pragma once
#include "../PedalBase.h"
#include "../Common/Lfo.h"
#include "../Common/OnePole.h"
#include "../Common/NoiseGen.h"
#include <array>
namespace luthier::reference::models
{
// Circuit-derived behavioral prototype: values and uncertainties are recorded
// in specs/pedals/mxr-phase90-script.md; no hardware-matched claim.
class MxrPhase90Script final : public PedalBase<13>
{
public:
    static PreparedMemoryRequirement prepareMemory(double sr,int block,int policy,const float* params) noexcept {return fixedPreparedMemory<MxrPhase90Script>(sr,block,policy,params);}
    uint64_t getPreparedMemoryBytes() const noexcept override {return sizeof(*this);}
    MxrPhase90Script();
    static const PedalDesc& descriptor() noexcept;
    void prepare(double,int,int) override;
    void reset() noexcept override;
    void process(double*,double*,int) noexcept override;
    void setEnabled(bool) noexcept override;
    void setServices(const PedalServices& s) override { seed=s.instanceSeed; }
    void setTime(const TimeInfo&) noexcept override;
    void onTransport(TransportEvent,int) noexcept override;
    void setExpression(int,float) noexcept override;
    double modulationPhase() const noexcept {return oscillator.phase();}
private:
    void onParamChanged(int,float) noexcept override;
    common::Lfo oscillator;
    common::OnePole highpass[2];
    common::NoiseGen noise;
    std::array<std::array<double,4>,2> states{};
    std::array<double,4> mismatch{};
    double allpass[2][4]{},allpassStep[2][4]{},triangleCoefficient=0,fadeStep=0;
    double lastSpeed=-1,lastVariant=-1,cachedRate=1;int coefficientClock=0;
    double sr=48000,triangle[2]{.5,.5},feedback[2]{},engage=1;
    uint32_t seed=1;
    bool enabled=false;
    TimeInfo time;
};
}
