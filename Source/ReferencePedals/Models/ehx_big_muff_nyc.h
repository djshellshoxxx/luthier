// OWNER: WP13
#pragma once
#include "../Common/CircuitPedal.h"
#include "ehx_muff_core.h"
namespace luthier::reference::models
{
class EhxBigMuffNyc final : public common::CircuitPedal<8>
{
  public:
    EhxBigMuffNyc();
    static const PedalDesc &descriptor() noexcept;
    static PreparedMemoryRequirement prepareMemory(double, int, int, const float *) noexcept;
    uint64_t getPreparedMemoryBytes() const noexcept override;
    void prepare(double, int, int) override;
    void reset() noexcept override;
    void process(double *, double *, int) noexcept override;

  private:
    void update() noexcept;
    std::array<MuffCore, 2> core;
};
} // namespace luthier::reference::models
