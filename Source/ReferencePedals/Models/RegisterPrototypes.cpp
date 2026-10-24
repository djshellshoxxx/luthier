// OWNER: WP2
#include "../Registration.h"
#include "../PedalRegistry.h"
#include "PrototypePedals.h"
namespace luthier::reference
{
namespace
{
template<Model M> std::unique_ptr<IPedal> makePrototype() { return std::make_unique<models::PrototypePedal> (M); }
}
void registerPrototypes()
{
    PedalRegistry::add (models::PrototypePedal::description (Model::ts808Prototype), &makePrototype<Model::ts808Prototype>, &fixedPreparedMemory<models::PrototypePedal>);
    PedalRegistry::add (models::PrototypePedal::description (Model::ds1Prototype), &makePrototype<Model::ds1Prototype>, &fixedPreparedMemory<models::PrototypePedal>);
    PedalRegistry::add (models::PrototypePedal::description (Model::phase90Prototype), &makePrototype<Model::phase90Prototype>, &fixedPreparedMemory<models::PrototypePedal>);
}
}
