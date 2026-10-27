// OWNER: WP16/WP24
#pragma once
#include <array>
#include <cstdint>
#include "DcBlock.h"
namespace luthier::reference::common
{
// Original synthesized rhythm module. No Roland samples/pattern recordings.
// prepare is off audio; all other methods use fixed storage and never allocate.
class Rc5Rhythm
{
public:
    using Grid=std::array<uint16_t,8>;
    static constexpr int patternCount=57,kitCount=7;
    void prepare(double sampleRate,uint32_t seed=0x51a7u) noexcept;
    void reset() noexcept;
    void select(int pattern,int variation,int kit) noexcept;
    void setTempo(double bpm) noexcept;
    void setLevel(double percent) noexcept;
    void locate(double quarterNotes) noexcept; // explicit transport seek, after setRunning
    void setRunning(bool) noexcept;
    void process(double* left,double* right,int samples) noexcept;
    int currentPattern() const noexcept {return pattern;}
    int currentKit() const noexcept {return kit;}
    int currentVariation() const noexcept {return variation;}
    double stepPosition() const noexcept {return step+phase;}
    static const Grid& grid(int pattern,int variation) noexcept;
    static const char* name(int pattern) noexcept;
    static int bars(int pattern) noexcept;
private:
    struct Voice {double amplitude=0,phase=0,frequency=0,noiseLow=0,body=0;int samples=0,kit=0;};
    struct Coeff {double decay,frequency,frequencyFloor,frequencyDecay,noiseLow,noiseMix,bodyMix;int duration;};
    void applySelection() noexcept;
    void trigger() noexcept;
    double noise() noexcept;
    double renderVoice(int) noexcept;
    double rate=48000,increment=120./(15*48000),phase=0,level=0,targetLevel=.64,levelCoefficient=0;
    int step=0,bar=0,pattern=1,variation=0,kit=1,pendingPattern=1,pendingVariation=0,pendingKit=1;
    bool running=false,needsTrigger=true;
    uint32_t initialSeed=0x51a7u,random=0x51a7u;
    std::array<Voice,8> voices{};
    std::array<std::array<Coeff,8>,7> coefficients{};
    DcBlock dc;
};
}
