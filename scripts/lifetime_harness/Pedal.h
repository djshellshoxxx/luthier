#pragma once
// Minimal DSP/JUCE boundary for the real EffectsChain lifetime tests. This is
// not a DSP implementation or a substitute for native JUCE/host validation.
#include <algorithm>
#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
#include <vector>
#define jassert(x) assert(x)
#define JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(T) T(const T&) = delete; T& operator=(const T&) = delete;
namespace juce {
template<class T> T jmin(T a,T b) {return std::min(a,b);}
template<class T> T jmin(T a,T b,T c) {return std::min(a,std::min(b,c));}
template<class T> T jmax(T a,T b) {return std::max(a,b);}
template<class T> T jlimit(T a,T b,T v) {return std::clamp(v,a,b);}
inline bool isPositiveAndBelow(int v,int n) {return v>=0&&v<n;}
using CriticalSection=std::recursive_mutex;
struct ScopedLock {std::lock_guard<CriticalSection> lock; explicit ScopedLock(CriticalSection& c):lock(c){}};
struct ScopedTryLock {std::unique_lock<CriticalSection> lock; explicit ScopedTryLock(CriticalSection& c):lock(c,std::try_to_lock){} bool isLocked() const{return lock.owns_lock();}};
template<class T> struct AudioBuffer {
 int getNumSamples() const{return 0;} int getNumChannels() const{return 0;}
 T* getWritePointer(int){return nullptr;}
};
}
namespace luthier {
enum class PedalType {None,Boost};
struct PedalParam {};
class Pedal {
public:
 static constexpr int kMaxParams=10;
 virtual ~Pedal()=default;
 virtual PedalType getType() const noexcept=0;
 virtual const char* getName() const noexcept=0;
 virtual void prepare(double,int)=0;
 virtual void reset() noexcept=0;
 virtual void process(double*,double*,int) noexcept=0;
 virtual int getNumParameters() const noexcept=0;
 virtual const PedalParam& getParameterDescriptor(int) const noexcept=0;
 virtual int getLatencySamples() const noexcept{return 0;}
 virtual void setOversamplingFactor(int,int,bool) noexcept{}
 void setBypassed(bool){} void setMix(double){} void setTempoBpm(double){}
 void setExpression(double){} void resetBase(){} void resetParametersToDefault(){}
 void setParameterNormalised(int,double){} void processWithBypass(double*,double*,int){}
 static std::unique_ptr<Pedal> create(PedalType type){assert(type==PedalType::None);return nullptr;}
protected:
 virtual void parameterChanged(int,double)=0;
};
}
