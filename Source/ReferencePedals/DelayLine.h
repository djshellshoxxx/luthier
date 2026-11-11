// OWNER: WP2
#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace luthier::reference
{
// Allocation is confined to prepare(), called at the stopped-audio barrier.
class DelayLine
{
public:
    void prepare (int maximumDelay) { data.assign (static_cast<size_t> (std::max (0, maximumDelay) + 1), 0.0); head = 0; currentDelay = targetDelay = -1; fadeRemaining = fadeTotal = 0; }
    uint64_t preparedMemoryBytes() const noexcept { return static_cast<uint64_t> (data.capacity()) * sizeof (double); }
    void reset() noexcept { std::fill (data.begin(), data.end(), 0.0); head = 0; currentDelay = targetDelay = -1; fadeRemaining = fadeTotal = 0; }
    double process (double input, int delay) noexcept
    {
        if (data.empty()) return input;
        delay = std::clamp (delay, 0, static_cast<int> (data.size()) - 1);
        data[head] = input;
        const auto read = (head + data.size() - static_cast<size_t> (delay)) % data.size();
        const double output = data[read];
        if (++head == data.size()) head = 0;
        return output;
    }
    // Two read taps share one history. A changed target fades over a fixed
    // sample count; another request during a fade waits until it completes,
    // so rapid topology edits never replace an audible tap discontinuously.
    double processCrossfade (double input, int delay, int samples) noexcept
    {
        if (data.empty()) return input;
        delay = std::clamp (delay, 0, static_cast<int> (data.size()) - 1);
        data[head] = input;
        if (currentDelay < 0) currentDelay = targetDelay = delay;
        if (fadeRemaining == 0 && delay != currentDelay)
        {
            targetDelay = delay; fadeTotal = fadeRemaining = std::max (1, samples);
        }
        const auto read = [&] (int tap) { return data[(head + data.size() - static_cast<size_t> (tap)) % data.size()]; };
        double output = read (currentDelay);
        if (fadeRemaining > 0)
        {
            const double amount = static_cast<double> (fadeTotal - fadeRemaining + 1) / fadeTotal;
            output += amount * (read (targetDelay) - output);
            if (--fadeRemaining == 0) currentDelay = targetDelay;
        }
        if (++head == data.size()) head = 0;
        return output;
    }
private:
    int currentDelay = -1, targetDelay = -1, fadeRemaining = 0, fadeTotal = 0;
    std::vector<double> data;
    size_t head = 0;
};
}
