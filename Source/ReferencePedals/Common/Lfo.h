// OWNER: WP7
#pragma once
#include "TableSin.h"
namespace luthier::reference::common {
class Lfo {
public:
 void prepare(double rate) noexcept {sr=sampleRate(rate);setRate(hz);}
 void setRate(double rate) noexcept {hz=std::clamp(safe(rate),0.0,sr*.49);inc=hz/sr;}
 void setPhase(double x) noexcept {p=safe(x);p-=std::floor(p);}
 double phase() const noexcept {return p;}
 double sine() const noexcept {return table(p);}
 double triangle() const noexcept {return 1-4*std::abs(p-.5);}
 // setPhase may round a tiny negative input to1; inc<=.49 still keeps
 // p+inc below1.5, so the original floor wrap is exactly one subtraction.
 void advance() noexcept {p+=inc;if(p>=1)p-=1;}
private: TableSin table;double sr=48000,hz=1,inc=1.0/48000,p=0;
};
}
