// OWNER: WP7
#pragma once
#include "../PedalBase.h"
#include "Smoothers.h"
#include "OversamplerAdapter.h"
#include <array>
namespace luthier::reference::common {
inline ParamDesc continuous(const char* id,const char* label,const char* unit,float lo,float hi,float def,int cc,Taper taper=Taper::Linear,float skew=1) noexcept
{return {id,label,unit,ParamKind::Continuous,taper,lo,hi,def,skew,0,nullptr,PF_Automatable,20,(int16_t)cc};}
inline ParamDesc choice(const char* id,const char* label,const char* const* names,int count,int def,int cc,uint16_t flags=PF_Automatable) noexcept
{return {id,label,"",ParamKind::Enum,Taper::Linear,0,(float)(count-1),(float)def,1,count,names,flags,5,(int16_t)cc};}
inline ParamDesc engage(const char* id,int cc) noexcept
{return {id,"ON","",ParamKind::Toggle,Taper::Linear,0,1,1,1,0,nullptr,PF_Engage,5,(int16_t)cc};}
inline constexpr const char* qualityNames[]={"Eco","Normal","High"};
// Shared sample clock prevents block boundaries from changing coefficient updates.
// All signal/control interpolation state is double; PedalBase owns host norm floats.
template<int N> class Processor : public PedalBase<N> {
public:
 explicit Processor(const PedalDesc& desc):PedalBase<N>(desc){}
 // Slot bypass and its dry-delay ramp are owned by BoardEngine (§5.2).
 int getLatencySamples() const noexcept override {return latency;}
 float getTailEnergyDb() const noexcept override {return -120;}
protected:
 void prepareCommon(double rate,int policy,bool nonlinear,int qualityIndex=-1) noexcept {
  sr=common::sampleRate(rate);this->prepareParams(sr);
  const int quality=qualityIndex>=0?std::min(policy,(int)this->value(qualityIndex)):policy;
  for(auto& o:os)o.prepare(sr,quality);
  latency=nonlinear?os[0].latencySamples():0;
  for(int i=0;i<N;++i){smooth[(size_t)i].prepare(sr,this->desc().params[i].isDiscreteKind()?5:20);smooth[(size_t)i].reset(this->value(i));}
  resetCommon();
 }
 void prepareFactor(int factor) noexcept {for(auto& o:os)o.prepareFactor(sr,factor);latency=os[0].latencySamples();}
 void resetCommon() noexcept {for(auto& o:os)o.reset();clock=0;for(int i=0;i<N;++i)smooth[(size_t)i].reset(this->value(i));}
 void onParamChanged(int i,float v) noexcept override {smooth[(size_t)i].setTarget(v);}
 void tick() noexcept {for(int i=0;i<N;++i)p[(size_t)i]=smooth[(size_t)i].next();}
 double mix(int,double,double wet) noexcept {return guard(wet);}
 void advance() noexcept {++clock;}
 static double guard(double x) noexcept {return std::clamp(common::safe(x),-16.0,16.0);}
 static double input(double x) noexcept {return std::clamp(common::safe(x),-1e6,1e6);}
 double sr=48000;std::array<double,N> p{};std::array<common::OversamplerAdapter,2> os;
 unsigned clock=0;
private:
 std::array<common::LinearSmoother,N> smooth;
 int latency=0;
};
inline double modeBlend(double mode,double a,double b,double c) noexcept {return mode<1?a+(b-a)*mode:b+(c-b)*(mode-1);}
}
