// OWNER: WP10
#pragma once
#include "../PreparedMemory.h"
#include <cmath>
#include <climits>
namespace luthier::reference::generic::detail
{
// Constructor-free conservative fresh-runtime estimates. Ceil bounds positive
// truncation and equivalent decimal-expression rounding by one sample. The
// rate bound only prevents overflowing the existing int buffer/sieve arithmetic;
// the pool applies its byte limit before factory construction.
class TimeFxMemoryEstimate
{
public:
    TimeFxMemoryEstimate(double sampleRate,uint64_t objectBytes) noexcept
        : rate(sampleRate),bytes(objectBytes),valid(std::isfinite(rate) && rate>=8000 && rate<=(INT_MAX-64)/4.) {}
    void doubles(double seconds,int padding,int count=1) noexcept {addArray(seconds,padding,count,sizeof(double));}
    void delay(double seconds,int padding,int count=1) noexcept {doubles(seconds,padding+16,count);}
    void integers(double seconds,int padding,int count=1) noexcept {addArray(seconds,padding,count,sizeof(int));}
    void fdn(int count=1) noexcept
    {
        delay(.25,16,8*count);delay(.03,16,4*count);delay(.012,16,4*count);integers(.25,17,count);
        // vector<bool> bit packing is implementation dependent; one byte per
        // logical bit conservatively bounds the one-live-sieve prepare scratch.
        if(valid)temporary=2*(uint64_t(std::ceil(rate*.25))+16)+1;
    }
    PreparedMemoryRequirement result() const noexcept {return valid?PreparedMemoryRequirement{bytes,temporary}:PreparedMemoryRequirement{};}
private:
    void addArray(double seconds,int padding,int count,uint64_t elementBytes) noexcept
    {
        if(!valid)return;
        const double requested=std::ceil(rate*seconds);
        if(!std::isfinite(requested) || requested<0 || requested>INT_MAX-padding){valid=false;return;}
        const uint64_t amount=(uint64_t(requested)+uint64_t(padding))*uint64_t(count)*elementBytes;
        valid=addPreparedBytes(bytes,amount,bytes);
    }
    double rate;uint64_t bytes,temporary=0;bool valid;
};
}
