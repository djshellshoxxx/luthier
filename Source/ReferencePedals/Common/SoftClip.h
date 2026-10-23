#pragma once
#include "Smoothers.h"
namespace luthier::reference::common {
inline double softClip(double x) noexcept {return std::tanh(safe(x));}
inline double cleanClip(double x) noexcept {x=std::clamp(safe(x),-1e6,1e6);double q=x*x;return x/std::sqrt(std::sqrt(1+q*q));}
inline double asymmetricClip(double x,double bias=.15) noexcept {return std::tanh(safe(x)+bias)-std::tanh(bias);}
inline double diodeApprox(double x) noexcept {x=safe(x);return std::copysign(-std::expm1(-std::abs(x)),x)/.9;}
}
