#pragma once
#include "Smoothers.h"
namespace luthier::reference::common {
// Monotone antiparallel-diode equation: v + a*sinh(v/Vt) = input.
// Defaults are an ESTIMATED numerical fixture, not a particular pedal circuit.
class NewtonClipper {
public:
 void configure(double diodeScale,double thermalVoltage) noexcept {a=std::clamp(safe(diodeScale),1e-12,1e3);vt=std::clamp(safe(thermalVoltage),.001,1.0);}
 double residual(double v,double x) const noexcept {return v+a*std::sinh(v/vt)-x;}
 double process(double input) noexcept {
  const double x=std::clamp(safe(input),-16.0,16.0),mag=std::abs(x);
  double v=std::min(mag,vt*std::asinh(mag/a));iterations=0;
  for(int i=0;i<8;++i){++iterations;double f=residual(v,mag);if(std::abs(f)<1e-12)break;v-=f/(1+a/vt*std::cosh(v/vt));}
  return std::copysign(v,x);
 }
 int lastIterations() const noexcept {return iterations;}
 void reset() noexcept {iterations=0;}
private: double a=.001,vt=.025;int iterations=0;
};
}
