// OWNER: WP16
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

namespace luthier::reference::common
{
// Audio-owner internal RC-5 tap estimator. Caller supplies sample-boundary times
// and applies accepted BPM under its own clock-authority rules. No phase changes.
class Rc5TapTempo
{
public:
    bool prepare(double sampleRate) noexcept
    {
        if(!std::isfinite(sampleRate) || sampleRate<1 || sampleRate>192000)return false;
        rate=sampleRate;
        minimum=int64_t(std::ceil(rate/5));
        maximum=int64_t(std::floor(3*rate/2));
        timeout=int64_t(std::ceil(2*rate));
        reset();
        return true;
    }
    // Retains the prepared rate; call when deliberately resetting the timeline.
    void reset() noexcept
    {anchor=-1;intervals={};sum=0;count=next=0;}

    std::optional<double> tap(int64_t sample) noexcept
    {
        if(rate==0)return std::nullopt;
        if(sample<0 || sample<anchor){reset();return std::nullopt;}
        if(anchor<0){anchor=sample;return std::nullopt;}
        // Both timestamps are nonnegative and ordered, so subtraction is safe
        // even when the current timestamp is INT64_MAX.
        const auto interval=sample-anchor;
        anchor=sample;
        if(interval>=timeout){reset();anchor=sample;return std::nullopt;}
        if(interval<minimum || interval>maximum)return std::nullopt;
        if(count==intervals.size())sum-=intervals[next];
        else ++count;
        intervals[next]=interval;sum+=interval;
        next=(next+1)%intervals.size();
        return std::clamp(60*rate*double(count)/double(sum),40.,300.);
    }
private:
    double rate=0;
    int64_t anchor=-1,minimum=0,maximum=0,timeout=0,sum=0;
    std::array<int64_t,4> intervals{};
    size_t count=0,next=0;
};
}
