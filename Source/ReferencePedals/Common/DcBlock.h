#pragma once
#include "OnePole.h"
namespace luthier::reference::common {
class DcBlock { OnePole low;
public: void prepare(double sr,double hz=5) noexcept {low.setLowpass(sr,hz);reset();} double process(double x) noexcept {return low.highpass(x);} void reset() noexcept {low.reset();} };
}
