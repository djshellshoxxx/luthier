// OWNER: WP10
#pragma once
#include "../Common/FractionalDelay.h"
#include "../Common/OnePole.h"
#include "../Common/RandomWalkLfo.h"
#include <array>
namespace luthier::reference::generic
{
// Dattorro figure-eight tank using published reference-rate delay lengths.
class ReverbPlate
{
public:
    void prepare(double sampleRate,uint32_t seed=1);
    void reset() noexcept;
    void set(double decay,double size,double tone,double mod,double freeze) noexcept;
    std::array<double,2> process(double input) noexcept;
private:
    std::array<double,5> lastSettings{};
    bool settingsValid=false;
    std::array<common::FractionalDelay,4> inputDiffusion;
    std::array<common::FractionalDelay,2> modDiffusion,lateDiffusion,firstDelay,lastDelay;
    common::OnePole damping[2];common::RandomWalkLfo lfo;
    double sr=48000,scale=1,decayGain=.7,mod=.25,freeze=0,feedback[2]{};
};
}
