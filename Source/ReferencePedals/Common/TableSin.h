#pragma once
#include "Smoothers.h"
#include <array>
namespace luthier::reference::common {
class TableSin {
public:
 TableSin() noexcept {for(size_t i=0;i<values.size();++i)values[i]=std::sin(2*pi*i/2048.0);}
 double operator()(double phase) const noexcept {phase=safe(phase);phase-=std::floor(phase);double p=phase*2048;int i=(int)p;return values[(size_t)i]+(p-i)*(values[(size_t)i+1]-values[(size_t)i]);}
private: std::array<double,2049> values{};
};
}
