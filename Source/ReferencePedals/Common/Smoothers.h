// OWNER: WP7
#pragma once
#include <algorithm>
#include <cmath>
namespace luthier::reference::common {
inline constexpr double pi = 3.14159265358979323846;
inline double safe(double x) noexcept { return !std::isfinite(x) || std::abs(x)<1e-30 ? 0.0 : x; }
inline double sampleRate(double sr) noexcept { return std::isfinite(sr) && sr>=8000 ? sr : 48000; }
inline double frequency(double sr,double hz) noexcept { return std::clamp(safe(hz), 0.01, sampleRate(sr)*0.49); }
inline double dbGain(double db) noexcept {return std::pow(10.0,std::clamp(safe(db),-160.0,80.0)/20.0);}
class LinearSmoother {
public:
 void prepare(double sr,double ms) noexcept { length=std::max(1,(int)(std::max(1.0,sr)*std::max(0.0,safe(ms))*0.001)); }
 void reset(double x=0) noexcept { current=target=safe(x); remaining=0;step=0; }
 void setTarget(double x) noexcept { x=safe(x);if(x==target)return; target=x;remaining=length;step=(target-current)/remaining; }
 double next() noexcept {if(remaining>0){current+=step;if(--remaining==0)current=target;}return current;}
 double value() const noexcept {return current;}
private: double current=0,target=0,step=0;int length=1,remaining=0;
};
}
