#pragma once
#include "Smoothers.h"
namespace luthier::reference::common {
// Bilinear transform of the analog RC lowpass, with prewarped cutoff.
class OnePole {
public:
 void setLowpass(double sr,double hz) noexcept {double g=std::tan(pi*frequency(sr,hz)/sampleRate(sr)); b=g/(1+g);a=(g-1)/(g+1);}
 double process(double x) noexcept {x=safe(x);double y=safe(b*(x+x1)-a*y1);x1=x;y1=y;return y;}
 double highpass(double x) noexcept {x=safe(x);return safe(x-process(x));}
 void reset() noexcept {x1=y1=0;}
private: double a=0,b=1,x1=0,y1=0;
};
}
