// OWNER: WP2
#pragma once
#include <algorithm>
#include <vector>

namespace luthier::reference
{
// Allocation is confined to prepare(), called at the stopped-audio barrier.
class DelayLine
{
public:
    void prepare (int maximumDelay) { data.assign (static_cast<size_t> (std::max (0, maximumDelay) + 1), 0.0); head = 0; }
    void reset() noexcept { std::fill (data.begin(), data.end(), 0.0); head = 0; }
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
private:
    std::vector<double> data;
    size_t head = 0;
};
}
