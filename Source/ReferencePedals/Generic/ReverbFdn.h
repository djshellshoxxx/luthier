// OWNER: WP10
#pragma once
#include "../Common/FractionalDelay.h"
#include "../Common/OnePole.h"
#include "../Common/RandomWalkLfo.h"
#include <array>
namespace luthier::reference::generic
{
// Eight-line orthogonal Householder FDN; Room/Hall/Ambient families only.
class ReverbFdn
{
public:
    void prepare(double sampleRate,uint32_t seed=1);
    void reset() noexcept;
    void set(double decaySeconds,double size,double tone,double modulation,int family,double freeze) noexcept;
    std::array<double,2> process(double input) noexcept;
    std::array<double,8> delayLengths() const noexcept {return delays;}
private:
    std::array<double,7> lastSettings{};
    bool settingsValid=false;
    std::array<common::FractionalDelay,8> lines;
    std::array<common::FractionalDelay,4> diffusion;
    std::array<std::array<common::FractionalDelay,2>,2> hallOutputDiffusion;
    std::array<std::array<double,2>,2> hallOutputDelays{};
    bool isHall=true;
    std::array<common::OnePole,8> damping;
    std::array<double,8> delays{},gains{},dampingAmount{};
    std::array<double,4> diffuserDelays{};
    std::vector<int> primeLookup;
    common::RandomWalkLfo lfo;
    double sr=48000,mod=0,freeze=0;
};
}
