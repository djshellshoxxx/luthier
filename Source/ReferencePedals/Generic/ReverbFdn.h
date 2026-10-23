// OWNER: WP10
#pragma once
#include "../Common/FractionalDelay.h"
#include "../Common/OnePole.h"
#include "../Common/Lfo.h"
#include <array>
namespace luthier::reference::generic
{
// Eight-line orthogonal Householder FDN; Room/Hall/Ambient families only.
class ReverbFdn
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void set(double decaySeconds,double size,double tone,double modulation,int family,double freeze) noexcept;
    std::array<double,2> process(double input) noexcept;
private:
    std::array<double,7> lastSettings{};
    bool settingsValid=false;
    std::array<common::FractionalDelay,8> lines;
    std::array<common::FractionalDelay,4> diffusion;
    std::array<common::OnePole,8> damping;
    std::array<double,8> delays{},gains{},dampingAmount{};
    common::Lfo lfo;
    double sr=48000,mod=0,freeze=0;
};
}
