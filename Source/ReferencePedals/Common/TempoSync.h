// OWNER: WP7
#pragma once
#include "Smoothers.h"
namespace luthier::reference::common {
struct TempoSync {
 static double rate(double bpm,double quarters) noexcept {return std::clamp(safe(bpm),20.0,400.0)/(60*std::max(.015625,safe(quarters)));}
 static double phase(double ppq,double quarters) noexcept {double p=safe(ppq)/std::max(.015625,safe(quarters));return p-std::floor(p);}
};
}
