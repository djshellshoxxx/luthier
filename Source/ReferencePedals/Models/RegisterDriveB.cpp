// OWNER: WP12
#include "../Registration.h"
namespace luthier::reference
{
// Dyna Comp, Distortion+ and RAT remain unregistered until their original
// acoustic, CPU and integration acceptance gates pass. They are instantiated
// directly by candidate tests and evidence probes.
void registerDriveB() {}
} // namespace luthier::reference
