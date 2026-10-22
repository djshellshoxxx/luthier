// OWNER: WP5 (interim declaration shipped by WP1 so MidiRouter compiles; WP5 owns the content)
#pragma once

#include <cstdint>
#include "MidiTypes.h"

namespace luthier::reference::midi
{

// Immutable binding table, built on the message thread and published to the
// audio thread through MidiRouter::publishMap (RCU, MIDI_EXPRESSION 2). WP5
// replaces this placeholder with the flat lookup table of sec 12; the type
// name and its use through std::unique_ptr<MapSnap> are frozen.
struct MapSnap
{
    uint32_t version = 0;
    int numBindings = 0;
};

} // namespace luthier::reference::midi
