#pragma once
#include "MidiMap.h"
#include <vector>
namespace luthier::reference::midi
{
// Message-thread defaults below explicit bindings; profiles are always local.
void addDefaultMaps (MapSnap&, const std::vector<PedalInstance>&, const BoardSettings&);
}
