#pragma once
#include "Smoothers.h"
namespace luthier::reference::common {
class TptSvf {
public:
 struct Output {double low,band,high;};
 void prepare(double rate) noexcept {sr=sampleRate(rate);reset();set(1000,0.7071067811865476);}
 void set(double hz,double q) noexcept {g=std::tan(pi*frequency(sr,hz)/sr);k=1/std::clamp(safe(q),0.05,100.0);a=1/(1+g*(g+k));}
 Output process(double x) noexcept {x=safe(x);double v1=a*(s1+g*(x-s2)),v2=s2+g*v1;s1=safe(2*v1-s1);s2=safe(2*v2-s2);return {safe(v2),safe(v1),safe(x-k*v1-v2)};}
 void reset() noexcept {s1=s2=0;}
private: double sr=48000,g=0,k=1,a=1,s1=0,s2=0;
};
}
