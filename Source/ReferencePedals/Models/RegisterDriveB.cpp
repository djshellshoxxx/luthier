// OWNER: WP12
#include "../PedalRegistry.h"
#include "../Registration.h"
#include "mxr_dynacomp.h"
namespace luthier::reference
{
void registerDriveB()
{
    PedalRegistry::add(models::MxrDynaComp::descriptor(),
                       []() -> std::unique_ptr<IPedal> { return std::make_unique<models::MxrDynaComp>(); });
}
} // namespace luthier::reference
