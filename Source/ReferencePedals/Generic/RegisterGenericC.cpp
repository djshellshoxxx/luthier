// OWNER: WP10
#include "../Registration.h"
#include "../PedalRegistry.h"
#include "Delay.h"
#include "Reverb.h"
namespace luthier::reference
{
void registerGenericC()
{
    PedalRegistry::add(generic::Delay::descriptor(), []() -> std::unique_ptr<IPedal> { return std::make_unique<generic::Delay>(); },&generic::Delay::prepareMemory);
    PedalRegistry::add(generic::Reverb::descriptor(), []() -> std::unique_ptr<IPedal> { return std::make_unique<generic::Reverb>(); },&generic::Reverb::prepareMemory);
}
}
