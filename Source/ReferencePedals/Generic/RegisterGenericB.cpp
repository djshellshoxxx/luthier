// OWNER: WP9
#include "../Registration.h"
#include "../PedalRegistry.h"
#include "Chorus.h"
#include "Phaser.h"
#include "Tremolo.h"
#include "Wah.h"
namespace luthier::reference
{
void registerGenericB()
{
    PedalRegistry::add(generic::Chorus::descriptor(), []() -> std::unique_ptr<IPedal> { return std::make_unique<generic::Chorus>(); });
    PedalRegistry::add(generic::Tremolo::descriptor(), []() -> std::unique_ptr<IPedal> { return std::make_unique<generic::Tremolo>(); });
    PedalRegistry::add(generic::Wah::descriptor(), []() -> std::unique_ptr<IPedal> { return std::make_unique<generic::Wah>(); });
    PedalRegistry::add(generic::Phaser::descriptor(), []() -> std::unique_ptr<IPedal> { return std::make_unique<generic::Phaser>(); });
}
}
