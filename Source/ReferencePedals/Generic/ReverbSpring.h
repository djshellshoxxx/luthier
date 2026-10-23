// OWNER: WP10
#pragma once
#include "../Common/FractionalDelay.h"
#include "../Common/OnePole.h"
#include <array>
namespace luthier::reference::generic
{
class ReverbSpring
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void set(double decay,double size,double tone,double freeze) noexcept;
    std::array<double,2> process(double input) noexcept;
private:
    std::array<double,4> lastSettings{};
    bool settingsValid=false;
    std::array<std::array<double,24>,3> dispersion{};
    std::array<common::FractionalDelay,2> delays;
    common::OnePole lowpass[2];
    double sr=48000,length[2]{},gain[2]{},freeze=0;
};
}
