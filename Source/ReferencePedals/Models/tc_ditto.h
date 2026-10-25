// OWNER: WP15
#pragma once
#include "../IPedal.h"
namespace luthier::reference::models
{
const PedalDesc& tcDittoDescription() noexcept;
std::unique_ptr<IPedal> makeTcDitto();
PreparedMemoryRequirement tcDittoPreparedMemory(double,int,int,const float*) noexcept;
}
