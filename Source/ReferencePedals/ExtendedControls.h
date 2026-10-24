// OWNER: WP2
#pragma once
#include "BoardCommand.h"
#include "Midi/ExtendedHostAutomation.h"
namespace luthier::reference
{
struct ExtendedControls
{
    std::array<float, ExtendedHostAutomationSnapshot::count> values {};
    std::array<uint32_t, ExtendedHostAutomationSnapshot::count> serials {};
};
ExtendedControls compileExtendedControls (const BoardSettings&, const std::vector<PedalInstance>&,
    const ExtendedControls* previous, const BoardCommand*, bool hostEcho, uint32_t version, AbSelect resolved);
}
