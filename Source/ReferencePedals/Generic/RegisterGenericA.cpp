// OWNER: WP8
#include "../Registration.h"
#include "../PedalRegistry.h"
#include "Boost.h"
#include "Overdrive.h"
#include "Fuzz.h"
#include "Compressor.h"
namespace luthier::reference {
void registerGenericA(){
 PedalRegistry::add(generic::Boost::descriptor(),[]()->std::unique_ptr<IPedal>{return std::make_unique<generic::Boost>();});
 PedalRegistry::add(generic::Overdrive::descriptor(),[]()->std::unique_ptr<IPedal>{return std::make_unique<generic::Overdrive>();});
 PedalRegistry::add(generic::Fuzz::descriptor(),[]()->std::unique_ptr<IPedal>{return std::make_unique<generic::Fuzz>();});
 PedalRegistry::add(generic::Compressor::descriptor(),[]()->std::unique_ptr<IPedal>{return std::make_unique<generic::Compressor>();});
}
}
