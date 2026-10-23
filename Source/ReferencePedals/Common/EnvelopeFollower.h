#pragma once
#include "Smoothers.h"
namespace luthier::reference::common {
class EnvelopeFollower {
public:
 void prepare(double sr,double attackMs,double releaseMs) noexcept {a=coefficient(sr,attackMs);r=coefficient(sr,releaseMs);}
 static double coefficient(double sr,double ms) noexcept {return std::exp(-1/(sampleRate(sr)*std::max(0.001,safe(ms))*.001));}
 double process(double x) noexcept {x=std::abs(safe(x));double c=x>state?a:r;return state=safe(x+c*(state-x));}
 void reset() noexcept {state=0;}
private: double a=0,r=0,state=0;
};
}
