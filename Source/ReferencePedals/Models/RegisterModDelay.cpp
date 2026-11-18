// OWNER: WP14
// Registers the WP14 named models with PedalRegistry::add(desc, factory). The
// function name and its place in PedalRegistry::registerAll() are frozen
// (Registration.h). Only the four-stage phaser is registered for the 0.1 beta;
// the CE-2, CE-2W and DD-3T models are deferred and stay unregistered.
#include "../Registration.h"
#include "../PedalRegistry.h"
#include "mxr_phase90_script.h"

namespace luthier::reference
{

void registerModDelay()
{
    PedalRegistry::add (models::MxrPhase90Script::descriptor(),
                        []() -> std::unique_ptr<IPedal> { return std::make_unique<models::MxrPhase90Script>(); },
                        &models::MxrPhase90Script::prepareMemory);
}

} // namespace luthier::reference
