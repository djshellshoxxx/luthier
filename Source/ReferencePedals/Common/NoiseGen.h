#pragma once
#include <cstdint>
namespace luthier::reference::common {
class NoiseGen {
public: explicit NoiseGen(uint32_t seed=1) noexcept {reset(seed);} void reset(uint32_t seed=1) noexcept {state=seed?seed:1;}
 double next() noexcept {state^=state<<13;state^=state>>17;state^=state<<5;return (double)state/2147483648.0-1;}
private:uint32_t state=1;
};
}
