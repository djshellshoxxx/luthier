// OWNER: WP8
#include "../PedalRegistry.h"
#include "../Registration.h"
#include "Boost.h"
#include "Compressor.h"
#include "Fuzz.h"
#include "Overdrive.h"
namespace luthier::reference
{
void registerGenericA()
{
    PedalRegistry::add(generic::Boost::descriptor(),
                       []() -> std::unique_ptr<IPedal> { return std::make_unique<generic::Boost>(); });
    PedalRegistry::add(generic::Overdrive::descriptor(),
                       []() -> std::unique_ptr<IPedal> { return std::make_unique<generic::Overdrive>(); });
    PedalRegistry::add(generic::Fuzz::descriptor(),
                       []() -> std::unique_ptr<IPedal> { return std::make_unique<generic::Fuzz>(); });
    PedalRegistry::add(generic::Compressor::descriptor(),
                       []() -> std::unique_ptr<IPedal> { return std::make_unique<generic::Compressor>(); });
}
} // namespace luthier::reference
